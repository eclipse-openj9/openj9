#ifndef JAAS_CLIENT_H
#define JAAS_CLIENT_H

#include <grpc++/grpc++.h>
#include "rpc/types.h"

namespace JAAS
{

class CompilationClient
   {
public:
   CompilationClient()
      : _stub(J9CompileService::NewStub(grpc::CreateChannel("localhost:38400", grpc::InsecureChannelCredentials())))
      {}

   // TODO: should maybe return a const JAAS::ServerMessage& instead of having to call getReplyCode?
   void requestCompilation(uint32_t cOffset, uint32_t mOffset, uint32_t ccCOffset, uint32_t ccCLOffset)
      {
      _ctx.reset(new grpc::ClientContext);
      _stream = _stub->Compile(_ctx.get());

      _clientMsg.set_classoffset(cOffset);
      _clientMsg.set_methodoffset(mOffset);
      _clientMsg.set_classchaincoffset(ccCOffset);
      _clientMsg.set_classchaincloffset(ccCLOffset);

      _stream->Write(_clientMsg);
      _stream->Read(&_serverMsg);
      }

   Status waitForFinish()
      {
      return _stream->Finish();
      }

   uint32_t getReplyCode() const
      {
      return _serverMsg.compilation_code();
      }

private:
   std::unique_ptr<JAAS::J9CompileService::Stub> _stub;
   std::unique_ptr<grpc::ClientContext> _ctx;
   std::unique_ptr<JAAS::J9ClientStream> _stream;

   // re-useable message objects
   JAAS::J9ClientMessage _clientMsg;
   JAAS::J9ServerMessage _serverMsg;
   };

}

#endif // JAAS_CLIENT_H
