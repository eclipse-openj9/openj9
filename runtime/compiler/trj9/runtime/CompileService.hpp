#ifndef COMPILE_SERVICE_H
#define COMPILE_SERVICE_H

#include "rpc/J9Server.h"
#include "j9.h"
#include "j9nonbuilder.h"
#include "vmaccess.h"
#include "infra/Monitor.hpp"  // TR::Monitor
#include "control/CompilationRuntime.hpp"
#include "control/CompilationController.hpp"
#include "env/ClassLoaderTable.hpp"

class VMAccessHolder
   {
public:
   VMAccessHolder(J9VMThread *vm): _vm(vm)
      {
      acquireVMAccess(_vm);
      }

   ~VMAccessHolder()
      {
      releaseVMAccess(_vm);
      }

private:
   J9VMThread *_vm;
   };


class J9CompileDispatcher : public JAAS::J9BaseCompileDispatcher
{
public:
   J9CompileDispatcher(J9JITConfig *jitConfig, J9VMThread *vmThread) : _jitConfig(jitConfig), _vmThread(vmThread) { }

   void compile(JAAS::J9ServerStream *stream) override;

private:
   J9JITConfig *_jitConfig;
   J9VMThread *_vmThread;
};




#endif
