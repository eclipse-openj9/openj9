#ifndef RPC_TYPES_H
#define RPC_TYPES_H

#include <grpc++/grpc++.h>
#include "rpc/gen/compile.grpc.pb.h"

namespace JAAS
{
   typedef grpc::ServerContext ServerContext;
   typedef grpc::Server Server;
   typedef grpc::Service BaseService;
   typedef grpc::Status Status;

   // Inherit from this
   typedef CompileSCCService::Service BaseCompileService;
}

#endif // RPC_TYPES_H
