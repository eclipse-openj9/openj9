#ifndef COMPILE_SERVICE_H
#define COMPILE_SERVICE_H


#include "j9.h"
#include "trj9/rpc/Server.h"
#include "j9nonbuilder.h"
#include "vmaccess.h"
#include "infra/Monitor.hpp"  // TR::Monitor
#include "control/CompilationRuntime.hpp"
#include "control/CompilationController.hpp"

J9Method *ramMethodFromRomMethod(J9JITConfig* jitConfig, J9VMThread* vmThread, const J9ROMClass* romClass, const J9ROMMethod* romMethod)
   {
   TR_J9VMBase *fe = TR_J9VMBase::get(jitConfig, vmThread);
   J9UTF8* className = J9ROMCLASS_CLASSNAME(romClass);
   J9Class *ramClass = (J9Class*) fe->getSystemClassFromClassName((const char*) className->data, className->length);
   J9Method *ramMethods = ramClass->ramMethods;
   for (int32_t i = 0; i < romClass->romMethodCount; i++)
      {
      J9Method *curMethod = ramMethods + i;
      J9ROMMethod *curROMMethod = J9_ROM_METHOD_FROM_RAM_METHOD(curMethod);
      if (curROMMethod == romMethod)
         return curMethod;
      }
   return NULL;
   }

bool doAOTCompile(J9JITConfig* jitConfig, J9VMThread* vmThread, 
                  J9ROMClass* romClass, const J9ROMMethod* romMethod)
   {
   const char* methodName = (const char*)&(J9UTF8_DATA(J9ROMNAMEANDSIGNATURE_NAME(&romMethod->nameAndSignature)));
   const char* className = (const char*)&(J9UTF8_DATA(J9ROMCLASS_CLASSNAME(romClass)));
   acquireVMAccess(vmThread);
   PORT_ACCESS_FROM_JITCONFIG(jitConfig);
   j9tty_printf(PORTLIB, "JaaS: doAOTCompile class %s, method %s.", className, methodName);
   TR::CompilationInfo * compInfo = getCompilationInfo(jitConfig);
   if (!(compInfo->reloRuntime()->isROMClassInSharedCaches((UDATA)romClass, jitConfig->javaVM))) 
      { 
      j9tty_printf(PORTLIB, "JaaS: Class %s is not in SCC so we cannot compile method %s AOT. Aborting compilation.", className, methodName);
      return false;
      }
   else 
      {
      if (jitConfig->javaVM->sharedClassConfig->existsCachedCodeForROMMethod(vmThread, romMethod)) 
         {
         j9tty_printf(PORTLIB, "JaaS: Method %s already exists in SCC, aborting compilation.", methodName);
         return true;
         }
      else // do AOT compilation
         {
         J9Method *method = ramMethodFromRomMethod(jitConfig, vmThread, romClass, romMethod);
         j9tty_printf(PORTLIB, "JaaS: Server doing compile for method %s.\n", methodName);
         bool queued = false;
         TR_YesNoMaybe async = TR_no;
         TR_MethodEvent event;
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
                  j9tty_printf(PORTLIB, "Compilation for method %s is done.\n", methodName); // types are int32_t, meaningful?
                  return true;
                  } 
               else
                  {
                  j9tty_printf(PORTLIB, "Compilation for method %s %s failed.\n", methodName);
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
