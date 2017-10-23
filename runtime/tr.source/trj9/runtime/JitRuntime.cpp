/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifdef WINDOWS
#include <windows.h>
#else
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#define  TIME_RESOLUTION 1000000
#endif

#if defined(_MSC_VER)
#include <malloc.h>
#endif

#include "j9.h"
#include "jitprotos.h"
#include "codegen/CodeGenerator.hpp"
#include "codegen/FrontEnd.hpp"
#include "compile/CompilationTypes.hpp"
#include "compile/Method.hpp"
#include "control/CompilationController.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "control/Recompilation.hpp"
#include "env/IO.hpp"
#include "env/TRMemory.hpp"
#include "env/PersistentInfo.hpp"
#include "env/jittypes.h"
#include "env/CompilerEnv.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/Runtime.hpp"
#include "runtime/asmprotos.h"
#include "trj9/control/CompilationRuntime.hpp"
#include "trj9/env/J2IThunk.hpp"
#include "trj9/env/j9method.h"
#include "trj9/env/ut_j9jit.h"
#include "trj9/runtime/IProfiler.hpp"

#if defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM)
#include "codegen/PicHelpers.hpp"
#include "control/Recompilation.hpp"
#include "runtime/Runtime.hpp"
#endif

#include "runtime/J9ValueProfiler.hpp"

#if defined(J9ZOS390)
static UDATA sliphandle=0;
static UDATA do_slip_func=0;
static void (*fptrl)(char*,char*,void*,void*,char*,char*,char*)=0;
extern "C"{
#include "atoe.h"
}
#endif



//---------------------------------------------------------------------
// TR::Recompilation
//---------------------------------------------------------------------

int32_t J9::Recompilation::globalSampleCount = 0;
int32_t J9::Recompilation::hwpGlobalSampleCount = 0;
int32_t J9::Recompilation::jitGlobalSampleCount = 0;
int32_t J9::Recompilation::jitRecompilationsInduced = 0;
int32_t J9::Recompilation::limitMethodsCompiled = 0;
int32_t J9::Recompilation::hotThresholdMethodsCompiled = 0;
int32_t J9::Recompilation::scorchingThresholdMethodsCompiled = 0;

#if !defined(TR_CROSS_COMPILE_ONLY)
bool
J9::Recompilation::isAlreadyBeingCompiled(
      TR_OpaqueMethodBlock *methodInfo,
      void *startPC,
      TR_FrontEnd *fe)
   {
   TR_J9VMBase *fej9 = (TR_J9VMBase *)fe;
   if (fej9->isAsyncCompilation())
      return fej9->isBeingCompiled(methodInfo, startPC);
   return TR::Recompilation::isAlreadyPreparedForRecompile(startPC);
   }
#endif

void setDllSlip(char*CodeStart,char*CodeEnd,char*dllName, TR::Compilation * comp)
{
#if defined(J9ZOS390)
   char  errBuf[512];
   UDATA rc=0;
   J9PortLibrary *portLib = jitConfig->javaVM->portLibrary;
   PORT_ACCESS_FROM_PORT(portLib);

   TR_ASSERT(comp, "Logging requires a compilation object");
   traceMsg(comp, "code start 0x%016p , code end 0x%016p ,size = %d\n",CodeStart,CodeEnd,CodeStart-CodeEnd);

   if (sliphandle==0)
      {
      rc = j9sl_open_shared_library(dllName, &sliphandle, FALSE);
      if (rc)
         {
         traceMsg(comp, "Failed to open SLIP DLL: %s (%s) %016p\n", dllName, j9error_last_error_message(), sliphandle);
         }
      }

   if (sliphandle != 0)
      {
      if (do_slip_func == 0)
         j9sl_lookup_name(sliphandle, "do_slip", &do_slip_func, (char*)NULL);
      if (do_slip_func != 0)
         {
         fptrl=(void (*)(char *, char *, void *, void *, char *, char *, char *))do_slip_func;
         (*fptrl)(CodeStart,
                  CodeEnd,
                  (void*)NULL,
                  (void*)NULL,
                  (char*)NULL,
                  (char*)NULL,
                  (char*)NULL
                 );
         }
      else if (comp)
         {
         traceMsg(comp, "\nCannot find do_slip function within SLIP DLL\n");
         }
      }
   else if (comp)
      {
      traceMsg(comp, "Cannot load slip/trap DLL\n");
      }
#endif
   return;

}


// This method is called at runtime to sample a method
//
void
J9::Recompilation::sampleMethod(
      void *thread,
      TR_FrontEnd *fe,
      void *startPC,
      int32_t codeSize,
      void *samplePC,
      void *methodInfo,
      int32_t tickCount)
   {
   TR_J9VMBase *feJ9 = (TR_J9VMBase *)fe;
   J9VMThread *vmThread = (J9VMThread *)thread;
   TR::Options * cmdLineOptions = TR::Options::getJITCmdLineOptions();
   J9JITConfig *jitConfig = getJ9JitConfigFromFE(feJ9);
   TR::CompilationInfo *compInfo = 0;
   if (jitConfig)
      compInfo = TR::CompilationInfo::get(jitConfig);

   int32_t totalSampleCount = globalSampleCount;

#if !defined(TR_CROSS_COMPILE_ONLY)
   J9Method * j9method = (J9Method *) methodInfo;

   /* Reject samples to native methods */
   if (J9_ROM_METHOD_FROM_RAM_METHOD(j9method)->modifiers & J9AccNative)
      return;

   char msg[350];  // size should be big enough to hold the whole one-line msg
   char *curMsg = msg;
   msg[0] = 0;
   bool logSampling = feJ9->isLogSamplingSet() || TrcEnabled_Trc_JIT_Sampling || TrcEnabled_Trc_JIT_Sampling_Detail;
#define SIG_SZ 150

   if (startPC == NULL)
      {
      TR_MethodEvent event;
      event._eventType = TR_MethodEvent::InterpretedMethodSample;
      event._j9method = j9method;
      event._oldStartPC = 0;
      event._vmThread = vmThread;
      event._classNeedingThunk = 0;
      bool newPlanCreated;
      TR_OptimizationPlan *plan = TR::CompilationController::getCompilationStrategy()->processEvent(&event, &newPlanCreated);
      // the plan needs to be created only when async compilation is possible
      // Otherwise the compilation will be triggerred on next invocation
      if (plan)
         {
         bool queued = false;
         feJ9->startAsyncCompile((TR_OpaqueMethodBlock *) j9method, 0, &queued, plan);
         if (!queued && newPlanCreated)
            TR_OptimizationPlan::freeOptimizationPlan(plan);
         }
      }
   else  // Sampling a compiled method
      {
      TR_MethodEvent event;
      event._eventType = TR_MethodEvent::JittedMethodSample;
      event._j9method = j9method;
      event._oldStartPC = startPC;
      event._samplePC = samplePC;
      event._vmThread = vmThread;
      event._classNeedingThunk = 0;
      bool newPlanCreated;
      TR_OptimizationPlan *plan = TR::CompilationController::getCompilationStrategy()->processEvent(&event, &newPlanCreated);
      if (plan)
         {
         bool queued = false;
         bool rc = TR::Recompilation::induceRecompilation(feJ9, startPC, &queued, plan);
         if (!queued && newPlanCreated)
            TR_OptimizationPlan::freeOptimizationPlan(plan);
         if (rc)
            TR::Recompilation::jitRecompilationsInduced++;
         }
      }
#endif
   }


bool
J9::Recompilation::induceRecompilation(
      TR_FrontEnd *fe,
      void *startPC,
      bool *queued,
      TR_OptimizationPlan *optimizationPlan)
   {
   TR_LinkageInfo              *linkageInfo = TR_LinkageInfo::get(startPC);
   TR_PersistentMethodInfo     *methodInfo;
   TR_PersistentJittedBodyInfo *bodyInfo;

   if (linkageInfo->recompilationAttempted())
      {
      // Do nothing.  Whether or not the other recomp attempt succeeds,
      // anything we do here would only interfere.
      return false;
      }
   bodyInfo = getJittedBodyInfoFromPC(startPC);
   TR_ASSERT(bodyInfo, "A method that can be recompiled must have bodyInfo");
   methodInfo = bodyInfo->getMethodInfo();
   TR_J9VMBase *fej9 = (TR_J9VMBase *)fe;

   if (fej9->isThunkArchetype(fej9->convertMethodOffsetToMethodPtr(methodInfo->getMethodInfo())))
      {
      // Do nothing. A thunk archetype should be not compiled as if it were a java ordinary method
      return false;
      }

   // If there is no plan (EDO triggered) we could create one
   //
   TR_ASSERT(optimizationPlan, "Must have an optimization plan");
   if (fej9->isAsyncCompilation())
      {
      // Schedule the compilation asynchronously
      //
      return fej9->startAsyncCompile(methodInfo->getMethodInfo(), startPC, queued, optimizationPlan);
      }
   else
      {

      // Save the optimization plan for later
      //
      TR_OptimizationPlan::_optimizationPlanMonitor->enter();
      if (methodInfo->_optimizationPlan)
         {
         if (TR::CompilationController::verbose() >= TR::CompilationController::LEVEL1)
            fprintf(stderr, "induceRecompilation: already having an optPlan saved in methodInfo\n");
         }
      else
         {
         methodInfo->_optimizationPlan  = optimizationPlan;
         if (TR::CompilationController::verbose() >= TR::CompilationController::LEVEL1)
            fprintf(stderr, "induceRecompilation: saving the plan into methodInfo\n");
         *queued = true; // to prevent the plan being deallocated by creator
         methodInfo->setNextCompileLevel(methodInfo->_optimizationPlan->getOptLevel(),
                                         methodInfo->_optimizationPlan->insertInstrumentation());
         }
       TR_OptimizationPlan::_optimizationPlanMonitor->exit();
      // Change the current method to trigger a recompilation on the next invocation
      //
      fixUpMethodCode(startPC);
      return true;
      }
   }

