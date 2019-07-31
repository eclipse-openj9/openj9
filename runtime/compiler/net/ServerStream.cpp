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
#include "ServerStream.hpp"
#include "env/TRMemory.hpp"
#include "net/SSLProtobufStream.hpp"
#if defined(JITSERVER_ENABLE_SSL)
#include <openssl/err.h>
#include "control/CompilationRuntime.hpp"
#endif

namespace JITServer
{
int ServerStream::_numConnectionsOpened = 0;
int ServerStream::_numConnectionsClosed = 0;

#if defined(JITSERVER_ENABLE_SSL)
ServerStream::ServerStream(int connfd, BIO *ssl)
   : CommunicationStream()
   {
   initStream(connfd, ssl);
   _numConnectionsOpened++;
   }
#else // JITSERVER_ENABLE_SSL
ServerStream::ServerStream(int connfd)
   : CommunicationStream()
   {
   initStream(connfd);
   _numConnectionsOpened++;
   }
#endif

#if defined(JITSERVER_ENABLE_SSL)
SSL_CTX *createSSLContext(TR::PersistentInfo *info)
   {
   SSL_CTX *ctx = SSL_CTX_new(SSLv23_server_method());
   if (!ctx)
      {
      perror("can't create SSL context");
      ERR_print_errors_fp(stderr);
      exit(1);
      }

   const char *sessionIDContext = "JITServer";
   SSL_CTX_set_session_id_context(ctx, (const unsigned char*)sessionIDContext, strlen(sessionIDContext));

   if (SSL_CTX_set_ecdh_auto(ctx, 1) != 1)
      {
      perror("failed to configure SSL ecdh");
      ERR_print_errors_fp(stderr);
      exit(1);
      }

   TR::CompilationInfo *compInfo = TR::CompilationInfo::get();
   auto &sslKeys = compInfo->getJITServerSslKeys();
   auto &sslCerts = compInfo->getJITServerSslCerts();
   auto &sslRootCerts = compInfo->getJITServerSslRootCerts();

   TR_ASSERT_FATAL(sslKeys.size() == 1 && sslCerts.size() == 1, "only one key and cert is supported for now");
   TR_ASSERT_FATAL(sslRootCerts.size() == 0, "server does not understand root certs yet");

   // Parse and set private key
   BIO *keyMem = BIO_new_mem_buf(&sslKeys[0][0], sslKeys[0].size());
   if (!keyMem)
      {
      perror("cannot create memory buffer for private key (OOM?)");
      ERR_print_errors_fp(stderr);
      exit(1);
      }
   EVP_PKEY *privKey = PEM_read_bio_PrivateKey(keyMem, NULL, NULL, NULL);
   if (!privKey)
      {
      perror("cannot parse private key");
      ERR_print_errors_fp(stderr);
      exit(1);
      }
   if (SSL_CTX_use_PrivateKey(ctx, privKey) != 1)
      {
      perror("cannot use private key");
      ERR_print_errors_fp(stderr);
      exit(1);
      }

   // Parse and set certificate
   BIO *certMem = BIO_new_mem_buf(&sslCerts[0][0], sslCerts[0].size());
   if (!certMem)
      {
      perror("cannot create memory buffer for cert (OOM?)");
      ERR_print_errors_fp(stderr);
      exit(1);
      }
   X509 *certificate = PEM_read_bio_X509(certMem, NULL, NULL, NULL);
   if (!certificate)
      {
      perror("cannot parse cert");
      ERR_print_errors_fp(stderr);
      exit(1);
      }
   if (SSL_CTX_use_certificate(ctx, certificate) != 1)
      {
      perror("cannot use cert");
      ERR_print_errors_fp(stderr);
      exit(1);
      }

   // Verify key and cert are valid
   if (SSL_CTX_check_private_key(ctx) != 1)
      {
      perror("private key check failed");
      ERR_print_errors_fp(stderr);
      exit(1);
      }

   // verify server identity using standard method
   SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Successfully initialized SSL context: OPENSSL_VERSION_NUMBER 0x%lx\n", OPENSSL_VERSION_NUMBER);

   return ctx;
   }

static bool
handleOpenSSLConnectionError(int connfd, SSL *&ssl, BIO *&bio, const char *errMsg)
{
   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
       TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "%s: errno=%d", errMsg, errno);
   ERR_print_errors_fp(stderr);

   close(connfd);
   if (bio)
      {
      BIO_free_all(bio);
      bio = NULL;
      }
   if (ssl)
      {
      SSL_free(ssl);
      ssl = NULL;
      }
   return false;
}

static bool
acceptOpenSSLConnection(SSL_CTX *sslCtx, int connfd, BIO *&bio)
   {
   SSL *ssl = SSL_new(sslCtx);
   if (!ssl)
      return handleOpenSSLConnectionError(connfd, ssl, bio, "Error creating SSL connection");

   SSL_set_accept_state(ssl);

   if (SSL_set_fd(ssl, connfd) != 1)
      return handleOpenSSLConnectionError(connfd, ssl, bio, "Error setting SSL file descriptor");

   if (SSL_accept(ssl) <= 0)
      return handleOpenSSLConnectionError(connfd, ssl, bio, "Error accepting SSL connection");

   bio = BIO_new_ssl(sslCtx, false);
   if (!bio)
      return handleOpenSSLConnectionError(connfd, ssl, bio, "Error creating new BIO");

   if (BIO_set_ssl(bio, ssl, true) != 1)
      return handleOpenSSLConnectionError(connfd, ssl, bio, "Error setting BIO SSL");

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "SSL connection on socket 0x%x, Version: %s, Cipher: %s\n",
                                                     connfd, SSL_get_version(ssl), SSL_get_cipher(ssl));
   return true;
   }
#endif

void
ServerStream::serveRemoteCompilationRequests(BaseCompileDispatcher *compiler, TR::PersistentInfo *info)
   {
#if defined(JITSERVER_ENABLE_SSL)
   SSL_CTX *sslCtx = NULL;
   if (CommunicationStream::useSSL())
      {
      CommunicationStream::initSSL();
      sslCtx = createSSLContext(info);
      }
#endif

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

#if defined(JITSERVER_ENABLE_SSL)
      BIO *bio = NULL;
      if (sslCtx && !acceptOpenSSLConnection(sslCtx, connfd, bio))
         continue;

      ServerStream *stream = new (PERSISTENT_NEW) ServerStream(connfd, bio);
#else
      ServerStream *stream = new (PERSISTENT_NEW) ServerStream(connfd);
#endif
      compiler->compile(stream);
      }

#if defined(JITSERVER_ENABLE_SSL)
   // The following piece of code will be executed only if the server shuts down properly
   if (sslCtx)
      {
      SSL_CTX_free(sslCtx);
      EVP_cleanup();
      }
#endif
   }
}
