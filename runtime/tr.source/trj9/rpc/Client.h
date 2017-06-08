#ifndef RPC_CLIENT_H
#define RPC_CLIENT_H

#include <grpc++/grpc++.h>
#include "rpc/types.h"

using JAAS::CompileSCCService;

namespace JAAS
{

class CompilationClient
   {
public:
   CompilationClient()
      : _stub(CompileSCCService::NewStub(grpc::CreateChannel("localhost:38400", grpc::InsecureChannelCredentials())))
      {}

   Status requestCompilation(uint32_t cOffset, uint32_t mOffset)
      {
      _request.set_classoffset(cOffset);
      _request.set_methodoffset(mOffset);

      grpc::ClientContext ctx;
      _reply.set_success(false);
      return _stub->Compile(&ctx, _request, &_reply);
      }

   bool wasCompilationSuccessful()
      {
      return _reply.success();
      }

private:
   std::unique_ptr<RPC::CompileSCCService::Stub> _stub;
   RPC::CompileSCCRequest _request;
   RPC::CompileSCCReply _reply;
   };

}

#endif // RPC_CLIENT_H