void induceRecompilation_unwrapper(void **argsPtr, void **resultPtr)
   {
   J9VMThread  *vmThread  = (J9VMThread*)argsPtr[1];
   void        *startPC   =              argsPtr[0];

   J9JITConfig *jitConfig = vmThread->javaVM->jitConfig;
   TR_FrontEnd * fe = TR_J9VMBase::get(jitConfig, vmThread);

   // When compilation controller is used we need to create an optimization plan
   bool queued = false;
   TR_PersistentJittedBodyInfo *bodyInfo = TR::Recompilation::getJittedBodyInfoFromPC(startPC);
   TR_ASSERT(bodyInfo, "A method that can be recompiled must have bodyInfo");
   TR_PersistentMethodInfo  *methodInfo = bodyInfo->getMethodInfo();
   TR_Hotness level = TR::Options::getJITCmdLineOptions()->getNextHotnessLevel(bodyInfo->getHasLoops(), bodyInfo->getHotness());
   // If there is no next level, lets keep the current one
   if (level == unknownHotness)
      level = bodyInfo->getHotness();
   // TODO shouldn't we increase the level here?
   TR_OptimizationPlan *optimizationPlan = TR_OptimizationPlan::alloc(level);
   if (optimizationPlan)
      {
      TR::Recompilation::induceRecompilation(fe, startPC, &queued, optimizationPlan);
      if (!queued)
         TR_OptimizationPlan::freeOptimizationPlan(optimizationPlan);
      }
   else // OOM
      {
      // very rare case
      // It's safe to ignore the compilations because these are not invalidations
      TR::CompilationInfo * compInfo = getCompilationInfo(jitConfig);
      compInfo->getPersistentInfo()->setDisableFurtherCompilation(true);
      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerbosePerformance))
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_PERF,"t=%6u Disable further compilation due to OOM while inducing a recompilation", (uint32_t)compInfo->getPersistentInfo()->getElapsedTime());
         }
      }
   }



extern "C" {


void J9FASTCALL _jitProfileParseBuffer(uintptrj_t vmThread)
   {
   J9VMThread *currentThread = (J9VMThread *)vmThread;
   J9JITConfig * jitConfg = currentThread->javaVM->jitConfig;
   TR_J9VMBase *fe = NULL;

   if (jitConfig)
      fe = TR_J9VMBase::get(jitConfig, currentThread);
   else
      return; //If we ever get in here we are having a really bad day.
   TR_ASSERT(fe, "FrontEnd must exist\n");

   TR_IProfiler *iProfiler = fe->getIProfiler();
   TR_ASSERT(iProfiler, "iProfiler must exist\n");

   iProfiler->jitProfileParseBuffer(currentThread);
   }
}

//-------------------------------------------------------------------------
//  _jitProfileAddress and _jitProfileValue are very similar except for
//  their "value" and "info" parameters.
//  Whenever you change one of them, you MUST check the other to see if
//  a similar change is needed.
//-------------------------------------------------------------------------

extern "C" {

void J9FASTCALL _jitProfileWarmCompilePICAddress(uintptrj_t address, TR_WarmCompilePICAddressInfo *info, int32_t maxNumValuesProfiled, int32_t *recompilationCounter)
   {
   if (recompilationCounter)
      {
      if (*recompilationCounter > 0)
         *recompilationCounter = (*recompilationCounter) >> 1;
      else
         {
         *recompilationCounter = 0;
         return;
         }
      }
   else
      return;

   for (int32_t i=0; i<MAX_UNLOCKED_PROFILING_VALUES; i++)
      {
      if (address == info->_address[i])
         {
         info->_frequency[i]++;
         info->_totalFrequency++;
         return;
         }
      else if (info->_frequency[i]==0)
         {
         info->_address[i] = address;
         info->_frequency[i]++;
         info->_totalFrequency++;
         return;
         }
      }

   return;
}

void J9FASTCALL _jitProfileAddress(uintptrj_t value, TR_AddressInfo *info, int32_t maxNumValuesProfiled, int32_t *recompilationCounter)
   {
   if (recompilationCounter)
      {
      if (*recompilationCounter > 0)
         *recompilationCounter = (*recompilationCounter) - 1 ;
      else
         {
         *recompilationCounter = 0;
         return;
         }
      }

   OMR::CriticalSection profilingAddress(vpMonitor);

   uintptrj_t *addrOfTotalFrequency;
   uintptrj_t totalFrequency = info->getTotalFrequency(&addrOfTotalFrequency);

   if (totalFrequency == 0)
      info->_value1 = value;
   if (info->_value1 == value)
      {
      if (totalFrequency < 0x7fffffff)
         {
         info->_frequency1++;
         totalFrequency++;
         }
      else
         {
         TR_ASSERT(0, "Total profiling frequency seems to be very large\n");
         return;
         }
      *addrOfTotalFrequency = totalFrequency;
      }
   else
      {
      if (totalFrequency >= 0x7fffffff)
         {
         TR_ASSERT(0, "Total profiling frequency seems to be very large\n");
         return;
         }

      if (maxNumValuesProfiled)
         info->incrementOrCreateExtraAddressInfo(value, &addrOfTotalFrequency, maxNumValuesProfiled);
      else
         {
         totalFrequency++;
         *addrOfTotalFrequency = totalFrequency;
         }
      }
   }
}



extern "C" {
void J9FASTCALL _jitProfileLongValue(uint64_t value, TR_LongValueInfo *info, int32_t maxNumValuesProfiled, int32_t *recompilationCounter)
   {
   if (recompilationCounter)
      {
      if (*recompilationCounter > 0)
         *recompilationCounter = (*recompilationCounter) - 1 ;
      else
         {
         *recompilationCounter = 0;
         return;
         }
      }

   OMR::CriticalSection profilingLongValue(vpMonitor);

   uintptrj_t *addrOfTotalFrequency;
   uintptrj_t totalFrequency = info->getTotalFrequency(&addrOfTotalFrequency);

   if (totalFrequency == 0)
      info->_value1 = value;
   if (info->_value1 == value)
      {
      if (totalFrequency < 0x7fffffff)
         {
         info->_frequency1++;
         totalFrequency++;
         }
      else
         {
         TR_ASSERT(0, "Total profiling frequency seems to be very large\n");
         return;
         }
      *addrOfTotalFrequency = totalFrequency;
      }
   else
      {
      if (totalFrequency >= 0x7fffffff)
         {
         TR_ASSERT(0, "Total profiling frequency seems to be very large\n");
         return;
         }

      if (maxNumValuesProfiled)
         info->incrementOrCreateExtraLongValueInfo(value, &addrOfTotalFrequency, maxNumValuesProfiled);
      else
         {
         totalFrequency++;
         *addrOfTotalFrequency = totalFrequency;
         }
      }
   }
}




extern "C" {
/*this is a work around for GCC 4.4 that caused a seg fault in this method. the detail is in CMVC 198085*/
#if defined(LINUX) && defined(TR_TARGET_S390)
     #if __GNUC__ > 4 || \
         (__GNUC__ == 4 && __GNUC_MINOR__ >= 4)
void _jitProfileBigDecimalValue(uintptrj_t value, uintptrj_t bigdecimalj9class, int32_t scaleOffset, int32_t flagOffset, TR_BigDecimalValueInfo *info, int32_t maxNumValuesProfiled, int32_t *recompilationCounter)  __attribute__((optimize(0)));
    #endif
#endif

void J9FASTCALL _jitProfileBigDecimalValue(uintptrj_t value, uintptrj_t bigdecimalj9class, int32_t scaleOffset, int32_t flagOffset, TR_BigDecimalValueInfo *info, int32_t maxNumValuesProfiled, int32_t *recompilationCounter)
   {
   if (recompilationCounter)
      {
      if (*recompilationCounter > 0)
         *recompilationCounter = (*recompilationCounter) - 1 ;
      else
         {
         *recompilationCounter = 0;
         return;
         }
      }

   OMR::CriticalSection profilingBigDecimalValue(vpMonitor);

   uintptrj_t *addrOfTotalFrequency;
   uintptrj_t totalFrequency = info->getTotalFrequency(&addrOfTotalFrequency);

   int32_t scale;
   int32_t flag;
   bool readValues = false;
   if (value)
      {
      if ((((J9Object *) value)->clazz & (UDATA)(-J9_REQUIRED_CLASS_ALIGNMENT)) == ((j9objectclass_t) bigdecimalj9class))
         {
         readValues = true;
         scale = *((int32_t *) (value + scaleOffset));

         flag = *((int32_t *) (value + flagOffset));
         flag = flag & 1;

         //printf("Called new profiling routine for BD %p with scale %d flags %d\n", bigdecimalj9class, scale, flag); fflush(stdout);
         }
      }

   if (!readValues)
      {
      totalFrequency++;
      *addrOfTotalFrequency = totalFrequency;
      return;
      }

   if (totalFrequency == 0)
      {
      info->_scale1 = scale;
      info->_flag1 = flag;
      }

   if ((info->_flag1 == flag) &&
       (info->_scale1 == scale))
      {
      if (totalFrequency < 0x7fffffff)
         {
         info->_frequency1++;
         totalFrequency++;
         }
      else
         {
         TR_ASSERT(0, "Total profiling frequency seems to be very large\n");
         return;
         }
      *addrOfTotalFrequency = totalFrequency;
      }
   else
      {
      if (totalFrequency >= 0x7fffffff)
         {
         TR_ASSERT(0, "Total profiling frequency seems to be very large\n");
         return;
         }

      if (maxNumValuesProfiled)
         info->incrementOrCreateExtraBigDecimalValueInfo(scale, value, &addrOfTotalFrequency, maxNumValuesProfiled);
      else
         {
         totalFrequency++;
         *addrOfTotalFrequency = totalFrequency;
         }
      }
   }
}


