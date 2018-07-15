#ifndef J9_SERVER_H
#define J9_SERVER_H

#include <google/protobuf/io/zero_copy_stream_impl.h>
#include "rpc/StreamTypes.hpp"
#include "rpc/ProtobufTypeConvert.hpp"
#include "env/CHTable.hpp"

namespace JITaaS
{
using namespace google;

class J9ServerStream
   {
public:
   J9ServerStream(int connfd, uint32_t timeout);
   ~J9ServerStream();

   template <typename ...T>
   void write(J9ServerMessageType type, T... args)
      {
      setArgs<T...>(_sMsg.mutable_data(), args...);
      _sMsg.set_type(type);
      writeBlocking();
      }
   template <typename ...T>
   std::tuple<T...> read()
      {
      readBlocking();
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
   void readBlocking();
   void writeBlocking();

   int _connfd; // connection file descriptor
   uint32_t _msTimeout;
   uint64_t _clientId;

   // re-usable message objects
   J9ServerMessage _sMsg;
   J9ClientMessage _cMsg;

   protobuf::io::FileInputStream _inputStream;
   protobuf::io::FileOutputStream _outputStream;
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

   void buildAndServe(J9BaseCompileDispatcher *compiler, uint32_t port, uint32_t timeout);

private:
   void serve(J9BaseCompileDispatcher *compiler, uint32_t timeout);
   bool waitOnQueue();
   };

}

#endif // J9_SERVER_H
