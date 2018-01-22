#ifndef RPC_TYPES_H
#define RPC_TYPES_H

#include <grpc++/grpc++.h>
#include "rpc/gen/compile.grpc.pb.h"
#include "infra/Assert.hpp"

namespace JAAS
{
   typedef grpc::Status Status;
   typedef grpc::ClientAsyncReaderWriter<J9ClientMessage, J9ServerMessage> J9ClientReaderWriter;
   typedef grpc::ServerAsyncReaderWriter<J9ServerMessage, J9ClientMessage> J9ServerReaderWriter;

   class StreamFailure: public virtual std::exception
      {
   public:
      StreamFailure() : _message("Generic stream failure") { }
      StreamFailure(std::string message) : _message(message) { }
      virtual const char* what() const throw() { return _message.c_str(); }
   private:
      std::string _message;
      };

   class StreamCancel: public virtual std::exception
      {
   public:
      virtual const char* what() const throw() { return "compilation canceled by client"; }
      };

   class StreamTypeMismatch: public virtual StreamFailure
      {
   public:
      StreamTypeMismatch(std::string message) : StreamFailure(message) { TR_ASSERT(false, "type mismatch"); }
      };

   class StreamArityMismatch: public virtual StreamFailure
      {
   public:
      StreamArityMismatch(std::string message) : StreamFailure(message) { TR_ASSERT(false, "arity mismatch"); }
      };
}

#endif // RPC_TYPES_H