extern "C" {
void J9FASTCALL _jitProfileStringValue(uintptrj_t value, int32_t charsOffset, int32_t lengthOffset, TR_StringValueInfo *info, int32_t maxNumValuesProfiled, int32_t *recompilationCounter)
   {

   // charsOffset is the offset to the 'value' field in a String object relative to the start of the object.
   // lengthOffset is the offset to the 'count' field in a String object relative to the start of the object.

   if (recompilationCounter)
      {
      if (*recompilationCounter > 0)
         *recompilationCounter = (*recompilationCounter) - 1 ;
      else
         {
         *recompilationCounter = 0;
         return;
         }
      }

   OMR::CriticalSection profilingStringValue(vpMonitor);

   uintptrj_t *addrOfTotalFrequency;
   uintptrj_t totalFrequency = info->getTotalFrequency(&addrOfTotalFrequency);

   char *chars;
   int32_t length;
   bool readValues = false;
   if (value)
      {
      readValues = true;

#if defined(J9VM_GC_COMPRESSED_POINTERS)
      J9JavaVM *jvm = jitConfig->javaVM;
      if (!jvm)
         return;

      J9MemoryManagerFunctions * mmf = jvm->memoryManagerFunctions;
      J9VMThread *vmThread = jvm->internalVMFunctions->currentVMThread(jvm);
      int32_t result = mmf->j9gc_objaccess_compressedPointersShift(vmThread);

      chars = (char *) (( (uintptrj_t) (*((uint32_t *) (value + charsOffset)))) << result);
#else
      chars = *((char **) (value + charsOffset));
#endif

      chars = chars + (sizeof(J9IndexableObjectContiguous));

      length = *((int32_t *) (value + lengthOffset));
      if (length > 128)
         readValues = false;

      //printf("Called new profiling routine for BD %p with scale %d flags %d\n", bigdecimalj9class, scale, flag); fflush(stdout);
      }

   if (!readValues)
      {
      totalFrequency++;
      *addrOfTotalFrequency = totalFrequency;
      return;
      }

   if (totalFrequency == 0)
      {
      char *newChars = TR_StringValueInfo::createChars(length);
      memcpy(newChars, chars, 2*length);

      info->_chars1 = newChars;
      info->_length1 = length;
      }

   if ((info->_length1 == length) &&
       TR_StringValueInfo::matchStrings(info->_chars1, info->_length1, chars, length))
      {
      if (totalFrequency < 0x7fffffff)
         {
         info->_frequency1++;
         totalFrequency++;
         }
      else
         {
         TR_ASSERT(0, "Total profiling frequency seems to be very large\n");
         return;
         }

      *addrOfTotalFrequency = totalFrequency;
      }
   else
      {
      if (totalFrequency >= 0x7fffffff)
         {
         TR_ASSERT(0, "Total profiling frequency seems to be very large\n");
         return;
         }

      if (maxNumValuesProfiled)
         info->incrementOrCreateExtraStringValueInfo(chars, length, &addrOfTotalFrequency, maxNumValuesProfiled);
      else
         {
         totalFrequency++;
         *addrOfTotalFrequency = totalFrequency;
         }
      }
   }
}

extern "C" {
void J9FASTCALL _jitProfileValue(uint32_t value, TR_ValueInfo *info, int32_t maxNumValuesProfiled, int32_t *recompilationCounter)
   {

   if (info->getMaxValue() > value)
      info->setMaxValue(value);

   if (recompilationCounter)
      {
      if (*recompilationCounter > 0)
         *recompilationCounter = (*recompilationCounter) - 1 ;
      else
         {
         *recompilationCounter = 0;
         return;
         }
      }

   OMR::CriticalSection profilingValue(vpMonitor);

   uintptrj_t *addrOfTotalFrequency;
   uintptrj_t totalFrequency = info->getTotalFrequency(&addrOfTotalFrequency);

   if (totalFrequency == 0)
      info->_value1 = value;
   if (info->_value1 == value)
      {
      if (totalFrequency < 0x7fffffff)
         {
         info->_frequency1++;
         totalFrequency++;
         }
      else
         {
         TR_ASSERT(0, "Total profiling frequency seems to be very large\n");
         return;
         }
      *addrOfTotalFrequency = totalFrequency;
      }
   else
      {
      if (totalFrequency >= 0x7fffffff)
         {
         TR_ASSERT(0, "Total profiling frequency seems to be very large\n");
         return;
         }

      if (maxNumValuesProfiled)
         info->incrementOrCreateExtraValueInfo(value, &addrOfTotalFrequency, maxNumValuesProfiled);
      else
         {
         totalFrequency++;
         *addrOfTotalFrequency = totalFrequency;
         }
      }
   }
}

