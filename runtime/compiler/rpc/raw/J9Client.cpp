#ifdef JITAAS_USE_RAW_SOCKETS

#include "J9Client.hpp"
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

namespace JITaaS
{

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

J9ClientStream::J9ClientStream(TR::PersistentInfo *info)
   : _timeout(info->getJITaaSTimeout()),
   _connfd(openConnection(info->getJITaaSServerAddress(), info->getJITaaSServerPort(), info->getJITaaSTimeout())),
   _inputStream(_connfd), _outputStream(_connfd)
   {
   }

J9ClientStream::~J9ClientStream()
   {
   close(_connfd);
   _connfd = -1;
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
   writeBlocking();
   }

J9ServerMessageType
J9ClientStream::read()
   {
   _sMsg.mutable_data()->clear_data();
   protobuf::io::CodedInputStream codedInputStream(&_inputStream);
   uint32_t messageSize;
   if (!codedInputStream.ReadLittleEndian32(&messageSize))
      throw JITaaS::StreamFailure("JITaaS I/O error: reading message size");
   auto limit = codedInputStream.PushLimit(messageSize);
   if (!_sMsg.ParseFromCodedStream(&codedInputStream))
      throw JITaaS::StreamFailure("JITaaS I/O error: reading from stream");
   if (!codedInputStream.ConsumedEntireMessage())
      throw JITaaS::StreamFailure("JITaaS I/O error: did not receive entire message");
   codedInputStream.PopLimit(limit);
   return _sMsg.type();
   }

void
J9ClientStream::writeBlocking()
   {
      {
      protobuf::io::CodedOutputStream codedOutputStream(&_outputStream);
      size_t messageSize = _cMsg.ByteSizeLong();
      TR_ASSERT(messageSize < 1ul<<32, "message size too big");
      codedOutputStream.WriteLittleEndian32(messageSize);
      _cMsg.SerializeWithCachedSizes(&codedOutputStream);
      if (codedOutputStream.HadError())
         throw JITaaS::StreamFailure("JITaaS I/O error: writing to stream");
      // codedOutputStream must be dropped before calling flush
      }
   if (!_outputStream.Flush())
      throw JITaaS::StreamFailure("JITaaS I/O error: flushing stream");
   }

void
J9ClientStream::shutdown()
   {
   // destructor is used instead
   }

};

#endif
