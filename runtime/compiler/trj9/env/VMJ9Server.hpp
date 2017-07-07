#ifndef VMJ9SERVER_H
#define VMJ9SERVER_H

#include "rpc/Server.h"
#include "env/VMJ9.h"
#include "trj9/control/CompilationThread.hpp"
#include "trj9/control/MethodToBeCompiled.hpp"

class TR_J9ServerVM: public TR_J9SharedCacheVM
   {
public:
   TR_J9ServerVM(J9JITConfig *jitConfig, TR::CompilationInfo *compInfo, J9VMThread *vmContext)
      :TR_J9SharedCacheVM(jitConfig, compInfo, vmContext)
      {}

   virtual bool canMethodEnterEventBeHooked() override
      {
      JAAS::J9ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;

      if (!stream)
         return TR_J9VMBase::canMethodEnterEventBeHooked();
      stream->serverMessage()->set_info_0(JAAS::J9InformationType::canMethodEnterEventBeHooked);
      stream->writeBlocking();
      stream->readBlocking();
      return stream->clientMessage().single_bool();
      }
   };

#endif // VMJ9SERVER_H
