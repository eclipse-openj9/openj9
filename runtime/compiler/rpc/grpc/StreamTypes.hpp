#include "rpc/gen/compile.grpc.pb.h"
#include <grpc++/grpc++.h>

namespace JITaaS
{
   typedef grpc::Status Status;
   typedef grpc::ClientAsyncReaderWriter<J9ClientMessage, J9ServerMessage> J9ClientReaderWriter;
   typedef grpc::ServerAsyncReaderWriter<J9ServerMessage, J9ClientMessage> J9ServerReaderWriter;
}
