#ifndef RPC_TYPES_H
#define RPC_TYPES_H

#include <grpc++/grpc++.h>
#include "rpc/gen/compile.grpc.pb.h"

namespace JAAS
{
   typedef grpc::Status Status;
   typedef grpc::ClientReaderWriter<J9ClientMessage, J9ServerMessage> J9ClientReaderWriter;
   typedef grpc::ServerAsyncReaderWriter<J9ServerMessage, J9ClientMessage> J9ServerReaderWriter;

   class StreamFailure: public virtual std::exception
      {
   public:
      virtual const char* what() const throw() { return "stream failed, closed or disconnected"; }
      };
}

#endif // RPC_TYPES_H
