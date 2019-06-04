/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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
#include <netinet/tcp.h>	/* for tcp_nodelay option */
#include <fcntl.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> /// gethostname, read, write
#include <openssl/err.h>
#include "J9Server.hpp"
#include "control/Options.hpp"
#include "env/VerboseLog.hpp"
#include "env/TRMemory.hpp"
#include "env/j9method.h"
#include "rpc/SSLProtobufStream.hpp"


namespace JITaaS
{
int J9ServerStream::_numConnectionsOpened = 0;
int J9ServerStream::_numConnectionsClosed = 0;

J9ServerStream::J9ServerStream(int connfd, BIO *ssl, uint32_t timeout)
   : J9Stream(),
   _msTimeout(timeout)
   {
   initStream(connfd, ssl);
   _numConnectionsOpened++;
   }

// J9Stream destructor is used instead
// cancel is unnecessary as errors will either throw or be indicated by the statusCode
void
J9ServerStream::finish()
   {
   }
void
J9ServerStream::cancel()
   {
   }

void
J9ServerStream::finishCompilation(uint32_t statusCode, std::string codeCache, std::string dataCache, CHTableCommitData chTableData,
                                 std::vector<TR_OpaqueClassBlock*> classesThatShouldNotBeNewlyExtended,
                                 std::string logFileStr, std::string symbolToIdStr,
                                 std::vector<TR_ResolvedJ9Method*> resolvedMethodsForPersistIprofileInfo)
   {
   try
      {
      write(J9ServerMessageType::compilationCode, statusCode, codeCache, dataCache, chTableData,
            classesThatShouldNotBeNewlyExtended, logFileStr, symbolToIdStr, resolvedMethodsForPersistIprofileInfo);
      finish();
      }
   catch (std::exception &e)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "Could not finish compilation: %s", e.what());
      }
   }

SSL_CTX *createSSLContext(TR::PersistentInfo *info)
   {
   SSL_CTX *ctx = SSL_CTX_new(SSLv23_server_method());
   if (!ctx)
      {
      perror("can't create SSL context");
      ERR_print_errors_fp(stderr);
      exit(1);
      }

   SSL_CTX_set_session_id_context(ctx, (const unsigned char*) "JITaaS", 6);

   if (SSL_CTX_set_ecdh_auto(ctx, 1) != 1)
      {
      perror("failed to configure SSL ecdh");
      ERR_print_errors_fp(stderr);
      exit(1);
      }

#if OPENSSL_VERSION_NUMBER >= 0x10101000L
   SSL_CTX_set_max_proto_version(ctx, TLS1_2_VERSION);
#endif

   auto &sslKeys = info->getJITaaSSslKeys();
   auto &sslCerts = info->getJITaaSSslCerts();
   auto &sslRootCerts = info->getJITaaSSslRootCerts();

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

   if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "Successfully initialized SSL context: OPENSSL_VERSION_NUMBER 0x%lx\n", OPENSSL_VERSION_NUMBER);

   return ctx;
   }

static bool
handleOpenSSLConnectionError(int connfd, SSL *&ssl, BIO *&bio, const char *errMsg)
{
   if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
       TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "%s: errno=%d", errMsg, errno);
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

   if (SSL_set_fd(ssl, connfd) != 1)
      return handleOpenSSLConnectionError(connfd, ssl, bio, "Error setting SSL file descriptor");

   if (SSL_accept(ssl) <= 0)
      return handleOpenSSLConnectionError(connfd, ssl, bio, "Error accepting SSL connection");

   SSL_set_accept_state(ssl);

   bio = BIO_new_ssl(sslCtx, false);
   if (!bio)
      return handleOpenSSLConnectionError(connfd, ssl, bio, "Error creating new BIO");

   if (BIO_set_ssl(bio, ssl, true) != 1)
      return handleOpenSSLConnectionError(connfd, ssl, bio, "Error setting BIO SSL");

   if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "SSL connection on socket 0x%x, Version: %s, Cipher: %s\n",
                                                     connfd, SSL_get_version(ssl), SSL_get_cipher(ssl));
   return true;
   }


void
J9CompileServer::buildAndServe(J9BaseCompileDispatcher *compiler, TR::PersistentInfo *info)
   {
   SSL_CTX *sslCtx = NULL;
   if (J9Stream::useSSL(info))
      {
      J9Stream::initSSL();
      sslCtx = createSSLContext(info);
      }

   uint32_t port = info->getJITaaSServerPort();
   uint32_t timeoutMs = info->getJITaaSTimeout();
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

   struct timeval t = {timeoutMs/1000, timeoutMs*1000%1000000};
   if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (void *)&t, sizeof(t)) < 0)
      {
      perror("Can't set option SO_RCVTIMEO on socket");
      exit(-1);
      }
   if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (void *)&t, sizeof(t)) < 0)
      {
      perror("Can't set option SO_SNDTIMEO on socket");
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
         if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "Error accepting connection: errno=%d", errno);
         continue;
         }

      BIO *bio = NULL;
      if (sslCtx && !acceptOpenSSLConnection(sslCtx, connfd, bio))
         continue;

      J9ServerStream *stream = new (PERSISTENT_NEW) J9ServerStream(connfd, bio, timeoutMs);
      compiler->compile(stream);
      }

   // TODO This will never be called - if the server actually shuts down properly then we should do this
   if (sslCtx)
      {
      SSL_CTX_free(sslCtx);
      EVP_cleanup();
      }
   }
}
