#ifdef JITAAS_USE_RAW_SOCKETS

#include "J9Client.hpp"
#include "control/Options.hpp"
#include "env/VerboseLog.hpp"
#include "rpc/SSLProtobufStream.hpp"
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

namespace JITaaS
{

// Create SSL context, load certs and keys. Only needs to be done once.
// This is called during startup from rossa.cpp
void J9ClientStream::static_init(TR::PersistentInfo *info)
   {
   if (!(info->getJITaaSSslKeys().size() || info->getJITaaSSslCerts().size() || info->getJITaaSSslRootCerts().size()))
      return;

   SSL_load_error_strings();
   SSL_library_init();
   OpenSSL_add_ssl_algorithms();

   SSL_CTX *ctx = SSL_CTX_new(SSLv23_client_method());
   if (!ctx)
      {
      perror("can't create SSL context");
      ERR_print_errors_fp(stderr);
      exit(1);
      }

   if (SSL_CTX_set_ecdh_auto(ctx, 1) != 1)
      {
      perror("failed to configure SSL ecdh");
      ERR_print_errors_fp(stderr);
      exit(1);
      }

   auto &sslKeys = info->getJITaaSSslKeys();
   auto &sslCerts = info->getJITaaSSslCerts();
   auto &sslRootCerts = info->getJITaaSSslRootCerts();

   // TODO: we could also set up keys and certs here, if it's necessary to use TLS client keys 
   // it's not needed for a basic setup.
   TR_ASSERT_FATAL(sslKeys.size() == 0 && sslCerts.size() == 0, "client keypairs are not yet supported, use a root cert chain instead");

   // Parse and set certificate chain
   BIO *certMem = BIO_new_mem_buf(&sslRootCerts[0], sslRootCerts.size());
   if (!certMem)
      {
      perror("cannot create memory buffer for cert (OOM?)");
      ERR_print_errors_fp(stderr);
      exit(1);
      }
   STACK_OF(X509_INFO) *certificates = PEM_X509_INFO_read_bio(certMem, nullptr, nullptr, nullptr);
   if (!certificates)
      {
      perror("cannot parse cert");
      ERR_print_errors_fp(stderr);
      exit(1);
      }
   X509_STORE *certStore = SSL_CTX_get_cert_store(ctx);
   if (!certStore)
      {
      perror("cannot get cert store");
      ERR_print_errors_fp(stderr);
      exit(1);
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
   SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);

   _sslCtx = ctx;

   if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "Sucessfully initialized SSL context");
   }

SSL_CTX *J9ClientStream::_sslCtx;

int openConnection(const std::string &address, uint32_t port, uint32_t timeoutMs)
   {
   // TODO consider using newer API like getaddrinfo to support IPv6
   struct hostent *entSer;
   if ((entSer = gethostbyname(address.c_str())) == NULL)
      throw StreamFailure("Cannot resolve server name");

   struct sockaddr_in servAddr;
   memset(&servAddr, 0, sizeof(servAddr));
   memcpy(&servAddr.sin_addr.s_addr, entSer->h_addr, entSer->h_length);
   servAddr.sin_family = AF_INET;
   servAddr.sin_port = htons(port);

   int sockfd = socket(AF_INET, SOCK_STREAM, 0);
   if(sockfd < 0)
      throw StreamFailure("Cannot create socket for JITaaS");

   int flag = 1;
   if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE , (void*)&flag, sizeof(flag)) <0)
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

   struct timeval timeout = {timeoutMs/1000, timeoutMs*1000%1000000};
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

   if(connect(sockfd, (struct sockaddr *) &servAddr, sizeof(servAddr)) < 0)
      {
      close(sockfd);
      throw StreamFailure("Connect failed");
      }

   return sockfd;
   }

BIO *openSSLConnection(SSL_CTX *ctx, int connfd)
   {
   if (!ctx)
      return nullptr;

   SSL *ssl = SSL_new(ctx);
   if (!ssl)
      {
      ERR_print_errors_fp(stderr);
      throw JITaaS::StreamFailure("Failed to create new SSL connection");
      }
   if (SSL_set_fd(ssl, connfd) != 1)
      {
      ERR_print_errors_fp(stderr);
      SSL_free(ssl);
      throw JITaaS::StreamFailure("Cannot set file descriptor for SSL");
      }
   if (SSL_connect(ssl) != 1)
      {
      ERR_print_errors_fp(stderr);
      SSL_free(ssl);
      throw JITaaS::StreamFailure("Failed to SSL_connect");
      }
   SSL_set_connect_state(ssl);

   X509* cert = SSL_get_peer_certificate(ssl);
   if (!cert)
      {
      ERR_print_errors_fp(stderr);
      SSL_free(ssl);
      throw JITaaS::StreamFailure("Server certificate unspecified");
      }
   X509_free(cert);

   if (X509_V_OK != SSL_get_verify_result(ssl))
      {
      ERR_print_errors_fp(stderr);
      SSL_free(ssl);
      throw JITaaS::StreamFailure("Server certificate verification failed");
      }

   BIO *bio = BIO_new_ssl(ctx, true);
   if (!bio)
      {
      ERR_print_errors_fp(stderr);
      SSL_free(ssl);
      throw JITaaS::StreamFailure("fAiled to make new BIO");
      }
   if (BIO_set_ssl(bio, ssl, true) != 1)
      {
      ERR_print_errors_fp(stderr);
      SSL_free(ssl);
      BIO_free_all(bio);
      throw JITaaS::StreamFailure("Failed to set BIO SSL");
      }

   return bio;
   }

J9ClientStream::J9ClientStream(TR::PersistentInfo *info)
   : _timeout(info->getJITaaSTimeout())
   {
   int connfd = openConnection(info->getJITaaSServerAddress(), info->getJITaaSServerPort(), info->getJITaaSTimeout());
   BIO *ssl = openSSLConnection(_sslCtx, connfd);
   initStream(connfd, ssl);
   }

Status
J9ClientStream::waitForFinish()
   {
   // any error would have thrown earlier
   return Status::OK;
   }

void
J9ClientStream::writeError()
   {
   _cMsg.set_status(false);
   _cMsg.mutable_data()->clear_data();
   writeBlocking(_cMsg);
   }

void
J9ClientStream::shutdown()
   {
   // destructor is used instead
   }

};

#endif
