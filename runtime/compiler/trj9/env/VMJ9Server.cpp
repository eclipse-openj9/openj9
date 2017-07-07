#include "rpc/Server.h"
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

bool
TR_J9ServerVM::isInterfaceClass(TR_OpaqueClassBlock * clazzPointer)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;

   if (!stream)
      return TR_J9VMBase::isInterfaceClass(clazzPointer);
   if (_compInfo->reloRuntime()->isROMClassInSharedCaches((UDATA)(clazzPointer), _jitConfig->javaVM))
      {
      JAAS::J9InformationType_Param_1 *info = stream->serverMessage()->mutable_info_1();
      info->set_info(JAAS::J9InformationType::isInterfaceClass);
      info->set_param_1((uint32_t)(reinterpret_cast<uintptr_t>(sharedCache()->offsetInSharedCacheFromPointer((void*)clazzPointer))));
      stream->writeBlocking();
      stream->readBlocking();
      return stream->clientMessage().single_bool();
      }
   else //TODO 
      {
      TR_VerboseLog::writeLineLocked(TR_Vlog_JAAS, "******The rom class is not in the scc, I need another way to communicate with client.");
      }
   }
