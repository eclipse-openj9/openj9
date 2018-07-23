#ifndef J9_CLIENT_H
#define J9_CLIENT_H

#include <grpc++/grpc++.h>
#include <chrono>
#include "compile/CompilationTypes.hpp"
#include "rpc/StreamTypes.hpp"
#include "rpc/ProtobufTypeConvert.hpp"
#include "j9.h"
#include "ilgen/J9IlGeneratorMethodDetails.hpp"

namespace JITaaS
{

class J9ClientStream
   {
public:
   J9ClientStream(TR::PersistentInfo *info)
      {
      _msTimeout = info->getJITaaSTimeout();
      uint32_t port = info->getJITaaSServerPort();
      const std::string &address = info->getJITaaSServerAddress();
      auto &sslKeys = info->getJITaaSSslKeys();
      auto &sslCerts = info->getJITaaSSslCerts();
      auto &sslRootCerts = info->getJITaaSSslRootCerts();
      bool useSSL =  sslKeys.size() || sslCerts.size() || sslRootCerts.size();
      grpc::SslCredentialsOptions sslOpts;
      if (useSSL)
         {
         TR_ASSERT(sslCerts.size() <= 1 && sslKeys.size() <= 1, "doesn't make sense have multiple ssl keys for the client");
         if (sslCerts.size() > 0)
            sslOpts.pem_cert_chain = sslCerts[0];
         if (sslKeys.size() > 0)
            sslOpts.pem_private_key = sslKeys[0];
         sslOpts.pem_root_certs = sslRootCerts;
         }
      auto creds = useSSL ? grpc::SslCredentials(sslOpts) : grpc::InsecureChannelCredentials();
      _stub = J9CompileService::NewStub(grpc::CreateChannel(address + ":" + std::to_string(port), creds));
      }


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

   std::unique_ptr<JITaaS::J9CompileService::Stub> _stub;
   std::unique_ptr<grpc::ClientContext> _ctx;
   std::unique_ptr<JITaaS::J9ClientReaderWriter> _stream;
   grpc::CompletionQueue _cq;
   uint32_t _msTimeout;

   // re-useable message objects
   JITaaS::J9ClientMessage _clientMsg;
   JITaaS::J9ServerMessage _serverMsg;
   };

}

#endif // J9_CLIENT_H