extern "C" {

#if defined(WINDOWS)
static uint64_t QueryCounter()
   {
   LARGE_INTEGER start;
   QueryPerformanceCounter(&start);
   return (uint64_t)start.QuadPart;
   }
#elif defined(AIXPPC)
static uint64_t QueryCounter()
   {
   timebasestruct_t tb;
   read_real_time(&tb, sizeof(tb));
   uint64_t result = tb.tb_high << 32 | tb.tb_low;
   return result;
   }
#else
static uint64_t QueryCounter()
   {
   struct timeval  tvalue;

   gettimeofday(&tvalue, 0);
   return (uint64_t)tvalue.tv_sec*TIME_RESOLUTION+(uint64_t)tvalue.tv_usec;
   }
#endif // WINDOWS

void dumpMethodsForClass(::FILE *fp, J9Class *classPointer)
   {
   J9Method * resolvedMethods = (J9Method *) classPointer->ramMethods;
   uint32_t i;
   uint32_t numMethods = classPointer->romClass->romMethodCount;

   for (i=0; i<numMethods; i++)
      {
      J9UTF8 * nameUTF8;
      J9UTF8 * signatureUTF8;
      J9UTF8 * clazzUTRF8;
      getClassNameSignatureFromMethod(((J9Method *)&(resolvedMethods[i])), clazzUTRF8, nameUTF8, signatureUTF8);

      fprintf(fp, "\t%u, %.*s.%.*s%.*s\n", &(resolvedMethods[i]), J9UTF8_LENGTH(clazzUTRF8), J9UTF8_DATA(clazzUTRF8), J9UTF8_LENGTH(nameUTF8), J9UTF8_DATA(nameUTF8), J9UTF8_LENGTH(signatureUTF8), J9UTF8_DATA(signatureUTF8));
      }
   }

void dumpInstanceFieldsForClass(::FILE *fp, J9Class *instanceClass, J9VMThread *vmThread)
   {
  J9JavaVM *javaVM = vmThread->javaVM;
   UDATA superclassIndex, depth;

   depth = J9CLASS_DEPTH(instanceClass);
   for (superclassIndex = 0; superclassIndex <= depth; superclassIndex++)
      {
      J9ROMFieldWalkState fieldWalkState;
      J9ROMFieldShape *romFieldCursor;
      J9Class* superclass;

      if (superclassIndex == depth)
         {
         superclass = instanceClass;
         }
      else
         {
         superclass = instanceClass->superclasses[superclassIndex];
         }

      romFieldCursor = romFieldsStartDo(superclass->romClass, &fieldWalkState);
      while (romFieldCursor)
         {
         if ((romFieldCursor->modifiers & J9AccStatic) == 0)
            {
            J9UTF8* nameUTF = J9ROMFIELDSHAPE_NAME(romFieldCursor);
            J9UTF8* sigUTF = J9ROMFIELDSHAPE_SIGNATURE(romFieldCursor);
            IDATA offset;

            fprintf(fp, "%u, %.*s, %.*s, %08x, ", instanceClass, J9UTF8_LENGTH(sigUTF), J9UTF8_DATA(sigUTF), J9UTF8_LENGTH(nameUTF), J9UTF8_DATA(nameUTF), romFieldCursor->modifiers);

            offset = javaVM->internalVMFunctions->instanceFieldOffset(vmThread, superclass, J9UTF8_DATA(nameUTF), J9UTF8_LENGTH(nameUTF), J9UTF8_DATA(sigUTF), J9UTF8_LENGTH(sigUTF), NULL, NULL, 0);

            if (offset >= 0)
               {
               fprintf(fp, "%d\n", offset + sizeof(J9Object));
               }
            else
               {
               fprintf(fp, "UNKNOWN\n");
               }
            }

         romFieldCursor = romFieldsNextDo(&fieldWalkState);
         }
      }
   }


void dumpClassStaticsForClass(::FILE *fp, J9Class *clazz, J9VMThread *vmThread)
   {
   J9JavaVM *javaVM = vmThread->javaVM;
   J9ROMClass *romClazz = clazz->romClass;
   J9ROMFieldShape* romFieldCursor;
   J9ROMFieldWalkState state;

   romFieldCursor = romFieldsStartDo(romClazz, &state);
   while (romFieldCursor != NULL)
      {
      if (romFieldCursor->modifiers & J9AccStatic)
         {
         J9UTF8* nameUTF = J9ROMFIELDSHAPE_NAME(romFieldCursor);
         J9UTF8* sigUTF = J9ROMFIELDSHAPE_SIGNATURE(romFieldCursor);

         fprintf(fp, "%u, %.*s, %.*s, %08x, ", clazz, J9UTF8_LENGTH(sigUTF), J9UTF8_DATA(sigUTF), J9UTF8_LENGTH(nameUTF), J9UTF8_DATA(nameUTF), romFieldCursor->modifiers);

         void *address = javaVM->internalVMFunctions->staticFieldAddress(vmThread, clazz, J9UTF8_DATA(nameUTF), J9UTF8_LENGTH(nameUTF), J9UTF8_DATA(sigUTF), J9UTF8_LENGTH(sigUTF), NULL, NULL, 0, NULL);

         if (address)
            {
            fprintf(fp, "%p\n", address);
            }
         else
            {
            fprintf(fp, "UNKNOWN\n");
            }
         }

      romFieldCursor = romFieldsNextDo(&state);
      }
   }


void dumpAllClasses(J9VMThread *vmThread)
   {
   J9JavaVM * javaVM = vmThread->javaVM;
   J9JITConfig * jitConfg = javaVM->jitConfig;
   PORT_ACCESS_FROM_JAVAVM(javaVM);
   J9ClassWalkState classWalkState;
   ::FILE *fp = NULL;
   ::FILE *methodFP = NULL;
   ::FILE *fieldFP = NULL;
   ::FILE *staticsFP = NULL;

   char fileName[256];
   J9Class * clazz = NULL;

   sprintf(fileName, "tracer-classdump-%p.txt", vmThread);

   if (!(fp = fopen(fileName, "at")))
      {
      fprintf(stderr, "Cannot open file\n");
      return;
      }

   sprintf(fileName, "tracer-methoddump-%p.txt", vmThread);

   if (!(methodFP = fopen(fileName, "at")))
      {
      fprintf(stderr, "Cannot open file\n");
      return;
      }

   sprintf(fileName, "tracer-fielddump-%p.txt", vmThread);

   if (!(fieldFP = fopen(fileName, "at")))
      {
      fprintf(stderr, "Cannot open file\n");
      return;
      }

   sprintf(fileName, "tracer-staticsdump-%p.txt", vmThread);

   if (!(staticsFP = fopen(fileName, "at")))
      {
      fprintf(stderr, "Cannot open file\n");
      return;
      }

   clazz = javaVM->internalVMFunctions->allClassesStartDo(&classWalkState, javaVM, NULL);
   IDATA foundClassToCompile = 0;
   while (clazz)
      {
      J9UTF8 * clazzUTRF8 = J9ROMCLASS_CLASSNAME(clazz->romClass);

      // print the class address
      fprintf(fp, "%u, ", clazz);

      J9ROMClass *romClass = clazz->romClass;
      // print the class name, for arrays get to the leaf array class and print that
      if (J9ROMCLASS_IS_ARRAY(romClass))
         {
         J9ArrayClass *arrayClass = (J9ArrayClass*) clazz;
         UDATA arity = arrayClass->arity;
         J9ROMClass *leafRomClass = arrayClass->leafComponentType->romClass;
         J9UTF8 *arrayClazzUTRF8;
         while(--arity) { fprintf(fp, "["); }
         if (J9ROMCLASS_IS_PRIMITIVE_TYPE(leafRomClass))
            {
            arity = arrayClass->arity;
            J9ArrayClass *leafParentClass = arrayClass;
            while(--arity) { leafParentClass = (J9ArrayClass *) leafParentClass->componentType; }
            arrayClazzUTRF8 = J9ROMCLASS_CLASSNAME(leafParentClass->romClass);

            fprintf(fp, "%.*s", J9UTF8_LENGTH(arrayClazzUTRF8), J9UTF8_DATA(arrayClazzUTRF8));
            }
         else
            {
            arrayClazzUTRF8 = J9ROMCLASS_CLASSNAME(leafRomClass);
            fprintf(fp, "[L%.*s;", J9UTF8_LENGTH(arrayClazzUTRF8), J9UTF8_DATA(arrayClazzUTRF8));
            }
         fprintf(fp, "\n");
         }
      else
         {
         fprintf(fp, "%.*s\n", J9UTF8_LENGTH(clazzUTRF8), J9UTF8_DATA(clazzUTRF8));
         }

      // now print the methods with address and name
      dumpMethodsForClass (methodFP, clazz);

      // Dump instance fields.
      //
      dumpInstanceFieldsForClass(fieldFP, clazz, vmThread);

      // Dump class statics.
      //
      dumpClassStaticsForClass(staticsFP, clazz, vmThread);

      clazz = javaVM->internalVMFunctions->allClassesNextDo(&classWalkState);
      }
   fclose(fp);
   fclose(methodFP);
   fclose(fieldFP);
   fclose(staticsFP);

   javaVM->internalVMFunctions->allClassesEndDo(&classWalkState);

   }

}


// _patchJNICallSite
#if defined(TR_HOST_X86)
  #if !defined(TR_HOST_64BIT)
extern "C" void _patchJNICallSite(J9Method *method, uint8_t *callPC, uint8_t *newAddress, TR_FrontEnd *fe, int32_t smpFlag)
   {
   intptr_t offset = newAddress - callPC - 5; // distance relative to the next instruction
   *(uint32_t*)(callPC+1) = offset; // patch the immediate in the call instruction
   }
  #else
extern "C" void _patchJNICallSite(J9Method *method, uint8_t *callPC, uint8_t *newAddress, TR_FrontEnd *fe, int32_t smpFlag)
   {
   // The code below is disabled because of a problem with direct call to JNI methods
   // through trampoline. The problem is that at the time of generation of the trampoline
   // we don't know if we did directToJNI call or we are calling the native thunk. In case
   // we are calling the thunk the code below won't work and it will give the caller address
   // of the C native and we crash.
#if 0
   TR::CodeCache *codeCache  = TR::CodeCacheManager::instance()->findCodeCacheFromPC(callPC);
   uint8_t        *trampoline = (uint8_t*)codeCache->findTrampoline(method);

   // The trampoline would look like the following
   // 49 bb ?? ?? ?? ?? ?? ?? ?? ??     mov r11, 0xaddress
   // 41 ff e3                          jmp r11
   //
   // Patch the trampoline
   //
   TR_ASSERT(*(uint16_t*)trampoline == 0xbb49,      "unexpected trampoline sequence at %p: %p", trampoline, *(uint16_t*)trampoline);
   TR_ASSERT(*(uint16_t*)(trampoline+10) == 0xff41, "unexpected trampoline sequence at %p: %p", trampoline+10, *(uint16_t*)(trampoline+10));
   *(uint8_t**)(trampoline+2) = newAddress;

   // Now patch the call in the jitted code
   //
   intptrj_t offset = trampoline - callPC - 5;
   TR_ASSERT((intptrj_t)(int32_t)offset == offset, "trampoline is not reachable!");
   *(uint32_t*)(callPC+1) = (uint32_t)offset;
#else
   // The code in directToJNI would look like
   // 49 bb ?? ?? ?? ?? ?? ?? ?? ??     mov r11, 0xaddress <--we patch here
   // 41 ff d3                          call r11
   *(uintptrj_t*)(callPC+2) = (uintptrj_t)newAddress;
#endif
   }
  #endif
#elif defined(TR_HOST_ARM)
extern "C" void _patchJNICallSite(J9Method *method, uint8_t *pc, uint8_t *newAddress, TR_FrontEnd *fe, int32_t smpFlag)
   {
   uint32_t *cursor = (uint32_t*) pc;
   TR_ASSERT(*cursor == 0xe28fe004, "unexpected JNI call sequence at %p: %p", cursor, *cursor); // add lr, pc, #4
   cursor++;
   TR_ASSERT(*cursor == 0xe51ff004, "unexpected JNI call sequence at %p: %p", cursor, *cursor); // ldr pc, [pc, #-4]
   cursor++;
   *cursor = (uint32_t)newAddress;
   TR::CodeGenerator::syncCode(pc, 12);
   }
