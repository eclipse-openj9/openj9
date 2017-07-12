#ifndef J9_SERVER_H
#define J9_SERVER_H

#include <grpc++/grpc++.h>
#include "rpc/types.h"
#include "control/Options.hpp"
#include "env/VerboseLog.hpp"

namespace JAAS
{

class J9ServerStream
   {
public:
   J9ServerStream(size_t streamNum,
                   J9CompileService::AsyncService *service,
                   grpc::ServerCompletionQueue *notif)
      : _streamNum(streamNum), _service(service), _notif(notif)
      {
      acceptNewRPC();
      }

   ~J9ServerStream()
      {
      _cq.Shutdown();
      }

   void readBlocking()
      {
      auto tag = (void *)_streamNum;
      bool ok;

      _stream->Read(&_cMsg, tag);
      if (!_cq.Next(&tag, &ok) || !ok)
         throw JAAS::StreamFailure();
      }

   void writeBlocking()
      {
      auto tag = (void *)_streamNum;
      bool ok;

      _stream->Write(_sMsg, tag);
      if (!_cq.Next(&tag, &ok) || !ok)
         throw JAAS::StreamFailure();
      }

   // TODO: add checks for when Next fails
   void finish()
      {
      auto tag = (void *)_streamNum;
      bool ok;
      _stream->Finish(Status::OK, tag);
      _cq.Next(&tag, &ok);
      acceptNewRPC();
      }

   // Same as finish, but with Status::CANCELLED
   void cancel()
      {
      auto tag = (void *)_streamNum;
      bool ok;
      _stream->Finish(Status::CANCELLED, tag);
      _cq.Next(&tag, &ok);
      acceptNewRPC();
      }

   // Hopefully temporary
   void finishWithOnlyCode(uint32_t code)
      {
      _sMsg.set_compilation_code(code);
      try
         {
         writeBlocking();
         finish();
         }

      catch (const JAAS::StreamFailure &e)
         {
         if (TR::Options::getVerboseOption(TR_VerboseJaas))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JAAS, "Server-Client stream failed before finishing");
         // Cleanup using cancel instead of finish
         cancel();
         }
      }

   // For reading after calling readBlocking
   const J9ClientMessage& clientMessage()
      {
      return _cMsg;
      }

   // For setting up the message before calling writeBlocking
   J9ServerMessage* serverMessage()
      {
      return &_sMsg;
      }

   void acceptNewRPC()
      {
      _ctx.reset(new grpc::ServerContext);
      _stream.reset(new J9ServerReaderWriter(_ctx.get()));
      _service->RequestCompile(_ctx.get(), _stream.get(), &_cq, _notif, (void *)_streamNum);
      }

private:
   const size_t _streamNum; // tagging for notification loop, used to identify associated CompletionQueue in vector
   grpc::ServerCompletionQueue *_notif;
   grpc::CompletionQueue _cq;
   J9CompileService::AsyncService *const _service;
   std::unique_ptr<J9ServerReaderWriter> _stream;
   std::unique_ptr<grpc::ServerContext> _ctx;

   // re-usable message objects
   J9ServerMessage _sMsg;
   J9ClientMessage _cMsg;
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
      _server->Shutdown();
      _notificationQueue->Shutdown();
      }

   void buildAndServe(J9BaseCompileDispatcher *compiler)
      {
      grpc::ServerBuilder builder;
      builder.AddListeningPort("0.0.0.0:38400", grpc::InsecureServerCredentials());
      builder.RegisterService(&_service);
      _notificationQueue = builder.AddCompletionQueue();

      _server = builder.BuildAndStart();
      serve(compiler);
      }

private:
   void serve(J9BaseCompileDispatcher *compiler)
      {
      bool ok = false;
      void *tag;

      // TODO: make this nicer
      for (size_t i = 0; i < 7; ++i)
         {
         _streams.push_back(std::unique_ptr<J9ServerStream>(new J9ServerStream(i, &_service, _notificationQueue.get())));
         }

      while (true)
         {
         _notificationQueue->Next(&tag, &ok);
         compiler->compile(_streams[(size_t)tag].get());
         }
      }


   std::unique_ptr<grpc::Server> _server;
   J9CompileService::AsyncService _service;
   std::unique_ptr<grpc::ServerCompletionQueue> _notificationQueue;
   std::vector<std::unique_ptr<J9ServerStream>> _streams;
   };

}

#endif // J9_SERVER_H
