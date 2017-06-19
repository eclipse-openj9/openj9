#ifndef JAAS_CLIENT_H
#define JAAS_CLIENT_H

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
      _reply.set_compilation_code(0);
      return _stub->Compile(&ctx, _request, &_reply);
      }

   uint32_t getReplyCode()
      {
      return _reply.compilation_code();
      }

private:
   std::unique_ptr<JAAS::CompileSCCService::Stub> _stub;
   JAAS::CompileSCCRequest _request;
   JAAS::CompileSCCReply _reply;
   };

}

#endif // JAAS_CLIENT_H
