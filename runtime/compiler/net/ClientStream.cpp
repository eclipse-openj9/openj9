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

#include "ClientStream.hpp"
#include "control/Options.hpp"
#include "env/VerboseLog.hpp"
#include "net/SSLProtobufStream.hpp"
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
#if defined(JITSERVER_ENABLE_SSL)
#include <openssl/err.h>
#include "control/CompilationRuntime.hpp"
#endif

namespace JITServer
{
int ClientStream::_numConnectionsOpened = 0;
int ClientStream::_numConnectionsClosed = 0;

// used for checking server compatibility
int ClientStream::_incompatibilityCount = 0;
uint64_t ClientStream::_incompatibleStartTime = 0;
const uint64_t ClientStream::RETRY_COMPATIBILITY_INTERVAL_MS = 10000; //ms
const int ClientStream::INCOMPATIBILITY_COUNT_LIMIT = 5;

// Create SSL context, load certs and keys. Only needs to be done once.
// This is called during startup from rossa.cpp
int ClientStream::static_init(TR::PersistentInfo *info)
   {
#if defined(JITSERVER_ENABLE_SSL)
   if (!CommunicationStream::useSSL())
      return 0;

   CommunicationStream::initSSL();

   SSL_CTX *ctx = SSL_CTX_new(SSLv23_client_method());
   if (!ctx)
      {
      perror("can't create SSL context");
      ERR_print_errors_fp(stderr);
      return -1;
      }

   if (SSL_CTX_set_ecdh_auto(ctx, 1) != 1)
      {
      perror("failed to configure SSL ecdh");
      ERR_print_errors_fp(stderr);
      return -1;
      }

   TR::CompilationInfo *compInfo = TR::CompilationInfo::get();
   auto &sslKeys = compInfo->getJITServerSslKeys();
   auto &sslCerts = compInfo->getJITServerSslCerts();
   auto &sslRootCerts = compInfo->getJITServerSslRootCerts();

   // We could also set up keys and certs here, if it's necessary to use TLS client keys
   // it's not needed for a basic setup.
   TR_ASSERT_FATAL(sslKeys.size() == 0 && sslCerts.size() == 0, "client keypairs are not yet supported, use a root cert chain instead");

   // Parse and set certificate chain
   BIO *certMem = BIO_new_mem_buf(&sslRootCerts[0], sslRootCerts.size());
   if (!certMem)
      {
      perror("cannot create memory buffer for cert (OOM?)");
      ERR_print_errors_fp(stderr);
      return -1;
      }
   STACK_OF(X509_INFO) *certificates = PEM_X509_INFO_read_bio(certMem, NULL, NULL, NULL);
   if (!certificates)
      {
      perror("cannot parse cert");
      ERR_print_errors_fp(stderr);
      return -1;
      }
   X509_STORE *certStore = SSL_CTX_get_cert_store(ctx);
   if (!certStore)
      {
      perror("cannot get cert store");
      ERR_print_errors_fp(stderr);
      return -1;
      }
   // add all certificates in the chain to the cert store
   for (size_t i = 0; i < sk_X509_INFO_num(certificates); i++)
      {
      X509_INFO *certInfo = sk_X509_INFO_value(certificates, i);
      if (certInfo->x509)
         X509_STORE_add_cert(certStore, certInfo->x509);
      if (certInfo->crl)
         X509_STORE_add_crl(certStore, certInfo->crl);
      }
   sk_X509_INFO_pop_free(certificates, X509_INFO_free);

   // verify server identity using standard method
   SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);

   _sslCtx = ctx;

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Successfully initialized SSL context: OPENSSL_VERSION_NUMBER 0x%lx\n", OPENSSL_VERSION_NUMBER);
#endif
   return 0;
   }

#if defined(JITSERVER_ENABLE_SSL)
SSL_CTX *ClientStream::_sslCtx = NULL;
#endif

