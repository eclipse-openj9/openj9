#ifndef J9_CLIENT_H
#define J9_CLIENT_H

#include <grpc++/grpc++.h>
#include <chrono>
#include "compile/CompilationTypes.hpp"
#include "rpc/types.h"
#include "rpc/ProtobufTypeConvert.hpp"
#include "j9.h"
#include "ilgen/J9IlGeneratorMethodDetails.hpp"

namespace JAAS
{

class J9ClientStream
   {
public:
   J9ClientStream(std::string address, uint32_t timeout=0)
      : _stub(J9CompileService::NewStub(grpc::CreateChannel(address, grpc::InsecureChannelCredentials()))),
        _msTimeout(timeout)
      {}


   template <typename... T>
   void buildCompileRequest(T... args)
      {
      _ctx.reset(new grpc::ClientContext);
      _stream = _stub->AsyncCompile(_ctx.get(), &_cq, this);
      if (!waitOnQueue())
         throw StreamFailure("Failed to connect to server");

      write(args...);
      }

   Status waitForFinish()
      {
      void *tag;
      Status status;
      _stream->Finish(&status, &tag);
      if (waitOnQueue())
         return status;
      return Status::CANCELLED;
      }

   template <typename ...T>
   void write(T... args)
      {
      _clientMsg.set_status(true);
      setArgs<T...>(_clientMsg.mutable_data(), args...);
      _stream->Write(_clientMsg, this);
      if (!waitOnQueue())
         {
         shutdown();
         throw StreamFailure("Client stream failure while doing a write");
         }
      }

   void writeError()
      {
      _clientMsg.set_status(false);
      _clientMsg.mutable_data()->clear_data();
      _stream->Write(_clientMsg, this);
      if (!waitOnQueue())
         {
         shutdown();
         throw StreamFailure("Client stream failure in writeEror()");
         }
      }

   J9ServerMessageType read()
      {
      _stream->Read(&_serverMsg, this);
      if (!waitOnQueue())
         {
         shutdown();
         throw StreamFailure("Client stream failure while doing a read");
         }
      return _serverMsg.type();
      }

   template <typename ...T>
   std::tuple<T...> getRecvData()
      {
      return getArgs<T...>(_serverMsg.mutable_data());
      }

   void shutdown()
      {
      void *tag;
      bool ok;
      _ctx->TryCancel();
      _cq.Shutdown();
      while (_cq.Next(&tag, &ok));
      }

private:
   std::chrono::system_clock::time_point deadlineFromNow()
      {
      return std::chrono::system_clock::now() + std::chrono::milliseconds(_msTimeout);
      }

   bool waitOnQueue()
      {
      void *tag;
      bool ok;
      if (_msTimeout == 0)
         {
         return _cq.Next(&tag, &ok) && ok;
         }
      else
         {
         auto nextStatus = _cq.AsyncNext(&tag, &ok, deadlineFromNow());
         return ok && nextStatus == grpc::CompletionQueue::GOT_EVENT;
         }
      }

   std::unique_ptr<JAAS::J9CompileService::Stub> _stub;
   std::unique_ptr<grpc::ClientContext> _ctx;
   std::unique_ptr<JAAS::J9ClientReaderWriter> _stream;
   grpc::CompletionQueue _cq;
   uint32_t _msTimeout;

   // re-useable message objects
   JAAS::J9ClientMessage _clientMsg;
   JAAS::J9ServerMessage _serverMsg;
   };

}

#endif // J9_CLIENT_H