#elif defined(TR_HOST_POWER)
extern "C" void _patchJNICallSite(J9Method *method, uint8_t *pc, uint8_t *newAddress, TR_FrontEnd *fe, int32_t smpFlag)
   {
   uintptrj_t address = (uintptrj_t) newAddress;
   uint32_t  *cursor = (uint32_t*) pc;

#if defined(TR_HOST_64BIT)
   // We expect the following code sequence:
   // 3d60 ????     lis gr11, imm16           <- patch1
   // 616b ????     ori gr11, gr11, imm16     <- patch2
   // 796b 07c6     rldicr gr11, gr11, 0x20, 0xffffffff00000000
   // 656b ????     oris gr11, gr11, imm16    <- patch3
   // 616b ????     ori  gr11, gr11, imm16
   //
   TR_ASSERT((*cursor & 0xffff0000) == 0x3d600000, "unexpected JNI call sequence at %p: %p", cursor, *cursor);
   *cursor = (*cursor & 0xffff0000) | (address >> 48 & 0x0ffff); cursor++;
   TR_ASSERT((*cursor & 0xffff0000) == 0x616b0000, "unexpected JNI call sequence at %p: %p", cursor, *cursor);
   *cursor = (*cursor & 0xffff0000) | (address >> 32 & 0x0ffff); cursor++;
   TR_ASSERT(*cursor == 0x796b07c6, "unexpected JNI call sequence at %p: %p", cursor, *cursor);
   cursor++; // skip the rldicr
   TR_ASSERT((*cursor & 0xffff0000) == 0x656b0000, "unexpected JNI call sequence at %p: %p", cursor, *cursor);
   *cursor = (*cursor & 0xffff0000) | (address >> 16 & 0x0ffff); cursor++;
   TR_ASSERT((*cursor & 0xffff0000) == 0x616b0000, "unexpected JNI call sequence at %p: %p", cursor, *cursor);
   *cursor = (*cursor & 0xffff0000) | (address & 0x0ffff);
   TR::CodeGenerator::syncCode(pc, 20);
#else
   // We expect the following code sequence:
   // 3d60 ????     lis gr11, imm16
   // 616b ????     ori gr11, gr11, imm16
   TR_ASSERT((*cursor & 0xffff0000) == 0x3d600000, "unexpected JNI call sequence at %p: %p", cursor, *cursor);
   *cursor = (*cursor & 0xffff0000) | (address >> 16 & 0x0ffff); cursor++;
   TR_ASSERT((*cursor & 0xffff0000) == 0x616b0000, "unexpected JNI call sequence at %p: %p", cursor, *cursor);
   *cursor = (*cursor & 0xffff0000) | (address & 0x0ffff);
   TR::CodeGenerator::syncCode(pc, 8);
#endif
   }
#elif defined(TR_HOST_S390)
extern "C" void _patchJNICallSite(J9Method *method, uint8_t *pc, uint8_t *newAddress, TR_FrontEnd *fe, int32_t smpFlag)
   {
   // pc corresponds to a slot in the literal pool area.
   // simply patch in the new address
   *(uint8_t**)pc = newAddress;
   }
#endif

JIT_HELPER(_prepareForOSR);

#ifdef TR_HOST_X86
JIT_HELPER(_countingRecompileMethod);
JIT_HELPER(_samplingRecompileMethod);
JIT_HELPER(_countingPatchCallSite);
JIT_HELPER(_samplingPatchCallSite);
JIT_HELPER(_induceRecompilation);

#elif defined(TR_HOST_POWER)

JIT_HELPER(_samplingPatchCallSite);
JIT_HELPER(_samplingRecompileMethod);
JIT_HELPER(_countingPatchCallSite);
JIT_HELPER(_countingRecompileMethod);
JIT_HELPER(_induceRecompilation);
JIT_HELPER(_revertToInterpreterGlue);

#elif defined(TR_HOST_ARM)

JIT_HELPER(_samplingPatchCallSite);
JIT_HELPER(_samplingRecompileMethod);
JIT_HELPER(_countingPatchCallSite);
JIT_HELPER(_countingRecompileMethod);
JIT_HELPER(_induceRecompilation);
JIT_HELPER(_revertToInterpreterGlue);

#elif defined(TR_HOST_S390)

JIT_HELPER(_countingRecompileMethod);
JIT_HELPER(_samplingRecompileMethod);
JIT_HELPER(_countingPatchCallSite);
JIT_HELPER(_samplingPatchCallSite);
JIT_HELPER(_jitProfileValueWrap);
JIT_HELPER(_jitProfileLongValueWrap);
JIT_HELPER(_jitProfileBigDecimalValueWrap);
JIT_HELPER(_jitProfileStringValueWrap);
JIT_HELPER(_jitProfileAddressWrap);
JIT_HELPER(_jitProfileValueX);
JIT_HELPER(_jitProfileLongValueX);
JIT_HELPER(_jitProfileBigDecimalValueX);
JIT_HELPER(_jitProfileStringValueX);
JIT_HELPER(_jitProfileAddressX);
JIT_HELPER(_induceRecompilation);


#endif


void initializeJitRuntimeHelperTable(char isSMP)
   {
   #define SET runtimeHelpers.setAddress
   #define SET_CONST runtimeHelpers.setConstant

#if !defined(TR_CROSS_COMPILE_ONLY)

#if defined(TR_HOST_POWER)
   PPCinitializeValueProfiler();
#else
#if defined(TR_HOST_S390)
   SET(TR_jitProfileAddress,                    (void *)_jitProfileAddressWrap,           TR_Helper);
   SET(TR_jitProfileValue,                      (void *)_jitProfileValueWrap,             TR_Helper);
   SET(TR_jitProfileLongValue,                  (void *)_jitProfileLongValueWrap,         TR_Helper);
   SET(TR_jitProfileBigDecimalValue,            (void *)_jitProfileBigDecimalValueWrap,   TR_Helper);
   SET(TR_jitProfileStringValue,                (void *)_jitProfileStringValueWrap,       TR_Helper);
   SET(TR_jitProfileParseBuffer,                (void *)_jitProfileParseBuffer,           TR_Helper);
#elif defined(TR_HOST_X86)
   SET(TR_jitProfileWarmCompilePICAddress,      (void *)_jitProfileWarmCompilePICAddress, TR_CHelper);
   SET(TR_jitProfileAddress,                    (void *)_jitProfileAddress,               TR_CHelper);
   SET(TR_jitProfileValue,                      (void *)_jitProfileValue,                 TR_CHelper);
   SET(TR_jitProfileLongValue,                  (void *)_jitProfileLongValue,             TR_CHelper);
   SET(TR_jitProfileBigDecimalValue,            (void *)_jitProfileBigDecimalValue,       TR_CHelper);
   SET(TR_jitProfileStringValue,                (void *)_jitProfileStringValue,           TR_CHelper);
   SET(TR_jitProfileParseBuffer,                (void *)_jitProfileParseBuffer,           TR_CHelper);
#else
   SET(TR_jitProfileWarmCompilePICAddress,      (void *)_jitProfileWarmCompilePICAddress, TR_Helper);
   SET(TR_jitProfileAddress,                    (void *)_jitProfileAddress,               TR_Helper);
   SET(TR_jitProfileValue,                      (void *)_jitProfileValue,                 TR_Helper);
   SET(TR_jitProfileLongValue,                  (void *)_jitProfileLongValue,             TR_Helper);
   SET(TR_jitProfileBigDecimalValue,            (void *)_jitProfileBigDecimalValue,       TR_Helper);
   SET(TR_jitProfileStringValue,                (void *)_jitProfileStringValue,           TR_Helper);
   SET(TR_jitProfileParseBuffer,                (void *)_jitProfileParseBuffer,           TR_Helper);
#endif // TR_HOST_S390
#endif // TR_HOST_POWER

#if defined(J9ZOS390)
   SET_CONST(TR_prepareForOSR,                  (void *)_prepareForOSR);
#else
   SET(TR_prepareForOSR,                        (void *)_prepareForOSR, TR_Helper);
#endif

#ifdef TR_HOST_X86

#  if defined(TR_HOST_64BIT)
   SET(TR_AMD64samplingRecompileMethod,         (void *)_samplingRecompileMethod, TR_Helper);
   SET(TR_AMD64countingRecompileMethod,         (void *)_countingRecompileMethod, TR_Helper);
   SET(TR_AMD64samplingPatchCallSite,           (void *)_samplingPatchCallSite,   TR_Helper);
   SET(TR_AMD64countingPatchCallSite,           (void *)_countingPatchCallSite,   TR_Helper);
   SET(TR_AMD64induceRecompilation,             (void *)_induceRecompilation,     TR_Helper);
#  else
   SET(TR_IA32samplingRecompileMethod,          (void *)_samplingRecompileMethod, TR_Helper);
   SET(TR_IA32countingRecompileMethod,          (void *)_countingRecompileMethod, TR_Helper);
   SET(TR_IA32samplingPatchCallSite,            (void *)_samplingPatchCallSite,   TR_Helper);
   SET(TR_IA32countingPatchCallSite,            (void *)_countingPatchCallSite,   TR_Helper);
   SET(TR_IA32induceRecompilation,              (void *)_induceRecompilation,     TR_Helper);
#  endif

#elif defined(TR_HOST_POWER)

   SET(TR_PPCsamplingRecompileMethod,           (void *)_samplingRecompileMethod, TR_Helper);
   SET(TR_PPCcountingRecompileMethod,           (void *)_countingRecompileMethod, TR_Helper);
   SET(TR_PPCsamplingPatchCallSite,             (void *)_samplingPatchCallSite,   TR_Helper);
   SET(TR_PPCcountingPatchCallSite,             (void *)_countingPatchCallSite,   TR_Helper);
   SET(TR_PPCinduceRecompilation,               (void *)_induceRecompilation,     TR_Helper);
   SET(TR_PPCrevertToInterpreterGlue,           (void *)_revertToInterpreterGlue, TR_Helper);

#elif defined(TR_HOST_ARM)
   SET(TR_ARMsamplingRecompileMethod,           (void *)_samplingRecompileMethod, TR_Helper);
   SET(TR_ARMcountingRecompileMethod,           (void *)_countingRecompileMethod, TR_Helper);
   SET(TR_ARMsamplingPatchCallSite,             (void *)_samplingPatchCallSite,   TR_Helper);
   SET(TR_ARMcountingPatchCallSite,             (void *)_countingPatchCallSite,   TR_Helper);
   SET(TR_ARMinduceRecompilation,               (void *)_induceRecompilation,     TR_Helper);
   SET(TR_ARMrevertToInterpreterGlue,           (void *)_revertToInterpreterGlue, TR_Helper);

#elif defined(TR_HOST_S390)
   SET(TR_S390samplingRecompileMethod,          (void *)_samplingRecompileMethod,   TR_Helper);
   SET(TR_S390countingRecompileMethod,          (void *)_countingRecompileMethod,   TR_Helper);
   SET(TR_S390samplingPatchCallSite,            (void *)_samplingPatchCallSite,     TR_Helper);
   SET(TR_S390countingPatchCallSite,            (void *)_countingPatchCallSite,     TR_Helper);
   SET(TR_S390jitProfileAddressC,               (void *)_jitProfileAddress,         TR_Helper);
   SET(TR_S390jitProfileValueC,                 (void *)_jitProfileValue,           TR_Helper);
   SET(TR_S390jitProfileLongValueC,             (void *)_jitProfileLongValue,       TR_Helper);
   SET(TR_S390jitProfileBigDecimalValueC,       (void *)_jitProfileBigDecimalValue, TR_Helper);
   SET(TR_S390jitProfileStringValueC,           (void *)_jitProfileStringValue,     TR_Helper);
   SET(TR_S390induceRecompilation,              (void *)_induceRecompilation,       TR_Helper);
   SET_CONST(TR_S390induceRecompilation_unwrapper, (void *) induceRecompilation_unwrapper);

// Set the Address Enviroment pointer.  Same for all routines from
// same library, so doesn't matter which routine, but currently only
// used when calling jitProfile* in zOS, so use one of them
#ifdef J9ZOS390
#define TRS390_TOC_UNWRAP_ENV(wrappedPointer) (((J9FunctionDescriptor_T *) (wrappedPointer))->ada)
#else
#define TRS390_TOC_UNWRAP_ENV(wrappedPointer) 0xdeafbeef
#endif
   SET_CONST(TR_S390CEnvironmentAddress,(void *)TRS390_TOC_UNWRAP_ENV((void *)_jitProfileValue));



#endif

#endif

#if defined(TR_HOST_ARM)
   SET(TR_releaseVMAccess, (void *)(jitConfig->javaVM->internalVMFunctions->internalReleaseVMAccess), TR_Helper);
#endif

   #undef SET
   }

