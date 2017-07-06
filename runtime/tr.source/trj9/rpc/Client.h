#ifndef JAAS_CLIENT_H
#define JAAS_CLIENT_H

#include <grpc++/grpc++.h>
#include "rpc/types.h"

namespace JAAS
{

class J9ClientStream
   {
public:
   J9ClientStream()
      : _stub(J9CompileService::NewStub(grpc::CreateChannel("localhost:38400", grpc::InsecureChannelCredentials())))
      {}

   bool buildCompileRequest(uint32_t cOffset, uint32_t mOffset, uint32_t ccCOffset, uint32_t ccCLOffset)
      {
      _ctx.reset(new grpc::ClientContext);
      _stream = _stub->Compile(_ctx.get());

      J9CompileRequest *req = _clientMsg.mutable_compile_request();

      req->set_classoffset(cOffset);
      req->set_methodoffset(mOffset);
      req->set_classchaincoffset(ccCOffset);
      req->set_classchaincloffset(ccCLOffset);
      }

   Status waitForFinish()
      {
      return _stream->Finish();
      }

   uint32_t getReplyCode() const
      {
      return _serverMsg.compilation_code();
      }

   const JAAS::J9ServerMessage & serverMessage()
      { return _serverMsg; }

   JAAS::J9ClientMessage * clientMessage()
      { return &_clientMsg; }

   bool readBlocking()
      {
      return _stream->Read(&_serverMsg);
      }

   bool writeBlocking()
      {
      return _stream->Write(_clientMsg);
      }

private:
   std::unique_ptr<JAAS::J9CompileService::Stub> _stub;
   std::unique_ptr<grpc::ClientContext> _ctx;
   std::unique_ptr<JAAS::J9ClientReaderWriter> _stream;

   // re-useable message objects
   JAAS::J9ClientMessage _clientMsg;
   JAAS::J9ServerMessage _serverMsg;
   };

}

#endif // JAAS_CLIENT_H
