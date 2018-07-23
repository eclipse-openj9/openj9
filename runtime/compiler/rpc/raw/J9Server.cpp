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

namespace JITaaS
{

J9ServerStream::J9ServerStream(int connfd, uint32_t timeout)
   : _connfd(connfd), _msTimeout(timeout),
   _inputStream(connfd), _outputStream(connfd)
   {
   }

J9ServerStream::~J9ServerStream()
   {
   close(_connfd);
   _connfd = -1;
   }

void
J9ServerStream::readBlocking()
   {
   _cMsg.mutable_data()->clear_data();
   protobuf::io::CodedInputStream codedInputStream(&_inputStream);
   uint32_t messageSize;
   if (!codedInputStream.ReadLittleEndian32(&messageSize))
      throw JITaaS::StreamFailure("JITaaS I/O error: reading message size");
   auto limit = codedInputStream.PushLimit(messageSize);
   if (!_cMsg.ParseFromCodedStream(&codedInputStream))
      throw JITaaS::StreamFailure("JITaaS I/O error: reading from stream");
   if (!codedInputStream.ConsumedEntireMessage())
      throw JITaaS::StreamFailure("JITaaS I/O error: did not receive entire message");
   codedInputStream.PopLimit(limit);
   }

void
J9ServerStream::writeBlocking()
   {
      {
      protobuf::io::CodedOutputStream codedOutputStream(&_outputStream);
      size_t messageSize = _sMsg.ByteSizeLong();
      TR_ASSERT(messageSize < 1ul<<32, "message size too big");
      codedOutputStream.WriteLittleEndian32(messageSize);
      _sMsg.SerializeWithCachedSizes(&codedOutputStream);
      if (codedOutputStream.HadError())
         throw JITaaS::StreamFailure("JITaaS I/O error: writing to stream");
      // codedOutputStream must be dropped before calling flush
      }
   if (!_outputStream.Flush())
      throw JITaaS::StreamFailure("JITaaS I/O error: flushing stream");
   }

// destructor is used instead
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

void
J9CompileServer::buildAndServe(J9BaseCompileDispatcher *compiler, TR::PersistentInfo *info)
   {
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

      J9ServerStream *stream = new (PERSISTENT_NEW) J9ServerStream(connfd, timeoutMs);
      compiler->compile(stream);
      }
   }
}

#endif
