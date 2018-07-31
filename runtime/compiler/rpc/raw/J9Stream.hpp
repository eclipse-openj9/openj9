#ifndef J9_STREAM_HPP
#define J9_STREAM_HPP

#include "rpc/SSLProtobufStream.hpp"
#include "env/TRMemory.hpp"

namespace JITaaS
{
using namespace google::protobuf::io;

class J9Stream
   {
protected:
   void initStream(int connfd, BIO *ssl)
      {
      _connfd = connfd;
      _ssl = ssl;
      if (_ssl)
         {
         _sslInputStream = new (PERSISTENT_NEW) SSLInputStream(_ssl);
         _sslOutputStream = new (PERSISTENT_NEW) SSLOutputStream(_ssl);
         _inputStream = new (PERSISTENT_NEW) CopyingInputStreamAdaptor(_sslInputStream);
         _outputStream = new (PERSISTENT_NEW) CopyingOutputStreamAdaptor(_sslOutputStream);
         }
      else
         {
         _inputStream = new (PERSISTENT_NEW) FileInputStream(_connfd);
         _outputStream = new (PERSISTENT_NEW) FileOutputStream(_connfd);
         }
      }
   virtual ~J9Stream()
      {
      _inputStream->~ZeroCopyInputStream();
      TR_Memory::jitPersistentFree(_inputStream);
      _outputStream->~ZeroCopyOutputStream();
      TR_Memory::jitPersistentFree(_outputStream);
      if (_ssl)
         {
         _sslInputStream->~SSLInputStream();
         TR_Memory::jitPersistentFree(_sslInputStream);
         _sslOutputStream->~SSLOutputStream();
         TR_Memory::jitPersistentFree(_sslOutputStream);
         BIO_free_all(_ssl);
         }
      close(_connfd);
      _connfd = -1;
      }

   template <typename T>
   void readBlocking(T &val)
      {
      val.mutable_data()->clear_data();
      CodedInputStream codedInputStream(_inputStream);
      uint32_t messageSize;
      if (!codedInputStream.ReadLittleEndian32(&messageSize))
         throw JITaaS::StreamFailure("JITaaS I/O error: reading message size");
      auto limit = codedInputStream.PushLimit(messageSize);
      if (!val.ParseFromCodedStream(&codedInputStream))
         throw JITaaS::StreamFailure("JITaaS I/O error: reading from stream");
      if (!codedInputStream.ConsumedEntireMessage())
         throw JITaaS::StreamFailure("JITaaS I/O error: did not receive entire message");
      codedInputStream.PopLimit(limit);
      }
   template <typename T>
   void writeBlocking(const T &val)
      {
         {
         CodedOutputStream codedOutputStream(_outputStream);
         size_t messageSize = val.ByteSizeLong();
         TR_ASSERT(messageSize < 1ul<<32, "message size too big");
         codedOutputStream.WriteLittleEndian32(messageSize);
         val.SerializeWithCachedSizes(&codedOutputStream);
         if (codedOutputStream.HadError())
            throw JITaaS::StreamFailure("JITaaS I/O error: writing to stream");
         // codedOutputStream must be dropped before calling flush
         }
      if (_ssl ? !((CopyingOutputStreamAdaptor*)_outputStream)->Flush()
               : !((FileOutputStream*)_outputStream)->Flush())
         throw JITaaS::StreamFailure("JITaaS I/O error: flushing stream");
      }

   int _connfd; // connection file descriptor
   BIO *_ssl; // SSL connection, null if not using SSL

   // re-usable message objects
   J9ServerMessage _sMsg;
   J9ClientMessage _cMsg;

   SSLInputStream *_sslInputStream;
   SSLOutputStream *_sslOutputStream;
   ZeroCopyInputStream *_inputStream;
   ZeroCopyOutputStream *_outputStream;
   };
};

#endif
