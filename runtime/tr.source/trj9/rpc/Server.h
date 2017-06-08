#ifndef RPC_SERVER_H
#define RPC_SERVER_H

#include <grpc++/grpc++.h>
#include "rpc/types.h"

using grpc::ServerBuilder;

// To create the server:
// Inherit from BaseCompileService and override the method:
// Compile(RPC::ServerContext *, RPC::CompileSCCRequest *, RPC::CompileSCCReply *)
// In listening thread, create the service and service manager.
// Call buildServer which will return a server, then call server->Wait()

namespace JAAS
{

class SyncServer
   {
public:
   SyncServer()
      {
      std::string endpoint = "0.0.0.0:38400";
      _builder.AddListeningPort(endpoint, grpc::InsecureServerCredentials());
      }

   void runService(BaseService *service)
      {
      _builder.RegisterService(service);
      auto server = _builder.BuildAndStart();
      server->Wait();
      }
private:
   ServerBuilder _builder;
   };

}

#endif // RPC_SERVER_H