#if defined (TR_TARGET_X86)
extern "C" int32_t compareAndExchange4(uint32_t *ptr, uint32_t, uint32_t);
#elif defined (TR_HOST_S390)
extern "C" int32_t _CompareAndSwap4(int32_t * addr, uint32_t, uint32_t);
#elif defined (TR_HOST_PPC)
extern "C" _tr_try_lock(uint32_t *a, uint32_t b, uint32_t c); // return 1 if compareAndSwap "c" successfully into "a"; otherwise 0;
extern "C" _tr_unlock(uint32_t *a, uint32_t b, uint32_t c);   // store "c" into "a", and returns 1;
#endif

#define PLATFORM_MONITOR_LOCKED    1
#define PLATFORM_MONITOR_UNLOCKED  0

extern "C" {
uint8_t platformLightweightLockingIsSupported()
   {
#if defined (TR_TARGET_X86) || defined (TR_HOST_S390) || defined (TR_HOST_PPC)
   return 1;
#else
   return 0;
#endif
   }

uint32_t platformTryLock(uint32_t *ptr)
   {
#if defined(TR_TARGET_X86)
   return compareAndExchange4(ptr, PLATFORM_MONITOR_UNLOCKED, PLATFORM_MONITOR_LOCKED);
#elif defined (TR_HOST_S390)
   return _CompareAndSwap4((int32_t *)ptr, PLATFORM_MONITOR_UNLOCKED, PLATFORM_MONITOR_LOCKED);
#elif defined (TR_HOST_PPC)
   return _tr_try_lock (ptr, PLATFORM_MONITOR_UNLOCKED, PLATFORM_MONITOR_LOCKED);
#else // monitor implementation
   return 0;
#endif
   }

void platformUnlock(uint32_t *ptr)
   {
#if defined(TR_TARGET_X86)
   compareAndExchange4(ptr, PLATFORM_MONITOR_LOCKED, PLATFORM_MONITOR_UNLOCKED);
#elif defined (TR_HOST_S390)
   _CompareAndSwap4((int32_t *)ptr, PLATFORM_MONITOR_LOCKED, PLATFORM_MONITOR_UNLOCKED);
#elif defined (TR_HOST_PPC)
   _tr_unlock (ptr, PLATFORM_MONITOR_LOCKED, PLATFORM_MONITOR_UNLOCKED);
#endif
   }
}

#if defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || (defined(TR_HOST_ARM))
uint32_t *getLinkageInfo(void *startPC)
   {
   return (uint32_t *)TR_LinkageInfo::get(startPC);
   }

uint32_t isRecompMethBody(void *li)
   {
   TR_LinkageInfo *linkageInfo = (TR_LinkageInfo *)li;
   return linkageInfo->isRecompMethodBody();
   }

// This method MUST be used only for methods that were AOTed and then relocated
// It marks the BodyInfo that this is an aoted method.
void fixPersistentMethodInfo(void *table)
   {
   J9JITExceptionTable *exceptionTable = (J9JITExceptionTable *)table;
   TR_PersistentJittedBodyInfo *bodyInfo = (TR_PersistentJittedBodyInfo *)exceptionTable->bodyInfo;
   void *vmMethodInfo = (void *)exceptionTable->ramMethod;
   TR_PersistentMethodInfo *methodInfo = (TR_PersistentMethodInfo *)((char *)bodyInfo + sizeof(TR_PersistentJittedBodyInfo));
   bodyInfo->setMethodInfo(methodInfo);
   methodInfo->setMethodInfo(vmMethodInfo);

   if (TR::Options::getCmdLineOptions()->getOption(TR_EnableHCR))
      {
      createClassRedefinitionPicSite(vmMethodInfo, (void *)methodInfo->getAddressOfMethodInfo(), sizeof(UDATA), 0, (OMR::RuntimeAssumption**)(&exceptionTable->runtimeAssumptionList));
      }

   bodyInfo->setStartCount(TR::Recompilation::globalSampleCount);
   bodyInfo->setOldStartCountDelta(TR::Options::_sampleThreshold);
   bodyInfo->setHotStartCountDelta(0);
   bodyInfo->setSampleIntervalCount(0);
   methodInfo->setProfileInfo(NULL);
   bodyInfo->setIsAotedBody(true);
   }
#endif

