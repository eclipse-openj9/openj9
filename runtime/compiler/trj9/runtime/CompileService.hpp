#ifndef COMPILE_SERVICE_H
#define COMPILE_SERVICE_H


#include "j9.h"
#include "trj9/rpc/Server.h"
#include "j9nonbuilder.h"
#include "infra/Monitor.hpp"  // TR::Monitor
#include "control/CompilationRuntime.hpp"
#include "control/CompilationController.hpp"

bool doAOTCompile(J9JITConfig* jitConfig, J9VMThread* vmThread, 
                  J9ROMClass* romClass, const J9ROMMethod* romMethod)
   {
   PORT_ACCESS_FROM_JITCONFIG(jitConfig);
   TR::CompilationInfo * compInfo = getCompilationInfo(jitConfig);
   if (!(compInfo->reloRuntime()->isROMClassInSharedCaches((UDATA)romClass, jitConfig->javaVM))) 
      { 
      return false;
      }
   else 
      {
      if (jitConfig->javaVM->sharedClassConfig->existsCachedCodeForROMMethod(vmThread, romMethod)) 
         {
         return true;
         }
      else // do AOT compilation
         {
         j9tty_printf(PORTLIB, "JaaS: Server doing AOT compile for method %s.\n",
               &(J9UTF8_DATA(J9ROMNAMEANDSIGNATURE_NAME(&romMethod->nameAndSignature))));
         bool queued = false;
         TR_YesNoMaybe async = TR_no;
         TR_MethodEvent event;
         J9Method *method = getNewInstancePrototype(vmThread);
         event._eventType = TR_MethodEvent::InterpreterCounterTripped;
         event._j9method = method;
         event._oldStartPC = 0;
         event._vmThread = vmThread;
         event._classNeedingThunk = 0;
         bool newPlanCreated;
         IDATA result = 0;
         TR_OptimizationPlan *plan = TR::CompilationController::getCompilationStrategy()->processEvent(&event, &newPlanCreated);

         // if the controller decides to compile this method, trigger the compilation
         if (plan)
            {
            TR::IlGeneratorMethodDetails details((J9Method*)romMethod); 
            result = (IDATA)compInfo->compileMethod(vmThread, details, 0, async, NULL, &queued, plan);
            
            if (!queued && newPlanCreated)
               {
               TR_OptimizationPlan::freeOptimizationPlan(plan); 
               if (result)
                  {
                  j9tty_printf(PORTLIB, "Compilation for method %s %s is done.\n", 
                               romMethod->nameAndSignature.name, romMethod->nameAndSignature.signature); // types are int32_t, meaningful?
                  return true;
                  } 
               else
                  {
                  j9tty_printf(PORTLIB, "Compilation for method %s %s failed.\n", 
                               romMethod->nameAndSignature.name, romMethod->nameAndSignature.signature); 
                  return false;
                  }
               }        
            else
               {
                  j9tty_printf(PORTLIB, "Compilation was queued or a new plan could not be created.\n");
                  return false;
               }
            }
         else 
            {
            j9tty_printf(PORTLIB, "No memory available to create an optimization plan.\n");
            return false; 
            }
         }
      }
   }

class CompileService : public JAAS::AsyncCompileService
{
   public:
   CompileService(J9JITConfig *jitConfig, J9VMThread *vmThread) : _jitConfig(jitConfig), _vmThread(vmThread) { }

   void compile(JAAS::CompileCallData *ccd) override
      {
      auto req = ccd->getRequest();
      PORT_ACCESS_FROM_JITCONFIG(_jitConfig);
      UDATA sccPtr = (UDATA)_jitConfig->javaVM->sharedClassConfig->cacheDescriptorList->cacheStartAddress;
      //UDATA sccPtr = ((TR_J9SharedCache *)_jitConfig->javaVM->sharedCache())->getCacheStartAddress();
      //UDATA sccPtr = (UDATA)_jitConfig->javaVM->sharedClassConfig->sharedClassCache;
      J9ROMClass *romClass = (J9ROMClass*)(sccPtr + req->classoffset());
      J9ROMMethod *romMethod = (J9ROMMethod*)(sccPtr + req->methodoffset());
      j9tty_printf(PORTLIB, "romClass %p romMethod %p", req->classoffset(), req->methodoffset());
      bool result = doAOTCompile(_jitConfig, _vmThread, romClass, romMethod);
      JAAS::CompileSCCReply reply;
      reply.set_success(result);
      ccd->finish(reply, JAAS::Status::OK); 
      }

   private:
   J9JITConfig *_jitConfig;
   J9VMThread *_vmThread;
};




#endif