int openConnection(const std::string &address, uint32_t port, uint32_t timeoutMs)
   {
   // TODO consider using newer API like getaddrinfo to support IPv6
   struct hostent *entSer = gethostbyname(address.c_str());
   if (!entSer)
      throw StreamFailure("Cannot resolve server name");

   struct sockaddr_in servAddr;
   memset(&servAddr, 0, sizeof(servAddr));
   memcpy(&servAddr.sin_addr.s_addr, entSer->h_addr, entSer->h_length);
   servAddr.sin_family = AF_INET;
   servAddr.sin_port = htons(port);

   int sockfd = socket(AF_INET, SOCK_STREAM, 0);
   if (sockfd < 0)
      throw StreamFailure("Cannot create socket for JITServer");

   int flag = 1;
   if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (void*)&flag, sizeof(flag)) < 0)
      {
      close(sockfd);
      throw StreamFailure("Cannot set option SO_KEEPALIVE on socket");
      }

   struct linger lingerVal = {1, 2}; // linger 2 seconds
   if (setsockopt(sockfd, SOL_SOCKET, SO_LINGER, (void *)&lingerVal, sizeof(lingerVal)) < 0)
      {
      close(sockfd);
      throw StreamFailure("Cannot set option SO_LINGER on socket");
      }

   struct timeval timeout = {(timeoutMs / 1000), ((timeoutMs % 1000) * 1000)};
   if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (void *)&timeout, sizeof(timeout)) < 0)
      {
      close(sockfd);
      throw StreamFailure("Cannot set option SO_RCVTIMEO on socket");
      }

   if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (void *)&timeout, sizeof(timeout)) < 0)
      {
      close(sockfd);
      throw StreamFailure("Cannot set option SO_SNDTIMEO on socket");
      }

   if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0)
      {
      close(sockfd);
      throw StreamFailure("Cannot set option TCP_NODELAY on socket");
      }

   if (connect(sockfd, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0)
      {
      close(sockfd);
      throw StreamFailure("Connect failed");
      }

   return sockfd;
   }

#if defined(JITSERVER_ENABLE_SSL)
BIO *openSSLConnection(SSL_CTX *ctx, int connfd)
   {
   if (!ctx)
      return NULL;

   SSL *ssl = SSL_new(ctx);
   if (!ssl)
      {
      ERR_print_errors_fp(stderr);
      throw JITServer::StreamFailure("Failed to create new SSL connection");
      }

   SSL_set_connect_state(ssl);

   if (SSL_set_fd(ssl, connfd) != 1)
      {
      ERR_print_errors_fp(stderr);
      SSL_free(ssl);
      throw JITServer::StreamFailure("Cannot set file descriptor for SSL");
      }
   if (SSL_connect(ssl) != 1)
      {
      ERR_print_errors_fp(stderr);
      SSL_free(ssl);
      throw JITServer::StreamFailure("Failed to SSL_connect");
      }

   X509* cert = SSL_get_peer_certificate(ssl);
   if (!cert)
      {
      ERR_print_errors_fp(stderr);
      SSL_free(ssl);
      throw JITServer::StreamFailure("Server certificate unspecified");
      }
   X509_free(cert);

   if (X509_V_OK != SSL_get_verify_result(ssl))
      {
      ERR_print_errors_fp(stderr);
      SSL_free(ssl);
      throw JITServer::StreamFailure("Server certificate verification failed");
      }

   BIO *bio = BIO_new_ssl(ctx, true);
   if (!bio)
      {
      ERR_print_errors_fp(stderr);
      SSL_free(ssl);
      throw JITServer::StreamFailure("Failed to make new BIO");
      }
   if (BIO_set_ssl(bio, ssl, true) != 1)
      {
      ERR_print_errors_fp(stderr);
      BIO_free_all(bio);
      SSL_free(ssl);
      throw JITServer::StreamFailure("Failed to set BIO SSL");
      }

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "SSL connection on socket 0x%x, Version: %s, Cipher: %s\n",
                                                      connfd, SSL_get_version(ssl), SSL_get_cipher(ssl));
   return bio;
   }

ClientStream::ClientStream(TR::PersistentInfo *info)
   : CommunicationStream(), _versionCheckStatus(NOT_DONE)
   {
   int connfd = openConnection(info->getJITServerAddress(), info->getJITServerPort(), info->getSocketTimeout());
   BIO *ssl = openSSLConnection(_sslCtx, connfd);
   initStream(connfd, ssl);
   _numConnectionsOpened++;
   }
#else // JITSERVER_ENABLE_SSL
ClientStream::ClientStream(TR::PersistentInfo *info)
   : CommunicationStream(), _versionCheckStatus(NOT_DONE)
   {
   int connfd = openConnection(info->getJITServerAddress(), info->getJITServerPort(), info->getSocketTimeout());
   initStream(connfd);
   _numConnectionsOpened++;
   }
#endif

};