static void printMethodHandleArgs(j9object_t methodHandle, void **stack, J9VMThread *vmThread, TR_VlogTag vlogTag, TR_FrontEnd *fe)
   {
   // Note: 'stack' points at the MethodHandle on the stack

   if (stack[0] != methodHandle && TR::Options::isAnyVerboseOptionSet())
      {
      int i;
      TR_VerboseLog::vlogAcquire();; // we want all the following output to appear together in the log
      TR_VerboseLog::writeLine(TR_Vlog_FAILURE, "%p Pointer %p found on stack @ %p does not match MethodHandle %p", vmThread, stack[0], stack, methodHandle);
      TR_VerboseLog::writeLine(TR_Vlog_FAILURE, "%p   Nearby stack slots:", vmThread);
      for (i = -9; i <= 9; i++)
         {
         char *tag = "";
         void *slotValue = stack[i];
         if (slotValue == methodHandle)
            tag = " <- target MethodHandle is here";
         else if (slotValue == vmThread)
            tag = " <- current thread";
         else if (vmThread->sp <= slotValue && slotValue < vmThread->stackObject->end)
            tag = " <- stack address";
         TR_VerboseLog::writeLine(TR_Vlog_FAILURE, "%p     %p @ %+d: %p%s", vmThread, stack+i, i, slotValue, tag);
         }
      TR_VerboseLog::vlogRelease();
      }
   TR_ASSERT(stack[0] == methodHandle, "Pointer %p on stack @ %p must match MethodHandle %p", stack[0], stack, methodHandle);

   TR_J9VMBase *fej9 = (TR_J9VMBase *)fe;

   uintptrj_t sigObject = fej9->methodType_descriptor(fej9->methodHandle_type((uintptrj_t)methodHandle));
   intptrj_t  sigLength = fej9->getStringUTF8Length(sigObject);
   char *sig = (char*)alloca(sigLength+1);
   fej9->getStringUTF8(sigObject, sig, sigLength+1);

   if (vlogTag)
      {
      TR_VerboseLog::vlogAcquire();

      char *curArg;
      int numArgs = 0;
      for (char *curArg = sig+1; curArg[0] != ')'; curArg = nextSignatureArgument(curArg))
         numArgs++;

      // NOTE: The following code assumes the stack grows down

      if (numArgs == 0)
         {
         TR_VerboseLog::writeLine(vlogTag, "%p   no arguments @ %p", vmThread, stack);
         }
      else
         {
         TR_VerboseLog::writeLine(vlogTag, "%p   arguments @ %p", vmThread, stack);
         TR_VerboseLog::writeLine(vlogTag, "%p     arg " POINTER_PRINTF_FORMAT " receiver handle", vmThread, stack[0]);
         stack--;

         char *nextArg;
         for (curArg = sig+1; curArg[0] != ')'; curArg = nextArg)
            {
            nextArg = nextSignatureArgument(curArg);
            switch (curArg[0])
               {
               case 'J':
               case 'D':
                  // On 64-bit the value is in the low-address slot.
                  // Decrementing the stack pointer first and using an index of 1
                  // on it works on both 32- and 64-bit.
                  //
                  stack -= 2;
                  TR_VerboseLog::writeLine(vlogTag, "%p     arg " INT64_PRINTF_FORMAT_HEX " %.*s", vmThread, *(int64_t*)(stack+1), nextArg-curArg, curArg);
                  break;
               case 'L':
               case '[':
                  TR_VerboseLog::writeLine(vlogTag, "%p     arg " POINTER_PRINTF_FORMAT " %.*s", vmThread, (void*)(*(intptrj_t*)stack), nextArg-curArg, curArg);
                  stack -= 1;
                  break;
               default:
                  TR_VerboseLog::writeLine(vlogTag, "%p     arg 0x%x %.*s", vmThread, (int)*(int32_t*)stack, nextArg-curArg, curArg);
                  stack -= 1;
                  break;
               }
            }
         }

      TR_VerboseLog::vlogRelease();
      }

   }

#if !defined(TRANSLATE_METHODHANDLE_FLAG_CUSTOM)
#define TRANSLATE_METHODHANDLE_FLAG_CUSTOM      (1)
#define TRANSLATE_METHODHANDLE_FLAG_SYNCHRONOUS (2)
#endif

extern "C" uint8_t *compileMethodHandleThunk(j9object_t methodHandle, j9object_t arg, J9VMThread *vmThread, U_32 flags); // called from rossa.cpp
uint8_t *compileMethodHandleThunk(j9object_t methodHandle, j9object_t arg, J9VMThread *vmThread, U_32 flags)
   {
   J9JITConfig *jitConfig = vmThread->javaVM->jitConfig;
   TR_ASSERT(jitConfig, "compileMethodHandleThunk must have a jitConfig");
   TR_J9VMBase * fej9 = TR_J9VMBase::get(jitConfig, vmThread);
   TR_ASSERT(fej9->haveAccess(), "Must have VM access in compileMethodHandleThunk");

   TR::Options * cmdLineOptions = TR::Options::getCmdLineOptions();
   bool verbose = cmdLineOptions->getVerboseOption(TR_VerboseMethodHandles);
   bool details = cmdLineOptions->getVerboseOption(TR_VerboseMethodHandleDetails);

   if (verbose)
      {
      TR_VerboseLog::vlogAcquire();
      TR_VerboseLog::writeLine(TR_Vlog_MH, "%p Starting compileMethodHandleThunk on MethodHandle %p", vmThread, methodHandle);
      if (arg)
         TR_VerboseLog::write(" arg %p", arg);
      static const char *flagNames[] = { "CUSTOM", "SYNCHRONOUS" };
      for (int32_t i = 0; i < sizeof(flagNames)/sizeof(flagNames[0]); i++)
         {
         U_32 flag = 1 << i;
         if (flags & flag)
            TR_VerboseLog::write(" %s", flagNames[i]);
         }
      TR_VerboseLog::vlogRelease();
      }
   bool disabled = false;
   if (flags & TRANSLATE_METHODHANDLE_FLAG_CUSTOM)
      disabled = cmdLineOptions->getOption(TR_DisableCustomMethodHandleThunks);
   else
      disabled = cmdLineOptions->getOption(TR_DisableShareableMethodHandleThunks);
   if (disabled)
      {
      if (verbose)
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_MH, "%p   * Disabled -- aborting.", vmThread);
         }
      return NULL;
      }


   TR_OpaqueClassBlock *handleClass = fej9->getObjectClass((uintptrj_t)methodHandle);
   int32_t classNameLength;
   char *className = fej9->getClassNameChars(handleClass, classNameLength);

   //
   // RAS galore
   //

   if (details)
      {
      J9MemoryManagerFunctions * mmf = jitConfig->javaVM->memoryManagerFunctions;
      int32_t    hashCode         = mmf->j9gc_objaccess_getObjectHashCode(jitConfig->javaVM, (J9Object*)methodHandle);
      uintptrj_t methodType       = fej9->methodHandle_type((uintptrj_t)methodHandle);
      uintptrj_t descriptorObject = fej9->methodType_descriptor(methodType);
      intptrj_t  descriptorLength = fej9->getStringUTF8Length(descriptorObject);
      char      *descriptorNTS    = (char*)alloca(descriptorLength+1); // NTS = null-terminated string
      fej9->getStringUTF8(descriptorObject, descriptorNTS, descriptorLength+1);
      TR_VerboseLog::writeLineLocked(TR_Vlog_MHD, "%p   %.*s %p hash %x type %p %s", vmThread, classNameLength, className, methodHandle, hashCode, methodType, descriptorNTS);
      }
   else if (verbose)
      {
      TR_VerboseLog::writeLineLocked(TR_Vlog_MH, "%p   %.*s %p", vmThread, classNameLength, className, methodHandle);
      }

   bool isCustom = (flags & TRANSLATE_METHODHANDLE_FLAG_CUSTOM) != 0;
   void **stack = (void**)vmThread->arg0EA;
   if ((verbose || details) && !isCustom) // custom thunks arrive with a different stack shape that we don't control
      printMethodHandleArgs(methodHandle, stack, vmThread, verbose? TR_Vlog_MH : details? TR_Vlog_MHD : TR_Vlog_null, fej9);

   //
   // Now try to queue a compile for this method
   //

   uint8_t *startPC = NULL;
   if (cmdLineOptions->getOption(TR_DisableMethodHandleThunks))
      {
      if (details)
         TR_VerboseLog::writeLineLocked(TR_Vlog_MHD, "%p   Thunks disabled -- will proceed in interpreter", vmThread);
      }
   else
      {
      if (details)
         {
         uintptrj_t thunkableSignatureString = fej9->methodHandle_thunkableSignature((uintptrj_t)methodHandle);
         intptrj_t  thunkableSignatureLength = fej9->getStringUTF8Length(thunkableSignatureString);
         char *thunkSignature = (char*)alloca(thunkableSignatureLength+1);
         fej9->getStringUTF8(thunkableSignatureString, thunkSignature, thunkableSignatureLength+1);
         TR_VerboseLog::writeLineLocked(TR_Vlog_MHD, "%p   Looking up archetype for class %.*s signature %s", vmThread, classNameLength, className, thunkSignature);
         }

      J9Method *invokeExact = (J9Method*)fej9->lookupMethodHandleThunkArchetype((uintptrj_t)methodHandle);
      if (!invokeExact)
         {
         TR_ASSERT(0, "compileMethodHandleThunk must find an archetype for MethodHandle %p", methodHandle);

         // In production, continue in the interpreter.
         //
         if (verbose)
            TR_VerboseLog::writeLineLocked(TR_Vlog_MH, "%p ERROR: Failed to find thunk archetype for MethodHandle %p; continuing in interpreter", vmThread, methodHandle);
         return NULL;
         }

      // Create a global reference for the MethodHandle object.
      // Note that the compilation thread has the responsibility to delete this when it's done.
      //
      jobject handleRef = vmThread->javaVM->internalVMFunctions->j9jni_createGlobalRef((JNIEnv*)vmThread, methodHandle, false); // TODO:JSR292: Make this weak?
      jobject argRef    = arg? vmThread->javaVM->internalVMFunctions->j9jni_createGlobalRef((JNIEnv*)vmThread, arg, false) : NULL;

      // Create optimization plan
      //
      TR::CompilationInfo *compInfo = TR::CompilationInfo::get(jitConfig);
      TR_MethodEvent event =
         {
         isCustom? TR_MethodEvent::CustomMethodHandleThunk : TR_MethodEvent::ShareableMethodHandleThunk,
         invokeExact,
         0, 0, vmThread
         };
      bool newPlanCreated = false;
      TR_OptimizationPlan *plan = TR::CompilationController::getCompilationStrategy()->processEvent(&event, &newPlanCreated);
      if (plan)
         {
         // Compile
         //
         TR_YesNoMaybe isAsync = (flags & TRANSLATE_METHODHANDLE_FLAG_SYNCHRONOUS)? TR_no : TR_maybe;
         bool queued = false;
         if (details)
            TR_VerboseLog::writeLineLocked(TR_Vlog_MHD, "%p   About to request compile", vmThread);

         if (isCustom)
            {
            J9::CustomInvokeExactThunkDetails details(invokeExact, (uintptrj_t*)handleRef, (uintptrj_t*)argRef);
            startPC = (uint8_t*)compInfo->compileMethod(vmThread, details, 0, isAsync, NULL, &queued, plan);
            }
         else
            {
            J9::ShareableInvokeExactThunkDetails details(invokeExact, (uintptrj_t*)handleRef, (uintptrj_t*)argRef);
            startPC = (uint8_t*)compInfo->compileMethod(vmThread, details, 0, isAsync, NULL, &queued, plan);
            }

         if (details)
            TR_VerboseLog::writeLineLocked(TR_Vlog_MHD, "%p   Compile request returned startPC=%p queued=%d newPlanCreated=%d", vmThread, startPC, queued, newPlanCreated);
         if (!queued && newPlanCreated)
            TR_OptimizationPlan::freeOptimizationPlan(plan);
         }
      else
         {
         if (details)
            TR_VerboseLog::writeLineLocked(TR_Vlog_MHD, "%p   Thunk cannot be compile due to OOM -- will proceed in interpreter", vmThread);
         }
      }

   return startPC;
   }

