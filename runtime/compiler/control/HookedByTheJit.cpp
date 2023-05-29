/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include <algorithm>
#include <limits.h>
#include <stdarg.h>
#include "bcnames.h"
#include "jithash.h"
#include "jitprotos.h"
#include "j9.h"
#include "j9cfg.h"
#include "j9modron.h"
#include "j9nonbuilder.h"
#include "j9consts.h"
#include "mmhook.h"
#include "mmomrhook.h"
#include "vmaccess.h"
#if JAVA_SPEC_VERSION >= 19
#include "HeapIteratorAPI.h"
#endif /* JAVA_SPEC_VERSION >= 19 */
#include "codegen/CodeGenerator.hpp"
#include "compile/CompilationTypes.hpp"
#include "compile/Method.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/OptimizationPlan.hpp"
#include "control/OptionsUtil.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "control/CompilationController.hpp"
#include "control/CompilationStrategy.hpp"
#include "env/ClassLoaderTable.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/J2IThunk.hpp"
#include "env/PersistentCHTable.hpp"
#include "env/PersistentInfo.hpp"
#include "env/jittypes.h"
#include "env/ClassTableCriticalSection.hpp"
#include "env/VMAccessCriticalSection.hpp"
#include "env/VMJ9.h"
#include "env/VerboseLog.hpp"
#include "il/DataTypes.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"
#include "infra/Monitor.hpp"
#include "infra/MonitorTable.hpp"
#include "infra/CriticalSection.hpp"
#include "optimizer/DebuggingCounters.hpp"
#include "optimizer/JProfilingBlock.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/HookHelpers.hpp"
#include "runtime/MethodMetaData.h"
#include "runtime/RelocationRuntime.hpp"
#include "runtime/asmprotos.h"
#include "runtime/codertinit.hpp"
#include "control/MethodToBeCompiled.hpp"
#include "control/CompilationRuntime.hpp"
#include "control/CompilationThread.hpp"
#include "env/VMJ9.h"
#include "env/j9method.h"
#include "env/ut_j9jit.h"
#include "ilgen/J9ByteCodeIlGenerator.hpp"
#include "ilgen/J9ByteCodeIterator.hpp"
#include "runtime/IProfiler.hpp"
#include "runtime/HWProfiler.hpp"
#include "env/SystemSegmentProvider.hpp"
#if defined(J9VM_OPT_JITSERVER)
#include "control/JITServerHelpers.hpp"
#include "runtime/JITServerAOTDeserializer.hpp"
#include "runtime/JITServerIProfiler.hpp"
#include "runtime/JITServerStatisticsThread.hpp"
#include "runtime/Listener.hpp"
#include "runtime/MetricsServer.hpp"
#endif /* defined(J9VM_OPT_JITSERVER) */

extern "C" {
struct J9JavaVM;
}

#if defined(J9ZOS390) && defined(TR_TARGET_32BIT)
#include <stdlib.h>

#define PSAAOLD  0x224 ///< offset of current ASCB in prefixed save area (located at address 0x0)
#define ASCBLDA  0x30  ///< offset of LDA field in ASCB
#define LDASTRTA 0x3c  ///< offset of user region start in LDA
#define LDASIZA  0x40  ///< offset of maximum user region size in LDA
#define LDAESTRA 0x4c  ///< offset of extended user region start in LDA
#define LDAESIZA 0x50  ///< offset of maximum extended user region size in LDA
#define LDACRGTP 0x98  ///< offset of user region top in LDA
#define LDAERGTP 0x9c  ///< offset of extended user region top in LDA

struct LDA {
   uint32_t padding1[15];   ///< Padding
   void *   strta;          ///< User Region Start
   uint32_t siza;           ///< Max size of the User Region
   uint32_t padding2[2];    ///< Padding
   void *   estra;          ///< Extended User Region Start
   uint32_t esiza;          ///< Max size of the Extended User Region
   uint32_t padding3[17];   ///< Padding
   void *   crgtp;          ///< User Region Top
   void *   ergtp;          ///< Extended User Region Top
};
#endif

#ifdef J9VM_JIT_RUNTIME_INSTRUMENTATION
// extern until function is added to oti/jitprotos.h
extern "C" void shutdownJITRuntimeInstrumentation(J9JavaVM *vm);
#endif

/* 1) Regular hooks
 * These fire when a normal condition occurs.
 * Ie: In the case of the "jitHookInitializeSendTarget" hook,
 * the VM triggers the event as its initializing the send targets for a class.
 * This is part of the normal code flow.
 */

/* 2) Async hooks
 * These fire when requested by an outside thread.
 * The thread requesting the async event has each thread/requested thread stop at
 * its next safepoint to acknowledge the event.
 * The interrupted thread runs the handler and then continues its execution.
 */

void
cgOnClassUnloading(void *loaderPtr)
   {
   #if defined(TR_TARGET_POWER) && defined(TR_HOST_POWER) && defined(TR_TARGET_64BIT)
      if (TR::Compiler->target.cpu.isPower())
         TR::CodeGenerator::ppcCGOnClassUnloading(loaderPtr);
   #endif
   }

extern TR::Monitor *assumptionTableMutex;

extern volatile bool shutdownSamplerThread;

#if defined(AOTRT_DLL)
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
extern void rtHookClassUnload(J9HookInterface * *, UDATA , void *, void *);
extern void rtHookClassLoaderUnload(J9HookInterface * *, UDATA , void *, void *);
#endif
#endif

#ifdef FIXUP_UNALIGNED
#ifdef J9VM_ENV_LITTLE_ENDIAN
#define BC_OP_U16(bcPtr) (((*(((uint8_t*)(bcPtr))+1))<<8) + (*((uint8_t*)(bcPtr))))
#else
#define BC_OP_U16(bcPtr) (((*((uint8_t*)(bcPtr)))<<8) + (*(((uint8_t*)(bcPtr))+1)))
#endif
#else
#define BC_OP_U16(bcPtr)  (*((uint16_t*)(bcPtr)))
#endif


TR::OptionSet *findOptionSet(J9Method *method, bool isAOT)
   {
   TR::OptionSet *optionSet = NULL;
   J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
   J9UTF8 *className;
   J9UTF8 *name;
   J9UTF8 *signature;
   getClassNameSignatureFromMethod(method, className, name, signature);
   char *methodSignature;
   char arr[1024];
   int32_t len = J9UTF8_LENGTH(className) + J9UTF8_LENGTH(name) + J9UTF8_LENGTH(signature) + 3;
   if (len < 1024)
      methodSignature = arr;
   else
      methodSignature = (char *) TR_Memory::jitPersistentAlloc(len);

   if (methodSignature)
      {
      sprintf(methodSignature, "%.*s.%.*s%.*s", J9UTF8_LENGTH(className), utf8Data(className), J9UTF8_LENGTH(name), utf8Data(name), J9UTF8_LENGTH(signature), utf8Data(signature));

      TR_FilterBST * filter = 0;
      if (TR::Options::getDebug() && TR::Options::getDebug()->getCompilationFilters())
         TR::Options::getDebug()->methodSigCanBeCompiled(methodSignature, filter, TR::Method::J9);

      int32_t index = filter ? filter->getOptionSet() : 0;
      int32_t lineNum = filter ? filter->getLineNumber() : 0;

      bool hasBackwardBranches = (J9ROMMETHOD_HAS_BACKWARDS_BRANCHES(romMethod) ? true : false);

      optionSet = TR::Options::findOptionSet(index, lineNum, methodSignature, TR::Options::getInitialHotnessLevel(hasBackwardBranches), isAOT);
      if (len >= 1024)
         TR_Memory::jitPersistentFree(methodSignature);
      }

   return optionSet;
   }

static void reportHook(J9VMThread *curThread, char *name, char *format=NULL, ...)
   {
   J9JITConfig * jitConfig = curThread->javaVM->jitConfig;
   TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig);
   if (  TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseHooks)
      || TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseHookDetails))
      {
      TR_VerboseLog::CriticalSection vlogLock;
      TR_VerboseLog::write(TR_Vlog_HK,"vmThread=%p hook %s ", curThread, name);
      if (format)
         {
         va_list args;
         va_start(args, format);
         j9jit_vprintf(jitConfig, format, args);
         va_end(args);
         }
      TR_VerboseLog::writeLine("");
      }
   }

static void reportHookFinished(J9VMThread *curThread, char *name, char *format=NULL, ...)
   {
   J9JITConfig * jitConfig = curThread->javaVM->jitConfig;
   TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig);
   if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseHookDetails))
      {
      TR_VerboseLog::CriticalSection vlogLock;
      TR_VerboseLog::writeLine(TR_Vlog_HD,"vmThread=%p hook %s finished ", curThread, name);
      if (format)
         {
         va_list args;
         va_start(args, format);
         j9jit_vprintf(jitConfig, format, args);
         va_end(args);
         }
      }
   }

static void reportHookDetail(J9VMThread *curThread, char *name, char *format, ...)
   {
   J9JITConfig * jitConfig = curThread->javaVM->jitConfig;
   TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig);
   if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseHookDetails))
      {
      TR_VerboseLog::CriticalSection vlogLock;
      TR_VerboseLog::writeLine(TR_Vlog_HD,"vmThread=%p hook %s detail ", curThread, name);
      va_list args;
      va_start(args, format);
      j9jit_vprintf(jitConfig, format, args);
      va_end(args);
      }
   }

extern "C" {

extern void freeJITConfig(J9JITConfig *);
extern void stopSamplingThread(J9JITConfig *);
extern void getOutOfIdleStates(TR::CompilationInfo::TR_SamplerStates expectedState, TR::CompilationInfo *compInfo, const char* reason);
extern void getOutOfIdleStatesUnlocked(TR::CompilationInfo::TR_SamplerStates expectedState, TR::CompilationInfo *compInfo, const char* reason);


//***************************************************************************************
//  Hooked by the JIT
//***************************************************************************************

int32_t encodeCount(int32_t count)
   {
   return (count == -1) ? 0 : (count << 1) + 1;
   }

int32_t getCount(J9ROMMethod *romMethod, TR::Options *optionsJIT, TR::Options *optionsAOT)
   {
   int32_t count;
   if (J9ROMMETHOD_HAS_BACKWARDS_BRANCHES(romMethod))
      {
      count = std::min(optionsJIT->getInitialBCount(), optionsAOT->getInitialBCount());
      }
   else
      {
      count = std::min(optionsJIT->getInitialCount(), optionsAOT->getInitialCount());
      if (TR::Options::_smallMethodBytecodeSizeThreshold > 0)
         {
         uint32_t methodSize = TR::CompilationInfo::getMethodBytecodeSize(romMethod);
         if ((int32_t)methodSize <= TR::Options::_smallMethodBytecodeSizeThreshold)
            {
            count = count << 3;
            }
         }
      }
   return count;
   }


bool sharedCacheContainsProfilingInfoForMethod(J9VMThread *vmThread, TR::CompilationInfo *compInfo, J9ROMMethod * romMethod)
   {
   J9SharedClassConfig * scConfig = compInfo->getJITConfig()->javaVM->sharedClassConfig;

   if(!scConfig)
      return false;

   unsigned char storeBuffer[1000];
   uint32_t bufferLength=1000;
   J9SharedDataDescriptor descriptor;
   descriptor.address = storeBuffer;
   descriptor.length = bufferLength;
   descriptor.type = J9SHR_ATTACHED_DATA_TYPE_JITPROFILE;
   descriptor.flags = J9SHR_ATTACHED_DATA_NO_FLAGS;

   IDATA dataIsCorrupt;
   TR_IPBCDataStorageHeader *store = (TR_IPBCDataStorageHeader *)scConfig->findAttachedData(vmThread, romMethod, &descriptor, &dataIsCorrupt);

   if (!store)
      return false;

   if (store != (TR_IPBCDataStorageHeader *)descriptor.address)  // a stronger check, as found can be error value
      return false;

   return true;
   }


/// This is a helper function used by jitHookInitializeSendTarget().
/// This function calculates for the hash value for methods using method names.
/// The hash value will be eventually added to variable `count` in `jitHookInitializeSendTarget()` under some conditions.
/// The formula for the hash value is:
///
///       hashValue[0] = HASH_INIT_VALUE;
///       hashValue[i+1] = hashValue[i] * HASH_BASE_VALUE + name[i];
///       hashValue[length(name)] will be used as the hash value
static uint32_t initializeSendTargetHelperFuncHashValueForSpreading(J9Method* method)
   {
   // extract class name, method name, and signature
   J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
   J9UTF8 *className;
   J9UTF8 *name;
   J9UTF8 *signature;
   getClassNameSignatureFromMethod(method, className, name, signature);
   char *classNameChar = utf8Data(className);
   char *nameChar = utf8Data(name);
   char *signatureChar = utf8Data(signature);

   // set the base and initial value for the hash value
   // const uint32_t HASH_BASE_VALUE = 33;
   const uint32_t HASH_INIT_VALUE = 5381;

   // get options
   TR::Options * optionsJIT = TR::Options::getJITCmdLineOptions();
   TR::Options * optionsAOT = TR::Options::getAOTCmdLineOptions();

   // extract the CAP of the hash value
   uint32_t hashValueCap;
   if (J9ROMMETHOD_HAS_BACKWARDS_BRANCHES(romMethod))
      hashValueCap = std::max(optionsJIT->getMaxSpreadCountLoopy(), optionsAOT->getMaxSpreadCountLoopy());
   else
      hashValueCap = std::max(optionsJIT->getMaxSpreadCountLoopless(), optionsAOT->getMaxSpreadCountLoopless());

   // compute the hash value
   uint32_t hashValue = HASH_INIT_VALUE;
   for (uint32_t i = 0; i < J9UTF8_LENGTH(className); i++)
      hashValue = (hashValue << 5) + hashValue + classNameChar[i];
   for (uint32_t i = 0; i < J9UTF8_LENGTH(name); i++)
      hashValue = (hashValue << 5) + hashValue + nameChar[i];
   for (uint32_t i = 0; i < J9UTF8_LENGTH(signature); i++)
      hashValue = (hashValue << 5) + hashValue + signatureChar[i];
   hashValue = hashValue % hashValueCap;

   return hashValue;
   }

static bool highCodeCacheOccupancyThresholdReached = false;

static void jitHookInitializeSendTarget(J9HookInterface * * hook, UDATA eventNum, void * eventData, void * userData)
   {
   J9VMInitializeSendTargetEvent * event = (J9VMInitializeSendTargetEvent *)eventData;

   J9VMThread  * vmThread = event->currentThread;
   J9JITConfig* jitConfig = vmThread->javaVM->jitConfig;
   TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig);

   if ((jitConfig->runtimeFlags & J9JIT_DEFER_JIT) != 0)
      return; // No need to set counts in this mode

   TR_J9VMBase * fe = TR_J9VMBase::get(jitConfig, vmThread);

   J9Method * method = event->method;
   J9ROMMethod * romMethod = fe->getROMMethodFromRAMMethod(method);

   // Allow native and abstract methods to be initialized by the interpreter
   if (romMethod->modifiers & (J9AccAbstract | J9AccNative))
      {
      TR::CompilationInfo::setInitialInvocationCountUnsynchronized(method,0);
      return;
      }

   TR::Options * optionsJIT = TR::Options::getJITCmdLineOptions();
   TR::Options * optionsAOT = TR::Options::getAOTCmdLineOptions();

   method->methodRunAddress = jitGetCountingSendTarget(vmThread, method);

   bool countInOptionSet = false;

   if (TR::Options::getJITCmdLineOptions()->anOptionSetContainsACountValue())
      {
      TR::OptionSet * optionSet = findOptionSet(method, false);
      if (optionSet)
         {
         optionsJIT = optionSet->getOptions();
         countInOptionSet = true;
         }
      }

   if (TR::Options::getAOTCmdLineOptions()->anOptionSetContainsACountValue())
      {
      TR::OptionSet * optionSet = findOptionSet(method, true);
      if (optionSet)
         {
         optionsAOT = optionSet->getOptions();
         countInOptionSet = true;
         }
      }

   int32_t count = -1; // means we didn't set the value yet

      {
      J9ROMClass *declaringClazz = J9_CLASS_FROM_METHOD(method)->romClass;
      J9UTF8 * className = J9ROMCLASS_CLASSNAME(declaringClazz);
      J9UTF8 * name = J9ROMMETHOD_NAME(romMethod);
      if (J9UTF8_LENGTH(className) == 36
          && J9UTF8_LENGTH(name) == 21
          && 0==memcmp(utf8Data(className), "com/ibm/rmi/io/FastPathForCollocated", 36)
          && 0==memcmp(utf8Data(J9ROMMETHOD_NAME(romMethod)), "isVMDeepCopySupported", 21)
         )
         {
         count = 0;
         }
      else if (J9UTF8_LENGTH(className) == 39
          && J9UTF8_LENGTH(name) == 9
          && 0 == memcmp(utf8Data(className), "java/util/concurrent/ThreadPoolExecutor", 39)
          && 0 == memcmp(utf8Data(J9ROMMETHOD_NAME(romMethod)), "runWorker", 9)
          )
         {
         count = 0;
         }
      else if (highCodeCacheOccupancyThresholdReached && !countInOptionSet && !TR::Options::getCountsAreProvidedByUser())
         {
         count = J9ROMMETHOD_HAS_BACKWARDS_BRANCHES(romMethod) ?
                     TR::Options::getHighCodeCacheOccupancyBCount() :
                     TR::Options::getHighCodeCacheOccupancyCount();
         }
      else if (TR::Options::sharedClassCache())
         {
         // The default FE may not have TR_J9SharedCache object because the FE may have
         // been created before options were processed.
         TR_J9VMBase *fej9 = TR_J9VMBase::get(jitConfig, vmThread, TR_J9VMBase::AOT_VM);
         TR_J9SharedCache *sc = fej9 ? fej9->sharedCache() : NULL;
#if defined(J9VM_INTERP_AOT_COMPILE_SUPPORT) && defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))
         if (sc && sc->isROMClassInSharedCache(J9_CLASS_FROM_METHOD(method)->romClass))
            {
            PORT_ACCESS_FROM_JAVAVM(jitConfig->javaVM);
            I_64 sharedQueryTime = 0;
            if (optionsAOT->getOption(TR_EnableSharedCacheTiming))
               sharedQueryTime = j9time_hires_clock(); // may not be good for SMP

            if (jitConfig->javaVM->sharedClassConfig->existsCachedCodeForROMMethod(vmThread, romMethod))
               {
               int32_t scount = optionsAOT->getInitialSCount();
               uint16_t newScount = 0;
               if (sc && sc->isHint(method, TR_HintFailedValidation, &newScount))
                  {
                  if ((scount == TR_QUICKSTART_INITIAL_SCOUNT) || (scount == TR_INITIAL_SCOUNT))
                     { // If scount is not user specified (coarse way due to info being lost from options parsing)
                     // TODO: Is casting the best thing to do here?
                     scount= std::min(getCount(romMethod, optionsJIT, optionsAOT), static_cast<int32_t>(newScount) ); // Find what would've been normal count for this method and
                     // make sure new scount isn't larger than that
                     if (optionsAOT->getVerboseOption(TR_VerboseSCHints) || optionsJIT->getVerboseOption(TR_VerboseSCHints))
                        TR_VerboseLog::writeLineLocked(TR_Vlog_SCHINTS,"Found hint in sc, increase scount to: %d, wanted scount: %d", scount, newScount);
                     }
                  }
               count = scount;
               compInfo->incrementNumMethodsFoundInSharedCache();
               }
            // AOT Body not in SCC, so scount was not set
            else if (!TR::Options::getCountsAreProvidedByUser() && !countInOptionSet)
               {
               bool useLowerCountsForAOTCold = false;
               if (TR::Options::getCmdLineOptions()->getOption(TR_LowerCountsForAotCold) && compInfo->isWarmSCC() == TR_no)
                  {
                  // Because C-interpreter is slower we need to rely more on jitted code
                  // This means compiling more, but we have to be careful.
                  // Let's use some smaller than normal counts in the cold run (to avoid
                  // extra compilations in the warm runs), but only if
                  //
                  // 1) The Default SCC isn't used - because we don't want to increase footprint
                  //
                  // OR
                  //
                  // 1) Quickstart - because we don't risk losing iprofiling info
                  // 2) GracePeriod - because we want to limit the number of 'extra'
                  //                  compilations and short apps are affected more
                  // 3) Bootstrap - same as above, plus if these methods get into the
                  //                SCC I would rather have non-app specific methods
                  //
                  // The danger is that very small applications that don't even get to AOT 200 methods
                  // may think that the runs are always cold


                  // J9SHR_RUNTIMEFLAG_ENABLE_CACHE_NON_BOOT_CLASSES is a good proxy for whether the
                  // Default SCC is set because, when -Xshareclasses option is used, by default non
                  // bootstrap loaded classes are put into the SCC. However, if the -Xshareclasses
                  // option isn't used, then the Default SCC only contains bootstrap loaded classes.
                  if (J9_SHARED_CACHE_DEFAULT_BOOT_SHARING(jitConfig->javaVM) &&
                      J9_ARE_ALL_BITS_SET(jitConfig->javaVM->sharedClassConfig->runtimeFlags, J9SHR_RUNTIMEFLAG_ENABLE_CACHE_NON_BOOT_CLASSES))
                     {
                     useLowerCountsForAOTCold = true;
                     }
                  else if (compInfo->getPersistentInfo()->getElapsedTime() <= (uint64_t)compInfo->getPersistentInfo()->getClassLoadingPhaseGracePeriod() &&
                           TR::Options::isQuickstartDetected() &&
                           fe->isClassLibraryMethod((TR_OpaqueMethodBlock *)method))
                     {
                     useLowerCountsForAOTCold = true;
                     }
                  }

               if (useLowerCountsForAOTCold)
                  {
                  // TODO: modify the function that reads a specified count such that
                  // if the user specifies a count or bcount on the command line that is obeyed
                  // and we don't try the following line
                  count = J9ROMMETHOD_HAS_BACKWARDS_BRANCHES(romMethod) ?
                               std::min(optionsJIT->getInitialColdRunBCount(), optionsAOT->getInitialColdRunBCount()) :
                               std::min(optionsJIT->getInitialColdRunCount(), optionsAOT->getInitialColdRunCount());
                  }
               // Increase counts for methods from non-bootstrap classes to improve throughput
               else if (TR::Options::getCmdLineOptions()->getOption(TR_IncreaseCountsForNonBootstrapMethods))
                  {
                  if (!fe->isClassLibraryMethod((TR_OpaqueMethodBlock *)method))
                     count = J9ROMMETHOD_HAS_BACKWARDS_BRANCHES(romMethod) ? TR_DEFAULT_INITIAL_BCOUNT : TR_DEFAULT_INITIAL_COUNT;
                  }
               // We may lower or increase the counts based on TR_HintMethodCompiledDuringStartup
               if (count == -1 && // Not yet changed
                   jitConfig->javaVM->phase != J9VM_PHASE_NOT_STARTUP &&
                   (TR_HintMethodCompiledDuringStartup & TR::Options::getAOTCmdLineOptions()->getEnableSCHintFlags()) && sc)
                  {
                  bool wasCompiledDuringStartup = sc->isHint(method, TR_HintMethodCompiledDuringStartup);
                  if (wasCompiledDuringStartup)
                     {
                     // Lower the counts for any method that doesn't have an AOT body,
                     // but we know it has been compiled in previous runs.
                     if (TR::Options::getCmdLineOptions()->getOption(TR_ReduceCountsForMethodsCompiledDuringStartup))
                        count = TR::Options::getCountForMethodsCompiledDuringStartup();
                     }
                  else // method was not compiled during startup
                     {
                     if (TR::Options::getCmdLineOptions()->getOption(TR_IncreaseCountsForMethodsCompiledOutsideStartup) &&
                         TR::Options::startupTimeMatters() == TR_maybe) // For TR_no the counts are already high, for TR_yes we cannot get in here
                         count = J9ROMMETHOD_HAS_BACKWARDS_BRANCHES(romMethod) ? TR_DEFAULT_INITIAL_BCOUNT : TR_DEFAULT_INITIAL_COUNT;
                     }
                  }
               }
            if (optionsAOT->getOption(TR_EnableSharedCacheTiming))
               {
               sharedQueryTime = j9time_hires_delta(sharedQueryTime, j9time_hires_clock(), J9PORT_TIME_DELTA_IN_MICROSECONDS);
               compInfo->setAotQueryTime(compInfo->getAotQueryTime() + (UDATA)sharedQueryTime);
               }
            }
         else // ROM class not in shared class cache
            {
#if !defined(J9ZOS390)  // Do not change the counts on zos at the moment since the
            // shared cache capacity is higher on this platform and by
            // increasing counts we could end up significantly impacting startup
            if (TR::Options::getCmdLineOptions()->getOption(TR_UseHigherCountsForNonSCCMethods))
               count = J9ROMMETHOD_HAS_BACKWARDS_BRANCHES(romMethod) ? TR_DEFAULT_INITIAL_BCOUNT : TR_DEFAULT_INITIAL_COUNT;
#endif // !J9ZOS390
            }
#endif // defined(J9VM_INTERP_AOT_COMPILE_SUPPORT) && defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))
         } // if (TR::Options::sharedClassCache())
      if (count == -1) // count didn't change yet
         {
         if (!TR::Options::getCountsAreProvidedByUser() &&
            fe->isClassLibraryMethod((TR_OpaqueMethodBlock *)method))
            count = J9ROMMETHOD_HAS_BACKWARDS_BRANCHES(romMethod) ? TR::Options::getCountForLoopyBootstrapMethods() : TR::Options::getCountForLooplessBootstrapMethods();
         if (count == -1)
            count = getCount(romMethod, optionsJIT, optionsAOT);

         // If add-spreading-invocation mode is enabled, add a hash value (hashed from method name) to count
         if (optionsAOT->getOption(TR_EnableCompilationSpreading) || optionsJIT->getOption(TR_EnableCompilationSpreading))
            {
            count += initializeSendTargetHelperFuncHashValueForSpreading(method);
            }
         else if (TR::Options::getCmdLineOptions()->getOption(TR_EnableEarlyCompilationDuringIdleCpu))
            {
            // Set only a fraction of a count to accelerate compilations
            if (count > 20)
               count = count * TR::Options::_countPercentageForEarlyCompilation / 100;
            // TODO: disable this mechanism after an hour or so to conserve idle time
            }
         }
      }

   // Option to display chosen counts to track possible bugs
   if (optionsJIT->getVerboseOption(TR_VerboseCounts))
      {
      char buffer[500];
      fe->printTruncatedSignature(buffer, 500, (TR_OpaqueMethodBlock *) method);
      TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "Setting count=%d for %s", count, buffer);
      }

   TR::CompilationInfo::setInitialInvocationCountUnsynchronized(method,count);

   if (TR::Options::getJITCmdLineOptions()->getOption(TR_DumpInitialMethodNamesAndCounts) || TR::Options::getAOTCmdLineOptions()->getOption(TR_DumpInitialMethodNamesAndCounts))
      {
      bool containsInfo = sharedCacheContainsProfilingInfoForMethod(vmThread, compInfo, romMethod);
      char buf[3072];
      J9UTF8 * className = J9ROMCLASS_CLASSNAME(J9_CLASS_FROM_METHOD(method)->romClass);
      J9UTF8 * name      = J9ROMMETHOD_NAME(J9_ROM_METHOD_FROM_RAM_METHOD(method));
      J9UTF8 * signature = J9ROMMETHOD_SIGNATURE(J9_ROM_METHOD_FROM_RAM_METHOD(method));
      int32_t sigLen = sprintf(buf, "%.*s.%.*s%.*s", J9UTF8_LENGTH(className), utf8Data(className), J9UTF8_LENGTH(name), utf8Data(name), J9UTF8_LENGTH(signature), utf8Data(signature));
      printf("Initial: Signature %s Count %d isLoopy %d isAOT %" OMR_PRIuPTR " is in SCC %d SCCContainsProfilingInfo %d \n",buf,TR::CompilationInfo::getInvocationCount(method),J9ROMMETHOD_HAS_BACKWARDS_BRANCHES(romMethod),
            TR::Options::sharedClassCache() ? jitConfig->javaVM->sharedClassConfig->existsCachedCodeForROMMethod(vmThread, romMethod) : 0,
            TR::Options::sharedClassCache() ? TR_J9VMBase::get(jitConfig, vmThread, TR_J9VMBase::AOT_VM)->sharedCache()->isROMClassInSharedCache(J9_CLASS_FROM_METHOD(method)->romClass) : 0,containsInfo) ; fflush(stdout);
      }
   }

#if defined(J9VM_INTERP_PROFILING_BYTECODES)

static int32_t interpreterProfilingState        = IPROFILING_STATE_OFF;
static int32_t interpreterProfilingRecordsCount = 0; // reset when state changes to IPROFILING_STATE_GOING_OFF
static int32_t interpreterProfilingJITSamples   = 0;
static int32_t interpreterProfilingINTSamples   = 0;
static int32_t interpreterProfilingMonitoringWindow = 0;
static bool    interpreterProfilingWasOnAtStartup = false;

/**
 * J9 VM hook, called when the profiling bytecode buffer is full.
 */
static void jitHookBytecodeProfiling(J9HookInterface * * hook, UDATA eventNum, void * eventData, void * userData);

static void turnOnInterpreterProfiling(J9JavaVM* javaVM,  TR::CompilationInfo * compInfo)
   {
   if (interpreterProfilingState == IPROFILING_STATE_OFF)
      {
      TR_J9VMBase * vmj9 = (TR_J9VMBase *)(TR_J9VMBase::get(javaVM->jitConfig, 0));
      TR_IProfiler *iProfiler = vmj9->getIProfiler();

      if (iProfiler->getProfilerMemoryFootprint() >= TR::Options::_iProfilerMemoryConsumptionLimit)
         return;

      J9HookInterface ** hook = javaVM->internalVMFunctions->getVMHookInterface(javaVM);

      interpreterProfilingRecordsCount = 0;
      interpreterProfilingState = IPROFILING_STATE_ON;
      interpreterProfilingJITSamples = 0;

      PORT_ACCESS_FROM_JAVAVM(javaVM);


      if ((*hook)->J9HookRegisterWithCallSite(hook, J9HOOK_VM_PROFILING_BYTECODE_BUFFER_FULL, jitHookBytecodeProfiling, OMR_GET_CALLSITE(), NULL))
         {
         j9tty_printf(PORTLIB, "Error: Unable to install J9HOOK_VM_PROFILING_BYTECODE_BUFFER_FULL listener\n");
         return;
         }
      else
         {
         if (TR::Options::getCmdLineOptions()->getOption(TR_VerboseInterpreterProfiling))
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_IPROFILER,"t=%6u IProfiler reactivated...", (uint32_t)compInfo->getPersistentInfo()->getElapsedTime());
            }
         }
      }
   }
#endif

#if defined(J9VM_JIT_DYNAMIC_LOOP_TRANSFER)
static TR_YesNoMaybe shouldInitiateDLT(J9DLTInformationBlock *dltInfo, int32_t idx, bool *bcRepeats, int32_t *hitCnt)
   {
   static int32_t triggerCount = -1;

   if (triggerCount == -1)
      {
      static char *envTrigger = feGetEnv("TR_DLTcount");
      if (envTrigger == NULL)
         {
         triggerCount = 3;
         }
      else
         triggerCount = atoi(envTrigger);
      }

   *bcRepeats = false;
   *hitCnt = -1; // -1 means unknown

   if (triggerCount <= 1)
      return TR_yes;

   J9Method *currentMethod = dltInfo->methods[idx];
   int32_t   hitCount=0, loopCnt=0, bcIdx = dltInfo->bcIndex[idx];

   idx = (idx==0)?(J9DLT_HISTORY_SIZE-1):(idx-1);

   while (loopCnt < J9DLT_HISTORY_SIZE-1)
      {
      loopCnt++;
      if (dltInfo->methods[idx] == currentMethod)
         {
         if (dltInfo->bcIndex[idx] >= bcIdx)
            *bcRepeats = true;
         hitCount++;
         }
      idx = (idx==0)?(J9DLT_HISTORY_SIZE-1):(idx-1);
      if (loopCnt==triggerCount-1 && hitCount==triggerCount-1)
         return TR_maybe;
      }

   *hitCnt = hitCount;
   if (hitCount>=triggerCount)
      return TR_maybe;

   void *extra = TR::CompilationInfo::getPCIfCompiled(currentMethod);
   if(extra && !TR::Options::getCmdLineOptions()->getOption(TR_DisableFastDLTOnLongRunningInterpreter))
      {
      TR_PersistentJittedBodyInfo *bodyInfo = TR::Recompilation::getJittedBodyInfoFromPC(extra);
      if (bodyInfo && bodyInfo->isLongRunningInterpreted())
         return TR_yes;
      }

   return TR_no;
   }

void *jitLookupDLT(J9VMThread *currentThread, J9Method *method, UDATA bcIndex)
   {
   J9JITConfig * jitConfig = currentThread->javaVM->jitConfig;

   if (!jitConfig)
      return 0;

   TR::CompilationInfo *compInfo = TR::CompilationInfo::get(jitConfig);
   void               *dltEntry = compInfo->searchForDLTRecord(method, bcIndex);

   if (!dltEntry)
      return 0;

   J9DLTInformationBlock *dltBlock = &(currentThread->dltBlock);
   dltBlock->dltSP = (uintptr_t)CONVERT_TO_RELATIVE_STACK_OFFSET(currentThread, currentThread->sp);
   dltBlock->dltEntry = dltEntry;
   return (void *)1;
   }

static UDATA dltTestIterator(J9VMThread * currentThread, J9StackWalkState * walkState)
   {
   switch(walkState->framesWalked)
      {
      case 1 :
         if (((UDATA) walkState->pc <= J9SF_MAX_SPECIAL_FRAME_TYPE) || (walkState->pc == walkState->walkThread->javaVM->callInReturnPC))
            return J9_STACKWALK_KEEP_ITERATING;
         if (walkState->jitInfo!=NULL)
            return J9_STACKWALK_STOP_ITERATING;
         walkState->userData1 = (void *)1;
         return J9_STACKWALK_STOP_ITERATING;
         break;

      case 2 :
         if (((UDATA) walkState->pc <= J9SF_MAX_SPECIAL_FRAME_TYPE) || (walkState->pc == walkState->walkThread->javaVM->callInReturnPC))
            return J9_STACKWALK_STOP_ITERATING;

         if (walkState->jitInfo!=NULL)
            return J9_STACKWALK_STOP_ITERATING;

         // We can stop at this point: candidate for sync transfer
         walkState->userData1 = (void *)2;
         return J9_STACKWALK_STOP_ITERATING;
         break;

      case 3 : // unused currently
         if (walkState->jitInfo!=NULL || ((UDATA) walkState->pc <= J9SF_MAX_SPECIAL_FRAME_TYPE) ||
             (walkState->pc == walkState->walkThread->javaVM->callInReturnPC) || (*walkState->bp & J9SF_A0_INVISIBLE_TAG))
            return J9_STACKWALK_STOP_ITERATING;
         break;
      }

   return J9_STACKWALK_KEEP_ITERATING;
   }

static void emptyJitGCMapCheck(J9VMThread * currentThread, J9StackWalkState * walkState, j9object_t * objectSlot, const void * stackLocation)
   {
   return;
   }


static void jitGCMapCheck(J9VMThread* vmThread, IDATA handlerKey, void* userData)
   {

   J9StackWalkState walkState;
   walkState.flags = J9_STACKWALK_ITERATE_O_SLOTS | J9_STACKWALK_ITERATE_HIDDEN_JIT_FRAMES | J9_STACKWALK_CHECK_I_SLOTS_FOR_OBJECTS;
   walkState.objectSlotWalkFunction = emptyJitGCMapCheck;
   walkState.maxFrames = 2;
   walkState.walkThread = vmThread;
   walkState.userData1=0;

   static char *verbose = feGetEnv("TR_GCMapCheckVerbose");
   if (verbose)
      walkState.userData1 = (void*)((uintptr_t) walkState.userData1 | 1);

   static char *local = feGetEnv("TR_GCMapCheckLocalScavenge");
   if (local)
      walkState.userData1 = (void*)((uintptr_t) walkState.userData1 | 2);

   static char *global = feGetEnv("TR_GCMapCheckGlobalScavenge");
   if (global)
      walkState.userData1 = (void*)((uintptr_t) walkState.userData1 | 4);

   vmThread->javaVM->walkStackFrames(vmThread, &walkState);


   return;
   }

void DLTLogic(J9VMThread* vmThread, TR::CompilationInfo *compInfo)
   {
   #define SIG_SZ 150
   char sig[SIG_SZ];
   if (!TR::Options::canJITCompile() ||
       TR::Options::getCmdLineOptions()->getOption(TR_FullSpeedDebug) ||
       TR::Options::getCmdLineOptions()->getOption(TR_DisableDynamicLoopTransfer) )
      return;
   if (TR::Options::_compilationDelayTime > 0 && // feature enabled
      TR::Options::_compilationDelayTime * 1000 > compInfo->getPersistentInfo()->getElapsedTime())
      return;

   J9StackWalkState walkState;
   walkState.maxFrames = 3;
   walkState.userData1 = 0;
   walkState.walkThread = vmThread;
   walkState.frameWalkFunction = dltTestIterator;
   walkState.flags = J9_STACKWALK_ITERATE_FRAMES | J9_STACKWALK_COUNT_SPECIFIED | J9_STACKWALK_SKIP_INLINES;
   vmThread->javaVM->walkStackFrames(vmThread, &walkState);
   uint8_t * startPC = 0;
   if (walkState.userData1 == 0)
      startPC = (uint8_t *)-1;

   J9DLTInformationBlock *dltBlock = &(vmThread->dltBlock);
   int32_t    idx = dltBlock->cursor + 1;
   J9ROMMethod *romMethod = NULL;
   bool         bcRepeats;
   void        *extra = NULL;

   if (startPC!=(uint8_t *)-1 &&  walkState.method!=0)
      romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(walkState.method);

   idx = (idx==J9DLT_HISTORY_SIZE) ? 0 : idx;
   dltBlock->cursor = idx;
   if (startPC ||
       walkState.method==0 ||
       (romMethod->modifiers & J9AccNative) ||
       ((intptr_t)(walkState.method->constantPool) & J9_STARTPC_JNI_NATIVE) ||
       !J9ROMMETHOD_HAS_BACKWARDS_BRANCHES(romMethod) ||
       TR::CompilationInfo::getJ9MethodVMExtra(walkState.method)==J9_JIT_NEVER_TRANSLATE ||
       (J9CLASS_FLAGS(J9_CLASS_FROM_METHOD(walkState.method)) & J9AccClassHotSwappedOut) ||
       walkState.bytecodePCOffset<=0)      // FIXME: Deal with loop back on entry later
      {
      dltBlock->methods[idx] = 0;
      return;
      }
   else
      {
      extra = TR::CompilationInfo::getPCIfCompiled(walkState.method);
      if (extra)
         {
         TR_PersistentJittedBodyInfo *bodyInfo = TR::Recompilation::getJittedBodyInfoFromPC(extra);
         if (bodyInfo && bodyInfo->getMethodInfo()->hasFailedDLTCompRetrials())
            {
            dltBlock->methods[idx] = 0;
            return;
            }
         }
      }

   // Check whether we need a DLT compilation
   bool doPerformDLT = false;
   dltBlock->methods[idx] = walkState.method;
   dltBlock->bcIndex[idx] = walkState.bytecodePCOffset;
   TR_J9ByteCode bc = TR_J9ByteCodeIterator::convertOpCodeToByteCodeEnum(J9_BYTECODE_START_FROM_ROM_METHOD(romMethod)[walkState.bytecodePCOffset]);
   if ((walkState.unwindSP - walkState.walkSP) == 0 && bc != J9BCinvokevirtual &&
      bc != J9BCinvokespecial && bc != J9BCinvokestatic && bc != J9BCinvokeinterface &&
      bc != J9BCinvokedynamic && bc != J9BCinvokehandle && bc != J9BCinvokehandlegeneric &&
      bc != J9BCinvokespecialsplit && bc != J9BCinvokestaticsplit)
      {
      int32_t numHitsInDLTBuffer = -1;
      TR_YesNoMaybe answer = shouldInitiateDLT(dltBlock, idx, &bcRepeats, &numHitsInDLTBuffer);
      if (answer != TR_no)
         {
         // Perform another test
         if (compInfo->getDLT_HT())
            {
            // If numHitsInDLTBuffer was not computed by shouldInitiateDLT do it now
            if (numHitsInDLTBuffer == -1)
               {
               // Search again for number of hits
               int32_t i = 0;
               for (numHitsInDLTBuffer = 0; i < J9DLT_HISTORY_SIZE; i++)
                  if (dltBlock->methods[i] == walkState.method)
                     numHitsInDLTBuffer++;
               }
            // Check the DLT_HT
            doPerformDLT = compInfo->getDLT_HT()->shouldIssueDLTCompilation(walkState.method, numHitsInDLTBuffer);
            }
         else // DLT_HT is not allocated, so just issue the DLT request
            {
            doPerformDLT = true;
            }
         }
      }

   if (doPerformDLT)
      {
      int32_t bcIndex = walkState.bytecodePCOffset;
      J9JITConfig * jitConfig = vmThread->javaVM->jitConfig;
      TR_J9VMBase * vm = TR_J9VMBase::get(jitConfig, vmThread);

      static char *enableDebugDLT = feGetEnv("TR_DebugDLT");
      bool dltMostOnce = false;
      int32_t enableDLTidx = -1;
      int32_t disableDLTidx = -1;
      int32_t dltOptLevel = -1;

      if (enableDebugDLT!=NULL)
         {
         TR::OptionSet *optionSet = findOptionSet(walkState.method, false);
         TR::Options *options = optionSet ? optionSet->getOptions() : NULL;

         enableDLTidx = options ? options->getEnableDLTBytecodeIndex() : -1;
         disableDLTidx = options ? options->getDisableDLTBytecodeIndex() : -1;

         if (enableDLTidx != -1)
            {
            J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(walkState.method);
            bcIndex = enableDLTidx;
            if (enableDLTidx >= (J9_BYTECODE_END_FROM_ROM_METHOD(romMethod)) - (J9_BYTECODE_START_FROM_ROM_METHOD(romMethod)))
               return;
            dltBlock->bcIndex[idx] = enableDLTidx;
            }
         if (disableDLTidx != -1 && disableDLTidx == bcIndex) return;

         dltMostOnce = options ? options->getOption(TR_DLTMostOnce) :
            TR::Options::getCmdLineOptions()->getOption(TR_DLTMostOnce);
         dltOptLevel = options ? options->getDLTOptLevel() :
            TR::Options::getCmdLineOptions()->getDLTOptLevel();
         }

      // This setup is for matching dltEntry to the right transfer point. It can be an issue only
      // in rare situations where Java code is executed for asyncEvents, leading to recursive DLT.
      dltBlock->dltSP = (uintptr_t)CONVERT_TO_RELATIVE_STACK_OFFSET(vmThread, vmThread->sp);

      dltBlock->dltEntry = compInfo->searchForDLTRecord(dltBlock->methods[idx], bcIndex);
      if (dltBlock->dltEntry != NULL)
         return;

      static char *TR_DLTmostOnce = feGetEnv("TR_DLTmostOnce");
      if (TR_DLTmostOnce != NULL || dltMostOnce)
         {
         if (compInfo->searchForDLTRecord(dltBlock->methods[idx], -1))
            return;
         }

      static char *TR_DLTforcedHot = feGetEnv("TR_DLTforcedHot");
      static char *TR_DLTforcedCold = feGetEnv("TR_DLTforcedCold"); // Usage error: set both
      TR_OptimizationPlan    *plan = NULL;
      TR_CompilationErrorCode eCode;
      bool                    queued = false;


      // TODO: add an event for the controller to decide what to do
      TR_Hotness optLevel = warm;
      // decide if we are going to compile this method to hot
      if (dltOptLevel != -1)
         {
         optLevel = (TR_Hotness)dltOptLevel;
         }
      else if (TR_DLTforcedHot!=NULL)
         {
         optLevel = hot;
         }
      else if (TR_DLTforcedCold != NULL)
         {
         optLevel = cold;
         }
      else
         {
         bool systemClass = vm->isClassLibraryMethod((TR_OpaqueMethodBlock*)walkState.method);
         if (systemClass)
            {
            optLevel = cold;
            //return;
            }
         else // userClass
            {
            // DLTs cannot be very important for large apps, so use cold compilations
            if (compInfo->getPersistentInfo()->getNumLoadedClasses() >= TR::Options::_bigAppThreshold)
               {
               optLevel = cold;
               }
            else
               {
               bool classLoadPhase = compInfo->getPersistentInfo()->isClassLoadingPhase();
               if (bcRepeats)
                  {
                  if (!classLoadPhase)
                     optLevel = hot;
                  }
               else
                  {
                  if (classLoadPhase)
                     optLevel = cold;
                  }
               }
            }
         }

      plan = TR_OptimizationPlan::alloc(optLevel);
      if (!plan)
         return;

      if (vm->isLogSamplingSet())
         {
         vm->printTruncatedSignature(sig, SIG_SZ, (TR_OpaqueMethodBlock*)walkState.method);
         TR_VerboseLog::writeLineLocked(TR_Vlog_DLT,"Will try to queue DLT compilation for %s bcIndex=%d", sig, bcIndex);
         }

      // Issue a DLT compilation request (scope for compilation request)
         {
         J9::MethodInProgressDetails details(walkState.method, dltBlock->bcIndex[dltBlock->cursor]);
         dltBlock->dltEntry = compInfo->compileMethod(vmThread, details, 0, TR_maybe, &eCode, &queued, plan);
         }

      if (walkState.userData1==(void *)2 && dltBlock->dltEntry)   // Cannot transfer synchronously
         dltBlock->dltEntry = NULL;

      for (idx=0; idx<J9DLT_HISTORY_SIZE; idx++)
         if (dltBlock->methods[idx] == walkState.method)
            dltBlock->methods[idx] = 0;

      if (!queued)
         {
         // plan must exist because we checked above
         TR_OptimizationPlan::freeOptimizationPlan(plan);
         }
      else // DLT request has been queued
         {
         // Create a request for normal JIT compilation as well
         J9Method *j9method = walkState.method;
         if (!TR::CompilationInfo::isCompiled(j9method))
            {
            // No need to check for j9method->extra != (void*)J9_JIT_QUEUED_FOR_COMPILATION
            // because we check the count value below
            int32_t count = TR::CompilationInfo::getInvocationCount(j9method);
            if (count > 0)
               {
               // Change the count to 0 as if this interpreted method got a sample
               if (TR::CompilationInfo::setInvocationCount(j9method, count, 0))
                  {
                  if (vm->isLogSamplingSet())
                     {
                     TR_VerboseLog::writeLineLocked(TR_Vlog_DLT,"side-effect: reducing count to 0 for %s", sig);
                     }
                  if (vm->isAsyncCompilation())
                     {
                     TR_MethodEvent event;
                     event._eventType = TR_MethodEvent::JitCompilationInducedByDLT;
                     event._j9method = j9method;
                     event._oldStartPC = 0;
                     event._vmThread = vmThread;
                     event._classNeedingThunk = 0;
                     bool newPlanCreated;
                     TR_OptimizationPlan *jitplan = TR::CompilationController::getCompilationStrategy()->processEvent(&event, &newPlanCreated);
                     if (jitplan)
                        {
                        bool queued1 = false;
                        vm->startAsyncCompile((TR_OpaqueMethodBlock *) j9method, 0, &queued1, jitplan);
                        if (!queued1 && newPlanCreated)
                           TR_OptimizationPlan::freeOptimizationPlan(jitplan);
                        }
                     }
                  }
               }
            }
         }
      }
   } // DLTLogic

#endif

/**
 * Stack frame iterator.  Causes a tracepoint to be triggered for each frame.
 *
 * Returns:
 * J9_STACKWALK_KEEP_ITERATING
 * J9_STACKWALK_STOP_ITERATING
 */
static UDATA
walkStackIterator(J9VMThread *currentThread, J9StackWalkState *walkState)
   {
   if(walkState->userData1 == 0)
      {
      Trc_JIT_MethodSampleStart(currentThread, walkState->method, walkState->pc, walkState->jitInfo);
      walkState->userData1 = (void *) 1;
      }
   else
      {
      Trc_JIT_MethodSampleContinue(currentThread, walkState->method, walkState->pc, walkState->jitInfo);
      }
   return J9_STACKWALK_KEEP_ITERATING;
   }

static void walkStackForSampling(J9VMThread *vmThread)
   {
   J9StackWalkState walkState;

   walkState.userData1 = 0;
   walkState.walkThread = vmThread;
   walkState.skipCount = 0;
   walkState.maxFrames = 32;  // TODO arbitrary, will become a settable property
   walkState.flags = J9_STACKWALK_VISIBLE_ONLY |
      J9_STACKWALK_INCLUDE_NATIVES |
      J9_STACKWALK_ITERATE_FRAMES;
   walkState.frameWalkFunction = walkStackIterator;

   if (vmThread->javaVM->walkStackFrames(vmThread, &walkState) != J9_STACKWALK_RC_NONE)
      {
      Trc_JIT_MethodSampleFail(vmThread, 0);
      }
   }

/**
 * Stack frame iterator (reduced version).  Causes a tracepoint to be triggered for each frame.
 * Only sends the method name.
 * Every time tries to walk two frames (sample two methods) and send them out together.
 *
 * Returns:
 * J9_STACKWALK_KEEP_ITERATING
 * J9_STACKWALK_STOP_ITERATING
 */
static UDATA
walkStackIteratorReduced(J9VMThread *currentThread, J9StackWalkState *walkState)
   {
   if(walkState->userData1 == 0)
      {
      Trc_JIT_MethodSampleStart1(currentThread, walkState->method);
      walkState->userData1 = (void *) 1;
      }
   else if(walkState->userData2 == 0)
      {
      // userData2 is used as the cache. If userData2 is NULL, the current method will be stored in userData2 temporarily
      walkState->userData2 = (void *) walkState->method;
      }
   else
      {
      // userData2 is used as the cache. If userData2 is nonempty, the previously stored method (in userData2)
      // and the current method will be sent out together, then userData2 will be cleared.
      Trc_JIT_MethodSampleContinue2(currentThread, (J9Method*) walkState->userData2, walkState->method);
      walkState->userData2 = (void *) 0;
      }
   return J9_STACKWALK_KEEP_ITERATING;
   }

static void walkStackForSamplingReduced(J9VMThread *vmThread)
   {
   J9StackWalkState walkState;

   walkState.userData1 = 0;
   walkState.userData2 = 0;
   walkState.walkThread = vmThread;
   walkState.skipCount = 0;
   walkState.maxFrames = 32;  // TODO arbitrary, will become a settable property
   walkState.flags = J9_STACKWALK_VISIBLE_ONLY |
      J9_STACKWALK_INCLUDE_NATIVES |
      J9_STACKWALK_ITERATE_FRAMES;
   walkState.frameWalkFunction = walkStackIteratorReduced;

   if (vmThread->javaVM->walkStackFrames(vmThread, &walkState) != J9_STACKWALK_RC_NONE)
      {
      Trc_JIT_MethodSampleFail(vmThread, 0);
      }
   else if(walkState.userData2 != 0)
      {
      // If there is one last method left in userData2, send it out.
      Trc_JIT_MethodSampleContinue1(vmThread, (J9Method*) walkState.userData2);
      walkState.userData2 = (void *) 0;
      }
   }

inline static bool tryAndProcessBuffers(J9VMThread *vmThread, TR_J9VMBase *vm, TR_HWProfiler *hwProfiler)
   {
   bool canEnableRI = false;
   if (hwProfiler->getProcessBufferState() >= 0)
      {
      if (vm->_hwProfilerShouldNotProcessBuffers)
         {
         vm->_hwProfilerShouldNotProcessBuffers--;
         }
      else
         {
         vm->_hwProfilerShouldNotProcessBuffers = TR::Options::_hwProfilerRIBufferProcessingFrequency;
         canEnableRI = hwProfiler->processBuffers(vmThread, vm);
         }
      }
   return canEnableRI;
   }

static void processHWPBuffer(J9VMThread *vmThread, TR_J9VMBase *vm)
   {
   TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig);
   if (!compInfo->getPersistentInfo()->isRuntimeInstrumentationEnabled())
      return; // Nothing to do in a RI disabled environment

   TR_HWProfiler *hwProfiler = compInfo->getHWProfiler();

   if (hwProfiler->isExpired())
      {
      if (IS_THREAD_RI_INITIALIZED(vmThread))
         {
         if (TR::Options::isAnyVerboseOptionSet(TR_VerboseHWProfiler))
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_HWPROFILER, "Thread %p will be de-initialized for RI because RI expiration time was reached", vmThread);
            }
         hwProfiler->deinitializeThread(vmThread);
         }
      }
   else if (hwProfiler->getProcessBufferState() >= 0 && // avoid overhead if RI is off
            hwProfiler->isHWProfilingAvailable(vmThread))
      {
      bool threadInitialized = false;
      // HW Available, but thread not initialized.
      if (!IS_THREAD_RI_INITIALIZED(vmThread))
         threadInitialized = hwProfiler->initializeThread(vmThread);
      else
         threadInitialized = true;

      bool canUseStartUsingRI = (!TR::Options::getCmdLineOptions()->getOption(TR_DisableHardwareProfilerDuringStartup)
                                 || vmThread->javaVM->phase == J9VM_PHASE_NOT_STARTUP);

      // If RI is enabled, poll buffer to swap.
      if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableHWProfilerDataCollection)
          && threadInitialized
          && canUseStartUsingRI)
         {
         if (IS_THREAD_RI_ENABLED(vmThread))
            {
            tryAndProcessBuffers(vmThread, vm, hwProfiler);
            }
#ifdef J9VM_JIT_RUNTIME_INSTRUMENTATION
         else
            {
            PORT_ACCESS_FROM_VMC(vmThread);
            if (tryAndProcessBuffers(vmThread, vm, hwProfiler))
               {
               j9ri_enable(vmThread->riParameters);
               if (TR::Options::isAnyVerboseOptionSet(TR_VerboseHWProfiler))
                  TR_VerboseLog::writeLineLocked(TR_Vlog_HWPROFILER, "RI is enabled for vmThread 0x%p", vmThread);
               }
            }
#endif
         }
      }
   }

static void jitMethodSampleInterrupt(J9VMThread* vmThread, IDATA handlerKey, void* userData)
   {
   J9StackWalkState walkState;

   walkState.flags = J9_STACKWALK_SKIP_INLINES | J9_STACKWALK_VISIBLE_ONLY | J9_STACKWALK_INCLUDE_NATIVES | J9_STACKWALK_COUNT_SPECIFIED;
   walkState.skipCount = 0;
   walkState.maxFrames = 1;
   walkState.walkThread = vmThread;
   vmThread->javaVM->walkStackFrames(vmThread, &walkState);
   if (walkState.framesWalked == 0)
      return;

   J9JITConfig * jitConfig = vmThread->javaVM->jitConfig;
   if (jitConfig == 0)
      return; // if a hook gets called after freeJitConfig then not much else we can do

   TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig);

   TR_J9VMBase * vm = TR_J9VMBase::get(jitConfig, vmThread);

   // Runtime Instrumentation
   processHWPBuffer(vmThread, vm);

   if ((jitConfig->runtimeFlags & J9JIT_DEFER_JIT) == 0)  // Reject any samples if we decided to postpone jitting
      {
      uint8_t * startPC = 0;
      int32_t codeSize = 0;
      TR_MethodMetaData * metaData = (TR_MethodMetaData *)walkState.jitInfo;
      if (metaData)
         {
         startPC = (uint8_t*)metaData->startPC;
         codeSize = compInfo->calculateCodeSize(metaData);
         }

#if defined(J9VM_INTERP_PROFILING_BYTECODES)
      if (interpreterProfilingState != IPROFILING_STATE_OFF &&
          !TR::Options::getCmdLineOptions()->getOption(TR_DisableInterpreterProfiling))
         {
         if (startPC)
            interpreterProfilingJITSamples ++;
         else
            interpreterProfilingINTSamples ++;
         }

      if (interpreterProfilingState == IPROFILING_STATE_OFF &&
          !startPC && !TR::Options::getCmdLineOptions()->getOption(TR_DisableInterpreterProfiling))
         {
         interpreterProfilingINTSamples++;
         }
#endif
      if (startPC)
         compInfo->_intervalStats._compiledMethodSamples++;
      else
         compInfo->_intervalStats._interpretedMethodSamples++;
      compInfo->getPersistentInfo()->incJitTotalSampleCount();

#if defined(J9VM_JIT_DYNAMIC_LOOP_TRANSFER)
      DLTLogic(vmThread, compInfo);
#endif

      if(TrcEnabled_Trc_JIT_MethodSampleStart1)
         {
         walkStackForSamplingReduced(vmThread);
         }
      else if(TrcEnabled_Trc_JIT_MethodSampleStart)
         {
         walkStackForSampling(vmThread);
         }

#if defined(J9VM_JIT_DYNAMIC_LOOP_TRANSFER)
      if (!TR::Options::getCmdLineOptions()->getOption(TR_MimicInterpreterFrameShape)
#if defined(J9VM_OPT_CRIU_SUPPORT)
          /* It's ok to not acquire the comp monitor here. Even if at this
           * point a checkpoint isn't in progress but later it does, a
           * compilation won't be started because of other places where the
           * flag is checked with the monitor in hand. This is more of an
           * optimization that statistically should be useful.
           */
          && !compInfo->shouldSuspendThreadsForCheckpoint()
#endif
          && !compInfo->getPersistentInfo()->getDisableFurtherCompilation())
         {
         static char *enableDebugDLT = feGetEnv("TR_DebugDLT");
         static J9Method *skipDLTMethod = NULL;
         bool skipSampleMethod = false;

         /* Code to support disableCompilationAfterDLT option:
          *   If env var TR_DebugDLT & option disableCompilationAfterDLT
          * is used, do not allow sampling interrupt to queue a standard
          * compilation for any method that has already been DLT compiled.
          *
          * A static J9Method pointer is used to avoid incurring the overhead
          * of finding option sets as much as possible.
          */
         if (enableDebugDLT != NULL && compInfo->searchForDLTRecord(walkState.method, -1) != NULL)
            {
            if (skipDLTMethod==walkState.method)
               {
               skipSampleMethod = true;
               }
            else if (TR::Options::getCmdLineOptions()->getOption(TR_DisableCompilationAfterDLT))
               {
               skipDLTMethod = walkState.method;
               skipSampleMethod = true;
               }
            else
               {
               TR::OptionSet *optionSet = findOptionSet(walkState.method, false);
               if (optionSet != NULL &&
                   optionSet->getOptions() != NULL &&
                   optionSet->getOptions()->getOption(TR_DisableCompilationAfterDLT))
                  {
                  skipDLTMethod = walkState.method;
                  skipSampleMethod = true;
                  }
               }
            }
         /* If TR_DebugDLT env var is not used, no DLT has been done
          * for this method, or the disableCompilationAfterDLT option was not
          * used for the method, then queue a compilation with sampleMethod()
          */
         if (!skipSampleMethod)
            TR::Recompilation::sampleMethod(vmThread, vm, startPC, codeSize, walkState.pc, walkState.method, jitConfig->samplingTickCount);
         }
#else // !J9VM_JIT_DYNAMIC_LOOP_TRANSFER
      if (!TR::Options::getCmdLineOptions()->getOption(TR_MimicInterpreterFrameShape)
#if defined(J9VM_OPT_CRIU_SUPPORT)
          /* It's ok to not acquire the comp monitor here. Even if at this
           * point a checkpoint isn't in progress but later it does, a
           * compilation won't be started because of other places where the
           * flag is checked with the monitor in hand. This is more of an
           * optimization that statistically should be useful.
           */
          && !compInfo->shouldSuspendThreadsForCheckpoint()
#endif
          && !compInfo->getPersistentInfo()->getDisableFurtherCompilation())
         {
         TR::Recompilation::sampleMethod(vmThread, vm, startPC, codeSize, walkState.pc, walkState.method, jitConfig->samplingTickCount);
         }
#endif
      }
   }

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
static void jitHookClassesUnloadEnd(J9HookInterface * * hookInterface, UDATA eventNum, void * eventData, void * userData)
   {
   }
#endif

typedef struct J9JitPrivateThreadData{
U_8 numberOfFrames;
U_8 numberOfBuffers;
U_8 frameCount;
int index;
U_8 * pcList[1];
}J9JitPrivateThreadData;

/**
 * Collect jitted stack frames during GC stack walks to help with debugging GC map PMRs.
 *
 * CAUTION walkThread is the thread doing the walking - usually a GC worker thread. It is not
 * the thread we want to use to find the appropriate buffer to update. For that we need to look
 * in walkState->walkThread. The walkThread parameter seems to be unnecessary.
 */
static UDATA collectJitPrivateThreadData(J9VMThread * walkThread, J9StackWalkState * walkState)
{
   J9JitPrivateThreadData * jitData = NULL;
   J9VMThread * threadBeingWalked = walkState->walkThread;
   if (threadBeingWalked)
      jitData = (J9JitPrivateThreadData *)threadBeingWalked->jitPrivateData;

   if(NULL != walkState->jitInfo && jitData && (jitData->frameCount < (jitData->numberOfFrames-1) ))
      {
      jitData->pcList[jitData->index] = walkState->pc;
      jitData->frameCount++;
      jitData->index = (jitData->index+1);
      }

   return J9_STACKWALK_KEEP_ITERATING;
}

void finalizeJitPrivateThreadData(J9VMThread * currentThread)
{
   J9VMThread *thread = currentThread;
   J9JitPrivateThreadData * jitData;
   do
      {
      if(thread->jitPrivateData)
         {
         jitData= (J9JitPrivateThreadData *)thread->jitPrivateData;
         while(jitData->index%jitData->numberOfFrames!=0 && jitData->index <(jitData->numberOfFrames)*(jitData->numberOfBuffers))
            {
            jitData->pcList[jitData->index]=0;
            jitData->index = (jitData->index+1)%((jitData->numberOfFrames)*(jitData->numberOfBuffers));
            }
         jitData->frameCount=0;
         }
      thread = thread->linkNext;
      }
   while (thread &&(thread != currentThread));
}

void initJitPrivateThreadData(J9VMThread * currentThread)
{
   if(!currentThread->javaVM->collectJitPrivateThreadData)
      {
      currentThread->javaVM->collectJitPrivateThreadData = &collectJitPrivateThreadData;
      }
   J9VMThread *thread = currentThread;
   J9JitPrivateThreadData * jitData;
   do
      {
      if(thread->jitPrivateData)
         {
         jitData= (J9JitPrivateThreadData *)thread->jitPrivateData;
         while(jitData->index%jitData->numberOfFrames!=0 && jitData->index <(jitData->numberOfFrames)*(jitData->numberOfBuffers))
            {
            jitData->pcList[jitData->index]=0;
            jitData->index = (jitData->index+1)%((jitData->numberOfFrames)*(jitData->numberOfBuffers));
            }

         if(jitData->frameCount!=0)
            {
            if(jitData->index!=0)
               jitData->pcList[jitData->index-1] =(U_8*) 1;
            else
               jitData->pcList[jitData->numberOfFrames*jitData->numberOfBuffers-1] = (U_8*)1;
            }

         jitData->frameCount=0;
         }
      thread = thread->linkNext;
      }
   while (thread &&(thread != currentThread));
}

static void jitHookGlobalGCStart(J9HookInterface * * hookInterface, UDATA eventNum, void * eventData, void * userData)
   {
   J9VMThread* vmThread = (J9VMThread*)((MM_GlobalGCStartEvent *)eventData)->currentThread->_language_vmthread;
   J9JITConfig * jitConfig = vmThread->javaVM->jitConfig;

   if(TR::Options::getCmdLineOptions()->getStackPCDumpNumberOfBuffers() && TR::Options::getCmdLineOptions()->getStackPCDumpNumberOfFrames())
      initJitPrivateThreadData(vmThread);

   if (jitConfig && jitConfig->runtimeFlags & J9JIT_GC_NOTIFY)
      printf("\n{GGC");
   jitReclaimMarkedAssumptions(false);
   }

static void jitHookLocalGCStart(J9HookInterface * * hookInterface, UDATA eventNum, void * eventData, void * userData)
   {
   MM_LocalGCStartEvent * localGCStartEventData = (MM_LocalGCStartEvent *)eventData;
   J9VMThread * vmThread = (J9VMThread *)localGCStartEventData->currentThread->_language_vmthread;
   J9JITConfig * jitConfig = vmThread->javaVM->jitConfig;

   if(TR::Options::getCmdLineOptions()->getStackPCDumpNumberOfBuffers() && TR::Options::getCmdLineOptions()->getStackPCDumpNumberOfFrames())
      initJitPrivateThreadData(vmThread);

   if (jitConfig == 0)
      return; // if a hook gets called after freeJitConfig then not much else we can do

   if (jitConfig->runtimeFlags & J9JIT_GC_NOTIFY)
      printf("\n{Scavenge");

   if (jitConfig->gcTraceThreshold && jitConfig->gcCount == jitConfig->gcTraceThreshold)
      {
      printf("\n<jit: enabling stack tracing at gc %" OMR_PRIuPTR ">", jitConfig->gcCount);
      TR::Options::getCmdLineOptions()->setVerboseOption(TR_VerboseGc);
      }
   jitReclaimMarkedAssumptions(false);
   }

static void jitHookGlobalGCEnd(J9HookInterface * * hookInterface, UDATA eventNum, void * eventData, void * userData)
   {
   J9VMThread* vmThread = (J9VMThread*)((MM_GlobalGCStartEvent *)eventData)->currentThread->_language_vmthread;
   J9JITConfig * jitConfig = vmThread->javaVM->jitConfig;

   if(TR::Options::getCmdLineOptions()->getStackPCDumpNumberOfBuffers() && TR::Options::getCmdLineOptions()->getStackPCDumpNumberOfFrames())
      finalizeJitPrivateThreadData(vmThread);

   if (jitConfig == 0)
      return; // if a hook gets called after freeJitConfig then not much else we can do

   TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig);

   getOutOfIdleStatesUnlocked(TR::CompilationInfo::SAMPLER_DEEPIDLE, compInfo, "GC");

   TR::CodeCacheManager::instance()->synchronizeTrampolines();
   if (jitConfig->runtimeFlags & J9JIT_GC_NOTIFY)
      printf("}");

   }

static void jitHookLocalGCEnd(J9HookInterface * * hookInterface, UDATA eventNum, void * eventData, void * userData)
   {
   J9VMThread * vmThread = (J9VMThread *) ((MM_LocalGCEndEvent *)eventData)->currentThread->_language_vmthread;
   J9JITConfig * jitConfig = vmThread->javaVM->jitConfig;

   if(TR::Options::getCmdLineOptions()->getStackPCDumpNumberOfBuffers() && TR::Options::getCmdLineOptions()->getStackPCDumpNumberOfFrames())
      finalizeJitPrivateThreadData(vmThread);

   if (jitConfig == 0)
      return; // if a hook gets called after freeJitConfig then not much else we can do

   if (jitConfig->runtimeFlags & J9JIT_GC_NOTIFY)
      printf("}");
   }

static void initThreadAfterCreation(J9VMThread *vmThread)
   {
   J9JITConfig * jitConfig = vmThread->javaVM->jitConfig;
   if (jitConfig == 0)
      return; // if a hook gets called after freeJitConfig then not much else we can do

   if (TR::Options::getCmdLineOptions()->getOption(TR_EnableValueTracing))
      {
      PORT_ACCESS_FROM_JAVAVM(jitConfig->javaVM);

      TR_JitPrivateConfig *pJitConfig = (TR_JitPrivateConfig *)jitConfig->privateConfig;

      if (pJitConfig)
         {
         int32_t size = pJitConfig->maxRuntimeTraceBufferSizeInBytes;
         UDATA buffer = (UDATA)j9mem_allocate_memory(size, J9MEM_CATEGORY_JIT);
         if (!buffer)
            return;

         VMTHREAD_TRACINGBUFFER_CURSOR(vmThread) = buffer;

         // Leave some headroom at the top of each buffer.
         //
         VMTHREAD_TRACINGBUFFER_TOP(vmThread) = buffer + size - pJitConfig->maxTraceBufferEntrySizeInBytes;

         // Create a tracing file handle and prepare the file for writing.
         //
         char fileName[64];
         IDATA tracefp= -1;

         sprintf(fileName, "%s_" POINTER_PRINTF_FORMAT, pJitConfig->itraceFileNamePrefix, vmThread);

         if ((tracefp = j9file_open(fileName, EsOpenWrite | EsOpenAppend | EsOpenCreate, 0644)) == -1)
            {
            j9tty_err_printf(PORTLIB, "Error: Failed to open jit trace file %s.\n", fileName);
            }

         VMTHREAD_TRACINGBUFFER_FH(vmThread) = tracefp;
         }
      }

#if defined(TR_HOST_S390)
   vmThread->codertTOC = (void *)jitConfig->pseudoTOC;
#endif

   if (TR::Options::getCmdLineOptions()->getOption(TR_CountWriteBarriersRT))
      {
      vmThread->debugEventData6 = 0;
      vmThread->debugEventData7 = 0;
      }

   TR::CompilationInfo *compInfo = TR::CompilationInfo::get(jitConfig);
#if defined(ENABLE_GPU)
   extern void *initGPUThread(J9VMThread *vmThread, TR::PersistentInfo *persistentInfo);
   initGPUThread(vmThread, compInfo->getPersistentInfo());
#endif
   getOutOfIdleStates(TR::CompilationInfo::SAMPLER_DEEPIDLE, compInfo, "thread creation");

   if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableSamplingJProfiling))
      {
      uint8_t jitState = compInfo->getPersistentInfo()->getJitState();
      if (jitState == IDLE_STATE || jitState == STARTUP_STATE)
         vmThread->debugEventData4 = 1;
      else
         vmThread->debugEventData4 = 1;
      }

   vmThread->jitCountDelta = 2; // by default we assume there are compilation threads active
   if (compInfo)
      {
      compInfo->acquireCompMonitor(vmThread);
      if (compInfo->getNumUsableCompilationThreads() > 0 && compInfo->getNumCompThreadsActive() == 0)
         {
         vmThread->jitCountDelta = 0;
         }
      compInfo->releaseCompMonitor(vmThread);

      vmThread->maxProfilingCount = (UDATA)encodeCount(compInfo->getIprofilerMaxCount());
      }

   U_8 numberOfBuffers,numberOfFrames;
   if(TR::Options::getCmdLineOptions()->getStackPCDumpNumberOfBuffers() <= 255)
      numberOfBuffers=TR::Options::getCmdLineOptions()->getStackPCDumpNumberOfBuffers();
   else
      numberOfBuffers=255;

   if(TR::Options::getCmdLineOptions()->getStackPCDumpNumberOfFrames() <= 254)
      numberOfFrames=TR::Options::getCmdLineOptions()->getStackPCDumpNumberOfFrames();
   else
      numberOfFrames=254;

   if(!vmThread->jitPrivateData && numberOfFrames && numberOfBuffers)
      {
      numberOfFrames++;
      vmThread->jitPrivateData = (J9JitPrivateThreadData *) TR_Memory::jitPersistentAlloc(sizeof(J9JitPrivateThreadData)+(numberOfFrames*numberOfBuffers -1)*sizeof(U_8*));
      J9JitPrivateThreadData * jitData= (J9JitPrivateThreadData *)vmThread->jitPrivateData;
      //199878 : jitPersistentAlloc may return NULL and this extra diagnostics should take an extra care to not segfault in such a scenario.
      if (!jitData)
         {
         return;
         }
      jitData->numberOfBuffers = numberOfBuffers;
      jitData->numberOfFrames = numberOfFrames;
      memset(jitData->pcList,0,(jitData->numberOfFrames)*(jitData->numberOfBuffers)*sizeof(U_8*));
      jitData->index=0;
      jitData->frameCount=0;
      }

   return;
   }

static void accumulateAndPrintDebugCounters(J9JITConfig *jitConfig)
   {
   TR_Debug *debug = TR::Options::getDebug();
   if (debug)
      {
      TR::DebugCounterGroup *counters;
      counters = TR::CompilationInfo::get(jitConfig)->getPersistentInfo()->getStaticCounters();
      if (counters)
         {
         counters->accumulate();
         debug->printDebugCounters(counters, "Static debug counters");
         }
      counters = TR::CompilationInfo::get(jitConfig)->getPersistentInfo()->getDynamicCounters();
      if (counters)
         {
         counters->accumulate();
         debug->printDebugCounters(counters, "Dynamic debug counters");
         }
      }
   }

static void jitHookThreadStart(J9HookInterface * * hookInterface, UDATA eventNum, void * eventData, void * userData)
   {
#if defined(TR_HOST_POWER)
   J9VMThread *vmThread = ((J9VMThreadStartedEvent *)eventData)->currentThread;

   J9JITConfig * jitConfig = vmThread->javaVM->jitConfig;
   TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig);
   if (compInfo->getPersistentInfo()->isRuntimeInstrumentationEnabled())
      {
      if (!IS_THREAD_RI_INITIALIZED(vmThread))
         {
         TR_HWProfiler *hwProfiler = compInfo->getHWProfiler();
         hwProfiler->initializeThread(vmThread);
         }
      }
#endif //defined(TR_HOST_POWER)
   }


static void jitHookThreadCreate(J9HookInterface * * hookInterface, UDATA eventNum, void * eventData, void * userData)
   {
   J9VMThread *vmThread = ((J9VMThreadCreatedEvent *)eventData)->vmThread;
   initThreadAfterCreation(vmThread);
   }

static void jitHookThreadCrash(J9HookInterface * * hookInterface, UDATA eventNum, void * eventData, void * userData)
   {
   J9VMThread * vmThread = ((J9VMThreadCrashEvent *)eventData)->currentThread;
   J9JITConfig * jitConfig = vmThread->javaVM->jitConfig;

   if (jitConfig == 0)
      return; // if a hook gets called after freeJitConfig then not much else we can do

   accumulateAndPrintDebugCounters(jitConfig);

   fflush(stdout);

   } // jitHookThreadCrash

static void jitHookThreadEnd(J9HookInterface * * hookInterface, UDATA eventNum, void * eventData, void * userData)
   {
   J9VMThread * vmThread = ((J9VMThreadEndEvent *)eventData)->currentThread;
   J9JITConfig * jitConfig = vmThread->javaVM->jitConfig;
   PORT_ACCESS_FROM_JAVAVM(vmThread->javaVM);

   if (jitConfig == 0)
      return; // if a hook gets called after freeJitConfig then not much else we can do

   if (TR::Options::getCmdLineOptions()->getOption(TR_CountWriteBarriersRT))
      {
      fprintf(stderr,"Thread %p: Executed %" OMR_PRIuPTR " barriers, %" OMR_PRIuPTR " went to slow path\n", vmThread, vmThread->debugEventData6, vmThread->debugEventData7);
      }
   return;
   }

static void jitHookThreadDestroy(J9HookInterface * * hookInterface, UDATA eventNum, void * eventData, void * userData)
   {
   J9VMThread * vmThread = ((J9VMThreadEndEvent *)eventData)->currentThread;
   J9JITConfig * jitConfig = vmThread->javaVM->jitConfig;
   PORT_ACCESS_FROM_JAVAVM(vmThread->javaVM);

   // Runtime Instrumentation
   TR::CompilationInfo *compInfo = TR::CompilationInfo::get(jitConfig);
#if defined(ENABLE_GPU)
   extern void *terminateGPUThread(J9VMThread *vmThread);
   terminateGPUThread(vmThread);
#endif
   TR_HWProfiler *hwProfiler = compInfo->getHWProfiler();
   if (compInfo->getPersistentInfo()->isRuntimeInstrumentationEnabled() && hwProfiler->isHWProfilingAvailable(vmThread))
      {
      if (IS_THREAD_RI_INITIALIZED(vmThread))
         hwProfiler->deinitializeThread(vmThread);
      }

   void  *vmWithThreadInfo = vmThread->jitVMwithThreadInfo;

   if (vmWithThreadInfo)
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)vmWithThreadInfo;
      fej9->freeSharedCache();
      vmThread->jitVMwithThreadInfo = 0;
      j9mem_free_memory(vmWithThreadInfo);
      }

   void  *ExceptionHandlerCache = vmThread->jitExceptionHandlerCache;

   if (ExceptionHandlerCache)
      {
      vmThread->jitExceptionHandlerCache = 0;
      j9mem_free_memory(ExceptionHandlerCache);
      }

   void *ArtifactSearchCache = vmThread->jitArtifactSearchCache;
   if (ArtifactSearchCache)
      {
      vmThread->jitArtifactSearchCache=0;
      j9mem_free_memory(ArtifactSearchCache);
      }

   J9JitPrivateThreadData * jitData = (J9JitPrivateThreadData *)vmThread->jitPrivateData;
   if(jitData && jitConfig)
      {
      vmThread->jitPrivateData=0;
      TR_Memory::jitPersistentFree(jitData);
      }


#ifdef J9VM_INTERP_AOT_COMPILE_SUPPORT
   vmWithThreadInfo = vmThread->aotVMwithThreadInfo;
   if (vmWithThreadInfo)
      {
      TR_J9VMBase *vm = (TR_J9VMBase *)vmWithThreadInfo;
      vm->freeSharedCache();
      vmThread->aotVMwithThreadInfo = 0;
      j9mem_free_memory(vmWithThreadInfo);
      }
#endif

   return;
   }

#if defined(J9VM_OPT_CRIU_SUPPORT)
static void jitHookPrepareCheckpoint(J9HookInterface * * hookInterface, UDATA eventNum, void * eventData, void * userData)
   {
   J9VMClassesUnloadEvent * restoreEvent = (J9VMClassesUnloadEvent *)eventData;
   J9VMThread * vmThread = restoreEvent->currentThread;
   J9JavaVM * javaVM = vmThread->javaVM;
   J9JITConfig * jitConfig = javaVM->jitConfig;

   TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig);
   compInfo->prepareForCheckpoint();
   }

static void jitHookPrepareRestore(J9HookInterface * * hookInterface, UDATA eventNum, void * eventData, void * userData)
   {
   J9VMClassesUnloadEvent * restoreEvent = (J9VMClassesUnloadEvent *)eventData;
   J9VMThread * vmThread = restoreEvent->currentThread;
   J9JavaVM * javaVM = vmThread->javaVM;
   J9JITConfig * jitConfig = javaVM->jitConfig;

   /* If the restored run does not allow further checkpoints, then
    * remove the portability restrictions on the target CPU (used
    * for JIT compiles) to allow optimal code generation
    */
   if (!javaVM->internalVMFunctions->isCheckpointAllowed(vmThread))
      {
      TR::Compiler->target.cpu = TR::CPU::detect(TR::Compiler->omrPortLib);
      jitConfig->targetProcessor = TR::Compiler->target.cpu.getProcessorDescription();

      /* Reinitialize the (technically unused) targetProcesssorInfo on x86 to prevent asserts */
      TR::Compiler->target.cpu.initializeTargetProcessorInfo(true);
      }

   TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig);
   compInfo->prepareForRestore();
   }
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
static void jitHookClassesUnload(J9HookInterface * * hookInterface, UDATA eventNum, void * eventData, void * userData)
   {
   J9VMClassesUnloadEvent * unloadedEvent = (J9VMClassesUnloadEvent *)eventData;
   J9VMThread * vmThread = unloadedEvent->currentThread;
   J9JITConfig * jitConfig = vmThread->javaVM->jitConfig;

   TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig);
   TR::PersistentInfo * persistentInfo = compInfo->getPersistentInfo();

   // Here we need to set CompilationShouldBeInterrupted. Currently if the TR_EnableNoVMAccess is not
   // set the compilation is stopped, but should be notify not to continue afterwards.
   //
   compInfo->setAllCompilationsShouldBeInterrupted();

   bool firstRange = true;
   bool coldRangeUninitialized = true;
   uintptr_t rangeStartPC = 0;
   uintptr_t rangeEndPC = 0;
   uintptr_t rangeColdStartPC = 0;
   uintptr_t rangeColdEndPC = 0;

   uintptr_t rangeStartMD = 0;
   uintptr_t rangeEndMD = 0;

   TR_RuntimeAssumptionTable * rat = compInfo->getPersistentInfo()->getRuntimeAssumptionTable();

   bool hasMethodOverrideAssumptions = false;
   bool hasClassExtendAssumptions = false;
   bool hasClassUnloadAssumptions = false;
   bool hasClassRedefinitionAssumptions = false;
   bool p = TR::Options::getVerboseOption(TR_VerboseHookDetailsClassUnloading);
   if (p)
      {
      TR_VerboseLog::writeLineLocked(TR_Vlog_HD, "Classes unloaded \n");
      }

   //All work here is only done if there is a chtable. Small have no table, thus nothing to do.

   TR_PersistentCHTable * table = 0;
   if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableCHOpts))
      table = persistentInfo->getPersistentCHTable();

   if (table && table->isActive())
      {
      TR_FrontEnd * fe = TR_J9VMBase::get(jitConfig, vmThread);

      // If space for the array of visited superclasses has not yet been allocated, do it now
      // There is no race condition because everything is frozen during class unloading
      if (!persistentInfo->getVisitedSuperClasses())
         persistentInfo->setVisitedSuperClasses((TR_OpaqueClassBlock **)TR_Memory::jitPersistentAlloc(MAX_SUPERCLASSES*sizeof(TR_OpaqueClassBlock *), TR_Memory::PersistentInfo));
      // The following is safe to execute even if there is no backing for the visitedSuperClasses array
      persistentInfo->clearVisitedSuperClasses();

      PORT_ACCESS_FROM_JAVAVM(vmThread->javaVM);
      J9ClassWalkState classWalkState;
      J9Class * j9clazz;
      TR_OpaqueClassBlock *clazz;
      j9clazz = vmThread->javaVM->internalVMFunctions->allClassesStartDo(&classWalkState, vmThread->javaVM, NULL);
      while (j9clazz)
         {
         // If the romableAotITable field is set to 0, that means this class was not caught
         // by the JIT load hook and has not been loaded.
         //
         if (J9CLASS_FLAGS(j9clazz) &  J9AccClassDying && j9clazz->romableAotITable !=0 )
            {
            clazz = ((TR_J9VMBase *)fe)->convertClassPtrToClassOffset(j9clazz);
            table->classGotUnloadedPost(fe,clazz); // side-effect: builds the array of visited superclasses
            }
         j9clazz = vmThread->javaVM->internalVMFunctions->allClassesNextDo(&classWalkState);
         }

      vmThread->javaVM->internalVMFunctions->allClassesEndDo(&classWalkState);


      TR_OpaqueClassBlock **visitedSuperClasses = persistentInfo->getVisitedSuperClasses();
      if (visitedSuperClasses && !persistentInfo->tooManySuperClasses())
         {
         int32_t numSuperClasses = persistentInfo->getNumVisitedSuperClasses();

         for (int32_t index = 0; index < numSuperClasses; index++)
            {
            clazz = visitedSuperClasses[index];
            TR_PersistentClassInfo *classInfo = table->findClassInfo(clazz);
            if (classInfo)
               classInfo->resetVisited();
            }
         }
      else
         {
         table->resetVisitedClasses();
         }
      }

   return;
   }

/// Side effect
/// 1. Every anon class to be unloaded will have j9clazz->classLoader = NULL
/// 2. Every j9clazz->jitMetaDataList will be set to NULL
static void jitHookAnonClassesUnload(J9HookInterface * * hookInterface, UDATA eventNum, void * eventData, void * userData)
   {
   J9VMAnonymousClassesUnloadEvent * unloadedEvent = (J9VMAnonymousClassesUnloadEvent *)eventData;
   J9VMThread * vmThread = unloadedEvent->currentThread;
   UDATA anonymousClassUnloadCount = unloadedEvent->anonymousClassUnloadCount;

   if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseClassUnloading))
      TR_VerboseLog::writeLineLocked(TR_Vlog_GC, "jitHookAnonClassesUnload: unloading %u anonymous classes\n", (uint32_t)anonymousClassUnloadCount);

   // Create a dummy classLoader and change j9class->classLoader to point to this fake one
   J9ClassLoader dummyClassLoader;
   int32_t numClasses = 0;
   bool needsMCCCleaning = false;
   for (J9Class* j9clazz = unloadedEvent->anonymousClassesToUnload; j9clazz; j9clazz = j9clazz->gcLink)
      {
      numClasses++;
      j9clazz->classLoader = &dummyClassLoader;

      if (j9clazz->classFlags & J9ClassContainsMethodsPresentInMCCHash)
         {
         needsMCCCleaning = true;
         }
      }
   // TODO assert that numClasses == anonymousClassUnloadCount

   // Concatenate all the lists of metadata from each class to be unloaded into
   // a bigger list (fullChainOfMetaData). Then attach this bigger list to the dummy classloader
   //
   J9JITExceptionTable* fullChainOfMetaData = NULL;
   uint32_t numMetaData = 0;
   for (J9Class* j9clazz = unloadedEvent->anonymousClassesToUnload; j9clazz; j9clazz = j9clazz->gcLink)
      {
      if (j9clazz->jitMetaDataList) // is there any metadata for this class?
         {
         // ASSUME that j9clazz->extendedClassFlags & J9ClassContainsJittedMethods  is non zero
         // Find the end the metadata list
         J9JITExceptionTable *lastMetaData = j9clazz->jitMetaDataList;
         for (; lastMetaData->nextMethod; lastMetaData = lastMetaData->nextMethod)
            numMetaData++;
         // Attach the chain assembled so far at the end of the current list
         lastMetaData->nextMethod = fullChainOfMetaData;
         if (fullChainOfMetaData)
            fullChainOfMetaData->prevMethod = lastMetaData;
         fullChainOfMetaData = j9clazz->jitMetaDataList;
         j9clazz->jitMetaDataList = NULL; // don't want metadata appearing in two lists
         }
      }
   // If I have any metadata to be cleaned up
   if (fullChainOfMetaData)
      {
      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseClassUnloading))
         TR_VerboseLog::writeLineLocked(TR_Vlog_GC, "jitHookAnonClassesUnload: will remove %u metadata entities\n", numMetaData);
      // Attach the full chain to the dummy classloader
      dummyClassLoader.jitMetaDataList = fullChainOfMetaData;
      // Perform the cleanup
      jitRemoveAllMetaDataForClassLoader(vmThread, &dummyClassLoader);
      }

   // Remove entries from the MCC hash tables related to trampolines
   if (needsMCCCleaning)
      {
      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseClassUnloading))
         TR_VerboseLog::writeLineLocked(TR_Vlog_GC, "jitHookAnonClassesUnload: will perform MCC cleaning\n");
      TR::CodeCacheManager::instance()->onClassUnloading(&dummyClassLoader);
      }

   J9JITConfig *jitConfig = vmThread->javaVM->jitConfig;
   TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig);
#if defined(J9VM_JIT_DYNAMIC_LOOP_TRANSFER)
   compInfo->cleanDLTRecordOnUnload();
   if (compInfo->getDLT_HT())
      compInfo->getDLT_HT()->onClassUnloading();
#endif

   compInfo->getLowPriorityCompQueue().purgeEntriesOnClassLoaderUnloading(&dummyClassLoader);

   compInfo->getPersistentInfo()->incGlobalClassUnloadID();
#if defined(J9VM_INTERP_PROFILING_BYTECODES)
   if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableIProfilerThread))
      {
      TR_J9VMBase * vmj9 = (TR_J9VMBase *)(TR_J9VMBase::get(jitConfig, vmThread));
      TR_IProfiler *iProfiler = vmj9->getIProfiler();
      if (iProfiler) // even if Iprofiler is disabled, there might be some buffers in the queue
         {           // which need to be invalidated
         iProfiler->invalidateProfilingBuffers();
         }
      }
#endif

   // Invalidate the buffers from the hardware profiler
   if (compInfo->getPersistentInfo()->isRuntimeInstrumentationEnabled())
      compInfo->getHWProfiler()->invalidateProfilingBuffers();

   // Don't want j9classes to point to the dummy class loader that will disappear
   for (J9Class* j9clazz = unloadedEvent->anonymousClassesToUnload; j9clazz; j9clazz = j9clazz->gcLink)
      {
      cgOnClassUnloading(j9clazz);
      j9clazz->classLoader = NULL;
      }
   }
#endif /* defined (J9VM_GC_DYNAMIC_CLASS_UNLOADING)*/


#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
static void jitHookClassUnload(J9HookInterface * * hookInterface, UDATA eventNum, void * eventData, void * userData)
   {
   J9VMClassUnloadEvent * unloadedEvent = (J9VMClassUnloadEvent *)eventData;
   J9VMThread * vmThread = unloadedEvent->currentThread;
   J9Class * j9clazz = unloadedEvent->clazz;
   J9JITConfig * jitConfig = vmThread->javaVM->jitConfig;

   TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig);

   TR_J9VMBase * fej9 = TR_J9VMBase::get(jitConfig, vmThread);
   TR_OpaqueClassBlock *clazz = fej9->convertClassPtrToClassOffset(j9clazz);

      {
      TR::ClassTableCriticalSection removeClasses(fej9);
      TR_ASSERT(!removeClasses.acquiredVMAccess(), "jitHookClassUnload should already have VM access");
      TR_LinkHead0<TR_ClassHolder> *classList = compInfo->getListOfClassesToCompile();

      TR_ClassHolder *prevClass = NULL;
      TR_ClassHolder *crtClass  = classList->getFirst();
      while (crtClass)
         {
         if (crtClass->getClass() == j9clazz) // found the class; remove it from the list
            {
            classList->removeAfter(prevClass, crtClass);
            //break; // we cannot have duplicates - we might have duplicates, see JTC-JAT 67575 comment 104
            }
         prevClass = crtClass;
         crtClass = crtClass->getNext();
         }
      }

   bool p = TR::Options::getVerboseOption(TR_VerboseHookDetailsClassUnloading);
   if (p)
      {
      TR_VerboseLog::writeLineLocked(TR_Vlog_HD, "Class unloading for class=0x%p\n", j9clazz);
      }

   PORT_ACCESS_FROM_JAVAVM(vmThread->javaVM);

   // remove from compilation request queue any methods that belong to this class
   fej9->acquireCompilationLock();
   fej9->invalidateCompilationRequestsForUnloadedMethods(clazz, false);
   fej9->releaseCompilationLock();

   J9Method * resolvedMethods = (J9Method *) fej9->getMethods((TR_OpaqueClassBlock*)j9clazz);
   uint32_t numMethods = fej9->getNumMethods((TR_OpaqueClassBlock*)j9clazz);
   uintptr_t methodsStartAddr = 0;
   uintptr_t methodsEndAddr = 0;

   if ( numMethods >0 )
      {
      methodsStartAddr = TR::Compiler->mtd.bytecodeStart( (TR_OpaqueMethodBlock *)&(resolvedMethods[0]));
      methodsEndAddr   = TR::Compiler->mtd.bytecodeStart( (TR_OpaqueMethodBlock *)&(resolvedMethods[numMethods-1]))
         + TR::Compiler->mtd.bytecodeSize( (TR_OpaqueMethodBlock *)&(resolvedMethods[numMethods-1]) );
      }

   static char *disableUnloadedClassRanges = feGetEnv("TR_disableUnloadedClassRanges");
   if (!disableUnloadedClassRanges)
      compInfo->getPersistentInfo()->addUnloadedClass(clazz, methodsStartAddr, (uint32_t)(methodsEndAddr-methodsStartAddr));

   TR_RuntimeAssumptionTable * rat = compInfo->getPersistentInfo()->getRuntimeAssumptionTable();
   rat->notifyClassUnloadEvent(fej9, 0, clazz, clazz);

   // Patch all the sites registered with dummy class pointer (-1)
   // These sites are patched each time any class is loaded
   //
   // These will be IPIC slots registered at compile time (PICs have not been populated yet)
   //
   rat->notifyClassUnloadEvent(fej9, 0, (TR_OpaqueClassBlock *)-1, clazz);

   // THIS ONLY NEEDS TO BE DONE FOR s390-31
   // BEGIN

      {
      TR::VMAccessCriticalSection notifyClassUnloadEvent(fej9);
      for (J9ITable * iTableEntry = (J9ITable *) TR::Compiler->cls.convertClassOffsetToClassPtr(clazz)->iTable; iTableEntry; iTableEntry = iTableEntry->next)
         {
         TR_OpaqueClassBlock *interfaceCl = fej9->convertClassPtrToClassOffset(iTableEntry->interfaceClass);
         rat->notifyClassUnloadEvent(fej9, 0, interfaceCl, clazz);
         }
      }

   // END

   TR_PersistentCHTable * table = 0;
   if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableCHOpts))
      table = compInfo->getPersistentInfo()->getPersistentCHTable();
   if (table && table->isActive())
      table->classGotUnloaded(fej9, clazz);

#if defined(J9VM_OPT_JITSERVER)
   // Add to JITServer unload list
   if (compInfo->getPersistentInfo()->getRemoteCompilationMode() == JITServer::CLIENT)
      {
      compInfo->getUnloadedClassesTempList()->push_back(clazz);
      // Loop through the set to find the class that needs to be purged.
      // Once found erase from the set.
      compInfo->getclassesCachedAtServer().erase(unloadedEvent->clazz);
      if (auto deserializer = compInfo->getJITServerAOTDeserializer())
         deserializer->invalidateClass(vmThread, j9clazz);
      }
#endif
   }
#endif /* defined (J9VM_GC_DYNAMIC_CLASS_UNLOADING)*/

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
static void jitHookClassLoaderUnload(J9HookInterface * * hookInterface, UDATA eventNum, void * eventData, void * userData)
   {
   J9VMClassLoaderUnloadEvent * unloadedEvent = (J9VMClassLoaderUnloadEvent *)eventData;
   J9VMThread * vmThread = unloadedEvent->currentThread;
   J9ClassLoader * classLoader = unloadedEvent->classLoader;

   // Assumptions are registered during class loading event. For a class loader that has never loaded a class, the JIT won't know about it
   if (classLoader->classSegments == NULL)
      return;

   J9JITConfig * jitConfig = vmThread->javaVM->jitConfig;

   TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig);

   bool p = TR::Options::getVerboseOption(TR_VerboseHookDetailsClassUnloading);
   if (p)
      {
      TR_VerboseLog::writeLineLocked(TR_Vlog_HD, "Class unloading for classLoader=0x%p\n", classLoader);
      }
   compInfo->getPersistentInfo()->incGlobalClassUnloadID();

   PORT_ACCESS_FROM_JAVAVM(vmThread->javaVM);

   if (classLoader->flags & J9CLASSLOADER_CONTAINS_JITTED_METHODS)
      jitRemoveAllMetaDataForClassLoader(vmThread, classLoader);

   if (classLoader->flags & J9CLASSLOADER_CONTAINS_METHODS_PRESENT_IN_MCC_HASH)
      TR::CodeCacheManager::instance()->onClassUnloading(classLoader);

   // CodeGen-specific actions on class unloading
   cgOnClassUnloading(classLoader);

   compInfo->getLowPriorityCompQueue().purgeEntriesOnClassLoaderUnloading(classLoader);

#if defined(J9VM_INTERP_PROFILING_BYTECODES)
   if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableIProfilerThread))
      {
      TR_J9VMBase * vmj9 = (TR_J9VMBase *)(TR_J9VMBase::get(jitConfig, vmThread));
      TR_IProfiler *iProfiler = vmj9->getIProfiler();
      if (iProfiler) // even if Iprofiler is disabled, there might be some buffers in the queue
         {           // which need to be invalidated
         iProfiler->invalidateProfilingBuffers();
         }
      }
#endif

   // Invalidate the buffers from the hardware profiler
   if (compInfo->getPersistentInfo()->isRuntimeInstrumentationEnabled())
      compInfo->getHWProfiler()->invalidateProfilingBuffers();

   compInfo->getPersistentInfo()->getPersistentClassLoaderTable()->removeClassLoader(vmThread, classLoader);

#if defined(J9VM_OPT_JITSERVER)
   if (auto deserializer = compInfo->getJITServerAOTDeserializer())
      deserializer->invalidateClassLoader(vmThread, classLoader);
#endif /* defined(J9VM_OPT_JITSERVER) */
   }

#endif /* defined (J9VM_GC_DYNAMIC_CLASS_UNLOADING)*/

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING) && defined(J9VM_JIT_DYNAMIC_LOOP_TRANSFER)
static void jitHookClassLoadersUnload(J9HookInterface * * hookInterface, UDATA eventNum, void * eventData, void * userData)
   {
   J9VMClassUnloadEvent * unloadedEvent = (J9VMClassUnloadEvent *)eventData;
   J9VMThread * vmThread = unloadedEvent->currentThread;
   J9JITConfig * jitConfig = vmThread->javaVM->jitConfig;
   TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig);
   compInfo->cleanDLTRecordOnUnload();
   if (compInfo->getDLT_HT())
      compInfo->getDLT_HT()->onClassUnloading();
   }
#endif /* defined (J9VM_GC_DYNAMIC_CLASS_UNLOADING) and defined (J9VM_JIT_DYNAMIC_LOOP_TRANSFER) */

void jitDiscardPendingCompilationsOfNatives(J9VMThread *vmThread, J9Class *clazz)
   {
   J9JITConfig * jitConfig = vmThread->javaVM->jitConfig;
   TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig);
   // need to get the compilation lock before updating the queue
   compInfo->acquireCompilationLock();
   compInfo->setAllCompilationsShouldBeInterrupted();
   compInfo->invalidateRequestsForNativeMethods(clazz, vmThread);
   compInfo->releaseCompilationLock();
   }

static bool classesAreRedefinedInPlace()
   {
   if(TR::Options::getCmdLineOptions()->getOption(TR_EnableHCR))
      return true;
   else return false;
   }

static bool methodsAreRedefinedInPlace()
   {
   // NOTE: making this return "true" will require careful thought.
   // Don't expect callers to respond properly.  At the time this comment was
   // written, we had never tested that configuration.  Consider calls to this
   // function just to be markers for places in the code that may require attention.
   //
   return false;
   }

// Hack markers
#define VM_PASSES_SAME_CLASS_TWICE 1

#if (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))
void jitClassesRedefined(J9VMThread * currentThread, UDATA classCount, J9JITRedefinedClass *classList, UDATA extensionsUsed)
   {
   reportHook(currentThread, "jitClassesRedefined");

   if ((classCount == 0 || classList == NULL) && TR::Options::getCmdLineOptions()->getOption(TR_EnableHCR))
      {
      reportHookFinished(currentThread, "jitClassesRedefined", "Nothing to do");
      return;
      }

   J9JITConfig * jitConfig = currentThread->javaVM->jitConfig;
   TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig);
   TR_J9VMBase * fe = TR_J9VMBase::get(jitConfig, currentThread);
   TR_PersistentCHTable * table = 0;
   if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableCHOpts))
      table = compInfo->getPersistentInfo()->getPersistentCHTable();

   TR_RuntimeAssumptionTable * rat = compInfo->getPersistentInfo()->getRuntimeAssumptionTable();

   TR_OpaqueClassBlock  *oldClass,          *newClass;
   J9Method             *oldMethod,         *newMethod;

   // A few definitions.  In the jit's terminology:
   //   The "stale method" is the one that points at the old bytecodes from before the hot swap.
   //   The "stale class" is the one that points at the stale methods.
   //   The "old class" is the j9class struct that existed before the hot swap.
   //   The "new class" is the one created in response to the hot swap.
   //
   // NOTE: THIS MAY NOT MATCH THE VM'S TERMINOLOGY!
   //
   // Here we define various aliases so we can freely use the terminology we want.
   //
   TR_OpaqueClassBlock  *&freshClass = classesAreRedefinedInPlace()? oldClass : newClass;
   TR_OpaqueClassBlock  *&staleClass = classesAreRedefinedInPlace()? newClass : oldClass;
   J9Method             *&freshMethod = methodsAreRedefinedInPlace()? oldMethod : newMethod;
   J9Method             *&staleMethod = methodsAreRedefinedInPlace()? newMethod : oldMethod;

   int methodCount;
   J9JITMethodEquivalence *methodList;
   bool equivalent;
   void* startPC;
   int i, j;

   // JIT compilation thread could be running without exclusive access so we need to explicitly stop it
   if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableNoVMAccess))
      {
      TR::MonitorTable::get()->getClassUnloadMonitor()->enter_write();
      }

   // need to get the compilation lock before updating the queue
   fe->acquireCompilationLock();
   compInfo->setAllCompilationsShouldBeInterrupted();
   J9JITRedefinedClass *classPair = classList;
   if (!TR::Options::getCmdLineOptions()->getOption(TR_FullSpeedDebug))
      {
      for (i = 0; i < classCount; i++)
         {
         freshClass = ((TR_J9VMBase *)fe)->convertClassPtrToClassOffset(classPair->newClass);
         if (VM_PASSES_SAME_CLASS_TWICE)
            staleClass = ((TR_J9VMBase *)fe)->convertClassPtrToClassOffset(((J9Class*)freshClass)->replacedClass);
         else
            staleClass = ((TR_J9VMBase *)fe)->convertClassPtrToClassOffset(classPair->oldClass);
         methodCount = classPair->methodCount;
         methodList  = classPair->methodList;

         int32_t length;
         char *name = fe->getClassNameChars((TR_OpaqueClassBlock*)freshClass, length);
         reportHookDetail(currentThread, "jitClassesRedefined", "Redefined class old=%p new=%p stale=%p fresh=%p %.*s", oldClass, newClass, staleClass, freshClass, length, name);

         compInfo->getLowPriorityCompQueue().purgeEntriesOnClassRedefinition((J9Class*)staleClass);

         // Step 1 remove from compilation request queue any methods that are redefined
         reportHookDetail(currentThread, "jitClassesRedefined", "  Invalidate compilation requests for classes old=%p and new=%p", oldClass, newClass);
         fe->invalidateCompilationRequestsForUnloadedMethods(oldClass, true);
         fe->invalidateCompilationRequestsForUnloadedMethods(newClass, true);

         for (j = 0; j < methodCount; j++)
            {
            staleMethod = methodList[j].oldMethod;
            freshMethod = methodList[j].newMethod;
            equivalent = (bool) methodList[j].equivalent;

            reportHookDetail(currentThread, "jitClassesRedefined", "    Notify MCC for method stale=%p fresh=%p e=%d", staleMethod, freshMethod, equivalent);
            TR::CodeCacheManager::instance()->onClassRedefinition(reinterpret_cast<TR_OpaqueMethodBlock *>(staleMethod),
                                                                  reinterpret_cast<TR_OpaqueMethodBlock *>(freshMethod));
            // Step 2 invalidate methods that are already compiled and trigger a new compilation.
            if (staleMethod && freshMethod && (startPC = compInfo->getPCIfCompiled(staleMethod)))
               {
               // Update the ram method information in PersistentMethodInfo
               TR_PersistentJittedBodyInfo *bodyInfo = TR::Recompilation::getJittedBodyInfoFromPC(startPC);
               if (bodyInfo)
                  {
                  reportHookDetail(currentThread, "jitClassesRedefined", "    Invalidate method body stale=%p startPC=%p", staleMethod, startPC);
                  TR::Recompilation::invalidateMethodBody(startPC, fe);
                  bodyInfo->setDisableSampling(true);
                  TR_PersistentMethodInfo *pmi = bodyInfo->getMethodInfo();
                  if (pmi)
                     {
                     pmi->setHasBeenReplaced();
                     }
                  // Note: we don't do pmi->setMethodInfo here because (a) they'll be patched by runtime
                  // assumptions anyway, and (b) we can only get our hands on the latest PMI, not all the
                  // older ones.
                  }
               // Same as TR_ResolvedMethod::isNative without allocating Scratch Memory
               else if(_J9ROMMETHOD_J9MODIFIER_IS_SET((J9_ROM_METHOD_FROM_RAM_METHOD(staleMethod)), J9AccNative ))
                  {
                  reportHookDetail(currentThread, "jitClassesRedefined",    "No need to invalidate native method stale=%p startPC=%p", staleMethod, startPC);
                  }
               else
                  {
                  reportHookDetail(currentThread, "jitClassesRedefined",    "WARNING!  Cannot invalidate method body stale=%p startPC=%p", staleMethod, startPC);
                  TR_ASSERT(0,"JIT HCR should make all methods recompilable, so startPC=%p should have a persistentBodyInfo", startPC);
                  }
               }
            }
         classPair = (J9JITRedefinedClass *) ((char *) classPair->methodList + (classPair->methodCount * sizeof(struct J9JITMethodEquivalence)));
         }
      }
   //for extended HCR under FSD, all the methods in code cache needs to be
   //discarded because of inlining
   else
      {
      // Don't know what got replaced, so get pessimistic and clear the whole compilation queue
      reportHookDetail(currentThread, "jitClassesRedefined", "  Invalidate all compilation requests");
      fe->invalidateCompilationRequestsForUnloadedMethods(NULL, true);

      //clean up the trampolines
      TR::CodeCacheManager::instance()->onFSDDecompile();
      }

   fe->releaseCompilationLock();

   classPair = classList;
   for (i = 0; i < classCount; i++)
      {
#if defined(J9VM_OPT_JITSERVER)
      // Add to JITServer unload list
      if (compInfo->getPersistentInfo()->getRemoteCompilationMode() == JITServer::CLIENT)
         {
         compInfo->getUnloadedClassesTempList()->push_back((TR_OpaqueClassBlock *) classPair->oldClass);
         if (auto deserializer = compInfo->getJITServerAOTDeserializer())
            deserializer->invalidateClass(currentThread, classPair->oldClass);
         }
#endif
      freshClass = ((TR_J9VMBase *)fe)->convertClassPtrToClassOffset(classPair->newClass);
      if (VM_PASSES_SAME_CLASS_TWICE)
         staleClass = ((TR_J9VMBase *)fe)->convertClassPtrToClassOffset(((J9Class*)freshClass)->replacedClass);
      else
         staleClass = ((TR_J9VMBase *)fe)->convertClassPtrToClassOffset(classPair->oldClass);
      methodCount = classPair->methodCount;
      methodList = classPair->methodList;

      // Do this before modifying the CHTable
      if (table && table->isActive() && TR::Options::sharedClassCache() && TR::Options::getCmdLineOptions()->getOption(TR_EnableClassChainValidationCaching))
         {
         table->resetCachedCCVResult(fe, oldClass);
         }

      // Step 3  patch modified classes
      if (rat)
         {
         reportHookDetail(currentThread, "jitClassesRedefined", "  Notify RAT on class old=%p fresh=%p", oldClass, freshClass);
         rat->notifyClassRedefinitionEvent(fe, 0, oldClass, freshClass);
         }
      for (j = 0; j < methodCount; j++)
         {
         staleMethod = methodList[j].oldMethod;
         freshMethod = methodList[j].newMethod;
         bool isSMP = 1; // conservative
         if (table)
            {
            reportHookDetail(currentThread, "jitClassesRedefined", "    Notify CHTable on method old=%p fresh=%p", oldMethod, freshMethod);
            table->methodGotOverridden(fe, compInfo->persistentMemory(), (TR_OpaqueMethodBlock*)freshMethod, (TR_OpaqueMethodBlock*)oldMethod, isSMP);
            }
         // Step 4 patch modified J9Method
         if (oldMethod && newMethod && rat)
            {
            reportHookDetail(currentThread, "jitClassesRedefined", "    Notify RAT on method old=%p fresh=%p", oldMethod, freshMethod);
            rat->notifyClassRedefinitionEvent(fe, 0,
               oldMethod,
               freshMethod);

            // Same as TR_ResolvedMethod::virtualMethodIsOverridden without allocating Scratch Memory
            if ((UDATA)oldMethod->constantPool & J9_STARTPC_METHOD_IS_OVERRIDDEN)
               {
               UDATA *cp = (UDATA*)&(newMethod->constantPool);
               *cp |= J9_STARTPC_METHOD_IS_OVERRIDDEN;
               }
            }
         }
      // Step 5 patch and update virtual guard. may also need to refresh CHTable
      if (table)
         {
         reportHookDetail(currentThread, "jitClassesRedefined", "  Notify CHTable on class old=%p fresh=%p", oldClass, freshClass);
         table->classGotRedefined(fe, oldClass, freshClass);
         }

      classPair = (J9JITRedefinedClass *) ((char *) classPair->methodList + (classPair->methodCount * sizeof(struct J9JITMethodEquivalence)));
      }

#if defined(J9VM_OPT_JITSERVER)
   if (compInfo->getPersistentInfo()->getRemoteCompilationMode() == JITServer::CLIENT && extensionsUsed)
      {
      compInfo->getUnloadedClassesTempList()->push_back(ClientSessionData::mustClearCachesFlag);
      }
#endif

   if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableNoVMAccess))
      {
      TR::MonitorTable::get()->getClassUnloadMonitor()->exit_write();
      }

   // In extended HCR, empty classList means methods are being added or removed from classes
   // Fall back to TR_MimicInterpreterFrameShape in this case because it's not properly
   // handled from VM side
   if (classCount == 0 || classList == NULL)
      TR::Options::getCmdLineOptions()->setOption(TR_MimicInterpreterFrameShape);

   reportHookFinished(currentThread, "jitClassesRedefined");
   }

void jitFlushCompilationQueue(J9VMThread * currentThread, J9JITFlushCompilationQueueReason reason)
   {
   char *buffer = "unknown reason";
   if (reason == J9FlushCompQueueDataBreakpoint)
      buffer = "DataBreakpoint";
   else
      TR_ASSERT(0, "unexpected use of jitFlushCompilationQueue");

   reportHook(currentThread, "jitFlushCompilationQueue ", buffer);

   J9JITConfig * jitConfig = currentThread->javaVM->jitConfig;
   TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig);
   TR_J9VMBase * fe = TR_J9VMBase::get(jitConfig, currentThread);

   // JIT compilation thread could be running without exclusive access so we need to explicitly stop it
   if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableNoVMAccess))
      {
      TR::MonitorTable::get()->getClassUnloadMonitor()->enter_write();
      }

   // need to get the compilation lock before updating the queue
   fe->acquireCompilationLock();
   compInfo->setAllCompilationsShouldBeInterrupted();
   reportHookDetail(currentThread, "jitFlushCompilationQueue", "  Invalidate all compilation requests");
   fe->invalidateCompilationRequestsForUnloadedMethods(NULL, true);
   //clean up the trampolines
   TR::CodeCacheManager::instance()->onFSDDecompile();
   fe->releaseCompilationLock();

   if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableNoVMAccess))
      {
      TR::MonitorTable::get()->getClassUnloadMonitor()->exit_write();
      }

   reportHookFinished(currentThread, "jitFlushCompilationQueue ", buffer);
   }

#endif // #if (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))

void jitMethodBreakpointed(J9VMThread *currentThread, J9Method *j9method)
   {
   J9JITConfig * jitConfig = currentThread->javaVM->jitConfig;
   TR_J9VMBase * fe = TR_J9VMBase::get(jitConfig, currentThread);
   TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig);
   TR_RuntimeAssumptionTable *rat = compInfo->getPersistentInfo()->getRuntimeAssumptionTable();

   reportHook(currentThread, "jitMethodbreakpointed", "j9method %p\n", j9method);

   if (rat)
      {
      rat->notifyMethodBreakpointed(fe, reinterpret_cast<TR_OpaqueMethodBlock *>(j9method));
      }

   reportHookFinished(currentThread, "jitMethodbreakpointed");
   }

/*
 * JIT hook called by the VM on the event of illegal modification. Runtime assumption table will be notified.
 */
void jitIllegalFinalFieldModification(J9VMThread *currentThread, J9Class *fieldClass)
   {
   J9JITConfig * jitConfig = currentThread->javaVM->jitConfig;
   TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig);
   TR_J9VMBase * fe = TR_J9VMBase::get(jitConfig, currentThread);

   // Set the bit so that VM doesn't report the modification next time
   fieldClass->classFlags |= J9ClassHasIllegalFinalFieldModifications;

#if defined(J9VM_OPT_JITSERVER)
   if (compInfo->getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER)
      {
      // Don't execute this hook at the jitserver side
      return;
      }
   else if (compInfo->getPersistentInfo()->getRemoteCompilationMode() == JITServer::CLIENT)
      {
      TR_OpaqueClassBlock *clazz = fe->convertClassPtrToClassOffset(fieldClass);
      compInfo->getSequencingMonitor()->enter();
      compInfo->getIllegalFinalFieldModificationList()->push_back(clazz);
      compInfo->getSequencingMonitor()->exit();
      }
#endif

   int32_t length;
   char *className = fe->getClassNameChars((TR_OpaqueClassBlock*)fieldClass, length);
   reportHook(currentThread, "jitIllegalFinalFieldModification", "class %p %.*s", fieldClass, length, className);

   TR_RuntimeAssumptionTable * rat = compInfo->getPersistentInfo()->getRuntimeAssumptionTable();
   if (rat)
      {
      rat->notifyIllegalStaticFinalFieldModificationEvent(fe, fieldClass);
      }
   reportHookFinished(currentThread, "jitIllegalFinalFieldModification");
   }

#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)
// JIT hook called by the VM to update the target of a MutableCallSite.
void jitSetMutableCallSiteTarget(J9VMThread *vmThread, j9object_t mcs, j9object_t newTarget)
   {
   J9JITConfig *jitConfig = vmThread->javaVM->jitConfig;
   TR::CompilationInfo *compInfo = TR::CompilationInfo::get(jitConfig);
   TR_J9VMBase *fej9 = TR_J9VMBase::get(jitConfig, vmThread);
   TR_RuntimeAssumptionTable *rat = compInfo->getPersistentInfo()->getRuntimeAssumptionTable();

   bool verbose = TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseHooks);
   bool details = TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseHookDetails);
   verbose = verbose || details;

   TR_OpaqueClassBlock *mcsClass = fej9->getObjectClass((uintptr_t)mcs);
   uintptr_t targetOffset = fej9->getInstanceFieldOffset(
      mcsClass, "target", "Ljava/lang/invoke/MethodHandle;");

   TR::ClassTableCriticalSection commit(fej9);

   // There are no concurrent modifications because target is only modified
   // while holding the CH table lock.
   uintptr_t prevTarget = fej9->getReferenceFieldAt((uintptr_t)mcs, targetOffset);
   if ((uintptr_t)newTarget == prevTarget)
      return; // Nothing to do.

   // The CH table lock must be acquired before reading the cookie. Otherwise,
   // a nonzero value written by another thread (in the compiler's CH table
   // commit phase) won't necessarily be observed; and worse, if this MCS
   // object were first inlined in a compilation that is committing
   // concurrently, the following would be possible even with strong memory
   // ordering:
   //
   // 1. jitSetMutableCallSiteTarget() reads the cookie, which is zero, so no
   //    assumptions will be invalidated; then
   //
   // 2. the compilation reads the MCS target during CH table commit, which
   //    matches what it saw during inlining, so it considers its assumption
   //    valid; then
   //
   // 3. in any order, the compilation finishes and the MCS target is updated,
   //    resulting in a compiled body that has inlined the wrong target.

   uintptr_t cookie = fej9->mutableCallSiteCookie((uintptr_t)mcs);
   if (cookie == 0)
      {
      if (verbose)
         {
         TR_VerboseLog::writeLineLocked(
            TR_Vlog_HD, "%p skipping nonexistent cookie", vmThread);
         }
      }
   else
      {
      // There may be assumptions to invalidate. Any such assumptions must be
      // invalidated before updating the target field. Otherwise, another
      // thread concurrently running JIT-compiled code where the previous
      // target is inlined could read the new target object from the field and
      // still pass the MCS target guard into the inlined body, even though the
      // freshly loaded target differs from the known object used during
      // optimization.
      //
      // While it would actually be OK to continue using the old target until
      // syncAll() is called, code optimized based on known object information
      // does not keep using the original reference it had when it was
      // compiled. Instead, it retrieves the reference at runtime using the
      // original chain of loads. The compiler will have transformed some but
      // not necessarily all of the uses of this reference, so if a different
      // reference is found at runtime, inconsistencies will result.
      //
      // For example, suppose the compiled code gets the mutable call site MCS
      // and then updates ((Foo)((Species_L)MCS.target).argL0).f. Making use of
      // the known object information, the compiler has likely improved the
      // code so that it stores to MCS.target.argL0.f without any casts, but
      // (crucially) the object reference MCS.target.argL0 is not truly
      // constant-folded, so it must be found by chasing pointers at runtime.
      // If MCS.target is changed to e.g. an instance of Species_I, and if the
      // optimized code is still allowed to run, then the load of argL0 will
      // reinterpret the bound integer as a reference, and the subsequent store
      // will dereference it. Kaboom!

      if (verbose)
         {
         TR_VerboseLog::writeLineLocked(
            TR_Vlog_HD, "%p notifying cookie %p", vmThread, (void*)cookie);
         }

      rat->notifyMutableCallSiteChangeEvent(fej9, cookie);

      if (verbose)
         {
         TR_VerboseLog::writeLineLocked(
            TR_Vlog_HD, "%p finished notifying cookie %p", vmThread, (void*)cookie);
         }
      }

   // Finally, update the target. This must be done while the CH table lock is
   // still held. Otherwise, it would be possible for a concurrent compilation
   // to inline the wrong target without invalidating its guard:
   //
   // 1. jitSetMutableCallSiteTarget() invalidates MCS target guards for this
   //    call site and releases the CH table lock; then
   //
   // 2. the compilation gets the CH table lock and starts its CH table commit,
   //    wherein it observes the previous target, which matches what it saw
   //    during inlining, so it considers its assumption valid; then
   //
   // 3. finally, the target is updated.
   //
   // Note that it's fine to use a regular (non-volatile) store here. For other
   // threads loading the target at runtime / in the interpreter, they are not
   // required to see the update before syncAll() is called. For compilations
   // in other threads, any subsequent CH table commit is guaranteed to see the
   // update due to synchronization via the CH table lock.

   targetOffset += TR::Compiler->om.objectHeaderSizeInBytes();
   vmThread->javaVM->memoryManagerFunctions->j9gc_objaccess_mixedObjectStoreObject(
      vmThread, mcs, targetOffset, newTarget, 0);
   }
#endif

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
static void jitHookInterruptCompilation(J9HookInterface * * hookInterface, UDATA eventNum, void * eventData, void * userData)
   {
   MM_InterruptCompilationEvent * interruptCompilationEvent = (MM_InterruptCompilationEvent *)eventData;
   J9VMThread * vmThread = interruptCompilationEvent->currentThread;
   J9JITConfig * jitConfig = vmThread->javaVM->jitConfig;
   TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig);

   //compInfo->setAllCompilationsShouldBeInterrupted();
   compInfo->getPersistentInfo()->setGCwillBlockOnClassUnloadMonitor();
   }
#endif /* defined (J9VM_GC_DYNAMIC_CLASS_UNLOADING)*/

// jitUpdateMethodOverride is called indirectly from updateCHTable
//
void jitUpdateMethodOverride(J9VMThread * vmThread, J9Class * cl, J9Method * overriddenMethod, J9Method * overriddingMethod)
   {
   J9JITConfig * jitConfig = vmThread->javaVM->jitConfig;

   TR_FrontEnd * vm = TR_J9VMBase::get(jitConfig, vmThread);

   PORT_ACCESS_FROM_PORT(jitConfig->javaVM->portLibrary);

   TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig);

   // Pretend that we are always running smp; current implementation
   // of virtual guard patching (on ia32/ppc) ignore it.  Since patching
   // is infrequent anyway probably it is not worth maintaining two versions
   // of code for patching.
   // On Linux, get_number_CPUs calls sysconfig which has to generate and
   // parse /proc/stat (slow) -- if re-enabling, cache the isSMP flag
   // rather than querying each time.
   //
   bool isSMP = 1; // conservative
   if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableCHOpts))
      {
      jitAcquireClassTableMutex(vmThread);
      compInfo->getPersistentInfo()->getPersistentCHTable()->methodGotOverridden(
                                                                                 vm, compInfo->persistentMemory(), (TR_OpaqueMethodBlock *) overriddingMethod, (TR_OpaqueMethodBlock *) overriddenMethod, isSMP);
      jitReleaseClassTableMutex(vmThread);
      }
   }

/* updateOverriddenFlag() replaces jitUpdateInlineAttribute.  See Design 1812 */

static void updateOverriddenFlag( J9VMThread *vm , J9Class *cl)
   {

   static const char *traceIt = 0; //  feGetEnv("TR_TraceUpdateOverridenFlag"); //this trace should only be enabled if it is needed

   J9ROMClass *ROMCl = cl->romClass;

   if(ROMCl->modifiers &  J9AccInterface )   //Do nothing if interface
      return;

   int32_t classDepth = J9CLASS_DEPTH(cl) - 1;

   if (classDepth >= 0)                // Do nothing if we don't have a superclass
      {
      J9Class * superCl = cl->superclasses[classDepth];

      J9VTableHeader * superVTableHeader = J9VTABLE_HEADER_FROM_RAM_CLASS(superCl);
      intptr_t methodCount =  (intptr_t)superVTableHeader->size;
      J9Method ** superVTable = J9VTABLE_FROM_HEADER(superVTableHeader);
      J9Method ** subVTable = J9VTABLE_FROM_RAM_CLASS(cl);

      intptr_t methodIndex=0;

      while(methodCount--)
         {
         J9Method * superMethod = *superVTable++;
         J9Method * subMethod = *subVTable++;

         J9ROMMethod *subROM   = J9_ROM_METHOD_FROM_RAM_METHOD(subMethod);
         J9ROMMethod *superROM = J9_ROM_METHOD_FROM_RAM_METHOD(superMethod);

         bool methodModifiersAreSafe = (subROM->modifiers & J9AccForwarderMethod) && !((subROM->modifiers & J9AccSynchronized) || (subROM->modifiers & J9AccStrict) || (subROM->modifiers & J9AccNative)) ;

         if (traceIt)
            {
         char *classNameChars = (char *)J9UTF8_DATA(J9ROMCLASS_CLASSNAME(ROMCl));
         int32_t classNameLen = (int32_t)J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(ROMCl));
         char *superSignature = (char*)J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(J9_ROM_METHOD_FROM_RAM_METHOD(superMethod)));
         int32_t superSigLen = (int32_t)J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(J9_ROM_METHOD_FROM_RAM_METHOD(superMethod)));
         char *superName = (char*)J9UTF8_DATA(J9ROMMETHOD_NAME(J9_ROM_METHOD_FROM_RAM_METHOD(superMethod)));
         int32_t superNameLen = (int32_t) J9UTF8_LENGTH(J9ROMMETHOD_NAME(J9_ROM_METHOD_FROM_RAM_METHOD(superMethod)));
         char *subSignature = (char*)J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(J9_ROM_METHOD_FROM_RAM_METHOD(subMethod)));
         int32_t subSigLen = (int32_t)J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(J9_ROM_METHOD_FROM_RAM_METHOD(subMethod)));
         char *subName = (char*)J9UTF8_DATA(J9ROMMETHOD_NAME(J9_ROM_METHOD_FROM_RAM_METHOD(subMethod)));
         int32_t subNameLen = (int32_t)J9UTF8_LENGTH(J9ROMMETHOD_NAME(J9_ROM_METHOD_FROM_RAM_METHOD(subMethod)));

         printf("class = %.*s superSignature = %.*s, superName = %.*s, supermodifers = %x , subSignature = %.*s, subName = %.*s, submodifiers = %x subMethod = %p methodModifiersAreSafe = %d\n"
               ,classNameLen,classNameChars,superSigLen,superSignature,superNameLen,superName,(int)superROM->modifiers,subSigLen,subSignature,subNameLen,subName,(int)subROM->modifiers,subMethod,methodModifiersAreSafe );
            printf("For subROM %p:\n",subROM);

            if(subROM->modifiers & J9AccAbstract)                       printf("\tsubMethod is J9AccAbstract (%x)\n",J9AccAbstract);
            if(subROM->modifiers & J9AccAnnotation)                     printf("\tsubMethod is J9AccAnnotation\n");
            if(subROM->modifiers & J9AccBridge)                         printf("\tsubMethod is J9AccBridge\n");
            if(subROM->modifiers & J9AccEmptyMethod)                    printf("\tsubMethod is J9AccEmptyMethod\n");
            if(subROM->modifiers & J9AccEnum)                           printf("\tsubMethod is J9AccEnum\n");
            if(subROM->modifiers & J9AccFinal)                          printf("\tsubMethod is J9AccFinal\n");
            if(subROM->modifiers & J9AccForwarderMethod)                printf("\tsubMethod is J9AccForwarderMethod\n");
            if(subROM->modifiers & J9AccGetterMethod)                   printf("\tsubMethod is J9AccGetterMethod\n");
            if(subROM->modifiers & J9AccInterface)                      printf("\tsubMethod is J9AccInterface\n");
            if(subROM->modifiers & J9AccMethodCallerSensitive)          printf("\tsubMethod is J9AccMethodCallerSensitive\n");
            if(subROM->modifiers & J9AccMethodFrameIteratorSkip)        printf("\tsubMethod is J9AccMethodFrameIteratorSkip\n");
            if(subROM->modifiers & J9AccMethodHasBackwardBranches)      printf("\tsubMethod is J9AccMethodHasBackwardBranches\n");
            if(subROM->modifiers & J9AccMethodHasDebugInfo)             printf("\tsubMethod is J9AccMethodHasDebugInfo\n");
            if(subROM->modifiers & J9AccMethodHasDefaultAnnotation)     printf("\tsubMethod is J9AccMethodHasDefaultAnnotation\n");
            if(subROM->modifiers & J9AccMethodHasExceptionInfo)         printf("\tsubMethod is J9AccMethodHasExceptionInfo\n");
            if(subROM->modifiers & J9AccMethodHasGenericSignature)      printf("\tsubMethod is J9AccMethodHasGenericSignature\n");
            if(subROM->modifiers & J9AccMethodHasMethodAnnotations)     printf("\tsubMethod is J9AccMethodHasMethodAnnotations\n");
            if(subROM->modifiers & J9AccMethodHasParameterAnnotations)  printf("\tsubMethod is J9AccMethodHasParameterAnnotations\n");
            if(subROM->modifiers & J9AccMethodHasStackMap)              printf("\tsubMethod is J9AccMethodHasStackMap\n");
            if(subROM->modifiers & J9AccMethodObjectConstructor)        printf("\tsubMethod is J9AccMethodObjectConstructor\n");
            if(subROM->modifiers & J9AccMethodReturn0)                  printf("\tsubMethod is J9AccMethodReturn0\n");
            if(subROM->modifiers & J9AccMethodReturn1)                  printf("\tsubMethod is J9AccMethodReturn1\n");
            if(subROM->modifiers & J9AccMethodReturn2)                  printf("\tsubMethod is J9AccMethodReturn2\n");
            if(subROM->modifiers & J9AccMethodReturnA)                  printf("\tsubMethod is J9AccMethodReturnA\n");
            if(subROM->modifiers & J9AccMethodReturnD)                  printf("\tsubMethod is J9AccMethodReturnD\n");
            if(subROM->modifiers & J9AccMethodReturnF)                  printf("\tsubMethod is J9AccMethodReturnF\n");
            if(subROM->modifiers & J9AccMethodReturnMask)               printf("\tsubMethod is J9AccMethodReturnMask\n");
            if(subROM->modifiers & J9AccMethodReturnShift)              printf("\tsubMethod is J9AccMethodReturnShift\n");
            if(subROM->modifiers & J9AccMethodVTable)                   printf("\tsubMethod is J9AccMethodVTable\n");
            if(subROM->modifiers & J9AccNative)                         printf("\tsubMethod is J9AccNative\n");
            if(subROM->modifiers & J9AccPrivate)                        printf("\tsubMethod is J9AccPrivate\n");
            if(subROM->modifiers & J9AccProtected)                      printf("\tsubMethod is J9AccProtected\n");
            if(subROM->modifiers & J9AccPublic)                         printf("\tsubMethod is J9AccPublic\n");
            if(subROM->modifiers & J9AccStatic)                         printf("\tsubMethod is J9AccStatic\n");
            if(subROM->modifiers & J9AccStrict)                         printf("\tsubMethod is J9AccStrict\n");
            if(subROM->modifiers & J9AccSuper)                          printf("\tsubMethod is J9AccSuper\n");
            if(subROM->modifiers & J9AccSynchronized)                   printf("\tsubMethod is J9AccSynchronized\n");
            if(subROM->modifiers & J9AccSynthetic)                      printf("\tsubMethod is J9AccSynthetic\n");
            if(subROM->modifiers & J9AccTransient)                      printf("\tsubMethod is J9AccTransient\n");
            if(subROM->modifiers & J9AccVarArgs)                        printf("\tsubMethod is J9AccVarArgs\n");
            if(subROM->modifiers & J9AccVolatile)                       printf("\tsubMethod is J9AccVolatile\n");

            printf("For superROM %p:\n",superROM);

            if(superROM->modifiers & J9AccAbstract)                     printf("\tsuperMethod is J9AccAbstract\n");
            if(superROM->modifiers & J9AccAnnotation)                   printf("\tsuperMethod is J9AccAnnotationt\n");
            if(superROM->modifiers & J9AccBridge)                       printf("\tsuperMethod is J9AccBridge\n");
            if(superROM->modifiers & J9AccEmptyMethod)                  printf("\tsuperMethod is J9AccEmptyMethod\n");
            if(superROM->modifiers & J9AccEnum)                         printf("\tsuperMethod is J9AccEnum\n");
            if(superROM->modifiers & J9AccFinal)                        printf("\tsuperMethod is J9AccFinal\n");
            if(superROM->modifiers & J9AccForwarderMethod)              printf("\tsuperMethod is J9AccForwarderMethod\n");
            if(superROM->modifiers & J9AccGetterMethod)                 printf("\tsuperMethod is J9AccGetterMethod\n");
            if(superROM->modifiers & J9AccInterface)                    printf("\tsuperMethod is J9AccInterface\n");
            if(superROM->modifiers & J9AccMethodCallerSensitive)        printf("\tsuperMethod is J9AccMethodCallerSensitive\n");
            if(superROM->modifiers & J9AccMethodFrameIteratorSkip)      printf("\tsuperMethod is J9AccMethodFrameIteratorSkip\n");
            if(superROM->modifiers & J9AccMethodHasBackwardBranches)    printf("\tsuperMethod is J9AccMethodHasBackwardBranches\n");
            if(superROM->modifiers & J9AccMethodHasDebugInfo)           printf("\tsuperMethod is J9AccMethodHasDebugInfo\n");
            if(superROM->modifiers & J9AccMethodHasDefaultAnnotation)   printf("\tsuperMethod is J9AccMethodHasDefaultAnnotation\n");
            if(superROM->modifiers & J9AccMethodHasExceptionInfo)       printf("\tsuperMethod is J9AccMethodHasExceptionInfo\n");
            if(superROM->modifiers & J9AccMethodHasGenericSignature)    printf("\tsuperMethod is J9AccMethodHasGenericSignature\n");
            if(superROM->modifiers & J9AccMethodHasMethodAnnotations)   printf("\tsuperMethod is J9AccMethodHasMethodAnnotations\n");
            if(superROM->modifiers &J9AccMethodHasParameterAnnotations) printf("\tsuperMethod is J9AccMethodHasParameterAnnotations\n");
            if(superROM->modifiers & J9AccMethodHasStackMap)            printf("\tsuperMethod is J9AccMethodHasStackMap\n");
            if(superROM->modifiers & J9AccMethodObjectConstructor)      printf("\tsuperMethod is J9AccMethodObjectConstructor\n");
            if(superROM->modifiers & J9AccMethodReturn0)                printf("\tsuperMethod is J9AccMethodReturn0\n");
            if(superROM->modifiers & J9AccMethodReturn1)                printf("\tsuperMethod is J9AccMethodReturn1\n");
            if(superROM->modifiers & J9AccMethodReturn2)                printf("\tsuperMethod is J9AccMethodReturn2\n");
            if(superROM->modifiers & J9AccMethodReturnA)                printf("\tsuperMethod is J9AccMethodReturnA\n");
            if(superROM->modifiers & J9AccMethodReturnD)                printf("\tsuperMethod is J9AccMethodReturnD\n");
            if(superROM->modifiers & J9AccMethodReturnF)                printf("\tsuperMethod is J9AccMethodReturnF\n");
            if(superROM->modifiers & J9AccMethodReturnMask)             printf("\tsuperMethod is J9AccMethodReturnMask\n");
            if(superROM->modifiers & J9AccMethodReturnShift)            printf("\tsuperMethod is J9AccMethodReturnShift\n");
            if(superROM->modifiers & J9AccMethodVTable)                 printf("\tsuperMethod is J9AccMethodVTable\n");
            if(superROM->modifiers & J9AccNative)                       printf("\tsuperMethod is J9AccNative\n");
            if(superROM->modifiers & J9AccPrivate)                      printf("\tsuperMethod is J9AccPrivate\n");
            if(superROM->modifiers & J9AccProtected)                    printf("\tsuperMethod is J9AccProtected\n");
            if(superROM->modifiers & J9AccPublic)                       printf("\tsuperMethod is J9AccPublic\n");
            if(superROM->modifiers & J9AccStatic)                       printf("\tsuperMethod is J9AccStatic\n");
            if(superROM->modifiers & J9AccStrict)                       printf("\tsuperMethod is J9AccStrict\n");
            if(superROM->modifiers & J9AccSuper)                        printf("\tsuperMethod is J9AccSuper\n");
            if(superROM->modifiers & J9AccSynchronized)                 printf("\tsuperMethod is J9AccSynchronized\n");
            if(superROM->modifiers & J9AccSynthetic)                    printf("\tsuperMethod is J9AccSynthetic\n");
            if(superROM->modifiers & J9AccTransient)                    printf("\tsuperMethod is J9AccTransient\n");
            if(superROM->modifiers & J9AccVarArgs)                      printf("\tsuperMethod is J9AccVarArgs\n");
            if(superROM->modifiers & J9AccVolatile)                     printf("\tsuperMethod is J9AccVolatile\n");

            fflush(stdout);
            }

         if((subMethod != superMethod) && methodModifiersAreSafe  && (J9_BYTECODE_END_FROM_ROM_METHOD(subROM) - J9_BYTECODE_START_FROM_ROM_METHOD(subROM))>0 )        //If the j9methods don't match, method has been overridden
            {
            if (traceIt)
               {
               printf("For submethod %p, trying to match signature for overridden bit\n",subMethod);
               fflush(stdout);
               }


            const uint8_t * _code = subMethod->bytecodes;         //a pointer to bytecodes
            bool matches=false;
            bool finishedArgs = false;
            int32_t curLocalSize;
            int32_t i = 0;
            uint16_t cpIndex = 0;
            for (int32_t nextLocalIndex = 0; !matches && !finishedArgs && nextLocalIndex<255; nextLocalIndex += curLocalSize)
               {
               curLocalSize = 1;
               switch (TR_J9ByteCodeIterator::convertOpCodeToByteCodeEnum(_code[i++]))
                  {
                  case J9BCaload0:
                     finishedArgs = (nextLocalIndex != 0);
                     break;
                  case J9BClload1: case J9BCdload1:
                     curLocalSize = 2;
                     // fall through
                  case J9BCiload1: case J9BCfload1: case J9BCaload1:
                     finishedArgs = (nextLocalIndex != 1);
                     break;
                  case J9BClload2: case J9BCdload2:
                     curLocalSize = 2;
                     // fall through
                  case J9BCiload2: case J9BCfload2: case J9BCaload2:
                     finishedArgs = (nextLocalIndex != 2);
                     break;
                  case J9BClload3: case J9BCdload3:
                     curLocalSize = 2;
                     // fall through
                  case J9BCiload3: case J9BCfload3: case J9BCaload3:
                     finishedArgs = (nextLocalIndex != 3);
                     break;
                  case J9BClload: case J9BCdload:
                     curLocalSize = 2;
                     // fall through
                  case J9BCiload: case J9BCfload: case J9BCaload:
                     finishedArgs = (nextLocalIndex != _code[i++]);
                     break;
                  case J9BCinvokespecial:
                      cpIndex = BC_OP_U16(&_code[i]); //will need to compare signature with superMethod.
                      matches=true;
                      break;
                  case J9BCinvokespecialsplit:
                     cpIndex = *(U_16*)(J9ROMCLASS_SPECIALSPLITMETHODREFINDEXES(ROMCl) + BC_OP_U16(&_code[i])); //will need to compare signature with superMethod.
                     matches=true;
                     break;
                  default:
                     finishedArgs = true;
                     break;
                  }
               }

            J9UTF8 * callSignature;
            J9UTF8 * callName;
            if(matches)   //so far we are matching our pattern
               {
               if (traceIt)
                  {
                  printf("For submethod %p, pattern matches after bc walk\n",subMethod);
                  fflush(stdout);
                  }


               J9ROMFieldRef * ref1 = &(((J9ROMFieldRef *)((char *)ROMCl+sizeof(J9ROMClass)))[cpIndex]);
               J9ROMNameAndSignature *nameAndSignature = J9ROMFIELDREF_NAMEANDSIGNATURE(ref1);      //this isn't a string its a struct.
               callSignature = J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSignature);
               callName = J9ROMNAMEANDSIGNATURE_NAME(nameAndSignature);

               J9UTF8 *superSignature = J9ROMMETHOD_SIGNATURE(J9_ROM_METHOD_FROM_RAM_METHOD(superMethod));
               J9UTF8 *superName = J9ROMMETHOD_NAME(J9_ROM_METHOD_FROM_RAM_METHOD(superMethod));

               if(   J9UTF8_LENGTH(superSignature) != J9UTF8_LENGTH(callSignature)
                   || J9UTF8_LENGTH(superName) != J9UTF8_LENGTH(callName)
                   || !(strncmp((char *)J9UTF8_DATA(superSignature),(char *)J9UTF8_DATA(callSignature),J9UTF8_LENGTH(superSignature))==0)
                   || !(strncmp((char *)J9UTF8_DATA(superName),(char *)J9UTF8_DATA(callName),J9UTF8_LENGTH(superSignature))==0) )

                  {
                  if (traceIt)
                     {
                     printf("For submethod %p,signature compare fails after bc walk superSiglen = %d subsiglen = %d supernamelen = %d subnamelen =%d\n"
                           ,subMethod,J9UTF8_LENGTH(superSignature),J9UTF8_LENGTH(callSignature),J9UTF8_LENGTH(superName),J9UTF8_LENGTH(callName));
                     fflush(stdout);
                     }
                  matches=false;
                  }
               else
                  {
                  uint8_t opcode = _code[i+2];  //an invokespecial takes up 3 bytes.. we point at second byte after for loop, so increment by 2
                  TR_J9ByteCode bc = TR_J9ByteCodeIterator::convertOpCodeToByteCodeEnum(opcode);
                  if(bc != J9BCgenericReturn || &(_code[i+3]) != J9_BYTECODE_END_FROM_ROM_METHOD(subROM)) //second condition ensures that the return is the last bytecode. See defect 148222.
                     {
                     if (traceIt)
                        {
                        printf("For submethod %p,signature compare fails after bc walk bc !=genericReturn at i(%d)+2\n", subMethod, i);
                        fflush(stdout);
                        }
                     matches=false;
                     }
                  }
               }

            if(!matches)        //matches is false if method is overridden.
               {
               jitUpdateMethodOverride(vm, cl, superMethod,subMethod);
               vm->javaVM->internalVMFunctions->atomicOrIntoConstantPool(vm->javaVM, superMethod,J9_STARTPC_METHOD_IS_OVERRIDDEN);

               if (traceIt)
                  {
                  printf("For submethod %p, pattern does not match after sig compares.\n",subMethod);
                  fflush(stdout);
                  }


               //Updating all grandparent classes overridden bits
               J9Class * tempsuperCl;
               J9VTableHeader * tempsuperVTableHeader;
               J9Method ** tempsuperVTable;
               J9Method * tempsuperMethod;
               intptr_t tempmethodCount;
               for(int32_t k=classDepth-1;k>=0;k--)
                  {

                  tempsuperCl = cl->superclasses[k];
                  tempsuperVTableHeader = J9VTABLE_HEADER_FROM_RAM_CLASS(tempsuperCl);
                  tempmethodCount =  (intptr_t)tempsuperVTableHeader->size;

                  if(methodIndex>= tempmethodCount)  //we are outside the grandparent's vft slots
                     break;

                  tempsuperVTable = J9VTABLE_FROM_HEADER(tempsuperVTableHeader);
                  tempsuperVTable = tempsuperVTable + methodIndex;
                  tempsuperMethod= *tempsuperVTable;
                  jitUpdateMethodOverride(vm, cl, tempsuperMethod,subMethod);
                  vm->javaVM->internalVMFunctions->atomicOrIntoConstantPool(vm->javaVM, tempsuperMethod,J9_STARTPC_METHOD_IS_OVERRIDDEN);
                  }
               }
            else
               {
               if (traceIt)
                  {
                  printf("For submethod %p, determined it is not overridden by bytecode walk and signature compare\n",subMethod);
                  fflush(stdout);
                  }
               }
            }
         else if (superMethod != subMethod)    // we don't have bytecodes, but the j9methods don't match, so set the overridden bit anyways
            {
            jitUpdateMethodOverride(vm, cl, superMethod,subMethod);
            vm->javaVM->internalVMFunctions->atomicOrIntoConstantPool(vm->javaVM, superMethod,J9_STARTPC_METHOD_IS_OVERRIDDEN);

            if (traceIt)
               {
               printf("For submethod %p, j9methods don't match.  Setting overridden bit. endbc - startbc = %" OMR_PRIdPTR " methodmodifiersaresafe = %d\n",subMethod,(J9_BYTECODE_END_FROM_ROM_METHOD(subROM) - J9_BYTECODE_START_FROM_ROM_METHOD(subROM)),methodModifiersAreSafe);
               fflush(stdout);
               }


            //Updating all grandparent classes overridden bits
            J9Class * tempsuperCl;
            J9VTableHeader * tempsuperVTableHeader;
            J9Method ** tempsuperVTable;
            J9Method * tempsuperMethod;
            intptr_t tempmethodCount;
            for(int32_t k=classDepth-1;k>=0;k--)
               {

               tempsuperCl = cl->superclasses[k];
               tempsuperVTableHeader = J9VTABLE_HEADER_FROM_RAM_CLASS(tempsuperCl);
               tempmethodCount =  (intptr_t)tempsuperVTableHeader->size;

               if(methodIndex >= tempmethodCount) //we are outside the grandparent's vft slots
                  break;

               tempsuperVTable = J9VTABLE_FROM_HEADER(tempsuperVTableHeader);
               tempsuperVTable = tempsuperVTable + methodIndex;
               tempsuperMethod= *tempsuperVTable;

               jitUpdateMethodOverride(vm, cl, tempsuperMethod,subMethod);
               vm->javaVM->internalVMFunctions->atomicOrIntoConstantPool(vm->javaVM, tempsuperMethod,J9_STARTPC_METHOD_IS_OVERRIDDEN);
               }

            }

         methodIndex++;
         }
      }
   }

static bool updateCHTable(J9VMThread * vmThread, J9Class  * cl)
   {
   typedef void JIT_METHOD_OVERRIDE_UPDATE(J9VMThread *, J9Class *, J9Method *, J9Method *);

   JIT_METHOD_OVERRIDE_UPDATE * callBack = jitUpdateMethodOverride;
   bool updateFailed = false;

   {
   J9JITConfig * jitConfig = vmThread->javaVM->jitConfig;

   TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig);

#if defined(J9VM_OPT_JITSERVER)
   TR_ASSERT_FATAL(compInfo->getPersistentInfo()->getRemoteCompilationMode() != JITServer::SERVER, "updateCHTable() should not be called on JITServer!\n");
#endif

   TR_PersistentCHTable * table = 0;
   if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableCHOpts))
      table = compInfo->getPersistentInfo()->getPersistentCHTable();

   TR_J9VMBase *vm = TR_J9VMBase::get(jitConfig, vmThread);
   TR_OpaqueClassBlock *clazz = ((TR_J9VMBase *)vm)->convertClassPtrToClassOffset(cl);

   char *name; int32_t len;
   bool p = TR::Options::getVerboseOption(TR_VerboseHookDetailsClassLoading);
   if (p)
      {
      name = vm->getClassNameChars(clazz, len);
      TR_VerboseLog::writeLineLocked(TR_Vlog_HD, "--updt-- %.*s\n", len, name);
      }

   int32_t classDepth = J9CLASS_DEPTH(cl) - 1;
   if (classDepth >= 0)
      {
      J9Class * superCl = cl->superclasses[classDepth];
      superCl->classDepthAndFlags |= J9AccClassHasBeenOverridden;

      TR_OpaqueClassBlock *superClazz = ((TR_J9VMBase *)vm)->convertClassPtrToClassOffset(superCl);
      if (p)
         {
         name = vm->getClassNameChars(superClazz, len);
         TR_VerboseLog::writeLineLocked(TR_Vlog_HD, "\textending %.*s\n", len, name);
         }
      if (table)
         {
         if (!table->classGotExtended(vm, compInfo->persistentMemory(), superClazz, clazz))
            updateFailed = true;
         }
      for (J9ITable * iTableEntry = (J9ITable *)cl->iTable; iTableEntry; iTableEntry = iTableEntry->next)
         {
         superCl = iTableEntry->interfaceClass;
         if (superCl != cl)
            {
            superCl->classDepthAndFlags |= J9AccClassHasBeenOverridden;
            superClazz = ((TR_J9VMBase *)vm)->convertClassPtrToClassOffset(superCl);
            if (p)
               {
               name = vm->getClassNameChars(superClazz, len);
               TR_VerboseLog::writeLineLocked(TR_Vlog_HD, "\textending interface %.*s\n", len, name);
               }
            if (table)
               {
               if (!table->classGotExtended(vm, compInfo->persistentMemory(), superClazz, clazz))
                  updateFailed = true;
               }
            }
         }
      }
   }
   // method override
   if(!TR::Options::getCmdLineOptions()->getOption(TR_DisableNewMethodOverride))
      updateOverriddenFlag(vmThread,cl);
   else
      jitUpdateInlineAttribute(vmThread, cl, (void *)callBack);


   return !updateFailed;
   }

void turnOffInterpreterProfiling(J9JITConfig *jitConfig);



/**
 * @brief  A function to get the available virtual memory.
 *
 * Note: Only supported for 32 bit Windows in Production
 *       Code currently runs under PROD_WITH_ASSUMES in 31/32 bit Linux and 31 bit z/OS but
 *       returns -1 (error) to prevent any decision making
 *
 * @param compInfo                  TR::CompilationInfo needed for getting Port Access
 * @param vmThread                  J9VMThread needed for tracepoints
 * @param availableVirtualMemoryMB  Pointer to availableVirtualMemoryKB
 * @return                          0 if success, -1 if error
 */
int32_t getAvailableVirtualMemoryMB(TR::CompilationInfo *compInfo, J9VMThread *vmThread, uint32_t *availableVirtualMemoryMB)
   {
   TRC_JIT_getAvailableVirtualMemoryMBEnter(vmThread);
#if defined(TR_TARGET_32BIT)

#if defined(WINDOWS)
   J9MemoryInfo memInfo;
   PORT_ACCESS_FROM_JITCONFIG(compInfo->getJITConfig());
   if (j9sysinfo_get_memory_info(&memInfo) != 0 ||
       memInfo.availVirtual == J9PORT_MEMINFO_NOT_AVAILABLE)
      {
      TRC_JIT_getAvailableVirtualMemoryMBExit(vmThread);
      return -1; // error case
      }
   *availableVirtualMemoryMB = (uint32_t)(memInfo.availVirtual >> 20); // conversion to uint32_t is safe on 32-bit OS

   if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseJitMemory))
         {
         uint32_t totalVirtualMemoryMB = (uint32_t)(memInfo.totalVirtual >> 20);
         uint32_t virtualMemoryUsedMB = totalVirtualMemoryMB - *availableVirtualMemoryMB;

         TR_VerboseLog::writeLineLocked(TR_Vlog_MEMORY, "Virtual Memory Used: %d MB out of %d MB", virtualMemoryUsedMB, totalVirtualMemoryMB);
         }

   TRC_JIT_getAvailableVirtualMemoryMBExit(vmThread);
   return 0;
#elif defined(LINUX)

   // Having TR::Options::_userSpaceVirtualMemoryMB < 1 means to determine the
   // userspace size dynamically. However this isn't implemented on Linux so
   // the function returns -1
   if (TR::Options::_userSpaceVirtualMemoryMB < 0)
      {
      TRC_JIT_getAvailableVirtualMemoryMBExit(vmThread);
      return -1;
      }

   uint32_t totalVirtualMemoryMB = (uint32_t)TR::Options::_userSpaceVirtualMemoryMB;

   /*
   See https://man7.org/linux/man-pages/man5/proc.5.html or run "man proc" for information on the
   /proc pseudo filesystem. The file we're using to get the virtual memory information
   is /proc/self/status. A small glimpse into the file:

   ...
   Utrace: 0
   FDSize: 256
   Groups: 476297 478112 819790 837093 887132 903597
   VmPeak:    99044 kB
   VmSize:    99044 kB
   VmLck:         0 kB
   ...

   We are interested in the VmSize field
   */

   ::FILE* statFile = fopen("/proc/self/status", "r");
   if (statFile)
      {
      static const int bufferSize = 1024;
      char buffer[bufferSize];
      bool foundVirtualMemory = false;
      int bufferStartIndex = 0;

      while (!foundVirtualMemory && !feof(statFile))
         {
         if (!fgets(buffer,bufferSize,statFile))
            break;

         if (buffer[0] == 'V')
            {
            /* Check if we have the right field, namely "VmSize" */
            if (buffer[1] == 'm'
                && buffer[2] == 'S'
                && buffer[3] == 'i'
                && buffer[4] == 'z'
                && buffer[5] == 'e'
                && buffer[6] == ':')
               {
               int bufferIndex = 0;

               /* skip whitespace until we hit the first number */
               while (bufferIndex < bufferSize && buffer[bufferIndex] != '\0' && (buffer[bufferIndex] < '0' || buffer[bufferIndex] > '9'))
                  bufferIndex++;

               /* Out of bounds check */
               if (bufferIndex == bufferSize || buffer[bufferIndex] == '\0')
                  break;

               bufferStartIndex = bufferIndex;

               /* Determine the end of the number */
               while (bufferIndex < bufferSize && buffer[bufferIndex] >= '0' && buffer[bufferIndex] <= '9')
                  bufferIndex++;

               /* Bounds and sanity check */
               if ((bufferIndex + 2) < bufferSize
                   && (bufferIndex - bufferStartIndex + 1) < 9
                   && buffer[bufferIndex] == ' '
                   && buffer[bufferIndex + 1] == 'k'
                   && buffer[bufferIndex + 2] == 'B')
                  {
                  buffer[bufferIndex] = '\0';
                  foundVirtualMemory = true;
                  }
               else
                  break;
               }
            } // if (buffer[0] == 'V')
         } // while(!feof(statFile) && !foundVirtualMemory && !error)

      fclose(statFile);

      if (!foundVirtualMemory)
         {
         TRC_JIT_getAvailableVirtualMemoryMBExit(vmThread);
         return -1; // error case
         }

      uint32_t virtualMemoryUsedMB = (uint32_t)(atoi(&buffer[bufferStartIndex]) >> 10);

      // Check if amount read from the file is less than or equal to the total virtual memory
      if (totalVirtualMemoryMB >= virtualMemoryUsedMB)
         *availableVirtualMemoryMB = totalVirtualMemoryMB - virtualMemoryUsedMB;
      else
         {
         TRC_JIT_getAvailableVirtualMemoryMBExit(vmThread);
         return -1; // error case
         }

      // Always return -1 to test this code out
      TRC_JIT_getAvailableVirtualMemoryMBExit(vmThread);
      if (TR::Options::getCmdLineOptions()->getOption(TR_EnableLowerCompilationLimitsDecisionMaking))
         return 0;
      else
         return -1;

      } // if (statFile)
   else
      {
      TRC_JIT_getAvailableVirtualMemoryMBExit(vmThread);
      return -1; // error case
      }

#elif defined(J9ZOS390)

   /*

   In 31 bit z/OS, the address space is structured as follows:

   LOCAL STORAGE MAP
    ___________________________
   |                           |80000000  <- Top of Ext. Private
   | Extended                  |
   | LSQA/SWA/229/230          |80000000  <- Max Ext. User Region Address
   |___________________________|77636000  <- ELSQA Bottom
   |                           |
   | (Free Extended Storage)   |
   |___________________________|289B8000  <- Ext. User Region Top
   |                           |
   | Extended User Region      |
   |___________________________|25200000  <- Ext. User Region Start
   :                           :
   : Extended Global Storage   :
   =======================================<- 16M Line
   : Global Storage            :
   :___________________________:  900000  <- Top of Private
   |                           |
   | LSQA/SWA/229/230          |  900000  <- Max User Region Address
   |___________________________|  821000  <- LSQA Bottom
   |                           |
   | (Free Storage)            |
   |___________________________|   4B000  <- User Region Top
   |                           |
   | User Region               |
   |___________________________|    6000  <- User Region Start
   : System Storage            :
   :___________________________:       0

   The exact addresses may vary from system to system (with the exception of the "Top of Private" and "Top of Ext. Private"
   which generally stay the same). The System Storage, LSQA, Global Storage, Extended Global Storage, and Extended LSQA cannot be
   allocated. Thus the total userspace available can be approximated by doing:
   (Max Ext. User Region Address - (Ext. User Region Start - Max User Region Address))

   The amount of memory used can be determined by the __heaprpt() function.

   */

   // Determine how much memory is being used
   hreport_t heapReport;

   if (__heaprpt(&heapReport) != 0)
      {
      TRC_JIT_getAvailableVirtualMemoryMBExit(vmThread);
      return -1; // error case
      }

   uint32_t virtualMemoryUsedMB = (uint32_t)(heapReport.__uheap_bytes_alloc >> 20);

   // Determine how much memory is available
   if (TR::Options::_userSpaceVirtualMemoryMB > 0)
      {
      if (TR::Options::_userSpaceVirtualMemoryMB >= virtualMemoryUsedMB)
         *availableVirtualMemoryMB = (uint32_t)TR::Options::_userSpaceVirtualMemoryMB - virtualMemoryUsedMB;
      else
         {
         TRC_JIT_getAvailableVirtualMemoryMBExit(vmThread);
         return -1;
         }
      }
   else // Use Dynamically determined user space
      {
      // A pointer to the current ASCB is located at offset PSAAOLD in the PSA
      void *ascb = (void *)(*((uint32_t *)PSAAOLD));

      // A pointer to the LDA is located at offset ASCBLDA in the ASCB
      struct LDA *lda = (struct LDA *)(*((uint32_t *)((uint32_t)ascb + ASCBLDA)));

      if (!lda)
         {
         TRC_JIT_getAvailableVirtualMemoryMBExit(vmThread);
         return -1;
         }

      // Determine how much of the 2 GB address space is user space
      void *maxUserRegionAddress = (void*)((uint32_t)(lda->strta) + lda->siza);
      void *maxExtUserRegionAddress = (void*)((uint32_t)(lda->estra) + lda->esiza);
      uint32_t totalVirtualMemoryMB = ((uint32_t)maxExtUserRegionAddress - ((uint32_t)(lda->estra) - (uint32_t)maxUserRegionAddress)) >> 20;

      if (totalVirtualMemoryMB >= virtualMemoryUsedMB)
         *availableVirtualMemoryMB = totalVirtualMemoryMB - virtualMemoryUsedMB;
      else
         {
         TRC_JIT_getAvailableVirtualMemoryMBExit(vmThread);
         return -1;
         }
      }

   // Always return -1 to test this code out
   TRC_JIT_getAvailableVirtualMemoryMBExit(vmThread);
   if (TR::Options::getCmdLineOptions()->getOption(TR_EnableLowerCompilationLimitsDecisionMaking))
      return 0;
   else
      return -1;

#else // Unsupported platforms
   TRC_JIT_getAvailableVirtualMemoryMBExit(vmThread);
   return -1;
#endif

#else // Not 32 bit
   TRC_JIT_getAvailableVirtualMemoryMBExit(vmThread);
   return -1;
#endif //#if defined(TR_TARGET_32BIT)
   }

/**
 * @brief A function to lower compilation limits when the JIT detects low virtual memory remaining
 *
 * 32-bit Windows due to low 2GB virtual space limit available to user mode
 *
 * When the available virtual memory is too small we may reduce some compilation
 * parameters like: scratchSpaceLimit, number of compilation threads, optServer, GCR
 *
 * @param compInfo TR::CompilationInfo needed for getting Port Access
 * @param vmThread Current thread
 */
void lowerCompilationLimitsOnLowVirtualMemory(TR::CompilationInfo *compInfo, J9VMThread *vmThread)
   {
#if defined(TR_TARGET_32BIT) && (defined(WINDOWS) || defined(LINUX) || defined(J9ZOS390))
   // Mechanism to disable this function
   if (0 == TR::Options::_userSpaceVirtualMemoryMB)
      return;

   uint32_t availableVirtualMemoryMB;

   int32_t rc = getAvailableVirtualMemoryMB(compInfo, vmThread, &availableVirtualMemoryMB);

   if (rc)
      return; //error case

   if ((availableVirtualMemoryMB < (uint32_t)TR::Options::getLowVirtualMemoryMBThreshold()/2) ||
       (availableVirtualMemoryMB < (uint32_t)TR::Options::getLowVirtualMemoryMBThreshold() &&
        compInfo->getPersistentInfo()->getNumLoadedClasses() >= TR::Options::_bigAppThreshold))
      {
      uint32_t crtTime = (uint32_t)compInfo->getPersistentInfo()->getElapsedTime(); // 49 days resolution through casting
      static bool traceSent = false;
      // Generate a trace point
      if (TrcEnabled_Trc_JIT_LowUserVirtualMemory && !traceSent)
         {
         traceSent = true; // prevent multiple messages to the trace file
         // Do I know the thread?
         if (vmThread)
            {
            // Generate a trace point
            Trc_JIT_LowUserVirtualMemory(vmThread, availableVirtualMemoryMB);
            }
         }

      // Decrease the scratch space limit
      if (TR::Options::getScratchSpaceLimit() > TR::Options::getScratchSpaceLimitKBWhenLowVirtualMemory()*1024)
         {
         TR::Options::setScratchSpaceLimit(TR::Options::getScratchSpaceLimitKBWhenLowVirtualMemory() * 1024);
         if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerbosePerformance))
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_PERF,"t=%6u WARNING: changed scratchSpaceLimit to %d KB because VMemAv=%u MB",
               crtTime, TR::Options::getScratchSpaceLimit() / 1024, availableVirtualMemoryMB);
            }
         }

      // Shut down all compilation threads but one
      if (!compInfo->getRampDownMCT())
         {
         compInfo->setRampDownMCT();
         if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCompilationThreads))
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_INFO,"t=%u setRampDownMCT because VMemAv=%u MB", crtTime, availableVirtualMemoryMB);
            }
         }

      bool doDisableGCR = false;
      if (!TR::Options::getAOTCmdLineOptions()->getOption(TR_DisableGuardedCountingRecompilations))
         {
         TR::Options::getAOTCmdLineOptions()->setOption(TR_DisableGuardedCountingRecompilations);
         doDisableGCR = true;
         }
      if (!TR::Options::getJITCmdLineOptions()->getOption(TR_DisableGuardedCountingRecompilations))
         {
         TR::Options::getJITCmdLineOptions()->setOption(TR_DisableGuardedCountingRecompilations);
         doDisableGCR = true;
         }
      if (doDisableGCR && TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerbosePerformance))
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_PERF,"t=%6u WARNING: GCR disabled because VMemAv=%u MB", crtTime, availableVirtualMemoryMB);
         }

      // Disable server mode
      bool doDisableServerMode = false;
      if (!TR::Options::getAOTCmdLineOptions()->getOption(TR_NoOptServer))
         {
         TR::Options::getAOTCmdLineOptions()->setOption(TR_NoOptServer);
         doDisableServerMode = true;
         }
      if (!TR::Options::getJITCmdLineOptions()->getOption(TR_NoOptServer))
         {
         TR::Options::getJITCmdLineOptions()->setOption(TR_NoOptServer);
         doDisableServerMode = true;
         }
      if (doDisableServerMode && TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerbosePerformance))
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_PERF,"t=%6u WARNING: server mode disabled because VMemAv=%u MB", crtTime, availableVirtualMemoryMB);
         }

      // Turn off interpreter profiling
      turnOffInterpreterProfiling(compInfo->getJITConfig());

      // Disable all future non-essential compilations if virtual memory is really low
         {
         compInfo->getPersistentInfo()->setDisableFurtherCompilation(true);
         if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerbosePerformance))
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_PERF,"t=%6u WARNING: Disable non-essential compilations because VMemAv=%u MB", crtTime, availableVirtualMemoryMB);
            }
         }
      }
#endif //#if defined(TR_TARGET_32BIT) && (defined(WINDOWS) || defined(LINUX) || defined(J9ZOS390))
   }


J9Method * getNewInstancePrototype(J9VMThread * context);

static void getClassNameIfNecessary(TR_J9VMBase *vm, TR_OpaqueClassBlock *clazz, char *&className, int32_t &len)
   {
   if (className == NULL)
      className = vm->getClassNameChars(clazz, len);
   }

static bool chTableOnClassLoad(J9VMThread *vmThread, TR_OpaqueClassBlock *clazz, TR::CompilationInfo *compInfo, TR_J9VMBase *vm)
   {
   J9Class *cl = TR::Compiler->cls.convertClassOffsetToClassPtr(clazz);
   bool allocFailed = false;

   if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableCHOpts)
#if defined(J9VM_OPT_JITSERVER)
      && compInfo->getPersistentInfo()->getRemoteCompilationMode() != JITServer::SERVER
#endif
      )
      {
      TR_PersistentClassInfo *info = compInfo->getPersistentInfo()->getPersistentCHTable()->classGotLoaded(vm, clazz);

      if (info)
         {
         // If its an interface class it won't be initialized, so we have to update the CHTable now.
         // Otherwise, we will update the CHTable once the class gets initialized (i.e. live)
         //
         if (vm->isInterfaceClass(clazz))
            {
            if (!updateCHTable(vmThread, cl))
               {
               allocFailed = true;
               compInfo->getPersistentInfo()->getPersistentCHTable()->removeClass(vm, clazz, info, true);
               }
            }
         else if (vm->isClassArray(clazz))
            {
            if (!compInfo->getPersistentInfo()->getPersistentCHTable()->classGotInitialized(vm, compInfo->persistentMemory(), clazz))
               {
               TR_PersistentClassInfo *arrayClazzInfo = compInfo->getPersistentInfo()->getPersistentCHTable()->findClassInfo(clazz);
               if (arrayClazzInfo)
                  compInfo->getPersistentInfo()->getPersistentCHTable()->removeClass(vm, clazz, arrayClazzInfo, false);
               }
            TR_OpaqueClassBlock *compClazz = vm->getComponentClassFromArrayClass(clazz);
            if (compClazz)
               {
               TR_PersistentClassInfo *clazzInfo = compInfo->getPersistentInfo()->getPersistentCHTable()->findClassInfo(compClazz);
               if (clazzInfo && !clazzInfo->isInitialized())
                  {
                  bool initFailed = false;
                  if (!compInfo->getPersistentInfo()->getPersistentCHTable()->classGotInitialized(vm, compInfo->persistentMemory(), compClazz))
                     initFailed = true;

                  if (!initFailed &&
                      !vm->isClassArray(compClazz) &&
                      !vm->isInterfaceClass(compClazz) &&
                      !vm->isPrimitiveClass(compClazz))
                     initFailed = !updateCHTable(vmThread, ((J9Class *) compClazz));

                  if (initFailed)
                     {
                     compInfo->getPersistentInfo()->getPersistentCHTable()->removeClass(vm, compClazz, clazzInfo, false);
                     allocFailed = true;
                     }
                  }
               }
            }
         }
      else
         allocFailed = true;
      }

   return allocFailed;
   }

static void checkForLockReservation(J9VMThread *vmThread,
                                    J9JITConfig *jitConfig,
                                    J9ClassLoader *classLoader,
                                    TR_OpaqueClassBlock *clazz,
                                    TR_J9VMBase *vm,
                                    TR::CompilationInfo *compInfo,
                                    char * className,
                                    int32_t classNameLen)
   {
   TR::Options * options = TR::Options::getCmdLineOptions();
   if (options->getOption(TR_ReservingLocks)
#if defined(J9VM_OPT_JITSERVER)
      && compInfo->getPersistentInfo()->getRemoteCompilationMode() != JITServer::SERVER
#endif
      )
      {
      J9Class *cl = TR::Compiler->cls.convertClassOffsetToClassPtr(clazz);
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(TR_J9VMBase::get(jitConfig, 0));
      int lwOffset = fej9->getByteOffsetToLockword(clazz);
      if (lwOffset > 0)
         {
         bool reserve = options->getOption(TR_ReserveAllLocks);

         if (!reserve && ((J9JavaVM *)vmThread->javaVM)->systemClassLoader == classLoader)
            {
            getClassNameIfNecessary(vm, clazz, className, classNameLen);
            if (classNameLen == 22 && !strncmp(className, "java/lang/StringBuffer", 22))
               reserve = true;
            else if (classNameLen == 16 && !strncmp(className, "java/util/Random", 16))
               reserve = true;
            }

         TR::SimpleRegex *resRegex = options->getLockReserveClass();
         if (!reserve && resRegex != NULL)
            {
            getClassNameIfNecessary(vm, clazz, className, classNameLen);
            if (TR::SimpleRegex::match(resRegex, className))
               reserve = true;
            }

         if (reserve)
            {
            TR_PersistentClassInfo *classInfo = compInfo
               ->getPersistentInfo()
               ->getPersistentCHTable()
               ->findClassInfo(clazz);

            if (classInfo != NULL)
               {
               classInfo->setReservable();
               if (!TR::Options::_aggressiveLockReservation)
                  J9CLASS_EXTENDED_FLAGS_SET(cl, J9ClassReservableLockWordInit);
               }
            }
         }
      }
   }

void jitHookClassLoadHelper(J9VMThread *vmThread,
                            J9JITConfig * jitConfig,
                            J9Class * cl,
                            TR::CompilationInfo *compInfo,
                            UDATA *classLoadEventFailed)
   {
   bool allocFailed = false;
   TR_J9VMBase *vm = TR_J9VMBase::get(jitConfig, vmThread);
   TR_OpaqueClassBlock *clazz = TR::Compiler->cls.convertClassPtrToClassOffset(cl);
   jitAcquireClassTableMutex(vmThread);

   compInfo->getPersistentInfo()->incNumLoadedClasses();

   if (compInfo->getPersistentInfo()->getNumLoadedClasses() == TR::Options::_bigAppThreshold)
      {
#if defined(TR_TARGET_32BIT) && (defined(WINDOWS) || defined(LINUX) || defined(J9ZOS390))
      // When we reach the bigAppThreshold limit we examine the available virtual memory
      // and if this is too small we reduce some compilation parameters like:
      // scratchSpaceLimit, number of compilation threads, optServer, GCR
      lowerCompilationLimitsOnLowVirtualMemory(compInfo, vmThread);
#endif
      // For large applications be more conservative with hot compilations
      if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableConservativeHotRecompilationForServerMode))
         {
         TR::Options::_sampleThreshold /= 3; // divide by 3 to become more conservative (3% instead of 1%)
         TR::Options::_resetCountThreshold /= 3;
         if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerbosePerformance))
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_PERF,"t=%6u INFO: Changed sampleThreshold to %d",
               (uint32_t)compInfo->getPersistentInfo()->getElapsedTime(), TR::Options::_sampleThreshold);
            }
         }
      }

   // todo: why is the override bit on already....temporarily reset it
   // ALI 20031015: I think I have fixed the above todo - we should never
   // get an inconsistent state now.  The following should be unnecessary -
   // verify and remove  FIXME
   cl->classDepthAndFlags &= ~J9AccClassHasBeenOverridden;

   // For regular classes, cl->classLoader points to the correct class loader by the time we enter this hook.
   // For anonymous classes however, it points to the anonymous class loader and not the correct class loader.
   // Once the class is fully loaded the classLoader member will be updated to point to the correct class loader,
   // which is the anonymous class's host class's class loader, but that doesn't do us any good in this hook.
   // We need the correct class loader right now, so we grab the host class's class loader instead.
   // For regular classes, cl->hostClass points back to the class itself, so by doing this we get the correct
   // class loader for both regular and anonymous classes without having to check if this is an anonymous class.
   J9ClassLoader *classLoader = cl->hostClass->classLoader;

   bool p = TR::Options::getVerboseOption(TR_VerboseHookDetailsClassLoading);
   char * className = NULL;
   int32_t classNameLen = -1;
   if (p)
      {
      getClassNameIfNecessary(vm, clazz, className, classNameLen);
      TR_VerboseLog::writeLineLocked(TR_Vlog_HD, "--load-- loader %p, class %p : %.*s\n", classLoader, cl, classNameLen, className);
      }

   // add the newInstance hook
#if defined(J9ZOS390)
   cl->romableAotITable = (UDATA) TOC_UNWRAP_ADDRESS((void *)&jitTranslateNewInstanceMethod);
#elif defined(TR_HOST_POWER) && (defined(TR_HOST_64BIT) || defined(AIXPPC)) && !defined(__LITTLE_ENDIAN__)
   cl->romableAotITable = (UDATA) (*(void **)jitTranslateNewInstanceMethod);
#else
   cl->romableAotITable = (UDATA) jitTranslateNewInstanceMethod;
#endif

   if (((J9JavaVM *)vmThread->javaVM)->systemClassLoader != classLoader)
      {
      TR::Options::_numberOfUserClassesLoaded ++;
      }

   compInfo->getPersistentInfo()->getPersistentClassLoaderTable()->associateClassLoaderWithClass(vmThread, classLoader, clazz);

   // Update the count for the newInstance
   //
   TR::Options * options = TR::Options::getCmdLineOptions();
   if (options->anOptionSetContainsACountValue())
      {
      J9Method *method = getNewInstancePrototype(vmThread);
      if (method)
         {
         TR::OptionSet * optionSet = findOptionSet(method, false);
         if (optionSet)
            options = optionSet->getOptions();
         }
      }
   //fprintf(stderr, "Will set the count for NewInstancePrototype to %d\n", options->getInitialCount());
   cl->newInstanceCount = options->getInitialCount();

   allocFailed = chTableOnClassLoad(vmThread, clazz, compInfo, vm);

   compInfo->getPersistentInfo()->ensureUnloadedAddressSetsAreInitialized();
   // TODO: change the above line to something like the following in order to handle allocation failures:
   // if (!allocFailed)
   //    allocFailed = !compInfo->getPersistentInfo()->ensureUnloadedAddressSetsAreInitialized();

    *classLoadEventFailed = allocFailed;

   // Determine whether this class gets lock reservation
   checkForLockReservation(vmThread, jitConfig, classLoader, clazz, vm, compInfo, className, classNameLen);

   jitReleaseClassTableMutex(vmThread);
   }

static void jitHookClassLoad(J9HookInterface * * hookInterface, UDATA eventNum, void * eventData, void * userData)
   {
   J9VMInternalClassLoadEvent * classLoadEvent = (J9VMInternalClassLoadEvent *)eventData;
   J9VMThread * vmThread = classLoadEvent->currentThread;
   J9Class * cl = classLoadEvent->clazz;
   J9JITConfig * jitConfig = vmThread->javaVM->jitConfig;
   if (jitConfig == 0)
      return; // if a hook gets called after freeJitConfig then not much else we can do

   TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig);
   TR_PersistentCHTable * cht = NULL;
   if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableCHOpts))
      cht = compInfo->getPersistentInfo()->getPersistentCHTable();
   if (cht && !cht->isActive())
      return;

   getOutOfIdleStates(TR::CompilationInfo::SAMPLER_DEEPIDLE, compInfo, "class load");

   jitHookClassLoadHelper(vmThread, jitConfig, cl, compInfo, &(classLoadEvent->failed));
   }

int32_t loadingClasses;

static bool chTableOnClassPreinitialize(J9VMThread *vmThread,
                                        J9Class *cl,
                                        TR_OpaqueClassBlock *clazz,
                                        TR::CompilationInfo *compInfo,
                                        TR_J9VMBase *vm)
   {
   bool initFailed = false;

#if defined(J9VM_OPT_JITSERVER)
   if (compInfo->getPersistentInfo()->getRemoteCompilationMode() != JITServer::SERVER)
#endif
      {
      if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableCHOpts))
         {
         if (!initFailed && !compInfo->getPersistentInfo()->getPersistentCHTable()->classGotInitialized(vm, compInfo->persistentMemory(), clazz))
            initFailed = true;

         if (!initFailed &&
             !vm->isInterfaceClass(clazz))
            updateCHTable(vmThread, cl);
         }
      else
         {
         if (!initFailed && !updateCHTable(vmThread, cl))
            initFailed = true;
         }

      if (initFailed)
         {
         TR_PersistentClassInfo *info = compInfo->getPersistentInfo()->getPersistentCHTable()->findClassInfo(clazz);
         compInfo->getPersistentInfo()->getPersistentCHTable()->removeClass(vm, clazz, info, false);
         }
      }

   return initFailed;
   }

void jitHookClassPreinitializeHelper(J9VMThread *vmThread,
                                     J9JITConfig *jitConfig,
                                     J9Class *cl,
                                     UDATA *classPreinitializeEventFailed)
   {
   TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig);
   TR_J9VMBase *vm = TR_J9VMBase::get(jitConfig, vmThread);
   TR_OpaqueClassBlock *clazz = ((TR_J9VMBase *)vm)->convertClassPtrToClassOffset(cl);
   bool p = TR::Options::getVerboseOption(TR_VerboseHookDetailsClassLoading);
   if (p)
      {
      int32_t len;
      char * className = vm->getClassNameChars(clazz, len);
      TR_VerboseLog::writeLineLocked(TR_Vlog_HD, "--init-- %.*s\n", len, className);
      }

   jitAcquireClassTableMutex(vmThread);

   *classPreinitializeEventFailed = chTableOnClassPreinitialize(vmThread, cl, clazz, compInfo, vm);

   jitReleaseClassTableMutex(vmThread);
   }


/// This routine is used to indicate successful initialization of the J9Class
/// before any Java code (<clinit>) is run. When analyzing code in the <clinit>
/// with CHTable assumptions, this ensures that the CHTable is updated correctly.
/// Otherwise a class will not be seen as having been initialized in the Java code
/// reachable from the <clinit>; causing possibly incorrect devirtualization or other
/// CHTable opts to be applied in a method called by <clinit> (if the <clinit> for class C calls a
/// virtual method on an object of class C which is instantiated in the code reachable
/// from <clinit> (this was an actual WSAD scenario)).
///
static void jitHookClassPreinitialize(J9HookInterface * * hookInterface, UDATA eventNum, void * eventData, void * userData)
   {
   J9VMClassPreinitializeEvent * classPreinitializeEvent = (J9VMClassPreinitializeEvent *)eventData;
   J9VMThread * vmThread = classPreinitializeEvent->currentThread;
   J9Class * cl = classPreinitializeEvent->clazz;

   J9JITConfig * jitConfig = vmThread->javaVM->jitConfig;
   if (jitConfig == 0)
      return; // if a hook gets called after freeJitConfig then not much else we can do

   loadingClasses = true;

   TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig);
   TR_PersistentCHTable * cht = NULL;
   if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableCHOpts))
      cht = compInfo->getPersistentInfo()->getPersistentCHTable();
   if (cht && !cht->isActive())
      return;

   jitHookClassPreinitializeHelper(vmThread, jitConfig, cl, &(classPreinitializeEvent->failed));
   }

static void jitHookClassInitialize(J9HookInterface * * hookInterface, UDATA eventNum, void * eventData, void * userData)
   {
   J9VMClassInitializeEvent * classInitializeEvent = (J9VMClassInitializeEvent *)eventData;
   J9VMThread * vmThread = classInitializeEvent->currentThread;
   J9Class * cl = classInitializeEvent->clazz;

   J9JITConfig * jitConfig = vmThread->javaVM->jitConfig;
   if (jitConfig == 0)
      return; // if a hook gets called after freeJitConfig then not much else we can do

   loadingClasses = false;
   }

int32_t returnIprofilerState()
   {
#if defined(J9VM_INTERP_PROFILING_BYTECODES)
   return interpreterProfilingState;
#else
   return IPROFILING_STATE_OFF;
#endif
   }

#if defined(J9VM_INTERP_PROFILING_BYTECODES)

#define TEST_verbose 0
#define TEST_events  0
#define TEST_records 0

void turnOffInterpreterProfiling(J9JITConfig *jitConfig)
   {
   // Turn off interpreter profiling
   //
   if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableInterpreterProfiling))
      {
      if (interpreterProfilingState != IPROFILING_STATE_OFF)
         {
         interpreterProfilingState = IPROFILING_STATE_OFF;
         J9HookInterface ** hook = jitConfig->javaVM->internalVMFunctions->getVMHookInterface(jitConfig->javaVM);
         (*hook)->J9HookUnregister(hook, J9HOOK_VM_PROFILING_BYTECODE_BUFFER_FULL, jitHookBytecodeProfiling, NULL);

         PORT_ACCESS_FROM_JITCONFIG(jitConfig);
         if (TR::Options::getCmdLineOptions()->getOption(TR_VerboseInterpreterProfiling))
            {
            TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig);
            TR_VerboseLog::writeLineLocked(TR_Vlog_IPROFILER,"t=%6u IProfiler stopped", (uint32_t)compInfo->getPersistentInfo()->getElapsedTime());
            }
         }
      }
   }

/// The following two methods (stopInterpreterProfiling and restartInterpreterProfiling)
/// are used when we disable/enable JIT compilation at runtime
/// @{
void stopInterpreterProfiling(J9JITConfig *jitConfig)
   {
   // Turn off interpreter profiling
   //
   if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableInterpreterProfiling))
      {
      turnOffInterpreterProfiling(jitConfig);
      TR::Options::getCmdLineOptions()->setOption(TR_DisableInterpreterProfiling, true);
      }
   }

void restartInterpreterProfiling()
   {
   if (interpreterProfilingWasOnAtStartup && TR::Options::getCmdLineOptions()->getOption(TR_DisableInterpreterProfiling))
      {
      TR::Options::getCmdLineOptions()->setOption(TR_DisableInterpreterProfiling, false);
      // The hook for interpreter profiling will be registered when a class load phase is detected
      // We could more fancy and memorize the state of interpreter profiling
      // and revert to the old state.
      }
   }

/// @}

static bool checkAndTurnOffProfilingHook(TR::CompilationInfo * compInfo)
   {
   if (!compInfo->getPersistentInfo()->isClassLoadingPhase())
      {
      if (interpreterProfilingState == IPROFILING_STATE_ON)
         {
         interpreterProfilingRecordsCount = 0;
         interpreterProfilingState = IPROFILING_STATE_GOING_OFF;
         }
      }
   else if ((interpreterProfilingState == IPROFILING_STATE_GOING_OFF) ||
            (interpreterProfilingState == IPROFILING_STATE_OFF))
      {
      interpreterProfilingState = IPROFILING_STATE_ON;
      }

   return false;
   }



static void jitHookBytecodeProfiling(J9HookInterface * * hook, UDATA eventNum, void * eventData, void * userData)
   {
   J9VMProfilingBytecodeBufferFullEvent* event = (J9VMProfilingBytecodeBufferFullEvent*)eventData;
   J9VMThread * vmThread = event->currentThread;
   const U_8* cursor = event->bufferStart;
   UDATA size = event->bufferSize;
   UDATA records = 0;
   TR_J9VMBase * fe = NULL;

   PORT_ACCESS_FROM_VMC(vmThread);

   if (TEST_verbose)
      {
      j9tty_printf(PORTLIB, "%p - Buffer full: %zu bytes at %p\n", vmThread, size, cursor);
      }
   //TEST_events += 1;

   J9JITConfig * jitConfig = vmThread->javaVM->jitConfig;
   if (jitConfig)
      fe = TR_J9VMBase::get(jitConfig, vmThread);
   else
      return;
   TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig);
   // In DEEP_IDLE we seem to be interrupted too often by IProfiler buffers
   // sent by VM. To alleviate this issue the sampling thread will get out
   // of DEEP_IDLE only if the VM sends us buffers "very frequently".
   // Basically we measure the time needed for the arrival of several such
   // buffers and interrupt the sleep of the sampling thread if this time
   // is less than 5 seconds (configurable)
   if (compInfo->getSamplerState() == TR::CompilationInfo::SAMPLER_DEEPIDLE)
      {
      static const uint32_t ARRAY_SIZE = 4; // must be power of two due to wrap around logic
      static uint64_t _IPBufferArrivalTimes[ARRAY_SIZE] = { 0 };
      static uint32_t crtPos = 0;

      if (compInfo->getIProfilerBufferArrivalMonitor()) // safety check; will most likely exist
         {
         PORT_ACCESS_FROM_JITCONFIG(jitConfig);
         uint64_t crtTime = j9time_current_time_millis();

         compInfo->getIProfilerBufferArrivalMonitor()->enter();

         uint32_t oldestPos = (crtPos + 1) & (ARRAY_SIZE - 1);
         uint64_t oldestTime = _IPBufferArrivalTimes[oldestPos]; // read the oldest entry
         crtPos = oldestPos; // update position in the circular buffer
         _IPBufferArrivalTimes[oldestPos] = crtTime; // write the new entry

         compInfo->getIProfilerBufferArrivalMonitor()->exit();

         // Get out of deepIdle only if ARRAY_SIZE buffers arrive within 5 seconds
         // Note that crtTime can go backwards, however, this means we will get
         // out of deep idle more often that we should for a small period of time
         // (until we fill the entire array with new values)
         if ((oldestTime != 0) && (crtTime < oldestTime + TR::Options::_iProfilerBufferInterarrivalTimeToExitDeepIdle))
            {
            getOutOfIdleStates(TR::CompilationInfo::SAMPLER_DEEPIDLE, compInfo, "IP buffer received"); // this changes the state to IDLE
            // TODO: try to induce compilations for methods on the Java stack
            }
         else
            {
            if (TR::Options::getCmdLineOptions()->getOption(TR_VerboseInterpreterProfiling))
               {
               uint32_t t = (uint32_t)(crtTime - compInfo->getPersistentInfo()->getStartTime());
               TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "t=%u\tSampling thread avoided an interruption in DEEP_IDLE due to IProfiler buffer being received", t);
               }
            }
         }
      }
   TR_ASSERT(fe, "FrontEnd must exist\n");
   TR_IProfiler *iProfiler = fe->getIProfiler();
   if (!iProfiler || !iProfiler->isIProfilingEnabled())
      {
      // Even if Iprofiler has been turned-off/stopped/disabled we should clear
      // the iprofiler buffer the VM has given us to avoid livelock (RTC 61279)
      vmThread->profilingBufferCursor = (U_8*)cursor; // discard the content
      return;
      }
   iProfiler->incrementNumRequests();
   if (TR::Options::getCmdLineOptions()->getOption(TR_DisableIProfilerThread))
      {
      iProfiler->parseBuffer(vmThread, cursor, size);
      vmThread->profilingBufferCursor = (U_8*)cursor; // reuse the same buffer
      }
   else // use a separate thread for iprofiler
      {
      if (!iProfiler->processProfilingBuffer(vmThread, cursor, size))
         {
         // iprofilerThread did not process or discard the buffer,
         // but it delegated this task to the java thread itself
         iProfiler->parseBuffer(vmThread, cursor, size);
         vmThread->profilingBufferCursor = (U_8*)cursor; // reuse the same buffer
         }
      }

   checkAndTurnOffProfilingHook(compInfo);
   if (iProfiler->getProfilerMemoryFootprint() >= TR::Options::_iProfilerMemoryConsumptionLimit)
      {
      if (TR::Options::getCmdLineOptions()->getOption(TR_VerboseInterpreterProfiling))
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_IPROFILER,"t=%6u IProfiler exceeded memory limit %d", (uint32_t)compInfo->getPersistentInfo()->getElapsedTime(), iProfiler->getProfilerMemoryFootprint());
         }
      turnOffInterpreterProfiling(jitConfig);
      // Generate a trace point
      Trc_JIT_IProfilerCapReached(vmThread, (iProfiler->getProfilerMemoryFootprint() >> 10));
      }

   if (interpreterProfilingState == IPROFILING_STATE_GOING_OFF)
      {
      //interpreterProfilingRecordsCount += records;
      // interpreterProfilingRecordsCount could be artificially set very high from other parts of the code

      if (interpreterProfilingRecordsCount >= TR::Options::_iprofilerSamplesBeforeTurningOff )
         {
         (*hook)->J9HookUnregister(hook, J9HOOK_VM_PROFILING_BYTECODE_BUFFER_FULL, jitHookBytecodeProfiling, NULL);
         if (TR::Options::getCmdLineOptions()->getOption(TR_VerboseInterpreterProfiling))
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_IPROFILER,"t=%6u IProfiler stopped after %d records", (uint32_t)compInfo->getPersistentInfo()->getElapsedTime(), TEST_records+records);
            }
         interpreterProfilingState = IPROFILING_STATE_OFF;
         }
      }
   }

#endif /* J9VM_INTERP_PROFILING_BYTECODES  */

extern IDATA compileClasses(J9VMThread *, const char * pattern);

static void jitHookAboutToRunMain(J9HookInterface * * hook, UDATA eventNum, void * eventData, void * userData)
   {
   J9VMLookupJNIIDEvent * event = (J9VMLookupJNIIDEvent *)eventData;
   J9VMThread * vmThread = event->currentThread;
   J9JavaVM * javaVM = vmThread->javaVM;
   J9JITConfig * jitConfig = javaVM->jitConfig;
   if (jitConfig == 0)
      return; // if a hook gets called after freeJitConfig then not much else we can do

   if (!event->isStatic || event->isField || strncmp(event->name, "main", 4) || strncmp(event->signature, "([Ljava/lang/String;)V", 22))
      return;

   J9HookInterface * * vmHooks = vmThread->javaVM->internalVMFunctions->getVMHookInterface(vmThread->javaVM);
   (*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_LOOKUP_JNI_ID, jitHookAboutToRunMain, NULL);

   bool alreadyHaveVMAccess = ((vmThread->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS) != 0);
   if (!alreadyHaveVMAccess)
      javaVM->internalVMFunctions->internalAcquireVMAccess(vmThread);
   javaVM->internalVMFunctions->acquireExclusiveVMAccess(vmThread);
   jitConfig->runtimeFlags &= ~J9JIT_DEFER_JIT;

#ifdef J9VM_JIT_SUPPORTS_DIRECT_JNI
   initializeDirectJNI(javaVM);
#endif

   jitResetAllMethodsAtStartup(vmThread);
   javaVM->internalVMFunctions->releaseExclusiveVMAccess(vmThread);
   if (!alreadyHaveVMAccess)
      javaVM->internalVMFunctions->internalReleaseVMAccess(vmThread);

   if (TR::Options::getCmdLineOptions()->getOption(TR_jitAllAtMain))
      compileClasses(vmThread, "");
   }


#if defined(J9VM_INTERP_PROFILING_BYTECODES)
// Below, options and jitConfig are guaranteed to be not null
void printIprofilerStats(TR::Options *options, J9JITConfig * jitConfig, TR_IProfiler *iProfiler)
   {
   if (!options->getOption(TR_DisableInterpreterProfiling))
      {
      PORT_ACCESS_FROM_JITCONFIG(jitConfig);
      if (TR::Options::getCmdLineOptions()->getOption(TR_VerboseInterpreterProfiling))
         {
         j9tty_printf(PORTLIB, "VM shutdown event received.\n");
         j9tty_printf(PORTLIB, "Total events: %d\n", TEST_events);
         j9tty_printf(PORTLIB, "Total records: %d\n", TEST_records);
         j9tty_printf(PORTLIB, "Total method persistence opportunities: %d\n", TR_IProfiler::_STATS_methodPersistenceAttempts);
         j9tty_printf(PORTLIB, "Total jitprofile entries: %d\n", TR_IProfiler::_STATS_methodPersisted);
         j9tty_printf(PORTLIB, "Total IProfiler persistence aborted due to locked entry:                %d\n", TR_IProfiler::_STATS_abortedPersistence);
         j9tty_printf(PORTLIB, "Total IProfiler persistence failed:                                     %d\n", TR_IProfiler::_STATS_persistError);
         j9tty_printf(PORTLIB, "Total IProfiler persistence aborted because SCC full:                   %d\n", TR_IProfiler::_STATS_methodNotPersisted_SCCfull);
         j9tty_printf(PORTLIB, "Total IProfiler persistence aborted because ROM class in not in SCC:    %d\n", TR_IProfiler::_STATS_methodNotPersisted_classNotInSCC);
         j9tty_printf(PORTLIB, "Total IProfiler persistence aborted due to other reasons:               %d\n", TR_IProfiler::_STATS_methodNotPersisted_other);
         j9tty_printf(PORTLIB, "Total IProfiler persistence aborted because already stored:             %d\n", TR_IProfiler::_STATS_methodNotPersisted_alreadyStored);
         j9tty_printf(PORTLIB, "Total IProfiler persistence aborted because nothing needs to be stored: %d\n", TR_IProfiler::_STATS_methodNotPersisted_noEntries);
         j9tty_printf(PORTLIB, "Total IProfiler persisted delayed:                                      %d\n", TR_IProfiler::_STATS_methodNotPersisted_delayed);
         j9tty_printf(PORTLIB, "Total records persisted:                        %d\n", TR_IProfiler::_STATS_entriesPersisted);
         j9tty_printf(PORTLIB, "Total records not persisted_NotInSCC:           %d\n", TR_IProfiler::_STATS_entriesNotPersisted_NotInSCC);
         j9tty_printf(PORTLIB, "Total records not persisted_unloaded:           %d\n", TR_IProfiler::_STATS_entriesNotPersisted_Unloaded);
         j9tty_printf(PORTLIB, "Total records not persisted_noInfo in bc table: %d\n", TR_IProfiler::_STATS_entriesNotPersisted_NoInfo);
         j9tty_printf(PORTLIB, "Total records not persisted_Other:              %d\n", TR_IProfiler::_STATS_entriesNotPersisted_Other);
         j9tty_printf(PORTLIB, "IP Total Persistent Read Failed Attempts:          %d\n", TR_IProfiler::_STATS_persistedIPReadFail);

         j9tty_printf(PORTLIB, "IP Total Persistent Reads with Bad Data:           %d\n", TR_IProfiler::_STATS_persistedIPReadHadBadData);
         j9tty_printf(PORTLIB, "IP Total Persistent Read Success:                  %d\n", TR_IProfiler::_STATS_persistedIPReadSuccess);
         j9tty_printf(PORTLIB, "IP Total Persistent vs Current Data Differ:        %d\n", TR_IProfiler::_STATS_persistedAndCurrentIPDataDiffer);
         j9tty_printf(PORTLIB, "IP Total Persistent vs Current Data Match:         %d\n", TR_IProfiler::_STATS_persistedAndCurrentIPDataMatch);
         j9tty_printf(PORTLIB, "IP Total Current Read Fail:                        %d\n", TR_IProfiler::_STATS_currentIPReadFail);
         j9tty_printf(PORTLIB, "IP Total Current Read Success:                     %d\n", TR_IProfiler::_STATS_currentIPReadSuccess);
         j9tty_printf(PORTLIB, "IP Total Current Read Bad Data:                    %d\n", TR_IProfiler::_STATS_currentIPReadHadBadData);
         j9tty_printf(PORTLIB, "Total records read: %d\n", TR_IProfiler::_STATS_IPEntryRead);
         j9tty_printf(PORTLIB, "Total records choose persistent: %d\n", TR_IProfiler::_STATS_IPEntryChoosePersistent);
         }
      if (TR_IProfiler::_STATS_abortedPersistence > 0)
         {
         TR_ASSERT(TR_IProfiler::_STATS_methodPersisted / TR_IProfiler::_STATS_abortedPersistence > 20 ||
            TR_IProfiler::_STATS_methodPersisted < 200,
            "too many aborted persistence attempts due to locked entries (%d aborted, %d total methods persisted)",
            TR_IProfiler::_STATS_abortedPersistence, TR_IProfiler::_STATS_methodPersisted);
         }
      if (TR::Options::getCmdLineOptions()->getOption(TR_EnableNewAllocationProfiling))
         iProfiler->printAllocationReport();
      if (TEST_verbose || TR::Options::getCmdLineOptions()->getOption(TR_VerboseInterpreterProfiling))
         iProfiler->outputStats();
      }
   }
#endif


/// JIT cleanup code
void JitShutdown(J9JITConfig * jitConfig)
   {
   static bool jitShutdownCalled = false;

   if (!jitConfig)
      return; // there isn't much we can do without a jitConfig

   J9JavaVM * javaVM = jitConfig->javaVM;
   J9VMThread *vmThread = javaVM->internalVMFunctions->currentVMThread(javaVM);

   // Prevent calling this function twice;
   // Races cannot occur because only the main thread executes shutdown stages
   if (jitShutdownCalled)
      {
      TRC_JIT_ShutDownEnd(vmThread, "jitShutdownCalled is true");
      return;
      }
   jitShutdownCalled = true;

   TRC_JIT_ShutDownBegin(vmThread);

   TR_J9VMBase * vm = TR_J9VMBase::get(jitConfig, NULL);
   // The rest of this function seems to only be valid if we have managed to allocate at least the global VM.
   if (!vm)
      {
      TRC_JIT_ShutDownEnd(vmThread, "vm variable is NULL");
      return;
      }


   PORT_ACCESS_FROM_JAVAVM(javaVM);

   TR::Options *options = TR::Options::getCmdLineOptions();
#if defined(J9VM_INTERP_PROFILING_BYTECODES)
   TR_ASSERT(jitConfig->privateConfig, "privateConfig must exist if a frontend exists\n");
   TR_IProfiler *iProfiler = vm->getIProfiler();

   // The TR_DisableInterpreterProfiling options can change during run
   // so the fact that this option is true doesn't mean that IProfiler structures were not allocated
   if (options /* && !options->getOption(TR_DisableInterpreterProfiling) */ && iProfiler)
      {
      printIprofilerStats(options, jitConfig, iProfiler);
      // Prevent the interpreter to accumulate more info
      // stopInterpreterProfiling is stronger than turnOff... because it prevents the reactivation
      // by setting TR_DisableInterpreterProfiling option to false
      stopInterpreterProfiling(jitConfig);
      if (!options->getOption(TR_DisableIProfilerThread))
         iProfiler->stopIProfilerThread();
#ifdef DEBUG
      uint32_t lockedEntries = iProfiler->releaseAllEntries();
      TR_ASSERT(lockedEntries == 0, "some entries were still locked on shutdown");
#endif
      // Dump all IProfiler related to virtual/interface invokes and instanceof/checkcasts
      // to track possible performance issues
      // iProfiler->dumpIPBCDataCallGraph(vmThread);

      // free the IProfiler structures

      // Deallocate the buffers used for interpreter profiling
      // Must be called when we are sure that no java thread can be running
      // Or at least that Java threads do not try to collect IProfiler info
      // We turn off IProfiler above, but let's add another check
      if (interpreterProfilingState == IPROFILING_STATE_OFF)
         iProfiler->deallocateIProfilerBuffers();
      iProfiler->shutdown();
      }
#endif // J9VM_INTERP_PROFILING_BYTECODES

   TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig);

   TR_HWProfiler *hwProfiler = ((TR_JitPrivateConfig*)(jitConfig->privateConfig))->hwProfiler;
   if (compInfo->getPersistentInfo()->isRuntimeInstrumentationEnabled())
      {
      char * printRIStats = feGetEnv("TR_PrintRIStats");
      if (printRIStats)
         hwProfiler->printStats();

      if (!options->getOption(TR_DisableHWProfilerThread))
         {
         hwProfiler->stopHWProfilerThread(javaVM);
         hwProfiler->releaseAllEntries();
         }
      }

   // Hardware profiling
#ifdef J9VM_JIT_RUNTIME_INSTRUMENTATION
   if (NULL != hwProfiler)
      shutdownJITRuntimeInstrumentation(javaVM);
#endif

   TR_JProfilerThread *jProfiler = ((TR_JitPrivateConfig*)(jitConfig->privateConfig))->jProfiler;
   if (jProfiler != NULL)
      jProfiler->stop(javaVM);

   if (options && options->getOption(TR_DumpFinalMethodNamesAndCounts))
      {
      try
         {
         TR::RawAllocator rawAllocator(jitConfig->javaVM);
         J9::SegmentAllocator segmentAllocator(MEMORY_TYPE_JIT_SCRATCH_SPACE | MEMORY_TYPE_VIRTUAL, *jitConfig->javaVM);
         J9::SystemSegmentProvider regionSegmentProvider(
            1 << 20,
            1 << 20,
            TR::Options::getScratchSpaceLimit(),
            segmentAllocator,
            rawAllocator
            );
         TR::Region dispatchRegion(regionSegmentProvider, rawAllocator);
         TR_Memory trMemory(*compInfo->persistentMemory(), dispatchRegion);

         compInfo->getPersistentInfo()->getPersistentCHTable()->dumpMethodCounts(vm, trMemory);
         }
      catch (const std::exception &e)
         {
         fprintf(stderr, "Failed to dump Final Method Names and Counts\n");
         }
      }

   TR::Compilation::shutdown(vm);

   TR::CompilationController::shutdown();

   if (!vm->isAOT_DEPRECATED_DO_NOT_USE())
      stopSamplingThread(jitConfig);

#if defined(J9VM_OPT_JITSERVER)
   JITServerStatisticsThread *statsThreadObj = ((TR_JitPrivateConfig*)(jitConfig->privateConfig))->statisticsThreadObject;
   if (statsThreadObj)
      {
      statsThreadObj->stopStatisticsThread(jitConfig);
      }

   if (compInfo->useSSL())
      {
      (*OEVP_cleanup)();
      }
#endif

   TR_DebuggingCounters::report();
   accumulateAndPrintDebugCounters(jitConfig);

   // TODO:JSR292: Delete or protect with env var
   /*
   TR_FrontEnd *fe = TR_J9VMBase::get(jitConfig, vmThread);
   TR_MHJ2IThunkTable *thunkTable = TR::CompilationInfo::get(jitConfig)->getPersistentInfo()->getInvokeExactJ2IThunkTable();
   if (thunkTable)
   thunkTable->dumpTo(fe, TR::IO::Stdout);
   */

   if (options && options->getOption(TR_VerboseInlineProfiling))
      {
      j9tty_printf(PORTLIB, "Inlining statistics:\n");
      j9tty_printf(PORTLIB, "\tFailed to devirtualize virtual calls:    %10d\n", TR::Options::INLINE_failedToDevirtualize);
      j9tty_printf(PORTLIB, "\tFailed to devirtualize interface calls:  %10d\n", TR::Options::INLINE_failedToDevirtualizeInterface);
      j9tty_printf(PORTLIB, "\tCallee method is too big:                %10d\n", TR::Options::INLINE_calleeToBig);
      j9tty_printf(PORTLIB, "\tCallee method is too deep:               %10d\n", TR::Options::INLINE_calleeToDeep);
      j9tty_printf(PORTLIB, "\tCallee method has too many nodes:        %10d\n", TR::Options::INLINE_calleeHasTooManyNodes);
      j9tty_printf(PORTLIB, "\tRan out of inlining budget:              %10d\n\n", TR::Options::INLINE_ranOutOfBudget);

      if (TR::Options::INLINE_calleeToBig)
         j9tty_printf(PORTLIB, "\tCallee method is too big (avg):          %10d\n", TR::Options::INLINE_calleeToBigSum / TR::Options::INLINE_calleeToBig);
      else
         j9tty_printf(PORTLIB, "\tCallee method is too big (avg):          x\n");
      if (TR::Options::INLINE_calleeToDeep)
         j9tty_printf(PORTLIB, "\tCallee method is too deep (avg):         %10d\n", TR::Options::INLINE_calleeToDeepSum / TR::Options::INLINE_calleeToDeep);
      else
         j9tty_printf(PORTLIB, "\tCallee method is too deep (avg):         x\n");

      if (TR::Options::INLINE_calleeHasTooManyNodes)
         j9tty_printf(PORTLIB, "\tCallee method has too many nodes (avg):  %10d\n", TR::Options::INLINE_calleeHasTooManyNodesSum / TR::Options::INLINE_calleeHasTooManyNodes);
      else
         j9tty_printf(PORTLIB, "\tCallee method has too many nodes (avg):  x\n");

      j9tty_printf(PORTLIB, "\tHas no profiling info:                   %10d\n", TR_IProfiler::_STATS_noProfilingInfo);
      j9tty_printf(PORTLIB, "\tHas weak profiling info:                 %10d\n", TR_IProfiler::_STATS_weakProfilingRatio);
      j9tty_printf(PORTLIB, "\tDoesn't want to give profiling info:     %10d\n", TR_IProfiler::_STATS_doesNotWantToGiveProfilingInfo);
      j9tty_printf(PORTLIB, "\tNo prof. info cause cannot get classInfo:%10d\n", TR_IProfiler::_STATS_cannotGetClassInfo);
      j9tty_printf(PORTLIB, "\tNo prof. info because timestamp expired: %10d\n", TR_IProfiler::_STATS_timestampHasExpired);
      }

#if defined(J9VM_OPT_JITSERVER)
   static char * isPrintJITServerMsgStats = feGetEnv("TR_PrintJITServerMsgStats");
   if (isPrintJITServerMsgStats)
      JITServerHelpers::printJITServerMsgStats(jitConfig, compInfo);
   static char * isPrintJITServerCHTableStats = feGetEnv("TR_PrintJITServerCHTableStats");
   if (isPrintJITServerCHTableStats)
      JITServerHelpers::printJITServerCHTableStats(jitConfig, compInfo);
#endif

   TRC_JIT_ShutDownEnd(vmThread, "end of JitShutdown function");
   }

static void samplingObservationsLogic(J9JITConfig * jitConfig, TR::CompilationInfo * compInfo)
   {
   if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseHeartbeat))
      {
      TR_VerboseLog::CriticalSection vlogLock;
      TR_VerboseLog::writeLine(TR_Vlog_INFO,"<samplewindow intervalTicks=%u interpretedMethodSamples=%u",
                   jitConfig->samplingTickCount + 1 - compInfo->_stats._windowStartTick, compInfo->_stats._interpretedMethodSamples);
      TR_VerboseLog::writeLine(TR_Vlog_INFO,"  compiledMethodSamples=%u compiledMethodSamplesIgnored=%u",
                   compInfo->_stats._compiledMethodSamples, compInfo->_stats._compiledMethodSamplesIgnored);
      TR_VerboseLog::writeLine(TR_Vlog_INFO,"  samplesSent=%u samplesReceived=%u ticksInIdleMode=%u",
                   compInfo->_stats._sampleMessagesSent, compInfo->_stats._sampleMessagesReceived, compInfo->_stats._ticksInIdleMode);
      TR_VerboseLog::writeLine(TR_Vlog_INFO,"  methodsCompiledOnCount=%u methodsReachingSampleInterval=%u",
                   compInfo->_stats._methodsCompiledOnCount, compInfo->_stats._methodsReachingSampleInterval);
      }

   // sample tick is about to be incremented for the samples we're about to take
   compInfo->_stats._windowStartTick = jitConfig->samplingTickCount + 1;
   compInfo->_stats._sampleMessagesSent = 0;
   compInfo->_stats._sampleMessagesReceived = 0;
   compInfo->_stats._ticksInIdleMode = 0;
   compInfo->_stats._interpretedMethodSamples = 0;
   compInfo->_stats._compiledMethodSamples = 0;
   compInfo->_stats._compiledMethodSamplesIgnored = 0;
   compInfo->_stats._methodsCompiledOnCount=0;
   compInfo->_stats._methodsReachingSampleInterval = 0;
   compInfo->_stats._methodsSelectedToRecompile = 0;
   compInfo->_stats._methodsSampleWindowReset = 0;
   }

char* jitStateNames[]=
   {
   "UNDEFINED",
   "IDLE     ",
   "STARTUP  ",
   "RAMPUP   ",
   "STEADY   ",
   "DEEPSTEADY",
   };

static int32_t startupPhaseId = 0;
static bool firstIdleStateAfterStartup = false;
static uint64_t timeToAllocateTrackingHT = 0xffffffffffffffff; // never
static bool lateDisclaimNeeded = true;

#define GCR_HYSTERESIS 100

static void jitStateLogic(J9JITConfig * jitConfig, TR::CompilationInfo * compInfo, uint32_t diffTime)
   {
   // We enter STARTUP too often because IDLE is not operating correctly
   static uint64_t lastTimeInJITStartupMode = 0;
   TR::PersistentInfo *persistentInfo = compInfo->getPersistentInfo();
   uint64_t crtElapsedTime = persistentInfo->getElapsedTime();

   static int32_t rampupPhaseID = 0;
   static uint32_t oldNumClassesLoaded = 0;
   J9JavaVM * javaVM = jitConfig->javaVM;

   uint32_t numClassesLoadedInInterval = (uint32_t)persistentInfo->getNumLoadedClasses() - oldNumClassesLoaded;
   uint32_t numClassesLoadedInIntervalNormalized = numClassesLoadedInInterval*1000/diffTime;
   oldNumClassesLoaded = (uint32_t)persistentInfo->getNumLoadedClasses(); // remember for next time

   uint8_t oldState = persistentInfo->getJitState();
   uint8_t newState;
   uint32_t totalSamples = compInfo->_intervalStats._compiledMethodSamples + compInfo->_intervalStats._interpretedMethodSamples;
   uint32_t totalSamplesNormalized = totalSamples*1000/diffTime;
   uint32_t samplesSentNormalized = compInfo->_intervalStats._samplesSentInInterval*1000/diffTime;
   float iSamplesRatio = totalSamples ? compInfo->_intervalStats._interpretedMethodSamples/(float)totalSamples : 0;
   uint32_t totalCompilationsNormalized = (compInfo->_intervalStats._numRecompilationsInInterval + compInfo->_intervalStats._numFirstTimeCompilationsInInterval)*1000/diffTime;

   // Read the CPU utilization as a percentage; -1 if not functional;
   // Can be greater than 100% if multiple cores
   static int32_t oldJvmCpuUtil = 10;
   int32_t avgJvmCpuUtil;
   if (compInfo->getCpuUtil()->isFunctional())
      {
      int32_t jvmCpuUtil = compInfo->getCpuUtil()->getVmCpuUsage();
      avgJvmCpuUtil = (oldJvmCpuUtil + jvmCpuUtil) >> 1;
      oldJvmCpuUtil = jvmCpuUtil;
      }
   else
      {
      avgJvmCpuUtil = -1;
      }

   if (compInfo->getSamplerState() == TR::CompilationInfo::SAMPLER_IDLE || // No need to acquire the monitor to read these
       compInfo->getSamplerState() == TR::CompilationInfo::SAMPLER_DEEPIDLE ||
       (avgJvmCpuUtil >= 0 && avgJvmCpuUtil <= 2) || // 2% or less CPU utilization puts us in idle mode
       (!persistentInfo->isClassLoadingPhase() && // classLoadPhase put us in STARTUP state
        totalCompilationsNormalized < 10 &&  // if compilations are taking place we cannot be in IDLE mode
        avgJvmCpuUtil < 10 && // if the JVM is using 10% or more CPU it cannot be idle
        compInfo->getMethodQueueSize() < TR::CompilationInfo::LARGE_QUEUE && // we cannot be in IDLE mode if the queue is large
        ((totalSamplesNormalized < 6 && samplesSentNormalized < 6) ||        // very few samples puts us in IDLE mode
         (oldState == IDLE_STATE && totalSamplesNormalized <= 15) || // while in IDLE mode the number of samples criterion becomes more lax
         (oldState != STARTUP_STATE && totalSamplesNormalized <=22 && iSamplesRatio > 0.5)
         )
        )
       )
      {
      newState = IDLE_STATE;
      }
   else if (persistentInfo->isClassLoadingPhase()) // new classe being injected into the system
      {
      newState = STARTUP_STATE;
      if (oldState != STARTUP_STATE)
         startupPhaseId++;
      lastTimeInJITStartupMode = crtElapsedTime;
      }
   // Make it easier to stay in STARTUP if we are already in startup, but harder otherwise
   else if ((oldState == STARTUP_STATE &&
              compInfo->_intervalStats._numRecompilationsInInterval*1000/diffTime < 30 && // many recompilations puts us in RAMPUP
             ((compInfo->_intervalStats._compiledMethodSamples == 0) ||
              (iSamplesRatio > 0.25) || // we could go for 0.33 when there are many samples
              (compInfo->_intervalStats._numFirstTimeCompilationsInInterval*1000/diffTime > 200) // 200 new compilations per second
              )
             ) ||
            (oldState != STARTUP_STATE && // to come back to STARTUP we need at least one class to be loaded
             (numClassesLoadedInIntervalNormalized > 0) &&
             ((compInfo->_intervalStats._compiledMethodSamples == 0) ||
              // If many interpreter samples then go to STARTUP, but only if we have enough overall samples to decide
              ((iSamplesRatio > 0.5) && (oldState != IDLE_STATE && totalSamplesNormalized > 15)) ||
              (compInfo->_intervalStats._numFirstTimeCompilationsInInterval*1000/diffTime > 250 &&
               compInfo->_intervalStats._numRecompilationsInInterval < 3)
              )
             )
            )
      // first-time-compilations/sec > threshold ==> must take into account machine speed; counts also affect this
      {
      newState = STARTUP_STATE;
      if (oldState != STARTUP_STATE)
         startupPhaseId++;
      lastTimeInJITStartupMode = crtElapsedTime;
      }
   else if ((oldState == RAMPUP_STATE &&
             ((compInfo->getMethodQueueSize() > TR::CompilationInfo::SMALL_QUEUE) ||
              (compInfo->_intervalStats._numRecompilationsInInterval*1000/diffTime >= 1) ||
              (compInfo->_intervalStats._numRecompPrevInterval*1000/diffTime >= 1) ||
              (compInfo->_intervalStats._numFirstTimeCompilationsInInterval*1000/diffTime > 15) ||
              //(compInfo->_intervalStats._interpretedMethodSamples*1000/diffTime > 10) // too many int samples ==> don't go to steady state yet
              (iSamplesRatio > 0.15) // not enough compiled samples to go to STEADY
              )
             ) ||
            (oldState != RAMPUP_STATE &&
             ((compInfo->getMethodQueueSize() >= TR::CompilationInfo::MEDIUM_LARGE_QUEUE) ||
              (compInfo->_intervalStats._numRecompilationsInInterval*1000/diffTime > 6) ||
              (oldState != STEADY_STATE && oldState != DEEPSTEADY_STATE && compInfo->_intervalStats._numRecompilationsInInterval*1000/diffTime > 2) ||
              (compInfo->_intervalStats._numFirstTimeCompilationsInInterval*1000/diffTime > 55) ||
              (oldState == STARTUP_STATE && rampupPhaseID == 0) // if we never had a rampup phase, switch now
              )
             )
            )
      // If the number of recompilations is not that high up to this point, do not hurry up to move from RAMPUP to STEADY
      // Maybe those recompilations will happen later
      {
      newState = RAMPUP_STATE;
      if (oldState != RAMPUP_STATE)
         rampupPhaseID++;
      }
   else // We will go to STEADY or DEEP_STEADY
      {
      if (oldState == DEEPSTEADY_STATE ||  // keep the DEEPSTEADY_STATE
          // after 10 minutes or STEADY_STATE switch to DEEPSTEADY_STATE
          // The 10 minutes may be shorter when multiple threads are running full speed
          // or longer when there isn't much work to be done
          ((oldState == STEADY_STATE) && ((persistentInfo->getJitTotalSampleCount() - persistentInfo->getJitSampleCountWhenActiveStateEntered()) > 60000)))
         {
         newState = DEEPSTEADY_STATE;
         }
      else
         {
         newState = STEADY_STATE;
         }
      }
   // A surge in compilations can make the transition back to STARTUP
   //t= 98186 oldState=3 newState=2 cSamples=125 iSamples= 11 comp=239 recomp=  4, Q_SZ=114

   static uint64_t lastTimeInStartupMode = 0;

   // current JPQ implementation does not use this, but it may need to be re-animated shortly so leaving commented out for now
   /*static char *disableJProfilingRecomp = feGetEnv("TR_DisableJProfilingRecomp");
   const int32_t intervalBase = 17000;
   if (disableJProfilingRecomp == NULL
       && javaVM->phase == J9VM_PHASE_NOT_STARTUP)
      {
      if (*(TR_BlockFrequencyInfo::getEnableJProfilingRecompilation()) == 0)
         {
         if ((crtElapsedTime - lastTimeInJITStartupMode) > intervalBase)
            {
            TR_BlockFrequencyInfo::enableJProfilingRecompilation();
            if (TR::Options::getCmdLineOptions()->isAnyVerboseOptionSet(TR_VerboseJitState, TR_VerboseCompileEnd, TR_VerbosePerformance))
               {
               TR_VerboseLog::writeLineLocked(TR_Vlog_INFO,"Enabling JProfiling recompilation. recompileThrehold = %d loopRecompileThreshold = %d nestedLoopRecompileThreshold = %d", TR_JProfiling::recompileThreshold, TR_JProfiling::loopRecompileThreshold, TR_JProfiling::nestedLoopRecompileThreshold);
               }
            }
         }
      else if (TR_JProfiling::recompileThreshold > 100)
         {
         bool thresholdsLowered = false;
         if ((crtElapsedTime - lastTimeInJITStartupMode) > intervalBase + 30000)
            {
            TR_JProfiling::recompileThreshold = 100;
            TR_JProfiling::loopRecompileThreshold = 10;
            TR_JProfiling::nestedLoopRecompileThreshold = 1;
            thresholdsLowered = true;
            }
         else if ((crtElapsedTime - lastTimeInJITStartupMode) > intervalBase + 20000)
            {
            TR_JProfiling::recompileThreshold = 10000;
            TR_JProfiling::loopRecompileThreshold = 1000;
            TR_JProfiling::nestedLoopRecompileThreshold = 100;
            thresholdsLowered = true;
            }
         else if ((crtElapsedTime - lastTimeInJITStartupMode) > intervalBase + 10000)
            {
            TR_JProfiling::recompileThreshold = 100000;
            TR_JProfiling::loopRecompileThreshold = 10000;
            TR_JProfiling::nestedLoopRecompileThreshold = 1000;
            thresholdsLowered = true;
            }
          if (thresholdsLowered && TR::Options::getCmdLineOptions()->isAnyVerboseOptionSet(TR_VerboseJitState, TR_VerboseCompileEnd, TR_VerbosePerformance))
             {
             TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, "Lowering JProfiling recompilation thresholds. recompileThrehold = %d loopRecompileThreshold = %d nestedLoopRecompileThreshold = %d", TR_JProfiling::recompileThreshold, TR_JProfiling::loopRecompileThreshold, TR_JProfiling::nestedLoopRecompileThreshold);
             }
         }
      }*/
   /*if (disableJProfilingRecomp == NULL
       && javaVM->phase == J9VM_PHASE_NOT_STARTUP
       && (crtElapsedTime - lastTimeInJITStartupMode) > 80000 //&& crtElapsedTime > 150000
       && *(TR_BlockFrequencyInfo::getEnableJProfilingRecompilation()) == 0)
      {
      printf("Enabling JProfiling recompilation\n");
      TR_BlockFrequencyInfo::enableJProfilingRecompilation();
      }*/


   // Enable/disable GCR counting
   if (!TR::Options::getAOTCmdLineOptions()->getOption(TR_DisableGuardedCountingRecompilations) &&
       !TR::Options::getJITCmdLineOptions()->getOption(TR_DisableGuardedCountingRecompilations))
      {
      if (!persistentInfo->_countForRecompile)// if counting is not yet enabled
         {
         bool enable = false;
         if (TR::Options::getAOTCmdLineOptions()->getOption(TR_EnableMultipleGCRPeriods) ||
             TR::Options::getJITCmdLineOptions()->getOption(TR_EnableMultipleGCRPeriods))
            {
            if (newState != STARTUP_STATE &&  // Do not enable GCR counting during JIT startup
               javaVM->phase == J9VM_PHASE_NOT_STARTUP && // Do not enable GCR counting during VM startup
               newState != DEEPSTEADY_STATE && // Do not enable GCR counting during DEEPSTEADY
               crtElapsedTime - lastTimeInJITStartupMode > TR::Options::_waitTimeToGCR &&
               // Do not enable GCR counting if we already have a large number of queued GCR requests
               compInfo->getNumGCRRequestsQueued() <= TR::Options::_GCRQueuedThresholdForCounting-GCR_HYSTERESIS)
               enable = true;
            }
         else // Old scheme
            {
            if (crtElapsedTime - lastTimeInJITStartupMode > TR::Options::_waitTimeToGCR)
               enable = true;
            }
         if (enable)
            {
            persistentInfo->_countForRecompile = 1; // flip the bit
            // write a message in the vlog
            if (TR::Options::isAnyVerboseOptionSet(TR_VerboseJitState, TR_VerbosePerformance))
               TR_VerboseLog::writeLineLocked(TR_Vlog_INFO,"t=%6u GCR enabled; GCR queued=%d", (uint32_t)crtElapsedTime, compInfo->getNumGCRRequestsQueued());
            }
         }
      else // GCR counting is already enabled; see if we need to disable it
         {
         if ((TR::Options::getAOTCmdLineOptions()->getOption(TR_EnableMultipleGCRPeriods) ||
              TR::Options::getJITCmdLineOptions()->getOption(TR_EnableMultipleGCRPeriods))
            && // stop counting if STARTUP, DEEPSTEADY or too many queued GCR requests
             (newState == STARTUP_STATE ||
              newState == DEEPSTEADY_STATE ||
              compInfo->getNumGCRRequestsQueued() > TR::Options::_GCRQueuedThresholdForCounting+GCR_HYSTERESIS)
            )
            {
            persistentInfo->_countForRecompile = 0; // disable counting
            // write a message in the vlog
            if (TR::Options::isAnyVerboseOptionSet(TR_VerboseJitState, TR_VerbosePerformance))
               TR_VerboseLog::writeLineLocked(TR_Vlog_INFO,"t=%6u GCR disabled; GCR queued=%d", (uint32_t)crtElapsedTime, compInfo->getNumGCRRequestsQueued());
            }
         }
      }

   // Enable/Disable RI Buffer processing
   if (persistentInfo->isRuntimeInstrumentationEnabled())
      {
      TR_HWProfiler *hwProfiler = compInfo->getHWProfiler();

      if (TR::Options::_hwProfilerExpirationTime != 0 &&
          crtElapsedTime > TR::Options::_hwProfilerExpirationTime)
         {
         if (!hwProfiler->isExpired())
            {
            hwProfiler->setExpired();
            hwProfiler->setProcessBufferState(-1);
            if (TR::Options::isAnyVerboseOptionSet(TR_VerboseHWProfiler, TR_VerbosePerformance))
               TR_VerboseLog::writeLineLocked(TR_Vlog_HWPROFILER, "Buffer processing is disabled because expiration time has been reached");
            }
         }
      else if (hwProfiler->getProcessBufferState() >= 0) // Buffer profiling is ON
         {
         // Should we turn it off?
         if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableDynamicRIBufferProcessing))
            {
            hwProfiler->checkAndTurnBufferProcessingOff();
            }
         else if (TR::Options::getCmdLineOptions()->getOption(TR_InhibitRIBufferProcessingDuringDeepSteady))
            {
            if (oldState != newState && newState == DEEPSTEADY_STATE)
               {
               hwProfiler->setProcessBufferState(-1);
               if (TR::Options::isAnyVerboseOptionSet(TR_VerboseHWProfiler, TR_VerbosePerformance))
                  TR_VerboseLog::writeLineLocked(TR_Vlog_HWPROFILER, "Buffer processing is disabled");
               }
            }
         }
      else // Should we turn it on?
         {
         if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableDynamicRIBufferProcessing))
            {
            // Do not turn on during startup
            if (javaVM->phase == J9VM_PHASE_NOT_STARTUP)
               hwProfiler->checkAndTurnBufferProcessingOn();
            }
         else if (TR::Options::getCmdLineOptions()->getOption(TR_InhibitRIBufferProcessingDuringDeepSteady))
            {
            if (oldState != newState && oldState == DEEPSTEADY_STATE)
               {
               hwProfiler->setProcessBufferState(TR::Options::_hwProfilerRIBufferProcessingFrequency);
               if (TR::Options::isAnyVerboseOptionSet(TR_VerboseHWProfiler, TR_VerbosePerformance))
                  TR_VerboseLog::writeLineLocked(TR_Vlog_HWPROFILER, "Buffer processing is enabled");
               }
            }
         }
      }

   // Control how much application threads will be sleeping to give
   // application threads more time on the CPU
   TR_YesNoMaybe starvation = compInfo->detectCompThreadStarvation();
   bool newStarvationStatus = (starvation == TR_yes);
   if (newStarvationStatus != compInfo->getStarvationDetected()) // did the status change?
      {
      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerbosePerformance))
         TR_VerboseLog::writeLineLocked(TR_Vlog_INFO,"t=%6u Starvation status changed to %d QWeight=%d CompCPUUtil=%d CompThreadsActive=%d",
                                          (uint32_t)crtElapsedTime, starvation, compInfo->getOverallQueueWeight(),
                                          compInfo->getTotalCompThreadCpuUtilWhenStarvationComputed(),
                                          compInfo->getNumActiveCompThreadsWhenStarvationComputed());
      compInfo->setStarvationDetected(newStarvationStatus);
      }

   if (TR::Options::getCmdLineOptions()->getOption(TR_EnableAppThreadYield))
      {
      int32_t oldSleepNano = compInfo->getAppSleepNano();
      int32_t newSleepNano = starvation != TR_yes ? 0 :compInfo->computeAppSleepNano(); // TODO should we look at JIT state as well
      if (newSleepNano != oldSleepNano)
         {
         compInfo->setAppSleepNano(newSleepNano);
         if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerbosePerformance))
            TR_VerboseLog::writeLineLocked(TR_Vlog_INFO,"t=%6u SleepTime changed from %d to %d QWeight=%d", (uint32_t)crtElapsedTime, oldSleepNano, newSleepNano, compInfo->getOverallQueueWeight());
         }
      }


#if defined(J9VM_INTERP_PROFILING_BYTECODES)
   // Turn on Iprofiler if we started with it OFF and it has never been activated before
   //
   static bool IProfilerOffSinceStartup = true;

   if (IProfilerOffSinceStartup &&
       !TR::Options::getCmdLineOptions()->getOption(TR_DisableInterpreterProfiling) &&
       TR::Options::getCmdLineOptions()->getOption(TR_NoIProfilerDuringStartupPhase) &&
       interpreterProfilingState == IPROFILING_STATE_OFF)
      {
       // Should we turn it ON?
      TR_IProfiler *iProfiler = TR_J9VMBase::get(jitConfig, 0)->getIProfiler();
      uint32_t failRate = iProfiler->getReadSampleFailureRate();
      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseJitState))
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_IPROFILER,"t=%6u IProfiler current fail rate = %u total=%u fail=%u samplesInBuffer=%u",
            (uint32_t)crtElapsedTime, failRate, iProfiler->getTotalReadSampleRequests(),
            iProfiler->getFailedReadSampleRequests(), iProfiler->numSamplesInHistoryBuffer());
         }
      if (crtElapsedTime - lastTimeInJITStartupMode > TR::Options::_waitTimeToStartIProfiler ||
         (int32_t)failRate > TR::Options::_iprofilerFailRateThreshold)
         {
         interpreterProfilingMonitoringWindow = 0;
         IProfilerOffSinceStartup = false;
         turnOnInterpreterProfiling(jitConfig->javaVM, compInfo);
         if (TR::Options::isAnyVerboseOptionSet(TR_VerboseJitState, TR_VerbosePerformance))
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_IPROFILER,"t=%6u IProfiler enabled", (uint32_t)crtElapsedTime);
            }
         }
      iProfiler->advanceEpochForHistoryBuffer();
      }
#endif // defined(J9VM_INTERP_PROFILING_BYTECODES)

   if (TR::Options::getCmdLineOptions()->getOption(TR_UseIdleTime) &&
       TR::Options::getCmdLineOptions()->getOption(TR_EarlyLPQ))
      {
      if (!compInfo->getLowPriorityCompQueue().isTrackingEnabled() && timeToAllocateTrackingHT == 0xffffffffffffffff)
         {
         uint64_t t = crtElapsedTime + TR::Options::_delayToEnableIdleCpuExploitation;
         if (TR::Options::_compilationDelayTime > 0 && (uint64_t)TR::Options::_compilationDelayTime * 1000 > t)
            t = TR::Options::_compilationDelayTime * 1000;
         timeToAllocateTrackingHT = t;
         }
      }

   // We should accelerate compilations of methods that get samples during rampup or steady state
   // The invocation count is decremented too slow
   // Set the state in the VM
   if (javaVM->phase != J9VM_PHASE_NOT_STARTUP) // once in NOT_STARTUP we cannot perform any changes
      {
      if (javaVM->phase == J9VM_PHASE_STARTUP)
         {
         // Analyze if we exited the STARTUP stage
         // Tolerate situations when we temporarily go out from STARTUP
         // Use JIT heuristics if (1) 'beginningOfStartup' hint didn't arrive yet  OR
         // (2) Both 'beginningOfStartup' and 'endOfStartup' hints arrived,
         // but we don't follow hints strictly outside startup
         if (!TR::Options::getCmdLineOptions()->getOption(TR_AssumeStartupPhaseUntilToldNotTo) ||
            (persistentInfo->getExternalStartupEndedSignal() && !TR::Options::getCmdLineOptions()->getOption(TR_UseStrictStartupHints)))
            {
            //if (newState != STARTUP_STATE && oldState != STARTUP_STATE)
            //   javaVM->internalVMFunctions->jvmPhaseChange(javaVM, J9VM_PHASE_NOT_STARTUP);
            if (newState != STARTUP_STATE)
               {
               int32_t waitTime = TR::Options::_waitTimeToExitStartupMode;
               // Double this value for zOS control region
#if defined(J9ZOS390)
               if (compInfo->isInZOSSupervisorState())
                  waitTime = waitTime * 2;
#endif
               if (crtElapsedTime - lastTimeInStartupMode > waitTime)
                  {
                  javaVM->internalVMFunctions->jvmPhaseChange(javaVM, J9VM_PHASE_NOT_STARTUP);
                  }
               }
            else
               {
               lastTimeInStartupMode = crtElapsedTime;
               }
            }
         else // The application will provide hints about startup ending
            {
            // Exit startup when the 'endOfStartup' arrives
            // The case where the 'endOfStartup' hint arrived, but don't want to follow strictly
            // is implemented above in the IF block
            if (persistentInfo->getExternalStartupEndedSignal())
               javaVM->internalVMFunctions->jvmPhaseChange(javaVM, J9VM_PHASE_NOT_STARTUP);
            }
         }
      else // javaVM->phase == J9VM_PHASE_EARLY_STARTUP
         {
         //TR_ASSERT(!TR::Options::getCmdLineOptions()->getOption(TR_AssumeStartupPhaseUntilToldNotTo), "assertion failure"); // we should be in STARTUP
         // Use JIT heuristics if (1) 'beginningOfStartup' hint didn't arrive yet  OR
         // (2) Both 'beginningOfStartup' and 'endOfStartup' hints arrived,
         // but we don't follow hints strictly outside startup
         if (!TR::Options::getCmdLineOptions()->getOption(TR_AssumeStartupPhaseUntilToldNotTo) ||
            (persistentInfo->getExternalStartupEndedSignal() && !TR::Options::getCmdLineOptions()->getOption(TR_UseStrictStartupHints)))
            {
            // Normal gracePeriod rules apply
            if (crtElapsedTime >= (uint64_t)persistentInfo->getClassLoadingPhaseGracePeriod()) // grace period has ended
               javaVM->internalVMFunctions->jvmPhaseChange(javaVM, (newState == STARTUP_STATE) ? J9VM_PHASE_STARTUP : J9VM_PHASE_NOT_STARTUP);
            }
         else
            {
            // 'beginningOfStartup' hint was seen
            // If 'endOfStartup' was not seen, move to STARTUP, otherwise, following hints strictly,
            // we have to exit STARTUP
            javaVM->internalVMFunctions->jvmPhaseChange(javaVM, !persistentInfo->getExternalStartupEndedSignal() ? J9VM_PHASE_STARTUP : J9VM_PHASE_NOT_STARTUP);
            }
         }
      if (javaVM->phase == J9VM_PHASE_NOT_STARTUP)
         {
         // We just exited the STARTUP phase
         // Print a message in the vlog
         if (TR::Options::getCmdLineOptions()->isAnyVerboseOptionSet(TR_VerboseJitState, TR_VerboseCompileEnd, TR_VerbosePerformance))
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITSTATE,"t=%6u VM changed state to NOT_STARTUP", (uint32_t)crtElapsedTime);
            }
         // Release AOT data caches to normal compilations
         TR_DataCacheManager::getManager()->startupOver();

         // Logic related to IdleCPU exploitation
         // If we are in idle mode immediately after JVM exited startup mode, set specific flag
         if (newState == IDLE_STATE)
            {
            firstIdleStateAfterStartup = true;
            }
         // If we don't hit idle state when we exit JVM startup, set the desired timestamp to allocate the tracking hashtable
         else if (TR::Options::getCmdLineOptions()->getOption(TR_UseIdleTime) &&
                  !compInfo->getLowPriorityCompQueue().isTrackingEnabled())
            {
            uint64_t t = crtElapsedTime + TR::Options::_delayToEnableIdleCpuExploitation;
            if (TR::Options::_compilationDelayTime > 0 && (uint64_t)TR::Options::_compilationDelayTime * 1000 > t)
               t = TR::Options::_compilationDelayTime * 1000;
            timeToAllocateTrackingHT = t;
            }

         // If we wanted to restrict inliner during startup, now it's the time to let the inliner go
         // Note: if we want to extend the heuristic of when Inliner should be restricted
         // we have to change the condition below
         if ((TR::Options::getCmdLineOptions()->getOption(TR_RestrictInlinerDuringStartup) ||
              TR::Options::getAOTCmdLineOptions()->getOption(TR_RestrictInlinerDuringStartup)) &&
             persistentInfo->getInlinerTemporarilyRestricted())
            {
            persistentInfo->setInlinerTemporarilyRestricted(false);
            if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerbosePerformance))
               {
               TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "t=%6u Stopped restricting the inliner", (uint32_t)crtElapsedTime);
               }
            }


         // If using lower counts, start using higher counts
         if (TR::Options::getCmdLineOptions()->getOption(TR_UseHigherMethodCountsAfterStartup) &&
             TR::Options::sharedClassCache())
            {
            if (TR::Options::getCmdLineOptions()->isAnyVerboseOptionSet(TR_VerboseJitState, TR_VerbosePerformance))
               {
               TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, "t=%6u JIT counts: %d %d %d  AOT counts: %d %d %d",
                  (uint32_t)crtElapsedTime,
                  TR::Options::getCmdLineOptions()->getInitialCount(),
                  TR::Options::getCmdLineOptions()->getInitialBCount(),
                  TR::Options::getCmdLineOptions()->getInitialMILCount(),
                  TR::Options::getAOTCmdLineOptions()->getInitialCount(),
                  TR::Options::getAOTCmdLineOptions()->getInitialBCount(),
                  TR::Options::getAOTCmdLineOptions()->getInitialMILCount());
               }
            TR::Options::getCmdLineOptions()->setInitialCount(TR_DEFAULT_INITIAL_COUNT);
            TR::Options::getCmdLineOptions()->setInitialBCount(TR_DEFAULT_INITIAL_BCOUNT);
            TR::Options::getCmdLineOptions()->setInitialMILCount(TR_DEFAULT_INITIAL_MILCOUNT);
            TR::Options::getAOTCmdLineOptions()->setInitialCount(TR_DEFAULT_INITIAL_COUNT);
            TR::Options::getAOTCmdLineOptions()->setInitialBCount(TR_DEFAULT_INITIAL_BCOUNT);
            TR::Options::getAOTCmdLineOptions()->setInitialMILCount(TR_DEFAULT_INITIAL_MILCOUNT);

            if (TR::Options::getCmdLineOptions()->isAnyVerboseOptionSet(TR_VerboseJitState, TR_VerbosePerformance))
               {
               TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "t=%6u JIT changed invocation counts to: JIT: %d %d %d  AOT: %d %d %d",
                  (uint32_t)crtElapsedTime,
                  TR::Options::getCmdLineOptions()->getInitialCount(),
                  TR::Options::getCmdLineOptions()->getInitialBCount(),
                  TR::Options::getCmdLineOptions()->getInitialMILCount(),
                  TR::Options::getAOTCmdLineOptions()->getInitialCount(),
                  TR::Options::getAOTCmdLineOptions()->getInitialBCount(),
                  TR::Options::getAOTCmdLineOptions()->getInitialMILCount());
               }
            }
         } // if (javaVM->phase == J9VM_PHASE_NOT_STARTUP)

      // May need to update the iprofilerMaxCount; this has a higher value in startup
      // mode to minimize overhead, but a higher value in throughput mode.
      // The startup mode for this variable is defined as classLoadPhase AND
      // VM->phase != NOT_STARTUP
      if (interpreterProfilingWasOnAtStartup) // if we used -Xjit:disableInterpreterProfiling don't bother
         {
         int32_t newIprofilerMaxCount = -1;
         //if (persistentInfo->isClassLoadingPhase() && javaVM->phase != J9VM_PHASE_NOT_STARTUP)
         if (javaVM->phase == J9VM_PHASE_EARLY_STARTUP || (persistentInfo->isClassLoadingPhase() && javaVM->phase == J9VM_PHASE_STARTUP))
            {
            if (compInfo->getIprofilerMaxCount() != TR::Options::_maxIprofilingCountInStartupMode)
               newIprofilerMaxCount = TR::Options::_maxIprofilingCountInStartupMode;
            }
         else
            {
            if (compInfo->getIprofilerMaxCount() != TR::Options::_maxIprofilingCount)
               newIprofilerMaxCount = TR::Options::_maxIprofilingCount;
            }
         if (newIprofilerMaxCount != -1) // needs to be updated
            {
            compInfo->setIprofilerMaxCount(newIprofilerMaxCount);

            j9thread_monitor_enter(javaVM->vmThreadListMutex);
            J9VMThread * currentThread = javaVM->mainThread;
            do {
               currentThread->maxProfilingCount = (UDATA)encodeCount(newIprofilerMaxCount);
               } while ((currentThread = currentThread->linkNext) != javaVM->mainThread);
            j9thread_monitor_exit(javaVM->vmThreadListMutex);

            if (TR::Options::isAnyVerboseOptionSet(TR_VerboseJitState, TR_VerbosePerformance))
               {
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITSTATE,"t=%6u Changing maxIProfilingCount to %d", (uint32_t)crtElapsedTime, newIprofilerMaxCount);
               }
            }
         } // if (interpreterProfilingWasOnAtStartup)
      } // if (javaVM->phase != J9VM_PHASE_NOT_STARTUP)


   if (newState != oldState) // state changed
      {
      persistentInfo->setJitState(newState);
      persistentInfo->setJitStateChangeSampleCount(persistentInfo->getJitTotalSampleCount());
      if ((oldState == IDLE_STATE || oldState == STARTUP_STATE) &&
          (newState != IDLE_STATE && newState != STARTUP_STATE))
         persistentInfo->setJitSampleCountWhenActiveStateEntered(persistentInfo->getJitTotalSampleCount());

      if (newState == STARTUP_STATE) // I have moved from some state into STARTUP
         {
         persistentInfo->setJitSampleCountWhenStartupStateEntered(persistentInfo->getJitTotalSampleCount());
         if (compInfo->getCpuUtil()->isFunctional())
            persistentInfo->setVmTotalCpuTimeWhenStartupStateEntered(compInfo->getCpuUtil()->getVmTotalCpuTime());
         }
      else
         {
         persistentInfo->setJitSampleCountWhenStartupStateExited(persistentInfo->getJitTotalSampleCount());
         if (compInfo->getCpuUtil()->isFunctional())
            persistentInfo->setVmTotalCpuTimeWhenStartupStateExited(compInfo->getCpuUtil()->getVmTotalCpuTime());
         }


      if (newState == IDLE_STATE)
         {
         static char *disableIdleRATCleanup = feGetEnv("TR_disableIdleRATCleanup");
         if (disableIdleRATCleanup == NULL)
            persistentInfo->getRuntimeAssumptionTable()->reclaimMarkedAssumptionsFromRAT(-1);
         }

      // Logic related to IdleCPU exploitation
      // If I left IDLE state we may set a desired delay to allocate some data structures
      if (oldState == IDLE_STATE)
         {
         if (firstIdleStateAfterStartup) // This flag only gets set once if we happen to be in idle state when we exit JVM startup
            {
            firstIdleStateAfterStartup = false;
            if (TR::Options::getCmdLineOptions()->getOption(TR_UseIdleTime) &&
               !compInfo->getLowPriorityCompQueue().isTrackingEnabled())
               {
               uint64_t t = crtElapsedTime + TR::Options::_delayToEnableIdleCpuExploitation;
               if (TR::Options::_compilationDelayTime > 0 && (uint64_t)TR::Options::_compilationDelayTime * 1000 > t)
                  t = TR::Options::_compilationDelayTime * 1000;
               timeToAllocateTrackingHT = t;
               }
            }
         }

      if (TR::Options::isAnyVerboseOptionSet(TR_VerboseJitState, TR_VerbosePerformance))
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITSTATE,"t=%6u JIT changed state from %s to %s cSmpl=%3u iSmpl=%3u comp=%3u recomp=%3u, Q_SZ=%3d CLP=%s jvmCPU=%d%%",
                      (uint32_t)crtElapsedTime,
                      jitStateNames[oldState], jitStateNames[newState], compInfo->_intervalStats._compiledMethodSamples,
                      compInfo->_intervalStats._interpretedMethodSamples,
                      compInfo->_intervalStats._numFirstTimeCompilationsInInterval,
                      compInfo->_intervalStats._numRecompilationsInInterval,
                      compInfo->getMethodQueueSize(),
                      persistentInfo->isClassLoadingPhase()?"ON":"OFF",
                      avgJvmCpuUtil);
         }

      // Turn on/off profiling in the jit
      if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableSamplingJProfiling))
         {
         int32_t newProfilingValue = -1;
         if ((oldState == STARTUP_STATE || oldState == IDLE_STATE) &&
            (newState == RAMPUP_STATE || newState == STEADY_STATE))
            newProfilingValue = 1;
         else if ((oldState == RAMPUP_STATE || oldState == STEADY_STATE) &&
            (newState == STARTUP_STATE || newState == IDLE_STATE))
            newProfilingValue = 0;
         if (newProfilingValue != -1)
            {
            //fprintf(stderr, "Changed profiling value to %d\n", newProfilingValue);
            j9thread_monitor_enter(javaVM->vmThreadListMutex);
            J9VMThread * currentThread = javaVM->mainThread;
            do
               {
               currentThread->debugEventData4 = newProfilingValue;
               } while ((currentThread = currentThread->linkNext) != javaVM->mainThread);
               j9thread_monitor_exit(javaVM->vmThreadListMutex);
            }
         }
      }

   if (lateDisclaimNeeded)
      {
      CpuUtilization *cpuUtil = compInfo->getCpuUtil();
      cpuUtil->updateCpuUtil(jitConfig);
      if (cpuUtil->getVmTotalCpuTime() >= persistentInfo->getLateSCCDisclaimTime())
         {
         javaVM->internalVMFunctions->jvmPhaseChange(javaVM, J9VM_PHASE_LATE_SCC_DISCLAIM);
         lateDisclaimNeeded = false;
         if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerbosePerformance))
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "t=%u JIT issuing late SCC disclaim", (uint32_t)crtElapsedTime);
            }
         }
      }

   if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseJitState))
      {
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITSTATE,"t=%6u oldState=%s newState=%s cls=%3u ssn=%u tsn=%3u cSmpl=%3u iSmpl=%3u comp=%3u recomp=%3u, Q_SZ=%3d VMSTATE=%d jvmCPU=%d%%",
                   (uint32_t)crtElapsedTime,
                   jitStateNames[oldState], jitStateNames[newState],
                   numClassesLoadedInIntervalNormalized,
                   samplesSentNormalized,
                   totalSamplesNormalized,
                   compInfo->_intervalStats._compiledMethodSamples,
                   compInfo->_intervalStats._interpretedMethodSamples,
                   compInfo->_intervalStats._numFirstTimeCompilationsInInterval,
                   compInfo->_intervalStats._numRecompilationsInInterval,
                   compInfo->getMethodQueueSize(),
                   javaVM->phase,
                   avgJvmCpuUtil);
      }
   if(TR::Options::getVerboseOption(TR_VerboseJitMemory))
      {
      TR_VerboseLog::writeLine(TR_Vlog_MEMORY, "FIXME: Report JIT memory usage\n");
      }

   // Allocate the tracking hashtable if needed
   // Note that timeToAllocateTrackingHT will be set to something useful only if TR_UseIdleTime option is set
   if (crtElapsedTime >= timeToAllocateTrackingHT)
      {
      compInfo->getLowPriorityCompQueue().startTrackingIProfiledCalls(TR::Options::_numIProfiledCallsToTriggerLowPriComp);
      timeToAllocateTrackingHT = 0xfffffffffffffff0; // never again
      }

   // Reset stats for next interval
   if (compInfo->getSamplerState() != TR::CompilationInfo::SAMPLER_DEEPIDLE &&
       compInfo->getSamplerState() != TR::CompilationInfo::SAMPLER_IDLE)
      compInfo->_intervalStats.decay();
   else
      compInfo->_intervalStats.reset(); // if we are going to sleep for seconds, do not keep any history
   }

/// Determine if CPU throttling may be enabled at this time.
/// We use this to determine if we need to bother calculating CPU usage.
bool CPUThrottleEnabled(TR::CompilationInfo *compInfo, uint64_t crtTime)
   {
   // test if feature is enabled
   if (TR::Options::_compThreadCPUEntitlement <= 0)
      return false;

   // During startup we apply throttling only if enabled
   if (!TR::Options::getCmdLineOptions()->getOption(TR_EnableCompThreadThrottlingDuringStartup) &&
      compInfo->getJITConfig()->javaVM->phase != J9VM_PHASE_NOT_STARTUP)
      return false;

   // Maybe the user wants to start throttling only after some time
   if (crtTime < (uint64_t)TR::Options::_startThrottlingTime)
      return false;

   // Maybe the user wants to stop throttling after some time
   if (TR::Options::_stopThrottlingTime != 0 && crtTime >= (uint64_t)TR::Options::_stopThrottlingTime)
      {
      if (compInfo->exceedsCompCpuEntitlement() != TR_no)
         {
         compInfo->setExceedsCompCpuEntitlement(TR_no);
         if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerbosePerformance))
            TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, "t=%6u Changed throttling value for compilation threads to NO because throttling reached expiration time", (uint32_t)crtTime);
         }
      return false;
      }
   return true;
   }

/// Sums up CPU utilization of all compilation thread and write this
/// value in the compilation info (or -1 in case of error)
static void DoCalculateOverallCompCPUUtilization(TR::CompilationInfo *compInfo, uint64_t crtTime, J9VMThread *currentThread, int32_t *cpuUtilizationValues)
   {
   // Sum up the CPU utilization of all the compilation threads
   int32_t totalCompCPUUtilization = 0;
   //TODO: Is getArrayOfCompilationInfoPerThread() called after setupCompilationThreads()
   TR::CompilationInfoPerThread * const *arrayOfCompInfoPT = compInfo->getArrayOfCompilationInfoPerThread();

   for (int32_t i = compInfo->getFirstCompThreadID(); i <= compInfo->getLastCompThreadID(); i++)
      {
      int32_t cpuArrIndex = i - compInfo->getFirstCompThreadID();
      const CpuSelfThreadUtilization& cpuUtil = arrayOfCompInfoPT[i]->getCompThreadCPU();
      if (cpuUtil.isFunctional())
         {
         // If the last interval ended more than 1.5 second ago, do not include it
         // in the calculations.
         int32_t cpuUtilValue = cpuUtil.computeThreadCpuUtilOverLastNns(1500000000);
         cpuUtilizationValues[cpuArrIndex] = cpuUtilValue; // memorize for later
         if (cpuUtilValue >= 0) // if first interval is not done, we read -1
            totalCompCPUUtilization += cpuUtilValue;
         }
      else
         {
         totalCompCPUUtilization = -1; // error
         break;
         }
      }
   compInfo->setOverallCompCpuUtilization(totalCompCPUUtilization);
   // Issue tracepoint indicating CPU usage percent (-1 on error)
   Trc_JIT_OverallCompCPU(currentThread, totalCompCPUUtilization);
   // Print the overall comp CPU utilization if the right verbose option is specified
   if (TR::Options::isAnyVerboseOptionSet(TR_VerboseCompilationThreads, TR_VerboseCompilationThreadsDetails))
      {
      TR_VerboseLog::CriticalSection vlogLock;
      TR_VerboseLog::write(TR_Vlog_INFO, "t=%6u TotalCompCpuUtil=%3d%%.", static_cast<uint32_t>(crtTime), totalCompCPUUtilization);
      TR::CompilationInfoPerThread * const *arrayOfCompInfoPT = compInfo->getArrayOfCompilationInfoPerThread();
      for (int32_t i = compInfo->getFirstCompThreadID(); i <= compInfo->getLastCompThreadID(); i++)
         {
         int32_t cpuArrIndex = i - compInfo->getFirstCompThreadID();
         const CpuSelfThreadUtilization& cpuUtil = arrayOfCompInfoPT[i]->getCompThreadCPU();
         TR_VerboseLog::write(" compThr%d:%3d%% (%2d%%, %2d%%) ", i, cpuUtilizationValues[cpuArrIndex], cpuUtil.getThreadLastCpuUtil(), cpuUtil.getThreadPrevCpuUtil());
         if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCompilationThreadsDetails))
            TR_VerboseLog::write("(%dms, %dms, lastCheckpoint=%u) ",
               static_cast<int32_t>(cpuUtil.getLastMeasurementInterval()) / 1000000,
               static_cast<int32_t>(cpuUtil.getSecondLastMeasurementInterval()) / 1000000,
               cpuUtil.getLowResolutionClockAtLastUpdate());
         }
      TR_VerboseLog::writeLine("");
      }
   }

void CalculateOverallCompCPUUtilization(TR::CompilationInfo *compInfo, uint64_t crtTime, J9VMThread *currentThread)
   {
   if (compInfo->getOverallCompCpuUtilization() >= 0) // No error so far
      {
      if (compInfo->getNumUsableCompilationThreads() < 8)
         {
         int32_t cpuUtilizationValues[7];
         DoCalculateOverallCompCPUUtilization(compInfo, crtTime, currentThread, cpuUtilizationValues);
         }
      else
         {
         PORT_ACCESS_FROM_JAVAVM(currentThread->javaVM);

         int32_t *cpuUtilizationValues = static_cast<int32_t *>(j9mem_allocate_memory(compInfo->getNumUsableCompilationThreads() * sizeof(int32_t), J9MEM_CATEGORY_JIT));
         if (cpuUtilizationValues)
            {
            DoCalculateOverallCompCPUUtilization(compInfo, crtTime, currentThread, cpuUtilizationValues);
            j9mem_free_memory(cpuUtilizationValues);
            }
         }
      }
   }

/// Sets a exceedsCompCpuEntitlement flag according to whether the CPU should be throttled
void CPUThrottleLogic(TR::CompilationInfo *compInfo, uint64_t crtTime)
   {
   const int32_t totalCompCPUUtilization = compInfo->getOverallCompCpuUtilization();
   // Decide whether we want to throttle compilation threads
   // and set the throttle flag if we exceed the valued specified by the user
   if (totalCompCPUUtilization >= 0)
      {
      TR_YesNoMaybe oldThrottleValue = compInfo->exceedsCompCpuEntitlement();
      // Implement some for of hysterisis; once in throttle mode the CPU utilization should
      // be 10 percentage points lower than the target to get out of throttling mode
      bool shouldThrottle = (oldThrottleValue != TR_no && TR::Options::_compThreadCPUEntitlement >= 15) ?
                            totalCompCPUUtilization > TR::Options::_compThreadCPUEntitlement - 10 :
                            totalCompCPUUtilization > TR::Options::_compThreadCPUEntitlement;
      // We want to avoid situations where we end up throttling and all compilation threads
      // get activated working at full capacity (until, half a second later we discover that we throttle again)
      // The solution is to go into a transient state; so from TR_yes we go into TR_maybe and from TR_maybe we go into TR_no
      compInfo->setExceedsCompCpuEntitlement(shouldThrottle ? TR_yes : oldThrottleValue == TR_yes ? TR_maybe : TR_no);
      // If the value changed we may want to print a message in the vlog
      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerbosePerformance) &&
         oldThrottleValue != compInfo->exceedsCompCpuEntitlement()) // did the value change?
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, "t=%6u Changed throttling value for compilation threads to %s because compCPUUtil=%d",
            (uint32_t)crtTime,
            compInfo->exceedsCompCpuEntitlement() == TR_yes ? "YES" : compInfo->exceedsCompCpuEntitlement() == TR_maybe ? "MAYBE" : "NO",
            totalCompCPUUtilization);
         }
      }
   else
      {
      compInfo->setExceedsCompCpuEntitlement(TR_no); // Conservative decision is not to throttle in case of error
      // TODO: add an option to force throttling no matter what the compilation thread utilization looks like
      }
   }

/// When many classes are loaded per second (like in Websphere startup)
/// we would like to decrease the initial level of compilation from warm to cold
/// The following fragment of code uses a heuristic to detect when we are
/// in a class loading phase and sets a global variable accordingly
static void classLoadPhaseLogic(J9JITConfig * jitConfig, TR::CompilationInfo * compInfo, uint32_t diffTime)
   {
   //static uint64_t oldElapsedTime = 0;
   static int32_t  oldNumLoadedClasses = 0;
   static int32_t  oldNumUserLoadedClasses = 0;
   static int32_t  numTicksCapped = 0;  // will be capped at 2
   static int32_t  classLoadRateForFirstInterval;
   static int32_t  numCLPQuiesceIntervals = 0;

   TR::PersistentInfo *persistentInfo = compInfo->getPersistentInfo();
   int32_t  prevNumLoadedClasses = oldNumLoadedClasses;
   int32_t  prevNumUserLoadedClasses = oldNumUserLoadedClasses;
   uint64_t crtElapsedTime       = persistentInfo->getElapsedTime();

   TR::Options * cmdLineOptions = TR::Options::getCmdLineOptions();

   if (cmdLineOptions->getOption(TR_ExperimentalClassLoadPhase))
      {
      // Let's experiment with a class loading phase algorithm that is less cpu and time dependent
      // If a sample occurs between a class loading preinitialize hook and a class load initialize hook then
      // we'll set the class load phase threshold to be true until 'n' more ticks have been seen,
      // where 'n' is TR::Options::_experimentalClassLoadPhaseInterval.
      //
      static int32_t classLoadPhaseCount;
      if (loadingClasses && TR::Options::_experimentalClassLoadPhaseInterval > 0)
         {
         persistentInfo->setClassLoadingPhase(true);
         classLoadPhaseCount = TR::Options::_experimentalClassLoadPhaseInterval;
         }
      else if (classLoadPhaseCount > 0)
         --classLoadPhaseCount;
      else if (persistentInfo->isClassLoadingPhase())
         persistentInfo->setClassLoadingPhase(false);

      return;
      }

   else
      {
      oldNumLoadedClasses = persistentInfo->getNumLoadedClasses();
      oldNumUserLoadedClasses = TR::Options::_numberOfUserClassesLoaded;

      int32_t loadedClasses = persistentInfo->getNumLoadedClasses() - prevNumLoadedClasses;

      // during WAS5 startup we have 300-500 classes loaded per second (uniprocessor P4/2.2Ghz)
      // caveat: this formula depends on the machine speed (also network issues)
      int classLoadRate = loadedClasses*1000/diffTime;
      if (numTicksCapped < 2)
         {
         if (numTicksCapped == 0)
            {
            // This is the first interval; memorize the classLoadRate for later
            classLoadRateForFirstInterval = classLoadRate;
            }
         else
            {
            // This is the second interval; use the classLoadRate to adjust the classLoadPhaseThreshold
            int32_t variance = TR::Options::_classLoadingPhaseVariance < 100 ? TR::Options::_classLoadingPhaseVariance : 0;
            int32_t newCLPThreshold = (int32_t)(0.01 *
                                                (TR::Options::_classLoadingPhaseThreshold * (100+variance) -
                                                 2 * TR::Options::_classLoadingRateAverage  * TR::Options::_classLoadingPhaseThreshold * variance/
                                                 (TR::Options::_classLoadingRateAverage + classLoadRateForFirstInterval)));
            // Scale down even more if the user tells us we have only a fraction of a processor
            newCLPThreshold = newCLPThreshold * TR::Options::_availableCPUPercentage / 100;

            double scalingFactor = (double)newCLPThreshold/(double)TR::Options::_classLoadingPhaseThreshold;

            int32_t newSecondaryCLPThreshold = (int32_t)(scalingFactor*TR::Options::_secondaryClassLoadingPhaseThreshold);
            TR::Options::_classLoadingPhaseThreshold = newCLPThreshold;
            TR::Options::_secondaryClassLoadingPhaseThreshold = newSecondaryCLPThreshold;

            // Scale other thresholds as well. Hmm, not sure this is the right thing to do.
            //TR::Options::_waitTimeToExitStartupMode = (int32_t) (TR::Options::_waitTimeToExitStartupMode/scalingFactor);

            if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCLP))
               {
               TR_VerboseLog::writeLineLocked(TR_Vlog_INFO,"ScalingFactor=%.2f Changed CLPTHreshold to %d secondaryCLPThreshold to %d", scalingFactor, newCLPThreshold, newSecondaryCLPThreshold);
               }

            // On some platforms, if application startup hints have NOT been used by now
            // turn off the selectiveNoServer feature
            if (TR::Options::getCmdLineOptions()->getOption(TR_TurnOffSelectiveNoOptServerIfNoStartupHint) &&
                !TR::Options::getCmdLineOptions()->getOption(TR_AssumeStartupPhaseUntilToldNotTo) &&
                !TR::Options::getCmdLineOptions()->getOption(TR_DisableSelectiveNoOptServer))
               {
               TR::Options::getCmdLineOptions()->setOption(TR_DisableSelectiveNoOptServer); // Turn this feature off
               TR::Options::getAOTCmdLineOptions()->setOption(TR_DisableSelectiveNoOptServer); // Turn this feature off
               if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerbosePerformance))
                  TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, "t=%u selectiveNoOptServer feature turned off", crtElapsedTime);
               }
            }
         numTicksCapped++;
         }
      bool classLoadPhase = false;
      // needed to figure out the first time we exit grace period
      if (crtElapsedTime >= (uint64_t)persistentInfo->getClassLoadingPhaseGracePeriod())
         {
         if (classLoadRate >= TR::Options::_classLoadingPhaseThreshold)
            {
            classLoadPhase = true;
            numCLPQuiesceIntervals = TR::Options::_numClassLoadPhaseQuiesceIntervals; // reload
            }
         else
            {
            if (numCLPQuiesceIntervals > 0)
               {
               if (classLoadRate >= TR::Options::_secondaryClassLoadingPhaseThreshold)
                  {
                  classLoadPhase = true;
                  numCLPQuiesceIntervals--;
                  }
               else
                  {
                  numCLPQuiesceIntervals = 0;
                  }
               }
            }
         }
      // If TR_AssumeStartupPhaseUntilToldNotTo is seen, then we may override previous decision
      if (TR::Options::getCmdLineOptions()->getOption(TR_AssumeStartupPhaseUntilToldNotTo))
         {
         if (TR::Options::getCmdLineOptions()->getOption(TR_UseStrictStartupHints))
            {
            // presence of the endOfStartup signal alone dictates CLP (and startup)
            classLoadPhase = !persistentInfo->getExternalStartupEndedSignal();
            }
         else
            {
            // If endOfStartup signal didn't come yet, assume CLP (and startup)
            // Otherwise, let the normal CLP algorithm work its course and decide startup
            if (!persistentInfo->getExternalStartupEndedSignal()) // endOfStartup didn't come yet
               classLoadPhase = true;
            }
         }
      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCLP))
         {
         if ((persistentInfo->isClassLoadingPhase() && !classLoadPhase) ||
             (!persistentInfo->isClassLoadingPhase() && classLoadPhase))
            {
            if (classLoadPhase)
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITSTATE,"Entering classLoadPhase");
            else
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITSTATE,"Exiting classLoadPhase");
            }
         }
      persistentInfo->setClassLoadingPhase(classLoadPhase);

      int32_t userLoadedClasses = TR::Options::_numberOfUserClassesLoaded - prevNumUserLoadedClasses;
      TR::Options::_userClassLoadingPhase = (userLoadedClasses*1024/diffTime >= TR::Options::_userClassLoadingPhaseThreshold);

      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCLP))
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_MEMORY,"diffTime %d  classes %d  userClasses %d  threshold %d  secondaryThreshold %d",
                      diffTime, loadedClasses, userLoadedClasses, TR::Options::_classLoadingPhaseThreshold,
                      TR::Options::_secondaryClassLoadingPhaseThreshold);
         }
      }
   }


#if defined(J9VM_INTERP_PROFILING_BYTECODES)
static void iProfilerActivationLogic(J9JITConfig * jitConfig, TR::CompilationInfo * compInfo)
   {
   //printf("%d\n", interpreterProfilingINTSamples);
   if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableInterpreterProfiling))
      {
      if (interpreterProfilingState == IPROFILING_STATE_OFF)
         {
         // Should we turn it ON?
         TR_J9VMBase *fej9 = (TR_J9VMBase *)(TR_J9VMBase::get(jitConfig, 0));
         TR_IProfiler *iProfiler = fej9->getIProfiler();
         TR::PersistentInfo *persistentInfo = compInfo->getPersistentInfo();
         if (iProfiler
            && iProfiler->getProfilerMemoryFootprint() < TR::Options::_iProfilerMemoryConsumptionLimit
#if defined(J9VM_OPT_JITSERVER)
            && compInfo->getPersistentInfo()->getRemoteCompilationMode() != JITServer::SERVER
#endif
            )
            {
            // Turn on if classLoadPhase became active or
            // if many interpreted samples
            if (persistentInfo->isClassLoadingPhase() ||
                interpreterProfilingINTSamples > TR::Options::_iprofilerReactivateThreshold)
               {
               // Don't turn on during first startup phase
               if (!TR::Options::getCmdLineOptions()->getOption(TR_NoIProfilerDuringStartupPhase) ||
                   jitConfig->javaVM->phase == J9VM_PHASE_NOT_STARTUP)
                  {
                  interpreterProfilingMonitoringWindow = 0;
                  turnOnInterpreterProfiling(jitConfig->javaVM, compInfo);
                  }
               }
            }
         }
      else // STATE != OFF
         {
         // Should we turn it OFF?
         if (TR::Options::getCmdLineOptions()->getOption(TR_UseOldIProfilerDeactivationLogic))
            {
            if (interpreterProfilingINTSamples > 0 && interpreterProfilingJITSamples > 0)
               {
               if (interpreterProfilingINTSamples <= TR::Options::_iprofilerReactivateThreshold &&
                   interpreterProfilingINTSamples > 0) // WHY? we already know this
                  interpreterProfilingMonitoringWindow ++;
               else // reset the window
                  interpreterProfilingMonitoringWindow = 0;

               if (interpreterProfilingMonitoringWindow > 60)
                  turnOffInterpreterProfiling (jitConfig);
               }
            }
         else
            {
            if (interpreterProfilingINTSamples > 0 || interpreterProfilingJITSamples > 0)
               {
               if (interpreterProfilingINTSamples <= TR::Options::_iprofilerReactivateThreshold &&
                   (((float)interpreterProfilingINTSamples) / ((float) (interpreterProfilingINTSamples + interpreterProfilingJITSamples))) < (((float)TR::Options::_iprofilerIntToTotalSampleRatio) / 100))
                  interpreterProfilingMonitoringWindow ++;
               else // reset the window
                  interpreterProfilingMonitoringWindow = 0;

               if (interpreterProfilingMonitoringWindow > 60)
                  turnOffInterpreterProfiling (jitConfig);
               }
            }
         }
      //printf("interpreter samples %d, jit samples %d, monitor window %d, profiler state = %d\n", interpreterProfilingINTSamples, interpreterProfilingJITSamples, interpreterProfilingMonitoringWindow, interpreterProfilingState);
      interpreterProfilingINTSamples = 0;
      interpreterProfilingJITSamples = 0;
      }
   }
#endif // J9VM_INTERP_PROFILING_BYTECODES

static void initJitGCMapCheckAsyncHook(J9JavaVM * vm, IDATA handlerKey, J9JITConfig *jitConfig)
   {
      TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig);
      compInfo->getPersistentInfo()->setGCMapCheckEventHandle(handlerKey);
   }


//int32_t samplerThreadStateFrequencies[TR::CompilationInfo::SAMPLER_LAST_STATE+1] = {0, 2, 1000, 100000, INT_MAX, INT_MAX, -1};
char* samplerThreadStateNames[TR::CompilationInfo::SAMPLER_LAST_STATE+1] =
                                 {
                                   "NOT_INITIALIZED",
                                   "DEFAULT",
                                   "IDLE",
                                   "DEEPIDLE",
                                   "SUSPENDED",
                                   "STOPPED",
                                   "INVALID",
                                 };

/// Method executed by various hooks on application thread
/// when we should take the samplerThread out of DEEPIDLE/IDLE state
/// This version does not acquire the vmThreadListMutex assuming that
/// its caller has already done that
void getOutOfIdleStatesUnlocked(TR::CompilationInfo::TR_SamplerStates expectedState, TR::CompilationInfo *compInfo, const char* reason)
   {
   if (compInfo->getSamplerState() != expectedState) // state may have changed when not checked under monitor
      return;
   J9JITConfig * jitConfig = compInfo->getJITConfig();
   TR::PersistentInfo *persistentInfo = compInfo->getPersistentInfo();
   // Time as maintained by sampling thread will be inaccurate here because
   // sampling thread has slept for a while and didn't have the chance to update time
   // Thus, let's use j9time_current_time_millis() instead
   //persistentInfo->setLastTimeSamplerThreadEnteredIdle(persistentInfo->getElapsedTime());
   PORT_ACCESS_FROM_JITCONFIG(jitConfig);
   uint64_t crtTime = j9time_current_time_millis() - persistentInfo->getStartTime();

   if (compInfo->getSamplerState() == TR::CompilationInfo::SAMPLER_DEEPIDLE)
      {
      compInfo->setSamplerState(TR::CompilationInfo::SAMPLER_IDLE);
      jitConfig->samplingFrequency = TR::Options::getSamplingFrequencyInIdleMode();
      persistentInfo->setLastTimeSamplerThreadEnteredIdle(crtTime);
      }
   else if (compInfo->getSamplerState() == TR::CompilationInfo::SAMPLER_IDLE)
      {
      J9JavaVM * vm = jitConfig->javaVM;
      J9VMRuntimeStateListener *listener = &vm->vmRuntimeStateListener;
      compInfo->setSamplerState(TR::CompilationInfo::SAMPLER_DEFAULT);
      jitConfig->samplingFrequency = TR::Options::getSamplingFrequency();
      persistentInfo->setLastTimeThreadsWereActive(crtTime); // make the sampler thread wait for another 5 sec before entering IDLE again
      uint32_t currentVMState = vm->internalVMFunctions->getVMRuntimeState(vm);

      if (currentVMState == J9VM_RUNTIME_STATE_IDLE)
         {
         if (vm->internalVMFunctions->updateVMRuntimeState(vm, J9VM_RUNTIME_STATE_ACTIVE) &&
            (TR::Options::getVerboseOption(TR_VerbosePerformance)))
            TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "t=%u\tSampling thread interrupted and changed VM state to %u", (uint32_t)crtTime, J9VM_RUNTIME_STATE_ACTIVE);
         }
      }

   // Interrupt the samplerThread
   j9thread_interrupt(jitConfig->samplerThread);
   if (TR::Options::getVerboseOption(TR_VerbosePerformance))
      {
      TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "t=%u\tSampling thread interrupted and changed state to %s and frequency to %d ms due to %s",
                                     (uint32_t)crtTime, samplerThreadStateNames[compInfo->getSamplerState()],
                                     jitConfig->samplingFrequency, reason);
      }
   }

/// Method executed by various hooks on application thread
/// when we should take the samplerThread out of DEEPIDLE state
/// Side-effect: will acquire/release vmThreadListMutex; note that we should
/// not use this version in GC hooks because a deadlock may happen
///
/// In Balanced, once a mutator thread hits AF it will (only) trigger GC, but will not act as main thread.
/// However, it is still the one that will request (and wait while the request is completed) exclusive VM access.
/// Once it acquires it will notify main GC thread (which is sleeping). Main GC wakes up and takes control
/// driving GC till completion. The mutator thread will just wait on 'control mutex' for notification back
/// from GC main thread that GC has completed. When resumed, the mutator thread will
/// release the exclusive VM access and proceed with allocation, and program execution.
/// It is main GC thread that will invoke the hooks (start/end), but it does not directly hold
/// 'VM thread list' lock. Mutator thread does it.
void getOutOfIdleStates(TR::CompilationInfo::TR_SamplerStates expectedState, TR::CompilationInfo *compInfo, const char* reason)
   {
   // First a cheap test without holding a monitor
   if (compInfo->getSamplerState() == expectedState)
      {
      J9JavaVM * vm = compInfo->getJITConfig()->javaVM;
      // Now acquire the monitor and do another test
      j9thread_monitor_enter(vm->vmThreadListMutex);
      getOutOfIdleStatesUnlocked(expectedState, compInfo, reason);
      j9thread_monitor_exit(vm->vmThreadListMutex);
      }
   }

/// This routine is executed with vm->vmThreadListMutex in hand
void samplerThreadStateLogic(TR::CompilationInfo *compInfo, TR_FrontEnd *fe, int32_t numActiveThreads)
   {
   static bool foundThreadActiveDuringIdleMode = false;
   J9JITConfig * jitConfig = compInfo->getJITConfig();
   TR::PersistentInfo *persistentInfo = compInfo->getPersistentInfo();
   uint64_t crtTime = persistentInfo->getElapsedTime();
   bool notifyVMThread = false;
   int32_t waitTimeInDeepIdleToNotifyVM = compInfo->getSamplingThreadWaitTimeInDeepIdleToNotifyVM();
   J9JavaVM * vm = jitConfig->javaVM;
   uint32_t currentVMState = vm->internalVMFunctions->getVMRuntimeState(vm);
   uint32_t newVMState = currentVMState;

   if (numActiveThreads > 0)
      persistentInfo->setLastTimeThreadsWereActive(crtTime);

   TR::CompilationInfo::TR_SamplerStates samplerState = compInfo->getSamplerState();
   TR::CompilationInfo::TR_SamplerStates newSamplerState = samplerState;

   TR_ASSERT(samplerState != TR::CompilationInfo::SAMPLER_NOT_INITIALIZED, "samplerThreadStateLogic: sampler must be initialized");
   TR_ASSERT(samplerState != TR::CompilationInfo::SAMPLER_STOPPED, "once stopped, samplerThread cannot call samplerThreadLogic again");

   // Application threads can set different states
   // 1) DisableJIT --> change state to SUSPENDED
   // 2) EnableJIT  --> may change state to DEFAULT
   // 3) Hooks      --> may change state from DEEPIDLE to IDLE


   // Option to stop the sampling thread after certain number of ticks
   //
   if (TR::Options::_samplingThreadExpirationTime >= 0 &&
       TR::Options::_samplingThreadExpirationTime*1000 < crtTime)
      {
      if (samplerState != TR::CompilationInfo::SAMPLER_SUSPENDED) // Nothing to do if sampler is already suspended
         {
         newSamplerState = TR::CompilationInfo::SAMPLER_SUSPENDED; // samplerThread will pick up the new frequency
         jitConfig->samplingFrequency = MAX_SAMPLING_FREQUENCY; // sampling frequency is signed int so set it to 2^31 - 1
         persistentInfo->setLastTimeSamplerThreadWasSuspended(crtTime);
         if (J9VM_RUNTIME_STATE_IDLE == currentVMState)// since sampler is being suspended, bring VM back to active
            newVMState = J9VM_RUNTIME_STATE_ACTIVE;
         }
      }
   else
      {
      switch (samplerState)
         {
         case TR::CompilationInfo::SAMPLER_DEFAULT:
            // We may go IDLE if no active threads for the last 5 seconds
            if (numActiveThreads == 0 &&
               crtTime - persistentInfo->getLastTimeThreadsWereActive() > TR::Options::_waitTimeToEnterIdleMode) // 5 seconds of inactivity
               {
               newSamplerState = TR::CompilationInfo::SAMPLER_IDLE;
               jitConfig->samplingFrequency = TR::Options::getSamplingFrequencyInIdleMode(); // sample every 1000 ms
               foundThreadActiveDuringIdleMode = false; // reset this flag when we enter idle mode
               persistentInfo->setLastTimeSamplerThreadEnteredIdle(crtTime);
               }
            break;

         case TR::CompilationInfo::SAMPLER_IDLE: // we may go DEFAULT or DEEP_IDLE
            if (numActiveThreads > 1 || (numActiveThreads == 1 && foundThreadActiveDuringIdleMode))
               {
               // exit idle mode
               newSamplerState = TR::CompilationInfo::SAMPLER_DEFAULT;
               jitConfig->samplingFrequency = TR::Options::getCmdLineOptions()->getSamplingFrequency();
               if (J9VM_RUNTIME_STATE_IDLE == currentVMState)
                  newVMState = J9VM_RUNTIME_STATE_ACTIVE;
               }
            else if (numActiveThreads == 0)
               {
               uint64_t timeInIdle = crtTime - persistentInfo->getLastTimeSamplerThreadEnteredIdle();
               // 50 seconds of IDLE puts us in DEEPIDLE
               int32_t waitTimeToEnterDeepIdle = TR::Options::_waitTimeToEnterDeepIdleMode;
               // However, if we came from DEEP_IDLE and we enter DEEP_IDLE again we may want to wait less
               if (compInfo->getPrevSamplerState() == TR::CompilationInfo::SAMPLER_DEEPIDLE)
                  waitTimeToEnterDeepIdle >>= 2; // 4 times less (12.5 sec)

               if (timeInIdle > waitTimeToEnterDeepIdle)
                  {
                  if (TR::Options::_samplingFrequencyInDeepIdleMode > 0) // setting frequency==0 is a way of disabling DEEPIDLE
                     {
                     // The JIT will enter DEEP_IDLE
                     //
                     // Decide whether we need to notify the VM/GC
                     // We want to ignore repeated IDLE<-->DEEP_IDLE transitions
                     if (compInfo->getPrevSamplerState() != TR::CompilationInfo::SAMPLER_DEEPIDLE)
                        {
                        persistentInfo->setLastTimeSamplerThreadEnteredDeepIdle(crtTime);
                        if (waitTimeInDeepIdleToNotifyVM == 0)
                           newVMState = J9VM_RUNTIME_STATE_IDLE;
                        }
                     else
                        {
                        if ((J9VM_RUNTIME_STATE_ACTIVE == currentVMState) &&
                           (waitTimeInDeepIdleToNotifyVM != -1) &&
                           (crtTime - persistentInfo->getLastTimeSamplerThreadEnteredDeepIdle() >= waitTimeInDeepIdleToNotifyVM))
                           newVMState = J9VM_RUNTIME_STATE_IDLE;
                        }
                     // Enter DEEPIDLE
                     newSamplerState = TR::CompilationInfo::SAMPLER_DEEPIDLE;
                     jitConfig->samplingFrequency = TR::Options::getSamplingFrequencyInDeepIdleMode();
                     }
                  else
                     {
                     if ((J9VM_RUNTIME_STATE_ACTIVE == currentVMState) &&
                         (waitTimeInDeepIdleToNotifyVM != -1) &&
                         (timeInIdle >= (waitTimeToEnterDeepIdle + waitTimeInDeepIdleToNotifyVM)))
                        newVMState = J9VM_RUNTIME_STATE_IDLE;
                     }
                  }
               foundThreadActiveDuringIdleMode = false;
               }
            else if (numActiveThreads == 1)
               {
               // This sample will postpone the moment when we can move to DEEPIDLE
               persistentInfo->setLastTimeSamplerThreadEnteredIdle(crtTime);
               // Not enough samples to takes out of idle, but maybe next time
               foundThreadActiveDuringIdleMode = true;
               }
            break;

         case TR::CompilationInfo::SAMPLER_DEEPIDLE: // we may go IDLE or directly DEFAULT
            if (numActiveThreads >= 2)
               {
               newSamplerState = TR::CompilationInfo::SAMPLER_DEFAULT;
               jitConfig->samplingFrequency = TR::Options::getCmdLineOptions()->getSamplingFrequency();
               if (J9VM_RUNTIME_STATE_IDLE == currentVMState)
                  newVMState = J9VM_RUNTIME_STATE_ACTIVE;
               }
            else if (numActiveThreads == 1)
               {
               newSamplerState = TR::CompilationInfo::SAMPLER_IDLE;
               jitConfig->samplingFrequency = TR::Options::getSamplingFrequencyInIdleMode();
               persistentInfo->setLastTimeSamplerThreadEnteredIdle(crtTime);
               foundThreadActiveDuringIdleMode = true; // another sample in idle mode will take us out of idle
               }
            else
               {
               if ((J9VM_RUNTIME_STATE_ACTIVE == currentVMState) &&
                  (waitTimeInDeepIdleToNotifyVM != -1) &&
                  (crtTime - persistentInfo->getLastTimeSamplerThreadEnteredDeepIdle() >= waitTimeInDeepIdleToNotifyVM))
                  newVMState = J9VM_RUNTIME_STATE_IDLE;
               }
            break;

         case TR::CompilationInfo::SAMPLER_SUSPENDED: // DisableJIT may request a samplerThread suspension
            newSamplerState = TR::CompilationInfo::SAMPLER_SUSPENDED; // samplerThread will pick up the new frequency
            jitConfig->samplingFrequency = MAX_SAMPLING_FREQUENCY; // sampling frequency is signed int so set it to 2^31 - 1
            persistentInfo->setLastTimeSamplerThreadWasSuspended(crtTime);
            if (J9VM_RUNTIME_STATE_IDLE == currentVMState) // since sampler is being suspended, bring VM back to active
               newVMState = J9VM_RUNTIME_STATE_ACTIVE;
            break;

         default:
            TR_ASSERT(false, "samplerThreadProc: invalid state at this point: %d\n", samplerState);
            // try correction
            compInfo->setSamplerState(TR::CompilationInfo::SAMPLER_DEFAULT);
            jitConfig->samplingFrequency = TR::Options::getCmdLineOptions()->getSamplingFrequency();
         } // end switch
      }


   // notify VM thread about state change
   if (newVMState != currentVMState)
      {
      if (vm->internalVMFunctions->updateVMRuntimeState(vm, newVMState) &&
         (TR::Options::getVerboseOption(TR_VerbosePerformance)))
         TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "t=%u\tSampling thread changed VM state to %u", (uint32_t)crtTime, newVMState);
      }
   // Check whether state has changed
   if (samplerState != newSamplerState)
      {
      compInfo->setSamplerState(newSamplerState);
      if (TR::Options::getVerboseOption(TR_VerbosePerformance))
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "t=%u\tSampling thread changed state to %s and frequency to %d ms",
            (uint32_t)crtTime, samplerThreadStateNames[compInfo->getSamplerState()], jitConfig->samplingFrequency);
         }
      }
   // FIXME:
   // Debug ext for the new fields
   }

/// Change inlining aggressiveness based on 'time' since we last entered
/// JIT startup phase. Inlining aggressiveness is a number between 100 and 0
/// with 100 meaning 'be very aggressive' and 0 meaning 'be very conservative'
/// 'time' is not wall clock time because the algorithm would be dependent on
/// machine speed/capability and load. Instead 'time' can be expressed in
/// terms of CPU cycles consumed by the JVM or number of samples taken
/// by application threads. Both are a loose measure of how much work the
/// JVM has done.
void inlinerAggressivenessLogic(TR::CompilationInfo *compInfo)
   {
    uint64_t crtAbstractTime, abstractTimeStartPoint;
    TR::PersistentInfo *persistentInfo = compInfo->getPersistentInfo();
    // Abstract time can be measured since we entered startup or since we exited startup
    // If we want the latter then we have to change getVmTotalCpuTimeWhenStartupStateEntered() to  getVmTotalCpuTimeWhenStartupStateExited()
    if (TR::Options::getCmdLineOptions()->getOption(TR_UseVmTotalCpuTimeAsAbstractTime))
       {
       if (compInfo->getCpuUtil()->isFunctional())
          {
          crtAbstractTime = compInfo->getCpuUtil()->getVmTotalCpuTime()/1000000; // convert from ns to ms
          abstractTimeStartPoint = persistentInfo->getVmTotalCpuTimeWhenStartupStateEntered()/1000000;
          }
       else // if we cannot get JVM cpu utilization, then the options to tune the algorithm will contain wrong values
          {
          // Force the usage of the other metric
          TR::Options::getCmdLineOptions()->setOption(TR_UseVmTotalCpuTimeAsAbstractTime, false);
          if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerbosePerformance))
             TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, "Changed from JVM time to JIT samples for abstract time measurement");
          // Set default values
          TR::Options::_abstractTimeGracePeriod = DEFAULT_ABSTRACT_TIME_SAMPLES_GRACE_PERIOD;
          TR::Options::_abstractTimeToReduceInliningAggressiveness = DEFAULT_ABSTRACT_TIME_SAMPLES_TO_REDUCE_INLINING_AGGRESSIVENESS;

          crtAbstractTime = persistentInfo->getJitTotalSampleCount();
          abstractTimeStartPoint = persistentInfo->getJitSampleCountWhenStartupStateEntered();
          }
       }
    else // use samples as abstract time
       {
       crtAbstractTime = persistentInfo->getJitTotalSampleCount();
       abstractTimeStartPoint = persistentInfo->getJitSampleCountWhenStartupStateEntered();
       }


    uint64_t abstractTimeElapsed = crtAbstractTime - abstractTimeStartPoint;
    int32_t inliningAggressiveness;
    if (abstractTimeElapsed <= TR::Options::_abstractTimeGracePeriod)
       {
       inliningAggressiveness = 100;
       }
    else if (abstractTimeElapsed >= TR::Options::_abstractTimeGracePeriod + TR::Options::_abstractTimeToReduceInliningAggressiveness)
       {
       inliningAggressiveness = 0;
       }
    else
       {
       inliningAggressiveness = 100 - 100 * (abstractTimeElapsed - (uint64_t)TR::Options::_abstractTimeGracePeriod) / (uint64_t)TR::Options::_abstractTimeToReduceInliningAggressiveness;
       }
    // write the computed inliningAggressiveness somewhere easily accessible
    if (inliningAggressiveness != persistentInfo->getInliningAggressiveness())
       {
       persistentInfo->setInliningAggressiveness(inliningAggressiveness);
       if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerbosePerformance))
          TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, "inliningAggressiveness changed to %d", inliningAggressiveness);
       }
    }

// Every N seconds read the CPU and if that exceeds M seconds write it into a circular buffer
// together with the number of compilations
class CompilationDensity
   {
   struct Entry
      {
      uint32_t _cpuConsumed; // in ms
      uint32_t _numComp;
      Entry() : _cpuConsumed(0), _numComp(0) {}
      };
   static const size_t BUF_SZ = 21;
   Entry _circularBuffer[BUF_SZ]; // sliding window of CPU readings and number of compilations
   int _crtBufPos;
   uint32_t _lastCpuValue;
   static const uint32_t CPU_INCREMENT = 1000 * 30; // 30 sec

public:
   CompilationDensity() : _crtBufPos(0), _lastCpuValue(0) {}

   uint64_t getCpuSpanForSlidingWindow() const { return (BUF_SZ-1) * CPU_INCREMENT; }

   bool update(TR::CompilationInfo *compInfo)
      {
      if (compInfo->getCpuUtil()->isFunctional())
         {
         uint32_t crtCpu = static_cast<uint32_t>(compInfo->getCpuUtil()->getVmTotalCpuTime()/1000000); // convert from ns to ms
         if (crtCpu >= _lastCpuValue + CPU_INCREMENT)
            {
            if (++_crtBufPos == BUF_SZ)
               _crtBufPos = 0;
            _circularBuffer[_crtBufPos]._cpuConsumed = crtCpu;
            _circularBuffer[_crtBufPos]._numComp = compInfo->getNumCompsUsedForCompDensityCalculations();
            _lastCpuValue = crtCpu;
            return true;
            }
          }
      return false;
      }

   // Computes and returns the CPU consumed by the JVM and the number
   // of compilations between the oldest and the newest entry in our circular buffer
   void getDensity(uint32_t &cpuConsumed, uint32_t &numComps) const
      {
      // Newest entry has index _crtBufPos
      // Oldest entry has index (_crtBufPos + 1)
      int oldestPos = _crtBufPos + 1;
      if (oldestPos == BUF_SZ)
         oldestPos = 0;
      cpuConsumed = _circularBuffer[_crtBufPos]._cpuConsumed - _circularBuffer[oldestPos]._cpuConsumed;
      numComps = _circularBuffer[_crtBufPos]._numComp - _circularBuffer[oldestPos]._numComp;
      }
   }; // class CompilationDensity

static int32_t J9THREAD_PROC samplerThreadProc(void * entryarg)
   {
   J9JITConfig * jitConfig = (J9JITConfig *) entryarg;
   J9JavaVM * vm           = jitConfig->javaVM;
   UDATA samplingPeriod    = std::max(static_cast<UDATA>(TR::Options::_minSamplingPeriod), jitConfig->samplingFrequency);
   uint64_t lastProcNumCheck = 0;
   bool idleMode = false;
   uint64_t lastMinuteCheck = 0; // for activities that need to be done rarely (every minute)
   // initialize the startTime and elapsedTime here
   PORT_ACCESS_FROM_JAVAVM(vm);

   TR::CompilationInfo *compInfo = TR::CompilationInfo::get(jitConfig);
   TR::PersistentInfo *persistentInfo = compInfo->getPersistentInfo();

   persistentInfo->setClassLoadingPhaseGracePeriod(2*TR::Options::_classLoadingPhaseInterval);
   persistentInfo->setStartTime(j9time_current_time_millis());
   persistentInfo->setElapsedTime(0);
   uint64_t oldSyncTime = 0; // ms
   uint64_t crtTime = 0; // cached version of the volatile obtained by persistentInfo->getElapsedTime()

   J9VMThread *samplerThread = 0;
   int rc = vm->internalVMFunctions->internalAttachCurrentThread
       (vm, &samplerThread, NULL,
        J9_PRIVATE_FLAGS_DAEMON_THREAD | J9_PRIVATE_FLAGS_NO_OBJECT |
        J9_PRIVATE_FLAGS_SYSTEM_THREAD | J9_PRIVATE_FLAGS_ATTACHED_THREAD,
        jitConfig->samplerThread);

   if (rc != JNI_OK)
      {
      // attach failed
      j9thread_monitor_enter(jitConfig->samplerMonitor);
      compInfo->setSamplingThreadLifetimeState(TR::CompilationInfo::SAMPLE_THR_FAILED_TO_ATTACH);
      j9thread_monitor_notify_all(jitConfig->samplerMonitor);
      j9thread_exit(jitConfig->samplerMonitor);
      return JNI_ERR; // not reachable
      }
   // Inform the waiting thread that attach was successful
   j9thread_monitor_enter(jitConfig->samplerMonitor);
   compInfo->setSamplerThread(samplerThread);
   compInfo->setSamplingThreadLifetimeState(TR::CompilationInfo::SAMPLE_THR_ATTACHED);
   j9thread_monitor_notify_all(jitConfig->samplerMonitor);
   j9thread_monitor_exit(jitConfig->samplerMonitor);

   // Read some stats about SCC. This code could have stayed in aboutToBootstrap,
   // but here we execute it on a separate thread and hide its overhead

   if (TR::Options::isAnyVerboseOptionSet())
      {
      OMRPORT_ACCESS_FROM_J9PORT(PORTLIB);
      char timestamp[32];
      bool incomplete;
      omrstr_ftime_ex(timestamp, sizeof(timestamp), "%b %d %H:%M:%S %Y", persistentInfo->getStartTime(), OMRSTR_FTIME_FLAG_LOCAL);
      TR_VerboseLog::CriticalSection vlogLock;
      TR_VerboseLog::writeLine(TR_Vlog_INFO, "StartTime: %s", timestamp);
      uint64_t phMemAvail = compInfo->computeAndCacheFreePhysicalMemory(incomplete);
      if (phMemAvail != OMRPORT_MEMINFO_NOT_AVAILABLE)
         TR_VerboseLog::writeLine(TR_Vlog_INFO, "Free Physical Memory: %lld MB %s", phMemAvail >> 20, incomplete?"estimated":"");
      else
         TR_VerboseLog::writeLine(TR_Vlog_INFO, "Free Physical Memory: Unavailable");
#if defined(J9VM_OPT_SHARED_CLASSES) && defined(J9VM_INTERP_AOT_RUNTIME_SUPPORT)
      // When we read the SCC data we set isWarmSCC either to Yes or No. Otherwise it remains in maybe state.
      J9SharedClassJavacoreDataDescriptor* javacoreData = compInfo->getAddrOfJavacoreData();
      if (compInfo->isWarmSCC() != TR_maybe && TR::Options::getVerboseOption(TR_VerbosePerformance))
         {
         TR_VerboseLog::writeLine(TR_Vlog_INFO,"Shared Class Cache Information:");
         TR_VerboseLog::writeLine(TR_Vlog_INFO,"\tSCCname:%s   SCCpath:%s", javacoreData->cacheName, javacoreData->cacheDir);
         TR_VerboseLog::writeLine(TR_Vlog_INFO,"\tSCC_stats_bytes: size=%u free=%u ROMClass=%u AOTCode=%u AOTData=%u JITHint=%u JITProfile=%u",
            javacoreData->cacheSize,
            javacoreData->freeBytes,
            javacoreData->romClassBytes,
            javacoreData->aotBytes,
            javacoreData->aotDataBytes,
            javacoreData->jitHintDataBytes,
            javacoreData->jitProfileDataBytes);
         TR_VerboseLog::writeLine(TR_Vlog_INFO,"\tSCC_stats_#:     ROMClasses=%u AOTMethods=%u AOTDataEntries=%u AOTHints=%u AOTJitProfiles=%u",
            javacoreData->numROMClasses,
            javacoreData->numAOTMethods,
            javacoreData->numAotDataEntries,
            javacoreData->numJitHints,
            javacoreData->numJitProfiles);
#if defined(OMR_GC_COMPRESSED_POINTERS)
         const TR_AOTHeader *hdrInCache = TR_SharedCacheRelocationRuntime::getStoredAOTHeaderWithConfig(vm->sharedClassConfig, samplerThread);
         if (NULL != hdrInCache)
            {
            TR_VerboseLog::writeLine(TR_Vlog_INFO, "\tAOT header compressedRefs shiftAmount=%u", hdrInCache->compressedPointerShift);
            }
#endif /* OMR_GC_COMPRESSED_POINTERS */
         }
#endif
      if (TR::Options::isAnyVerboseOptionSet(TR_VerboseHWProfiler))
         {
         TR_VerboseLog::writeLine(TR_Vlog_INFO, "Runtime Instrumentation Information:");
         if (compInfo->getPersistentInfo()->isRuntimeInstrumentationEnabled())
            {
            TR_VerboseLog::writeLine(TR_Vlog_INFO, "\tRuntime Instrumentation is Enabled");
            if (compInfo->getPersistentInfo()->isRuntimeInstrumentationRecompilationEnabled())
               {
               TR_VerboseLog::writeLine(TR_Vlog_INFO, "\tRuntime Instrumentation based Recompilation enabled");
               }
            }
         else
            {
            TR_VerboseLog::writeLine(TR_Vlog_INFO, "\tRuntime Instrumentation is not Enabled");
            }
         }

      if (compInfo->isHypervisorPresent())
         {
         J9HypervisorVendorDetails vendor;
         IDATA res =  j9hypervisor_get_hypervisor_info(&vendor);
         TR_VerboseLog::writeLine(TR_Vlog_INFO, "Running on hypervisor %s. CPU entitlement = %3.2f",
                                  res==0 ? vendor.hypervisorName : "",   compInfo->getGuestCpuEntitlement());
         }
      else
         {
         TR_VerboseLog::writeLine(TR_Vlog_INFO, "CPU entitlement = %3.2f", compInfo->getJvmCpuEntitlement());
         }
      } // if (TR::Options::isAnyVerboseOptionSet())

   if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableScorchingSampleThresholdScalingBasedOnNumProc))
      {
      // We want to become more conservative with scorching compilations for large number of
      // processors because JIT profiling is very taxing due to cache coherence issues
      // For a low number of processors (smaller than lowerBoundNumProc) we want our default
      // scorchingSampleThreshold of 240. For a large number of processors (larger than
      // upperBoundNumProc we want a small scorching threshold, say 60 (conservativeScorchingSampleThreshold)
      // For a number of processors between lowerBoundNumProc and upperBoundNumProc we want
      // to decrease the scorchingSampleThreshold linearly
      if (TR::Compiler->target.numberOfProcessors() <= TR::Options::_lowerBoundNumProcForScaling ||
          TR::Options::_scorchingSampleThreshold <= TR::Options::_conservativeScorchingSampleThreshold)
         {
         // Common case. No change in scorchingSampleThreshold
         }
      else
         {
         if (TR::Compiler->target.numberOfProcessors() >= TR::Options::_upperBoundNumProcForScaling)
            {
            // We want a smaller value for R::Options::_scorchingSampleThreshold
            TR::Options::_scorchingSampleThreshold = TR::Options::_conservativeScorchingSampleThreshold;
            }
         else
            {
            TR::Options::_scorchingSampleThreshold = TR::Options::_conservativeScorchingSampleThreshold +
               (TR::Options::_upperBoundNumProcForScaling - TR::Compiler->target.numberOfProcessors())*
               (TR::Options::_scorchingSampleThreshold - TR::Options::_conservativeScorchingSampleThreshold) /
               (TR::Options::_upperBoundNumProcForScaling - TR::Options::_lowerBoundNumProcForScaling);
            // Note that above we cannot divide by 0 due to the way the if-then-else is structured
            }
         if (TR::Options::isAnyVerboseOptionSet())
            {
            float scorchingCpuTarget = TR::Options::_scorchingSampleThreshold <= TR::Options::_sampleInterval ? 100.0 :
               100.0 * TR::Options::_sampleInterval / TR::Options::_scorchingSampleThreshold;
            TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, "scorchingSampleThreshold changed to %d (%3.1f%%)",
               TR::Options::_scorchingSampleThreshold, scorchingCpuTarget);
            }
         }
      }

   float loadFactorAdjustment = 100.0/(compInfo->getNumTargetCPUs()*TR::Options::_availableCPUPercentage);
   if (TR::Options::getCmdLineOptions()->getOption(TR_AssumeStartupPhaseUntilToldNotTo))
      {
      persistentInfo->setClassLoadingPhase(true); // start in CLP
      vm->internalVMFunctions->jvmPhaseChange(vm, J9VM_PHASE_STARTUP); // The VM state should be STARTUP
      }
#ifdef J9VM_OPT_JAVA_OFFLOAD_SUPPORT
   if (vm->javaOffloadSwitchOnWithReasonFunc != 0)
      (*vm->javaOffloadSwitchOnWithReasonFunc)(samplerThread, J9_JNI_OFFLOAD_SWITCH_JIT_SAMPLER_THREAD);
#endif

#if defined(J9VM_OPT_JITSERVER)
   CompilationDensity compDensity;
#endif /* defined(J9VM_OPT_JITSERVER) */

   TR_J9VMBase *fe = TR_J9VMBase::get(jitConfig, 0);

   j9thread_set_name(j9thread_self(), "JIT Sampler");
   while (!shutdownSamplerThread)
      {
      while (!shutdownSamplerThread && // watch for shutdown signals
             j9thread_sleep_interruptable((IDATA) samplingPeriod, 0) == 0) // Anything non-0 is an error condition so we shouldn't do the sampling //!= J9THREAD_INTERRUPTED)
         {
#if defined(J9VM_OPT_CRIU_SUPPORT)
         // Post-restore, reset the start and elapsed time. The Checkpoint
         // phase is conceptually part of building the application; therefore
         // it does not make sense to expect a user who specifies an option such
         // as -XsamplingExpirationTime  to take into account the time spent
         // executing in the Checkpoint phase.
         if (compInfo->resetStartAndElapsedTime())
            {
            if (TR::Options::isAnyVerboseOptionSet())
               TR_VerboseLog::writeLineLocked(TR_Vlog_CHECKPOINT_RESTORE, "Start and elapsed time: startTime=%6u, elapsedTime=%6u",
                                              (uint32_t)persistentInfo->getStartTime(), (uint32_t)persistentInfo->getElapsedTime());

            persistentInfo->setStartTime(j9time_current_time_millis());
            persistentInfo->setElapsedTime(0);

            if (TR::Options::isAnyVerboseOptionSet())
               TR_VerboseLog::writeLineLocked(TR_Vlog_CHECKPOINT_RESTORE, "Reset start and elapsed time: startTime=%6u, elapsedTime=%6u",
                                              (uint32_t)persistentInfo->getStartTime(), (uint32_t)persistentInfo->getElapsedTime());

            // Only reset the time once
            compInfo->setResetStartAndElapsedTime(false);
            }
#endif // #if defined(J9VM_OPT_CRIU_SUPPORT)

         J9VMThread * currentThread;

         persistentInfo->updateElapsedTime(samplingPeriod);
         crtTime += samplingPeriod;

         // periodic chores
         // FIXME: make a constant/macro for the period, and make it 100
         if (crtTime - oldSyncTime >= 100) // every 100 ms
            {
            // There is some imprecision regarding the way we maintain elapsed time
            // hence, we need to correct it periodically
            //
            crtTime = j9time_current_time_millis() - persistentInfo->getStartTime();
            persistentInfo->setElapsedTime(crtTime);
            oldSyncTime = crtTime;

            TR_DebuggingCounters::transferSmallCountsToTotalCounts();

            if (TR::Options::_compilationExpirationTime > 0 &&
                !persistentInfo->getDisableFurtherCompilation())
               {
               if (crtTime >= 1000*TR::Options::_compilationExpirationTime)
                  {
                  persistentInfo->setDisableFurtherCompilation(true) ;
                  if (fe->isLogSamplingSet())
                     {
                     TR_VerboseLog::writeLineLocked(TR_Vlog_SAMPLING,"Disable further compilation");
                     }
                  }
               }

            if (compInfo) // don't need this
               {
               // Implement the watch-dog for the compilation thread
               // We must detect situations where compilation requests get stuck for
               // a long time in the compilation queue, because the priority of the
               // compilation thread is too low
               //
               if (compInfo->dynamicThreadPriority() &&
                   compInfo->getCompilationLagUnlocked() == TR::CompilationInfo::LARGE_LAG)
                  compInfo->changeCompThreadPriority(J9THREAD_PRIORITY_MAX, 12);
               }

            int32_t heartbeatInterval = TR::Options::getSamplingHeartbeatInterval();
            if (TR::Options::getVerboseOption(TR_VerboseHeartbeat) && heartbeatInterval > 0)
               {
               compInfo->_stats._heartbeatWindowCount++;
               if (compInfo->_stats._heartbeatWindowCount == heartbeatInterval)
                  {
                  samplingObservationsLogic(jitConfig, compInfo);
                  compInfo->_stats._heartbeatWindowCount = 0;
                  }
               }

            //  Default: Every minute
            if (crtTime - lastMinuteCheck >= (TR::Options::_virtualMemoryCheckFrequencySec * 1000))
               {
               lastMinuteCheck = crtTime;
#if defined(TR_TARGET_32BIT) && (defined(WINDOWS) || defined(LINUX) || defined(J9ZOS390))
               // On 32 bit Windows, Linux, and 31 bit z/OS, monitor the virtual memory available to the user
               lowerCompilationLimitsOnLowVirtualMemory(compInfo, NULL);
#endif

#if defined(J9VM_OPT_SHARED_CLASSES) && defined(J9VM_INTERP_AOT_RUNTIME_SUPPORT)
               // Emit SCC tracepoint
               if (TR::Options::sharedClassCache() && TrcEnabled_Trc_JIT_SCCInfo &&
                  vm->sharedClassConfig && vm->sharedClassConfig->getJavacoreData)
                  {
                  J9SharedClassJavacoreDataDescriptor* scc = compInfo->getAddrOfJavacoreData();
                  memset(scc, 0, sizeof(J9SharedClassJavacoreDataDescriptor));
                  vm->sharedClassConfig->getJavacoreData(vm, scc); // need to rebuild javacore data or else it will be stale
                  Trc_JIT_SCCInfo(samplerThread, scc->cacheName, scc->cacheDir, scc->cacheSize, scc->freeBytes, scc->softMaxBytes,
                        scc->romClassBytes, scc->aotBytes, scc->aotDataBytes, scc->jitHintDataBytes, scc->jitProfileDataBytes,
                        scc->numROMClasses, scc->numAOTMethods, fe->sharedCache()->getSharedCacheDisabledReason());
                  }
#endif
               } // Default: Every minute
            } // every 100 ms

         //classLoadPhaseReanalyzed = classLoadPhaseLogic(jitConfig, compInfo);  // moved down

         // TODO: Does this need to be synchronized with the one that happens at shutdown?
         // TODO: If this has too much overhead, it can be added to the
         // "periodic chores" section so it's done less often.  I've stuck it here
         // because I'm paranoid about the 32-bit counters overflowing.
         //
         TR::DebugCounterGroup *dynamicCounters = TR::CompilationInfo::get(jitConfig)->getPersistentInfo()->getDynamicCounters();
         if (dynamicCounters)
            dynamicCounters->accumulate();
         if (  TR::Options::getCmdLineOptions()->getDebugCounterWarmupSeconds() > persistentInfo->getLastDebugCounterResetSeconds()
            && TR::Options::getCmdLineOptions()->getDebugCounterWarmupSeconds() <= crtTime/1000)
            {
            if (TR::Options::isAnyVerboseOptionSet())
               TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, "Reset debug counters at t=%dms", (int)crtTime);
            persistentInfo->setLastDebugCounterResetSeconds(crtTime/1000);
            if (dynamicCounters)
               dynamicCounters->resetAll();
            TR::DebugCounterGroup *staticCounters = TR::CompilationInfo::get(jitConfig)->getPersistentInfo()->getStaticCounters();
            if (staticCounters)
               staticCounters->resetAll();
            }

         jitConfig->samplingTickCount++;
         uint32_t numActiveThreads = 0;

         j9thread_monitor_enter(vm->vmThreadListMutex);
         currentThread = vm->mainThread;

         do {
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
            if (!currentThread->inNative)
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */
            {
               if (currentThread->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS)
                  {
                  vm->internalVMFunctions->J9SignalAsyncEvent(vm, currentThread, jitConfig->sampleInterruptHandlerKey);
                  currentThread->stackOverflowMark = (UDATA *) J9_EVENT_SOM_VALUE;
                  numActiveThreads++;
                  }
            }
         } while ((currentThread = currentThread->linkNext) != vm->mainThread);

         compInfo->_stats._sampleMessagesSent += numActiveThreads;
         compInfo->_intervalStats._samplesSentInInterval += numActiveThreads;
         compInfo->setNumAppThreadsActive(numActiveThreads, crtTime);

         if (compInfo->getSamplerState() == TR::CompilationInfo::SAMPLER_IDLE ||
             compInfo->getSamplerState() == TR::CompilationInfo::SAMPLER_DEEPIDLE)
            compInfo->_stats._ticksInIdleMode++;

         // number of processors might change
         if (crtTime > lastProcNumCheck + 300000) // every 5 mins
            {
            if (trPersistentMemory && TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseJitMemory))
               trPersistentMemory->printMemStatsToVlog();

            // time to reevaluate the number of processors
            compInfo->computeAndCacheCpuEntitlement();
            uint32_t tempProc = compInfo->getNumTargetCPUs(); // TODO is this needed?
            //compInfo->setNumTargetCPUs((tempProc = j9sysinfo_get_number_CPUs_by_type(J9PORT_CPU_TARGET)) > 0 ? tempProc : 1);


            loadFactorAdjustment = 100.0/(compInfo->getNumTargetCPUs()*TR::Options::_availableCPUPercentage);

            if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseJitMemory))
               {
               bool incomplete;
               TR_PersistentMemory *persistentMemory = compInfo->persistentMemory();
               TR_VerboseLog::CriticalSection vlogLock;
               uint64_t phMemAvail = compInfo->computeAndCacheFreePhysicalMemory(incomplete);
               if (phMemAvail != OMRPORT_MEMINFO_NOT_AVAILABLE)
                  TR_VerboseLog::writeLine(TR_Vlog_INFO, "Free Physical Memory: %lld MB %s", phMemAvail >> 20, incomplete ? "estimated" : "");
               else
                  TR_VerboseLog::writeLine(TR_Vlog_INFO, "Free Physical Memory: Unavailable");

               //TR_VerboseLog::writeLine(TR_Vlog_MEMORY,"t=%6u JIT memory usage", (uint32_t)crtTime);
               //TR_VerboseLog::writeLine(TR_Vlog_MEMORY, "FIXME: Report JIT memory usage\n");
               // Show stats on assumptions
               // assumptionTableMutex is not used, so the numbers may be a little off
               TR_VerboseLog::writeLine(TR_Vlog_MEMORY,"\tStats on assumptions:");
               TR_RuntimeAssumptionTable * rat = persistentInfo->getRuntimeAssumptionTable();
               for (int32_t i=0; i < LastAssumptionKind; i++)
                  TR_VerboseLog::writeLine(TR_Vlog_MEMORY,"\tAssumptionType=%d allocated=%d reclaimed=%d", i, rat->getAssumptionCount(i), rat->getReclaimedAssumptionCount(i));
               }
#if defined(WINDOWS) && defined(TR_TARGET_32BIT)
            if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseVMemAvailable))
               {
               J9MemoryInfo memInfo;
               if (j9sysinfo_get_memory_info(&memInfo) == 0 &&
                   memInfo.availVirtual != J9PORT_MEMINFO_NOT_AVAILABLE)
                  {
                  TR_VerboseLog::writeLineLocked(TR_Vlog_MEMORY,"t=%6u VMemAv=%u MB", (uint32_t)crtTime, (uint32_t)(memInfo.availVirtual>>20));
                  }
               }
#endif
            lastProcNumCheck = crtTime;
            }
         uint32_t loadFactor = (uint32_t) ((numActiveThreads==0?1:numActiveThreads)*loadFactorAdjustment);
         persistentInfo->setLoadFactor(loadFactor);
         // if the loadfactor is 0, set a default of 1. This will allow us to
         // temporarily stop the sampling process by setting a very high samplingFrequency
         //
         if (loadFactor == 0)
            loadFactor = 1;

         // The following may change the state of the samplerThread and the sampling frequency
         // Must be protected by vm->vmThreadListMutex
         samplerThreadStateLogic(compInfo, fe, numActiveThreads);

         UDATA samplingFreq = jitConfig->samplingFrequency;
         // TODO: Should the sampling frequency types in TR::Options be changed to more closely match the J9JITConfig types?
         samplingPeriod = std::max(static_cast<UDATA>(TR::Options::_minSamplingPeriod), (samplingFreq == MAX_SAMPLING_FREQUENCY) ? samplingFreq : samplingFreq * loadFactor);

         j9thread_monitor_exit(vm->vmThreadListMutex);

         // Sample every 10 seconds
         static uint64_t lastTimeCpuUsageCircularBufferUpdated = 0;
         if ((crtTime - lastTimeCpuUsageCircularBufferUpdated) > (TR::Options::_cpuUsageCircularBufferUpdateFrequencySec * 1000))
            {
            lastTimeCpuUsageCircularBufferUpdated = crtTime;

            if (compInfo->getCpuUtil()->isCpuUsageCircularBufferFunctional())
               compInfo->getCpuUtil()->updateCpuUsageCircularBuffer(jitConfig);
            }

         // Determine if we need to turn GCR counting off. GCR counting is known to have high overhead.
         // If more than a certain number of GCR induced compilations is queued, turn off counting.
         if (persistentInfo->_countForRecompile) // No need to check if GCR is disabled
            {
            if (TR::Options::getAOTCmdLineOptions()->getOption(TR_EnableMultipleGCRPeriods) ||
                TR::Options::getJITCmdLineOptions()->getOption(TR_EnableMultipleGCRPeriods))
               {
               if (compInfo->getNumGCRRequestsQueued() > TR::Options::_GCRQueuedThresholdForCounting + GCR_HYSTERESIS)
                  {
                  persistentInfo->_countForRecompile = 0; // disable counting
                  // write a message in the vlog
                  if (TR::Options::isAnyVerboseOptionSet(TR_VerboseJitState, TR_VerbosePerformance))
                     TR_VerboseLog::writeLineLocked(TR_Vlog_INFO,"t=%6u GCR disabled; GCR queued=%d", (uint32_t)crtTime, compInfo->getNumGCRRequestsQueued());
                  }
               }
            }

         // sample every _classLoadingPhaseInterval (i.e. 500 ms)
         static uint64_t lastTimeClassLoadPhaseAnalyzed = 0;
         uint32_t diffTime = (uint32_t)(crtTime - lastTimeClassLoadPhaseAnalyzed);

         if (diffTime >= (uint32_t)TR::Options::_classLoadingPhaseInterval) // time to re-analyze
            {
            lastTimeClassLoadPhaseAnalyzed = crtTime;

            // update the JVM cpu utilization if needed
            if (compInfo->getCpuUtil()->isFunctional())
               compInfo->getCpuUtil()->updateCpuUtil(jitConfig);

            if (CPUThrottleEnabled(compInfo, crtTime))
               {
               // Calculate CPU utilization and set throttle flag
               // This code needs to stay  before jitStateLogic because the decision to throttle
               // application threads (taken in jitStateLogic) depends on the decision to throttle
               // the compilation threads
               CalculateOverallCompCPUUtilization(compInfo, crtTime, samplerThread);
               CPUThrottleLogic(compInfo, crtTime);
               }
            // Check if we need to calculate CPU utilization for debug purposes
            else if (TR::Options::isAnyVerboseOptionSet(TR_VerboseCompilationThreads, TR_VerboseCompilationThreadsDetails) || TrcEnabled_Trc_JIT_CompCPU)
               {
               CalculateOverallCompCPUUtilization(compInfo, crtTime, samplerThread);
               }

#if defined(J9VM_OPT_JITSERVER)
            if (persistentInfo->getRemoteCompilationMode() == JITServer::CLIENT &&
                TR::Options::_lowCompDensityModeEnterThreshold > 0 && // A value of 0 disables this feature
                !TR::Options::getCmdLineOptions()->getOption(TR_FullSpeedDebug) && // FSD interferes with recompilations in LPQ
                compInfo->getJvmCpuEntitlement() <= 800) // More than 8 CPUs can consume CPU too fast
               {
               // Compute the density of compilations relative to the CPU consumed by the JVM
               // This code needs to stay after the code that updates the CPU utilization so that we can get fresh values
               if (compDensity.update(compInfo))
                  {
                  uint32_t cpuConsumed; // ms
                  uint32_t numComps;
                  compDensity.getDensity(cpuConsumed, numComps);

                  if (!compInfo->getLowCompDensityMode())
                     {
                     // If the LPQ has been drained, don't be so aggressive with its processing
                     if (compInfo->getLowPriorityCompQueue().getLowPriorityQueueSize() == 0)
                        compInfo->setCompileFromLPQRegardlessOfCPU(false);

                     // If density of compilations is lower than N every 10 minutes of CPU, we want to enter the lowCompDensityMode
                     if (numComps * compDensity.getCpuSpanForSlidingWindow() < J9::Options::_lowCompDensityModeEnterThreshold * cpuConsumed)
                        {
                        compInfo->enterLowCompDensityMode();
                        compInfo->setCompileFromLPQRegardlessOfCPU(false);
                        if (TR::Options::isAnyVerboseOptionSet(TR_VerbosePerformance, TR_VerboseJITServer))
                           {
                           TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "t=%6u Entering low compilation density mode. Will suppress most remote compilations. numComps=%u cpuConsumed=%llu ms LPQsize=%d",
                                                          (uint32_t)crtTime, numComps, cpuConsumed, compInfo->getLowPriorityCompQueue().getLowPriorityQueueSize());
                           }
                        }
                     }
                  else // Check whether we need to exit the lowCompDensityMode
                     {
                     // Exit lowCompDensityMode if
                     // (1) The density of compilations in the last 10 minutes of CPU window exceeds `J9::Options::_lowCompDensityModeExitThreshold`
                     // or
                     // (2) We have accumulated a large backlog of recompilations in the LPQ
                     if (numComps * compDensity.getCpuSpanForSlidingWindow() > J9::Options::_lowCompDensityModeExitThreshold * cpuConsumed ||
                         compInfo->getLowPriorityCompQueue().getLowPriorityQueueSize() > J9::Options::_lowCompDensityModeExitLPQSize)
                        {
                        compInfo->exitLowCompDensityMode();
                        // Typically, we take process entries from LPQ only when there is spare CPU available
                        // Ignore this rule if JITServer is available because remote compilations
                        // will not steal CPU from the running JVM
                        if (JITServerHelpers::isServerAvailable() && compInfo->getLowPriorityCompQueue().getLowPriorityQueueSize() > 0)
                           compInfo->setCompileFromLPQRegardlessOfCPU(true);
                        if (TR::Options::isAnyVerboseOptionSet(TR_VerbosePerformance, TR_VerboseJITServer))
                           {
                           TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "t=%6u Exiting low compilation density mode. Will re-enable remote compilations. numComps=%u cpuConsumed=%llu ms LPQsize=%d",
                                                          (uint32_t)crtTime, numComps, cpuConsumed, compInfo->getLowPriorityCompQueue().getLowPriorityQueueSize());
                           }
                        }
                     }
                  }
               }
#endif /* defined(J9VM_OPT_JITSERVER) */

            // Update information about global samples
            if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableDynamicSamplingWindow))
               compInfo->getJitSampleInfoRef().update(crtTime, TR::Recompilation::globalSampleCount);

            // determine whether we're in class-load phase
            classLoadPhaseLogic(jitConfig, compInfo, diffTime);
            // fprintf(stderr, "samplingPeriod=%u numProcs=%u numActiveThreads=%u samplingFrequency=%d\n", samplingPeriod, compInfo->getNumTargetCPUs(), numActiveThreads, jitConfig->samplingFrequency);

            // compute jit state
            jitStateLogic(jitConfig, compInfo, diffTime); // Update JIT state before going to sleep

            // detect high code cache occupancy
            TR::CodeCacheManager *manager = TR::CodeCacheManager::instance();
            size_t usedCacheSize = manager->getCurrTotalUsedInBytes();
            size_t threshold = manager->codeCacheConfig().highCodeCacheOccupancyThresholdInBytes();
            if ((usedCacheSize > threshold) && !highCodeCacheOccupancyThresholdReached)
               {
               highCodeCacheOccupancyThresholdReached = true;
               if (TR::Options::getCmdLineOptions()->isAnyVerboseOptionSet(TR_VerbosePerformance))
                  {
                  TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "t=%6u JIT entered high code cache occupancy state", (uint32_t)crtTime);
                  }
               }
            else if ((usedCacheSize <= threshold) && highCodeCacheOccupancyThresholdReached)
               {
               highCodeCacheOccupancyThresholdReached = false;
               if (TR::Options::getCmdLineOptions()->isAnyVerboseOptionSet(TR_VerbosePerformance))
                  {
                  TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "t=%6u JIT exited high code cache occupancy state", (uint32_t)crtTime);
                  }
               }

#if defined(J9VM_INTERP_PROFILING_BYTECODES)
            // iProfilerActivationLogic must stay after classLoadPhaseLogic and jitStateLogic
            iProfilerActivationLogic(jitConfig, compInfo);
#endif // J9VM_INTERP_PROFILING_BYTECODES
            inlinerAggressivenessLogic(compInfo);
            } // if
         } // while

      // This thread has been interrupted or shutdownSamplerThread flag was set
      // Sleep just a tiny bit just in case shutdownSamplerThread is set just prior going to sleep
      //
      samplingPeriod = TR::Options::_minSamplingPeriod; // sleep 10 ms and then take new decisions
      // The elapsed time is now different
      crtTime = j9time_current_time_millis()-persistentInfo->getStartTime();
      persistentInfo->setElapsedTime(crtTime);
      oldSyncTime = crtTime;
      }

   // Sampling thread is destroyed in stage 17 (LIBRARIES_UNLOAD)
   // We better not be holding any monitors at this point
   vm->internalVMFunctions->DetachCurrentThread((JavaVM *) vm);

   j9thread_monitor_enter(vm->vmThreadListMutex);
   compInfo->setSamplerState(TR::CompilationInfo::SAMPLER_STOPPED);
   j9thread_monitor_exit(vm->vmThreadListMutex);

   j9thread_monitor_enter(jitConfig->samplerMonitor);
   // Tell the application thread that samplerThread has finished executing
   compInfo->setSamplingThreadLifetimeState(TR::CompilationInfo::SAMPLE_THR_DESTROYED);
   j9thread_monitor_notify_all(jitConfig->samplerMonitor);

#ifdef J9VM_OPT_JAVA_OFFLOAD_SUPPORT
   if (vm->javaOffloadSwitchOffNoEnvWithReasonFunc != 0)
      (*vm->javaOffloadSwitchOffNoEnvWithReasonFunc)(vm, j9thread_self(), J9_JNI_OFFLOAD_SWITCH_JIT_SAMPLER_THREAD);
#endif

   j9thread_exit(jitConfig->samplerMonitor);       /* exit the monitor and terminate the thread */

   /* NO GUARANTEED EXECUTION BEYOND THIS POINT */

   return 0;
   }


void jitHookJNINativeRegistered(J9HookInterface **hookInterface, UDATA eventNum, void *eventData, void *userData)
   {
   J9VMJNINativeRegisteredEvent *event = (J9VMJNINativeRegisteredEvent *) eventData;
   J9VMThread *vmThread   = event->currentThread;
   J9Method   *method     = event->nativeMethod;
   void       *newAddress = event->nativeMethodAddress;
   bool        somethingWasDone = false;


   // First check if a thunk has been compiled for this native
   // If so, go and patch the word in the preprologue
   J9JITConfig *jitConfig = vmThread->javaVM->jitConfig;
   if (jitConfig == 0)
      return; // if a hook gets called after freeJitConfig then not much else we can do

   TR_FrontEnd *vm = TR_J9VMBase::get(jitConfig, vmThread);
   TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig);
   getOutOfIdleStates(TR::CompilationInfo::SAMPLER_DEEPIDLE, compInfo, "JNI registered");

   uint8_t *thunkStartPC = (uint8_t *)TR::CompilationInfo::getPCIfCompiled(method);
   if (thunkStartPC)
      {
      // The address in the word immediately before the linkage info
      uintptr_t **addressSlot = (uintptr_t **)(thunkStartPC - (4 + sizeof(uintptr_t)));

      // Write the address slot
      *addressSlot = (uintptr_t *) newAddress;

      // Sync/Flush
      TR::CodeGenerator::syncCode((uint8_t*)addressSlot, sizeof(uintptr_t));

      somethingWasDone = true;
      }

      // Notify the assumptions table that the native has been registered
      {
      OMR::CriticalSection registerNatives(assumptionTableMutex);
      TR_RuntimeAssumptionTable *rat = compInfo->getPersistentInfo()->getRuntimeAssumptionTable();
      OMR::RuntimeAssumption **headPtr = rat->getBucketPtr(RuntimeAssumptionOnRegisterNative, TR_RuntimeAssumptionTable::hashCode((uintptr_t)method));
      TR_PatchJNICallSite *cursor = (TR_PatchJNICallSite *)(*headPtr);
      while (cursor)
         {
         if (cursor->matches((uintptr_t)method))
            {
            cursor->compensate(vm, 0, newAddress);
            }
         cursor = (TR_PatchJNICallSite*)cursor->getNext();
         }
      }

   if (somethingWasDone)
      compInfo->setAllCompilationsShouldBeInterrupted();
   }


static UDATA jitReleaseCodeStackWalkFrame(J9VMThread *vmThread, J9StackWalkState *walkState)
   {
   J9JITConfig *jitConfig = vmThread->javaVM->jitConfig;
   J9JITExceptionTable *metaData = walkState->jitInfo;

   if (metaData)
      {
      int32_t numBlocks     = 0;
      int32_t numLiveBlocks = 0;
      for (OMR::FaintCacheBlock *cursor = (OMR::FaintCacheBlock *)jitConfig->methodsToDelete;
           cursor; cursor = cursor->_next)
         {
         if (metaData == cursor->_metaData)
            {
            cursor->_isStillLive = true;
            //fprintf(stderr, "  --ccr-- still live %p\n", cursor->metaData->startPC);
            }
         if (cursor->_isStillLive)
            numLiveBlocks++;
         numBlocks++;
         }

      // All the remaining blocks are still live - there is nothing
      // to collect.  Abort the rest of the stack walk now.
      //
      if (numBlocks == numLiveBlocks)
         return J9_STACKWALK_STOP_ITERATING;
      }

   return J9_STACKWALK_KEEP_ITERATING;
   }

#if JAVA_SPEC_VERSION >= 19
static void jitWalkContinuationStackFrames(J9HookInterface **hookInterface, UDATA eventNum, void *eventData, void *userData)
{
	MM_ContinuationIteratingEvent *continuationIteratingEvent = (MM_ContinuationIteratingEvent *)eventData;
	J9VMThread * vmThread = continuationIteratingEvent->vmThread;
	J9InternalVMFunctions *vmFuncs = vmThread->javaVM->internalVMFunctions;
	J9VMContinuation *continuation = J9VMJDKINTERNALVMCONTINUATION_VMREF(vmThread, continuationIteratingEvent->object);
	if (NULL != continuation) {
		J9StackWalkState walkState;
		walkState.flags     = J9_STACKWALK_ITERATE_HIDDEN_JIT_FRAMES | J9_STACKWALK_ITERATE_FRAMES | J9_STACKWALK_SKIP_INLINES;
		walkState.skipCount = 0;
		walkState.frameWalkFunction = jitReleaseCodeStackWalkFrame;

		vmFuncs->walkContinuationStackFrames(vmThread, continuation, &walkState);
	}
}

static jvmtiIterationControl jitWalkContinuationCallBack(J9VMThread *vmThread, J9MM_IterateObjectDescriptor *object, void *userData)
   {
   J9InternalVMFunctions *vmFuncs = vmThread->javaVM->internalVMFunctions;
   J9VMContinuation *continuation = J9VMJDKINTERNALVMCONTINUATION_VMREF(vmThread, object->object);
   if (NULL != continuation)
      {
      bool yieldHappened = false;
      if ((continuation->dropFlags & 0x1) ? false : true)
         {
         J9StackWalkState walkState;
         walkState.flags     = J9_STACKWALK_ITERATE_HIDDEN_JIT_FRAMES | J9_STACKWALK_ITERATE_FRAMES | J9_STACKWALK_SKIP_INLINES;
         walkState.skipCount = 0;
         walkState.frameWalkFunction = jitReleaseCodeStackWalkFrame;
         vmFuncs->walkContinuationStackFrames(vmThread, continuation, &walkState);
         continuation->dropFlags = 0x1;
         condYieldFromGCFunctionPtr condYield = (condYieldFromGCFunctionPtr)userData;
         yieldHappened = condYield(vmThread->omrVMThread, J9_GC_METRONOME_UTILIZATION_COMPONENT_JIT);
         }

      if (yieldHappened)
         {
         /* Stop the iteration, will resume later. */
         return JVMTI_ITERATION_ABORT;
         }
      }
   return JVMTI_ITERATION_CONTINUE;
   }

static jvmtiIterationControl jitResetContinuationFlag(J9VMThread *vmThread, J9MM_IterateObjectDescriptor *object, void *userData)
   {
   J9VMContinuation *continuation = J9VMJDKINTERNALVMCONTINUATION_VMREF(vmThread, object->object);
   if (NULL != continuation)
      {
      continuation->dropFlags = 0;
      }

   return JVMTI_ITERATION_CONTINUE;
   }
#endif /* JAVA_SPEC_VERSION >= 19 */

static void jitReleaseCodeStackWalk(OMR_VMThread *omrVMThread, condYieldFromGCFunctionPtr condYield = NULL)
   {
   J9VMThread *vmThread = (J9VMThread *)omrVMThread->_language_vmthread;
   J9JITConfig *jitConfig = vmThread->javaVM->jitConfig;
   if (!jitConfig)
      return; // not much we can do if the hook is called after freeJitConfig

   if (!jitConfig->methodsToDelete)
      return; // nothing to do

   bool isRealTimeGC = TR::Options::getCmdLineOptions()->realTimeGC();
#if JAVA_SPEC_VERSION >= 19
   J9JavaVM *vm = vmThread->javaVM;
   J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
   PORT_ACCESS_FROM_VMC(vmThread);

   if (isRealTimeGC && !TR::Options::getCmdLineOptions()->getOption(TR_DisableIncrementalCCR))
      {
      bool yieldHappened = false;
      do
         {
         J9VMThread *thread = vmThread;
         yieldHappened = false;
         do
            {
            if ((thread->dropFlags & 0x1) ? false : true)
               {
               J9StackWalkState walkState;
               walkState.flags     = J9_STACKWALK_ITERATE_HIDDEN_JIT_FRAMES | J9_STACKWALK_ITERATE_FRAMES | J9_STACKWALK_SKIP_INLINES;
               walkState.skipCount = 0;
               walkState.frameWalkFunction = jitReleaseCodeStackWalkFrame;
               walkState.walkThread = thread;
               vm->walkStackFrames(vmThread, &walkState);
               thread->dropFlags |= 0x1;
               }

            if ((NULL!= thread->currentContinuation) && ((thread->currentContinuation->dropFlags & 0x1) ? false : true))
               /* If a continuation is mounted, always walk the continuation as that represent the CarrierThread */
               {
                  J9StackWalkState walkState;
                  walkState.flags     = J9_STACKWALK_ITERATE_HIDDEN_JIT_FRAMES | J9_STACKWALK_ITERATE_FRAMES | J9_STACKWALK_SKIP_INLINES;
                  walkState.skipCount = 0;
                  walkState.frameWalkFunction = jitReleaseCodeStackWalkFrame;
                  vmFuncs->walkContinuationStackFrames(vmThread, thread->currentContinuation, &walkState);
                  thread->currentContinuation->dropFlags |= 0x1;
               }

            yieldHappened = condYield(omrVMThread, J9_GC_METRONOME_UTILIZATION_COMPONENT_JIT);

            if (!yieldHappened)
               thread = thread->linkNext;
            }
         while ((thread != vmThread) && !yieldHappened);
         }
      while (yieldHappened);

      do
         {
         vm->memoryManagerFunctions->j9gc_flush_nonAllocationCaches_for_walk(vm);
         jvmtiIterationControl rc = vm->memoryManagerFunctions->j9mm_iterate_all_continuation_objects(vmThread, PORTLIB, 0, jitWalkContinuationCallBack, (void*)condYield);
         if (JVMTI_ITERATION_ABORT == rc)
            yieldHappened = true;
         }
      while (yieldHappened);
   } else {
      J9StackWalkState walkState;
      walkState.flags     = J9_STACKWALK_ITERATE_HIDDEN_JIT_FRAMES | J9_STACKWALK_ITERATE_FRAMES | J9_STACKWALK_SKIP_INLINES;
      walkState.skipCount = 0;
      walkState.frameWalkFunction = jitReleaseCodeStackWalkFrame;

      vmFuncs->walkAllStackFrames(vmThread, &walkState);
   }
#else /* JAVA_SPEC_VERSION >= 19 */
   bool yieldHappened = false;
   bool doStackWalkForThread = true;
   do
      {
      J9VMThread *thread = vmThread;
      yieldHappened = false;
      do
         {
         if (isRealTimeGC && !TR::Options::getCmdLineOptions()->getOption(TR_DisableIncrementalCCR))
            doStackWalkForThread = (thread->dropFlags & 0x1) ? false : true;

         if (doStackWalkForThread)
            {
            J9StackWalkState walkState;
            walkState.flags     = J9_STACKWALK_ITERATE_HIDDEN_JIT_FRAMES | J9_STACKWALK_ITERATE_FRAMES | J9_STACKWALK_SKIP_INLINES;
            walkState.skipCount = 0;
            walkState.frameWalkFunction = jitReleaseCodeStackWalkFrame;
            walkState.walkThread = thread;
            vmThread->javaVM->walkStackFrames(vmThread, &walkState);

            if (isRealTimeGC && !TR::Options::getCmdLineOptions()->getOption(TR_DisableIncrementalCCR))
               {
               thread->dropFlags |= 0x1;
               yieldHappened = condYield(omrVMThread, J9_GC_METRONOME_UTILIZATION_COMPONENT_JIT);
               }
            }

         if (!yieldHappened)
            thread = thread->linkNext;
         }
      while ((thread != vmThread) && !yieldHappened);
      }
   while (yieldHappened);
#endif /* JAVA_SPEC_VERSION >= 19 */


   TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig);
   TR_RuntimeAssumptionTable * rat = compInfo->getPersistentInfo()->getRuntimeAssumptionTable();
   // Now walk all the faint blocks, and collect the ones that are not still live
   //
   OMR::FaintCacheBlock *cursor = (OMR::FaintCacheBlock *)jitConfig->methodsToDelete;
   OMR::FaintCacheBlock *prev = 0;

   uintptr_t rangeStartPC = 0;
   uintptr_t rangeEndPC = 0;
   uintptr_t rangeColdStartPC = 0;
   uintptr_t rangeColdEndPC = 0;

   uintptr_t rangeStartMD = 0;
   uintptr_t rangeEndMD = 0;

   bool firstRange = true;
   bool coldRangeUninitialized = true;

   bool hasMethodOverrideAssumptions = false;
   bool hasClassExtendAssumptions = false;
   bool hasClassUnloadAssumptions = false;
   bool hasClassRedefinitionAssumptions = false;

   cursor = (OMR::FaintCacheBlock *)jitConfig->methodsToDelete;
   prev = 0;
   int32_t condYieldCounter = 0;

   // cmvc 192753
   // It is not safe to exit this function until all faint records have been processed.
   // If we do the following can happen: (1) method A get compiled and recompiled creating a faint record
   // (2) Method A gets unloaded and both bodies get reclaimed. If the faint block for A is not
   // processed before the end of the GC cycle, a compilation for method B can reuse the exact
   // same spot where method A used to be. When we finally process the faint record for A we
   // will reclaim space belonging to B
   while (cursor/*&& (condYieldCounter < 2)*/)
      {
      if (!cursor->_isStillLive)
         {
         J9JITExceptionTable *metaData = cursor->_metaData;

         // Remove From List
         if (prev)
            prev->_next = cursor->_next;
         else
            jitConfig->methodsToDelete = (void*) cursor->_next;

         // Free storage for cursor and the code cache - CAREFUL!! iterating on cursor
         //
         OMR::FaintCacheBlock *next = cursor->_next;
         jitReleaseCodeCollectMetaData(jitConfig, vmThread, metaData, cursor);
         cursor = next;
         if (isRealTimeGC && !TR::Options::getCmdLineOptions()->getOption(TR_DisableIncrementalCCR))
            condYieldCounter += condYield(omrVMThread, J9_GC_METRONOME_UTILIZATION_COMPONENT_JIT);

         continue;
         }

      prev   = cursor;
      cursor = cursor->_next;
      }

   /*
    * After we have determined what can be reclaimed for the current pass,
    * we must clear the _isStillLive flag or we will never again consider
    * the faint blocks as candidates for reclaimation.
    */
   for (cursor = (OMR::FaintCacheBlock *)jitConfig->methodsToDelete; cursor; cursor = cursor->_next)
      {
      cursor->_isStillLive = false;
      }

   if (isRealTimeGC && !TR::Options::getCmdLineOptions()->getOption(TR_DisableIncrementalCCR))
      { //clear flags
      J9VMThread *thr = vmThread;
      do
         {
         thr->dropFlags &=0x0;
         thr = thr->linkNext;
         }
      while (thr != vmThread);
#if JAVA_SPEC_VERSION >= 19
      vm->memoryManagerFunctions->j9mm_iterate_all_continuation_objects(vmThread, PORTLIB, 0, jitResetContinuationFlag, NULL);
#endif /* JAVA_SPEC_VERSION >= 19 */
      }

   }

static void jitHookReleaseCodeGCCycleEnd(J9HookInterface **hook, UDATA eventNum, void *eventData, void *userData)
   {
   MM_GCCycleEndEvent *event = (MM_GCCycleEndEvent *)eventData;
   OMR_VMThread *omrVMThread = event->omrVMThread;
   condYieldFromGCFunctionPtr condYield = NULL;
   if (TR::Options::getCmdLineOptions()->realTimeGC())
      condYield = event->condYieldFromGCFunction;

   jitReleaseCodeStackWalk(omrVMThread,condYield);
   jitReclaimMarkedAssumptions(true);
   }

/// setupHooks is used in ABOUT_TO_BOOTSTRAP stage (13)
int32_t setUpHooks(J9JavaVM * javaVM, J9JITConfig * jitConfig, TR_FrontEnd * vm)
   {
   TR::CompilationInfo *compInfo = getCompilationInfo(jitConfig);
#if defined(J9VM_OPT_JITSERVER)
   if (compInfo->getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER)
      {
      javaVM->internalVMFunctions->jvmPhaseChange(javaVM, J9VM_PHASE_NOT_STARTUP);
      return 0;
      }
#endif

   TR_J9VMBase *vmj9 = (TR_J9VMBase *)vm;

   J9HookInterface * * vmHooks = javaVM->internalVMFunctions->getVMHookInterface(javaVM);
   J9HookInterface * * gcHooks = javaVM->memoryManagerFunctions->j9gc_get_hook_interface(javaVM);
   J9HookInterface * * gcOmrHooks = javaVM->memoryManagerFunctions->j9gc_get_omr_hook_interface(javaVM->omrVM);

   PORT_ACCESS_FROM_JAVAVM(javaVM);

   if (TR::Options::getCmdLineOptions()->getOption(TR_noJitDuringBootstrap) ||
       TR::Options::getCmdLineOptions()->getOption(TR_noJitUntilMain) ||
       TR::Options::getCmdLineOptions()->getOption(TR_jitAllAtMain))
      {
      jitConfig->runtimeFlags |= J9JIT_DEFER_JIT;

      if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_LOOKUP_JNI_ID, jitHookAboutToRunMain, OMR_GET_CALLSITE(), NULL))
         {
         j9tty_printf(PORTLIB, "Error: Unable to install J9HOOK_VM_LOOKUP_JNI_ID hook\n");
         return -1;
         }
      }
   else
      {
#ifdef J9VM_JIT_SUPPORTS_DIRECT_JNI
      initializeDirectJNI(javaVM);
#endif
      }

   // sync the two places for sampling frequency
   jitConfig->samplingFrequency = TR::Options::getSamplingFrequency();

   if(TR::Options::getCmdLineOptions()->getOption(TR_RTGCMapCheck))
      initJitGCMapCheckAsyncHook(javaVM, javaVM->internalVMFunctions->J9RegisterAsyncEvent(javaVM, jitGCMapCheck, NULL), jitConfig);

   jitConfig->samplerMonitor = NULL; // initialize this field just in case
   compInfo->setSamplingThreadLifetimeState(TR::CompilationInfo::SAMPLE_THR_NOT_CREATED); // just in case
   if (jitConfig->samplingFrequency
      && !vmj9->isAOT_DEPRECATED_DO_NOT_USE()
#if defined(J9VM_OPT_JITSERVER)
      && compInfo->getPersistentInfo()->getRemoteCompilationMode() != JITServer::SERVER
#endif
      )
      {
      if ((jitConfig->sampleInterruptHandlerKey = javaVM->internalVMFunctions->J9RegisterAsyncEvent(javaVM, jitMethodSampleInterrupt, NULL)) < 0)
         {
         j9tty_printf(PORTLIB, "Error: Unable to install method sample handler\n");
         return -1;
         }

      j9thread_monitor_init_with_name(&(jitConfig->samplerMonitor), 0, "JIT sampling thread");

      if (jitConfig->samplerMonitor)
         {
         UDATA priority;

         priority = J9THREAD_PRIORITY_MAX;

         compInfo->setSamplingThreadWaitTimeInDeepIdleToNotifyVM();
         // Frequency has been set above; now set the state because when the sampler
         // thread is started it must see the new state
         // If the sampler thread cannot be started we must change the state back
         compInfo->setSamplerState(TR::CompilationInfo::SAMPLER_DEFAULT);

         const UDATA defaultOSStackSize = javaVM->defaultOSStackSize; //256KB stack size

         if(javaVM->internalVMFunctions->createThreadWithCategory(&jitConfig->samplerThread,
                                                                  defaultOSStackSize,
                                                                  priority,
                                                                  0,
                                                                  &samplerThreadProc,
                                                                  jitConfig,
                                                                  J9THREAD_CATEGORY_SYSTEM_JIT_THREAD))
            {
            // cannot create the sampling thread; will continue without
            j9thread_monitor_destroy(jitConfig->samplerMonitor);
            jitConfig->samplerMonitor = 0;
            compInfo->setSamplerState(TR::CompilationInfo::SAMPLER_NOT_INITIALIZED);
            }
         else
            {
            // wait here until the samplingThread attaches to the VM
            j9thread_monitor_enter(jitConfig->samplerMonitor);
            while (compInfo->getSamplingThreadLifetimeState() == TR::CompilationInfo::SAMPLE_THR_NOT_CREATED)
               j9thread_monitor_wait(jitConfig->samplerMonitor);
            j9thread_monitor_exit(jitConfig->samplerMonitor);

            // At this point the sampling thread either attached successfully and changed the
            // state to SAMPLE_THR_ATTACHED, or failed to attach and exited after changing
            // the state to SAMPLE_THR_FAILED_TO_ATTACH
            if (compInfo->getSamplingThreadLifetimeState() == TR::CompilationInfo::SAMPLE_THR_FAILED_TO_ATTACH) // no monitor needed for this access
               {
               j9thread_monitor_destroy(jitConfig->samplerMonitor);
               jitConfig->samplerMonitor = 0;
               jitConfig->samplerThread = 0;
               compInfo->setSamplerState(TR::CompilationInfo::SAMPLER_NOT_INITIALIZED);
               }
            }
         }

      if (!jitConfig->samplerMonitor)
         {
         j9tty_printf(PORTLIB, "\nJIT: Method sample thread failed to start -- disabling sampling.\n");
         }
      }
   // If we cannot start the sampling thread or don't want to, then enter NON_STARTUP mode directly
   if (!jitConfig->samplerMonitor)
       javaVM->internalVMFunctions->jvmPhaseChange(javaVM, J9VM_PHASE_NOT_STARTUP);

   if (jitConfig->runtimeFlags & J9JIT_TOSS_CODE)
      {
      // JIT_TOSS_CODE is broken.  This routine isn't called if it's set...
      j9tty_printf(PORTLIB, "JIT: not installing counting send targets.\n");
      }
   else
      {
      // Do not register the hook that sets method invocation counts in JITServer server mode
      // This ensures that interpreter will not send methods for compilation
#if defined(J9VM_OPT_JITSERVER)
      if (compInfo->getPersistentInfo()->getRemoteCompilationMode() != JITServer::SERVER)
#endif
         {
         if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_INITIALIZE_SEND_TARGET, jitHookInitializeSendTarget, OMR_GET_CALLSITE(), NULL))
            {
            j9tty_printf(PORTLIB, "Error: Unable to install send target hook\n");
            return -1;
            }
         }
#if defined (J9VM_INTERP_PROFILING_BYTECODES)
      TR_IProfiler *iProfiler = vmj9->getIProfiler();

      if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableInterpreterProfiling) &&
          iProfiler && (iProfiler->getProfilerMemoryFootprint() < TR::Options::_iProfilerMemoryConsumptionLimit))
         {
         if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableIProfilerThread))
            {
            iProfiler->startIProfilerThread(javaVM);
            }
         if (TR::Options::getCmdLineOptions()->getOption(TR_NoIProfilerDuringStartupPhase)
#if defined(J9VM_OPT_JITSERVER)
            || compInfo->getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER
#endif
            )
            {
            interpreterProfilingState = IPROFILING_STATE_OFF;
            }
         else // start Iprofiler right away
            {
            if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_PROFILING_BYTECODE_BUFFER_FULL, jitHookBytecodeProfiling, OMR_GET_CALLSITE(), NULL))
               {
               j9tty_printf(PORTLIB, "Error: Unable to install J9HOOK_VM_PROFILING_BYTECODE_BUFFER_FULL listener\n");
               return -1;
               }
            interpreterProfilingState = IPROFILING_STATE_ON;
            }

         interpreterProfilingWasOnAtStartup = true;

         if (TR::Options::getCmdLineOptions()->getOption(TR_VerboseInterpreterProfiling))
            j9tty_printf(PORTLIB, "Succesfully installed J9HOOK_VM_PROFILING_BYTECODE_BUFFER_FULL listener\n");
         }
#endif
      if (compInfo->getPersistentInfo()->isRuntimeInstrumentationEnabled())
         {
         if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableHWProfilerThread))
            {
            TR_HWProfiler *hwProfiler = ((TR_JitPrivateConfig*)(jitConfig->privateConfig))->hwProfiler;
            hwProfiler->startHWProfilerThread(javaVM);
            }
         }

      if (TR::Options::getCmdLineOptions()->getOption(TR_EnableJProfiling) && !TR::Options::getCmdLineOptions()->getOption(TR_DisableJProfilerThread))
         {
         TR_JProfilerThread *jProfiler = ((TR_JitPrivateConfig*)(jitConfig->privateConfig))->jProfiler;
         jProfiler->start(javaVM);
         }
      }

   if ((*gcOmrHooks)->J9HookRegisterWithCallSite(gcOmrHooks, J9HOOK_MM_OMR_LOCAL_GC_START, jitHookLocalGCStart, OMR_GET_CALLSITE(), NULL) ||
       (*gcOmrHooks)->J9HookRegisterWithCallSite(gcOmrHooks, J9HOOK_MM_OMR_LOCAL_GC_END, jitHookLocalGCEnd, OMR_GET_CALLSITE(), NULL) ||
       (*gcOmrHooks)->J9HookRegisterWithCallSite(gcOmrHooks, J9HOOK_MM_OMR_GLOBAL_GC_START, jitHookGlobalGCStart, OMR_GET_CALLSITE(), NULL) ||
       (*gcOmrHooks)->J9HookRegisterWithCallSite(gcOmrHooks, J9HOOK_MM_OMR_GLOBAL_GC_END, jitHookGlobalGCEnd, OMR_GET_CALLSITE(), NULL))
      {
      j9tty_printf(PORTLIB, "Error: Unable to register gc hook\n");
      return -1;
      }

   if (!vmj9->isAOT_DEPRECATED_DO_NOT_USE())
      {

      IDATA unableToRegisterHooks = 0;


      if (!TR::Options::getCmdLineOptions()->realTimeGC() || !TR::Options::getCmdLineOptions()->getOption(TR_NoClassGC))
         {
         unableToRegisterHooks = (*gcOmrHooks)->J9HookRegisterWithCallSite(gcOmrHooks, J9HOOK_MM_OMR_GC_CYCLE_END, jitHookReleaseCodeGCCycleEnd, OMR_GET_CALLSITE(), NULL);
         }

      if (unableToRegisterHooks)
         {
         j9tty_printf(PORTLIB, "Error: Unable to register gc hook\n");
         return -1;
         }

      }

   if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_INTERNAL_CLASS_LOAD, jitHookClassLoad, OMR_GET_CALLSITE(), NULL) ||
       (*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_CLASS_PREINITIALIZE, jitHookClassPreinitialize, OMR_GET_CALLSITE(), NULL) ||
       (*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_CLASS_INITIALIZE, jitHookClassInitialize, OMR_GET_CALLSITE(), NULL))
      {
      j9tty_printf(PORTLIB, "Error: Unable to register class event hook\n");
      return -1;
      }

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
   if (!vmj9->isAOT_DEPRECATED_DO_NOT_USE())
      {
#if defined(AOTRT_DLL)
      (*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_CLASS_UNLOAD, rtHookClassUnload, NULL);
      (*vmHooks)->J9HookUnregister(vmHooks, J9HOOK_VM_CLASS_LOADER_UNLOAD, rtHookClassLoaderUnload, NULL);
#endif
      if ( (*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_CLASS_UNLOAD, jitHookClassUnload, OMR_GET_CALLSITE(), NULL) ||
           (*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_CLASSES_UNLOAD, jitHookClassesUnload, OMR_GET_CALLSITE(), NULL) ||
           (*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_CLASS_LOADER_UNLOAD, jitHookClassLoaderUnload, OMR_GET_CALLSITE(), NULL) ||
           (*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_ANON_CLASSES_UNLOAD, jitHookAnonClassesUnload, OMR_GET_CALLSITE(), NULL) ||
#if defined(J9VM_JIT_DYNAMIC_LOOP_TRANSFER)
           (*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_CLASS_LOADERS_UNLOAD, jitHookClassLoadersUnload, OMR_GET_CALLSITE(), NULL) ||
#endif
           (*gcHooks)->J9HookRegisterWithCallSite(gcHooks, J9HOOK_MM_INTERRUPT_COMPILATION, jitHookInterruptCompilation, OMR_GET_CALLSITE(), NULL) ||
#if JAVA_SPEC_VERSION >= 19
           (*gcHooks)->J9HookRegisterWithCallSite(gcHooks, J9HOOK_MM_WALKCONTINUATION, jitWalkContinuationStackFrames, OMR_GET_CALLSITE(), NULL) ||
#endif /* JAVA_SPEC_VERSION >= 19 */
           (*gcHooks)->J9HookRegisterWithCallSite(gcHooks, J9HOOK_MM_CLASS_UNLOADING_END, jitHookClassesUnloadEnd, OMR_GET_CALLSITE(), NULL))
         {
         j9tty_printf(PORTLIB, "Error: Unable to register class event hook\n");
         return -1;
         }

      }
#endif

   j9thread_monitor_enter(javaVM->vmThreadListMutex);
   if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_THREAD_CREATED, jitHookThreadCreate, OMR_GET_CALLSITE(), NULL) ||
       (*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_THREAD_END, jitHookThreadEnd, OMR_GET_CALLSITE(), NULL) ||
       (*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_THREAD_CRASH, jitHookThreadCrash, OMR_GET_CALLSITE(), NULL) ||
       (*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_THREAD_DESTROY, jitHookThreadDestroy, OMR_GET_CALLSITE(), NULL) ||
       (*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_THREAD_STARTED, jitHookThreadStart, OMR_GET_CALLSITE(), NULL))
      {
      j9tty_printf(PORTLIB, "Error: Unable to register thread hook\n");
      return -1;
      }
   else
      {
      // FIXME: we should really move the hooking these earlier so that the compilation thread
      // which is created earlier at JIT_INITIALIZED stage also gets the hooks.
      //
      // catch up with the existing threads
      J9VMThread *currThread = javaVM->mainThread;
      if (currThread)
         {
         do
            {
            initThreadAfterCreation(currThread);
            }
         while ((currThread = currThread->linkNext) != javaVM->mainThread);
         }
      }
   j9thread_monitor_exit(javaVM->vmThreadListMutex);

#if defined(J9VM_OPT_CRIU_SUPPORT)
   if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_PREPARING_FOR_CHECKPOINT, jitHookPrepareCheckpoint, OMR_GET_CALLSITE(), NULL) ||
       (*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_PREPARING_FOR_RESTORE, jitHookPrepareRestore, OMR_GET_CALLSITE(), NULL))
      {
      j9tty_printf(PORTLIB, "Error: Unable to register CRIU hook\n");
      return -1;
      }
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */

   if (!vmj9->isAOT_DEPRECATED_DO_NOT_USE()
#if defined(J9VM_OPT_JITSERVER)
      && compInfo->getPersistentInfo()->getRemoteCompilationMode() != JITServer::SERVER
#endif
      )
      {
      if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_JNI_NATIVE_REGISTERED, jitHookJNINativeRegistered, OMR_GET_CALLSITE(), NULL))
         {
         j9tty_printf(PORTLIB, "Error: Unable to register RegisterNatives hook\n");
         return -1;
         }
      }
   return 0;
   }

#if defined(J9VM_OPT_JITSERVER)
int32_t startJITServer(J9JITConfig *jitConfig)
   {
   J9JavaVM *javaVM = jitConfig->javaVM;
   TR_Listener *listener = ((TR_JitPrivateConfig*)(jitConfig->privateConfig))->listener;
   TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig);
   PORT_ACCESS_FROM_JAVAVM(javaVM);

   TR_ASSERT(compInfo->getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER, "startJITServer cannot be called in non-server mode\n");

   if (compInfo->useSSL())
      {
      (*OSSL_load_error_strings)();
      (*OSSL_library_init)();
      }

   listener->startListenerThread(javaVM);

   if (TR::Options::getVerboseOption(TR_VerboseJITServer))
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Started JITServer listener thread: %p ", listener->getListenerThread());


   MetricsServer *metricsServer = ((TR_JitPrivateConfig*)(jitConfig->privateConfig))->metricsServer;
   if (metricsServer)
      {
      metricsServer->startMetricsThread(javaVM);
      }

   if (jitConfig->samplingFrequency != 0)
      {
      JITServerStatisticsThread *statsThreadObj = ((TR_JitPrivateConfig*)(jitConfig->privateConfig))->statisticsThreadObject;
      // statsThreadObj is guaranteed to be non-null because JITServer will not start if statisticsThreadObject cannot be created
      statsThreadObj->startStatisticsThread(javaVM);
      // Verify that statistics thread was started
      if (!statsThreadObj->getStatisticsThread())
         {
         j9tty_printf(PORTLIB, "Error: Unable to start the statistics thread\n");
         return -1;
         // If we decide to start even without a statistics thread, we must
         // free `statsThreadObj` and set the corresponding jitConfig field to NULL
         }
      }
   return 0;
   }

int32_t waitJITServerTermination(J9JITConfig *jitConfig)
   {
   J9JavaVM *javaVM = jitConfig->javaVM;
   TR_Listener *listener = ((TR_JitPrivateConfig*)(jitConfig->privateConfig))->listener;
   TR::CompilationInfo * compInfo = TR::CompilationInfo::get(jitConfig);
   int32_t rc = 0;

   TR_ASSERT(compInfo->getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER, "waitJITServerTermination cannot be called in non-server mode\n");

   rc = listener->waitForListenerThreadExit(javaVM);
   return rc;
   }
#endif /* J9VM_OPT_JITSERVER */

} /* extern "C" */
