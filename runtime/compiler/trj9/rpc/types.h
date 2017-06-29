#ifndef RPC_TYPES_H
#define RPC_TYPES_H

#include <grpc++/grpc++.h>
#include "rpc/gen/compile.grpc.pb.h"

namespace JAAS
{
   typedef grpc::Status Status;
   typedef grpc::ClientReaderWriter<J9ClientMessage, J9ServerMessage> J9ClientStream;
   typedef grpc::ServerAsyncReaderWriter<J9ServerMessage, J9ClientMessage> J9AsyncServerStream;
}

#endif // RPC_TYPES_H