JIT_HELPER(_initialInvokeExactThunkGlue);

void *initialInvokeExactThunk(j9object_t methodHandle, J9VMThread *vmThread)
   {
   J9JITConfig *jitConfig = vmThread->javaVM->jitConfig;
   TR_ASSERT(jitConfig, "initialInvokeExactThunk must have a jitConfig");
   TR_J9VMBase * fej9 = TR_J9VMBase::get(jitConfig, vmThread);
   TR_ASSERT(fej9->haveAccess(), "Must have VM access in initialInvokeExactThunk");

   TR::Options * cmdLineOptions = TR::Options::getCmdLineOptions();
   bool verbose = cmdLineOptions->getVerboseOption(TR_VerboseMethodHandles);
   bool details = cmdLineOptions->getVerboseOption(TR_VerboseMethodHandleDetails);
   if (cmdLineOptions->getVerboseOption(TR_VerboseJ2IThunks))
      verbose = details = true;

   if (verbose)
      TR_VerboseLog::writeLineLocked(TR_Vlog_MH, "%p initialInvokeExactThunk on MethodHandle %p", vmThread, methodHandle);

   uintptrj_t thunkableSignatureString = fej9->methodHandle_thunkableSignature((uintptrj_t)methodHandle);
   intptrj_t  thunkableSignatureLength = fej9->getStringUTF8Length(thunkableSignatureString);
   char *thunkSignature = (char*)alloca(thunkableSignatureLength+1);
   fej9->getStringUTF8(thunkableSignatureString, thunkSignature, thunkableSignatureLength+1);

   uintptrj_t thunkTuple = fej9->getReferenceField((uintptrj_t)methodHandle, "thunks", "Ljava/lang/invoke/ThunkTuple;");
   if (details)
      {
      TR_OpaqueClassBlock *handleClass = fej9->getObjectClass((uintptrj_t)methodHandle);
      int32_t classNameLength;
      char *className = fej9->getClassNameChars(handleClass, classNameLength);
      J9MemoryManagerFunctions * mmf = jitConfig->javaVM->memoryManagerFunctions;
      int32_t    hashCode         = mmf->j9gc_objaccess_getObjectHashCode(jitConfig->javaVM, (J9Object*)methodHandle);
      uintptrj_t methodType       = fej9->methodHandle_type((uintptrj_t)methodHandle);
      uintptrj_t descriptorObject = fej9->methodType_descriptor(methodType);
      intptrj_t  descriptorLength = fej9->getStringUTF8Length(descriptorObject);
      char      *descriptorNTS    = (char*)alloca(descriptorLength+1); // NTS = null-terminated string
      fej9->getStringUTF8(descriptorObject, descriptorNTS, descriptorLength+1);
      TR_VerboseLog::writeLineLocked(TR_Vlog_MHD, "%p   %.*s %p hash %x type %p %s", vmThread, classNameLength, className, methodHandle, hashCode, methodType, descriptorNTS);
      TR_VerboseLog::writeLineLocked(TR_Vlog_MHD, "%p   ThunkTuple %p thunkableSignature: %s", vmThread, thunkTuple, thunkSignature);
      }

   TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig);
   TR::PersistentInfo *persistentInfo = compInfo->getPersistentInfo();
   TR_J2IThunkTable *j2iThunks = persistentInfo->getInvokeExactJ2IThunkTable();

   void *addressToDispatch = NULL;
   if (j2iThunks)
      {
      TR_J2IThunk *thunk = j2iThunks->getThunk(thunkSignature, fej9);
      addressToDispatch = thunk->entryPoint();
      if (details)
         TR_VerboseLog::writeLineLocked(TR_Vlog_MHD, "%p   J2I thunk is %p %s", vmThread, addressToDispatch, thunk->terseSignature());
      }
   else
      {
      // No need for a thunk; return the helper itself
      addressToDispatch = j9ThunkInvokeExactHelperFromSignature(jitConfig, strlen(thunkSignature), thunkSignature);
      if (details)
         TR_VerboseLog::writeLineLocked(TR_Vlog_MHD, "%p   invokeExact helper is %p", vmThread, addressToDispatch);
      }

   if (cmdLineOptions->getOption(TR_DisableThunkTupleJ2I))
      {
      if (details)
         TR_VerboseLog::writeLineLocked(TR_Vlog_MHD, "%p   ThunkTuple J2I disabled -- leave ThunkTuple %p unchanged", vmThread, thunkTuple);
      }
   else
      {
      uintptrj_t fieldOffset = fej9->getInstanceFieldOffset(fej9->getObjectClass(thunkTuple), "invokeExactThunk", "J");
      bool success = fej9->compareAndSwapInt64Field(thunkTuple, "invokeExactThunk", (uint64_t)(uintptrj_t)_initialInvokeExactThunkGlue, (uint64_t)(uintptrj_t)addressToDispatch);
      // If the CAS fails, we don't care much.  It just means another thread may already have put a MH thunk pointer in there.

      if (details)
         TR_VerboseLog::writeLineLocked(TR_Vlog_MHD, "%p   %s updating ThunkTuple %p field %+d from %p to %p",
            vmThread, success? "Succeeded" : "Failed", thunkTuple, (int)fieldOffset, _initialInvokeExactThunkGlue, addressToDispatch);
      }

   return addressToDispatch;
   }

extern "C" void initialInvokeExactThunk_unwrapper(void **argsPtr, void **resPtr);
void initialInvokeExactThunk_unwrapper(void **argsPtr, void **resPtr)
   {
   J9VMThread  *vmThread     = (J9VMThread*)argsPtr[1];
   j9object_t   methodHandle = (j9object_t)argsPtr[0];
   *resPtr = initialInvokeExactThunk(methodHandle, vmThread);
   }

void methodHandleJ2I(j9object_t methodHandle, void **stack, J9VMThread *vmThread)
   {
   J9JITConfig *jitConfig = vmThread->javaVM->jitConfig;
   TR_ASSERT(jitConfig, "initialInvokeExactThunk must have a jitConfig");
   TR_J9VMBase * fej9 = TR_J9VMBase::get(jitConfig, vmThread);
   TR_ASSERT(fej9->haveAccess(), "Must have VM access in initialInvokeExactThunk");

   TR::Options * cmdLineOptions = TR::Options::getCmdLineOptions();
   bool verbose = cmdLineOptions->getVerboseOption(TR_VerboseJ2IThunks);
   if (verbose)
      {
      TR_VerboseLog::writeLineLocked(TR_Vlog_J2I, "%p J2I mh: %p sp: %p", vmThread, methodHandle, stack);
      // TODO: Adjust "stack" so it points at the MethodHandle.  +1 for return address, +N for n argument slots
      uintptrj_t methodType = fej9->getReferenceField ((uintptrj_t)methodHandle, "type",     "Ljava/lang/invoke/MethodType;");
      int32_t    argSlots   = fej9->getInt32Field     (            methodType  , "argSlots");
      void **methodHandleOnStack = stack + argSlots;
      printMethodHandleArgs(methodHandle, methodHandleOnStack, vmThread, TR_Vlog_J2I, fej9);
      }
   }

extern "C" void methodHandleJ2I_unwrapper(void **argsPtr, void **resPtr);
void methodHandleJ2I_unwrapper(void **argsPtr, void **resPtr)
   {
   J9VMThread  *vmThread     = (J9VMThread*)argsPtr[2];
   void       **stack        = (void**)argsPtr[1];
   j9object_t   methodHandle = (j9object_t)argsPtr[0];
   methodHandleJ2I(methodHandle, stack, vmThread);
   }

