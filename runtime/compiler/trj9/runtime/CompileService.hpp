#ifndef COMPILE_SERVICE_H
#define COMPILE_SERVICE_H


#include "j9.h"
#include "trj9/rpc/Server.h"
#include "j9nonbuilder.h"
#include "vmaccess.h"
#include "infra/Monitor.hpp"  // TR::Monitor
#include "control/CompilationRuntime.hpp"
#include "control/CompilationController.hpp"

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

J9Method *ramMethodFromRomMethod(J9JITConfig* jitConfig, J9VMThread* vmThread, const J9ROMClass* romClass, const J9ROMMethod* romMethod)
   {
   TR_J9VMBase *fe = TR_J9VMBase::get(jitConfig, vmThread);
   J9UTF8* className = J9ROMCLASS_CLASSNAME(romClass);
   J9Class *ramClass = (J9Class*) fe->getSystemClassFromClassName((const char*) className->data, className->length, true);
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
   J9UTF8 *methodNameUTF = J9ROMNAMEANDSIGNATURE_NAME(&romMethod->nameAndSignature);
   std::string methodNameStr((const char*)methodNameUTF->data, (size_t)methodNameUTF->length);
   const char *methodName = methodNameStr.c_str();
   J9UTF8 *classNameUTF = J9ROMCLASS_CLASSNAME(romClass);
   std::string classNameStr((const char*)classNameUTF->data, (size_t)classNameUTF->length);
   const char *className = classNameStr.c_str();

   // Acquire vm access within this scope, variable is intentionally unused
   VMAccessHolder access(vmThread);

   PORT_ACCESS_FROM_JITCONFIG(jitConfig);

   if (TR::Options::getVerboseOption(TR_VerboseJaas))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JAAS,
            "Server received request to compile %s.%s", className, methodName);

   TR::CompilationInfo * compInfo = getCompilationInfo(jitConfig);
   if (!(compInfo->reloRuntime()->isROMClassInSharedCaches((UDATA)romClass, jitConfig->javaVM))) 
      { 
      if (TR::Options::getVerboseOption(TR_VerboseJaas))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JAAS,
               "ROMClass for %s is not in SCC so we cannot compile method %s. Aborting compilation", className, methodName);
      return false;
      }
   else 
      {
      if (jitConfig->javaVM->sharedClassConfig->existsCachedCodeForROMMethod(vmThread, romMethod)) 
         {
         if (TR::Options::getVerboseOption(TR_VerboseJaas))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JAAS,
                  "Method %s.%s already exists in SCC, aborting compilation.", className, methodName);
         return true;
         }
      else // do AOT compilation
         {
         J9Method *method = ramMethodFromRomMethod(jitConfig, vmThread, romClass, romMethod);
         TR_J9VMBase *fe = TR_J9VMBase::get(jitConfig, vmThread);
         char sig[1000];
         fe->printTruncatedSignature(sig, 1000, (TR_OpaqueMethodBlock*)method);
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
            TR::IlGeneratorMethodDetails details(method); 
            result = (IDATA)compInfo->compileMethod(vmThread, details, 0, async, NULL, &queued, plan);
            
            if (newPlanCreated)
               {
               if (!queued)
                  TR_OptimizationPlan::freeOptimizationPlan(plan);
               if (result)
                  {
                  if (TR::Options::getVerboseOption(TR_VerboseJaas))
                     TR_VerboseLog::writeLineLocked(TR_Vlog_JAAS,
                           "Server sucessfully compiled %s.%s", className, methodName);
                  return true;
                  } 
               else
                  {
                  if (TR::Options::getVerboseOption(TR_VerboseJaas))
                     TR_VerboseLog::writeLineLocked(TR_Vlog_JAAS,
                           "Server failed to compile %s.%s", className, methodName);
                  return false;
                  }
               }        
            else
               {
               if (TR::Options::getVerboseOption(TR_VerboseJaas))
                  TR_VerboseLog::writeLineLocked(TR_Vlog_JAAS,
                        "Server failed to compile %s.%s because a new plan could not be created.", className, methodName);
               return false;
               }
            }
         else 
            {
            if (TR::Options::getVerboseOption(TR_VerboseJaas))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JAAS,
                     "Server failed to compile %s.%s because no memory was available to create an optimization plan.", className, methodName);
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
