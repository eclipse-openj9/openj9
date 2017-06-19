#ifndef RPC_SERVER_H
#define RPC_SERVER_H

#include <grpc++/grpc++.h>
#include "rpc/types.h"

namespace JAAS
{
class CompileRPCInfo;

class AsyncCompileService
   {
public:

   // Inherited class MUST call callData.finish to respond and cleanup
   virtual void compile(CompileRPCInfo *callData) = 0;

   void requestCompile(ServerContext *ctx, CompileSCCRequest *req, CompilationResponder *res, grpc::ServerCompletionQueue *cq, void *tag)
      {
      _service.RequestCompile(ctx, req, res, cq, cq, tag);
      }

   CompileSCCService::AsyncService *getInnerSerice() { return &_service; }
private:
   CompileSCCService::AsyncService _service;
   };

class CompileRPCInfo
   {
public:
   CompileRPCInfo(AsyncCompileService *service, grpc::ServerCompletionQueue *cq)
      : _done(false), _service(service), _cq(cq), _writer(&_ctx), _next(nullptr)
      {
      _service->requestCompile(&_ctx, &_req, &_writer, _cq, this);
      }

   void finish(const CompileSCCReply &reply, const Status &status)
      {
      _done = true;
      _writer.Finish(reply, status, this);
      }

   void finish(const CompileSCCReply &reply)
      {
      finish(reply, Status::OK);
      }

   void finishWithOnlyCode(const uint32_t &code)
      {
      CompileSCCReply rep;
      rep.set_compilation_code(code);
      finish(rep);
      }

   void proceed()
      {
      if (!_done)
         {
         new CompileRPCInfo(_service, _cq);
         _service->compile(this);
         return;
         }
      else
         {
         delete this;
         }
      }

   CompileSCCRequest *getRequest() { return &_req; }
   ServerContext *getContext() { return &_ctx; }

   CompileRPCInfo *_next;
private:
   bool _done;
   ServerContext _ctx;
   AsyncCompileService *_service;
   grpc::ServerCompletionQueue *_cq;
   CompileSCCRequest _req;
   CompilationResponder _writer;
   };

// To create the async server:
// Inherit from AsyncCompileService and override the method:
// Compile(CompileData *) create the service and server; then call runService.
class AsyncServer
   {
public:
   ~AsyncServer()
      {
      _server->Shutdown();
      _cq->Shutdown();
      }

   AsyncServer()
      {
      std::string endpoint = "0.0.0.0:38400";
      _builder.AddListeningPort(endpoint, grpc::InsecureServerCredentials());
      _cq = _builder.AddCompletionQueue();
      }

   void runService(AsyncCompileService *service)
      {
      _service = service;
      _builder.RegisterService(_service->getInnerSerice());
      _server = _builder.BuildAndStart();
      wait();
      }

private:

   void wait()
      {
      void *tag;
      bool ok;
      new CompileRPCInfo(_service, _cq.get());
      // no exit logic currently
      while (_cq->Next(&tag, &ok))
         {
         auto rpc = static_cast<CompileRPCInfo *>(tag);
         if (!ok)
            {
            delete rpc;
            }
         else
            {
            rpc->proceed();
            }
         }
      }

   grpc::ServerBuilder _builder;
   std::unique_ptr<grpc::Server> _server;
   AsyncCompileService *_service;
   std::unique_ptr<grpc::ServerCompletionQueue> _cq;
   };


// To create the server:
// Inherit from BaseCompileService and override the method:
// Compile(RPC::ServerContext *, RPC::CompileSCCRequest *, RPC::CompileSCCReply *)
// In listening thread, create the service and server; then call runService.
class SyncServer
   {
public:
   ~SyncServer()
      {
      _server->Shutdown();
      }

   SyncServer()
      {
      std::string endpoint = "0.0.0.0:38400";
      _builder.AddListeningPort(endpoint, grpc::InsecureServerCredentials());
      }

   void runService(BaseService *service)
      {
      _builder.RegisterService(service);
      _server = _builder.BuildAndStart();
      _server->Wait();
      }

private:
   grpc::ServerBuilder _builder;
   std::unique_ptr<grpc::Server> _server;
   };

}

#endif // RPC_SERVER_H
