#ifndef J9_CLIENT_H
#define J9_CLIENT_H

#include <grpc++/grpc++.h>
#include "rpc/types.h"
#include "rpc/ProtobufTypeConvert.hpp"

namespace JAAS
{

class J9ClientStream
   {
public:
   J9ClientStream()
      : _stub(J9CompileService::NewStub(grpc::CreateChannel("localhost:38400", grpc::InsecureChannelCredentials())))
      {}

   template<typename... T>
   void buildCompileRequest(T... args)
      {
      _ctx.reset(new grpc::ClientContext);
      _stream = _stub->Compile(_ctx.get());

      write(args...);
      }

   Status waitForFinish()
      {
      return _stream->Finish();
      }

   template <typename ...T>
   void write(T... args)
      {
      setArgs<T...>(_clientMsg.mutable_data(), args...);
      if (!_stream->Write(_clientMsg))
         throw StreamFailure();
      }

   J9ServerMessageType read()
      {
      if (!_stream->Read(&_serverMsg))
         throw StreamFailure();
      return _serverMsg.type();
      }

   template <typename ...T>
   std::tuple<T...> getRecvData()
      {
      return getArgs<T...>(_serverMsg.mutable_data());
      }

private:
   std::unique_ptr<JAAS::J9CompileService::Stub> _stub;
   std::unique_ptr<grpc::ClientContext> _ctx;
   std::unique_ptr<JAAS::J9ClientReaderWriter> _stream;

   // re-useable message objects
   JAAS::J9ClientMessage _clientMsg;
   JAAS::J9ServerMessage _serverMsg;
   };

}

#endif // J9_CLIENT_H
