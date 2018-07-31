#ifndef J9_SERVER_H
#define J9_SERVER_H

#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <openssl/ssl.h>
#include "rpc/StreamTypes.hpp"
#include "rpc/ProtobufTypeConvert.hpp"
#include "rpc/raw/J9Stream.hpp"
#include "env/CHTable.hpp"

class SSLOutputStream;
class SSLInputStream;

namespace JITaaS
{
class J9ServerStream : J9Stream
   {
public:
   J9ServerStream(int connfd, BIO *ssl, uint32_t timeout);
   virtual ~J9ServerStream() {}

   template <typename ...T>
   void write(J9ServerMessageType type, T... args)
      {
      setArgs<T...>(_sMsg.mutable_data(), args...);
      _sMsg.set_type(type);
      writeBlocking(_sMsg);
      }
   template <typename ...T>
   std::tuple<T...> read()
      {
      readBlocking(_cMsg);
      if (!_cMsg.status())
         throw StreamCancel();
      return getArgs<T...>(_cMsg.mutable_data());
      }

   void cancel();
   void finishCompilation(uint32_t statusCode, std::string codeCache = "", std::string dataCache = "", CHTableCommitData chTableData = {}, std::vector<TR_OpaqueClassBlock*> classesThatShouldNotBeNewlyExtended = {}, std::string logFileStr = "");
   void setClientId(uint64_t clientId)
      {
      _clientId = clientId;
      }
   uint64_t getClientId() const
      {
      return _clientId;
      }

private:
   void finish();

   uint32_t _msTimeout;
   uint64_t _clientId;
   };

//
// Inherited class is starting point for the received compilation request
//
class J9BaseCompileDispatcher
   {
public:
   virtual void compile(J9ServerStream *stream) = 0;
   };

class J9CompileServer
   {
public:
   ~J9CompileServer()
      {
      }

   void buildAndServe(J9BaseCompileDispatcher *compiler, TR::PersistentInfo *info);

private:
   void serve(J9BaseCompileDispatcher *compiler, uint32_t timeout);
   bool waitOnQueue();
   };

}

#endif // J9_SERVER_H
