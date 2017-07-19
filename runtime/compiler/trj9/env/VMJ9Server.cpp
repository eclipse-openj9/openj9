#include "rpc/J9Server.h"
#include "VMJ9Server.hpp"
#include "control/CompilationRuntime.hpp"
#include "trj9/control/CompilationThread.hpp"
#include "trj9/control/MethodToBeCompiled.hpp"

bool
TR_J9ServerVM::canMethodEnterEventBeHooked()
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;

   stream->serverMessage()->set_info_0(JAAS::J9InformationType::canMethodEnterEventBeHooked);
   stream->writeBlocking();
   stream->readBlocking();
   return stream->clientMessage().single_bool();
   }

bool
TR_J9ServerVM::canMethodExitEventBeHooked()
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;

   stream->serverMessage()->set_info_0(JAAS::J9InformationType::canMethodExitEventBeHooked);
   stream->writeBlocking();
   stream->readBlocking();
   return stream->clientMessage().single_bool();
   }

bool
TR_J9ServerVM::isClassLibraryMethod(TR_OpaqueMethodBlock *method, bool vettedForAOT)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;

   stream->serverMessage()->mutable_is_class_library_class()->set_j9_method((uint64_t) method);
   stream->writeBlocking();
   stream->readBlocking();
   return stream->clientMessage().single_bool();
   }

bool
TR_J9ServerVM::isClassLibraryClass(TR_OpaqueClassBlock *clazz)
   {
   JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;

   stream->serverMessage()->mutable_is_class_library_class()->set_j9_class((uint64_t) clazz);
   stream->writeBlocking();
   stream->readBlocking();
   return stream->clientMessage().single_bool();
   }
