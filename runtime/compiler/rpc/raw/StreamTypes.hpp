#include "rpc/gen/compile.pb.h"

namespace JITaaS
{
struct Status
   {
   const static Status OK;
   bool ok() { return true; }
   };
}
