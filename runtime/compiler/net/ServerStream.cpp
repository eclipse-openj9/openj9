/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include <chrono>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>	/* for TCP_NODELAY option */
#include <fcntl.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> /// gethostname, read, write
#include <openssl/err.h>
#include "ServerStream.hpp"
#include "control/CompilationRuntime.hpp"
#include "env/TRMemory.hpp"
#include "net/LoadSSLLibs.hpp"

namespace JITServer
{
int ServerStream::_numConnectionsOpened = 0;
int ServerStream::_numConnectionsClosed = 0;

ServerStream::ServerStream(int connfd, BIO *ssl)
   : CommunicationStream()
   {
   initStream(connfd, ssl);
   _numConnectionsOpened++;
   }

SSL_CTX *createSSLContext(TR::PersistentInfo *info)
   {
   SSL_CTX *ctx = (*OSSL_CTX_new)((*OSSLv23_server_method)());

   if (!ctx)
      {
      perror("can't create SSL context");
      (*OERR_print_errors_fp)(stderr);
      exit(1);
      }

   const char *sessionIDContext = "JITServer";
   (*OSSL_CTX_set_session_id_context)(ctx, (const unsigned char*)sessionIDContext, strlen(sessionIDContext));

   if ((*OSSL_CTX_set_ecdh_auto)(ctx, 1) != 1)
      {
      perror("failed to configure SSL ecdh");
      (*OERR_print_errors_fp)(stderr);
      exit(1);
      }

   TR::CompilationInfo *compInfo = TR::CompilationInfo::get();
   auto &sslKeys = compInfo->getJITServerSslKeys();
   auto &sslCerts = compInfo->getJITServerSslCerts();
   auto &sslRootCerts = compInfo->getJITServerSslRootCerts();

   TR_ASSERT_FATAL(sslKeys.size() == 1 && sslCerts.size() == 1, "only one key and cert is supported for now");
   TR_ASSERT_FATAL(sslRootCerts.size() == 0, "server does not understand root certs yet");

   // Parse and set private key
   BIO *keyMem = (*OBIO_new_mem_buf)(&sslKeys[0][0], sslKeys[0].size());
   if (!keyMem)
      {
      perror("cannot create memory buffer for private key (OOM?)");
      (*OERR_print_errors_fp)(stderr);
      exit(1);
      }
   EVP_PKEY *privKey = (*OPEM_read_bio_PrivateKey)(keyMem, NULL, NULL, NULL);
   if (!privKey)
      {
      perror("cannot parse private key");
      (*OERR_print_errors_fp)(stderr);
      exit(1);
      }
   if ((*OSSL_CTX_use_PrivateKey)(ctx, privKey) != 1)
      {
      perror("cannot use private key");
      (*OERR_print_errors_fp)(stderr);
      exit(1);
      }

   // Parse and set certificate
   BIO *certMem = (*OBIO_new_mem_buf)(&sslCerts[0][0], sslCerts[0].size());
   if (!certMem)
      {
      perror("cannot create memory buffer for cert (OOM?)");
      (*OERR_print_errors_fp)(stderr);
      exit(1);
      }
   X509 *certificate = (*OPEM_read_bio_X509)(certMem, NULL, NULL, NULL);
   if (!certificate)
      {
      perror("cannot parse cert");
      (*OERR_print_errors_fp)(stderr);
      exit(1);
      }
   if ((*OSSL_CTX_use_certificate)(ctx, certificate) != 1)
      {
      perror("cannot use cert");
      (*OERR_print_errors_fp)(stderr);
      exit(1);
      }

   // Verify key and cert are valid
   if ((*OSSL_CTX_check_private_key)(ctx) != 1)
      {
      perror("private key check failed");
      (*OERR_print_errors_fp)(stderr);
      exit(1);
      }

   // verify server identity using standard method
   (*OSSL_CTX_set_verify)(ctx, SSL_VERIFY_PEER, NULL);

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Successfully initialized SSL context (%s)\n", (*OOpenSSL_version)(0));

   return ctx;
   }

static bool
handleOpenSSLConnectionError(int connfd, SSL *&ssl, BIO *&bio, const char *errMsg)
{
   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
       TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "%s: errno=%d", errMsg, errno);
   (*OERR_print_errors_fp)(stderr);

