/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

#ifdef JITAAS_USE_RAW_SOCKETS

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
#include "J9Server.hpp"
#include "control/Options.hpp"
#include "env/VerboseLog.hpp"
#include "env/TRMemory.hpp"
#include "rpc/SSLProtobufStream.hpp"
#include <openssl/err.h>

namespace JITaaS
{

bool useSSL(TR::PersistentInfo *info)
   {
   return info->getJITaaSSslKeys().size() || info->getJITaaSSslCerts().size() || info->getJITaaSSslRootCerts().size();
   }

J9ServerStream::J9ServerStream(int connfd, BIO *ssl, uint32_t timeout)
   : J9Stream(),
   _msTimeout(timeout)
   {
   initStream(connfd, ssl);
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
J9ServerStream::finishCompilation(uint32_t statusCode, std::string codeCache, std::string dataCache, CHTableCommitData chTableData, std::vector<TR_OpaqueClassBlock*> classesThatShouldNotBeNewlyExtended, std::string logFileStr)
   {
   try
      {
      write(J9ServerMessageType::compilationCode, statusCode, codeCache, dataCache, chTableData, classesThatShouldNotBeNewlyExtended, logFileStr);
      finish();
      }
   catch (std::exception &e)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "Could not finish compilation: %s", e.what());
      }
   }

void initSSL()
   {
   SSL_load_error_strings();
   SSL_library_init();
   OpenSSL_add_ssl_algorithms();
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
   EVP_PKEY *privKey = PEM_read_bio_PrivateKey(keyMem, nullptr, nullptr, nullptr);
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
   X509 *certificate = PEM_read_bio_X509(certMem, nullptr, nullptr, nullptr);
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
   SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);

   return ctx;
   }

void
J9CompileServer::buildAndServe(J9BaseCompileDispatcher *compiler, TR::PersistentInfo *info)
   {
   SSL_CTX *sslCtx = nullptr;
   if (useSSL(info))
      {
      initSSL();
      sslCtx = createSSLContext(info);

      if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "Sucessfully initialized SSL context");
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
      SSL *ssl = nullptr;
      BIO *bio = nullptr;

      int connfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
      if (connfd < 0)
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "Error accepting connection: errno=%d", errno);
         continue;
         }

      if (sslCtx)
         {
         ssl = SSL_new(sslCtx);
         if (!ssl)
            {
            if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "Error creating SSL connection: errno=%d", errno);
            ERR_print_errors_fp(stderr);
            close(connfd);
            SSL_free(ssl);
            continue;
            }
         if (SSL_set_fd(ssl, connfd) != 1)
            {
            if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "Error setting SSL file descriptor: errno=%d", errno);
            ERR_print_errors_fp(stderr);
            close(connfd);
            SSL_free(ssl);
            continue;
            }
         if (SSL_accept(ssl) <= 0)
            {
            if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "Error accepting SSL connection: errno=%d", errno);
            ERR_print_errors_fp(stderr);
            close(connfd);
            SSL_free(ssl);
            continue;
            }
         SSL_set_accept_state(ssl);

         bio = BIO_new_ssl(sslCtx, false);
         if (!bio)
            {
            if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "Error creating new BIO: errno=%d", errno);
            ERR_print_errors_fp(stderr);
            close(connfd);
            SSL_free(ssl);
            continue;
            }
         if (BIO_set_ssl(bio, ssl, true) != 1)
            {
            if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "Error setting BIO SSL: errno=%d", errno);
            ERR_print_errors_fp(stderr);
            close(connfd);
            BIO_free_all(bio);
            SSL_free(ssl);
            continue;
            }
         }

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

#endif
