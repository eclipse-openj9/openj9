#include "rpc/J9Server.h"
#include "VMJ9Server.hpp"
#include "control/CompilationRuntime.hpp"
#include "trj9/control/CompilationThread.hpp"
#include "trj9/control/MethodToBeCompiled.hpp"

bool
TR_J9ServerVM::canMethodEnterEventBeHooked()
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;

   if (!stream)
      return TR_J9VMBase::canMethodEnterEventBeHooked();
   stream->serverMessage()->set_info_0(JAAS::J9InformationType::canMethodEnterEventBeHooked);
   stream->writeBlocking();
   stream->readBlocking();
   return stream->clientMessage().single_bool();
   }

bool
TR_J9ServerVM::canMethodExitEventBeHooked()
   {   
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;

   if (!stream)
      return TR_J9VMBase::canMethodExitEventBeHooked();
   stream->serverMessage()->set_info_0(JAAS::J9InformationType::canMethodExitEventBeHooked);
   stream->writeBlocking();
   stream->readBlocking();
   return stream->clientMessage().single_bool();
   }
