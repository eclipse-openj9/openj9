#include "J9Server.h"
#include "control/Options.hpp"
#include "env/VerboseLog.hpp"

void
JAAS::J9ServerStream::readBlocking()
   {
   auto tag = (void *)_streamNum;
   bool ok;
   _stream->Read(&_cMsg, tag);
   if (!_cq.Next(&tag, &ok) || !ok)
      throw JAAS::StreamFailure();
   }

void
JAAS::J9ServerStream::writeBlocking()
   {
   auto tag = (void *)_streamNum;
   bool ok;
   _stream->Write(_sMsg, tag);
   if (!_cq.Next(&tag, &ok) || !ok)
      throw JAAS::StreamFailure();
   }

void
JAAS::J9ServerStream::finish()
   {
   auto tag = (void *)_streamNum;
   bool ok;
   _stream->Finish(Status::OK, tag);
   _cq.Next(&tag, &ok);
   acceptNewRPC();
   }

void
JAAS::J9ServerStream::cancel()
   {
   auto tag = (void *)_streamNum;
   bool ok;
   _stream->Finish(Status::CANCELLED, tag);
   _cq.Next(&tag, &ok);
   acceptNewRPC();
   }

void
JAAS::J9ServerStream::finishCompilation(uint32_t statusCode, std::string codeCache, std::string dataCache, std::string metaDataRelocation)
   {
   try
      {
      write(J9ServerMessageType::compilationCode, statusCode, codeCache, dataCache, metaDataRelocation);
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

void
JAAS::J9ServerStream::acceptNewRPC()
   {
   _ctx.reset(new grpc::ServerContext);
   _stream.reset(new J9ServerReaderWriter(_ctx.get()));
   _service->RequestCompile(_ctx.get(), _stream.get(), &_cq, _notif, (void *)_streamNum);
   }

void
JAAS::J9CompileServer::buildAndServe(J9BaseCompileDispatcher *compiler)
   {
   grpc::ServerBuilder builder;
   builder.AddListeningPort("0.0.0.0:38400", grpc::InsecureServerCredentials());
   builder.RegisterService(&_service);
   _notificationQueue = builder.AddCompletionQueue();

   _server = builder.BuildAndStart();
   serve(compiler);
   }

void
JAAS::J9CompileServer::serve(J9BaseCompileDispatcher *compiler)
   {
   bool ok = false;
   void *tag;

   // JAAS TODO: make this nicer; 7 should be stored in a const somewhere
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
