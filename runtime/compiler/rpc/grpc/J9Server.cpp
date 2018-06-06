#ifdef JITAAS_USE_GRPC

#include <chrono>
#include "J9Server.hpp"
#include "control/Options.hpp"
#include "env/VerboseLog.hpp"

bool
JITaaS::J9ServerStream::waitOnQueue()
   {
   void *tag;
   bool ok;
   if (_msTimeout == 0)
      return _cq->Next(&tag, &ok) && ok;

   auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(_msTimeout);
   grpc::CompletionQueue::NextStatus nextStatus = _cq->AsyncNext(&tag, &ok, deadline);
   return ok && nextStatus == grpc::CompletionQueue::GOT_EVENT;
   }

void
JITaaS::J9ServerStream::readBlocking()
   {
   auto tag = (void *)_streamNum;
   bool ok;
   _stream->Read(&_cMsg, tag);
   if (!waitOnQueue())
      throw JITaaS::StreamFailure("Server stream failure while doing readBlocking()");
   }

void
JITaaS::J9ServerStream::writeBlocking()
   {
   auto tag = (void *)_streamNum;
   bool ok;
   _stream->Write(_sMsg, tag);
   if (!waitOnQueue())
      throw JITaaS::StreamFailure("Server stream failure while doing writeBlocking()");
   }

void
JITaaS::J9ServerStream::finish()
   {
   auto tag = (void *)_streamNum;
   bool ok;
   _stream->Finish(Status::OK, tag);
   _cq->Next(&tag, &ok);
   drainQueue();
   acceptNewRPC();
   }

void
JITaaS::J9ServerStream::cancel()
   {
   auto tag = (void *)_streamNum;
   bool ok;
   _stream->Finish(Status::CANCELLED, tag);
   _cq->Next(&tag, &ok);
   drainQueue();
   acceptNewRPC();
   }

void
JITaaS::J9ServerStream::finishCompilation(uint32_t statusCode, std::string codeCache, std::string dataCache, CHTableCommitData chTableData, std::vector<TR_OpaqueClassBlock*> classesThatShouldNotBeNewlyExtended)
   {
   try
      {
      write(J9ServerMessageType::compilationCode, statusCode, codeCache, dataCache, chTableData, classesThatShouldNotBeNewlyExtended);
      finish();
      }

   catch (const JITaaS::StreamFailure &e)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITaaS))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITaaS, "Server-Client stream failed before finishing");
      // Cleanup using cancel instead of finish
      cancel();
      }
   }

void
JITaaS::J9ServerStream::acceptNewRPC()
   {
   _ctx.reset(new grpc::ServerContext);
   _stream.reset(new J9ServerReaderWriter(_ctx.get()));
   _cq.reset(new grpc::CompletionQueue);
   _service->RequestCompile(_ctx.get(), _stream.get(), _cq.get(), _notif, (void *)_streamNum);
   }

void
JITaaS::J9ServerStream::drainQueue()
   {
   void *tag;
   bool ok;
   _cq->Shutdown();
   while (_cq->Next(&tag, &ok));
   }

void
JITaaS::J9CompileServer::buildAndServe(J9BaseCompileDispatcher *compiler, uint32_t port, uint32_t timeout)
   {
   grpc::ServerBuilder builder;
   builder.AddListeningPort("0.0.0.0:" + std::to_string(port), grpc::InsecureServerCredentials());
   builder.RegisterService(&_service);
   _notificationQueue = builder.AddCompletionQueue();

   _server = builder.BuildAndStart();
   serve(compiler, timeout);
   }

void
JITaaS::J9CompileServer::serve(J9BaseCompileDispatcher *compiler, uint32_t timeout)
   {
   bool ok = false;
   void *tag;

   // JITaaS TODO: make this nicer; 7 should be stored in a const somewhere
   for (size_t i = 0; i < 7; ++i)
      {
      _streams.push_back(std::unique_ptr<J9ServerStream>(new J9ServerStream(i, &_service, _notificationQueue.get(), timeout)));
      }

   while (true)
      {
      _notificationQueue->Next(&tag, &ok);
      compiler->compile(_streams[(size_t)tag].get());
      }
   }

#endif