   close(connfd);
   if (bio)
      {
      (*OBIO_free_all)(bio);
      bio = NULL;
      }
   if (ssl)
      {
      (*OSSL_free)(ssl);
      ssl = NULL;
      }
   return false;
}

static bool
acceptOpenSSLConnection(SSL_CTX *sslCtx, int connfd, BIO *&bio)
   {
   SSL *ssl = (*OSSL_new)(sslCtx);
   if (!ssl)
      return handleOpenSSLConnectionError(connfd, ssl, bio, "Error creating SSL connection");

   (*OSSL_set_accept_state)(ssl);

   if ((*OSSL_set_fd)(ssl, connfd) != 1)
      return handleOpenSSLConnectionError(connfd, ssl, bio, "Error setting SSL file descriptor");

   if ((*OSSL_accept)(ssl) <= 0)
      return handleOpenSSLConnectionError(connfd, ssl, bio, "Error accepting SSL connection");

   bio = (*OBIO_new_ssl)(sslCtx, false);
   if (!bio)
      return handleOpenSSLConnectionError(connfd, ssl, bio, "Error creating new BIO");

   if ((*OBIO_ctrl)(bio, BIO_C_SET_SSL, true, (char *)ssl) != 1) // BIO_set_ssl(bio, ssl, true)
      return handleOpenSSLConnectionError(connfd, ssl, bio, "Error setting BIO SSL");

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "SSL connection on socket 0x%x, Version: %s, Cipher: %s\n",
                                                     connfd, (*OSSL_get_version)(ssl), (*OSSL_get_cipher)(ssl));
   return true;
   }

void
ServerStream::serveRemoteCompilationRequests(BaseCompileDispatcher *compiler, TR::PersistentInfo *info)
   {
   SSL_CTX *sslCtx = NULL;
   if (CommunicationStream::useSSL())
      {
      CommunicationStream::initSSL();
      sslCtx = createSSLContext(info);
      }

   uint32_t port = info->getJITServerPort();
   uint32_t timeoutMs = info->getSocketTimeout();
   int sockfd = socket(AF_INET, SOCK_STREAM, 0);
   if (sockfd < 0)
      {
      perror("can't open server socket");
      exit(1);
      }

   // see `man 7 socket` for option explanations
   int flag = true;
   if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&flag, sizeof(flag)) < 0)
      {
      perror("Can't set SO_REUSEADDR");
      exit(-1);
      }
   if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (void *)&flag, sizeof(flag)) < 0)
      {
      perror("Can't set SO_KEEPALIVE");
      exit(-1);
      }

   struct sockaddr_in serv_addr;
   memset((char *)&serv_addr, 0, sizeof(serv_addr));
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
   serv_addr.sin_port = htons(port);

   if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
      {
      perror("can't bind server address");
      exit(1);
      }
   if (listen(sockfd, SOMAXCONN) < 0)
      {
      perror("listen failed");
      exit(1);
      }

   while (true)
      {
      struct sockaddr_in cli_addr;
      socklen_t clilen = sizeof(cli_addr);

      int connfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
      if (connfd < 0)
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Error accepting connection: errno=%d", errno);
         continue;
         }

      struct timeval timeoutMsForConnection = {(timeoutMs / 1000), ((timeoutMs % 1000) * 1000)};
      if (setsockopt(connfd, SOL_SOCKET, SO_RCVTIMEO, (void *)&timeoutMsForConnection, sizeof(timeoutMsForConnection)) < 0)
         {
         perror("Can't set option SO_RCVTIMEO on connfd socket");
         exit(-1);
         }
      if (setsockopt(connfd, SOL_SOCKET, SO_SNDTIMEO, (void *)&timeoutMsForConnection, sizeof(timeoutMsForConnection)) < 0)
         {
         perror("Can't set option SO_SNDTIMEO on connfd socket");
         exit(-1);
         }

      BIO *bio = NULL;
      if (sslCtx && !acceptOpenSSLConnection(sslCtx, connfd, bio))
         continue;

      ServerStream *stream = new (PERSISTENT_NEW) ServerStream(connfd, bio);

      compiler->compile(stream);
      }

   // The following piece of code will be executed only if the server shuts down properly
   if (sslCtx)
      {
      (*OSSL_CTX_free)(sslCtx);
      (*OEVP_cleanup)();
      }
   }
}
