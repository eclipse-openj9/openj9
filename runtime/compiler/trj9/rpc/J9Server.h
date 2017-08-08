#ifndef J9_SERVER_H
#define J9_SERVER_H

#include <grpc++/grpc++.h>
#include "rpc/types.h"
#include "rpc/ProtobufTypeConvert.hpp"

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
      return getArgs<T...>(_cMsg.mutable_data());
      }

   void finish();
   void cancel();   // Same as finish, but with Status::CANCELLED
   void finishCompilation(uint32_t code);
   void acceptNewRPC();

private:
   void readBlocking();
   void writeBlocking();

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

   void buildAndServe(J9BaseCompileDispatcher *compiler);

private:
   void serve(J9BaseCompileDispatcher *compiler);

   std::unique_ptr<grpc::Server> _server;
   J9CompileService::AsyncService _service;
   std::unique_ptr<grpc::ServerCompletionQueue> _notificationQueue;
   std::vector<std::unique_ptr<J9ServerStream>> _streams;
   };

}

#endif // J9_SERVER_H
