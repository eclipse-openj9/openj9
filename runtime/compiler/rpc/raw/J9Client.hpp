#ifndef J9_CLIENT_H
#define J9_CLIENT_H

#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <chrono>
#include "compile/CompilationTypes.hpp"
#include "rpc/StreamTypes.hpp"
#include "rpc/ProtobufTypeConvert.hpp"
#include "j9.h"
#include "ilgen/J9IlGeneratorMethodDetails.hpp"

namespace JITaaS
{
using namespace google;

class J9ClientStream
   {
public:
   J9ClientStream(std::string address, uint32_t port, uint32_t timeout=0);
   ~J9ClientStream();

   template <typename... T>
   void buildCompileRequest(T... args)
      {
      write(args...);
      }

   Status waitForFinish();

   template <typename ...T>
   void write(T... args)
      {
      _cMsg.set_status(true);
      setArgs<T...>(_cMsg.mutable_data(), args...);
      writeBlocking();
      }

   void writeError();

   J9ServerMessageType read();

   template <typename ...T>
   std::tuple<T...> getRecvData()
      {
      return getArgs<T...>(_sMsg.mutable_data());
      }

   void shutdown();

private:
   void writeBlocking();

   uint32_t _timeout;
   int _connfd;

   // re-useable message objects
   JITaaS::J9ClientMessage _cMsg;
   JITaaS::J9ServerMessage _sMsg;

   protobuf::io::FileInputStream _inputStream;
   protobuf::io::FileOutputStream _outputStream;
   };

}

#endif // J9_CLIENT_H
