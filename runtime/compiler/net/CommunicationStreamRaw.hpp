#ifndef COMMUNICATION_STREAM_RAW_H
#define COMMUNICATION_STREAM_RAW_H

#include "net/Message.hpp"
#include "env/TRMemory.hpp"
#include "env/CompilerEnv.hpp" // for TR::Compiler->target.is64Bit()
#include "control/Options.hpp" // TR::Options::useCompressedPointers()
#include "net/MessageBuffer.hpp"
#include <unistd.h>

namespace JITServer
{
class CommunicationStreamRaw
   {
protected:
   CommunicationStreamRaw()
      {
      // what is to be done with the memory allocation?
      // Ideally, want to use per-compilation region memory,
      // maybe not here, but definitely in RawTypeConvert.
      // But on the server side, the first message is received
      // before the compilation object is created, so cannot do that
      //
      // Could just use persistent memory, but then would need to 
      // free it manually. In the communication stream that should be fine,
      // since we are normally keeping the stream for multiple compilations,
      // so recycling the same persistent memory space should be fine, even if
      // we end up consuming more memory than ideally possible, we would cut down
      // on the number of allocations. Although I doubt that would matter a lot,
      // since the compilations themselves do a ton of allocations.
      // what's important is cutting down on the number of copies and tons of small allocations
      //
      // Another solution could be to create a separate per-compilation region on the server,
      // look at CompilationThread.cpp:7564 for example on how to do that.
      //
      // In any case, I need to resolve all other issues first and test if this actually
      // consumes less CPU before tackling that problem
      }

   ~CommunicationStreamRaw()
      {
      close(_connfd);
      }

#if defined(JITSERVER_ENABLE_SSL)
   void initStream(int connfd, BIO *ssl)
#else
   void initStream(int connfd)
#endif
      {
      _connfd = connfd;
#if defined(JITSERVER_ENABLE_SSL)
      _ssl = ssl;
#endif
      }


   void readMessage(Message &msg);
   void writeMessage(Message &msg);

   int getConnFD() { return _connfd; }

   static void initVersion();

   static uint64_t getJITServerVersion()
      {
      return ((((uint64_t)CONFIGURATION_FLAGS) << 32) | (MAJOR_NUMBER << 24) | (MINOR_NUMBER << 8));
      }
   
   int _connfd;
   ServerMessage _sMsg;
   ClientMessage _cMsg;

   static const uint8_t MAJOR_NUMBER = 0;
   static const uint16_t MINOR_NUMBER = 1;
   static const uint8_t PATCH_NUMBER = 0;
   static uint32_t CONFIGURATION_FLAGS;

private:
   // readBlocking and writeBlocking are functions that directly read/write
   // passed object from/to the socket. For the object to be correctly written,
   // it needs to be contiguous.
   template <typename T>
   void readBlocking(T &val)
      {
      readBlocking(&val, sizeof(T));
      }

   template <typename T>
   void readBlocking(T *ptr, size_t size)
      {
      int32_t bytesRead = read(_connfd, ptr, size);
      if (bytesRead == -1)
         {
         throw JITServer::StreamFailure("JITServer I/O error: read error");
         }
      else if (bytesRead < size)
         {
         char *remainingPtr = reinterpret_cast<char *>(ptr) + bytesRead;
         int32_t remainingRead = read(_connfd, remainingPtr, size - bytesRead);
         if (remainingRead != size - bytesRead)
            {
            throw JITServer::StreamFailure("JITServer I/O error: read error");
            }
         }
      }

   template <typename T>
   void writeBlocking(const T &val)
      {
      writeBlocking(&val, sizeof(T));
      }

   template <typename T>
   void writeBlocking(T *ptr, size_t size)
      {
      int32_t bytesWritten = write(_connfd, ptr, size);
      if (bytesWritten == -1)
         {
         throw JITServer::StreamFailure("JITServer I/O error: write error");
         }
      else if (bytesWritten < size)
         {
         char *remainingPtr = reinterpret_cast<char *>(ptr) + bytesWritten;
         int32_t remainingWritten = write(_connfd, remainingPtr, size - bytesWritten);
         if (remainingWritten != size - bytesWritten)
            {
            throw JITServer::StreamFailure("JITServer I/O error: write error");
            }
         }
      }
   };
};

#endif
