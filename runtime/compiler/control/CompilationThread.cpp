/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#define J9_EXTERNAL_TO_VM

#if SOLARIS || AIXPPC || LINUX || OSX
#include <strings.h>
#define J9OS_STRNCMP strncasecmp
#else
#define J9OS_STRNCMP strncmp
#endif

#if defined (_MSC_VER) && _MSC_VER < 1900
#define snprintf _snprintf
#endif

#include "control/CompilationThread.hpp"

#include <exception>
#include <limits.h>
#include <stdlib.h>
#include <time.h>
#include "j9.h"
#include "j9cfg.h"
#include "j9protos.h"
#include "jitprotos.h"
#include "vmaccess.h"
#include "objhelp.h"
#include "shcdatatypes.h"
#include "codegen/CodeGenerator.hpp"
#include "codegen/Instruction.hpp"
#include "codegen/AheadOfTimeCompile.hpp"
#include "compile/CompilationTypes.hpp"
#include "compile/ResolvedMethod.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "control/MethodToBeCompiled.hpp"
#include "control/OptimizationPlan.hpp"
#include "env/CompilerEnv.hpp"
#include "env/IO.hpp"
#include "env/PersistentInfo.hpp"
#include "env/StackMemoryRegion.hpp"
#include "env/jittypes.h"
#include "env/ClassTableCriticalSection.hpp"
#include "env/VMAccessCriticalSection.hpp"
#include "compile/CompilationException.hpp"
#include "runtime/CodeCacheExceptions.hpp"
#include "exceptions/JITShutDown.hpp"
#include "exceptions/PersistenceFailure.hpp"
#include "exceptions/LowPhysicalMemory.hpp"
#include "exceptions/RuntimeFailure.hpp"
#include "exceptions/AOTFailure.hpp"
#include "exceptions/FSDFailure.hpp"
#include "exceptions/DataCacheError.hpp"
#include "il/Node.hpp"
#include "il/Node_inlines.hpp"
#include "ilgen/IlGenRequest.hpp"
#include "ilgen/IlGeneratorMethodDetails_inlines.hpp"
#include "infra/CriticalSection.hpp"
#include "infra/MonitorTable.hpp"
#include "infra/Monitor.hpp"
#include "ras/InternalFunctions.hpp"
#include "runtime/asmprotos.h"
#include "runtime/CodeCache.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/J9VMAccess.hpp"
#include "runtime/RelocationRuntime.hpp"
#include "runtime/RelocationTarget.hpp"
#include "runtime/J9Profiler.hpp"
#include "control/CompilationRuntime.hpp"
#include "env/j9method.h"
#include "env/J9SharedCache.hpp"
#include "env/VMJ9.h"
#include "env/annotations/AnnotationBase.hpp"
#include "runtime/MethodMetaData.h"
#include "env/J9JitMemory.hpp"
#include "env/J9SegmentCache.hpp"
#include "env/SystemSegmentProvider.hpp"
#include "env/DebugSegmentProvider.hpp"
#include "env/ClassLoaderTable.hpp"
#include "runtime/ArtifactManager.hpp"
#include "runtime/CodeCacheMemorySegment.hpp"
#include "env/j9methodServer.hpp"
#include "control/JITaaSCompilationThread.hpp"
#include "env/JITaaSPersistentCHTable.hpp"
#include "runtime/JITaaSIProfiler.hpp"

#ifdef COMPRESS_AOT_DATA
#ifdef J9ZOS390
// inflateInit checks the version of the zlib with which data was deflated. 
// Reason we need to avoid conversion here is because, we are statically linking
// system zlib which would have encoded String literals in some version which is not 
// same as the version we are converting out string literals in. This leads to issue
// where we fail inflating data with version mismatch. 
#pragma convlit(suspend)
#include <zlib.h>
#pragma convlit(resume)
#else
#include "zlib.h"
#endif
#endif
#if defined(J9VM_OPT_SHARED_CLASSES)
#include "j9jitnls.h"
#endif

OMR::CodeCacheMethodHeader *getCodeCacheMethodHeader(char *p, int searchLimit, J9JITExceptionTable* metaData);
extern "C" {   
   int32_t getCount(J9ROMMethod *romMethod, TR::Options *optionsJIT, TR::Options *optionsAOT);  
   int32_t encodeCount(int32_t count); 
   }
static void printCompFailureInfo(TR::Compilation * comp, const char * reason);

#include "env/ut_j9jit.h"

#if defined(TR_HOST_S390)
#include <sys/time.h>
#endif

#if defined(AIXPPC)
#include <unistd.h>
#endif

#if defined(J9VM_INTERP_PROFILING_BYTECODES)
#include "runtime/IProfiler.hpp"
#endif

IDATA J9THREAD_PROC compilationThreadProc(void *jitconfig);
IDATA J9THREAD_PROC protectedCompilationThreadProc(J9PortLibrary *portLib, TR::CompilationInfoPerThread*compInfoPT/*void *vmthread*/);

extern TR::OptionSet *findOptionSet(J9Method *, bool);

#define VM_STATE_COMPILING 64
#define MAX_NUM_CRASHES 4
#define COMPRESSION_FAILED -1
#define DECOMPRESSION_FAILED -1
#define DECOMPRESSION_SUCCEEDED 0

#if defined(WINDOWS)
void setThreadAffinity(unsigned _int64 handle, unsigned long mask)
   {
   if (SetThreadAffinityMask((HANDLE)handle, mask)==0)
      {
      #ifdef DEBUG
      fprintf(stderr, "cannot set affinity mask was %x\n", mask);
      #endif
      }
   }
#endif

TR_RelocationRuntime*
TR::CompilationInfoPerThreadBase::reloRuntime()
   {
   if (_methodBeingCompiled->isAotLoad() ||
       _compInfo.getPersistentInfo()->getRemoteCompilationMode() == JITServer::NONE ||
       (_compInfo.getPersistentInfo()->getRemoteCompilationMode() == JITServer::CLIENT && TR::Options::sharedClassCache())) // Enable local AOT compilations at client
      return static_cast<TR_RelocationRuntime*>(&_sharedCacheReloRuntime);
   return static_cast<TR_RelocationRuntime*>(&_remoteCompileReloRuntime);
   }

void
TR::CompilationInfoPerThreadBase::setCompilation(TR::Compilation *compiler)
   {
#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
   TR::Compilation *tlsCompiler = TR::comp();
   // Exclude PPC LE from the following assert due to xlc compiler bug
   TR_ASSERT(
      tlsCompiler == compiler || (TR::Compiler->target.cpu.isPower() && TR::Compiler->target.cpu.isLittleEndian()),
      "TLS compilation object @ " POINTER_PRINTF_FORMAT " does not match provided compilation object @ " POINTER_PRINTF_FORMAT,
      tlsCompiler,
      compiler);
#endif
   _compiler = compiler;
   }

#ifdef J9VM_RAS_DUMP_AGENTS
static UDATA
blankDumpSignalHandler(struct J9PortLibrary *portLibrary, U_32 gpType, void *gpInfo, void *arg)
   {
   J9VMThread *vmThread = (J9VMThread *) arg;
   TR_VerboseLog::writeLineLocked(TR_Vlog_JITDUMP, "vmThread=%p Recursive crash occurred. Aborting JIT dump.", vmThread);

   // Returning J9PORT_SIG_EXCEPTION_RETURN will make us come back to the same crashing instruction over and over
   //
   return J9PORT_SIG_EXCEPTION_RETURN; // FIXME: is this the right return type? - This appears to be the right return type
   }
#endif

thread_local TR::CompilationInfoPerThread *TR::compInfoPT;

static UDATA
jitSignalHandler(struct J9PortLibrary *portLibrary, U_32 gpType, void *gpInfo, void *arg)
   {
   static int32_t numCrashes = 0;
   bool recoverable = true;
   J9VMThread *vmThread = (J9VMThread *)arg;

   // Try to find the compilation object
   TR::Compilation * comp = 0;
   TR_MethodToBeCompiled *methodToBeCompiled = NULL;
   J9JITConfig *jitConfig = vmThread->javaVM->jitConfig;
   TR::CompilationInfo *compInfo = TR::CompilationInfo::get(jitConfig);
   TR::CompilationInfoPerThreadBase *cp = compInfo->getCompInfoForCompOnAppThread();

   // compiling on app thread
   if (cp)
      {
      comp = cp->getCompilation();
      methodToBeCompiled = cp->getMethodBeingCompiled();
      }

   // probably compiling on separate thread
   else
      {
      // make sure we're really compiling on separate thread
      if (compInfo->useSeparateCompilationThread())
         {
         TR::CompilationInfoPerThread *compInfoPT = compInfo->getCompInfoForThread(vmThread);
         if (compInfoPT)
            {
            comp = compInfoPT->getCompilation();
            methodToBeCompiled = compInfoPT->getMethodBeingCompiled();
            }
         }
      }

   const char *sig = (comp && comp->signature() ? comp->signature() : "<unknown>");

   TR::MonitorTable *table = TR::MonitorTable::get();

   if (!table ||
       !comp ||
       (table && !table->isThreadInSafeMonitorState(vmThread)))
       recoverable = false;

   static char *alwaysCrash = feGetEnv("TR_NoCrashHandling");
   if (alwaysCrash)
      recoverable = false;

   // If we have been seeing lots of crashes, bail out, and do not attempt to recover
   //
   if (numCrashes >= MAX_NUM_CRASHES)
      recoverable = false;

#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
   static char *autoRecover = feGetEnv("TR_CrashHandling");
   if (!autoRecover)
      recoverable = false;
   fprintf(stderr, "JIT: crashed while compiling %s (recoverable %d)\n", sig, recoverable);
#endif

   // For now: disable recovery
   //
   recoverable = false;

   // Possible return values here are
   // J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH    -- will cause a crash-dump
   // J9PORT_SIG_EXCEPTION_CONTINUE_EXECUTION -- will cause a secondary crash (DO NOT USE THIS)
   // J9PORT_SIG_EXCEPTION_RETURN             -- abort current compilation gracefully
   if (recoverable)
      {
      numCrashes++;
      Trc_JIT_recoverableCrash(vmThread, sig);
      return J9PORT_SIG_EXCEPTION_RETURN;
      }
   Trc_JIT_fatalCrash(vmThread, sig);

   TR_Debug::printStackBacktrace();

   return J9PORT_SIG_EXCEPTION_CONTINUE_SEARCH;
   }
#ifdef COMPRESS_AOT_DATA
#ifdef J9ZOS390
#pragma convlit(suspend)
#endif
/*
 * \brief
 *    Inflates the data buffer using zlib into output buffer
 *
 * \param 
 *    buffer Input buffer that is going to be inflated 
 * 
 * \param 
 *    numberOfBytes Size of the data in the input buffer to inflate
 * 
 * \param
 *    outBuffer Output buffer to hold the deflated data
 * 
 * \param
 *    unCompressedSize Size of the original data
 * 
 * \return
 *    Returns DECOMPRESSION_FAILED if it can not inflate the buffer
 * 
 */
static
int inflateBuffer(U_8 *buffer, int numberOfBytes, U_8 *outBuffer, int unCompressedSize)
   {
   Bytef *in = (Bytef*)buffer;
   Bytef *out = (Bytef*)outBuffer;
   z_stream _stream;
   _stream.zalloc = Z_NULL;
   _stream.zfree = Z_NULL;
   _stream.opaque = Z_NULL;
   _stream.avail_in = 0;
   _stream.next_in = Z_NULL;
   int ret = inflateInit(&_stream);
   if (ret != Z_OK)
      return DECOMPRESSION_FAILED;
   _stream.avail_out = unCompressedSize;
   _stream.next_out = out;
   _stream.next_in = in;
   _stream.avail_in = numberOfBytes;
   ret = inflate(&_stream, Z_NO_FLUSH);
   /**
    * ZLIB returns Z_STREAM_END if the buffer stream to be inflated is finished.
    * In this case it returns DECOMPRESSION_FAILED.
    * Caller of this routine can then decide if they want to retry deflating
    * Using larger output buffer. 
    */
   if (ret != Z_STREAM_END)
      {
      TR_ASSERT(0, "Fails while inflating buffer");
      ret = DECOMPRESSION_FAILED;
      }
   else
      {
      ret = DECOMPRESSION_SUCCEEDED;
      }
   inflateEnd(&_stream);
   return ret;
   }

/*
 * \brief
 *    Deflates the data buffer using zlib into output buffer
 *
 * \param 
 *    buffer Input buffer that is going to be deflated 
 * 
 * \param 
 *    numberOfBytes Size of the data in the input buffer to deflate
 * 
 * \param
 *    outBuffer Output buffer to hold the deflated data
 * 
 * \param
 *    level Level of compression
 * 
 * \return
 *    If successful, returns Size of data in the deflated bufer
 *    else returns COMPRESSION_FAILED
 * 
 */
static
int deflateBuffer(const U_8 *buffer, int numberOfBytes, U_8 *outBuffer, int level)
   {
   z_stream _stream;
   _stream.zalloc = Z_NULL;
   _stream.zfree = Z_NULL;
   _stream.opaque = Z_NULL;
   int ret=deflateInit(&_stream, level);
   if (ret != Z_OK)
      return COMPRESSION_FAILED;
   // Start deflating
   _stream.avail_in = numberOfBytes;
   _stream.next_in = (Bytef*) buffer;
   _stream.avail_out = numberOfBytes;
   _stream.next_out = (Bytef*) outBuffer;
   ret = deflate(&_stream, Z_FINISH);
   /**
    * ZLIB returns Z_STREAM_END if the buffer stream to be deflated is finished.
    * That Return COMPRESSION_FAILED in case we are ending up inflating the data.
    * Caller of this routine can then decide if they want to retry deflating
    * Using larger output buffer. 
    */
   if ( ret != Z_STREAM_END )
      {
      TR_ASSERT(0, "Deflated data is larger than original data.");
      deflateEnd(&_stream);
      return COMPRESSION_FAILED;
      }
   
   int compressedSize = numberOfBytes - _stream.avail_out;
   deflateEnd(&_stream);
   return compressedSize;
   }
#ifdef J9ZOS390
#pragma convlit(resume)
#endif
#endif
inline void
TR::CompilationInfo::incrementMethodQueueSize()
   {
   _numQueuedMethods++;
   // Keep track of maxQueueSize for information purposes
   if (_numQueuedMethods > _maxQueueSize)
      _maxQueueSize = _numQueuedMethods;
   }

int32_t TR::CompilationInfo::VERY_SMALL_QUEUE   = 2;
int32_t TR::CompilationInfo::SMALL_QUEUE        = 4;
int32_t TR::CompilationInfo::MEDIUM_LARGE_QUEUE = 30;
int32_t TR::CompilationInfo::LARGE_QUEUE        = 40;
int32_t TR::CompilationInfo::VERY_LARGE_QUEUE   = 55;

TR::CompilationInfo * TR::CompilationInfo::_compilationRuntime = NULL;

TR_MethodToBeCompiled *
TR::CompilationInfo::getCompilationQueueEntry()
   {
   TR_MethodToBeCompiled *unusedEntry = NULL;

   // Search for an empty entry in the methodPool.  It is possible
   // to have entries in the pool which are not fully free'd yet.
   // The compile request is completed, and hence the entry is in the pool.
   // Once in the pool, the numThreadsWaiting can only decrease.
   for (TR_MethodToBeCompiled *cur = _methodPool, *prev = NULL; cur; prev = cur, cur = cur->_next)
      {
      if (cur->_numThreadsWaiting == 0)
         {
         unusedEntry = cur;
         if (prev)
            prev->_next = cur->_next;
         else
            _methodPool = cur->_next;
         --_methodPoolSize;
         break;
         }
      }

   if (!unusedEntry)
      {
      unusedEntry = TR_MethodToBeCompiled::allocate(_jitConfig);
      if (unusedEntry)
         unusedEntry->_freeTag = ENTRY_IN_POOL_FREE;
      }

   return unusedEntry;

   }

TR::CompilationInfoPerThread *
TR::CompilationInfo::findFirstLowPriorityCompilationInProgress(CompilationPriority priority) // needs compilationQueueMonitor in hand
   {
   TR::CompilationInfoPerThread *lowPriorityCompInProgress = NULL;
   for (uint8_t i = 0; i < getNumUsableCompilationThreads(); i++)
      {
      TR::CompilationInfoPerThread *curCompThreadInfoPT = _arrayOfCompilationInfoPerThread[i];
      TR_ASSERT(curCompThreadInfoPT, "a thread's compinfo is missing\n");
      if (curCompThreadInfoPT->getMethodBeingCompiled() &&
          curCompThreadInfoPT->getMethodBeingCompiled()->_priority < priority)
         {
         lowPriorityCompInProgress = curCompThreadInfoPT;
         break;
         }
      } // end for
   return lowPriorityCompInProgress;
   }

int32_t TR::CompilationInfo::computeDynamicDumbInlinerBytecodeSizeCutoff(TR::Options *options)
   {
   // Q_SZ==0  ==> dynamicCutoff=1600
   // Q_SZ==32 ==> dynamicCutoff=200
   // dynamicCutoff = 1600 - (1600-200)*Q_SZ/32
   int32_t cutoff = options->getDumbInlinerBytecodeSizeMaxCutoff() -
                       (((options->getDumbInlinerBytecodeSizeMaxCutoff() - options->getDumbInlinerBytecodeSizeMinCutoff())*
                           getMethodQueueSize()) >> 5);
   if (cutoff < options->getDumbInlinerBytecodeSizeMinCutoff())
      cutoff = options->getDumbInlinerBytecodeSizeMinCutoff();
   return cutoff;
   }

// Examine if we need to activate a new thread
// Must have compilation queue monitor in hand when calling this routine
TR_YesNoMaybe TR::CompilationInfo::shouldActivateNewCompThread()
   {
   // We must have at least one compilation thread active
   if (getNumCompThreadsActive() <= 0)
      return TR_yes;
   int32_t numCompThreadsSuspended = getNumUsableCompilationThreads() - getNumCompThreadsActive();
   TR_ASSERT(numCompThreadsSuspended >= 0, "Accounting error for suspendedCompThreads usable=%d active=%d\n", getNumUsableCompilationThreads(), getNumCompThreadsActive());
   // Cannot activate if there is nothing to activate
   if (numCompThreadsSuspended <= 0)
      return TR_no;
   // Do not activate new threads if we are ramping down
   if (getRampDownMCT())
      return TR_no;

   // Always activate in JITServer server mode
   if (getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER)
      return TR_yes;

   // Do not activate if we already exceed the CPU enablement for compilation threads
   if (exceedsCompCpuEntitlement() != TR_no)
      {
      // The (- 50) below implements 'rounding', so one compilation thread is considered
      // enough for an enablement of [0 - 150]%
      if ((getNumCompThreadsActive() + 1) * 100 >= (TR::Options::_compThreadCPUEntitlement + 50))
         return TR_no;
      }
   // Do not activate if we are low on physical memory
   bool incompleteInfo;
   uint64_t freePhysicalMemorySizeB = computeAndCacheFreePhysicalMemory(incompleteInfo);
   if (freePhysicalMemorySizeB != OMRPORT_MEMINFO_NOT_AVAILABLE &&
       freePhysicalMemorySizeB <= (uint64_t)TR::Options::getSafeReservePhysicalMemoryValue() + TR::Options::getScratchSpaceLowerBound())
      return TR_no;
   // Do not activate a new thread during graceperiod if AOT is used and first run because
   // we may have too many warm compilations at warm. However, there is no such risk for quickstart
   // Another exception: activate if second run in AOT mode

   bool isSecondAOTRun = !TR::Options::getAOTCmdLineOptions()->getOption(TR_NoAotSecondRunDetection) &&
      !(numMethodsFoundInSharedCache() < TR::Options::_aotMethodThreshold ||
      _statNumAotedMethods > TR::Options::_aotMethodCompilesThreshold);
   if (TR::Options::sharedClassCache() &&
      !TR::Options::getCmdLineOptions()->isQuickstartDetected() &&
      !isSecondAOTRun &&
       getPersistentInfo()->getElapsedTime() < (uint64_t)getPersistentInfo()->getClassLoadingPhaseGracePeriod())
      return TR_no;

   // Activate if the compilation backlog is large.
   // If there is no comp thread starvation or if the number of comp threads was
   // determined based on the number of CPUs, then the upper bound of comp threads is _numTargetCPUs-1
   // However, if the compilation threads are starved (on Linux) we may want
   // to activate additional comp threads irrespective of the CPU entitlement
   if (TR::Options::_useCPUsToDetermineMaxNumberOfCompThreadsToActivate)
      {
      if (getNumCompThreadsActive() < getNumTargetCPUs() - 1)
         {
         if (_queueWeight > _compThreadActivationThresholds[getNumCompThreadsActive()])
            return TR_yes;
         }
      else if (_starvationDetected)
         {
         // comp thread starvation; may activate threads beyond numCpu-1
         if (_queueWeight > _compThreadActivationThresholdsonStarvation[getNumCompThreadsActive()])
            return TR_yes;
         }
      }
   else // number of compilation threads indicated by the user
      {
      if (_queueWeight > _compThreadActivationThresholds[getNumCompThreadsActive()])
         return TR_yes;
      }

   // Allow the caller to add additional tests if required
   return TR_maybe;
   }

bool TR::CompilationInfo::importantMethodForStartup(J9Method *method)
   {
   if (getMethodBytecodeSize(method) < TR::Options::_startupMethodDontDowngradeThreshold) // filter by size as well
      {
      // During startup java/lang/String* java/util/zip/* and java/util/Hash*  methods are rather important
      J9ROMClass *romClazz = J9_CLASS_FROM_METHOD(method)->romClass;
      J9UTF8 * className = J9ROMCLASS_CLASSNAME(romClazz);
      if (TR::Compiler->target.numberOfProcessors() <= 2)
         {
         if (className->length == 16 && 0==memcmp(utf8Data(className), "java/lang/String", 16))
            return true;
         }
      else if (className->length >= 14)
         {
         if (0==memcmp(utf8Data(className), "java/lang/Stri", 14) ||
             0==memcmp(utf8Data(className), "java/util/zip/", 14) ||
             0==memcmp(utf8Data(className), "java/util/Hash", 14))
            {
            return true;
            }
         }
      }
   return false;
   }


bool TR::CompilationInfo::shouldDowngradeCompReq(TR_MethodToBeCompiled *entry)
   {
   bool doDowngrade = false;
   J9Method *method = entry->getMethodDetails().getMethod();
   if (entry->_oldStartPC == 0 && /*entry->_priority <= CP_ASYNC_MAX &&*/
       entry->_optimizationPlan->getOptLevel() == warm && // only warm compilations are subject to downgrades
       !entry->isDLTCompile() &&
       !TR::Options::getCmdLineOptions()->getOption(TR_DontDowngradeToCold))
      {
      TR::PersistentInfo *persistentInfo = getPersistentInfo();
      const J9ROMMethod * romMethod = entry->getMethodDetails().getRomMethod();
      TR_J9VMBase *fe = TR_J9VMBase::get(_jitConfig, NULL);

      // Don't downgrade if method is JSR292. See CMVC 200145
      if (_J9ROMMETHOD_J9MODIFIER_IS_SET(romMethod, J9AccMethodHasMethodHandleInvokes))
         {
         doDowngrade = false;
         }
      // similarly don't downgrade a thunk archetype - inlining in these is also vital
      else if (fe->isThunkArchetype(method))
         {
         doDowngrade = false;
         }
              // perform JNI at cold
      else if (entry->isJNINative() ||
              // downgrade AOT loads that failed
              (entry->_methodIsInSharedCache == TR_yes && entry->_doNotUseAotCodeFromSharedCache && entry->_compilationAttemptsLeft < MAX_COMPILE_ATTEMPTS))
         {
         doDowngrade = true;
         }
               // downgrade during IDLE
      else if ((persistentInfo->getJitState() == IDLE_STATE && entry->_jitStateWhenQueued == IDLE_STATE) &&
               // but not if the machine uses very little CPU and we want to exploit idle
               !(TR::Options::getCmdLineOptions()->getOption(TR_UseIdleTime) &&
                 getCpuUtil() && getCpuUtil()->isFunctional() &&
                 getCpuUtil()->getCpuUsage() < 10 &&
                 persistentInfo->getElapsedTime() < 600000) // Downgrade after 10 minutes to conserve CPU in idle mode
              )
         {
         doDowngrade = true;
         }
      else
         {
         // We may skip downgrading during grace period
         if (TR::Options::getCmdLineOptions()->getOption(TR_DontDowgradeToColdDuringGracePeriod) &&
             persistentInfo->getElapsedTime() < (uint64_t)persistentInfo->getClassLoadingPhaseGracePeriod())
            {
            }
         else
            {
            // Downgrade during CLP when queue grows too large
            if ((persistentInfo->isClassLoadingPhase() && getNumQueuedFirstTimeCompilations() > TR::Options::_qsziThresholdToDowngradeDuringCLP) ||
                 // Downgrade if compilation queue is too large
                (TR::Options::getCmdLineOptions()->getOption(TR_EnableDowngradeOnHugeQSZ) &&
                 getMethodQueueSize() >= TR::Options::_qszThresholdToDowngradeOptLevel) ||
                 // Downgrade if compilation queue grows too much during startup
                (_jitConfig->javaVM->phase != J9VM_PHASE_NOT_STARTUP &&
                 getMethodQueueSize() >= TR::Options::_qszThresholdToDowngradeOptLevelDuringStartup) ||
                 // Downgrade if AOT and startup,
                (TR::Options::getCmdLineOptions()->sharedClassCache() &&
#if defined(TR_TARGET_POWER) // temporary hack until PPC AOT bug is found
                 _jitConfig->javaVM->phase == J9VM_PHASE_STARTUP &&
#else
                 _jitConfig->javaVM->phase != J9VM_PHASE_NOT_STARTUP &&
#endif
                 !TR::Options::getCmdLineOptions()->getOption(TR_DisableDowngradeToColdOnVMPhaseStartup))
               )
               {
               doDowngrade = true;
               }
            // Downgrade if RI based recompilation is enabled
            else if (persistentInfo->isRuntimeInstrumentationRecompilationEnabled() && // RI and RI Recompilation is functional
                     !getHWProfiler()->isExpired())
               {
#if !defined(J9ZOS390)  // disable for zOS because we don't want to increase CPU time
               if (!importantMethodForStartup(method))  // Do not downgrade important methods
#endif
                  {
                  if (TR::Options::getCmdLineOptions()->getOption(TR_UseRIOnlyForLargeQSZ))
                     {
                     TR_HWProfiler *hwProfiler = getHWProfiler();
                     // Check Q_SZ and downgrade if the backlog is rather large (>_qszMaxThresholdToRIDowngrade)
                     // If we entered this mode, keep downgrading until Q_SZ drops under _qszMinThresholdToRIDowngrade
                     int32_t qsz = getMethodQueueSize(); // cache it to avoid inconsistency because it can change
                     if (qsz > TR::Options::_qszMaxThresholdToRIDowngrade)
                        {
                        // Change the threshold to a smaller value to implement hysteresis
                        // Race conditions are not important here
                        if (hwProfiler->getQSzThresholdToDowngrade() != TR::Options::_qszMinThresholdToRIDowngrade)
                           hwProfiler->setQSzThresholdToDowngrade(TR::Options::_qszMinThresholdToRIDowngrade);
                        }
                     else if (qsz < TR::Options::_qszMinThresholdToRIDowngrade)
                        {
                        // Change the threshold to a bigger value to implement hysteresis
                        if (hwProfiler->getQSzThresholdToDowngrade() != TR::Options::_qszMaxThresholdToRIDowngrade)
                           hwProfiler->setQSzThresholdToDowngrade(TR::Options::_qszMaxThresholdToRIDowngrade);
                        }
                     if (qsz > hwProfiler->getQSzThresholdToDowngrade())
                        {
                        doDowngrade = true;
                        TR_HWProfiler::_STATS_NumCompDowngradesDueToRI++;
                        }
                     }
                  else if (getHWProfiler()->getProcessBufferState() >= 0 || // Downgrade when RI is on
                      !TR::Options::getCmdLineOptions()->getOption(TR_DontDowngradeWhenRIIsTemporarilyOff))
                     {
                     doDowngrade = true;
                     // Statistics for how many compilations we downgrade due to RI
                     TR_HWProfiler::_STATS_NumCompDowngradesDueToRI++;
                     }
                  }
               }
            }
         // Always downgrade J9VMInternals because they are expensive
         if (!doDowngrade)
            {
            J9UTF8 * className = J9ROMCLASS_CLASSNAME(entry->getMethodDetails().getRomClass());
            if (className->length == 23 && !memcmp(utf8Data(className), "java/lang/J9VMInternals", 23))
               {
               doDowngrade = true;
               }
            }

         // Count methods downgraded while RI buffer processing was off
         if (doDowngrade && getPersistentInfo()->isRuntimeInstrumentationEnabled() &&
             getHWProfiler()->getProcessBufferState() < 0)
            {
            getHWProfiler()->incNumDowngradesSinceTurnedOff();
            }
         }
      }
   return doDowngrade;
   }


bool
TR::CompilationInfo::createCompilationInfo(J9JITConfig * jitConfig)
   {
   TR_ASSERT(!_compilationRuntime, "The global compilation info has already been allocated");
   try
      {
      TR::RawAllocator rawAllocator(jitConfig->javaVM);
      void * alloc = rawAllocator.allocate(sizeof(TR::CompilationInfo));
      /* FIXME: Replace this with the appropriate initializers in the constructor */
      /* Note: there are embedded objects in TR::CompilationInfo that rely on the fact
         that we do memset this object to 0 */
      memset(alloc, 0, sizeof(TR::CompilationInfo));
      _compilationRuntime = new (alloc) TR::CompilationInfo(jitConfig);

      #ifdef DEBUG
         if (debug("traceThreadCompile"))
            _compilationRuntime->_traceCompiling = true;
      #endif
      }
   catch (const std::exception &e)
      {
      return false;
      }
   return true;
   }

void TR::CompilationInfo::freeCompilationInfo(J9JITConfig *jitConfig)
   {
   TR_ASSERT(_compilationRuntime, "The global compilation info has already been freed.");
   TR::CompilationInfo * compilationRuntime = _compilationRuntime;
   _compilationRuntime = NULL;

   compilationRuntime->freeAllCompilationThreads();

   TR::RawAllocator rawAllocator(jitConfig->javaVM);
   compilationRuntime->~CompilationInfo();
   rawAllocator.deallocate(compilationRuntime);
   }

J9ROMClass *
TR::CompilationInfoPerThread::getRemoteROMClassIfCached(J9Class *clazz)
   {
   return JITaaSHelpers::getRemoteROMClassIfCached(getClientData(), clazz);
   }

J9ROMClass *
TR::CompilationInfoPerThread::getAndCacheRemoteROMClass(J9Class *clazz, TR_Memory *trMemory)
   {
   auto romClass = getRemoteROMClassIfCached(clazz);
   if (romClass == NULL)
      {
      JITaaSHelpers::ClassInfoTuple classInfoTuple;
      romClass = JITaaSHelpers::getRemoteROMClass(clazz, getStream(), trMemory ? trMemory : TR::comp()->trMemory(), &classInfoTuple);
      JITaaSHelpers::cacheRemoteROMClass(getClientData(), clazz, romClass, &classInfoTuple);
      }
   return romClass;
   }

void
TR::CompilationInfoPerThread::waitForGCCycleMonitor(bool threadHasVMAccess)
   {
#if defined (J9VM_GC_REALTIME)
   J9JavaVM *vm = _jitConfig->javaVM;
   PORT_ACCESS_FROM_JAVAVM(vm);
   j9thread_monitor_enter(vm->omrVM->_gcCycleOnMonitor);
   while (vm->omrVM->_gcCycleOn)
      {
      uint64_t waitTime = 0;
      _compInfo.debugPrint(_compilationThread, "GC cycle is on, waiting for the cycle to finish\n");

      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseGCcycle))
         {
         waitTime = j9time_hires_clock(); // may not be good for SMP
         TR_VerboseLog::writeLineLocked(TR_Vlog_GCCYCLE,"CompilationThread will wait for GC cycle to finish");
         }

      // Before waiting on the monitor we need to release VM access (but only if the thread has it)
      //
      if (threadHasVMAccess)
         {
         _compInfo.debugPrint(_compilationThread, "\tcompilation thread releasing VM access\n");
         releaseVMAccess(_compilationThread);
         _compInfo.debugPrint(_compilationThread, "-VMacc\n");
         }

      j9thread_monitor_wait(vm->omrVM->_gcCycleOnMonitor);
      _compInfo.debugPrint(_compilationThread, "GC cycle is finished, resuming compilation\n");

      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseGCcycle))
         {
         // measure the time we spent waiting
         waitTime = j9time_hires_delta(waitTime, j9time_hires_clock(), J9PORT_TIME_DELTA_IN_MILLISECONDS);
         TR_VerboseLog::writeLineLocked(TR_Vlog_GCCYCLE,"CompilationThread woke up (GC cycle finished); Waiting time = %u msec", (uint32_t)waitTime);
         }

      // Acquire VM access
      //
      if (threadHasVMAccess)
         {
         j9thread_monitor_exit(vm->omrVM->_gcCycleOnMonitor);
         acquireVMAccessNoSuspend(_compilationThread);
         _compInfo.debugPrint(_compilationThread, "+VMacc\n");
         j9thread_monitor_enter(vm->omrVM->_gcCycleOnMonitor);
         }
      } // end while
   j9thread_monitor_exit(vm->omrVM->_gcCycleOnMonitor);
#endif
   }

extern char *compilationErrorNames[]; // defined in rossa.cpp

void
TR::CompilationInfoPerThreadBase::setCompilationThreadState(CompilationThreadState v)
   {
   TR_ASSERT(
      _compInfo.getCompilationMonitor()->owned_by_self(),
      "Modifying compilation thread state without ownership of the compilation queue monitor"
      );
   _previousCompilationThreadState = _compilationThreadState;
   _compilationThreadState = v;
   }

void
TR::CompilationInfoPerThread::setCompilationThreadState(CompilationThreadState v)
   {
   TR::CompilationInfoPerThreadBase::setCompilationThreadState(v);
   getCompilationInfo()->addCompilationTraceEntry(_compilationThread, OP_StateChange, (uint8_t)v);
   }

bool
TR::CompilationInfoPerThreadBase::compilationThreadIsActive()
   {
   bool compThreadIsActive =
      _compilationThreadState == COMPTHREAD_ACTIVE
      || _compilationThreadState == COMPTHREAD_SIGNAL_WAIT
      || _compilationThreadState == COMPTHREAD_WAITING;
   return compThreadIsActive;
   }

TR::FILE *TR::CompilationInfoPerThreadBase::_perfFile = NULL; // used on Linux for perl tool support

// Must not be called from different threads in parallel
TR::CompilationInfoPerThreadBase::CompilationInfoPerThreadBase(TR::CompilationInfo &compInfo, J9JITConfig *jitConfig, int32_t id, bool onSeparateThread) :
   _compInfo(compInfo),
   _jitConfig(jitConfig),
   _sharedCacheReloRuntime(jitConfig),
   _remoteCompileReloRuntime(jitConfig),
   _compThreadId(id),
   _onSeparateThread(onSeparateThread),
   _vm(NULL),
   _methodBeingCompiled(NULL),
   _compiler(NULL),
   _metadata(NULL),
   _reservedDataCache(NULL),
   _timeWhenCompStarted(),
   _numJITCompilations(),
   _qszWhenCompStarted(),
   _compilationCanBeInterrupted(false),
   _compilationThreadState(COMPTHREAD_UNINITIALIZED),
   _compilationShouldBeInterrupted(false),
   _addToJProfilingQueue(false),
   _cachedClientDataPtr(NULL),
   _clientStream(NULL)
   {
   // At this point, compilation threads have not been fully started yet. getNumTotalCompilationThreads()
   // would not return a correct value. Need to use TR::Options::_numUsableCompilationThreads
   TR_ASSERT(_compThreadId < (TR::Options::_numUsableCompilationThreads + TR::CompilationInfo::MAX_DIAGNOSTIC_COMP_THREADS),
             "Cannot have a compId %d greater than %u", _compThreadId, (TR::Options::_numUsableCompilationThreads + TR::CompilationInfo::MAX_DIAGNOSTIC_COMP_THREADS));
   }

TR::CompilationInfoPerThread::CompilationInfoPerThread(TR::CompilationInfo &compInfo, J9JITConfig *jitConfig, int32_t id, bool isDiagnosticThread)
                                   : TR::CompilationInfoPerThreadBase(compInfo, jitConfig, id, true),
                                     _compThreadCPU(_compInfo.persistentMemory()->getPersistentInfo(), jitConfig, 490000000, id)
   {
   PORT_ACCESS_FROM_JITCONFIG(jitConfig);
   _initializationSucceeded = false;
   _osThread = 0;
   _compilationThread = 0;
   _compThreadPriority = J9THREAD_PRIORITY_USER_MAX;
   _compThreadMonitor = TR::Monitor::create("JIT-CompThreadMonitor-??");
   _lastCompilationDuration = 0;

   // name the thread
   //
   // NOTE:
   //      increasing MAX_*_COMP_THREADS over 3 digits requires an
   //      increase in the length of _activeThreadName and _suspendedThreadName

   // constant thread name formats
   static const char activeDiagnosticThreadName[]    = "JIT Diagnostic Compilation Thread-%03d";
   static const char suspendedDiagnosticThreadName[] = "JIT Diagnostic Compilation Thread-%03d Suspended";
   static const char activeThreadName[]              = "JIT Compilation Thread-%03d";
   static const char suspendedThreadName[]           = "JIT Compilation Thread-%03d Suspended";

   const char * selectedActiveThreadName;
   const char * selectedSuspendedThreadName;
   int activeThreadNameLength;
   int suspendedThreadNameLength;

   // determine the correct name to use, and its length
   //
   // NOTE:
   //       using sizeof(...) - 1 because the characters "%d" will be replaced by one digit;
   //       the null character however *is* counted by sizeof
   if (isDiagnosticThread)
      {
      selectedActiveThreadName    = activeDiagnosticThreadName;
      selectedSuspendedThreadName = suspendedDiagnosticThreadName;
      activeThreadNameLength      = sizeof(activeDiagnosticThreadName) - 1;
      suspendedThreadNameLength   = sizeof(suspendedDiagnosticThreadName) - 1;
      _isDiagnosticThread         = true;
      }
   else
      {
      selectedActiveThreadName    = activeThreadName;
      selectedSuspendedThreadName = suspendedThreadName;
      activeThreadNameLength      = sizeof(activeThreadName) - 1;
      suspendedThreadNameLength   = sizeof(suspendedThreadName) - 1;
      _isDiagnosticThread         = false;
      }

   _activeThreadName    = (char *) j9mem_allocate_memory(activeThreadNameLength,    J9MEM_CATEGORY_JIT);
   _suspendedThreadName = (char *) j9mem_allocate_memory(suspendedThreadNameLength, J9MEM_CATEGORY_JIT);

   if (_activeThreadName && _suspendedThreadName)
      {
      // NOTE:
      //       the (char *) casts are done because on Z, sprintf expects
      //       a (char *) instead of a (const char *)
      sprintf(_activeThreadName,    (char *) selectedActiveThreadName,    getCompThreadId());
      sprintf(_suspendedThreadName, (char *) selectedSuspendedThreadName, getCompThreadId());

      _initializationSucceeded = true;
      }

   _numJITCompilations = 0;
   _lastTimeThreadWasSuspended = 0;
   _lastTimeThreadWentToSleep = 0;
   _serverVM = NULL;
   _sharedCacheServerVM = NULL;

   if (compInfo.getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER)
      {
      _classesThatShouldNotBeNewlyExtended = new (PERSISTENT_NEW) PersistentUnorderedSet<TR_OpaqueClassBlock*>(
         PersistentUnorderedSet<TR_OpaqueClassBlock*>::allocator_type(TR::Compiler->persistentAllocator()));
      }
   else
      {
      _classesThatShouldNotBeNewlyExtended = NULL;
      }
   }

TR::CompilationInfo::CompilationInfo(J9JITConfig *jitConfig) :
#if defined(JITSERVER_SUPPORT)
   _sslKeys(decltype(_sslKeys)::allocator_type(TR::Compiler->persistentAllocator())),
   _sslCerts(decltype(_sslCerts)::allocator_type(TR::Compiler->persistentAllocator())),
#endif /* defined(JITSERVER_SUPPORT) */
   _persistentMemory(pointer_cast<TR_PersistentMemory *>(jitConfig->scratchSegment)),
   _sharedCacheReloRuntime(jitConfig),
   _samplingThreadWaitTimeInDeepIdleToNotifyVM(-1),
   _numDiagnosticThreads(0),
   _numCompThreads(0),
   _arrayOfCompilationInfoPerThread(NULL)
   {
   // The object is zero-initialized before this method is called
   //
   ::jitConfig = jitConfig;
   _jitConfig = jitConfig;

   // For normal case with compilation on application thread this
   // initialization will be done later after we are sure that
   // the options have been processed

   _vmStateOfCrashedThread = 0;

   _cachedFreePhysicalMemoryB = 0;
   _cachedIncompleteFreePhysicalMemory = false;
   OMRPORT_ACCESS_FROM_J9PORT(jitConfig->javaVM->portLibrary);
   _cgroupMemorySubsystemEnabled = (OMR_CGROUP_SUBSYSTEM_MEMORY == omrsysinfo_cgroup_are_subsystems_enabled(OMR_CGROUP_SUBSYSTEM_MEMORY));
   _suspendThreadDueToLowPhysicalMemory = false;

   // Initialize the compilation monitor
   //
   _compilationMonitor = TR::Monitor::create("JIT-CompilationQueueMonitor");
   _schedulingMonitor = TR::Monitor::create("JIT-SchedulingMonitor");
#if defined(J9VM_JIT_DYNAMIC_LOOP_TRANSFER)
   _dltMonitor = TR::Monitor::create("JIT-DLTmonitor");
#endif
   _iprofilerBufferArrivalMonitor = TR::Monitor::create("JIT-IProfilerBufferArrivalMonitor");
   _classUnloadMonitor = TR::MonitorTable::get()->getClassUnloadMonitor(); // by this time this variable is initialized
                                             // TODO: hang these monitors to something persistent
   _j9MonitorTable = TR::MonitorTable::get();

   _gpuInitMonitor = TR::Monitor::create("JIT-GpuInitializationMonitor");
   _persistentMemory->getPersistentInfo()->setGpuInitializationMonitor(_gpuInitMonitor);

   _iprofilerMaxCount = TR::Options::_maxIprofilingCountInStartupMode;

   PORT_ACCESS_FROM_JAVAVM(jitConfig->javaVM);
   _cpuUtil = 0; // Field will be set in onLoadInternal after option processing
   static char *verySmallQueue = feGetEnv("VERY_SMALL_QUEUE");
   if (verySmallQueue)
      {
      int temp = atoi(verySmallQueue);
      if (temp)
         VERY_SMALL_QUEUE = temp;
      }
   static char *smallQueue = feGetEnv("SMALL_QUEUE");
   if (smallQueue)
      {
      int temp = atoi(smallQueue);
      if (temp)
          SMALL_QUEUE = temp;
      }
   static char *mediumLargeQueue = feGetEnv("MEDIUM_LARGE_QUEUE");
   if (mediumLargeQueue)
      {
      int temp = atoi(mediumLargeQueue);
      if (temp)
         MEDIUM_LARGE_QUEUE = temp;
      }
   static char *largeQueue = feGetEnv("LARGE_QUEUE");
   if (largeQueue)
      {
      int temp = atoi(largeQueue);
      if (temp)
         LARGE_QUEUE = temp;
      }
   static char *veryLargeQueue = feGetEnv("VERY_LARGE_QUEUE");
   if (veryLargeQueue)
      {
      int temp = atoi(veryLargeQueue);
      if (temp)
         VERY_LARGE_QUEUE = temp;
      }
   statCompErrors.init("CompilationErrors", compilationErrorNames, 0);

   // Options might not exist at this point
   //
   static char *compTracing = feGetEnv("TR_CompTracing");
   if (compTracing)
      _compilationTracingFacility.initialize(256); // the size must be a power of two

   setSamplerState(TR::CompilationInfo::SAMPLER_NOT_INITIALIZED);

   setIsWarmSCC(TR_maybe);
   _cpuEntitlement.init(jitConfig);
   _lowPriorityCompilationScheduler.setCompInfo(this);
   _JProfilingQueue.setCompInfo(this);
   _interpSamplTrackingInfo = new (PERSISTENT_NEW) TR_InterpreterSamplingTracking(this);
   _clientSessionHT = NULL; // This will be set later when options are processed
   _unloadedClassesTempList = NULL;
   _sequencingMonitor = TR::Monitor::create("JIT-SequencingMonitor");
   _compReqSeqNo = 0;
   _newlyExtendedClasses = NULL;
   _chTableUpdateFlags = 0;
   }

bool TR::CompilationInfo::initializeCompilationOnApplicationThread()
   {
   if (!_compilationMonitor) return false;
   OMR::CriticalSection initializingCompInfoForAppThreads(_compilationMonitor);
   if (!_compInfoForCompOnAppThread)
      {
      PORT_ACCESS_FROM_JITCONFIG(_jitConfig);

      // NOTE: _numCompThreads is 0 now because it's not incremented in TR::CompilationInfoPerThreadBase constructor
      _compInfoForCompOnAppThread = new (PERSISTENT_NEW) TR::CompilationInfoPerThreadBase(*this, _jitConfig, 0, false);

      if (!_compInfoForCompOnAppThread)
         return false;
      _compInfoForCompOnAppThread->setCompilationThreadState(COMPTHREAD_ACTIVE);
      }
   return true;
   }

TR::CompilationInfoPerThreadBase *TR::CompilationInfo::getCompInfoWithID(int32_t ID)
   {
   if (_compInfoForCompOnAppThread)
      return _compInfoForCompOnAppThread;

   for (uint8_t i = 0; i < getNumTotalCompilationThreads(); i++)
      {
      TR::CompilationInfoPerThread *curCompThreadInfoPT = _arrayOfCompilationInfoPerThread[i];
      TR_ASSERT(curCompThreadInfoPT, "a thread's compinfo is missing\n");

      if (curCompThreadInfoPT->getCompThreadId() == ID)
         return curCompThreadInfoPT;
      }

   return NULL;
   }

// Must hold compilation queue monitor when calling this method
//
TR::CompilationInfoPerThread *TR::CompilationInfo::getFirstSuspendedCompilationThread()  // MCT
   {
   if (_compInfoForCompOnAppThread)
      return NULL;

   for (uint8_t i = 0; i < getNumUsableCompilationThreads(); i++)
      {
      TR::CompilationInfoPerThread *curCompThreadInfoPT = _arrayOfCompilationInfoPerThread[i];
      TR_ASSERT(curCompThreadInfoPT, "a thread's compinfo is missing\n");

      CompilationThreadState currentState = curCompThreadInfoPT->getCompilationThreadState();
      if (currentState == COMPTHREAD_SUSPENDED ||
          currentState == COMPTHREAD_SIGNAL_SUSPEND)
         return curCompThreadInfoPT;
      }

   return NULL;
   }

TR::Compilation *TR::CompilationInfo::getCompilationWithID(int32_t ID)
   {
   TR::CompilationInfoPerThreadBase *ciptb = getCompInfoWithID(ID);
   return ciptb ? ciptb->getCompilation() : NULL;
   }

void TR::CompilationInfo::setAllCompilationsShouldBeInterrupted()
   {
   if (_compInfoForCompOnAppThread)
      _compInfoForCompOnAppThread->setCompilationShouldBeInterrupted(GC_COMP_INTERRUPT);
   else
      {
      for (uint8_t i = 0; i < getNumUsableCompilationThreads(); i++)
         {
         TR::CompilationInfoPerThread *curCompThreadInfoPT = _arrayOfCompilationInfoPerThread[i];
         TR_ASSERT(curCompThreadInfoPT, "a thread's compinfo is missing\n");

         curCompThreadInfoPT->setCompilationShouldBeInterrupted(GC_COMP_INTERRUPT);
         }
      }

   _lastCompilationsShouldBeInterruptedTime = getPersistentInfo()->getElapsedTime(); // RAS
   }

bool TR::CompilationInfo::useSeparateCompilationThread()
   {
   if (!TR::Options::getCmdLineOptions()->getOption(TR_AOT))
      return !TR::Options::getCmdLineOptions()->getOption(TR_DisableCompilationThread);
   return TR::Options::getCmdLineOptions()->getOption(TR_EnableCompilationThread);
   }

bool TR::CompilationInfo::asynchronousCompilation()
   {
   static bool answer = (!TR::Options::getJITCmdLineOptions()->getOption(TR_DisableAsyncCompilation) &&
                        useSeparateCompilationThread() &&
                        TR::Options::getJITCmdLineOptions()->getInitialBCount() &&
                        TR::Options::getJITCmdLineOptions()->getInitialCount() &&
                        TR::Options::getAOTCmdLineOptions()->getInitialSCount() &&
                        TR::Options::getAOTCmdLineOptions()->getInitialBCount() &&
                        TR::Options::getAOTCmdLineOptions()->getInitialCount()
                        );
   return answer;
   }

TR_YesNoMaybe TR::CompilationInfo::detectCompThreadStarvation()
   {
   // If compilation queue is small, then we don't care about comp thread starvation
   if (getOverallQueueWeight() < TR::Options::_queueWeightThresholdForStarvation)
      return TR_no;
   // If compilation threads already consume too much CPU, do not throttle application threads
   if (exceedsCompCpuEntitlement() != TR_no)
      return TR_no;
   // If the state is IDLE we cannot starve the compilation threads
   //if (getPersistentInfo()->getJitState() == IDLE_STATE)
   //   return TR_no;

   // If there are idle cycles on the CPU set where this JVM can run
   // then the compilation threads should be able to use those cycles
   // The following is just an approximation because we look at idle
   // cyles on the entire machine
   if (getCpuUtil()->isFunctional() &&
      getCpuUtil()->getCpuIdle() > 5 && // This is for the entire machine
      getCpuUtil()->getVmCpuUsage() + 10 < getJvmCpuEntitlement()) // at least 10% unutilized by this JVM
      return TR_no;

   // Large queue and small CPU utilization for the compilation thread
   // is a sign of compilation thread starvation
   // TODO: augment the logic with CPU utilization and number of CPUs allowed to use

   // Find compilation threads that are not suspended and look at their CPU utilization
   // NOTE: normally we need to acquire the compQMonitor when looking at comp thread state
   // Here we allow a small imprecision because it's just a heuristic
   // We count on the fact that threads never get deleted
   int32_t totalCompCpuUtil = 0;
   int32_t numActive = 0;
   TR_YesNoMaybe answer = TR_maybe;
   bool compCpuFunctional = true;
   for (int32_t compId = 0; compId < _numCompThreads; compId++)
      {
      // We must look at all active threads because we want to avoid the
      // case where they compete with each other (4 comp threads on a single processor)
      //
      TR::CompilationInfoPerThread *compInfoPT = _arrayOfCompilationInfoPerThread[compId];
      TR_ASSERT(compInfoPT, "compInfoPT must exist because we don't destroy compilation threads");
      if (compInfoPT->compilationThreadIsActive())
         {
         numActive++;
         int32_t compCpuUtil = compInfoPT->getCompThreadCPU().getThreadLastCpuUtil();
         if (compCpuUtil >= 0) // when not functional or too soon to tell the answer is negative
            {
            totalCompCpuUtil += compCpuUtil;
            if (compCpuUtil >= TR::Options::_cpuUtilThresholdForStarvation) // 25%
               answer = TR_no; // Good utilization for at least one thread; no reason to make app threads sleep
            }
         else
            {
            compCpuFunctional = false;
            }
         }
      }
   _totalCompThreadCpuUtilWhenStarvationComputed = totalCompCpuUtil; // RAS
   _numActiveCompThreadsWhenStarvationComputed = numActive; // RAS


   if (answer == TR_maybe && compCpuFunctional) // Didn't reach a conclusion yet
      {
      if (getCpuUtil()->isFunctional())
         {
         // If the CPU utilization of all the active compilation threads is at least
         // half of the JVM utilization of the JVM, then compilation threads are not starved
         if ((totalCompCpuUtil << 1) >= getCpuUtil()->getVmCpuUsage())
            {
            answer = TR_no;
            }
         else // If all compilation threads are using less than 75% CPU then we declare starvation
            {
            if (totalCompCpuUtil < 75)
               answer = TR_yes;
            }
         }
      }
   return answer;
   }

// This should be called only if detectCompThreadStarvation returns TR_yes
int32_t TR::CompilationInfo::computeAppSleepNano() const
   {
   int32_t QW = getOverallQueueWeight();
   if (QW < TR::Options::_queueWeightThresholdForAppThreadYield)
      return 0;

   // I don't like to throttle application threads if I can still activate comp threads.
   int32_t numCompThreadsSuspended = getNumUsableCompilationThreads() - getNumCompThreadsActive();
   if (numCompThreadsSuspended > 0)
      return 0;

   if (QW < 4*TR::Options::_queueWeightThresholdForAppThreadYield)
      return (250000*(QW/TR::Options::_queueWeightThresholdForAppThreadYield));
   else return 1000000;
   }

void TR::CompilationInfo::vlogAcquire()
   {
   if (!_vlogMonitor)
      _vlogMonitor = TR::Monitor::create("JIT-VerboseLogMonitor");

   if (_vlogMonitor)
      _vlogMonitor->enter();
   }

void TR::CompilationInfo::vlogRelease()
   {
   if (_vlogMonitor)
      _vlogMonitor->exit();
   }

void TR::CompilationInfo::rtlogAcquire()
   {
   if (!_rtlogMonitor)
      _rtlogMonitor = TR::Monitor::create("JIT-RunTimeLogMonitor");

   if (_rtlogMonitor)
      _rtlogMonitor->enter();
   }

void TR::CompilationInfo::rtlogRelease()
   {
   if (_rtlogMonitor)
      _rtlogMonitor->exit();
   }

const char * TR::CompilationInfo::OperationNames[] =
   {
   "EMPTY",
   "HasAcquiredCompilationMonitor",
   "WillReleaseCompilationMonitor",
   "WillNotifyCompilationMonitor",
   "WillWaitOnCompilationMonitor",
   "HasFinishedWaitingOnCompilationMonitor",
   "Changed state",
   "WillWaitOnSlotMonitorAfterCompMonRelease",
   "CompileOnSeparateThreadEnter",
   "INVALID"
   };

void TR::CompilationInfo::addCompilationTraceEntry(J9VMThread * vmThread, TR_CompilationOperations op, uint32_t otherData)
   {
   if (compTracingEnabled())
      {
      _compilationTracingFacility.addNewEntry(vmThread, op, (uint32_t)otherData);
      }
   }


void TR::CompilationInfo::acquireCompMonitor(J9VMThread *vmThread) // used when we know we have a compilation monitor
   {
   getCompilationMonitor()->enter();
   addCompilationTraceEntry(vmThread, OP_HasAcquiredCompilationMonitor);
   }

void TR::CompilationInfo::releaseCompMonitor(J9VMThread *vmThread) // used when we know we have a compilation monitor
   {
   addCompilationTraceEntry(vmThread, OP_WillReleaseCompilationMonitor);
   getCompilationMonitor()->exit();
   }

void TR::CompilationInfo::waitOnCompMonitor(J9VMThread *vmThread)
   {
   addCompilationTraceEntry(vmThread, OP_WillWaitOnCompilationMonitor);
   getCompilationMonitor()->wait();
   addCompilationTraceEntry(vmThread, OP_HasFinishedWaitingOnCompilationMonitor);
   }

intptr_t TR::CompilationInfo::waitOnCompMonitorTimed(J9VMThread *vmThread, int64_t millis, int32_t nanos)
   {
   intptr_t retCode;
   addCompilationTraceEntry(vmThread, OP_WillWaitOnCompilationMonitor);
   retCode = getCompilationMonitor()->wait_timed(millis, nanos);
   addCompilationTraceEntry(vmThread, OP_HasFinishedWaitingOnCompilationMonitor);
   return retCode;
   }

void TR::CompilationInfo::acquireCompilationLock()
   {
   if (useSeparateCompilationThread() && _compilationMonitor)
      {
      acquireCompMonitor(0);
      debugPrint("\t\texternal user acquiring compilation monitor\n");
      }
   }

void TR::CompilationInfo::releaseCompilationLock()
   {
   if (useSeparateCompilationThread() && _compilationMonitor)
      {
      debugPrint("\t\texternal user releasing compilatoin monitor\n");
      releaseCompMonitor(0);
      }
   }

TR::Monitor * TR::CompilationInfo::createLogMonitor()
   {
   _logMonitor = TR::Monitor::create("JIT-LogMonitor");
   return _logMonitor;
   }

void TR::CompilationInfo::acquireLogMonitor()
   {
   if (_logMonitor)
      {
      _logMonitor->enter();
      }
   }

void TR::CompilationInfo::releaseLogMonitor()
   {
   if ( _logMonitor)
      {
      _logMonitor->exit();
      }
   }

uint32_t
TR::CompilationInfo::getMethodBytecodeSize(const J9ROMMethod * romMethod)
   {
   return (romMethod->bytecodeSizeHigh << 16) + romMethod->bytecodeSizeLow;
   }

JITServer::ServerStream *
TR::CompilationInfo::getStream()
   {
   return (TR::compInfoPT) ? TR::compInfoPT->getStream() : NULL;
   }

uint32_t
TR::CompilationInfo::getMethodBytecodeSize(J9Method* method)
   {
   if (auto stream = TR::CompilationInfo::getStream())
      {
         {
         ClientSessionData *clientSessionData = TR::compInfoPT->getClientData();
         OMR::CriticalSection getRemoteROMClass(clientSessionData->getROMMapMonitor());
         auto it = clientSessionData->getJ9MethodMap().find(method);
         if (it != clientSessionData->getJ9MethodMap().end())
            {
            return getMethodBytecodeSize(it->second._romMethod);
            }
         }
      stream->write(JITServer::MessageType::CompInfo_getMethodBytecodeSize, method);
      return std::get<0>(stream->read<uint32_t>());
      }
   return getMethodBytecodeSize(J9_ROM_METHOD_FROM_RAM_METHOD(method));
   }

bool
TR::CompilationInfo::isJSR292(const J9ROMMethod *romMethod) // Check to see if the J9AccMethodHasMethodHandleInvokes flag is set
   {
   return (_J9ROMMETHOD_J9MODIFIER_IS_SET(romMethod, J9AccMethodHasMethodHandleInvokes));
   }

bool
TR::CompilationInfo::isJSR292(J9Method *j9method) // Check to see if the J9AccMethodHasMethodHandleInvokes flag is set
   {
   if (auto stream = TR::CompilationInfo::getStream())
      {
         {
         ClientSessionData *clientSessionData = TR::compInfoPT->getClientData();
         OMR::CriticalSection getRemoteROMClass(clientSessionData->getROMMapMonitor());
         auto it = clientSessionData->getJ9MethodMap().find(j9method);
         if (it != clientSessionData->getJ9MethodMap().end())
            {
            return isJSR292(it->second._romMethod);
            }
         }
      stream->write(JITServer::MessageType::CompInfo_isJSR292, j9method);
      return std::get<0>(stream->read<bool>());
      }
   return isJSR292(J9_ROM_METHOD_FROM_RAM_METHOD(j9method));
   }

#if defined(J9VM_INTERP_AOT_COMPILE_SUPPORT) && defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM))
void
TR::CompilationInfo::disableAOTCompilations()
      {
      TR::Options::getAOTCmdLineOptions()->setOption(TR_NoStoreAOT, true);
      TR_DataCacheManager::getManager()->startupOver();
      }
#endif


#if defined(J9VM_INTERP_AOT_RUNTIME_SUPPORT) && defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))

// Note: this method must be called only when we know that AOT mode for shared classes is enabled !!
bool TR::CompilationInfo::isRomClassForMethodInSharedCache(J9Method *method, J9JavaVM *javaVM)
   {
   J9ROMClass *romClass = J9_CLASS_FROM_METHOD(method)->romClass;
   return _sharedCacheReloRuntime.isRomClassForMethodInSharedCache(method, javaVM) ? true : false;
   }

TR_YesNoMaybe TR::CompilationInfo::isMethodInSharedCache(J9Method *method, J9JavaVM *javaVM)
   {
   if (isRomClassForMethodInSharedCache(method, javaVM))
      return TR_maybe;
   else
      return TR_no;
   }
#endif

// Print the entire qualified name of the given method to the vlog
// Caller must acquire vlog mutex
//
void TR::CompilationInfo::printMethodNameToVlog(J9Method *method)
   {
   J9UTF8 *className;
   J9UTF8 *name;
   J9UTF8 *signature;
   getClassNameSignatureFromMethod(method, className, name, signature);
   TR_VerboseLog::write("%.*s.%.*s%.*s", J9UTF8_LENGTH(className), (char *) J9UTF8_DATA(className),
                                         J9UTF8_LENGTH(name), (char *) J9UTF8_DATA(name),
                                         J9UTF8_LENGTH(signature), (char *) J9UTF8_DATA(signature));
   }

void TR::CompilationInfo::printMethodNameToVlog(const J9ROMClass* romClass, const J9ROMMethod* romMethod)
   {
   J9UTF8 * className = J9ROMCLASS_CLASSNAME(romClass);
   J9UTF8 * name = J9ROMMETHOD_NAME(romMethod);
   J9UTF8 * signature = J9ROMMETHOD_SIGNATURE(romMethod);
   TR_VerboseLog::write("%.*s.%.*s%.*s", J9UTF8_LENGTH(className), (char *)J9UTF8_DATA(className),
      J9UTF8_LENGTH(name), (char *)J9UTF8_DATA(name),
      J9UTF8_LENGTH(signature), (char *)J9UTF8_DATA(signature));
   }


//----------------------- Must hide these in TR::CompilationInfo ------
#include "env/CpuUtilization.hpp"
#include "infra/Statistics.hpp"
#undef STATS
#ifdef STATS
TR_Stats statBudgetEpoch("Budget len at begining of epoch");
TR_Stats statBudgetSmallLag("Budget/comp when smallLag");
TR_Stats statBudgetMediumLag("Budget/comp when mediumLag");
TR_Stats statEpochLength("Epoch lengh (ms)");
char *eventNames[]={"SyncReq", "SmallLag", "LargeLag", "Medium-NoBudget", "Medium-highBudget", "Medium-LowBudget", "Medium-Idle"};
TR_StatsEvents<7> statEvents("Scenarios", eventNames, 0);
char *priorityName[]={"High","Normal"};
TR_StatsEvents<2> statLowPriority("Medium-lowBudget-priority",priorityName,0);
TR_Stats statLowBudget("Medium-lowBudget-budget");
TR_Stats statQueueSize("Size of Compilation queue");
TR_Stats statOverhead("Overhead of dynamicThreadPriority in cycles");
#endif


//---------------------- getCompilationLag --------------------------------
// Looks at 2 things: size of method queue and the waiting time for the
// current request to be processed.
// Returns a compDelay code: LARGE_LAG, MEDIUM_LAG, SMALL_LAG
//--------------------------------------------------------------------------
int32_t TR::CompilationInfo::getCompilationLag()
   {
   if (getMethodQueueSize() > TR::CompilationInfo::LARGE_QUEUE)
      return TR::CompilationInfo::LARGE_LAG;

   if (getMethodQueueSize() < TR::CompilationInfo::SMALL_QUEUE)
      return TR::CompilationInfo::SMALL_LAG;

   return TR::CompilationInfo::MEDIUM_LAG;
   }

//------------------------ changeCompThreadPriority ------------------
int32_t TR::CompilationInfoPerThread::changeCompThreadPriority(int32_t newPriority, int32_t locationCode)
   {
   // once in a while test that _compThreadPriority is OK;
   // we need this because the code is not thread safe

   static uint32_t numInvocations = 0;
   if (((++numInvocations) & 0xf) == 0)
      {
      _compThreadPriority = j9thread_get_priority(getOsThread());
      }
   int32_t oldPriority = _compThreadPriority;
   // FIXME: minimize multithreading issues
   if (oldPriority != newPriority)
      {
      j9thread_set_priority(getOsThread(), newPriority);
      _compThreadPriority = newPriority;
      _compInfo._statNumPriorityChanges++;
#ifdef STATS
      fprintf(stderr, "Changed priority to %d, location=%d\n", newPriority, locationCode);
#endif
      }

   return oldPriority;
   }

//------------------------ dynamicThreadPriority -------------------
bool TR::CompilationInfo::dynamicThreadPriority()
   {
   static bool answer = TR::Options::getCmdLineOptions()->getOption(TR_DynamicThreadPriority) &&
      asynchronousCompilation() && TR::Compiler->target.numberOfProcessors() < 4;
   return answer;
   }


// Needs compilation monitor in hand
//----------------------------- enqueueCompReqToLPQ ------------------------
void TR_LowPriorityCompQueue::enqueueCompReqToLPQ(TR_MethodToBeCompiled *compReq)
   {
   // add at the end of queue
   if (_lastLPQentry)
      _lastLPQentry->_next = compReq;
   else
      _firstLPQentry = compReq;

   _lastLPQentry = compReq;
   _sizeLPQ++; // increase the size of LPQ
   increaseLPQWeightBy(compReq->_weight);
   }

//---------------------------- createLowPriorityCompReqAndQueueIt ---------------------
bool TR_LowPriorityCompQueue::createLowPriorityCompReqAndQueueIt(TR::IlGeneratorMethodDetails &details, void *startPC, uint8_t reason)
   {
   TR_OptimizationPlan *plan = TR_OptimizationPlan::alloc(warm); // TODO: pick first opt level
   if (!plan)
      return false; // OOM

   TR_MethodToBeCompiled * compReq = _compInfo->getCompilationQueueEntry();
   if (!compReq)
      {
      TR_OptimizationPlan::freeOptimizationPlan(plan);
      return false; // OOM
      }
   compReq->initialize(details, 0, CP_ASYNC_ABOVE_MIN, plan);
   compReq->_reqFromSecondaryQueue = reason;
   compReq->_jitStateWhenQueued = _compInfo->getPersistentInfo()->getJitState();
   compReq->_oldStartPC = startPC;
   compReq->_async = true; // app threads are not waiting for it



   // Determine entry weight
   J9Method *j9method = details.getMethod();
   J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(j9method);
   compReq->_weight = (J9ROMMETHOD_HAS_BACKWARDS_BRANCHES(romMethod)) ? TR::CompilationInfo::WARM_LOOPY_WEIGHT : TR::CompilationInfo::WARM_LOOPLESS_WEIGHT;
   // add at the end of queue
   enqueueCompReqToLPQ(compReq);
   incStatsReqQueuedToLPQ(reason);
   return true;
   }

//------------------------ addFirstTimeReqToLPQ ---------------------
bool TR_LowPriorityCompQueue::addFirstTimeCompReqToLPQ(J9Method *j9method, uint8_t reason)
   {
   if (TR::CompilationInfo::isCompiled(j9method))
      return false;
   TR::IlGeneratorMethodDetails details(j9method);
   return createLowPriorityCompReqAndQueueIt(details, NULL, reason);
   }


//------------------------ addUpgradeReqToLPQ ----------------------
// This method is used when the JIT performs a low optimized compilation
// and then schedules a low priority upgrade compilation for the same method
//------------------------------------------------------------------
bool TR_LowPriorityCompQueue::addUpgradeReqToLPQ(TR_MethodToBeCompiled *compReq)
   {
   TR_ASSERT(!compReq->getMethodDetails().isNewInstanceThunk(), "classForNewInstance is sync req");
   // filter out DLT compilations and fixed opt level situations
   if (compReq->isDLTCompile() || !TR::Options::getCmdLineOptions()->allowRecompilation())
      return false;
   return createLowPriorityCompReqAndQueueIt(compReq->getMethodDetails(), compReq->_newStartPC, TR_MethodToBeCompiled::REASON_UPGRADE);
   }

bool TR::CompilationInfo::canProcessLowPriorityRequest()
   {
   // To avoid overhead cycling through all the threads, we should first
   // check if there is anything in that low priority queue
   //
   if (!getLowPriorityCompQueue().hasLowPriorityRequest()) // access _firstLPQentry without monitor in hand
      return false;

   // If the main compilation queue has requests, we should serve those first
   if (getMethodQueueSize() != 0)
      return false;

   // To process a request from the low priority queue we need to have
   // (1) no other compilation in progress (not required if TR_ConcurrentLPQ is enabled)
   // (2) some idle CPU
   // (3) current JVM not taking its entire share of CPU entitlement
   //
   if (TR::Options::getCmdLineOptions()->getOption(TR_ConcurrentLPQ) &&
       _jitConfig->javaVM->phase == J9VM_PHASE_NOT_STARTUP) // ConcurrentLPQ is too damaging to startup
      {
      if ((getCpuUtil() && getCpuUtil()->isFunctional() &&
         getCpuUtil()->getCpuIdle() > idleThreshold() && // enough idle time
         getJvmCpuEntitlement() - getCpuUtil()->getVmCpuUsage() >= 200)) // at least 2 cpus should be not utilized
         return true;
      // Fall through the next case where we allow LPQ if the JVM leaves 50% of a CPU
      // unutilized, but there should be nothing in the main queue
      }

   // Cycle through all the compilation threads
   for (uint8_t i = 0; i < getNumUsableCompilationThreads(); i++)
      {
      TR::CompilationInfoPerThread *curCompThreadInfoPT = _arrayOfCompilationInfoPerThread[i];
      TR_ASSERT(curCompThreadInfoPT, "a thread's compinfo is missing\n");

      if (curCompThreadInfoPT->getMethodBeingCompiled())
         return false;
      }
   return (getCpuUtil() && getCpuUtil()->isFunctional() &&
      getCpuUtil()->getCpuIdle() > idleThreshold() && // enough idle time
           getJvmCpuEntitlement() - getCpuUtil()->getVmCpuUsage() > 50); // at least half a processor should be empty
   }

int64_t
TR::CompilationInfo::getCpuTimeSpentInCompilation()
   {
   I_64 totalTime = 0;
   for (uint8_t i = 0; i < getNumUsableCompilationThreads(); i++)
      {
      TR::CompilationInfoPerThread *curCompThreadInfoPT = _arrayOfCompilationInfoPerThread[i];
      TR_ASSERT(curCompThreadInfoPT, "a thread's compinfo is missing\n");

      totalTime += j9thread_get_cpu_time(curCompThreadInfoPT->getOsThread());
      }

   return totalTime;
   }

int32_t
TR::CompilationInfo::changeCompThreadPriority(int32_t priority, int32_t locationCode)
   {
   fprintf(stderr, "Look at this code again\n");
   return 0;
   }




//--------------------- SmoothCompilation ------------------
// How to improve: move all this computation outside the monitor
// Do not lower the priority when inside the monitor
// Minimize the number of priority changes by histerezis
//
// TODO: Does any of the calls in here require VM access?
//----------------------------------------------------------
bool
TR::CompilationInfo::SmoothCompilation(TR_MethodToBeCompiled *entry, int32_t *optLevelAdjustment)
   {
   bool shouldAddRequestToUpgradeQueue = false;

   if (shouldDowngradeCompReq(entry))
      {
      *optLevelAdjustment = -1; // decrease opt level
      _statNumDowngradeInterpretedMethod++;
      if (TR::Options::getCmdLineOptions()->getOption(TR_UseLowPriorityQueueDuringCLP) && !entry->isJNINative())
         shouldAddRequestToUpgradeQueue = true;
      }
   else
      {
      *optLevelAdjustment = 0; // unchanged opt level (default)
      }

   if (shouldAddRequestToUpgradeQueue && entry->getMethodDetails().isNewInstanceThunk())
      shouldAddRequestToUpgradeQueue = false;
   return shouldAddRequestToUpgradeQueue;
   }

int32_t TR::CompilationInfo::getCompBudget() const
   {
   return _compilationBudget * (int32_t)TR::Compiler->target.numberOfProcessors();
   }

void
TR::CompilationInfo::decNumGCRReqestsQueued(TR_MethodToBeCompiled *entry)
   {
   if (entry->_GCRrequest) _numGCRQueued--;
   }

void
TR::CompilationInfo::incNumGCRRequestsQueued(TR_MethodToBeCompiled *entry)
   {
   entry->_GCRrequest = true; _numGCRQueued++;
   }

void
TR::CompilationInfo::decNumInvReqestsQueued(TR_MethodToBeCompiled *entry)
   {
   if (entry->_entryIsCountedAsInvRequest)
      {
      _numInvRequestsInCompQueue--;
      TR_ASSERT(_numInvRequestsInCompQueue >= 0, "_numInvRequestsInCompQueue is negative : %d", _numInvRequestsInCompQueue);
      }
   }

void
TR::CompilationInfo::incNumInvRequestsQueued(TR_MethodToBeCompiled *entry)
   {
   entry->_entryIsCountedAsInvRequest = true; _numInvRequestsInCompQueue++;
   }

void
TR::CompilationInfo::updateCompQueueAccountingOnDequeue(TR_MethodToBeCompiled *entry)
   {
   _numQueuedMethods--; // one less method in the queue
   decNumGCRReqestsQueued(entry);
   decNumInvReqestsQueued(entry);
   if (entry->getMethodDetails().isOrdinaryMethod() && entry->_oldStartPC==0)
      {
      _numQueuedFirstTimeCompilations--;
      TR_ASSERT(_numQueuedFirstTimeCompilations >= 0, "_numQueuedFirstTimeCompilations is negative : %d", _numQueuedFirstTimeCompilations);
      }
   // Note: queue weight is handled separately because a method that is currently being
   // compiled is considered as bringing some weight to the processing backlog
   }


//----------------------- useOptLevelAdjustment ---------------------
// This scheme will be used only on multiprocessors because it is
// based on compilation request queue size. On uniprocessors this
// queue is always very small because, as soon as a request is put in
// the queue, the compilation thread (working at MAX priority) will
// occupy the CPU until compilation finishes
//-------------------------------------------------------------------
bool TR::CompilationInfo::useOptLevelAdjustment()
   {
   static bool answer = TR::Options::getCmdLineOptions()->getOption(TR_UseOptLevelAdjustment) &&
                        asynchronousCompilation() &&
                        TR::Options::getCmdLineOptions()->allowRecompilation();
   return answer;
   }


// Must have compilation monitor in hand when calling this function
void TR::CompilationInfo::invalidateRequestsForNativeMethods(J9Class * clazz, J9VMThread * vmThread)
   {
   bool verbose = TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseHooks);
   if (verbose)
      TR_VerboseLog::writeLineLocked(TR_Vlog_HK, "invalidateRequestsForNativeMethods class=%p vmThread=%p", clazz, vmThread);

   // Cycle through all the compilation threads
   for (uint8_t i = 0; i < getNumUsableCompilationThreads(); i++)
      {
      TR::CompilationInfoPerThread *curCompThreadInfoPT = _arrayOfCompilationInfoPerThread[i];
      TR_MethodToBeCompiled *methodBeingCompiled = curCompThreadInfoPT->getMethodBeingCompiled();
      // Mark the method being compiled that it has been unloaded.
      // If it is already marked, then there is nothing to do.
      //
      if (methodBeingCompiled && !methodBeingCompiled->_unloadedMethod)
         {
         J9Method *method = methodBeingCompiled->getMethodDetails().getMethod();
         if (method)
            {
            J9Class *classOfMethod = J9_CLASS_FROM_METHOD(method);
            if (classOfMethod == clazz &&  TR::CompilationInfo::isJNINative(method))
               {
               // fail this compilation
               if (methodBeingCompiled->_priority < CP_SYNC_MIN) // ASYNC compilation, typical case
                  {
                  TR_ASSERT(methodBeingCompiled->_numThreadsWaiting == 0, "Async compilations should not have threads waiting");
                  methodBeingCompiled->_newStartPC = 0;
                  }
               else
                  {
                  methodBeingCompiled->acquireSlotMonitor(vmThread); // get queue-slot monitor
                  methodBeingCompiled->_newStartPC = 0;
                  methodBeingCompiled->getMonitor()->notifyAll(); // notify the waiting thread
                  methodBeingCompiled->releaseSlotMonitor(vmThread); // release queue-slot monitor
                  }
               methodBeingCompiled->_unloadedMethod = true;
               if (verbose)
                  TR_VerboseLog::writeLineLocked(TR_Vlog_HK, "Have marked as unloaded the JNI thunk compilation for method %p", method);
               }
            }
         }
      } // end for

   // if compiling on app thread, there is no compilation queue
   TR_MethodToBeCompiled *cur = _methodQueue;
   TR_MethodToBeCompiled *prev = NULL;
   while (cur)
      {
      TR_MethodToBeCompiled *next = cur->_next;
      J9Method *method = cur->getMethodDetails().getMethod();
      if (method &&
          cur->getMethodDetails().getClass() == clazz &&
          TR::CompilationInfo::isJNINative(method))
         {
         if (verbose)
            TR_VerboseLog::writeLineLocked(TR_Vlog_HK, "Invalidating JNI thunk compile request for method %p class %p", method, J9_CLASS_FROM_METHOD(method));
         if (cur->_priority >= CP_SYNC_MIN)
            {
            // squash this compilation request but inform waiting threads
            cur->acquireSlotMonitor(vmThread); // get queue-slot monitor
            cur->_newStartPC = 0;
            cur->getMonitor()->notifyAll(); // notify the waiting thread
            cur->releaseSlotMonitor(vmThread); // release queue-slot monitor
            }
         else
            {
            TR_ASSERT(cur->_numThreadsWaiting == 0, "Async compilations should not have threads waiting");
            }

         // detach from queue
         if (prev)
            prev->_next = cur->_next;
         else
            _methodQueue = cur->_next;
         updateCompQueueAccountingOnDequeue(cur);
         // decrease the queue weight
         decreaseQueueWeightBy(cur->_weight);
         // put back into the pool
         recycleCompilationEntry(cur);
         }
      else
         {
         prev = cur;
         }
      cur = next;
      }
   // LPQ does not need to be checked because JNI thunk requests cannot be put in LPQ
   }

void TR::CompilationInfo::invalidateRequestsForUnloadedMethods(TR_OpaqueClassBlock * clazz, J9VMThread * vmThread, bool hotCodeReplacement)
   {
   TR_J9VMBase * fe = TR_J9VMBase::get(_jitConfig, vmThread);
   J9Class *unloadedClass = clazz ? TR::Compiler->cls.convertClassOffsetToClassPtr(clazz) : NULL;
   bool verbose = TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseHooks);
   if (verbose)
      TR_VerboseLog::writeLineLocked(TR_Vlog_HK, "invalidateRequestsForUnloadedMethods class=%p vmThread=%p hotCodeReplacement=%d", clazz, vmThread, hotCodeReplacement);

   for (uint8_t i = 0; i < getNumUsableCompilationThreads(); i++)
      {
      TR::CompilationInfoPerThread *curCompThreadInfoPT = _arrayOfCompilationInfoPerThread[i];
      TR_ASSERT(curCompThreadInfoPT, "a thread's compinfo is missing\n");

      TR_MethodToBeCompiled *methodBeingCompiled = curCompThreadInfoPT->getMethodBeingCompiled();
      // Mark the method being compiled that it has been unloaded.
      // If it is already marked, then there is nothing to do.
      //
      if (methodBeingCompiled && !methodBeingCompiled->_unloadedMethod)
         {
         TR::IlGeneratorMethodDetails &details = methodBeingCompiled->getMethodDetails();
         J9Method *method = details.getMethod();
         TR_ASSERT(method, "_method can be NULL only at shutdown time");
         if ((!unloadedClass && hotCodeReplacement) || // replacement in FSD mode
             J9_CLASS_FROM_METHOD(method) == unloadedClass ||
             details.isNewInstanceThunk() &&
             static_cast<J9::NewInstanceThunkDetails &>(details).isThunkFor(unloadedClass))
            {
            if (!hotCodeReplacement)
               {
               TR_ASSERT(methodBeingCompiled->_priority < CP_SYNC_MIN,
                      "Unloading cannot happen for SYNC requests");
               }
            else
               {
               TR_ASSERT(methodBeingCompiled->_numThreadsWaiting == 0 || methodBeingCompiled->_priority >= CP_SYNC_MIN,
                         "threads can be waiting only for sync compilations");
               if (methodBeingCompiled->_priority >= CP_SYNC_MIN)
                  {
                  // fail this compilation
                  methodBeingCompiled->acquireSlotMonitor(vmThread); // get queue-slot monitor
                  methodBeingCompiled->_newStartPC = 0;
                  methodBeingCompiled->getMonitor()->notifyAll(); // notify the waiting thread
                  methodBeingCompiled->releaseSlotMonitor(vmThread); // release queue-slot monitor
                  }
               }
            methodBeingCompiled->_unloadedMethod = true;
            }
         }
      } // end for
   // if compiling on app thread, there is no compilation queue
   TR_MethodToBeCompiled *cur  = _methodQueue;
   TR_MethodToBeCompiled *prev = NULL;
   bool verboseDetails = TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseHookDetails);
   while (cur)
      {
      TR_MethodToBeCompiled *next = cur->_next;
      TR::IlGeneratorMethodDetails &details = cur->getMethodDetails();
      J9Method *method = details.getMethod();
      //TR_ASSERT(details.getMethod(), "method can be NULL only at shutdown time");
      // ^^^ bad assume: shutdown and class unload CAN happen at the same time.
      if (method)
         {
         J9Class *methodClass = details.getClass();
         if (verboseDetails)
            TR_VerboseLog::writeLineLocked(TR_Vlog_HD,"Looking at compile request for method %p class %p", method, methodClass);
         if ((!unloadedClass && hotCodeReplacement) || // replacement in FSD mode
             methodClass == unloadedClass ||
             details.isNewInstanceThunk() &&
             static_cast<J9::NewInstanceThunkDetails &>(details).isThunkFor(unloadedClass))
            {
            if (verbose)
               TR_VerboseLog::writeLineLocked(TR_Vlog_HK,"Invalidating compile request for method %p class %p", method, methodClass);
            if (!hotCodeReplacement)
               {
               TR_ASSERT(cur->_priority < CP_SYNC_MIN,
                       "Unloading cannot happen for SYNC requests");
               }
            else
               {
               TR_ASSERT(cur->_numThreadsWaiting == 0 || cur->_priority >= CP_SYNC_MIN,
                         "threads can be waiting only for sync compilations");
               // if this method is synchronous we need to signal the waiting thread
               if (cur->_priority >= CP_SYNC_MIN)
                  {
                  // fail this compilation
                  cur->acquireSlotMonitor(vmThread); // get queue-slot monitor
                  cur->_newStartPC = 0;
                  cur->getMonitor()->notifyAll(); // notify the waiting thread
                  cur->releaseSlotMonitor(vmThread); // release queue-slot monitor
                  }
               }
            // detach from queue
            if (prev)
               prev->_next = cur->_next;
            else
               _methodQueue = cur->_next;
            updateCompQueueAccountingOnDequeue(cur);
            // decrease the queue weight
            decreaseQueueWeightBy(cur->_weight);
            // put back into the pool
            recycleCompilationEntry(cur);
            }
         else
            {
            prev = cur;
            }
         }
      else
         {
         prev = cur;
         }
      cur = next;
      }

   // process the low priority queue as well
   getLowPriorityCompQueue().invalidateRequestsForUnloadedMethods(unloadedClass);
   // and JProfiling queue ...
   getJProfilingCompQueue().invalidateRequestsForUnloadedMethods(unloadedClass);
   }

// Helper to determine if compilation needs to be aborted due to class unloading
// or hot code replacement
// Side-effects: may change entry->_compErrorCode
bool TR::CompilationInfo::shouldAbortCompilation(TR_MethodToBeCompiled *entry, TR::PersistentInfo *persistentInfo)
   {
   if (entry->isOutOfProcessCompReq())
      return false; // will abort at the client

   if (entry->_unloadedMethod) // method was unloaded while we were trying to compile it
      {
      TR_ASSERT(entry->_compErrCode == compilationInterrupted, "Received error code %u, expect compilationInterrupted when the method was unloaded", entry->_compErrCode);
      entry->_compErrCode = compilationNotNeeded; // change error code
      return true;
      }
   if ((TR::Options::getCmdLineOptions()->getOption(TR_EnableHCR) || TR::Options::getCmdLineOptions()->getOption(TR_FullSpeedDebug)))
      {
      TR::IlGeneratorMethodDetails & details = entry->getMethodDetails();
      J9Class *clazz = details.isNewInstanceThunk() ?
         static_cast<J9::NewInstanceThunkDetails &>(details).classNeedingThunk() :
         (J9Class*)J9JitMemory::convertClassPtrToClassOffset(J9_CLASS_FROM_METHOD(details.getMethod()));

      if (clazz && J9_IS_CLASS_OBSOLETE(clazz))
         {
         TR_ASSERT(0, "Should never have compiled replaced method %p", details.getMethod());
         entry->_compErrCode = compilationKilledByClassReplacement;
         return true;
         }
      }
   return false;
   }


// This method has side-effects, It modifies the optimization plan and persistentMethodInfo
// This method is executed with compilationMonitor in hand
//
bool TR::CompilationInfo::shouldRetryCompilation(TR_MethodToBeCompiled *entry, TR::Compilation *comp)
   {
   bool tryCompilingAgain = false;
   TR::IlGeneratorMethodDetails & details = entry->getMethodDetails();
   J9Method *method = details.getMethod();

   // The JITServer should not retry compilations on it's own,
   // it should let the client make that decision
   if (entry->isOutOfProcessCompReq())
      return false;

   // when the compilation fails we might retry
   //
   if (entry->_compErrCode != compilationOK &&
      useSeparateCompilationThread()) // Retrials for compilation on application thread are not implemented
      {
      if (entry->_compilationAttemptsLeft > 0)
         {

         TR_PersistentJittedBodyInfo *bodyInfo;
         switch (entry->_compErrCode)
            {
            case compilationAotValidateFieldFailure:
            case compilationAotStaticFieldReloFailure:
            case compilationAotClassReloFailure:
            case compilationAotThunkReloFailure:
            case compilationAotHasInvokehandle:
            case compilationAotHasInvokeVarHandle:
            case compilationAotValidateMethodExitFailure:
            case compilationAotValidateMethodEnterFailure:
            case compilationAotClassChainPersistenceFailure:
            case compilationAotValidateStringCompressionFailure:
            case compilationSymbolValidationManagerFailure:
            case compilationAOTNoSupportForAOTFailure:
            case compilationAOTValidateTMFailure:
               // switch to JIT for these cases (we don't want to relocate again)
               entry->_doNotUseAotCodeFromSharedCache = true;
               // if this is a remote request, fail the compilation here so that it is retried on the client.
               // there is no point in doing a jit compilation on the server as it will be useless to the client.
               tryCompilingAgain = true;
               break;
            //case compilationAotRelocationFailure:
            case compilationAotTrampolineReloFailure:
            case compilationAotPicTrampolineReloFailure:
            case compilationAotCacheFullReloFailure:
               // for the last one, switch to JIT
               if (entry->_compilationAttemptsLeft == 1)
                  entry->_doNotUseAotCodeFromSharedCache = true;
               // Intentionally fall through next case
            case compilationStreamFailure:
               // This feature TR_RequireJITServer is used when we would like the client to fail when server crashes
               if (feGetEnv("TR_RequireJITServer"))
                  {
                  TR_VerboseLog::writeLineLocked(TR_Vlog_FAILURE, "In mode TR_RequireJITServer, exit JITClient due to unavailable JITServer.");
                  exit(25);
                  }
            case compilationInterrupted:
            case compilationCodeReservationFailure:
            case compilationRecoverableTrampolineFailure:
            case compilationIllegalCodeCacheSwitch:
            case compilationRecoverableCodeCacheError:
            case compilationStreamMessageTypeMismatch:
            case compilationStreamVersionIncompatible:
               tryCompilingAgain = true;
               break;
            case compilationExcessiveComplexity:
            case compilationHeapLimitExceeded:
            case compilationLowPhysicalMemory:
            case compilationInternalPointerExceedLimit:
            case compilationVirtualAddressExhaustion:
               if (comp->getOption(TR_Timing))
                  {
                  comp->phaseTimer().DumpSummary(*comp);
                  }

               if (comp->getOption(TR_LexicalMemProfiler))
                  {
                  comp->phaseMemProfiler().DumpSummary(*comp);
                  }

               if (!((TR_J9VMBase *)comp->fej9())->isAOT_DEPRECATED_DO_NOT_USE())
                  {
                  TR_J9SharedCache *sc = (TR_J9SharedCache *) (((TR_J9VMBase *)comp->fej9())->sharedCache());
                  if (sc)
                     {
                     switch (entry->_optimizationPlan->getOptLevel())
                        {
                        case cold:
                        case warm:
                           sc->addHint(method, TR_HintFailedWarm);
                           break;
                        case hot:
                           sc->addHint(method, TR_HintFailedHot);
                           break;
                        case scorching:
                        case veryHot:
                           sc->addHint(method, TR_HintFailedScorching);
                           break;
                        default:
                           break;
                        }
                     }
                  }

               bodyInfo = 0;
               if (comp->allowRecompilation() && entry->_optimizationPlan && entry->_optimizationPlan->getOptLevel() > minHotness)
                  {
                  // Compile only if the method is interpreted or if it's compiled with
                  // profiling information.
                  if (entry->_oldStartPC == 0) // interpreter method
                     {
                     tryCompilingAgain = true;
                     }
                  else // Does the existing body contain profiling info?
                     {
                     bodyInfo = TR::Recompilation::getJittedBodyInfoFromPC(entry->_oldStartPC);
                     if (bodyInfo->getIsProfilingBody())
                        tryCompilingAgain = true;
                     // if existing body is invalidated, retry the compilation
                     else if (bodyInfo->getIsInvalidated())
                        tryCompilingAgain = true;
                     // if the existing body uses pre-existence, retry the compilation
                     // otherwise we will revert to interpreted when failing this compilation
                     else if (bodyInfo->getUsesPreexistence())
                        tryCompilingAgain = true;
                     }
                  }

               if (tryCompilingAgain)
                  {
                  TR_Hotness hotness = entry->_optimizationPlan->getOptLevel();
                  TR_Hotness newHotness;
                  if (hotness == veryHot)
                     newHotness = warm; // skip over hot and go down two levels
                  else if (hotness <= scorching)
                     newHotness = (TR_Hotness)(hotness - 1);
                  else // Why would we use hotness greater than scorching
                     newHotness = noOpt;
                  entry->_optimizationPlan->setOptLevel(newHotness);
                  entry->_optimizationPlan->setInsertInstrumentation(false); // prevent profiling
                  entry->_optimizationPlan->setUseSampling(false); // disable recompilation of this method
                  }
               break;
            case compilationCHTableCommitFailure:
               tryCompilingAgain = true;
               // if we have only one more trial left, disable optimizations based on CHTable
               if (entry->_compilationAttemptsLeft == 1)
                  entry->_optimizationPlan->setDisableCHOpts();
               break;
            case compilationNeededAtHigherLevel:
               if (comp->allowRecompilation() && (comp->getNextOptLevel()!=unknownHotness))
                  {
                  tryCompilingAgain = true;
                  TR_Hotness newHotness = comp->getNextOptLevel();
                  entry->_optimizationPlan->setOptLevel(newHotness);
                  }
               break;

            case compilationMaxCallerIndexExceeded:
               {
               tryCompilingAgain = true;
               // bump the factor that will indicate by how much to reduce the MAX_PEEKED_BYTECODE_SIZE
               uint32_t val = entry->_optimizationPlan->getReduceMaxPeekedBytecode();
               entry->_optimizationPlan->setReduceMaxPeekedBytecode(val+1);
               // if we have only one more trial left, make sure you don't try above warm opt level
               // Lookahead at warm is disabled by default
               if (entry->_compilationAttemptsLeft == 1)
                  {
                  TR_Hotness hotness = entry->_optimizationPlan->getOptLevel();
                  TR_Hotness newHotness = hotness > warm ? warm : hotness;
                  entry->_optimizationPlan->setOptLevel(newHotness);
                  entry->_optimizationPlan->setInsertInstrumentation(false); // prevent profiling
                  entry->_optimizationPlan->setUseSampling(false); // disable recompilation of this method
                  // In the future we may have to disable OptServer as well
                  }
               break;
               }
            case compilationGCRPatchFailure:
               // Disable GCR trees for this body
               entry->_optimizationPlan->setDisableGCR();
               tryCompilingAgain = true;
               break;
            case compilationLambdaEnforceScorching:
               // This must be a warm compilation. Retry the same compilation at scorching even if it's a first time compilation
               // Watch out for the following scenario: (1) warm comp req. (2) Ilgen fails comp because it wants a hot comp
               // (3) hot comp fails due to some compiler limit and we lower opt level at warm; (4) Ilgen fails again because
               // it wants a hot compilation
               // Also we should not fail the compilation for fixed opt level.
               if (entry->_compilationAttemptsLeft == MAX_COMPILE_ATTEMPTS)  // Raise opt level only for the first attempt
                  {
                  TR_Hotness hotness = entry->_optimizationPlan->getOptLevel();
                  if (hotness <= veryHot )
                     {
                     entry->_optimizationPlan->setOptLevel(scorching); // force scorching recompilation
                     entry->_optimizationPlan->setDontFailOnPurpose(true);
                     entry->_optimizationPlan->setDoNotSwitchToProfiling(true);
                     entry->_optimizationPlan->setDisableGCR();
                     tryCompilingAgain = true;
                     // If we were doing an AOT warm compilation, we will refrain from doing an AOT-hot
                     // because we do not allow AOT compilations as retrials
                     }
                  }
               break;
            case compilationEnforceProfiling:
               entry->_optimizationPlan->setInsertInstrumentation(true); // enable profiling
               entry->_optimizationPlan->setDoNotSwitchToProfiling(true); // don't allow another switch
               entry->_optimizationPlan->setDisableGCR(); // GCR isn't needed
               tryCompilingAgain = true;
               break;
            case compilationNullSubstituteCodeCache:
            case compilationCodeMemoryExhausted:
            case compilationCodeCacheError:
            case compilationDataCacheError:
            default:
               break;
            } // end switch
         if (tryCompilingAgain && comp)
            {
            TR_PersistentMethodInfo *methodInfo = TR_PersistentMethodInfo::get(comp);
            if (methodInfo)
               methodInfo->setNextCompileLevel(entry->_optimizationPlan->getOptLevel(), entry->_optimizationPlan->insertInstrumentation());
            }
         }
      else if (0) // later add test for multiple aot relocation failure to catch that scenario, which shouldn't happen
         {
         TR_ASSERT(0, "multiple aot relocations likely failed for same method");
         }
      }//  if (entry->_compErrCode != compilationOK)

   // don't carry the decision to generate AOT code to the next compilation
   // because it may no longer be the right thing to do at that point
   if (tryCompilingAgain)
      entry->_useAotCompilation = false;

   return tryCompilingAgain;
   }

 // if compiling on app thread, there is no compilation queue
void TR::CompilationInfo::purgeMethodQueue(TR_CompilationErrorCode errorCode)
   {
   // Must enter _compilationMonitor before calling this
   J9JavaVM   *vm       = _jitConfig->javaVM;
   J9VMThread *vmThread = vm->internalVMFunctions->currentVMThread(vm);
   // Generate a trace point
   Trc_JIT_purgeMethodQueue(vmThread);

   while (_methodQueue)
      {
      TR_MethodToBeCompiled * cur = _methodQueue;
      _methodQueue = cur->_next;
      updateCompQueueAccountingOnDequeue(cur);
      // decrease the queue weight
      decreaseQueueWeightBy(cur->_weight);

      // must wake up the working threads that are waiting for the
      // compilation to finish (only for sync comp req)
      // we also need to fail all those compilations

      // Acquire the queue slot monitor now
      //
      debugPrint(vmThread, "\tre-acquiring queue-slot monitor\n");
      cur->acquireSlotMonitor(vmThread);
      debugPrint(vmThread, "+AM-", cur);

      // fail the compilation
      void *startPC = 0;

      startPC = compilationEnd(vmThread, cur->getMethodDetails(), _jitConfig, NULL, cur->_oldStartPC);
      cur->_newStartPC = startPC;
      cur->_compErrCode = errorCode;

      // notify the sleeping threads
      //
      debugPrint(vmThread, "\tnotify sleeping threads that the compilation is done\n");
      cur->getMonitor()->notifyAll();
      debugPrint(vmThread, "ntfy-", cur);

      // Release the queue slot monitor
      //
      debugPrint(vmThread, "\tcompilation thread releasing queue slot monitor\n");
      debugPrint(vmThread, "-AM-", cur);
      cur->releaseSlotMonitor(vmThread);

      recycleCompilationEntry(cur);
      }

   // delete all entries from the low priority queue
   getLowPriorityCompQueue().purgeLPQ();
   // and from JProfiling queue
   getJProfilingCompQueue().purge();
   }

void TR::CompilationInfoPerThread::suspendCompilationThread()
   {
   _compInfo.acquireCompMonitor(_compilationThread);

   if (compilationThreadIsActive())
      {
      setCompilationThreadState(COMPTHREAD_SIGNAL_SUSPEND);
      getCompilationInfo()->decNumCompThreadsActive();
      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCompilationThreads))
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_PERF,"t=%6u Suspension request for compThread %d sleeping=%s",
                   (uint32_t)getCompilationInfo()->getPersistentInfo()->getElapsedTime(), getCompThreadId(), getMethodBeingCompiled()?"NO":"YES");
         }

      // What happens if the thread is sleeping on the compilationQueueMonitor?
      if (getCompilationInfo()->getNumCompThreadsActive() == 0)
         getCompilationInfo()->purgeMethodQueue(compilationSuspended);
      }
   _compInfo.releaseCompMonitor(_compilationThread);
   // The thread will be sleeping on the compThreadMonitor whenever it finishes current compilation
   }

//-------------------------- suspendCompilationThread ------------------------
// This method may be called when we want all JIT activity to stop temporarily
// It is called normally by a java thread
//----------------------------------------------------------------------------
void TR::CompilationInfo::suspendCompilationThread()
   {
   // if we compile on application thread, there is no compilation
   // request queue and there is no compilation thread
   // There can only be one request that is in progress. All the others
   // will see the SUSPENDED state when they get the application monitor
   if (!useSeparateCompilationThread())
      {
      if (_compInfoForCompOnAppThread->getCompilationThreadState() == COMPTHREAD_ACTIVE)
         _compInfoForCompOnAppThread->setCompilationThreadState(COMPTHREAD_SUSPENDED); // for compilation on application thread there is only one state
      }
   else // compilation on separate thread
      {
      TR_ASSERT(_compilationMonitor, "Must have a compilation queue monitor\n");
      J9JavaVM   *vm       = _jitConfig->javaVM;
      J9VMThread *vmThread = vm->internalVMFunctions->currentVMThread(vm);
      if (!vmThread)
         return; // cannot do it without a vmThread  FIXME: why?
      acquireCompMonitor(vmThread);

      // Must visit all compilation threads
      bool stoppedOneCompilationThread = false;
      for (uint8_t i = 0; i < getNumUsableCompilationThreads(); i++)
         {
         TR::CompilationInfoPerThread *curCompThreadInfoPT = _arrayOfCompilationInfoPerThread[i];
         TR_ASSERT(curCompThreadInfoPT, "a thread's compinfo is missing\n");

         // Only compilation threads that are active need to be suspended
         if (curCompThreadInfoPT->compilationThreadIsActive())
            {
            curCompThreadInfoPT->setCompilationThreadState(COMPTHREAD_SIGNAL_SUSPEND);
            decNumCompThreadsActive();
            // should we try to interrupt the compilation in progress?
            stoppedOneCompilationThread = true;
            if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCompilationThreads))
               {
               TR_VerboseLog::writeLineLocked(TR_Vlog_PERF,"t=%6u Suspension request for compThread %d sleeping=%s",
                  (uint32_t)getPersistentInfo()->getElapsedTime(), curCompThreadInfoPT->getCompThreadId(), curCompThreadInfoPT->getMethodBeingCompiled()?"NO":"YES");
               }
            }
         }
      if (stoppedOneCompilationThread)
         purgeMethodQueue(compilationSuspended);
      TR_ASSERT(_numCompThreadsActive == 0, "We must have all compilation threads suspended at this point\n");
      releaseCompMonitor(vmThread);
      } // endif
   }

void TR::CompilationInfoPerThread::resumeCompilationThread()
   {
   _compInfo.acquireCompMonitor(_compilationThread);

   if (getCompilationThreadState() == COMPTHREAD_SUSPENDED ||
       getCompilationThreadState() == COMPTHREAD_SIGNAL_SUSPEND)
      {
      if (getCompilationThreadState() == COMPTHREAD_SUSPENDED)
         {
         setCompilationThreadState(COMPTHREAD_ACTIVE);
         getCompThreadMonitor()->enter();
         getCompThreadMonitor()->notifyAll(); // wake the suspended thread
         getCompThreadMonitor()->exit();
         }
      else // COMPTHREAD_SIGNAL_SUSPEND
         {
         setCompilationThreadState(COMPTHREAD_ACTIVE);
         }

      // The diagnostic thread should not be counted as active
      if (!isDiagnosticThread())
         getCompilationInfo()->incNumCompThreadsActive();

      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCompilationThreads))
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_INFO,"t=%6u Resume request for compThread %d",
                   (uint32_t)getCompilationInfo()->getPersistentInfo()->getElapsedTime(), getCompThreadId());
         }
      }

   _compInfo.releaseCompMonitor(_compilationThread);
   }

//----------------------------- resumeCompilationThread -----------------------
// This version of resumeCompilationThread wakes up several compilation threads
//-----------------------------------------------------------------------------
void TR::CompilationInfo::resumeCompilationThread()
   {
   if (useSeparateCompilationThread())
      {
      J9JavaVM   *vm       = _jitConfig->javaVM;
      J9VMThread *vmThread = vm->internalVMFunctions->currentVMThread(vm);
      acquireCompMonitor(vmThread);

      /*
       * Count active/suspended threads for sake of sanity.
       * We can afford to do it because this method is not invoked very often
       *
       * FIXME:
       *    we are now using an array, so there is no linked list to count;
       *    therefore, we might be able to remove this entirely
       */
      int32_t numActiveCompThreads = 0; // RAS purposes
      int32_t numHot = 0;
      TR::CompilationInfoPerThread *compInfoPTHot = NULL;
      for (uint8_t i = 0; i < getNumUsableCompilationThreads(); i++)
         {
         TR::CompilationInfoPerThread *curCompThreadInfoPT = _arrayOfCompilationInfoPerThread[i];
         TR_ASSERT(curCompThreadInfoPT, "a thread's compinfo is missing\n");

         CompilationThreadState currentThreadState = curCompThreadInfoPT->getCompilationThreadState();
         if (currentThreadState == COMPTHREAD_ACTIVE ||
             currentThreadState == COMPTHREAD_SIGNAL_WAIT ||
             currentThreadState == COMPTHREAD_WAITING ||
             currentThreadState == COMPTHREAD_SIGNAL_SUSPEND)
            {
            if (curCompThreadInfoPT->compilationThreadIsActive())
               numActiveCompThreads++;
            if (curCompThreadInfoPT->getMethodBeingCompiled() &&
               curCompThreadInfoPT->getMethodBeingCompiled()->_hasIncrementedNumCompThreadsCompilingHotterMethods)
               {
               numHot++;
               if (currentThreadState == COMPTHREAD_SIGNAL_SUSPEND)
                  compInfoPTHot = curCompThreadInfoPT; // memorize this for later
               }
            }
         }

      // Check that our view about the number of active compilation threads is correct
      //
      if (numActiveCompThreads != getNumCompThreadsActive())
         {
         TR_ASSERT(false, "Discrepancy regarding active compilation threads: numActiveCompThreads=%d getNumCompThreadsActive()=%d\n", numActiveCompThreads, getNumCompThreadsActive());
         setNumCompThreadsActive(numActiveCompThreads); // Apply correction
         }
      if (getNumCompThreadsCompilingHotterMethods() != numHot)
         {
         TR_ASSERT(false, "Inconsistency with hot threads %d %d\n", getNumCompThreadsCompilingHotterMethods(), numHot);
         setNumCompThreadsCompilingHotterMethods(numHot); // apply correction
         }

      // If there is a compilation thread executing a hot compilation and the state is
      // SUSPENDING, activate this thread first (see defect 182392)
      if (compInfoPTHot)
         {
         compInfoPTHot->setCompilationThreadState(COMPTHREAD_ACTIVE);
         incNumCompThreadsActive();
         if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCompilationThreads))
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_INFO,"t=%6u Resume compThread %d Qweight=%d active=%d",
               (uint32_t)getPersistentInfo()->getElapsedTime(),
               compInfoPTHot->getCompThreadId(),
               getQueueWeight(),
               getNumCompThreadsActive());
            }
         }

      // If dynamicThreadActivation is used, wake compilation threads only as
      // many as required; otherwise wake them all
      for (uint8_t i = 0; i < getNumUsableCompilationThreads(); i++)
         {
         TR::CompilationInfoPerThread *curCompThreadInfoPT = _arrayOfCompilationInfoPerThread[i];
         TR_ASSERT(curCompThreadInfoPT, "a thread's compinfo is missing\n");

         TR_YesNoMaybe activate = shouldActivateNewCompThread();
         if (activate == TR_no)
            break;

         if (curCompThreadInfoPT->getCompilationThreadState() == COMPTHREAD_SIGNAL_SUSPEND ||
             curCompThreadInfoPT->getCompilationThreadState() == COMPTHREAD_SUSPENDED)
            {
            if (curCompThreadInfoPT->getCompilationThreadState() == COMPTHREAD_SUSPENDED)
               {
               curCompThreadInfoPT->setCompilationThreadState(COMPTHREAD_ACTIVE);
               curCompThreadInfoPT->getCompThreadMonitor()->enter();
               curCompThreadInfoPT->getCompThreadMonitor()->notifyAll(); // wake the suspended thread
               curCompThreadInfoPT->getCompThreadMonitor()->exit();
               }
            else // COMPTHREAD_SIGNAL_SUSPEND
               {
               curCompThreadInfoPT->setCompilationThreadState(COMPTHREAD_ACTIVE);
               }
            incNumCompThreadsActive();
            if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCompilationThreads))
               {
               TR_VerboseLog::writeLineLocked(TR_Vlog_INFO,"t=%6u Resume compThread %d Qweight=%d active=%d",
                  (uint32_t)getPersistentInfo()->getElapsedTime(),
                  curCompThreadInfoPT->getCompThreadId(),
                  getQueueWeight(),
                  getNumCompThreadsActive());
               }
            }
         } // end for
      TR_ASSERT(getNumCompThreadsActive() > 0, "We must have at least one compilation thread active");
      releaseCompMonitor(vmThread);
      }
   else // compilation on application thread
      {
      if (_compInfoForCompOnAppThread->getCompilationThreadState() == COMPTHREAD_SUSPENDED)
         _compInfoForCompOnAppThread->setCompilationThreadState(COMPTHREAD_ACTIVE);
      }
   }


int TR::CompilationInfo::computeCompilationThreadPriority(J9JavaVM *vm)
   {
   int priority = J9THREAD_PRIORITY_USER_MAX; // default value

   // convert priority codes into priorities defined by the VM
   static UDATA priorityConversionTable[]={J9THREAD_PRIORITY_MIN,
                                     J9THREAD_PRIORITY_USER_MIN,
                                     J9THREAD_PRIORITY_NORMAL,
                                     J9THREAD_PRIORITY_USER_MAX,
                                     J9THREAD_PRIORITY_MAX};
   if (TR::Options::_compilationThreadPriorityCode >= 0 &&
       TR::Options::_compilationThreadPriorityCode <= 4)
       priority = priorityConversionTable[TR::Options::_compilationThreadPriorityCode];
   return priority;
   }

TR::CompilationInfoPerThread* TR::CompilationInfo::getCompInfoForThread(J9VMThread *vmThread)
   {
   for (uint8_t i = 0; i < getNumTotalCompilationThreads(); i++)
      {
      TR::CompilationInfoPerThread *curCompThreadInfoPT = _arrayOfCompilationInfoPerThread[i];
      TR_ASSERT(curCompThreadInfoPT, "a thread's compinfo is missing\n");

      if (vmThread == curCompThreadInfoPT->getCompilationThread())
         return curCompThreadInfoPT;
      }

   return NULL;
   }

int32_t *TR::CompilationInfo::_compThreadActivationThresholds = NULL;
int32_t *TR::CompilationInfo::_compThreadSuspensionThresholds = NULL;
int32_t *TR::CompilationInfo::_compThreadActivationThresholdsonStarvation = NULL;

void TR::CompilationInfo::updateNumUsableCompThreads(int32_t &numUsableCompThreads)
   {
   if (getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER)
      {
      numUsableCompThreads = ((numUsableCompThreads < 0) ||
                              (numUsableCompThreads > MAX_SERVER_USABLE_COMP_THREADS)) ?
                               MAX_SERVER_USABLE_COMP_THREADS : numUsableCompThreads;
      }
   else
      {
      numUsableCompThreads = ((numUsableCompThreads < 0) ||
                              (numUsableCompThreads > MAX_CLIENT_USABLE_COMP_THREADS)) ?
                               MAX_CLIENT_USABLE_COMP_THREADS : numUsableCompThreads;
      }
   }

bool
TR::CompilationInfo::allocateCompilationThreads(int32_t numUsableCompThreads)
   {
   if (_compThreadActivationThresholds ||
       _compThreadSuspensionThresholds ||
       _compThreadActivationThresholdsonStarvation ||
       _arrayOfCompilationInfoPerThread)
      {
      TR_ASSERT_FATAL(false, "Compilation threads have been allocated\n");
      return false;
      }

   TR_ASSERT((numUsableCompThreads == TR::Options::_numUsableCompilationThreads),
             "numUsableCompThreads %d is not equal to the Option value %d", numUsableCompThreads, TR::Options::_numUsableCompilationThreads);

   if (getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER)
      {
      TR_ASSERT((0 < numUsableCompThreads) && (numUsableCompThreads <= MAX_SERVER_USABLE_COMP_THREADS),
               "numUsableCompThreads %d is greater than supported %d", numUsableCompThreads, MAX_SERVER_USABLE_COMP_THREADS);
      }
   else
      {
      TR_ASSERT((0 < numUsableCompThreads) && (numUsableCompThreads <= MAX_CLIENT_USABLE_COMP_THREADS),
               "numUsableCompThreads %d is greater than supported %d", numUsableCompThreads, MAX_CLIENT_USABLE_COMP_THREADS);
      }

   uint32_t numTotalCompThreads = numUsableCompThreads + MAX_DIAGNOSTIC_COMP_THREADS;

   TR::MonitorTable *table = TR::MonitorTable::get();
   if ((!table) ||
       (!table->allocInitClassUnloadMonitorHolders(numTotalCompThreads)))
      {
      return false;
      }

   _compThreadActivationThresholds = static_cast<int32_t *>(jitPersistentAlloc((numTotalCompThreads + 1) * sizeof(int32_t)));
   _compThreadSuspensionThresholds = static_cast<int32_t *>(jitPersistentAlloc((numTotalCompThreads + 1) * sizeof(int32_t)));
   _compThreadActivationThresholdsonStarvation = static_cast<int32_t *>(jitPersistentAlloc((numTotalCompThreads + 1) * sizeof(int32_t)));

   _arrayOfCompilationInfoPerThread = static_cast<TR::CompilationInfoPerThread **>(jitPersistentAlloc(numTotalCompThreads * sizeof(TR::CompilationInfoPerThread *)));

   if (_compThreadActivationThresholds &&
       _compThreadSuspensionThresholds &&
       _compThreadActivationThresholdsonStarvation &&
       _arrayOfCompilationInfoPerThread)
      {
      // How to read it: the queueWeight has to be over 100 to activate second comp thread
      //                 the queueWeight has to be below 10 to suspend second comp thread
      // For example:
      // compThreadActivationThresholds[MAX_TOTAL_COMP_THREADS+1] = {-1, 100, 200, 300, 400, 500, 600, 700, 800};
      // compThreadSuspensionThresholds[MAX_TOTAL_COMP_THREADS+1] = {-1,  -1,  10,  110, 210, 310, 410, 510, 610};
      // compThreadActivationThresholdsonStarvation[MAX_TOTAL_COMP_THREADS+1] = {-1, 800, 1600, 3200, 6400, 12800, 19200, 25600, 32000};
      _compThreadActivationThresholds[0] = -1;
      _compThreadActivationThresholds[1] = 100;
      _compThreadActivationThresholds[2] = _compThreadSuspensionThresholds[1] + 100;

      _compThreadSuspensionThresholds[0] = -1;
      _compThreadSuspensionThresholds[1] = -1;
      _compThreadSuspensionThresholds[2] = 10;

      for (int32_t i=3; i<(numTotalCompThreads+1); ++i)
         {
         _compThreadActivationThresholds[i] = _compThreadActivationThresholds[i-1] + 100;
         _compThreadSuspensionThresholds[i] = _compThreadSuspensionThresholds[i-1] + 100;
         }

      _compThreadActivationThresholdsonStarvation[0] = -1;
      _compThreadActivationThresholdsonStarvation[1] = 800;

      for (int32_t i=2; i<(numTotalCompThreads+1); ++i)
         {
         if (_compThreadActivationThresholdsonStarvation[i-1] < 12800)
            {
            _compThreadActivationThresholdsonStarvation[i] = _compThreadActivationThresholdsonStarvation[i-1] * 2;
            }
         else
            {
            _compThreadActivationThresholdsonStarvation[i] = _compThreadActivationThresholdsonStarvation[i-1] + 6400;
            }
         }

      for (int32_t i=0; i<numTotalCompThreads; ++i)
         {
         _arrayOfCompilationInfoPerThread[i] = NULL;
         }
      return true;
      }
   return false;
   }

void
TR::CompilationInfo::freeAllCompilationThreads()
   {
   if (_compThreadActivationThresholds)
      {
      jitPersistentFree(_compThreadActivationThresholds);
      }

   if (_compThreadSuspensionThresholds)
      {
      jitPersistentFree(_compThreadSuspensionThresholds);
      }

   if (_compThreadActivationThresholdsonStarvation)
      {
      jitPersistentFree(_compThreadActivationThresholdsonStarvation);
      }

   if (_arrayOfCompilationInfoPerThread)
      {
      jitPersistentFree(_arrayOfCompilationInfoPerThread);
      }
   }

//-------------------------- startCompilationThread --------------------------
// Start ONE compilation thread and initialize the associated
// TR::CompilationInfoPerThread structure
// This function returns immediately after the thread is created
// Parameters:
//   priority - the desired priority of the thread (0..5); negative number
//               means that the priority will be computed automatically
// Return value: 0 for success; error code for any error
//----------------------------------------------------------------------------
uintptr_t
TR::CompilationInfo::startCompilationThread(int32_t priority, int32_t threadId, bool isDiagnosticThread)
   {
   if (!_compilationMonitor)
      return 1;

   if (!isDiagnosticThread)
      {
      if (_numCompThreads >= TR::Options::_numUsableCompilationThreads)
         return 1;
      }
   else
      {
      if (_numDiagnosticThreads >= MAX_DIAGNOSTIC_COMP_THREADS)
         return 1;

      // _compInfoForDiagnosticCompilationThread should be NULL before creation
      if (_compInfoForDiagnosticCompilationThread)
         return 1;
      }

   J9JavaVM * vm   = jitConfig->javaVM;
   PORT_ACCESS_FROM_JAVAVM(vm);

   setCompBudget(TR::Options::_compilationBudget); // might do it several time, but it does not hurt

   // Create a compInfo for this thread
   TR::CompilationInfoPerThread  *compInfoPT = getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER ?
      new (persistentMemory()) TR::CompilationInfoPerThreadRemote(*this, _jitConfig, threadId, isDiagnosticThread) :
      new (persistentMemory()) TR::CompilationInfoPerThread(*this, _jitConfig, threadId, isDiagnosticThread);
   if (!compInfoPT || !compInfoPT->initializationSucceeded() || !compInfoPT->getCompThreadMonitor())
      return 1; // TODO must deallocate some things in compInfoPT as well as the memory for it

   int32_t jitPriority;
   if (priority < 0)
      {
      jitPriority = computeCompilationThreadPriority(vm);
      if (TR::Options::getCmdLineOptions()->realTimeGC())
         {
         static char *incMaxPriority = feGetEnv("IBM_J9_THREAD_INCREMENT_MAX_PRIORITY");
         static char *decJitPriority = feGetEnv("TR_DECREMENT_JIT_COMPILATION_PRIORITY");
         if (incMaxPriority && decJitPriority)
            jitPriority--;
         }
      }

   compInfoPT->setCompThreadPriority(priority < 0 ? jitPriority : priority);

   // add this entry to the array in the compilationInfo object
   // NOTE: This method must be issued by a single thread because we don't hold a monitor.
   _arrayOfCompilationInfoPerThread[compInfoPT->getCompThreadId()] = compInfoPT;

   // NOTE:
   //      variable (_numDiagnosticThreads) updates are wrapped in monitor to prevent instruction re-ordering
   //      since the order of updates matters and MUST be done AFTER _arrayOfCompilationInfoPerThread
   //      is updated; for example on PPC, these two statements may be done in the wrong order;
   //      the calls to the monitor insert synchronization instructions and prevent this
   if (isDiagnosticThread)
      {
      getCompilationMonitor()->enter();
      _compInfoForDiagnosticCompilationThread = compInfoPT;
      _numDiagnosticThreads++;
      getCompilationMonitor()->exit();
      }
   else
      {
      getCompilationMonitor()->enter();
      _numCompThreads++;
      getCompilationMonitor()->exit();
      }

   //_methodQueue = NULL; This is initialized when compInfo is created
   debugPrint("\t\tstarting compilation thread\n");

   if (vm->internalVMFunctions->createThreadWithCategory(&compInfoPT->_osThread,
                                                         TR::Options::_stackSize << 10, // stack size in kb
                                                         compInfoPT->getCompThreadPriority(),
                                                         0,                  // allow thread to start immediately
                                                         (j9thread_entrypoint_t)compilationThreadProc,
                                                         compInfoPT,
                                                         J9THREAD_CATEGORY_SYSTEM_JIT_THREAD))
      {
      //_compilationMonitor->destroy();
      //_compilationMonitor = NULL;
      // TODO must do some cleanup
      return 2;
      }

   // Would like to wait here until the compThread attaches to the VM
   compInfoPT->getCompThreadMonitor()->enter();
   while ( !compInfoPT->getCompilationThread() && compInfoPT->getCompilationThreadState() != COMPTHREAD_ABORT )
      compInfoPT->getCompThreadMonitor()->wait(); // for the compThread to be initialized
   compInfoPT->getCompThreadMonitor()->exit();

   if (compInfoPT->getCompilationThreadState() == COMPTHREAD_ABORT)
      return 3;

   return 0;
   }

#if defined(J9VM_JIT_DYNAMIC_LOOP_TRANSFER)
void *TR::CompilationInfo::searchForDLTRecord(J9Method *method, int32_t bcIndex)
   {
   int32_t hashVal = (intptrj_t)method * bcIndex % DLT_HASHSIZE;
   if (hashVal < 0)
      hashVal = -hashVal;

   //_dltMonitor->enter();
   struct DLT_record *dltPtr;

   // When bcIndex is negative(invalid) I have to scan the entire hashtable
   // for a matching j9method
   if (bcIndex < 0)
      {
      for (int32_t i=0; i<DLT_HASHSIZE; i++)
         {
         dltPtr = _dltHash[i];
         while (dltPtr != NULL)
            {
            if (dltPtr->_method==method)
               {
               //_dltMonitor->exit();
               return dltPtr->_dltEntry;
               }
            dltPtr = dltPtr->_next;
            }
         }
      }
   else
      {
      dltPtr = _dltHash[hashVal];
      while (dltPtr != NULL)
         {
         if (dltPtr->_method==method && dltPtr->_bcIndex==bcIndex)
            {
            // Safe: since we are not giving up on vmAccess yet
            //_dltMonitor->exit();
            return dltPtr->_dltEntry;
            }
         dltPtr = dltPtr->_next;
         }
      }
   //_dltMonitor->exit();
   return NULL;
   }

void TR::CompilationInfo::insertDLTRecord(J9Method *method, int32_t bcIndex, void *dltEntry)
   {
   int32_t hashVal = (intptrj_t)method * bcIndex % DLT_HASHSIZE;
   if (hashVal < 0)
      hashVal = -hashVal;

   {
   OMR::CriticalSection updatingDLTRecords(_dltMonitor);
   struct DLT_record *dltPtr = _dltHash[hashVal];

   while (dltPtr != NULL)
      {
      if (dltPtr->_method == method && dltPtr->_bcIndex == bcIndex)
         {
         return;
         }
      dltPtr = dltPtr->_next;
      }

   struct DLT_record *myRecord;

   if (_freeDLTRecord != NULL)
      {
      myRecord = _freeDLTRecord;
      _freeDLTRecord = myRecord->_next;
      }
   else
      myRecord = (struct DLT_record *)jitPersistentAlloc(sizeof(struct DLT_record));

   if (!myRecord)
      {
      return;
      }

   myRecord->_method = method;
   myRecord->_bcIndex = bcIndex;
   myRecord->_dltEntry = dltEntry;
   myRecord->_next = _dltHash[hashVal];
   // Memory Flush (for PPC) so myRecord is visible to other CPUs before we update the _dltHash
   // Doing this means we don't need locking when reading the _dltHash linked lists
   FLUSH_MEMORY(TR::Compiler->target.isSMP());
   _dltHash[hashVal] = myRecord;
   }
   }

void TR::CompilationInfo::cleanDLTRecordOnUnload(J9ClassLoader *classloader)
   {
   for (int32_t i=0; i<DLT_HASHSIZE; i++)
      {
      struct DLT_record *prev=NULL, *curr=_dltHash[i], *next;
      while (curr != NULL)
         {
         next = curr->_next;
         if (J9_CLASS_FROM_METHOD(curr->_method)->classLoader == classloader)
            {
            if (prev == NULL)
               _dltHash[i] = next;
            else
               prev->_next = next;

            // FIXME: free the codeCache
            curr->_next = _freeDLTRecord;
            _freeDLTRecord = curr;
            }
         else
            prev = curr;
         curr = next;
         }
      }
   }
#endif

#ifdef INVOCATION_STATS
extern "C" J9Method * getNewInstancePrototype(J9VMThread * context);
#include "infra/Statistics.hpp"
void printAllCounts(J9JavaVM *javaVM)
   {
   J9VMThread *vmThread = javaVM->internalVMFunctions->currentVMThread(javaVM);
   J9ClassWalkState classWalkState;
   TR_StatsHisto<30> statsInvocationCountLoopy("Stats inv. count loopy methods", 5, 155);
   TR_StatsHisto<30> statsInvocationCountLoopless("Stats inv. count loopless methods", 5, 155);
   int32_t numMethodsNeverInvoked = 0;
   J9Class * clazz = javaVM->internalVMFunctions->allClassesStartDo(&classWalkState, javaVM, NULL);

   while (clazz)
      {

      if (!J9ROMCLASS_IS_PRIMITIVE_OR_ARRAY(clazz->romClass))
         {
         J9Method * newInstanceThunk = getNewInstancePrototype(vmThread);
         J9Method * ramMethods = (J9Method *) (clazz->ramMethods);
         for (int32_t m = 0; m < clazz->romClass->romMethodCount; m++)
            {
            J9Method * method = &ramMethods[m];
            J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
            if (!(romMethod->modifiers & (J9AccNative | J9AccAbstract)) &&
                method != newInstanceThunk &&
                !TR::CompilationInfo::isCompiled(method))
               {
               // Print the count for the interpreted method
                  /*
               J9UTF8 *utf8;
               utf8 = J9ROMCLASS_CLASSNAME(clazz->romClass);
               fprintf(stderr, "%.*s", J9UTF8_LENGTH(utf8), (char *) J9UTF8_DATA(utf8));
               utf8 = J9ROMMETHOD_NAME(J9_ROM_METHOD_FROM_RAM_METHOD(method));
               fprintf(stderr, ".%.*s", J9UTF8_LENGTH(utf8), (char *) J9UTF8_DATA(utf8));
               utf8 = J9ROMMETHOD_SIGNATURE(J9_ROM_METHOD_FROM_RAM_METHOD(method));
               fprintf(stderr, "%.*s", J9UTF8_LENGTH(utf8), (char *) J9UTF8_DATA(utf8));
                  */
               intptrj_t count = (intptrj_t) TR::CompilationInfo::getInvocationCount(method);
               if (count < 0)
                  {
                  fprintf(stderr, "Bad count\n");
                  }
               else
                  {
                  //fprintf(stderr, " extra=%d ", count);
                  intptrj_t initialCount = J9ROMMETHOD_HAS_BACKWARDS_BRANCHES(romMethod) ? TR::Options::getCmdLineOptions()->getInitialBCount() : TR::Options::getCmdLineOptions()->getInitialCount();
                  if (count > initialCount)
                     {
                     fprintf(stderr, " Bad count\n");
                     }
                  else
                     {
                     intptrj_t invocationCount = initialCount - count;
                     //fprintf(stderr, " invocation count=%d\n", invocationCount);
                     if (invocationCount==0 || count == 0) // some methods have count==0 and are never invoked
                        {
                        numMethodsNeverInvoked++;
                        }
                     else
                        {
                        if (J9ROMMETHOD_HAS_BACKWARDS_BRANCHES(romMethod))
                           statsInvocationCountLoopy.update((double)invocationCount);
                        else
                           statsInvocationCountLoopless.update((double)invocationCount);
                        }
                     }
                  }
               }
            }
         }
      clazz = javaVM->internalVMFunctions->allClassesNextDo(&classWalkState);
      }
   javaVM->internalVMFunctions->allClassesEndDo(&classWalkState);
   statsInvocationCountLoopy.report(stderr);
   statsInvocationCountLoopless.report(stderr);
   fprintf(stderr, "Methods never invoked = %d\n", numMethodsNeverInvoked);
   }
#endif // INVOCATION_STATS

#include "infra/Statistics.hpp"  // MCT

void TR::CompilationInfo::stopCompilationThreads()
   {
   J9JavaVM   * const vm       = _jitConfig->javaVM;
   J9VMThread * const vmThread = vm->internalVMFunctions->currentVMThread(vm);
   //fprintf(stderr, "stopCompilationThread\n");
   // if we compile on application thread, there is no compilation
   // request queue and there is no compilation thread
   // The SMALL jit case is already treated separately
   if (!useSeparateCompilationThread())
      {
      acquireCompMonitor(vmThread);
      _compInfoForCompOnAppThread->setCompilationThreadState(COMPTHREAD_STOPPED);
      _compInfoForCompOnAppThread->setCompilationShouldBeInterrupted(SHUTDOWN_COMP_INTERRUPT);
      releaseCompMonitor(vmThread);
      return;
      }

   static char * printCompStats = feGetEnv("TR_PrintCompStats");
   if (printCompStats)
      {
      if (statCompErrors.samples() > 0)
         statCompErrors.report(stderr);

      fprintf(stderr, "Number of compilations per level:\n");
      for (int i = 0; i < (int)numHotnessLevels; i++)
         if (_statsOptLevels[i] > 0)
            fprintf(stderr, "Level=%d\tnumComp=%d\n", i, _statsOptLevels[i]);

      if (_statNumJNIMethodsCompiled > 0)
         fprintf(stderr, "NumJNIMethodsCompiled=%u\n", _statNumJNIMethodsCompiled);
      if (numMethodsFoundInSharedCache() > 0)
         fprintf(stderr, "NumMethodsFoundInSharedCache=%d\n", numMethodsFoundInSharedCache());
      if (_statNumMethodsFromSharedCache)
         fprintf(stderr, "NumMethodsTakenFromSharedCache=%u\n", _statNumMethodsFromSharedCache);
      if (_statNumAotedMethods)
         fprintf(stderr, "NumAotedMethods=%u\n", _statNumAotedMethods);
      if (_statNumAotedMethodsRecompiled)
         fprintf(stderr, "NumberOfAotedMethodsThatWereRecompiled=%u (forced=%d)\n", _statNumAotedMethodsRecompiled, _statNumForcedAotUpgrades);
      if (_statTotalAotQueryTime)
         fprintf(stderr, "Time spent querying shared cache for methods: %u ms\n", _statTotalAotQueryTime/1000);

      if (getHWProfiler() && TR_HWProfiler::_STATS_NumUpgradesDueToRI > 0)
         fprintf(stderr, "numUpgradesDueToRI=%u\n", TR_HWProfiler::_STATS_NumUpgradesDueToRI);

      fprintf(stderr, "Classes loaded=%d\n",  getPersistentInfo()->getNumLoadedClasses());

      // assumptionTableMutex is not used, so the numbers may be a little off
      fprintf(stderr, "\tStats on assumptions:\n");
      TR_RuntimeAssumptionTable * rat = getPersistentInfo()->getRuntimeAssumptionTable();
      int32_t unreclaimedAssumptions = 0;
      for (int32_t i=0; i < LastAssumptionKind; i++)
         {
         fprintf(stderr, "\tAssumptionType=%d allocated=%d reclaimed=%d\n", i, rat->getAssumptionCount(i), rat->getReclaimedAssumptionCount(i));
         unreclaimedAssumptions += rat->getAssumptionCount(i) - rat->getReclaimedAssumptionCount(i);
         }
      int32_t assumptionsInRAT = rat->countRatAssumptions();
      fprintf(stderr, "Summary of assumptions: unreclaimed=%d, in RAT=%d\n", unreclaimedAssumptions, rat->countRatAssumptions());

      fprintf(stderr, "GCR bodies=%d, GCRSaves=%d GCRRecomp=%u\n",
              getPersistentInfo()->getNumGCRBodies(),
              getPersistentInfo()->getNumGCRSaves(),
             _statNumGCRInducedCompilations);
      if (_statNumSamplingJProfilingBodies != 0)
         fprintf(stderr, "SamplingJProfiling bodies=%u\n", _statNumSamplingJProfilingBodies);
      if (_statNumJProfilingBodies != 0)
         fprintf(stderr, "Jprofiling bodies=%u\n", _statNumJProfilingBodies);
      if (_statNumMethodsFromJProfilingQueue != 0)
         fprintf(stderr, "Recompilation for bodies with JProfiling=%u\n", _statNumMethodsFromJProfilingQueue);
      if (_statNumRecompilationForBodiesWithJProfiling != 0)
         fprintf(stderr, "Methods taken from the queue with JProfiling requests=%u\n", _statNumRecompilationForBodiesWithJProfiling);

#ifdef INVOCATION_STATS
      printAllCounts(_jitConfig->javaVM);
#endif

      getLowPriorityCompQueue().printStats();

      fprintf(stderr, "Compilation queue peak size = %d\n", getPeakMethodQueueSize());
      fprintf(stderr, "Compilation queue size at shutdown = %d\n", getMethodQueueSize());
#ifdef MCT_STATS
      statCompReqResidencyTime.report(stderr); // MCT
      statCompReqProcessingTime.report(stderr); // MCT
      statNumReqForProcessing.report(stderr);  // MCT
      fprintf(stderr, "NumberOfActivations=%u number of suspensions=%u\n", numCompThreadsActivations, numCompThreadsSuspensions);
#endif
      } // if (printCompStats)

   if (TR::Options::getAOTCmdLineOptions()->getOption(TR_EnableAOTRelocationTiming))
      {
      fprintf(stderr, "Time spent relocating all AOT methods: %u ms\n", this->getAotRelocationTime()/1000);
      }

   static char * printCompMem = feGetEnv("TR_PrintCompMem");
   if (printCompMem)
      {
      int32_t codeCacheAllocated = TR::CodeCacheManager::instance()->getCurrentNumberOfCodeCaches() * _jitConfig->codeCacheKB;
      fprintf(stderr, "Allocated memory for code cache = %d KB\tLimit = %u KB\n",
         codeCacheAllocated, (uint32_t)_jitConfig->codeCacheTotalKB);

      TR::CodeCacheManager::instance()->printMccStats();

      fprintf(stderr, "Allocated memory for data cache = %d KB\tLimit = %d KB\n",
         TR_DataCacheManager::getManager()->getTotalSegmentMemoryAllocated()/1024,
          (uint32_t)_jitConfig->dataCacheTotalKB);

      if (getJProfilerThread())
         fprintf(stderr, "Allocated memory for profile info = %lu KB\n", getJProfilerThread()->getProfileInfoFootprint()/1024);
      }

   static char * printPersistentMem = feGetEnv("TR_PrintPersistentMem");
   if (printPersistentMem)
      {
      if (trPersistentMemory)
         trPersistentMemory->printMemStats();
      }

   TR_DataCacheManager::getManager()->printStatistics();

   bool aotStatsEnabled = TR::Options::getAOTCmdLineOptions()->getOption(TR_EnableAOTStats);
   if (aotStatsEnabled)
      {
      fprintf(stderr, "AOT code compatible: %d\n", static_cast<TR_JitPrivateConfig *>(jitConfig->privateConfig)->aotValidHeader);

      TR_AOTStats *aotStats = ((TR_JitPrivateConfig *)_jitConfig->privateConfig)->aotStats;
      fprintf(stderr, "AOT failedPerfAssumptionCode: %d\n", aotStats->failedPerfAssumptionCode);
      fprintf(stderr, "COMPILE TIME INFO ------\n");
      fprintf(stderr, "numCHEntriesAlreadyStoredInLocalList: %d\n", aotStats->numCHEntriesAlreadyStoredInLocalList);
      fprintf(stderr, "numNewCHEntriesInLocalList: %d\n", aotStats->numNewCHEntriesInLocalList);
      fprintf(stderr, "numNewCHEntriesInSharedClass: %d\n", aotStats->numNewCHEntriesInSharedClass);
      fprintf(stderr, "numEntriesFoundInLocalChain: %d\n", aotStats->numEntriesFoundInLocalChain);
      fprintf(stderr, "numEntriesFoundAndValidatedInSharedClass: %d\n", aotStats->numEntriesFoundAndValidatedInSharedClass);
      fprintf(stderr, "numClassChainNotInSharedClass: %d\n", aotStats->numClassChainNotInSharedClass);
      fprintf(stderr, "numCHInSharedCacheButFailValiation: %d\n", aotStats->numCHInSharedCacheButFailValiation);
      fprintf(stderr, "numInstanceFieldInfoNotUsed: %d\n", aotStats->numInstanceFieldInfoNotUsed);
      fprintf(stderr, "numStaticFieldInfoNotUsed: %d\n", aotStats->numStaticFieldInfoNotUsed);
      fprintf(stderr, "numDefiningClassNotFound: %d\n", aotStats->numDefiningClassNotFound);
      fprintf(stderr, "numInstanceFieldInfoUsed: %d\n", aotStats->numInstanceFieldInfoUsed);
      fprintf(stderr, "numStaticFieldInfoUsed: %d\n", aotStats->numStaticFieldInfoUsed);
      fprintf(stderr, "numCannotGenerateHashForStore: %d\n", aotStats->numCannotGenerateHashForStore);

      fprintf(stderr, "-------------------------\n");
      fprintf(stderr, "RUNTIME INFO -----------\n");

      fprintf(stderr, "numRuntimeChainNotFound: %d\n", aotStats->numRuntimeChainNotFound);
      fprintf(stderr, "numRuntimeStaticFieldUnresolvedCP: %d\n", aotStats->numRuntimeStaticFieldUnresolvedCP);
      fprintf(stderr, "numRuntimeInstanceFieldUnresolvedCP: %d\n", aotStats->numRuntimeInstanceFieldUnresolvedCP);
      fprintf(stderr, "numRuntimeUnresolvedStaticFieldFromCP: %d\n", aotStats->numRuntimeUnresolvedStaticFieldFromCP);
      fprintf(stderr, "numRuntimeUnresolvedInstanceFieldFromCP: %d\n", aotStats->numRuntimeUnresolvedInstanceFieldFromCP);
      fprintf(stderr, "numRuntimeResolvedStaticFieldButFailValidation: %d\n", aotStats->numRuntimeResolvedStaticFieldButFailValidation);
      fprintf(stderr, "numRuntimeResolvedInstanceFieldButFailValidation: %d\n", aotStats->numRuntimeResolvedInstanceFieldButFailValidation);
      fprintf(stderr, "numRuntimeStaticFieldReloOK: %d\n", aotStats->numRuntimeStaticFieldReloOK);
      fprintf(stderr, "numRuntimeInstanceFieldReloOK: %d\n", aotStats->numRuntimeInstanceFieldReloOK);

      fprintf(stderr, "numRuntimeClassAddressUnresolvedCP: %d\n", aotStats->numRuntimeClassAddressUnresolvedCP);
      fprintf(stderr, "numRuntimeClassAddressFromCP: %d\n", aotStats->numRuntimeClassAddressFromCP);
      fprintf(stderr, "numRuntimeClassAddressButFailValidation: %d\n", aotStats->numRuntimeClassAddressButFailValidation);
      fprintf(stderr, "numRuntimeClassAddressReloOK: %d\n", aotStats->numRuntimeClassAddressReloOK);

      fprintf(stderr, "numRuntimeClassAddressRelocationCount: %d\n", aotStats->numRuntimeClassAddressRelocationCount);
      fprintf(stderr, "numRuntimeClassAddressReloUnresolvedCP: %d\n", aotStats->numRuntimeClassAddressReloUnresolvedCP);
      fprintf(stderr, "numRuntimeClassAddressReloUnresolvedClass: %d\n", aotStats->numRuntimeClassAddressReloUnresolvedClass);

      fprintf(stderr, "numClassValidations: %d\n", aotStats->numClassValidations);
      fprintf(stderr, "numClassValidationsFailed: %d\n", aotStats->numClassValidationsFailed);
      fprintf(stderr, "numWellKnownClassesValidationsFailed: %d\n", aotStats->numWellKnownClassesValidationsFailed);

      fprintf(stderr, "numVMCheckCastEvaluator (x86): %d\n", aotStats->numVMCheckCastEvaluator);
      fprintf(stderr, "numVMInstanceOfEvaluator (x86): %d\n", aotStats->numVMInstanceOfEvaluator);
      fprintf(stderr, "numVMIfInstanceOfEvaluator (x86): %d\n", aotStats->numVMIfInstanceOfEvaluator);
      fprintf(stderr, "numCheckCastVMHelperInstructions (x86): %d\n", aotStats->numCheckCastVMHelperInstructions);
      fprintf(stderr, "numInstanceOfVMHelperInstructions (x86): %d\n", aotStats->numInstanceOfVMHelperInstructions);
      fprintf(stderr, "numIfInstanceOfVMHelperInstructions (x86): %d\n", aotStats->numIfInstanceOfVMHelperInstructions);


      fprintf(stderr, "-------------------------\n");
      fprintf(stderr, "AOT METHOD INLINING COMPILE TIME INFO ------\n");

      fprintf(stderr, "numStaticMethodFromDiffClassLoader: %d\n", aotStats->staticMethods.numMethodFromDiffClassLoader);
      fprintf(stderr, "numStaticMethodInSameClass: %d\n", aotStats->staticMethods.numMethodInSameClass);
      fprintf(stderr, "numStaticMethodNotInSameClass: %d\n", aotStats->staticMethods.numMethodNotInSameClass);
      fprintf(stderr, "numStaticMethodResolvedAtCompile: %d\n", aotStats->staticMethods.numMethodResolvedAtCompile);
      fprintf(stderr, "numStaticMethodNotResolvedAtCompile: %d\n", aotStats->staticMethods.numMethodNotResolvedAtCompile);
      fprintf(stderr, "numStaticMethodROMMethodNotInSC: %d\n", aotStats->staticMethods.numMethodROMMethodNotInSC);

      fprintf(stderr, "numSpecialMethodFromDiffClassLoader: %d\n", aotStats->specialMethods.numMethodFromDiffClassLoader);
      fprintf(stderr, "numSpecialMethodInSameClass: %d\n", aotStats->specialMethods.numMethodInSameClass);
      fprintf(stderr, "numSpecialMethodNotInSameClass: %d\n", aotStats->specialMethods.numMethodNotInSameClass);
      fprintf(stderr, "numSpecialMethodResolvedAtCompile: %d\n", aotStats->specialMethods.numMethodResolvedAtCompile);
      fprintf(stderr, "numSpecialMethodNotResolvedAtCompile: %d\n", aotStats->specialMethods.numMethodNotResolvedAtCompile);
      fprintf(stderr, "numSpecialMethodROMMethodNotInSC: %d\n", aotStats->specialMethods.numMethodROMMethodNotInSC);

      fprintf(stderr, "numVirtualMethodFromDiffClassLoader: %d\n", aotStats->virtualMethods.numMethodFromDiffClassLoader);
      fprintf(stderr, "numVirtualMethodInSameClass: %d\n", aotStats->virtualMethods.numMethodInSameClass);
      fprintf(stderr, "numVirtualMethodNotInSameClass: %d\n", aotStats->virtualMethods.numMethodNotInSameClass);
      fprintf(stderr, "numVirtualMethodResolvedAtCompile: %d\n", aotStats->virtualMethods.numMethodResolvedAtCompile);
      fprintf(stderr, "numVirtualMethodNotResolvedAtCompile: %d\n", aotStats->virtualMethods.numMethodNotResolvedAtCompile);
      fprintf(stderr, "numVirtualMethodROMMethodNotInSC: %d\n", aotStats->virtualMethods.numMethodROMMethodNotInSC);

      fprintf(stderr, "numInterfaceMethodFromDiffClassLoader: %d\n", aotStats->interfaceMethods.numMethodFromDiffClassLoader);
      fprintf(stderr, "numInterfaceMethodInSameClass: %d\n", aotStats->interfaceMethods.numMethodInSameClass);
      fprintf(stderr, "numInterfaceMethodNotInSameClass: %d\n", aotStats->interfaceMethods.numMethodNotInSameClass);
      fprintf(stderr, "numInterfaceMethodResolvedAtCompile: %d\n", aotStats->interfaceMethods.numMethodResolvedAtCompile);
      fprintf(stderr, "numInterfaceMethodNotResolvedAtCompile: %d\n", aotStats->interfaceMethods.numMethodNotResolvedAtCompile);
      fprintf(stderr, "numInterfaceMethodROMMethodNotInSC: %d\n", aotStats->interfaceMethods.numMethodROMMethodNotInSC);

      fprintf(stderr, "-------------------------\n");

      fprintf(stderr, "AOT METHOD INLINING RUNTIME INFO ------\n");

      fprintf(stderr, "numInlinedMethodOverridden: %d\n", aotStats->numInlinedMethodOverridden);
      fprintf(stderr, "numInlinedMethodNotResolved: %d\n", aotStats->numInlinedMethodNotResolved);
      fprintf(stderr, "numInlinedMethodClassNotMatch: %d\n", aotStats->numInlinedMethodClassNotMatch);
      fprintf(stderr, "numInlinedMethodCPNotResolved: %d\n", aotStats->numInlinedMethodCPNotResolved);
      fprintf(stderr, "numInlinedMethodRelocated: %d\n", aotStats->numInlinedMethodRelocated);
      fprintf(stderr, "numInlinedMethodValidationFailed: %d\n", aotStats->numInlinedMethodValidationFailed);

      fprintf(stderr, "numDataAddressRelosSucceed: %d\n", aotStats->numDataAddressRelosSucceed);
      fprintf(stderr, "numDataAddressRelosFailed: %d\n", aotStats->numDataAddressRelosFailed);

      fprintf(stderr, "-------------------------\n");

      fprintf(stderr, "numStaticMethodsValidationFailed: %d\n", aotStats->staticMethods.numFailedValidations);
      fprintf(stderr, "numStaticMethodsValidationSucceeded: %d\n", aotStats->staticMethods.numSucceededValidations);
      fprintf(stderr, "numSpecialMethodsValidationFailed: %d\n", aotStats->specialMethods.numFailedValidations);
      fprintf(stderr, "numSpecialMethodsValidationSucceeded: %d\n", aotStats->specialMethods.numSucceededValidations);
      fprintf(stderr, "numVirtualMethodsValidationFailed: %d\n", aotStats->virtualMethods.numFailedValidations);
      fprintf(stderr, "numVirtualMethodsValidationSucceeded: %d\n", aotStats->virtualMethods.numSucceededValidations);
      fprintf(stderr, "numInterfaceMethodsValidationFailed: %d\n", aotStats->interfaceMethods.numFailedValidations);
      fprintf(stderr, "numInterfaceMethodsValidationSucceeded: %d\n", aotStats->interfaceMethods.numSucceededValidations);
      fprintf(stderr, "numAbstractMethodsValidationFailed: %d\n", aotStats->abstractMethods.numFailedValidations);
      fprintf(stderr, "numAbstractMethodsValidationSucceeded: %d\n", aotStats->abstractMethods.numSucceededValidations);

      fprintf(stderr, "-------------------------\n");
      fprintf(stderr, "numProfiledClassGuardsValidationFailed: %d\n", aotStats->profiledClassGuards.numFailedValidations);
      fprintf(stderr, "numProfiledClassGuardsValidationSucceeded: %d\n", aotStats->profiledClassGuards.numSucceededValidations);
      fprintf(stderr, "numProfiledMethodGuardsValidationFailed: %d\n", aotStats->profiledMethodGuards.numFailedValidations);
      fprintf(stderr, "numProfiledMethodGuardsValidationSucceeded: %d\n", aotStats->profiledMethodGuards.numSucceededValidations);
      fprintf(stderr, "-------------------------\n");

      fprintf(stderr, "RELO FAILURES BY TYPE ------\n");
      for (uint32_t i = 0; i < TR_NumExternalRelocationKinds; i++)
         {
         fprintf(stderr, "%s: %d\n", TR::ExternalRelocation::getName((TR_ExternalRelocationTargetKind)i), aotStats->numRelocationsFailedByType[i]);
         }
      fprintf(stderr, "-------------------------\n");

      } // AOT stats


   if (printCompStats && (dynamicThreadPriority() || compBudgetSupport()))
      {
      fprintf(stderr, "Number of yields  =%4u\n", _statNumYields);
      fprintf(stderr, "NumPriorityChanges=%4u\n", _statNumPriorityChanges);
      fprintf(stderr, "NumUpgradeInterpretedMethod  =%u\n", _statNumUpgradeInterpretedMethod);
      fprintf(stderr, "NumDowngradeInterpretedMethod=%u\n", _statNumDowngradeInterpretedMethod);
      fprintf(stderr, "NumUpgradeJittedMethod=%u\n", _statNumUpgradeJittedMethod);
      fprintf(stderr, "NumQueuePromotions=%u\n", _statNumQueuePromotions);
      }

   if (feGetEnv("TR_PrintJITaaSIPMsgStats"))
      {
      if (getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER)
         {
         TR_J9VMBase * vmj9 = (TR_J9VMBase *)(TR_J9VMBase::get(_jitConfig, 0));
         TR_JITaaSIProfiler *JITaaSIProfiler = (TR_JITaaSIProfiler *)vmj9->getIProfiler();
         JITaaSIProfiler->printStats();
         }
      }

   if (feGetEnv("TR_PrintJITaaSConnStats"))
      {
      if (getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER)
         {
         fprintf(stderr, "Number of connections opened = %u\n", JITServer::ServerStream::getNumConnectionsOpened());
         fprintf(stderr, "Number of connections closed = %u\n", JITServer::ServerStream::getNumConnectionsClosed());
         }
      else if (getPersistentInfo()->getRemoteCompilationMode() == JITServer::CLIENT)
         {
         fprintf(stderr, "Number of connections opened = %u\n", JITServer::ClientStream::getNumConnectionsOpened());
         fprintf(stderr, "Number of connections closed = %u\n", JITServer::ClientStream::getNumConnectionsClosed());
         }
      }

#ifdef STATS
   if (compBudgetSupport() || dynamicThreadPriority())
      {
      fprintf(stderr, "Compilation request queue size at shutdown=%d\n", getMethodQueueSize());
      statQueueSize.report(stderr);
      if (compBudgetSupport())
         {
         statBudgetEpoch.report(stderr);
         statBudgetSmallLag.report(stderr);
         statBudgetMediumLag.report(stderr);
         statEpochLength.report(stderr);
         }
      statEvents.report(stderr);
      statLowPriority.report(stderr);
      statLowBudget.report(stderr);
      statOverhead.report(stderr);
      }
#endif
   acquireCompMonitor(vmThread);

   addCompilationTraceEntry(vmThread, OP_WillStopCompilationThreads);

   // cycle through all the threads
   // NOTE: comp monitor is held while this is happening
   for (uint8_t i = 0; i < getNumTotalCompilationThreads(); i++)
      {
      TR::CompilationInfoPerThread *curCompThreadInfoPT = _arrayOfCompilationInfoPerThread[i];
      TR_ASSERT(curCompThreadInfoPT, "a thread's compinfo is missing\n");

      // set the flag for the compilation-in-progress to be aborted at the next yield,
      // unless the thread is compiling for log - then it should finish
      // NOTE: even if the thread is compiling for dump, it will still be signaled to terminate
      if (!(curCompThreadInfoPT->isDiagnosticThread()))
         curCompThreadInfoPT->setCompilationShouldBeInterrupted(SHUTDOWN_COMP_INTERRUPT);

      switch (curCompThreadInfoPT->getCompilationThreadState())
         {
         case COMPTHREAD_SUSPENDED:
            curCompThreadInfoPT->setCompilationThreadState(COMPTHREAD_SIGNAL_TERMINATE);
            curCompThreadInfoPT->getCompThreadMonitor()->enter();
            curCompThreadInfoPT->getCompThreadMonitor()->notifyAll();
            curCompThreadInfoPT->getCompThreadMonitor()->exit();
            break;

         case COMPTHREAD_ACTIVE:
         case COMPTHREAD_SIGNAL_WAIT:
         case COMPTHREAD_WAITING:
            curCompThreadInfoPT->setCompilationThreadState(COMPTHREAD_SIGNAL_TERMINATE);
            // Only decrement the number of active threads if we are not a diagnostic
            // thread
            if (!(curCompThreadInfoPT->isDiagnosticThread()))
               decNumCompThreadsActive();
            break;

         case COMPTHREAD_SIGNAL_SUSPEND:
            curCompThreadInfoPT->setCompilationThreadState(COMPTHREAD_SIGNAL_TERMINATE);
            break;

         case COMPTHREAD_SIGNAL_TERMINATE:
         case COMPTHREAD_STOPPING:
         case COMPTHREAD_STOPPED:
            // weird case; we should not do anything
            break;

         case COMPTHREAD_UNINITIALIZED:
            // Compilation thread did not have time to become fully initialized
            curCompThreadInfoPT->setCompilationThreadState(COMPTHREAD_SIGNAL_TERMINATE);
            break;

         default:
            TR_ASSERT(false, "No other comp thread state possible here\n");
         } // end switch
      }
   TR_ASSERT(getNumCompThreadsActive()==0, "All threads must be inactive at this point\n");
   purgeMethodQueue(compilationSuspended);

   for (uint8_t i = 0; i < getNumTotalCompilationThreads(); i++)
      {
      TR::CompilationInfoPerThread *curCompThreadInfoPT = _arrayOfCompilationInfoPerThread[i];
      TR_ASSERT(curCompThreadInfoPT, "a thread's compinfo is missing\n");

      while (curCompThreadInfoPT->getCompilationThreadState() != COMPTHREAD_STOPPED)
         {
         getCompilationMonitor()->notifyAll();
         waitOnCompMonitor(vmThread);
         }
      }
   // Remove the method pool entries
   //
   PORT_ACCESS_FROM_JAVAVM(_jitConfig->javaVM);
   while (_methodPool)
      {
      TR_MethodToBeCompiled *next = _methodPool->_next;
      if (_methodPool->_numThreadsWaiting == 0)
         {
         _methodPool->shutdown();
         j9mem_free_memory(_methodPool);
         }
      else // Some thread may need this entry after the waiting thread resumes activity
         {
         _methodPool->_entryShouldBeDeallocated = true;
         }
      _methodPool = next;
      }
#ifdef LINUX
   // Now that all compilation threads are stopped we can close the perfFile stream
   if (TR::CompilationInfoPerThreadBase::getPerfFile())
      {
      // Generate any non-method entries (trampolines, etc) that are present in each codecache
      TR::CodeCacheManager *manager = TR::CodeCacheManager::instance();
      for (TR::CodeCache *cc = manager->getFirstCodeCache(); cc; cc = cc->getNextCodeCache())
         {
         cc->generatePerfToolEntries(TR::CompilationInfoPerThreadBase::getPerfFile());
         }

      j9jit_fclose(TR::CompilationInfoPerThreadBase::getPerfFile());
      TR::CompilationInfoPerThreadBase::setPerfFile(NULL); // prevent closing twice
      }
#endif

   releaseCompMonitor(vmThread);
   if (getPersistentInfo()->getRemoteCompilationMode() == JITServer::CLIENT)
      {
      try
         {
         JITServer::ClientStream client(getPersistentInfo());
         client.writeError(JITServer::MessageType::clientSessionTerminate, getPersistentInfo()->getClientUID());
         }
      catch (const JITServer::StreamFailure &e)
         {
         JITaaSHelpers::postStreamFailure(OMRPORT_FROM_J9PORT(_jitConfig->javaVM->portLibrary));
         // catch the stream failure exception if the server dies before the dummy message is send for termination.
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "JITServer StreamFailure (server unreachable before the termination message was sent): %s", e.what());
         }
      }
   }

IDATA J9THREAD_PROC compilationThreadProc(void *entryarg)
   {
   //J9JITConfig *jitConfig = (J9JITConfig *)entryarg;
   TR::CompilationInfoPerThread *compInfoPT = (TR::CompilationInfoPerThread *)entryarg;
   J9JITConfig *jitConfig = compInfoPT->getJitConfig();
   J9JavaVM   *vm         = jitConfig->javaVM;
   J9VMThread *compThread = 0;
   TR::CompilationInfo *compInfo = TR::CompilationInfo::get();
   static bool TR_NoStructuredHandler = feGetEnv("TR_NoStructuredHandler")?1:0;

   int rc = vm->internalVMFunctions->internalAttachCurrentThread
       (vm, &compThread, NULL,
        J9_PRIVATE_FLAGS_DAEMON_THREAD | J9_PRIVATE_FLAGS_NO_OBJECT |
        J9_PRIVATE_FLAGS_SYSTEM_THREAD | J9_PRIVATE_FLAGS_ATTACHED_THREAD,
        compInfoPT->getOsThread());

   if (rc != JNI_OK)
      {
      compInfoPT->getCompThreadMonitor()->enter();
      compInfoPT->setCompilationThreadState(COMPTHREAD_ABORT);
      compInfoPT->getCompThreadMonitor()->notifyAll();
      compInfoPT->getCompThreadMonitor()->exit();
      return JNI_ERR;
      }

#if defined(LINUX)
   j9thread_set_name(j9thread_self(), "JIT Compilation");
#endif

   compInfo->acquireCompMonitor(compThread);
   compInfo->debugPrint(compThread, "+CM\n");
   if (compInfoPT->getCompThreadId() == 0)
      {
      compInfoPT->setCompilationThreadState(COMPTHREAD_ACTIVE);
      compInfo->incNumCompThreadsActive();
      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCompilationThreads))
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_INFO,"t=%6u Created compThread %d as ACTIVE",
                   (uint32_t)compInfo->getPersistentInfo()->getElapsedTime(), compInfoPT->getCompThreadId());
         }
      }
   else // all other compilation threads should start as suspended
      {
      compInfoPT->setCompilationThreadState(COMPTHREAD_SIGNAL_SUSPEND);
      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCompilationThreads))
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_INFO,"t=%6u Created compThread %d as SUSPENDED",
                   (uint32_t)compInfo->getPersistentInfo()->getElapsedTime(), compInfoPT->getCompThreadId());
         }
      }
   compInfo->releaseCompMonitor(compThread);

   compInfoPT->getCompThreadMonitor()->enter();
   compInfoPT->setCompilationThread(compThread);

   compInfoPT->getCompThreadMonitor()->notifyAll(); // just in case main thread is waiting for compThread to appear
   compInfoPT->getCompThreadMonitor()->exit();

   compInfo->debugPrint(compThread, "\tCompilation Thread attached to the VM\n");

   compInfo->debugPrint(compThread, "\tCompilation Thread acquiring compilation monitor\n");
   compInfo->acquireCompMonitor(compThread);

   // It is possible that the shutdown signal came before this thread has had the time
   // to become fully initialized. If that's the case, the state will appear as STOPPING
   // instead of UNINITIALIZED. This can happen when we destroy the cache; the java app
   // starts and finishes immediately
   if (compInfoPT->getCompilationThreadState() == COMPTHREAD_SIGNAL_TERMINATE)
      {
      compInfoPT->setCompilationThreadState(COMPTHREAD_STOPPING);
      // Release the monitor before calling DetachCurrentThread to avoid deadlock
      compInfo->releaseCompMonitor(compThread);
      if (compThread)
         vm->internalVMFunctions->DetachCurrentThread((JavaVM *) vm);

      // Re-acquire the monitor
      compInfo->acquireCompMonitor(compThread);
      compInfoPT->setCompilationThreadState(COMPTHREAD_STOPPED);
      compInfo->getCompilationMonitor()->notify();

      j9thread_exit((J9ThreadMonitor*)((TR::Monitor *)compInfo->getCompilationMonitor())->getVMMonitor());

      return 0; // unreachable
      }

   PORT_ACCESS_FROM_VMC(compThread);
   IDATA result;

#if defined(J9VM_PORT_SIGNAL_SUPPORT)
   if (!TR_NoStructuredHandler)
      {
      /* copied the flags from vmthread.c: note that for the JIT we are not really
         interested in continuing.  We are installing the handler merely to get
         support for crash dumps */
      U_32 flags = J9PORT_SIG_FLAG_SIGALLSYNC | J9PORT_SIG_FLAG_MAY_CONTINUE_EXECUTION;
      compThread->gpProtected = 1;

      I_32 sig_result = j9sig_protect((j9sig_protected_fn)protectedCompilationThreadProc, compInfoPT,
                                      vm->internalVMFunctions->structuredSignalHandler, compThread,
                                      flags, (UDATA*)&result);
      if (sig_result != 0)
         {
         result = JNI_ERR;
         }
      }
   else
#endif
      result = protectedCompilationThreadProc(privatePortLibrary, compInfoPT);

   j9thread_exit((J9ThreadMonitor*)((TR::Monitor *)compInfo->getCompilationMonitor())->getVMMonitor());

   return result;
   }

#if defined(LINUX)
#include "sched.h"
#endif

IDATA J9THREAD_PROC protectedCompilationThreadProc(J9PortLibrary *, TR::CompilationInfoPerThread*compInfoPT/*void *vmthread*/)
   {
   J9VMThread  * compThread = compInfoPT->getCompilationThread();//(J9VMThread *) vmthread;
   J9JavaVM    * vm        = compThread->javaVM;
   TR::CompilationInfo * compInfo = TR::CompilationInfo::get();

   //TODO: decide if the compilation budget should be per thread or not
   compInfo->setCompBudgetSupport(TR::Options::_compilationBudget > 0 &&
                                  compInfo->asynchronousCompilation() &&
                                  TR::Compiler->target.numberOfProcessors() < 4 &&
                                  j9thread_get_cpu_time(j9thread_self()) >= 0); // returns negative value if not supported

   compInfo->setIdleThreshold(IDLE_THRESHOLD/TR::Compiler->target.numberOfProcessors());


#ifdef J9VM_OPT_JAVA_OFFLOAD_SUPPORT
   if ( vm->javaOffloadSwitchOnWithReasonFunc != NULL )
      (* vm->javaOffloadSwitchOnWithReasonFunc)(compThread,  J9_JNI_OFFLOAD_SWITCH_JIT_COMPILATION_THREAD);
#endif

#if defined(WINDOWS)
   // affinity to proc 0

   /*
   if (TR::Options::getCmdLineOptions()->getOption(TR_CummTiming) ||
       TR::Options::getCmdLineOptions()->getOption(TR_Timing) ||
       TR::Options::getCmdLineOptions()->getOption(TR_EnableCompYieldStats))
      setThreadAffinity(j9thread_get_handle(j9thread_self())); // otherwise we get bad numbers
      */
   if (TR::Options::_compThreadAffinityMask)
      {
      uintptrj_t affinityMask = TR::Options::_compThreadAffinityMask;
      int32_t numBitsSetInMask = populationCount(affinityMask);
      int32_t bitNumberToSet = compInfoPT->getCompThreadId() % numBitsSetInMask;
      uintptrj_t myMask = 0x1;
      int32_t oneBits = 0;
      for (int32_t k = 0; k < 8*sizeof(uintptrj_t); k++, myMask <<= 1)
        {
        if (!(affinityMask & myMask))
           continue; // jump over bits that are 0
        // I found a bit that is set
        if (oneBits == bitNumberToSet)
           break;
        oneBits++;
        }
      setThreadAffinity(j9thread_get_handle(j9thread_self()), myMask);
      }
#elif defined(LINUX) && !defined(J9ZTPF)

   // affinity to proc 0
   if (TR::Options::_compThreadAffinityMask)
      {
      cpu_set_t cpuMask;
      CPU_ZERO(&cpuMask);
      UDATA mask = TR::Options::_compThreadAffinityMask;

      for (int cpuID=0; mask; cpuID++,mask>>=1)
         {
         if (mask & 1)
            CPU_SET(cpuID, &cpuMask);
         }

      if (sched_setaffinity(0, sizeof(cpuMask), &cpuMask) < 0)
         //   if (pthread_setaffinity_np(hThread, sizeof(cpuMask), &cpuMask) < 0)
         {
         perror("Error setting affinity");
         }
      else
         {
         //printf("Successfully set affinity of comp thread!\n");
         }
      }

#endif
   compInfoPT->run();
   compInfoPT->setCompilationThreadState(COMPTHREAD_STOPPING);

   compInfo->debugPrint(compThread, "\tstopping compilation thread loop\n");

   // print time spent in compilation thread (may work reliably only on Windows)
   static char * printCompTime = feGetEnv("TR_PrintCompTime");
   if (printCompTime)
      {
      fprintf(stderr, "Time spent in compilation thread =%u ms\n",
              (unsigned)(j9thread_get_self_cpu_time(j9thread_self())/1000000));
      }
   if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerbosePerformance))
      {
       TR_VerboseLog::writeLineLocked(TR_Vlog_PERF,"Time spent in compilation thread =%u ms",
          (unsigned)(j9thread_get_self_cpu_time(j9thread_self())/1000000));
      }

#ifdef J9VM_OPT_JAVA_OFFLOAD_SUPPORT
   if ( vm->javaOffloadSwitchOffWithReasonFunc != NULL )
      (* vm->javaOffloadSwitchOffWithReasonFunc)(compThread, J9_JNI_OFFLOAD_SWITCH_JIT_COMPILATION_THREAD);
#endif


   // Release the monitor before calling DetachCurrentThread to avoid deadlock
   compInfo->releaseCompMonitor(compThread);
   if (compThread)
      vm->internalVMFunctions->DetachCurrentThread((JavaVM *) vm);

   // Re-acquire the monitor and signal the shutdown thread
   compInfo->acquireCompMonitor(compThread);
   compInfoPT->setCompilationThreadState(COMPTHREAD_STOPPED);
   compInfo->getCompilationMonitor()->notify();
   //compInfoPT->setCompilationThread(0);
   return 0;
   }

JITServer::ServerStream *
TR::CompilationInfoPerThread::getStream()
   {
   return (_methodBeingCompiled) ? _methodBeingCompiled->_stream : NULL;
   }

void
TR::CompilationInfoPerThread::run()
   {
   TR::compInfoPT = this; // set the thread_local pointer to this object on first run
   for (
      CompilationThreadState threadState = getCompilationThreadState();
      threadState != COMPTHREAD_SIGNAL_TERMINATE;
      threadState = getCompilationThreadState()
      )
      {
      switch (threadState)
         {
         case COMPTHREAD_ACTIVE:
            {
            processEntries();
            break;
            }
         case COMPTHREAD_SIGNAL_WAIT:
            {
            waitForWork();
            break;
            }
         case COMPTHREAD_SIGNAL_SUSPEND:
            {
            doSuspend();
            break;
            }
         default:
            {
            TR_ASSERT(false, "No other state possible here %d\n", getCompilationThreadState());
            break;
            }
         }
      }
   }

void
TR::CompilationInfoPerThread::waitForWork()
   {
   TR::CompilationInfo * compInfo = getCompilationInfo();
   J9VMThread  * compThread = getCompilationThread();
   compInfo->debugPrint(compThread, "\tcompilation thread waiting for work\n");
   compInfo->debugPrint(compThread, "wait-CM\n");
   compInfo->incNumCompThreadsJobless();
   setLastTimeThreadWentToSleep(compInfo->getPersistentInfo()->getElapsedTime());
   setCompilationThreadState(COMPTHREAD_WAITING);
   compInfo->waitOnCompMonitor(compThread);
   if (getCompilationThreadState() == COMPTHREAD_WAITING)
      {
      /*
       * If we were signaled to suspend or stop,
       * we need to make sure to not lose that information.
       */
      setCompilationThreadState(COMPTHREAD_ACTIVE);
      }
   compInfo->decNumCompThreadsJobless();
   compInfo->debugPrint(compThread, "+CM\n");
   }

void
TR::CompilationInfoPerThread::doSuspend()
   {
   _compInfo.setSuspendThreadDueToLowPhysicalMemory(false);
   getCompThreadMonitor()->enter();
   setCompilationThreadState(COMPTHREAD_SUSPENDED);
   _compInfo.releaseCompMonitor(getCompilationThread());   // release the queue monitor before waiting
   setLastTimeThreadWasSuspended(_compInfo.getPersistentInfo()->getElapsedTime());
   setVMThreadNameWithFlag(getCompilationThread(), getCompilationThread(), getSuspendedThreadName(), 1);
   getCompThreadMonitor()->wait(); // wait here until someone notifies us
   setVMThreadNameWithFlag(getCompilationThread(), getCompilationThread(), getActiveThreadName(), 1);
   getCompThreadMonitor()->exit();
   _compInfo.acquireCompMonitor(getCompilationThread());
   }

J9::J9SegmentCache
TR::CompilationInfoPerThread::initializeSegmentCache(J9::J9SegmentProvider &segmentProvider)
   {
   try
      {
      J9::J9SegmentCache segmentCache(1 << 24, segmentProvider);
      return segmentCache;
      }
   catch (const std::bad_alloc &allocationFailure)
      {
      if (TR::Options::getVerboseOption(TR_VerbosePerformance))
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "Failed to initialize segment cache of size 1 << 24");
         }
      }
   try
      {
      J9::J9SegmentCache segmentCache(1 << 21, segmentProvider);
      return segmentCache;
      }
   catch (const std::bad_alloc &allocationFailure)
      {
      if (TR::Options::getVerboseOption(TR_VerbosePerformance))
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "Failed to initialize segment cache of size 1 << 21");
         }
      }
   J9::J9SegmentCache segmentCache(1 << 16, segmentProvider);
   return segmentCache;
   }

void
TR::CompilationInfoPerThread::processEntries()
   {
   if (TR::Options::getVerboseOption(TR_VerboseCompilationDispatch))
      {
      TR_VerboseLog::writeLineLocked(
         TR_Vlog_DISPATCH,
         "Starting to process queue entries. compThread=%d state=%d Q_SZ=%d Q_SZI=%d QW=%d\n",
         getCompThreadId(),
         getCompilationThreadState(),
         _compInfo.getMethodQueueSize(),
         _compInfo.getNumQueuedFirstTimeCompilations(),
         _compInfo.getQueueWeight()
         );
      }

   // Possible scenarios here
   // 1. We take a method from queue and compile it
   // 2. We refrain to take a method from queue because that means two simultaneous compilations --> wait to be notified
   // 3. We refrain to take a method from queue because compilation consumes too much CPU.
   // 3.1 If there is another active comp thread, then suspend this one. The activation should be avoided in this situation
   // 3.2 If this is the last active comp thread, wait on the comp monitor timed.
   // Note: the diagnostic thread must not wait.
   TR::CompilationInfo *compInfo = getCompilationInfo();
   J9VMThread  * compThread = getCompilationThread();
   try
      {
      J9::SegmentAllocator scratchSegmentAllocator(MEMORY_TYPE_JIT_SCRATCH_SPACE | MEMORY_TYPE_VIRTUAL, *_jitConfig->javaVM);
      J9::J9SegmentCache scratchSegmentCache(initializeSegmentCache(scratchSegmentAllocator).ref());
      while (getCompilationThreadState() == COMPTHREAD_ACTIVE)
      {
      TR::CompilationInfo::TR_CompThreadActions compThreadAction = TR::CompilationInfo::UNDEFINED_ACTION;
      TR_MethodToBeCompiled *entry = compInfo->getNextMethodToBeCompiled(this, _previousCompilationThreadState == COMPTHREAD_WAITING, &compThreadAction);
      switch (compThreadAction)
         {
         case TR::CompilationInfo::PROCESS_ENTRY:
            {
            // Compilation request extracted; go work on it
            TR_ASSERT(entry, "Attempting to process NULL entry");
            processEntry(*entry, scratchSegmentCache);
            break;
            }
         case TR::CompilationInfo::GO_TO_SLEEP_EMPTY_QUEUE:
            {
            // This compilation thread goes to sleep because there is no more work to do

            // NOTE:
            //      if we are a diagnostic thread and we see an empty queue,
            //      then it is either an error, or we have finished all of our
            //      work; we should broadcast this fact because in the error
            //      case, there is a possibility of deadlock
            if (isDiagnosticThread() && TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseDump))
               TR_VerboseLog::writeLineLocked(TR_Vlog_JITDUMP, "diagnostic thread encountered an empty queue");

            setCompilationThreadState(COMPTHREAD_WAITING);
            setLastTimeThreadWentToSleep(compInfo->getPersistentInfo()->getElapsedTime());
            int64_t waitTimeMillis = 256;
            intptr_t monitorStatus = compInfo->waitOnCompMonitorTimed(compThread, waitTimeMillis, 0);
            if (getCompilationThreadState() == COMPTHREAD_WAITING)
               {
               /*
                * A status of 0 indicates that 'the monitor has been waited on, notified, and reobtained.'
                */
               if (monitorStatus == 0)
                  /*
                   * If we were signaled to suspend or stop,
                   * we need to make sure to not lose that information.
                   */
                  {
                  setCompilationThreadState(COMPTHREAD_ACTIVE);
                  }
               else
                  {
                  TR_ASSERT(monitorStatus == J9THREAD_TIMED_OUT, "Unexpected monitor state");
                  // It's possible that a notification was sent just after the
                  // timeout expired. Hence, if something was added to the compilation
                  // queue we must attempt to process it
                  if (compInfo->getMethodQueueSize() > 0)
                     {
                     setCompilationThreadState(COMPTHREAD_ACTIVE);
                     }
                  else
                     {
                     setCompilationThreadState(COMPTHREAD_SIGNAL_WAIT);
                     }
                  if (TR::Options::getVerboseOption(TR_VerboseCompilationDispatch))
                     {
                     TR_VerboseLog::writeLineLocked(TR_Vlog_DISPATCH, "compThread=%d woke up after timeout on compMonitor\n", getCompThreadId());
                     }
                  }
               }
            break;
            }
         case TR::CompilationInfo::GO_TO_SLEEP_CONCURRENT_EXPENSIVE_REQUESTS:
            setCompilationThreadState(COMPTHREAD_SIGNAL_WAIT);
            break;

         case TR::CompilationInfo::SUSPEND_COMP_THREAD_EXCEED_CPU_ENTITLEMENT:
         case TR::CompilationInfo::SUSPEND_COMP_THREAD_EMPTY_QUEUE:
            TR_ASSERT(compInfo->getNumCompThreadsActive() > 1, "Should not suspend the last active compilation thread: %d\n", compInfo->getNumCompThreadsActive());
            setCompilationThreadState(COMPTHREAD_SIGNAL_SUSPEND);
            compInfo->decNumCompThreadsActive();
            if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCompilationThreads))
               {
               TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, "t=%6u Suspending compThread %d due to %s Qweight=%d active=%d overallCompCpuUtil=%d",
                  (uint32_t)compInfo->getPersistentInfo()->getElapsedTime(),
                  getCompThreadId(),
                  compThreadAction == TR::CompilationInfo::SUSPEND_COMP_THREAD_EXCEED_CPU_ENTITLEMENT ? "exceeding CPU entitlement" : "empty queue",
                  compInfo->getQueueWeight(),
                  compInfo->getNumCompThreadsActive(),
                  compInfo->getOverallCompCpuUtilization());
               }
            break;

         case TR::CompilationInfo::THROTTLE_COMP_THREAD_EXCEED_CPU_ENTITLEMENT:
            {
            TR_ASSERT(compInfo->getNumCompThreadsActive() <= 1, "Throttling when we have several comp threads active: %d\n", compInfo->getNumCompThreadsActive());
            int32_t sleepTimeMs = compInfo->computeCompThreadSleepTime(getLastCompilationDuration());
            if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCompilationThreads))
               {
               TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, "t=%6u compThread %d sleeping %d ms due to throttling Qweight=%d active=%d overallCompCpuUtil=%d",
                  (uint32_t)compInfo->getPersistentInfo()->getElapsedTime(),
                  getCompThreadId(),
                  sleepTimeMs,
                  compInfo->getQueueWeight(),
                  compInfo->getNumCompThreadsActive(),
                  compInfo->getOverallCompCpuUtilization());
               }
            compInfo->debugPrint(compThread, "\tcompilation thread waiting some time\n");
            compInfo->debugPrint(compThread, "wait-CM\n");
            // Don't increment jobless here because there is work to be done
            // The thread will wake up after specified time and process the request at front of queue
            setCompilationThreadState(COMPTHREAD_WAITING);
            setLastTimeThreadWentToSleep(compInfo->getPersistentInfo()->getElapsedTime());
            intptr_t monitorStatus = compInfo->waitOnCompMonitorTimed(compThread, sleepTimeMs, 0);
            if (getCompilationThreadState() == COMPTHREAD_WAITING)
               {
               /*
                * If we were signaled to suspend or stop,
                * we need to make sure to not lose that information.
                */
               setCompilationThreadState(COMPTHREAD_ACTIVE);
               }
            compInfo->debugPrint(compThread, "+CM\n");
            }
            break;

         default:
            TR_ASSERT_FATAL(false, "Invalid action: %d\n", compThreadAction);
            break;
         }
      }
      }
   catch (const std::bad_alloc &allocationFailure)
      {
      TR_ASSERT(!TR::Compiler->host.is64Bit() ,"Virtual memory exhaustion is unexpected on 64 bit platforms.");
      compInfo->setRampDownMCT();
      if (compInfo->getNumCompThreadsActive() > 1)
         {
         if (compilationThreadIsActive())
            {
            setCompilationThreadState(COMPTHREAD_SIGNAL_SUSPEND);
            compInfo->decNumCompThreadsActive();
            if (TR::Options::getVerboseOption(TR_VerboseCompilationThreads) || TR::Options::getVerboseOption(TR_VerbosePerformance))
               {
               TR_VerboseLog::writeLineLocked(TR_Vlog_PERF,"t=%6u Suspending compilation thread %d due to insufficient resources",
                  (uint32_t)compInfo->getPersistentInfo()->getElapsedTime(), getCompThreadId());
               }
            }
         }
      else
         {
         /*
          * Even the last compilation thread could not allocate an initial segment
          * Memory is hopelessly fragmented: stop all compilations
          */
         compInfo->getPersistentInfo()->setDisableFurtherCompilation(true);
         if (TR::Options::getVerboseOption(TR_VerboseCompilationThreads) ||
             TR::Options::getVerboseOption(TR_VerbosePerformance) ||
             TR::Options::getVerboseOption(TR_VerboseCompFailure))
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_PERF,"t=%6u Disable further compilation due to OOM while processing compile entries", (uint32_t)compInfo->getPersistentInfo()->getElapsedTime());
            }
         compInfo->purgeMethodQueue(compilationVirtualAddressExhaustion);
         // Must change the state to prevent an infinite loop
         // Change it to COMPTHREAD_SIGNAL_WAIT because the compilation queue is empty
         setCompilationThreadState(COMPTHREAD_SIGNAL_WAIT);
         }
      }

   static bool enableJITaaSPerCompConn = feGetEnv("TR_EnableJITaaSPerCompConn") ? true : false;
   if (compInfo->getPersistentInfo()->getRemoteCompilationMode() == JITServer::CLIENT && !enableJITaaSPerCompConn)
      {
      JITServer::ClientStream *client = getClientStream();
      if (client)
         {
         // Inform the server that client is closing the connection with a connectionTerminate message
         if (JITaaSHelpers::isServerAvailable())
            {
            try
               {
               client->writeError(JITServer::MessageType::connectionTerminate, 0 /* placeholder */);
               }
            catch (const JITServer::StreamFailure &e)
               {
               if (TR::Options::getVerboseOption(TR_VerboseJITServer))
                  TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "JITServer StreamFailure when sending connectionTerminate: %s", e.what());
               }
            }

         client->~ClientStream();
         TR_Memory::jitPersistentFree(client);
         setClientStream(NULL);
         }
      }
   }

void
TR::CompilationInfoPerThread::processEntry(TR_MethodToBeCompiled &entry, J9::J9SegmentProvider &scratchSegmentProvider)
   {
   TR::CompilationInfo *compInfo = getCompilationInfo();
   J9VMThread *compThread = getCompilationThread();
   TR::IlGeneratorMethodDetails & details = entry.getMethodDetails();
   J9Method *method = details.getMethod();

   setMethodBeingCompiled(&entry); // must have compilation monitor

   // Increase main queue weight while still holding compilation monitor
   if (entry._reqFromSecondaryQueue || entry._reqFromJProfilingQueue)
      compInfo->increaseQueueWeightBy(entry._weight);

   compInfo->printQueue();

   entry._compInfoPT = this; // need to know which comp thread is handling this request

   // update the last time the compilation thread had to do something.
   compInfo->setLastReqStartTime(compInfo->getPersistentInfo()->getElapsedTime());

   // Update how many compilation threads are working on hot/scorching methods
   if (entry._weight >= TR::Options::_expensiveCompWeight)
      {
      compInfo->incNumCompThreadsCompilingHotterMethods(); // must be called with compMonitor in hand
      entry._hasIncrementedNumCompThreadsCompilingHotterMethods = true;
      }

   // Now we no longer need the monitor - it is not held during compilation
   //
   compInfo->debugPrint(compThread, "\tcompilation thread releasing monitor before compile\n");
   compInfo->debugPrint(compThread, "-CM\n");
   compInfo->releaseCompMonitor(compThread);

   // We cannot examine the fields of method because it might have been unloaded
   // First we need to acquire VM access or prevent unloading
   //
   // Acquire VM access
   //
   acquireVMAccessNoSuspend(compThread);
   compInfo->debugPrint(compThread, "+VMacc\n");

   // If GC is in the middle of a duty cycle, wait for the cycle to finish
   // This is for RealTime only. Note that we need to perform this operation with VMAccess
   // in hand. Otherwise, after the comp thread has found that GC is not active but before
   // it acquires VM access, the GC could start a cycle and decide to unload a method
   // which will be later pinned by the compilation thread. The problem is that in realtime
   // GC does not execute all the work atomically. It may decide to unload some classes now,
   // then pause for a while, then coming back and doing the actual unloading without
   // verifying again that the class has been pinned
   //
   if (TR::Options::getCmdLineOptions()->realTimeGC())
      waitForGCCycleMonitor(true); // the parameter indicates that the thread has VM access

   if (!shouldPerformCompilation(entry))
      {
      if (TR::Options::getVerboseOption(TR_VerboseCompilationDispatch))
         {
         TR_VerboseLog::writeLineLocked(
         TR_Vlog_DISPATCH,
         "Rejecting compilation request for j9m=%p. unloaded=%d fromJPQ=%d",
         entry.getMethodDetails().getMethod(), entry._unloadedMethod, entry._reqFromJProfilingQueue);
         }

      // Acquire compilation monitor
      //
      compInfo->debugPrint(compThread, "\tacquiring compilation monitor\n");
      compInfo->acquireCompMonitor(compThread);
      compInfo->debugPrint(compThread, "+CM\n");

      // Release VM access
      //
      compInfo->debugPrint(compThread, "\tcompilation thread releasing VM access\n");
      releaseVMAccess(compThread);
      compInfo->debugPrint(compThread, "-VMacc\n");
      // decrease the queue weight
      compInfo->decreaseQueueWeightBy(entry._weight);
      // Update how many compilation threads are working on hot/scorching methods
      if (entry._hasIncrementedNumCompThreadsCompilingHotterMethods)
         compInfo->decNumCompThreadsCompilingHotterMethods();

      // put the request back into the pool
      //
      setMethodBeingCompiled(NULL); // Must have the compQmonitor
      compInfo->recycleCompilationEntry(&entry);

      // there are no threads to be notified because either this is an async request
      // or a sync request where classes have been redefined in which case the jit hook will to the signalling
      return;
      }

   // Pin the class of the method being compiled to prevent it from being unloaded
   //
   // This conversion is safe. The macro J9VM_J9CLASS_TO_HEAPCLASS will not make a conversion if Classes on Heap is not enabled.
   jobject classObject = NULL;
   if (!entry.isOutOfProcessCompReq())
      classObject = compThread->javaVM->internalVMFunctions->j9jni_createLocalRef((JNIEnv*)compThread, J9VM_J9CLASS_TO_HEAPCLASS(details.getClass()));

   // Do the hack for newInstance thunks
   // Also make the method appear as interpreted, otherwise we might want to access recompilation info
   // FIXME: do this more elegantly
   //
   //TR::IlGeneratorMethodDetails & details = entry->getMethodDetails();
   if (details.isNewInstanceThunk())
      {
      J9::NewInstanceThunkDetails &newInstanceDetails = static_cast<J9::NewInstanceThunkDetails &>(details);
      J9Class  *classForNewInstance = newInstanceDetails.classNeedingThunk();
      TR::CompilationInfo::setJ9MethodExtra(method,(uintptrj_t)classForNewInstance | J9_STARTPC_NOT_TRANSLATED);
      }

#ifdef STATS
   statQueueSize.update(compInfo->getMethodQueueSize());
#endif
   bool shouldAddToUpgradeQueue = false;

   TR_ASSERT(entry._optimizationPlan, "Must have an optimization plan");

   // The server should not adjust the opt plan requested by the client.
   if (!entry.isOutOfProcessCompReq())
      TR::CompilationController::getCompilationStrategy()->adjustOptimizationPlan(&entry, 0);

   shouldAddToUpgradeQueue = entry._optimizationPlan->shouldAddToUpgradeQueue();

   entry._tryCompilingAgain = false; // this field may be set by compile()

   // The following call will return with compilation monitor and the
   // queue slot (entry) monitor in hand
   //
   void *startPC = compile(compThread, &entry, scratchSegmentProvider);

   // Unpin the class
   if (!entry.isOutOfProcessCompReq())
      compThread->javaVM->internalVMFunctions->j9jni_deleteLocalRef((JNIEnv*)compThread, classObject);

   // Update how many compilation threads are working on hot/scorching methods
   if (entry._hasIncrementedNumCompThreadsCompilingHotterMethods)
      compInfo->decNumCompThreadsCompilingHotterMethods();

   entry._newStartPC = startPC;

   if (startPC && startPC != entry._oldStartPC)
      {
      compInfo->debugPrint("\tcompilation succeeded for method", details, compThread);
      // Add upgrade request to secondary queue if we downgraded, this is not GCR and method
      // looks important enough that an upgrade is justified
      if (shouldAddToUpgradeQueue && entry._compErrCode == compilationOK)
         {
         // The flag could have been reset by GCR or other reasons
         if (entry._optimizationPlan->shouldAddToUpgradeQueue())
            {
            // clone this request, adjust its _oldStartPC to match the _newStartPC for the
            // this request, and insert it in the low upgrade queue
            compInfo->getLowPriorityCompQueue().addUpgradeReqToLPQ(getMethodBeingCompiled());
#ifdef STATS
            fprintf(stderr, "Downgraded request will be added to upgrade queue\n");
#endif
            }
         }
      }
   else // compilation failure
      {
      compInfo->debugPrint("\tcompilation failed for method", details, compThread);
      }
   // Update statistics regarding the compilation status (including compilationOK)
   compInfo->updateCompilationErrorStats((TR_CompilationErrorCode)entry._compErrCode);

   // Copy this because it is needed later and entry may be recycled
   bool tryCompilingAgain = entry._tryCompilingAgain;

   if (entry._tryCompilingAgain)
      {
      // We got interrupted during the compile, and want to requeue this compilation request
      //
      TR_ASSERT(!entry._unloadedMethod, "For unloaded methods, it should not be possible to try compiling again");

      CompilationPriority oldPriority = (CompilationPriority)entry._priority;
      if (oldPriority <= CP_ASYNC_MAX)
         {
         oldPriority = CP_ASYNC_MAX;
         // While we increased the priority of this request, there might be a SYNC request in the queue
         // which has higher priority. In that case we should remove the possible reservation of a dataCache
         if (reservedDataCache() && compInfo->getMethodQueue() && compInfo->getMethodQueue()->_priority >= CP_ASYNC_MAX)
            {
            TR_DataCacheManager::getManager()->makeDataCacheAvailable(reservedDataCache());
            setReservedDataCache(NULL);
            }
         }
      else
         {
         oldPriority = CP_SYNC_BELOW_MAX;
         }

      entry._priority = oldPriority;
      --entry._compilationAttemptsLeft;
      entry._hasIncrementedNumCompThreadsCompilingHotterMethods = false; // re-initialize
      entry._GCRrequest = false; // pretend it's not GCR
      entry._reqFromSecondaryQueue = TR_MethodToBeCompiled::REASON_NONE; // we are going to put this into the main queue, so entry is not coming from LPQ anymore
      // TODO: the following is needed or otherwise we will wrongly increase the Q weight when extracting it again
      // However, we lose our _reqFromJProfilingQueue flag so we will not insert profiling trees
      // We need some other form of flag, maybe in the optimization plan or
      // retrials of compilations from JPQ are not going to work correctly
      entry._reqFromJProfilingQueue = false;

      entry.unsetRemoteCompReq(); // remote compilation decisions do not carry over from one retrial to the next

      compInfo->debugPrint("\trequeueing interrupted compilation request", details, compThread);

      // After releasing the monitors and vm access below, we will loop back to the head of the loop, and retry the
      // compilation.  Do not put the request back into the pool, instead requeue.
      //
      requeue();
      setMethodBeingCompiled(NULL); // Must have the compQmonitor
      }
   else // compilation will not be retried
      {
      //fprintf(stderr, "compilation thread will free optimization plan %p %d\n", entry->_optimizationPlan, entry->_optimizationPlan->creatorCanFreePlan());
      TR_OptimizationPlan::freeOptimizationPlan(entry._optimizationPlan); // we no longer need the optimization plan
      // decrease the queue weight
      compInfo->decreaseQueueWeightBy(entry._weight);
      // Put the request back into the pool
      setMethodBeingCompiled(NULL);
      compInfo->recycleCompilationEntry(&entry);

      // notify the sleeping threads that the compilation request has been served
      // (either it succeeded or it failed and we are not going to requeue).
      //
      compInfo->debugPrint(compThread, "\tnotify sleeping threads that the compilation is done\n");
      entry.getMonitor()->notifyAll();
      compInfo->debugPrint(compThread, "ntfy-", &entry);
      }

   compInfo->printQueue();

   // Release the queue slot monitor
   //
   compInfo->debugPrint(compThread, "\tcompilation thread releasing queue slot monitor\n");
   compInfo->debugPrint(compThread, "-AM-", &entry);
   entry.releaseSlotMonitor(compThread);

   // At this point we should always have VMAccess
   // We should always have the compilation monitor
   // we should never have classUnloadMonitor

   // Release VM access
   //
   compInfo->debugPrint(compThread, "\tcompilation thread releasing VM access\n");
   releaseVMAccess(compThread);
   compInfo->debugPrint(compThread, "-VMacc\n");

   // We can suspend this thread if too many are active
   if (
      !(isDiagnosticThread()) // must not be reserved for log
      && compInfo->getNumCompThreadsActive() > 1 // we should have at least one active besides this one
      && compilationThreadIsActive() // We haven't already been signaled to suspend or terminate
      && (
         compInfo->getRampDownMCT() // force to have only one thread active
         || compInfo->getSuspendThreadDueToLowPhysicalMemory()
         || (
            !tryCompilingAgain
            /*&& compInfoPT->getCompThreadId() != 0*/
            && TR::Options::getCmdLineOptions()->getOption(TR_SuspendEarly)
            && compInfo->getQueueWeight() < TR::CompilationInfo::getCompThreadSuspensionThreshold(compInfo->getNumCompThreadsActive())
            )
         )
      )
      {
      // Suspend this thread
      setCompilationThreadState(COMPTHREAD_SIGNAL_SUSPEND);
      compInfo->decNumCompThreadsActive();
      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCompilationThreads))
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, "t=%6u Suspend compThread %d Qweight=%d active=%d %s %s",
            (uint32_t)compInfo->getPersistentInfo()->getElapsedTime(),
            getCompThreadId(),
            compInfo->getQueueWeight(),
            compInfo->getNumCompThreadsActive(),
            compInfo->getRampDownMCT() ? "RampDownMCT" : "",
            compInfo->getSuspendThreadDueToLowPhysicalMemory() ? "LowPhysicalMem" : "");
         }
      // If the other remaining active thread(s) are sleeping (maybe because
      // we wanted to avoid two concurrent hot requests) we need to wake them
      // now as a preventive measure. Worst case scenario they will go back to sleep
      if (compInfo->getNumCompThreadsJobless() > 0)
         {
         compInfo->getCompilationMonitor()->notifyAll();
         if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCompilationThreads))
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, "t=%6u compThread %d notifying other sleeping comp threads. Jobless=%d",
               (uint32_t)compInfo->getPersistentInfo()->getElapsedTime(),
               getCompThreadId(),
               compInfo->getNumCompThreadsJobless());
            }
         }
      if (tryCompilingAgain)
         {
         // For compilation retrial we normally keep the data cache
         // Because another thread may handle the request we have to unreserve any data cache
         if (reservedDataCache())
            {
            TR_DataCacheManager::getManager()->makeDataCacheAvailable(reservedDataCache());
            setReservedDataCache(NULL);
            // TODO: write a message into the trace buffer because this is a corner case
            }
         }
      }
   else // We will not suspend this thread
      {
      // If the low memory flag was set but there was no additional comp thread
      // to suspend, we must clear the flag now
      if (compInfo->getSuspendThreadDueToLowPhysicalMemory() &&
         compInfo->getNumCompThreadsActive() < 2)
         compInfo->setSuspendThreadDueToLowPhysicalMemory(false);
      }
   }

bool
TR::CompilationInfoPerThread::shouldPerformCompilation(TR_MethodToBeCompiled &entry)
   {
   TR::CompilationInfo *compInfo = getCompilationInfo();
   if (entry.isOutOfProcessCompReq())
      return true;
   TR::IlGeneratorMethodDetails &details = entry.getMethodDetails();
   J9Method *method = details.getMethod();

   // Do not compile unloaded methods
   if (entry._unloadedMethod)
      return false;

   // Do not compile replaced methods
   if ((TR::Options::getCmdLineOptions()->getOption(TR_EnableHCR) || TR::Options::getCmdLineOptions()->getOption(TR_FullSpeedDebug))
        && details.getClass() && J9_IS_CLASS_OBSOLETE(details.getClass()))
      return false;


   if (entry._reqFromSecondaryQueue)
      {
      // This is a request coming from the low priority queue
      bool doCompile = false;

      // Determine if this was a fast comp req for an interpreted method
      if (!entry._oldStartPC)
         {
         // Set method->extra = J9_JIT_QUEUED_FOR_COMPILATION
         // and stimulate a compilation (if conditions are right)
         if (!(J9_ROM_METHOD_FROM_RAM_METHOD(method)->modifiers & J9AccNative)) // never change the extra field of a native method
            {
            if (!TR::CompilationInfo::isCompiled(method))  // Don't touch methods compiled through other means
               {
               if (compInfo->getInvocationCount(method) > 0) // Valid invocation counter
                  {
                  // FIXME: is this safe here?
                  // What if the method is  about to be compiled?
                  compInfo->setJ9MethodVMExtra(method, J9_JIT_QUEUED_FOR_COMPILATION);
                  doCompile = true;
                  }
               }
            else // compiled body
               {
               compInfo->getLowPriorityCompQueue().incStatsBypass(); // statistics
               }
            }
         // stop tracking this method in the hashtable
         if (compInfo->getLowPriorityCompQueue().isTrackingEnabled())
            compInfo->getLowPriorityCompQueue().stopTrackingMethod(method);
         }
      else // RE-compilation request from LPQ
         {
         TR_ASSERT(entry._reqFromSecondaryQueue == TR_MethodToBeCompiled::REASON_UPGRADE, "wrong reason for upgrade");
         // This method might have failed compilation or have been rejected due to filters
         if (TR::CompilationInfo::isCompiled(method))
            {
            void *startPC = TR::CompilationInfo::getJ9MethodStartPC(method);
            // A compilation attempt might have already happened for this method
            // (and failed or succeeded)
            TR_LinkageInfo *linkageInfo = TR_LinkageInfo::get(startPC);
            if (!linkageInfo->isBeingCompiled()) // This field is never reset
               {
               // get its level
               TR_PersistentJittedBodyInfo *bodyInfo = TR::Recompilation::getJittedBodyInfoFromPC(startPC);
               // The method might have already been recompiled
               // Or a compilation request might have been attempted and failed. The linkage
               // word could tell us if this body has ever been queued for compilation

               if (bodyInfo && bodyInfo->getHotness() < warm)
                  {
                  doCompile = true;
                  linkageInfo->setIsBeingRecompiled();
                  TR_PersistentMethodInfo *methodInfo = bodyInfo->getMethodInfo();
                  // adjust the level in persistentMethodInfo as well
                  methodInfo->setNextCompileLevel(entry._optimizationPlan->getOptLevel(), false);
                  // set the reason for recompilation
                  methodInfo->setReasonForRecompilation(TR_PersistentMethodInfo::RecompDueToSecondaryQueue);
                  }
               }
            else
               {
               compInfo->getLowPriorityCompQueue().incStatsBypass(); // statistics
               }
            }
         else
            {
            // This case corresponds to a situation where we had a downgraded first time compilation
            // into the main queue and an upgrade into the LPQ. The request from the main queue fails
            // and we now have to execute the upgrade.
            // Do not do it
            }
         }
      if (!doCompile)
         return false;
      }
   else if (entry._reqFromJProfilingQueue)
      {
      // We should not compile a request from JProfiling queue if the method has been
      // recompiled since we put this request in the JPQ. Our expectation is that
      // the current startPC is entry->_oldStartPC
      void *startPC = TR::CompilationInfo::getJ9MethodStartPC(method);
      if (startPC != entry._oldStartPC)
         return false;

      // A compilation request might have been attempted and failed. The linkage
      // word could tell us if this body has ever been queued for compilation
      TR_LinkageInfo *linkageInfo = TR_LinkageInfo::get(startPC);
      if (linkageInfo->isBeingCompiled()) // This field is never reset
         return false;
      linkageInfo->setIsBeingRecompiled();
      TR_PersistentJittedBodyInfo *bodyInfo = TR::Recompilation::getJittedBodyInfoFromPC(startPC);
      // bodyInfo must exist because requests from JQP are always recompilations
      TR_PersistentMethodInfo *methodInfo = bodyInfo->getMethodInfo();
      // set the reason for recompilation
      methodInfo->setReasonForRecompilation(TR_PersistentMethodInfo::RecompDueToJProfiling);
      // adjust the level in persistentMethodInfo as well
      methodInfo->setNextCompileLevel(entry._optimizationPlan->getOptLevel(), false);
      }
   return true;
   }

TR_MethodToBeCompiled *
TR::CompilationInfo::addMethodToBeCompiled(TR::IlGeneratorMethodDetails & details, void *pc,
                                          CompilationPriority priority, bool async,
                                          TR_OptimizationPlan *optimizationPlan, bool *queued,
                                          TR_YesNoMaybe methodIsInSharedCache)
   {
   // Make sure the entry is consistent
   //
#if DEBUG
   if (pc)
      {
      TR_PersistentMethodInfo* methodInfo = TR::Recompilation::getMethodInfoFromPC(pc);
      if (methodInfo)
         TR_ASSERT(details.getMethod() == (J9Method *)methodInfo->getMethodInfo(), "assertion failure");
      }
#endif

   // Add this method to the queue of methods waiting to be compiled.
   TR_MethodToBeCompiled *cur = NULL, *prev = NULL;
   uint32_t queueWeight = 0; // QW
   int32_t numEntries = 0;

   // See if the method is already in the queue or is already being compiled
   //
   J9VMThread *vmThread = _jitConfig->javaVM->internalVMFunctions->currentVMThread(_jitConfig->javaVM);
   TR_J9VMBase * fe = TR_J9VMBase::get(_jitConfig, vmThread);

   // search among the threads
   for (uint8_t i = 0; i < getNumTotalCompilationThreads(); i++)
      {
      TR::CompilationInfoPerThread *curCompThreadInfoPT = _arrayOfCompilationInfoPerThread[i];
      TR_ASSERT(curCompThreadInfoPT, "a thread's compinfo is missing\n");

      TR_MethodToBeCompiled *compMethod = curCompThreadInfoPT->getMethodBeingCompiled();
      if (compMethod)
         {
         queueWeight += compMethod->_weight; // QW
         if (compMethod->getMethodDetails().sameAs(details, fe))
            {
            if (!compMethod->_unloadedMethod) // Redefinition; see cmvc 192606 and RTC 36898
               {
               // If the priority has increased, use the new priority.
               //
               if (compMethod->_priority < priority)
                  compMethod->_priority = priority;
               return compMethod;
               }
            }
         }
      }

   for (prev = NULL, cur = _methodQueue; cur; prev = cur, cur = cur->_next)
      {
      numEntries++;
      queueWeight += cur->_weight;
      if (cur->getMethodDetails().sameAs(details, fe))
         break;
      }

   // NOTE: we do not need to search the methodPool since we cannot reach here if an entry
   // for the compilation of this method is already in the pool.  Things are put in the pool
   // after the compilation completes and we have updated the J9Method etc.
   // in which case the compilation is already done and we should not even try to enqueue

   if (cur)
      {
      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCompileRequest))
         TR_VerboseLog::writeLineLocked(TR_Vlog_CR,"%p     Already present in compilation queue. OldPriority=%x NewPriority=%x entry=%p",
            _jitConfig->javaVM->internalVMFunctions->currentVMThread(_jitConfig->javaVM), cur->_priority, priority, cur);

      // Method is already in the queue.
      // If the startPC has changed, assume the new one is more recent.
      //
      if (pc)
         cur->_oldStartPC = pc;

      // If the priority has increased, use the new priority
      //
      if (cur->_priority < priority)
         cur->_priority = priority;
      // If the optimization level is higher, just upgrade
      // (unless the methods has excessive complexity)
      //
      if (cur->_optimizationPlan->getOptLevel() !=  optimizationPlan->getOptLevel())
         {
         if (cur->_optimizationPlan->getOptLevel() <  optimizationPlan->getOptLevel())
            cur->_optimizationPlan->setOptLevel(optimizationPlan->getOptLevel());
         if (pc)
            {
            TR_PersistentMethodInfo *methodInfo = TR::Recompilation::getMethodInfoFromPC(pc);
            if (methodInfo && methodInfo->getNextCompileLevel() != cur->_optimizationPlan->getOptLevel())
               methodInfo->setNextCompileLevel(cur->_optimizationPlan->getOptLevel(), cur->_optimizationPlan->insertInstrumentation());
            }
         }
      // If the position in the queue is still correct, just return
      //
      if (!prev || prev->_priority >= cur->_priority)
         return cur;

      // Must re-position in the queue
      //
      prev->_next = cur->_next; // take it out of the queue
      }

   // If method is not yet in the queue prepare the queue entry
   //
   else
      {
      if (queueWeight != _queueWeight) //QW
         {
         if (TR::Options::isAnyVerboseOptionSet())
            TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, "Discrepancy for queue weight while adding to queue: computed=%u recorded=%u\n", queueWeight, _queueWeight);
         // correction
         _queueWeight = queueWeight;
         }
      if (numEntries != _numQueuedMethods)
         {
         if (TR::Options::isAnyVerboseOptionSet())
            TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, "Discrepancy for queue size while adding to queue: Before adding numEntries=%d  _numQueuedMethods=%d\n", numEntries, _numQueuedMethods);
         TR_ASSERT(false, "Discrepancy for queue size while adding to queue");
         }

      cur = getCompilationQueueEntry();
      if (cur == NULL)  // Memory Allocation Failure.
         return NULL;

      cur->initialize(details, pc, priority, optimizationPlan);
      cur->_jitStateWhenQueued = getPersistentInfo()->getJitState();

      bool isJNINativeMethodRequest = false;
      if (pc)
         {
         TR_LinkageInfo *linkageInfo = TR_LinkageInfo::get(pc);
         linkageInfo->setIsBeingRecompiled(); // mark that we try to compile it

         // Update persistentMethodInfo with the level we want to compile to
         //
         TR_PersistentJittedBodyInfo* bodyInfo = TR::Recompilation::getJittedBodyInfoFromPC(pc);
         TR_ASSERT(bodyInfo, "We must have bodyInfo because we recompile");
         TR_PersistentMethodInfo* methodInfo = bodyInfo->getMethodInfo(); //TR::Recompilation::getMethodInfoFromPC(pc);
         TR_ASSERT(methodInfo, "We must have methodInfo because we recompile");
         methodInfo->setNextCompileLevel(optimizationPlan->getOptLevel(), optimizationPlan->insertInstrumentation());
         // check if this is an invalidation request and if so increase the appropriate counter
         //
         if (bodyInfo->getIsInvalidated())
            incNumInvRequestsQueued(cur);

         // Increment this only if recompilations are due to sampling
         if (linkageInfo->isSamplingMethodBody())
            {
            if (methodInfo->getReasonForRecompilation() != TR_PersistentMethodInfo::RecompDueToGCR)
               _intervalStats._numRecompilationsInInterval++;
            else
               incNumGCRRequestsQueued(cur);
            }
         // Should we bump the count of first time compilations?
         // If not, the danger is that there will be many GCR compilations and we enter STEADY state
         }
      else if (details.isOrdinaryMethod()) // ordinary interpreted methods
         {
         J9Method *method = details.getMethod();
         isJNINativeMethodRequest = isJNINative(method);
         if (method && async
            && (getInvocationCount(method) == 0)           // this will filter out JNI natives
            && !(details.getRomMethod()->modifiers & J9AccNative)) // Never change the extra field of a native method
            {
            setJ9MethodVMExtra(method, J9_JIT_QUEUED_FOR_COMPILATION);
            }
         _intervalStats._numFirstTimeCompilationsInInterval++;
         _numQueuedFirstTimeCompilations++;
         }
      cur->_entryTime = getPersistentInfo()->getElapsedTime(); // cheaper version
#if defined(J9VM_INTERP_AOT_RUNTIME_SUPPORT) && defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM))
      cur->_methodIsInSharedCache = methodIsInSharedCache;
#endif
      incrementMethodQueueSize(); // one more method added to the queue
      *queued = true; // set the flag that we added a new request to the queue

      Trc_JIT_CompRequest(vmThread, details.getMethod(), pc, !async, optimizationPlan->getOptLevel(), (int)priority, _numQueuedMethods);

      // Increase the queue weight
      uint8_t entryWeight; // must be less than 256
      if (!details.isOrdinaryMethod() || details.isNewInstanceThunk() || isJNINativeMethodRequest)
         entryWeight = THUNKS_WEIGHT; // 1
      else if (methodIsInSharedCache == TR_yes && !pc) // first time compilations that are AOT loads
         entryWeight = TR::Options::_weightOfAOTLoad;
      else if (optimizationPlan->getOptLevel() == warm) // most common case first
         {
         // Compilation may be downgraded to cold during classLoadPhase
         // While we cannot anticipate that classLoadPhase will last till the method
         // is extracted from the queue, this is the best estimate
         J9Method *method = details.getMethod();
         if (TR::CompilationInfo::isJSR292(details.getRomMethod()))
            {
            entryWeight = (uint32_t)TR::Options::_weightOfJSR292;
            }
         else if (getPersistentInfo()->isClassLoadingPhase() && !isCompiled(method) &&
            !TR::Options::getCmdLineOptions()->getOption(TR_DontDowngradeToCold) &&
            TR::Options::getCmdLineOptions()->allowRecompilation()) // don't do it for fixed level
            {
            entryWeight = COLD_WEIGHT;
            }
         else
            {
            // Smaller methods are easier to compile
            const J9ROMMethod * romMethod = details.getRomMethod();
            uint32_t methodSize = getMethodBytecodeSize(romMethod);

            if (methodSize < 8)
               {
               entryWeight = COLD_WEIGHT;
               }
            else
               {
               // Loopless methods are easier to compile
               if (J9ROMMETHOD_HAS_BACKWARDS_BRANCHES(romMethod))
                  entryWeight = WARM_LOOPY_WEIGHT;
               else
                  entryWeight = WARM_LOOPLESS_WEIGHT;
               }
            }
         }
      else if (optimizationPlan->getOptLevel() == cold)
         entryWeight = COLD_WEIGHT;
      else if (optimizationPlan->getOptLevel() >= veryHot)
         entryWeight = VERY_HOT_WEIGHT;
      else if (optimizationPlan->getOptLevel() == hot)
         entryWeight = HOT_WEIGHT;
      else if (optimizationPlan->getOptLevel() == noOpt)
         entryWeight = NOOPT_WEIGHT;
      else
         {
         entryWeight = THUNKS_WEIGHT;
         TR_ASSERT(false, "Unknown hotness level %d\n", optimizationPlan->getOptLevel());
         }

      // Other tweaks go here: method size, profiling bodies, loopy/loopless, classLoadPhase, sync/async
      cur->_weight = entryWeight;
      increaseQueueWeightBy(entryWeight);

      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCompileRequest))
         TR_VerboseLog::writeLineLocked(TR_Vlog_CR, "%p   Added entry %p of weight %d to comp queue. Now Q_SZ=%d weight=%d",
            _jitConfig->javaVM->internalVMFunctions->currentVMThread(_jitConfig->javaVM), cur, entryWeight, getMethodQueueSize(), getQueueWeight());

      // Examine if we need to activate a new thread
      TR_YesNoMaybe activate = shouldActivateNewCompThread();
      if (activate == TR_maybe &&
          TR::Options::getCmdLineOptions()->getOption(TR_ActivateCompThreadWhenHighPriReqIsBlocked) &&
          getNumCompThreadsActive() == 1 &&
          priority >= CP_ASYNC_BELOW_MAX) // AOT loads and JNI
         {
         TR::CompilationInfoPerThread * lowPriCompThread = findFirstLowPriorityCompilationInProgress(priority);
         if (lowPriCompThread)
            {
            activate = TR_yes;

            // Increase the weight of the blocking entry so that the newly activated thread
            // remains active until the blocking request gets compiled
            //
            TR_MethodToBeCompiled *lowPriCompReq = lowPriCompThread->getMethodBeingCompiled();
            uint8_t targetWeight = _compThreadSuspensionThresholds[2] <= 0xff ? _compThreadSuspensionThresholds[2] : 0xff;
            if (lowPriCompReq->_weight < targetWeight)
               {
               uint8_t diffWeight = targetWeight - lowPriCompReq->_weight;
               increaseQueueWeightBy(diffWeight);
               lowPriCompReq->_weight = targetWeight;
               }
            if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCompilationThreads))
               TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, "t=%6u High priority req (0x%x) blocked by priority 0x%x",
               (uint32_t)getPersistentInfo()->getElapsedTime(), priority, lowPriCompReq->_priority);
            }
         }

      if (activate == TR_yes)
         {
         // Must find one that is SUSPENDED/SUSPENDING
         TR::CompilationInfoPerThread *compInfoPT = getFirstSuspendedCompilationThread();
         if (compInfoPT) // FIXME this is null sometimes during shutdown. Why??
            {
            compInfoPT->resumeCompilationThread();
            if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCompilationThreads))
               {
               TR_VerboseLog::writeLineLocked(TR_Vlog_INFO,"t=%6u Activate compThread %d Qweight=%d active=%d",
                  (uint32_t)getPersistentInfo()->getElapsedTime(),
                  compInfoPT->getCompThreadId(),
                  getQueueWeight(),
                  getNumCompThreadsActive());
               }
            //fprintf(stderr, "Activate compthread %d: _numQueuedMethods=%d queueWeight=%d getNumCompThreadsJobless=%d numactive=%d\n",
            //   compInfoPT->getCompThreadId(), _numQueuedMethods, _queueWeight, getNumCompThreadsJobless(), getNumCompThreadsActive());
            }
         }
      }

   // Move the entry to the right place in the queue
   //
   queueEntry(cur);
   return cur;
   }

//--------------------------- queueEntry ---------------------------------
// Insert the compilation request in the queue at the appropriate place
// based on its priority. Must have compilationQueueMonitor in hanb
//------------------------------------------------------------------------
void TR::CompilationInfo::queueEntry(TR_MethodToBeCompiled *entry)
   {
   TR_ASSERT_FATAL(entry->_freeTag & ENTRY_INITIALIZED, "queuing an entry which is not initialized\n");

   entry->_freeTag |= ENTRY_QUEUED;

   if (!_methodQueue || _methodQueue->_priority < entry->_priority)
      {
      entry->_next = _methodQueue;
      _methodQueue = entry;
      }
   else
      {
      for (TR_MethodToBeCompiled *prev = _methodQueue; ; prev = prev->_next)
         {
         if (!prev->_next || prev->_next->_priority < entry->_priority)
            {
            entry->_next = prev->_next;
            prev->_next = entry;
            break;
            }
         }
      }
   }

//--------------------------------- requeue ----------------------------------
// Put the request that is currently being compiled, back into the queue
// and increment the number of queued methods
//----------------------------------------------------------------------------
void TR::CompilationInfoPerThread::requeue()
   {
   TR_ASSERT(_methodBeingCompiled, "Invalid use of requeue.");
   _compInfo.incrementMethodQueueSize();
   if (_methodBeingCompiled->getMethodDetails().isOrdinaryMethod() && _methodBeingCompiled->_oldStartPC==0)
       _compInfo._numQueuedFirstTimeCompilations++;
   if (_methodBeingCompiled->_entryIsCountedAsInvRequest)
      _compInfo.incNumInvRequestsQueued(_methodBeingCompiled);
   _methodBeingCompiled->_compErrCode = compilationOK; // reset the error code
   _compInfo.queueEntry(_methodBeingCompiled);
   _methodBeingCompiled = NULL;
   }

//------------------------- adjustCompilationEntryAndRequeue -----------------
// Search for the given method in the compilation queue
// If found, we may adjust the optimization plan. There is a very good
// likelihood that a matching entry will be found.
//----------------------------------------------------------------------------
TR_MethodToBeCompiled *
TR::CompilationInfo::adjustCompilationEntryAndRequeue(
                            TR::IlGeneratorMethodDetails &details,
                            TR_PersistentMethodInfo *methodInfo,
                            TR_Hotness newOptLevel, bool useProfiling,
                            CompilationPriority priority,
                            TR_J9VMBase *fe)
   {
   for (uint8_t i = 0; i < getNumUsableCompilationThreads(); i++)
      {
      TR::CompilationInfoPerThread *curCompThreadInfoPT = _arrayOfCompilationInfoPerThread[i];
      TR_ASSERT(curCompThreadInfoPT, "a thread's compinfo is missing\n");

      if (curCompThreadInfoPT->getMethodBeingCompiled() &&
          curCompThreadInfoPT->getMethodBeingCompiled()->getMethodDetails().sameAs(details, fe))
         return NULL; // didn't do anything
      }

   // Search the queue for my method
   TR_MethodToBeCompiled *cur, *prev;
   for (prev = NULL, cur = _methodQueue; cur; prev = cur, cur = cur->_next)
      {
      if (cur->getMethodDetails().sameAs(details, fe))
         break;
      }
   if (cur)
      {
      // here define the list of exclusions
      bool exclude = cur->getMethodDetails().isNewInstanceThunk() ||
                     cur->_compilationAttemptsLeft < MAX_COMPILE_ATTEMPTS; // do not do it for retrials
      // we could also do it only for upgrades cur->_optimizationPlan->isUpgradeRecompilation()
      if (exclude)
         {
         cur = NULL;
         }
      else
         {
         cur->_optimizationPlan->setOptLevel(newOptLevel);
         cur->_optimizationPlan->setInsertInstrumentation(useProfiling);
         methodInfo->setNextCompileLevel(newOptLevel, useProfiling);

         if (cur->_priority < priority)
            {
            // take the method out
            if (prev)
               prev->_next = cur->_next;
            else
               _methodQueue = cur->_next;
            // put it back at its proper place
            cur->_priority = priority;
            queueEntry(cur);
            }
         }
      //fprintf(stderr, "Adjusting optimization plan in the queue\n");
      }
   return cur;
   }

int32_t TR::CompilationInfo::promoteMethodInAsyncQueue(J9Method * method, void *pc)
   {
   // See if the method is already in the queue or is already being compiled
   //
   for (uint8_t i = 0; i < getNumUsableCompilationThreads(); i++)
      {
      TR::CompilationInfoPerThread *curCompThreadInfoPT = _arrayOfCompilationInfoPerThread[i];
      TR_ASSERT(curCompThreadInfoPT, "a thread's compinfo is missing\n");

      if (curCompThreadInfoPT->getMethodBeingCompiled() &&
          !curCompThreadInfoPT->getMethodBeingCompiled()->isDLTCompile() &&
          method == curCompThreadInfoPT->getMethodBeingCompiled()->getMethodDetails().getMethod())
         {
         changeCompThreadPriority(J9THREAD_PRIORITY_MAX, 9);
         return 0; // didn't do anything
         }
      }

   int i = 0;
   TR_MethodToBeCompiled *cur, *prev;
   for (prev = NULL, cur = _methodQueue; cur; prev = cur, cur = cur->_next)
      {
      if (!cur->isDLTCompile() && method == cur->getMethodDetails().getMethod())
         break;
      i++;
      }

   if (!cur || !prev || cur->_priority >= CP_ASYNC_MAX || prev->_priority >= CP_ASYNC_MAX)
      return -i;
   changeCompThreadPriority(J9THREAD_PRIORITY_MAX, 9);
   _statNumQueuePromotions++;
#ifdef STATS
   fprintf(stderr, "Promoting method in queue QSZ=%d\n", getMethodQueueSize());
#endif
   cur->_priority = CP_ASYNC_MAX;

   // take the method out
   prev->_next = cur->_next;
   // find insertion point
   if (_methodQueue->_priority < CP_ASYNC_MAX) // common case
      {
      cur->_next = _methodQueue;
      _methodQueue = cur;
      // FIXME: how about the compilation lag
      }
   else
      {
      for (prev = _methodQueue; prev->_next; prev = prev->_next)
         if (prev->_next->_priority < CP_ASYNC_MAX)
            {
            cur->_next = prev->_next;
            prev->_next = cur;
            break;
            }
      }
   return i;
   }

void TR::CompilationInfo::changeCompReqFromAsyncToSync(J9Method * method)
   {

   TR_MethodToBeCompiled *cur = NULL, *prev = NULL;
   // See if the method is already in the queue or is already being compiled
   //
   for (uint8_t i = 0; i < getNumUsableCompilationThreads(); i++)
      {
      TR::CompilationInfoPerThread *curCompThreadInfoPT = _arrayOfCompilationInfoPerThread[i];
      TR_ASSERT(curCompThreadInfoPT, "a thread's compinfo is missing\n");

      if (curCompThreadInfoPT->getMethodBeingCompiled() &&
          !curCompThreadInfoPT->getMethodBeingCompiled()->isDLTCompile() &&
          method == curCompThreadInfoPT->getMethodBeingCompiled()->getMethodDetails().getMethod())
         {
         if (curCompThreadInfoPT->getMethodBeingCompiled()->_priority <= CP_ASYNC_MAX)
            {
            curCompThreadInfoPT->getMethodBeingCompiled()->_priority = CP_SYNC_NORMAL;
            cur = curCompThreadInfoPT->getMethodBeingCompiled();
            break;
            }
         }
      }
   if (!cur)
      {
      for (cur = _methodQueue; cur; prev = cur, cur = cur->_next)
         if (!cur->isDLTCompile() && method == cur->getMethodDetails().getMethod())
            break;
      // Check if this is an asynchronous request
      //
      if (cur && cur->_priority <= CP_ASYNC_MAX)
         {
         // Take the method out, increase its priority and insert it at the proper place
         //
         cur->_priority = CP_SYNC_NORMAL;
         if (prev)
            {
            prev->_next = cur->_next;
            queueEntry(cur);
            }
         else // method already at the top of the queue
            {
            // Nothing to do
            }
         }
      else
         {
         cur = NULL; // prevent further processing
         }
      }
   if (cur)
      {
      TR_ASSERT(!cur->_changedFromAsyncToSync, "_changedFromAsyncToSync must be false");
      J9Method *method = cur->getMethodDetails().getMethod();
      cur->_changedFromAsyncToSync = true;  // Flag the method as synchronous
      // Allow new invocations to trigger compilations
      if (getJ9MethodVMExtra(method) == J9_JIT_QUEUED_FOR_COMPILATION)
         setJ9MethodVMExtra(method, 1);
      // fprintf(stderr, "Changed method %p priority from async to sync\n", method);
      }
   }

// Must hold compilation queue monitor in hand
// Should only be used when compiling on separate thread, otherwise
// _arrayOfCompilationInfoPerThread does not exist (or is full of null pointers)
TR_MethodToBeCompiled * TR::CompilationInfo::requestExistsInCompilationQueue(TR::IlGeneratorMethodDetails & details, TR_FrontEnd *fe)
   {
   // See if the method is already in the queue or under compilation
   //
   for (uint8_t i = 0; i < getNumUsableCompilationThreads(); i++)
      {
      TR::CompilationInfoPerThread *curCompThreadInfoPT = _arrayOfCompilationInfoPerThread[i];
      TR_ASSERT(curCompThreadInfoPT, "a thread's compinfo is missing\n");

      if (
          curCompThreadInfoPT->getMethodBeingCompiled() &&
          curCompThreadInfoPT->getMethodBeingCompiled()->getMethodDetails().sameAs(details, fe) &&
         !curCompThreadInfoPT->getMethodBeingCompiled()->_unloadedMethod) // Redefinition; see cmvc 192606 and RTC 36898
         return curCompThreadInfoPT->getMethodBeingCompiled();
      }

   for (TR_MethodToBeCompiled *cur = _methodQueue; cur; cur = cur->_next)
      if (cur->getMethodDetails().sameAs(details, fe))
         return cur;
   return NULL;
   }

TR_MethodToBeCompiled *TR::CompilationInfo::peekNextMethodToBeCompiled()
   {
   if (_methodQueue)
      return _methodQueue;
   else if (getLowPriorityCompQueue().hasLowPriorityRequest() && canProcessLowPriorityRequest())
      // These upgrade requests should not hinder the application too much.
      // If possible, we should decrease the priority of the compilation thread
      // Note that on LINUX priorities do not work
      // If we cannot lower the priority, we should wait for some idle time
      // or space compilations apart. if we return NULL here, the compilation
      // thread will go to sleep and we need a mechanism to wake it up
      //
      return getLowPriorityCompQueue().getFirstLPQRequest();
   else if (!getJProfilingCompQueue().isEmpty() && canProcessJProfilingRequest())
      return getJProfilingCompQueue().getFirstCompRequest();
   else
      return NULL;
   }


TR_MethodToBeCompiled *
TR::CompilationInfo::getNextMethodToBeCompiled(TR::CompilationInfoPerThread *compInfoPT,
                                              bool compThreadCameOutOfSleep,
                                              TR_CompThreadActions *compThreadAction)
   {
   TR_MethodToBeCompiled *m = NULL;
   *compThreadAction = PROCESS_ENTRY;
   if (_methodQueue)
      {
      // If the request is sync or AOT load or InstantReplay, take it now
      if (compInfoPT->isDiagnosticThread() || // InstantReplay compilations must be processed immediately
         _methodQueue->_priority >= CP_SYNC_MIN ||       // sync comp
         _methodQueue->_methodIsInSharedCache == TR_yes || // very cheap relocation
         getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER) // compile right away in server mode
         {
         m = _methodQueue;
         _methodQueue = _methodQueue->_next;
         }
      // Check if we need to throttle
      else if (exceedsCompCpuEntitlement() == TR_yes &&
              !compThreadCameOutOfSleep && // Don't throttle a comp thread that has just slept its share of time
              (TR::Options::_compThreadCPUEntitlement < 100 || getNumCompThreadsActive() * 100 > (TR::Options::_compThreadCPUEntitlement + 50)))
         {
         // If at all possible suspend the compilation thread
         // Otherwise, perform a timed wait
         if (getNumCompThreadsActive() > 1)
            *compThreadAction = SUSPEND_COMP_THREAD_EXCEED_CPU_ENTITLEMENT;
         else
            *compThreadAction = THROTTLE_COMP_THREAD_EXCEED_CPU_ENTITLEMENT;
         }
      // Avoid two concurrent hot compilations
      else if (getNumCompThreadsCompilingHotterMethods() <= 0 || // no hot compilation in progress
               _methodQueue->_weight < TR::Options::_expensiveCompWeight) // This is a cheaper comp
         {
         m = _methodQueue;
         _methodQueue = _methodQueue->_next;
         }
      else // scan for a cold/warm method
         {
         TR_MethodToBeCompiled *prev = _methodQueue;
         for (m = _methodQueue->_next; m; prev = m, m = m->_next)
            {
            if (m->_optimizationPlan->getOptLevel() <= warm || // cheaper comp
                m->_priority >= CP_SYNC_MIN ||       // sync comp
                m->_methodIsInSharedCache == TR_yes) // very cheap relocation
               {
               prev->_next = m->_next;
               break;
               }
            }
         if (!m)
            {
            *compThreadAction = GO_TO_SLEEP_CONCURRENT_EXPENSIVE_REQUESTS;

            // make sure there is at least one thread that is not jobless
            TR::CompilationInfoPerThread * const * arrayOfCompInfoPT = getArrayOfCompilationInfoPerThread();
            int32_t numActive = 0, numHot = 0, numLowPriority = 0;
            for (uint8_t i = 0; i < getNumUsableCompilationThreads(); i++)
               {
               TR::CompilationInfoPerThread *curCompThreadInfoPT = arrayOfCompInfoPT[i];
               TR_ASSERT(curCompThreadInfoPT, "a thread's compinfo is missing\n");

               CompilationThreadState currentThreadState = curCompThreadInfoPT->getCompilationThreadState();
               if (
                  currentThreadState == COMPTHREAD_ACTIVE
                  || currentThreadState == COMPTHREAD_SIGNAL_WAIT
                  || currentThreadState == COMPTHREAD_WAITING
                  || currentThreadState == COMPTHREAD_SIGNAL_SUSPEND
                  )
                  {
                  if (curCompThreadInfoPT->getMethodBeingCompiled() &&
                      curCompThreadInfoPT->getMethodBeingCompiled()->_priority < CP_ASYNC_NORMAL)
                      numLowPriority++;
                  if (curCompThreadInfoPT->compilationThreadIsActive())
                     numActive++;
                  if (curCompThreadInfoPT->getMethodBeingCompiled() &&
                      curCompThreadInfoPT->getMethodBeingCompiled()->_hasIncrementedNumCompThreadsCompilingHotterMethods)
                     numHot++;
                  }
               }

            // There is a method to be compiled, but this thread is not going to take it out of the queue
            // There are two cases when this might happen:
            // (1) Two hot requests going in parallel
            // (2) Two low priority requests going in parallel during startup phase
            TR_ASSERT(numHot >= 1 || numLowPriority >= 1, "We must have a comp thread working on a hot or low priority method %d %d", numHot, numLowPriority);

            if (numActive <= 1) // Current thread is by default active because it tried to dequeue a request
               {
               // There is no thread left to handle the existing request
               // See defect 182392 for why this could have happened and for the solution
               //fprintf(stderr, "JIT WARNING: no active thread left to handle queued request\n");
               }
            // sanity checks
            if (getNumCompThreadsActive() != numActive)
               {
               TR_ASSERT(false, "Inconsistency with active threads %d %d\n", getNumCompThreadsActive(), numActive);
               setNumCompThreadsActive(numActive); // apply correction
               }
            if (getNumCompThreadsCompilingHotterMethods() != numHot)
               {
               TR_ASSERT(false, "Inconsistency with hot threads %d %d\n", getNumCompThreadsCompilingHotterMethods(), numHot);
               setNumCompThreadsCompilingHotterMethods(numHot); // apply correction
               }
            }
         }
      if (m) // A request has been dequeued
         {
         updateCompQueueAccountingOnDequeue(m);
         }
      }
   // When no request is in the main queue we can look in the low priority queue
   else if (getLowPriorityCompQueue().hasLowPriorityRequest() &&
            canProcessLowPriorityRequest())
      {
      // Check if we need to throttle
      if (exceedsCompCpuEntitlement() == TR_yes &&
          !compThreadCameOutOfSleep && // Don't throttle a comp thread that has just slept its share of time
          (TR::Options::_compThreadCPUEntitlement < 100 || getNumCompThreadsActive() * 100 > (TR::Options::_compThreadCPUEntitlement + 50)))
         {
         // If at all possible suspend the compilation thread
         // Otherwise, perform a timed wait
         if (getNumCompThreadsActive() > 1)
            *compThreadAction = SUSPEND_COMP_THREAD_EXCEED_CPU_ENTITLEMENT;
         else
            *compThreadAction = THROTTLE_COMP_THREAD_EXCEED_CPU_ENTITLEMENT;
         }
      else
         {
         m = getLowPriorityCompQueue().extractFirstLPQRequest();
         }
      }
   // Now let's look in the JProfiling queue
   else if (!getJProfilingCompQueue().isEmpty() && canProcessJProfilingRequest())
      {
      // Check if we need to throttle
      if (exceedsCompCpuEntitlement() == TR_yes &&
         !compThreadCameOutOfSleep && // Don't throttle a comp thread that has just slept its share of time
         (TR::Options::_compThreadCPUEntitlement < 100 || getNumCompThreadsActive() * 100 >(TR::Options::_compThreadCPUEntitlement + 50)))
         {
         // If at all possible suspend the compilation thread
         // Otherwise, perform a timed wait
         if (getNumCompThreadsActive() > 1)
            *compThreadAction = SUSPEND_COMP_THREAD_EXCEED_CPU_ENTITLEMENT;
         else
            *compThreadAction = THROTTLE_COMP_THREAD_EXCEED_CPU_ENTITLEMENT;
         }
      else
         {
         m = getJProfilingCompQueue().extractFirstCompRequest();
         }
      }
   else
      {
      // Cannot take a request either from main queue or LPQ
      // If this is the last active compilation thread, then go to sleep,
      // otherwise suspend the compilation thread
      if (getNumCompThreadsActive() > 1)
         {
         *compThreadAction = SUSPEND_COMP_THREAD_EMPTY_QUEUE;
         }
      else
         {
         *compThreadAction = GO_TO_SLEEP_EMPTY_QUEUE;
         }
      }
   return m;
   }

//----------------------------- computeCompThreadSleepTime ----------------------
// Compute how much the compilation thread should sleep for throttling purposes
// Parameters: compilationTimeMs is the wall clock time spent by previous
// compilation
// The return value is in ms.
//-------------------------------------------------------------------------------
int32_t TR::CompilationInfo::computeCompThreadSleepTime(int32_t compilationTimeMs)
   {
   int32_t sleepTimeMs = 1;
   if (TR::Options::_compThreadCPUEntitlement > 0)
      {
      sleepTimeMs = compilationTimeMs * (100 / TR::Options::_compThreadCPUEntitlement - 1);
      }
   // some bounds for sleep time
   if (sleepTimeMs < TR::Options::_minSleepTimeMsForCompThrottling) // too small values are not useful because of high overhead of calling wait()
      sleepTimeMs = TR::Options::_minSleepTimeMsForCompThrottling;
   if (sleepTimeMs > TR::Options::_maxSleepTimeMsForCompThrottling) // TODO: need an options for this value
      sleepTimeMs = TR::Options::_maxSleepTimeMsForCompThrottling;
   return sleepTimeMs;
   }

// FIXME: this should be called only when running async - i have not yet figured
// out how to figure this info out for interpreted methods scheduled for their
// first compilation
bool TR::CompilationInfo::isQueuedForCompilation(J9Method * method, void *oldStartPC)
   {
   TR_ASSERT(oldStartPC, "isQueuedForCompilation should not be called for interpreted methods\n");

   TR_LinkageInfo *linkageInfo = TR_LinkageInfo::get(oldStartPC);
   return linkageInfo->isBeingCompiled();
   }

#if defined(TR_HOST_X86)
JIT_HELPER(initialInvokeExactThunkGlue);
#else
JIT_HELPER(_initialInvokeExactThunkGlue);
#endif

void *TR::CompilationInfo::startPCIfAlreadyCompiled(J9VMThread * vmThread, TR::IlGeneratorMethodDetails & details, void *oldStartPC)
   {
   if (details.isNewInstanceThunk())
      {
      return jitNewInstanceMethodStartAddress(vmThread, static_cast<J9::NewInstanceThunkDetails &>(details).classNeedingThunk());
      }
   else if (details.isMethodHandleThunk())
      {
      J9::MethodHandleThunkDetails &thunkDetails = static_cast<J9::MethodHandleThunkDetails &>(details);
      if (!thunkDetails.isShareable())
         return NULL; // Don't let the presence of a shareable thunk interfere with other requests

      J9JITConfig *jitConfig = vmThread->javaVM->jitConfig;
      if (!jitConfig)
         return NULL;

      void *thunkStartPC = NULL;
      TR_J9VMBase * fe = TR_J9VMBase::get(jitConfig, vmThread);

         {
         TR::VMAccessCriticalSection startPCIfAlreadyCompiled(fe);
         uintptrj_t methodHandle = *thunkDetails.getHandleRef();
         void *thunkAddress = fe->methodHandle_jitInvokeExactThunk(methodHandle);
         void *initialInvokeExactThunkGlueAddress;
#if defined(J9ZOS390)
         initialInvokeExactThunkGlueAddress = (void*)TOC_UNWRAP_ADDRESS(_initialInvokeExactThunkGlue);
#elif defined(TR_HOST_POWER) && (defined(TR_HOST_64BIT) || defined(AIXPPC)) && !defined(__LITTLE_ENDIAN__)
         initialInvokeExactThunkGlueAddress = (*(void **)_initialInvokeExactThunkGlue);
#elif defined(TR_HOST_X86)
         initialInvokeExactThunkGlueAddress = (void*)initialInvokeExactThunkGlue;
#else
         initialInvokeExactThunkGlueAddress = (void*)_initialInvokeExactThunkGlue;
#endif
         if (thunkAddress != initialInvokeExactThunkGlueAddress)
            {
            // thunkAddress is the jit entry point for a thunk.  We need to find
            // the corresponding startPC (which is the interpreter entry point).
            //
            // TODO:JSR292: This could also be a j2i thunk/helper address!  Should
            // return NULL in that case.  For now, set TR_DisableThunkTupleJ2I to
            // avoid this case.
            //
            J9JITHashTable *hashTable = (J9JITHashTable *) avl_search(jitConfig->translationArtifacts, (UDATA)thunkAddress);
            if (hashTable)
               {
               J9JITExceptionTable *metadata = hash_jit_artifact_search(hashTable, (UDATA)thunkAddress);
               if (metadata)
                  thunkStartPC = (void*)metadata->startPC;
               }
            bool verboseDetails = TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseMethodHandleDetails);
            if (verboseDetails)
               {
               if (thunkStartPC)
                  TR_VerboseLog::writeLineLocked(TR_Vlog_MHD, "%p   Metadata lookup: handle %p thunk body at %p has startPC %p", vmThread, (void*)methodHandle, thunkAddress, thunkStartPC);
               else
                  TR_VerboseLog::writeLineLocked(TR_Vlog_MHD, "%p   Metadata lookup FAILED for: handle %p thunk body at %p -- jit will probably create a redundant body", vmThread, (void*)methodHandle, thunkAddress);
               }
            }
         }

      return thunkStartPC;
      }

   void *startPC = 0;
   J9Method *method = details.getMethod();

   if (!oldStartPC)
      {
      // first compilation of the method: J9Method would be updated if the
      // compilation has already taken place
      //
      if (isCompiled(method))
         startPC = getJ9MethodStartPC(method);
      }
   else
      {
      TR_LinkageInfo *linkageInfo = TR_LinkageInfo::get(oldStartPC);
      if (linkageInfo->recompilationAttempted() && isCompiled(method))
         startPC = getJ9MethodStartPC(method);
      }
   return startPC;
   }

// Must have compilation monitor when calling this method
void TR::CompilationInfo::recycleCompilationEntry(TR_MethodToBeCompiled *entry)
   {
   TR_ASSERT_FATAL((entry->_freeTag & (ENTRY_INITIALIZED)) || (entry->_freeTag & (ENTRY_IN_POOL_NOT_FREE|ENTRY_IN_POOL_FREE|ENTRY_DEALLOCATED)),
                   "recycling an improper entry\n");
   entry->_freeTag |= ENTRY_IN_POOL_NOT_FREE;
   if (entry->_numThreadsWaiting == 0)
      entry->_freeTag |= ENTRY_IN_POOL_FREE;
   entry->cleanupJITaaS();

   entry->_next = _methodPool;
   _methodPool = entry;
   _methodPoolSize++;

   // If the pool grows too big, start deallocating entries
   if (_methodPoolSize >= METHOD_POOL_SIZE_THRESHOLD)
      {
      TR_MethodToBeCompiled *prev = _methodPool;
      TR_MethodToBeCompiled *crt = _methodPool->_next;
      while (crt && _methodPoolSize >=  METHOD_POOL_SIZE_THRESHOLD/2)
         {
         if (crt->_numThreadsWaiting == 0)
            {
            TR_ASSERT_FATAL(crt->_freeTag & ENTRY_IN_POOL_FREE, "Will deallocate an entry that is not free\n");

            prev->_next = crt->_next;
            _methodPoolSize--;
            crt->shutdown(); // free the monitor and string associated with it
            PORT_ACCESS_FROM_JITCONFIG(_jitConfig);
            j9mem_free_memory(crt);
            crt = prev->_next;
            }
         else
            {
            prev = crt;
            crt = crt->_next;
            }
         }
      }
   }


static void deleteMethodHandleRef(J9::MethodHandleThunkDetails & details, J9VMThread *vmThread, TR_FrontEnd *fe)
   {
   bool verboseDetails = TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseMethodHandleDetails);
   TR_J9VMBase *fej9 = (TR_J9VMBase *)fe;

   if (verboseDetails)
      {
      TR::VMAccessCriticalSection deleteMethodHandleRef(fej9);
      uintptrj_t methodHandle = * details.getHandleRef();
      TR_VerboseLog::writeLineLocked(TR_Vlog_MHD,"%p   Deleting MethodHandle %p global reference", vmThread, (j9object_t)methodHandle);
      }

   vmThread->javaVM->internalVMFunctions->j9jni_deleteGlobalRef((JNIEnv*)vmThread, (jobject)details.getHandleRef(), false);
   if (details.getArgRef())
      vmThread->javaVM->internalVMFunctions->j9jni_deleteGlobalRef((JNIEnv*)vmThread, (jobject)details.getArgRef(), false);
   }


void *TR::CompilationInfo::compileMethod(J9VMThread * vmThread, TR::IlGeneratorMethodDetails & details, void *oldStartPC,
                                        TR_YesNoMaybe requireAsyncCompile,
                                        TR_CompilationErrorCode *compErrCode,
                                        bool *queued, TR_OptimizationPlan * optimizationPlan)
   {
   TR_J9VMBase * fe = TR_J9VMBase::get(_jitConfig, vmThread);
   TR_ASSERT(!fe->isAOT_DEPRECATED_DO_NOT_USE(), "We need a non-AOT vm here.");

   J9Method *method = details.getMethod();

   J9::NewInstanceThunkDetails * newInstanceThunkDetails = NULL;
   if (details.isNewInstanceThunk())
      newInstanceThunkDetails = & static_cast<J9::NewInstanceThunkDetails &>(details);

   J9::MethodHandleThunkDetails *methodHandleThunkDetails = NULL;
   if (details.isMethodHandleThunk())
      methodHandleThunkDetails = & static_cast<J9::MethodHandleThunkDetails &>(details);

   J9Class *clazz = NULL;
   if (newInstanceThunkDetails)
      clazz = newInstanceThunkDetails->classNeedingThunk();
   else
      clazz = details.getClass();

   bool verbose = TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCompileRequest);
   if (verbose)
      {
      TR_VerboseLog::vlogAcquire();
      TR_VerboseLog::writeLine(TR_Vlog_CR,"%p   Compile request %s", vmThread, details.name());

      if (newInstanceThunkDetails)
         {
         if (clazz)
            {
            int32_t len;
            char * className = fe->getClassNameChars(fe->convertClassPtrToClassOffset(clazz), len);
            TR_VerboseLog::write(" j9class=%p %.*s", clazz, len, className);
            }
         }
      else if (methodHandleThunkDetails)
         {
         // No vm access needed here, because we're not inspecting the handle object -- only printing its address.
         TR_VerboseLog::write(" handle=%p", (void*)*(methodHandleThunkDetails->getHandleRef()));
         if (methodHandleThunkDetails->getArgRef())
            TR_VerboseLog::write(" arg=%p", (void*)*(methodHandleThunkDetails->getArgRef()));
         }

      char buf[500];
      fe->printTruncatedSignature(buf, sizeof(buf), (TR_OpaqueMethodBlock *)method);
      TR_VerboseLog::write(" j9method=%p %s optLevel=%d", method, buf, optimizationPlan->getOptLevel());
      if (clazz && J9_IS_CLASS_OBSOLETE(clazz))
         {
         TR_ASSERT(!fe->isAOT_DEPRECATED_DO_NOT_USE(), "Shouldn't get obsolete classes in AOT");
         TR_VerboseLog::write(" OBSOLETE class=%p -- request declined", clazz);
         }
      TR_VerboseLog::vlogRelease();
      }

   // Pin the class of the method in case GC wants to unload it
   bool classPushed = false;
   // Why wouldn't we do this for AOT??
   if (!fe->isAOT_DEPRECATED_DO_NOT_USE())
      {
      if (clazz && J9_IS_CLASS_OBSOLETE(clazz))
         return NULL;

      PUSH_OBJECT_IN_SPECIAL_FRAME(vmThread, J9VM_J9CLASS_TO_HEAPCLASS(clazz));
      classPushed = true;
      }

#ifdef J9VM_JIT_GC_ON_RESOLVE_SUPPORT
   // If gcOnResolve, scavenge now, unless the JIT is in test mode,
   // and not connected to a real fe.  Do this only when recompiling
   // or when compiling a newInstanceThunk (otherwise we're too slow)
   //
   if ( (_jitConfig->runtimeFlags & J9JIT_SCAVENGE_ON_RESOLVE) &&
       !(_jitConfig->runtimeFlags & J9JIT_TESTMODE) && !requireAsyncCompile &&
        (oldStartPC || newInstanceThunkDetails))
      jitCheckScavengeOnResolve(vmThread);
#endif

   // A request has been made to compile the method on the given application
   // thread. The compilation will actually occur either on this thread or a
   // separate compilation thread depending on startup options.

   // If compiling on this thread acquire the application thread monitor.
   // This is the monitor that prevents compilation on multiple application
   // threads at the same time. It is held for the duration of the compilation.
   //
   if (!useSeparateCompilationThread())
      {
      // If an async compile was required here, then bail out
      //
      if (requireAsyncCompile == TR_yes)
         {
         if (compErrCode)
            *compErrCode = compilationFailure;

         // again, why not for AOT?
         if (!fe->isAOT_DEPRECATED_DO_NOT_USE())
            if (classPushed)
               POP_OBJECT_IN_SPECIAL_FRAME(vmThread);

         if (methodHandleThunkDetails)
            deleteMethodHandleRef(*methodHandleThunkDetails, vmThread, fe);
         return NULL;
         }

      // Initialize the application thread monitor if not already done
      //
      if (!_applicationThreadMonitor)
         _applicationThreadMonitor = TR::Monitor::create("JIT-ApplicationThreadMonitor");

      if (!_applicationThreadMonitor)
         {
         if (compErrCode)
            *compErrCode = compilationFailure;

         // why not for AOT?
         if (!fe->isAOT_DEPRECATED_DO_NOT_USE())
            if (classPushed)
               POP_OBJECT_IN_SPECIAL_FRAME(vmThread);

         if (methodHandleThunkDetails)
            deleteMethodHandleRef(*methodHandleThunkDetails, vmThread, fe);

         return NULL;
         }

      // not for AOT....suspicious looking?
      if (!fe->isAOT_DEPRECATED_DO_NOT_USE())
         releaseVMAccess(vmThread);

      _applicationThreadMonitor->enter();
      // acquire fe access, but ignore any JAVA suspend request until we finish the
      // compilation and release the JIT-ApplicationThreadMonitor
      // NOTE: we must not release-acquire fe access during compilation in this
      // scenario (compileOnApplicationThread)

      // what is the AOT connection here?
      if (!fe->isAOT_DEPRECATED_DO_NOT_USE())
         acquireVMAccessNoSuspend(vmThread);
      }

   void *startPC = 0;
   bool needCompile = true;

   // always need to compile if it's a log compilation
   if (!(optimizationPlan->isLogCompilation()))
      {
      if (newInstanceThunkDetails)
         {
         // Ask the VM if this compilation is required
         //
         startPC =  jitNewInstanceMethodStartAddress(vmThread, clazz);
         if (startPC || fe->isAOT_DEPRECATED_DO_NOT_USE())
            needCompile = false;
         } // if
      else if (!oldStartPC && !details.isMethodInProgress())
         {
         if (isCompiled(method))
            {
            debugPrint("\tcompile request already done", method); //, vmThread, entry->getMethodDetails());
            startPC = getJ9MethodStartPC(method);
            needCompile = false;
            }
         } // if
      else if (oldStartPC) // recompilation case
         {
         if (!(useSeparateCompilationThread() && !fe->isAOT_DEPRECATED_DO_NOT_USE()))
            {
            TR_LinkageInfo *linkageInfo = TR_LinkageInfo::get(oldStartPC);
            if (linkageInfo->recompilationAttempted())
               {
               startPC = getJ9MethodStartPC(method); // provide the new startPC
               // must look like a compiled method
               if (isCompiled(method))
                  {
                  debugPrint("\tcompile request already done", method); //, vmThread, entry->getMethodDetails());
                  needCompile = false;
                  }
               } // if
            } // if
         } // if
      } // if !isLogCompilation()

   if (optimizationPlan->isGPUCompilation())
       needCompile=1;

   if (needCompile)
      {
      // AOT goes to application thread?
      if (useSeparateCompilationThread() && !fe->isAOT_DEPRECATED_DO_NOT_USE())
         startPC = compileOnSeparateThread(vmThread, details, oldStartPC, requireAsyncCompile, compErrCode, queued, optimizationPlan);
      else
         startPC = compileOnApplicationThread(vmThread, details, oldStartPC, compErrCode, optimizationPlan);
      }
   else
      {
      if (compErrCode)
         *compErrCode = compilationNotNeeded;
      }

   if (!useSeparateCompilationThread())
      {
      _applicationThreadMonitor->exit();

#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
      // After releasing the applicationThreadMonitor, let's see if we hold
      // any other monitors by mistake
      TR::Monitor * heldMonitor = TR::MonitorTable::get()->monitorHeldByCurrentThread();
      TR_ASSERT(!heldMonitor, "After taking a longjmp comp thread is holding TR::Monitor %p called %s\n",
              heldMonitor, ((TR_J9VMBase*)fe)->getJ9MonitorName((J9ThreadMonitor*)heldMonitor->getVMMonitor()));
#endif // #if defined(DEBUG) || defined(PROD_WITH_ASSUMES)

      if (!fe->isAOT_DEPRECATED_DO_NOT_USE())
         {
         // Now, release VM access and re-acquire it looking at the suspend condition
         releaseVMAccess(vmThread);
         acquireVMAccess(vmThread);
         }
      }

   if (!fe->isAOT_DEPRECATED_DO_NOT_USE())
      if (classPushed)
         POP_OBJECT_IN_SPECIAL_FRAME(vmThread);
   return startPC;
   }

#if defined(J9VM_OPT_SHARED_CLASSES)
extern bool
validateSharedClassAOTHeader(J9JavaVM *javaVM, J9VMThread *curThread, TR::CompilationInfo *compInfo, TR_FrontEnd *fe);
#endif

void *TR::CompilationInfo::compileOnSeparateThread(J9VMThread * vmThread, TR::IlGeneratorMethodDetails & details,
                                                  void *oldStartPC, TR_YesNoMaybe requireAsyncCompile,
                                                  TR_CompilationErrorCode *compErrCode,
                                                  bool *queued,
                                                  TR_OptimizationPlan * optimizationPlan)
   {
   void *startPC = NULL;

   J9Method *method = details.getMethod();
   TR_J9VMBase *fe = TR_J9VMBase::get(_jitConfig, vmThread);

   // Grab the compilation monitor
   //
   debugPrint(vmThread, "\tapplication thread acquiring compilation monitor\n");
   acquireCompMonitor(vmThread);
   debugPrint(vmThread, "+CM\n");
   addCompilationTraceEntry(vmThread, OP_CompileOnSeparateThreadEnter);
   // Even before checking whether compilation threads are active, we need
   // to check if a compilation for this body has already been performed

   // Now that we have the lock
   // Check if the compilation is already done by looking at the J9Method/LinkageInfo.
   //
   // If it is not already done, then either
   // 1 - we are the first thread to get here, we will create a brand new entry in the queue.
   // 2 - an entry in the queue already exists - a compilation is in progress or in queue
   //     that hasn't finished yet.  We will definitely find the old entry when we try to add
   //     to the queue.
   //
   if (!details.isMethodInProgress())
      startPC = startPCIfAlreadyCompiled(vmThread, details, oldStartPC);
   if (startPC && !details.isDumpMethod() &&
      !optimizationPlan->isGPUCompilation())
      {
      // Release the compilation lock and return
      debugPrint(vmThread, "\tapplication thread releasing compilation monitor - compile already done\n");
      debugPrint(vmThread, "-CM\n");
      releaseCompMonitor(vmThread);
      if (compErrCode)
         *compErrCode = compilationNotNeeded;
      if (TR::Options::getJITCmdLineOptions()->getVerboseOption(TR_VerboseCompileRequest))
         TR_VerboseLog::writeLineLocked(TR_Vlog_CR,"%p     Already compiled at %p", startPC);
      return startPC;
      }

   // Check to see if compilation threads are available to perform the compilation
   if (
         (getNumCompThreadsActive() == 0) ||
         (
            getPersistentInfo()->getDisableFurtherCompilation() &&
            oldStartPC == 0 &&
            !details.isDumpMethod()
         )
      )
      {
      bool shouldReturn = true;

      // Fail the compilation, unless this is on the queue (maybe even in progress)
      if (!requestExistsInCompilationQueue(details, fe))
         {
         startPC = compilationEnd(vmThread, details, _jitConfig, 0, oldStartPC);
         }
      else // Similar request already exists in the queue. We cannot fail this compilation
         {
         // If this is a sync compilation we must wait for the queued compilation to finish
         // Otherwise, code patching could put as in an infinite loop (see 185740)
         // It's also safe to continue for async compilations: if a similar request
         // already exists we will bail out and let the Java thread go.
         shouldReturn = false;
         if (TR::Options::isAnyVerboseOptionSet(TR_VerboseCompFailure, TR_VerboseCompileRequest))
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_FAILURE,"t=%u <WARNING: JIT Compilations are suspended. Current request will be coalesced with existing one in the queue>", (uint32_t)getPersistentInfo()->getElapsedTime());
            }
         }

      if (shouldReturn)
         {
         debugPrint(vmThread, "\tapplication thread release compilation monitor - comp thread AWOL\n");
         debugPrint(vmThread, "-CM\n");
         releaseCompMonitor(vmThread);
         if (compErrCode)
            *compErrCode = compilationSuspended;

         // Update statistics regarding the reason for compilation failure
         updateCompilationErrorStats(compilationSuspended);

         if (TR::Options::isAnyVerboseOptionSet(TR_VerboseCompFailure, TR_VerboseCompileRequest))
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_FAILURE,"t=%u <WARNING: JIT Compilations are suspended>", (uint32_t)getPersistentInfo()->getElapsedTime());
            }

         return startPC;
         }
      }


   // Determine if the compilation is going to be synchronous or asynchronous.
   // newInstance Thunks, and invalidated methods need be compiled synchronously
   //
   debugPrint(vmThread, "\tQueuing ");

   TR_ASSERT(asynchronousCompilation() || requireAsyncCompile != TR_yes, "We cannot require async compilation when it is turned off");

   bool forcedSync = false;
   bool async = asynchronousCompilation() && requireAsyncCompile != TR_no;
   if (async)
      {
      TR_LinkageInfo *linkageInfo = oldStartPC ? TR_LinkageInfo::get(oldStartPC) : 0;
      if (requireAsyncCompile != TR_yes && !oldStartPC && !details.isMethodInProgress())
         {
         J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
         // if the method is specified to be compiled at count=0, -Xjit:{*method*}(count=0), then do it synchronously
         //
         if (TR::Options::getJITCmdLineOptions()->anOptionSetContainsACountValue() ||
             TR::Options::getAOTCmdLineOptions()->anOptionSetContainsACountValue())
            {
            bool hasBackwardBranches = (J9ROMMETHOD_HAS_BACKWARDS_BRANCHES(romMethod)) ? true : false;
            bool isAOT = !TR::Options::getJITCmdLineOptions()->anOptionSetContainsACountValue();
            TR::OptionSet * optionSet = findOptionSet(method, isAOT);

            if (optionSet &&
                (optionSet->getOptions()->getInitialCount() == 0 ||
                (optionSet->getOptions()->getInitialBCount() == 0 && hasBackwardBranches)))
               async = false;
            }

         if (async)
            {
            bool isORB = false;
            J9ROMClass *declaringClazz = J9_CLASS_FROM_METHOD(method)->romClass;
            J9UTF8 * className = J9ROMCLASS_CLASSNAME(declaringClazz);
            if (J9UTF8_LENGTH(className) == 36 &&
                0==memcmp(utf8Data(className), "com/ibm/rmi/io/FastPathForCollocated", 36))
               {
               J9UTF8 *utf8 = J9ROMMETHOD_NAME(J9_ROM_METHOD_FROM_RAM_METHOD(method));
               if (J9UTF8_LENGTH(utf8)==21 &&
                   0==memcmp(J9UTF8_DATA(utf8), "isVMDeepCopySupported", 21))
                  isORB = true;
               }

            // compile BigDecimal methods containing DFP stubs synchronously on first compile
            // requireAsyncCompile will be false on first compile

            if (fe && requireAsyncCompile != TR_yes &&
                (isORB ||
                 ((!TR::Options::getCmdLineOptions()->getOption(TR_DisableDFP) || !TR::Options::getAOTCmdLineOptions()->getOption(TR_DisableDFP)) &&
                  (TR::Compiler->target.cpu.supportsDecimalFloatingPoint()
#ifdef TR_TARGET_S390
                  || TR::Compiler->target.cpu.getSupportsDecimalFloatingPointFacility()
#endif
                  ) && TR_J9MethodBase::isBigDecimalMethod((J9Method *)method))))
                async = false;
            }
         }

      if (!async)
         {
         debugPrint("forced synchronous");
         forcedSync = true;
         }
      else
         {
         debugPrint("asynchronous");
         // for async requests we can avoid checking the queue
         if ((linkageInfo && linkageInfo->isBeingCompiled()) || // for jitted methods
            (oldStartPC == 0 &&         // for interpreted methods
             details.isOrdinaryMethod() &&
             getJ9MethodVMExtra(details.getMethod()) == J9_JIT_QUEUED_FOR_COMPILATION)) // this will also filter out JNI natives
            {
            // Release the compilation lock and return
            debugPrint(vmThread, "\tapplication thread releasing compilation monitor - comp.req. in progress\n");
            debugPrint(vmThread, "-CM\n");
            releaseCompMonitor(vmThread);
            if (compErrCode)
               *compErrCode = compilationInProgress;
            return 0; // mark that compilation is not yet done
            }
         }
      }
   else
      {
      debugPrint("synchronous");
      }

   // Check to make sure we never queue a thunk archetype
   // See JTC-JAT 56314
   if (details.isOrdinaryMethod() || details.isMethodInProgress())
      {
      const J9ROMMethod *romMethod = details.getRomMethod();

      if (_J9ROMMETHOD_J9MODIFIER_IS_SET(romMethod, J9AccMethodFrameIteratorSkip))
         {
         if (details.isMethodHandleThunk())
            {
            if (compErrCode)
               *compErrCode = compilationFailure;

            debugPrint(vmThread, "\tapplication thread releasing compilation monitor - trying to compile thunk archetype\n");
            debugPrint(vmThread, "-CM\n");
            releaseCompMonitor(vmThread);
            return 0; // In prod, it's safe just to fail to queue the method
            }
         else
            {
            TR_ASSERT(0, "Should never attempt a JavaOrdinaryMethod compilation on a method with the J9AccMethodFrameIteratorSkip flag");
            Assert_JIT_unreachable();
            }
         }
      }

   TR_YesNoMaybe methodIsInSharedCache = TR_no;
   bool useCodeFromSharedCache = false;
#if defined(J9VM_INTERP_AOT_RUNTIME_SUPPORT) && defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM))
   // Check to see if we find the method in the shared cache
   // If yes, raise the priority to be processed ahead of other methods
   //
   J9JavaVM *javaVM = vmThread->javaVM;

   if (TR::Options::sharedClassCache() && !TR::Options::getAOTCmdLineOptions()->getOption(TR_NoLoadAOT) && details.isOrdinaryMethod())
      {
      if (!isJNINative(method) && !oldStartPC)
         {
         // If the method is in shared cache but we decide not to take it from there
         // we must bump the count, because this is a method whose count was decreased to scount
         // We can get the answer wrong if the method was not in the cache to start with, but other
         // other JVM added the method to the cache meanwhile. The net effect is that the said
         // method may have its count bumped up and compiled later
         //
         if (javaVM->sharedClassConfig->existsCachedCodeForROMMethod(vmThread, details.getRomMethod()))
            {
            TR_J9SharedCacheVM *fe = (TR_J9SharedCacheVM *) TR_J9VMBase::get(jitConfig, vmThread, TR_J9VMBase::AOT_VM);
            if (
               static_cast<TR_JitPrivateConfig *>(jitConfig->privateConfig)->aotValidHeader == TR_yes
               || (
                  static_cast<TR_JitPrivateConfig *>(jitConfig->privateConfig)->aotValidHeader == TR_maybe
                  && _sharedCacheReloRuntime.validateAOTHeader(javaVM, fe, vmThread)
                  )
               )
               {
               methodIsInSharedCache = TR_yes;
//             if (getPersistentInfo()->isClassLoadingPhase() || TR::Options::getCmdLineOptions()->getOption(TR_ForceLoadAOT))
                  useCodeFromSharedCache = true;
               }
            }
         }
      }
#endif // defined(J9VM_INTERP_AOT_RUNTIME_SUPPORT) && defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM))

   // determine priority
   CompilationPriority compPriority = CP_SYNC_NORMAL;
   if (async)
      {
      if (details.isMethodInProgress())
         compPriority = CP_ASYNC_BELOW_MAX;
      else if (details.isMethodHandleThunk())
         {
         if (static_cast<J9::MethodHandleThunkDetails &>(details).isShareable())
            {
            // Shareable thunks are quick to compile and provide much benefit over interpreter
            //
            compPriority = CP_ASYNC_ABOVE_NORMAL;
            }
         else if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableReducedPriorityForCustomMethodHandleThunks))
            {
            // Custom thunks are expensive to compile and likely provide less benefit
            // than compiling the java code that calls (and hopefully inlines) them.
            //
            compPriority = CP_ASYNC_BELOW_NORMAL;
            }
         }
      else
         {
         compPriority = CP_ASYNC_NORMAL;
         if (oldStartPC) // compiled method
            {
            TR_PersistentJittedBodyInfo *bodyInfo = TR::Recompilation::getJittedBodyInfoFromPC(oldStartPC);
            TR_ASSERT(bodyInfo, "assertion failure: bodyInfo is NULL");

            TR_PersistentMethodInfo *methodInfo = bodyInfo->getMethodInfo();
            TR_ASSERT(methodInfo, "assertion failure: methodInfo is NULL");

            TR_Hotness hotness = bodyInfo->getHotness();

            TR_Hotness nextHotness = optimizationPlan->getOptLevel();


            // Recompilations of profiling bodies are problematic when a large number of
            // threads is used and the compilation thread is starved; once we decided we
            // are done with the profiling step we must compile the next body sooner rather
            // than later
            if (bodyInfo->getIsProfilingBody() && !TR::Options::getCmdLineOptions()->getOption(TR_DisablePostProfileCompPriorityBoost))
               {
               if (getMethodQueueSize() > TR::Options::_compPriorityQSZThreshold)
                  compPriority = CP_ASYNC_ABOVE_NORMAL;
               }
            // decrease priority of upgrades
            // Also decrease priority of any recompilations during classLoadPhase
            // Also decrease priority of GCR recompilations
            // Also decrease priority of RI recompilations
            else if (optimizationPlan->isUpgradeRecompilation() ||
                getPersistentInfo()->isClassLoadingPhase() ||
                (bodyInfo->getUsesGCR() && nextHotness < hot) ||
                (methodInfo->getReasonForRecompilation() == TR_PersistentMethodInfo::RecompDueToRI))
               compPriority = CP_ASYNC_BELOW_NORMAL;
            // give priority to very-hot, scorching requests
            else if (nextHotness > hot && (dynamicThreadPriority() || compBudgetSupport()))
               compPriority = CP_ASYNC_ABOVE_NORMAL;
            // hot methods below 6% will be given lower priority
            else if (nextHotness == hot &&
                     optimizationPlan->getPerceivedCPUUtil() <= (TR::Options::_sampleInterval * 1000 / TR::Options::_veryHotSampleThreshold) &&
                     bodyInfo->getSamplingRecomp()) // recompilation triggered through sampling (as opposed to EDO)
               {
               compPriority = CP_ASYNC_BELOW_NORMAL;
               //fprintf(stderr, "CP_ASYNC_BELOW_NORMAL\n");
               }
            }
         else // interpreted method
            {
            // increase priority of JNI thunks
            if (method && isJNINative(method))
               compPriority = CP_ASYNC_ABOVE_NORMAL;
#if defined(J9VM_INTERP_AOT_RUNTIME_SUPPORT) && defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM))
            else if (useCodeFromSharedCache) // Use higher priority for methods from shared cache
               compPriority = CP_ASYNC_BELOW_MAX;
#endif
            }
         }
      if (TR::Options::getCmdLineOptions()->getOption(TR_EnableEarlyCompilationDuringIdleCpu) &&
         oldStartPC == 0 && // interpreted method
         compPriority <= CP_ASYNC_NORMAL &&
         details.isOrdinaryMethod() && // exclude DLT and thunks
         !useCodeFromSharedCache && // exclude AOT loads
         !isJNINative(method) // exclude JNI
         )
         {
         // Search the LPQ and if not found, add method to LPQ and bump invocation count
         // If method found, it means counter expired a second time, so move request to main queue
         TR_CompilationErrorCode err = scheduleLPQAndBumpCount(details, fe);
         if (err != compilationOK)
            {
            // Release the compilation lock before returning
            debugPrint(vmThread, "\tapplication thread releasing compilation monitor - comp.req. in progress\n");
            debugPrint(vmThread, "-CM\n");
            releaseCompMonitor(vmThread);
            if (compErrCode)
               *compErrCode = err;
            return 0; // mark that compilation is not yet done
            }
         }
      }// if (async)


   // Set up the compilation request
   //
   debugPrint(" compile request", method);


   TR_MethodToBeCompiled *entry =
      addMethodToBeCompiled(details, oldStartPC, compPriority, async,
                            optimizationPlan, queued, methodIsInSharedCache);

   if (entry == NULL)
      {
      releaseCompMonitor(vmThread);
      if (compErrCode)
         *compErrCode = compilationFailure;
      return 0;  // We couldn't add a method entry to be compiled.
      }

#if defined(J9VM_JIT_DYNAMIC_LOOP_TRANSFER)
   if (details.isMethodInProgress() &&
       !isCompiled(method) &&
       *queued &&
       !TR::Options::getCmdLineOptions()->getOption(TR_DisableDLTrecompilationPrevention))
      {
      // Scan the queue for a J9 request of the same j9method and make this request synchronous
      // The DLT flag means that we cannot have JNI or newInstance
      if (!async)
         changeCompReqFromAsyncToSync(method);
      else
         {
         TR_MethodToBeCompiled *reqMe, *prevReq;
         for (prevReq=NULL, reqMe=_methodQueue; reqMe; prevReq = reqMe, reqMe = reqMe->_next)
            {
            if (!reqMe->isDLTCompile() && reqMe->getMethodDetails().getMethod() == method)
               break;
            }
         if (reqMe && reqMe->_priority<CP_ASYNC_ABOVE_NORMAL)
            {
            reqMe->_priority = CP_ASYNC_ABOVE_NORMAL;
            if (prevReq && prevReq->_priority<CP_ASYNC_ABOVE_NORMAL)
               {
               prevReq->_next = reqMe->_next;
               queueEntry(reqMe);
               }
            }
         }
      }
#endif // J9VM_JIT_DYNAMIC_LOOP_TRANSFER

   // If a request is already queued for this method, see if we need to block
   // waiting for the compilation result. Do not wait if the requester specifically asks
   // for an asynchronous compilation (very rare event possible due to some races)
   //
   if (!(*queued) && entry->_changedFromAsyncToSync && requireAsyncCompile != TR_yes)
      {
      async = false;
      forcedSync = true;
      }
   entry->_async = async;

   // no need to grab slot monitor or releasing VM access if we are doing async
   if (async)
      {
      printQueue();

      // The compilation thread may be running at very low priority
      // Speed it up
      /*
      if (dynamicThreadPriority())
         {
         if (compPriority >= CP_ASYNC_ABOVE_NORMAL)
            {
            changeCompThreadPriority(J9THREAD_PRIORITY_MAX, 10);
            }
         else if (getMethodQueueSize() >= LARGE_QUEUE)
            {
            changeCompThreadPriority(J9THREAD_PRIORITY_MAX, 13);
            }
         else if (getCompThreadPriority() < J9THREAD_PRIORITY_NORMAL)
            {
            changeCompThreadPriority(J9THREAD_PRIORITY_MAX, 14);
            }
         }
         */
#ifdef STATS
      //if (compPriority >= CP_ASYNC_ABOVE_NORMAL)
      //   fprintf(stderr, "Adding request at priority CP_ASYNC_ABOVE_NORMAL\n");
#endif

      // We can avoid notifying the compilation thread if there were
      // other requests in the compilation queue
      // However we have to consider the case when one of the comp threads (though active)
      // is waiting on comp monitor because it wants the avoid two hot concurrent compilations
      // (if the really active comp thread gets suspended we remain with just one comp thread
      // sleeping and no notification will be sent)
      if (getMethodQueueSize() <= 1 ||
         getNumCompThreadsJobless() > 0) // send notification if any thread is sleeping on comp monitor waiting for suitable work
         {
         debugPrint(vmThread, "\tnotifying the compilation thread of the compile request\n");
         getCompilationMonitor()->notifyAll();
         debugPrint(vmThread, "ntfy-CM\n");
         }
      // TODO find a way to wake up only one compilation thread

      // Release the compilation monitor
      //
      debugPrint(vmThread, "\tapplication thread releasing compilation monitor\n");
      debugPrint(vmThread, "-CM\n");
      releaseCompMonitor(vmThread);

      if (compErrCode)
         *compErrCode = compilationInProgress;

      startPC = 0; // return 0 to show the compilation hasn't already been done
      }
   else // synchronous request
      {
      TR_ASSERT(requireAsyncCompile != TR_yes, "An async compile request is being forced to go in sync");

      // Grab the application thread monitor
      //
      debugPrint(vmThread, "\tapplication thread acquiring monitor for queue slot\n");
      entry->acquireSlotMonitor(vmThread);
      debugPrint(vmThread, "+AM-", entry);

      printQueue();
      // synchronous requests must receive immediate attention
      if (dynamicThreadPriority())
         changeCompThreadPriority(J9THREAD_PRIORITY_MAX, 11);
      debugPrint(vmThread, "\tnotifying the compilation thread of the compile request\n");
      getCompilationMonitor()->notifyAll();
      debugPrint(vmThread, "ntfy-CM\n");

      // Temporary sanity check
      TR_ASSERT_FATAL(entry->_numThreadsWaiting != 0x7fff, "ERROR _numThreadsWaiting is about to overflow\n");

      // Before releasing the compilation monitor, mark that we have one more thead waiting for this entry
      //
      entry->_numThreadsWaiting++;
      addCompilationTraceEntry(vmThread, OP_WillWaitOnSlotMonitorAfterCompMonRelease);
      // Release the compilation monitor
      //
      debugPrint(vmThread, "\tapplication thread releasing compilation monitor\n");
      debugPrint(vmThread, "-CM\n");
      releaseCompMonitor(vmThread);

      // Release VM Access
      //
      debugPrint(vmThread, "\tapplication thread releasing VM access\n");
      releaseVMAccess(vmThread);
      debugPrint(vmThread, "-VMacc\n");

      // wait for compilation to finish.
      //
      debugPrint(vmThread, "\tapplication thread waiting for compile to finish\n");
      debugPrint(vmThread, "wait-", entry);
      entry->getMonitor()->wait();

      debugPrint(vmThread, "\tapplication thread re-acquired queue slot monitor (got notification of compilation)\n");
      debugPrint(vmThread, "+AM-", entry);

      // Now the compilation should have been done (or the compilation
      // thread has been stopped or suspended).
      //
      startPC = entry->_newStartPC;
      // For HCR should we return 0 or oldStartPC?
      if (startPC && startPC != oldStartPC)
         debugPrint("\tcompile request succeeded", entry->getMethodDetails(), vmThread);
      else
         debugPrint("\tcompile request failed", entry->getMethodDetails(), vmThread);
      if (compErrCode)
         *compErrCode = (TR_CompilationErrorCode)entry->_compErrCode;

      //freeMethodToBeCompiled(entry);

      // Release the monitor now
      //
      debugPrint(vmThread, "\tapplication thread releasing AppMonitor\n");
      debugPrint(vmThread, "-AM-", entry);

      entry->releaseSlotMonitor(vmThread);

      // Grab the compilation mutex to be able to decrement numThreadsWaiting
      // and access entryShouldBeDeallocated
      // When numThreadsWaiting reaches 0, the entry can be reused or deallocated
      //
      acquireCompMonitor(vmThread);

      // If a remote sync compilation has been changed to a local sync compilation,
      // queue a new remote async compilation entry for this method.
      // - The second compilation is queued only when this is the last thread waiting
      //   for the compilation of the method.
      // - If for any reason, the current compilation request is changed to a sync
      //   compilation from an async compilation, it probably means we can’t do an
      //   async compilation. Don't queue the second async remote compilation.
      if (entry->hasChangedToLocalSyncComp() &&
          (entry->_numThreadsWaiting <= 1) &&
          !forcedSync &&
          (startPC && startPC != oldStartPC))
         {
         void *currOldStartPC = startPCIfAlreadyCompiled(vmThread, details, startPC);
         // If currOldStartPC is NULL, the method body defined by startPC has not been
         // recompiled. We will queue the second remote aync compilation here. Otherwise,
         // another thread is doing the recompilation and will take care of queueing
         // the second compilation.
         if (!currOldStartPC)
            {
            CompilationPriority compPriority = CP_ASYNC_NORMAL;
            bool queuedForRemote = false;
            bool isAsync = true;
            TR_OptimizationPlan *plan = TR_OptimizationPlan::alloc(entry->_origOptLevel);
            TR_YesNoMaybe mthInSharedCache = TR_no;

            if (plan)
               {
               TR_MethodToBeCompiled *entryRemoteCompReq = addMethodToBeCompiled(details, currOldStartPC, compPriority, isAsync,
                                                                                 plan, &queuedForRemote, mthInSharedCache);

               if (entryRemoteCompReq)
                  {
                  entryRemoteCompReq->_async = isAsync;

                  if (getMethodQueueSize() <= 1 ||
                     getNumCompThreadsJobless() > 0) // send notification if any thread is sleeping on comp monitor waiting for suitable work
                     {
                     getCompilationMonitor()->notifyAll();
                     }
                  if (TR::Options::getJITCmdLineOptions()->getVerboseOption(TR_VerboseCompileRequest))
                      TR_VerboseLog::writeLineLocked(TR_Vlog_CR,"%p   Queued a remote async compilation: entry=%p, j9method=%p",
                        vmThread, entryRemoteCompReq, entryRemoteCompReq->getMethodDetails().getMethod());
                  }
               if (!queuedForRemote)
                   TR_OptimizationPlan::freeOptimizationPlan(plan);
               }
            }
         }

      entry->_numThreadsWaiting--;

      TR_ASSERT_FATAL(!(entry->_freeTag & (ENTRY_DEALLOCATED|ENTRY_IN_POOL_FREE)), "Java thread waking up with a freed entry");

      if (entry->_numThreadsWaiting == 0)  // The tag should be ENTRY_IN_POOL_NOT_FREE
         if (entry->_freeTag & ENTRY_IN_POOL_NOT_FREE) // comp thread has recycled this entry
            entry->_freeTag |= ENTRY_IN_POOL_FREE;
         //else Do nothing cause we'll wait for the comp thread to completely recycle the entry

      // Temporary sanity check because PPC does not use prod with assumes.
      TR_ASSERT_FATAL(entry->_numThreadsWaiting >= 0, "ERROR _numThreadsWaiting has become negative");

      if (entry->_entryShouldBeDeallocated && entry->_numThreadsWaiting == 0)
         {
         entry->shutdown();
         PORT_ACCESS_FROM_JITCONFIG(_jitConfig);
         j9mem_free_memory(entry);
         }
      releaseCompMonitor(vmThread);

      // Reacquire VM access
      //
      acquireVMAccess(vmThread);
      debugPrint(vmThread, "+VMacc\n");
      } // synchronous requests

   return startPC;
   }

void *TR::CompilationInfo::compileOnApplicationThread(J9VMThread * vmThread, TR::IlGeneratorMethodDetails & details,
                                                     void *oldStartPC, TR_CompilationErrorCode *compErrCode,
                                                     TR_OptimizationPlan * optimizationPlan)
   {
   // Assert that compilation is not recursive (for single compilation thread)
   //
   TR_ASSERT(!_compInfoForCompOnAppThread->_methodBeingCompiled, "assertion failure");

   void *startPC = NULL;

   if (_compInfoForCompOnAppThread->getCompilationThreadState() == COMPTHREAD_ACTIVE)
      {
      TR_MethodToBeCompiled methodEntry;
      methodEntry._freeTag = ENTRY_IN_POOL_FREE;
      methodEntry._monitor = NULL;
      methodEntry.initialize(details, oldStartPC, CP_SYNC_NORMAL, optimizationPlan);
      methodEntry._numThreadsWaiting = 1;
      methodEntry._optimizationPlan = optimizationPlan;
      methodEntry._jitStateWhenQueued = getPersistentInfo()->getJitState();
      _compInfoForCompOnAppThread->_methodBeingCompiled = &methodEntry;

      J9Method *method = details.getMethod();

      // For newInstance thunks, we to stuff the class pointer
      // into the "extra" field of the RAMMethod before calling the JIT. This is
      // a convention that allows the same RAMMethod to be used to compile
      // different newInstance thunks.
      //
      if (details.isNewInstanceThunk())
         setJ9MethodVMExtra(method,
                            reinterpret_cast<uintptr_t>(
                               static_cast<J9::NewInstanceThunkDetails &>(details).classNeedingThunk()
                               ) | J9_STARTPC_NOT_TRANSLATED);

      if (getPersistentInfo()->isClassLoadingPhase() &&
          !TR::Options::getCmdLineOptions()->getOption(TR_DontDowngradeToCold) &&
          !isCompiled(method))
         TR::CompilationController::getCompilationStrategy()->adjustOptimizationPlan(&methodEntry, -1);// decrease opt level

#if defined(J9VM_INTERP_AOT_RUNTIME_SUPPORT) && defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM))
      // Check to see if we find the method in the shared cache
      //
      methodEntry._methodIsInSharedCache = TR_no;

      if (vmThread && TR::Options::sharedClassCache() && !TR::Options::getAOTCmdLineOptions()->getOption(TR_NoLoadAOT) &&
          details.isOrdinaryMethod() && !isJNINative(method) && !isCompiled(method) &&
          vmThread->javaVM->sharedClassConfig->existsCachedCodeForROMMethod(vmThread, details.getRomMethod()))
         {
         TR_J9SharedCacheVM *fe = (TR_J9SharedCacheVM *) TR_J9VMBase::get(jitConfig, vmThread, TR_J9VMBase::AOT_VM);
         if (
            static_cast<TR_JitPrivateConfig *>(jitConfig->privateConfig)->aotValidHeader == TR_yes
            || (
               static_cast<TR_JitPrivateConfig *>(jitConfig->privateConfig)->aotValidHeader == TR_maybe
               && _sharedCacheReloRuntime.validateAOTHeader(jitConfig->javaVM, fe, vmThread)
               )
            )
            {
            methodEntry._methodIsInSharedCache = TR_yes;
            }
         }
#endif // defined(J9VM_INTERP_AOT_RUNTIME_SUPPORT) && defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM))

      if (oldStartPC)
         {
         TR_PersistentJittedBodyInfo *bodyInfo = TR::Recompilation::getJittedBodyInfoFromPC(oldStartPC);
         if (bodyInfo)
            bodyInfo->getMethodInfo()->setNextCompileLevel(optimizationPlan->getOptLevel(), optimizationPlan->insertInstrumentation());
         }

      J9::SegmentAllocator scratchSegmentProvider(MEMORY_TYPE_JIT_SCRATCH_SPACE | MEMORY_TYPE_VIRTUAL, *_jitConfig->javaVM);
      startPC = _compInfoForCompOnAppThread->compile(vmThread, &methodEntry, scratchSegmentProvider);

      if (compErrCode)
         *compErrCode = (TR_CompilationErrorCode)methodEntry._compErrCode; // communicate the error code to the caller

      // the above call returns with the compilation monitor in hand

      _compInfoForCompOnAppThread->_methodBeingCompiled = NULL;
      }
   else // Compilation is not active
      {
      acquireCompMonitor(vmThread);
      startPC = compilationEnd(vmThread, details, _jitConfig, 0, oldStartPC);
      if (compErrCode)
         *compErrCode = compilationSuspended;
      }

   // Release the monitor
   //
   releaseCompMonitor(vmThread);

   return startPC;
   }

#ifdef TR_HOST_S390
void
TR::CompilationInfoPerThreadBase::outputVerboseMMapEntry(
   TR_ResolvedMethod *compilee,
   const struct tm &date,
   const struct timeval &time,
   void *startPC,
   void *endPC,
   const char *fmt,
   const char *profiledString,
   const char *compileString
   )
   {
   TR_VerboseLog::writeLine(
      TR_Vlog_MMAP,
      fmt,
      date.tm_hour,
      date.tm_min,
      date.tm_sec,
      time.tv_usec,
      compilee->classNameLength(), compilee->classNameChars(),
      compilee->nameLength(), compilee->nameChars(),
      compilee->signatureLength(), compilee->signatureChars(),
      "[",
      profiledString,
      compileString,
      "]",
      startPC,
      endPC
      );
   }

void
TR::CompilationInfoPerThreadBase::outputVerboseMMapEntries(
   TR_ResolvedMethod *compilee,
   TR_MethodMetaData *metaData,
   const char *profiledString,
   const char *compileString
   )
   {

#if defined(TR_HOST_64BIT) || defined(TR_TARGET_64BIT)
   static const char *fmtNoEOL="\n%02d%02d%02d.%06ld %.*s,%.*s,%.*s%s%s%s%s,%016p,%016p";
   static const char *  fmtEOL="\n%02d%02d%02d.%06ld %.*s,%.*s,%.*s%s%s%s%s,%016p,%016p\n";
#else
   static const char *fmtNoEOL="\n%02d%02d%02d.%06ld %.*s,%.*s,%.*s%s%s%s%s,%x,%x";
   static const char *  fmtEOL="\n%02d%02d%02d.%06ld %.*s,%.*s,%.*s%s%s%s%s,%x,%x\n";
#endif
   struct timeval time;
   gettimeofday(&time, 0);
   struct tm date;
   if (!localtime_r(&time.tv_sec, &date)) return;

   TR_VerboseLog::vlogAcquire();
   outputVerboseMMapEntry(
      compilee,
      date,
      time,
      reinterpret_cast<void *>(metaData->startPC),
      reinterpret_cast<void *>(metaData->endWarmPC),
      fmtNoEOL,
      profiledString,
      compileString
      );
   if (metaData->startColdPC)
      outputVerboseMMapEntry(
         compilee,
         date,
         time,
         reinterpret_cast<void *>(metaData->startColdPC),
         reinterpret_cast<void *>(metaData->endPC),
         fmtEOL,
         profiledString,
         compileString
         );
   TR_VerboseLog::vlogRelease();
   }
#endif // ifdef TR_HOST_S390

// work around spill issue on this method
#if defined(TR_HOST_S390)
#pragma option_override(TR::CompilationInfo::compile(J9VMThread *, TR_MethodToBeCompiled *, bool), "OPT(SPILL,256)")
#endif

#if defined(J9VM_INTERP_AOT_RUNTIME_SUPPORT) && defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))
TR_MethodMetaData *
TR::CompilationInfoPerThreadBase::installAotCachedMethod(
   J9VMThread *vmThread,
   const void *aotCachedMethod,
   J9Method *method,
   TR_FrontEnd *fe,
   TR::Options *options,
   TR_ResolvedMethod *compilee,
   TR_MethodToBeCompiled *entry,
   TR::Compilation *compiler
   )
   {
   // This function assumes that compilation monitor is released coming in.  This is to allow relocations
   // to be able to grow code caches. We also assume the queue slot monitor to be released coming in.

   if (entry)
      TR_ASSERT(method == entry->getMethodDetails().getMethod(), "assertion failure");

   // We need to perform the relocation for the method in the shared classes cache
   TR::CodeCache *aotMCCRuntimeCodeCache = NULL;

   const J9JITDataCacheHeader *cacheEntry = static_cast<const J9JITDataCacheHeader *>(aotCachedMethod);
   if (entry)
      TR_ASSERT(entry->_oldStartPC == 0, "assertion failure"); // we only locate first time compilations

   TR_MethodMetaData *metaData;
   int32_t returnCode = 0;

   if (_compInfo.getPersistentInfo()->isRuntimeInstrumentationEnabled())
      {
      reloRuntime()->setIsLoading();
      reloRuntime()->initializeHWProfilerRecords(compiler);
      }

   metaData = reloRuntime()->prepareRelocateAOTCodeAndData(vmThread,
                                                           fe,
                                                           aotMCCRuntimeCodeCache,
                                                           cacheEntry,
                                                           method,
                                                           false,
                                                           options,
                                                           compiler,
                                                           compilee);
   setMetadata(metaData);
   returnCode = reloRuntime()->returnCode();

   if (TR::Options::getVerboseOption(TR_VerboseCompilationDispatch))
      TR_VerboseLog::writeLineLocked(TR_Vlog_DISPATCH,
         "prepareRelocateAOTCodeAndData results: j9method=%p metaData=%p returnCode=%d method=%s",
         method, metaData, returnCode, compiler->signature());

   if (_compInfo.getPersistentInfo()->isRuntimeInstrumentationEnabled())
      {
      reloRuntime()->resetIsLoading();
      }

   if (metaData)
      {
      UDATA reloTime = 0;

      if (TrcEnabled_Trc_JIT_AotLoadEnd)
         {
         PORT_ACCESS_FROM_JITCONFIG(_jitConfig);
         reloTime = j9time_usec_clock() - reloRuntime()->reloStartTime();

         Trc_JIT_AotLoadEnd(vmThread, compiler->signature(),
              metaData->startPC, metaData->endWarmPC, metaData->startColdPC, metaData->endPC,
              reloTime, method, metaData, _compInfo.getMethodQueueSize(),
              TR::CompilationInfo::getMethodBytecodeSize(method),
              0);
         }

      if (TR::Options::isAnyVerboseOptionSet(TR_VerbosePerformance, TR_VerboseCompileEnd))
         {
         if (reloTime == 0)
            {
            PORT_ACCESS_FROM_JITCONFIG(_jitConfig);
            reloTime = j9time_usec_clock() - reloRuntime()->reloStartTime();
            }

         TR_VerboseLog::vlogAcquire();
         TR_VerboseLog::writeLine(TR_Vlog_COMP,"(AOT load) ");
         CompilationInfo::printMethodNameToVlog(method);
         TR_VerboseLog::write(" @ " POINTER_PRINTF_FORMAT "-" POINTER_PRINTF_FORMAT, metaData->startPC, metaData->endWarmPC);
         TR_VerboseLog::write(" Q_SZ=%d Q_SZI=%d QW=%d j9m=%p bcsz=%u", _compInfo.getMethodQueueSize(), _compInfo.getNumQueuedFirstTimeCompilations(),
                                _compInfo.getQueueWeight(), method, _compInfo.getMethodBytecodeSize(method));

         if (TR::Options::getVerboseOption(TR_VerbosePerformance))
            {
            TR_VerboseLog::write(" time=%dus", (uint32_t)reloTime);
            }
         if (entry)
            TR_VerboseLog::write(" compThread=%d", getCompThreadId());

         TR_VerboseLog::vlogRelease();
         }

#if defined(TR_HOST_S390)
      if (TR::Options::getVerboseOption(TR_VerboseMMap))
         {
         TR_J9VMBase *vm = TR_J9VMBase::get(_jitConfig, vmThread);
         TR_ResolvedMethod *compilee = vm->createResolvedMethod(compiler->trMemory(), (TR_OpaqueMethodBlock *)method);
         outputVerboseMMapEntries(compilee, metaData, "AOT ", "load");
         }
#endif

      if (J9_EVENT_IS_HOOKED(_jitConfig->javaVM->hookInterface, J9HOOK_VM_DYNAMIC_CODE_LOAD))
         {
         OMR::CodeCacheMethodHeader *ccMethodHeader;
         ALWAYS_TRIGGER_J9HOOK_VM_DYNAMIC_CODE_LOAD(_jitConfig->javaVM->hookInterface, vmThread, method, (U_8*)metaData->startPC, metaData->endWarmPC - metaData->startPC, "JIT warm body", metaData);
         if (metaData->startColdPC)
            {
            // Register the cold code section too
            //
            ALWAYS_TRIGGER_J9HOOK_VM_DYNAMIC_CODE_LOAD(_jitConfig->javaVM->hookInterface, vmThread, method, (U_8*)metaData->startColdPC, metaData->endPC - metaData->startColdPC, "JIT cold body", metaData);
            }
         ccMethodHeader = getCodeCacheMethodHeader((char *)metaData->startPC, 32, metaData);
         if (ccMethodHeader && (metaData->bodyInfo != NULL))
            {
            TR_LinkageInfo *linkageInfo = TR_LinkageInfo::get((void *)metaData->startPC);
            if (linkageInfo->isRecompMethodBody())
               {
               ALWAYS_TRIGGER_J9HOOK_VM_DYNAMIC_CODE_LOAD(_jitConfig->javaVM->hookInterface, vmThread, method, (U_8 *)((char*)ccMethodHeader->_eyeCatcher+4), metaData->startPC - (UDATA)((char*)ccMethodHeader->_eyeCatcher+4), "JIT method header", metaData);
               }
            }
         }

      ++_compInfo._statNumMethodsFromSharedCache;
      }
   else // relocation failed
      {
      if (entry)
         {
         entry->_compErrCode = returnCode;
         entry->setAotCodeToBeRelocated(NULL); // reset if relocation failed
         entry->_tryCompilingAgain = _compInfo.shouldRetryCompilation(entry, compiler); // this will set entry->_doNotUseAotCodeFromSharedCache = true;

         // Feature [Defect 172216/180384]: Implement hints for failed AOT validations.  The idea is that the failed validations are due to the
         // fact that AOT methods are loaded at a lower count than when they were compiled so the JVM is given less opportunity to resolve things.
         // The new hint tells us that a previous relocation failed a validation so specify a higher scount for the next run to give it more chance
         // to resolve things.
         switch (returnCode)
            {
            case compilationAotValidateFieldFailure:
            case compilationAotStaticFieldReloFailure:
            case compilationAotClassReloFailure:
               if ((options->getInitialBCount() != 0) &&
                   (options->getInitialCount() != 0))
                  {
                  TR_J9SharedCache *sc = (TR_J9SharedCache *) (compiler->fej9()->sharedCache());
                  if (sc)
                     sc->addHint(method, TR_HintFailedValidation);
                  }
                break;
            }
         }
      }
   return metaData;
   }
#endif // defined(J9VM_INTERP_AOT_RUNTIME_SUPPORT) && defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM) || defined(TR_HOST_ARM64))

//--------------- queueAOTUpgrade ----------
// The entry currently being processed will be cloned with the clone being an upgrade
// request (which will be performed at cold).
// The j9method currently being compiled, must have been compiled successfully because we use its startPC.
// This routine must be called with compilationMonitor in hand.
// This routine is normally invoked by the compilation thread.
// I think we can avoid scanning the queue for a duplicate because such a duplicate cannot exist
// long as the original is still being processed.
//-----------------------------------------
void TR::CompilationInfo::queueForcedAOTUpgrade(TR_MethodToBeCompiled *originalEntry, uint16_t hints, TR_FrontEnd *fe)
   {
#ifndef PUBLIC_BUILD
#if defined(J9VM_INTERP_AOT_RUNTIME_SUPPORT) && defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390))

   TR_ASSERT(!originalEntry->isDLTCompile(), "No DLTs allowed in queueForcedAOTUpgrade"); // no DLT here
   TR_ASSERT(originalEntry->_compInfoPT && originalEntry->_compInfoPT->_methodBeingCompiled == originalEntry, "assertion failure");
   // This must be an AOT load
   if (!TR::Options::getCmdLineOptions()->allowRecompilation())
      return;

   // TODO: if this is already available, pass it as a parameter
   TR_PersistentMethodInfo *methodInfo = TR::Recompilation::getMethodInfoFromPC(originalEntry->_newStartPC);
   if (!methodInfo)
      return;

   // If quickstart and we are in startup phase, do not perform the
   // upgrade hints for some methods that we know are memory expensive
   if (TR::Options::isQuickstartDetected() && // TODO: we may want to apply the same logic for server mode as well
       _jitConfig->javaVM->phase != J9VM_PHASE_NOT_STARTUP)
      {
      if (hints & TR_HintLargeMemoryMethodC)
         return; // Don't want to increase the footprint
      }

   // Search for an empty entry in the methodPool.
   TR_MethodToBeCompiled *cur = getCompilationQueueEntry();
   if (!cur)
      return;

   // must create a new optimization plan
   TR_Hotness hotness = cold;
   bool doProfiling = false;
   if (hints & TR_HintScorching)
      {
      hotness = veryHot;
      doProfiling = !TR::Options::getCmdLineOptions()->getOption(TR_DisableProfiling) &&
                    !methodInfo->profilingDisabled();
      }
   else if (hints & TR_HintHot)
      {
      hotness = hot;
      }
   else // See if we should upgrade at warm
      {
      // Do not upgrade at warm if the compilation is known to take a lot of memory or CPU
      // and we are in startup phase
      if (_jitConfig->javaVM->phase == J9VM_PHASE_NOT_STARTUP || !(hints & (TR_HintLargeMemoryMethodW | TR_HintLargeCompCPUW)))
         {
         if (!TR::Options::isQuickstartDetected())
            hotness = warm;
         else  if (TR::Options::getCmdLineOptions()->getOption(TR_UpgradeBootstrapAtWarm))// This is a JIT option because it pertains to JIT compilations
            {
            // To reduce affect on short applications like tomcat
            // upgrades to warm should be performed only outside the grace period
            //if (getPersistentInfo()->getElapsedTime() >= (uint64_t)getPersistentInfo()->getClassLoadingPhaseGracePeriod())
               {
               TR_OpaqueMethodBlock *method = (TR_OpaqueMethodBlock *) originalEntry->getMethodDetails().getMethod();
               TR_J9VMBase *fej9 = (TR_J9VMBase *)fe;
               if (fej9->isClassLibraryMethod(method))
                  hotness = warm;
               }
            }
         }
      }
   TR_OptimizationPlan *plan = TR_OptimizationPlan::alloc(hotness, doProfiling, true);
   if (!plan) // OOM
      {
      cur->_freeTag = ENTRY_INITIALIZED; // entry must appear as being initialized

      // put back into the pool
      recycleCompilationEntry(cur);
      return;
      }

   // Do not mark this compilation as un upgrade; otherwise we would try again to store
   // a hint in the shared cache when the upgrade is complete
   //
   cur->initialize(originalEntry->getMethodDetails(), originalEntry->_newStartPC, CP_ASYNC_BELOW_NORMAL, plan);
   cur->_jitStateWhenQueued = getPersistentInfo()->getJitState();

   TR_LinkageInfo *linkageInfo = TR_LinkageInfo::get(originalEntry->_newStartPC);
   linkageInfo->setIsBeingRecompiled(); // mark that we try to compile it

   methodInfo->setNextCompileLevel(plan->getOptLevel(), plan->insertInstrumentation());
   methodInfo->setReasonForRecompilation(TR_PersistentMethodInfo::RecompDueToForcedAOTUpgrade);
   _statNumForcedAotUpgrades++;

   cur->_entryTime = getPersistentInfo()->getElapsedTime(); // cheaper version
   incrementMethodQueueSize(); // one more method added to the queue
   // No need to increment _numQueuedFirstTimeCompilations because this is a recompilation

   // Must determine the entry weight; let's use something simpler
   // because we know the opt level can be either cold or warm
   // and we know this is not JNI or other thunk
   //
   uint8_t entryWeight; // must be less than 256
   switch (hotness)
      {
      case cold: entryWeight = COLD_WEIGHT; break;
      case warm:
         {
         J9Method *method = originalEntry->getMethodDetails().getMethod();
         J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
         if (J9ROMMETHOD_HAS_BACKWARDS_BRANCHES(romMethod))
            entryWeight = WARM_LOOPY_WEIGHT;
         else
            entryWeight = WARM_LOOPLESS_WEIGHT;
         }
         break;
      case hot: entryWeight = HOT_WEIGHT; break;
      case veryHot: entryWeight = VERY_HOT_WEIGHT; break;
      default: entryWeight = THUNKS_WEIGHT;
      } // end switch

   cur->_weight = entryWeight;
   _queueWeight += (int32_t)entryWeight; // increase queue weight

   cur->_async = true;

   // Do not activate any new compilation thread

   // Instead of searching for a matching entry, just queue this entry
   // We can away with scanning the queue for duplicates because the original method is still
   // at the head of the queue
   //
   queueEntry(cur);
   //fprintf(stderr, "Forcefully upgraded j9method %p (entry=%p)\n", cur->_method, cur);
#endif // #if defined(J9VM_INTERP_AOT_RUNTIME_SUPPORT) && defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390))
#endif //!PUBLIC_BUILD
   }


// This method is executed by compilation threads at the end of a compilation
// and needs to be executed with the compQueueMonitor in hand.
// Prerequisites: getCompilation() and getMetadata() return valid answers
void
TR::CompilationInfoPerThreadBase::generatePerfToolEntry()
   {
#if defined(LINUX)
   TR_ASSERT(getCompilation() && getMetadata(), "generatePerfToolEntry() must be executed only for successful compilations");
   // At this point we have the compilationQueueMonitor so we don't run into concurrency issues
   static bool firstAttempt = true;
   if (firstAttempt)
      {
      firstAttempt = false;
      uintptrj_t jvmPid = getCompilation()->fej9()->getProcessID();
      static const int maxPerfFilenameSize = 15 + sizeof(jvmPid)* 3; // "/tmp/perf-%ld.map"
      char perfFilename[maxPerfFilenameSize] = { 0 };

      int numCharsWritten = snprintf(perfFilename, maxPerfFilenameSize, "/tmp/perf-%ld.map", jvmPid);
      if (numCharsWritten > 0 && numCharsWritten < maxPerfFilenameSize)
         {
         TR::CompilationInfoPerThreadBase::_perfFile = j9jit_fopen(perfFilename, "a", true, false);
         }
      if (!getPerfFile()) // couldn't open the file
         {
         if (TR::Options::getJITCmdLineOptions()->getVerboseOption(TR_VerboseCompFailure))
            TR_VerboseLog::writeLineLocked(TR_Vlog_FAILURE, "t=%u WARNING: Cannot open perf tool file: %s",
               (uint32_t)_compInfo.getPersistentInfo()->getElapsedTime(), perfFilename); // even if snprintf failed above we should still be ok.
         // Do we want a message without verbose log being specified?
         }
      }
   if (getPerfFile())
      {
      j9jit_fprintf(getPerfFile(), "%p %lX %s_%s\n", getMetadata()->startPC, getMetadata()->endWarmPC - getMetadata()->startPC,
         getCompilation()->signature(), getCompilation()->getHotnessName(getCompilation()->getMethodHotness()));
      // If there is a cold section, add another line
      if (getMetadata()->startColdPC)
         {
         j9jit_fprintf(getPerfFile(), "%p %lX %s_%s\n", getMetadata()->startColdPC, getMetadata()->endPC - getMetadata()->startColdPC,
            getCompilation()->signature(), getCompilation()->getHotnessName(getCompilation()->getMethodHotness())); // should we change the name of the method?
         }
      // Flushing degrades performance, but ensures that we have the data
      // written even if the JVM is abruptly terminated
      j9jit_fflush(getPerfFile());
      }
#endif
   }

const void*
findAotBodyInSCC(J9VMThread *vmThread, const J9ROMMethod *romMethod)
   {
#if defined(J9VM_INTERP_AOT_RUNTIME_SUPPORT) && defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM))
   UDATA flags = 0;
   const void *aotCachedMethod = vmThread->javaVM->sharedClassConfig->findCompiledMethodEx1(vmThread, romMethod, &flags);
   if (!(flags & J9SHR_AOT_METHOD_FLAG_INVALIDATED))
      return aotCachedMethod; // possibly NULL
   else
#endif
      return NULL;
   }

const void*
findAotBodyInSCC(J9VMThread *vmThread, J9Method *method)
   {
   return findAotBodyInSCC(vmThread, J9_ROM_METHOD_FROM_RAM_METHOD(method));
   }


bool isMethodIneligibleForAot(const J9ROMMethod *romMethod, const J9ROMClass *romClass)
   {
   J9UTF8 *className = J9ROMCLASS_CLASSNAME(romClass);

   // Don't AOT-compile anything in j/l/i for now
   if (strncmp(utf8Data(className), "java/lang/invoke/", sizeof("java/lang/invoke/") - 1) == 0)
      return true;

   if (J9UTF8_LENGTH(className) == 36 &&
      0 == memcmp(utf8Data(className), "com/ibm/rmi/io/FastPathForCollocated", 36))
      {
      J9UTF8 *utf8 = J9ROMMETHOD_GET_NAME(romClass, romMethod);
      if (J9UTF8_LENGTH(utf8) == 21 &&
         0 == memcmp(J9UTF8_DATA(utf8), "isVMDeepCopySupported", 21))
         return true;
      }
   // don't AOT compile BigD methods, just JIT compile them
   if ((!TR::Options::getCmdLineOptions()->getOption(TR_DisableDFP) && !TR::Options::getAOTCmdLineOptions()->getOption(TR_DisableDFP))
      &&
      (TR::Compiler->target.cpu.supportsDecimalFloatingPoint()
#ifdef TR_TARGET_S390
         || TR::Compiler->target.cpu.getSupportsDecimalFloatingPointFacility()
#endif
         )
      && TR_J9MethodBase::isBigDecimalMethod((J9ROMMethod *)romMethod, (J9ROMClass *)romClass)
   )
      return true;
   return false;
   }


bool isMethodIneligibleForAot(J9Method *method)
   {
   return isMethodIneligibleForAot(J9_ROM_METHOD_FROM_RAM_METHOD(method), J9_CLASS_FROM_METHOD(method)->romClass);
   }

bool
TR::CompilationInfoPerThreadBase::shouldPerformLocalComp(const TR_MethodToBeCompiled *entry)
   {
   bool doLocalComp = false;
   static char *localColdCompilations = feGetEnv("TR_LocalColdCompilations");
   // Perform all AOT compilations remotely.
   // As a heuristic, cold compilations should be performed locally because
   // they are supposed to be cheap with respect to memory and CPU.
   //
   if (!entry->_useAotCompilation && entry->_optimizationPlan->getOptLevel() <= cold &&
      (TR::Options::getCmdLineOptions()->getOption(TR_EnableJITServerHeuristics) || localColdCompilations) ||
      !JITServer::ClientStream::isServerCompatible(OMRPORT_FROM_J9PORT(_jitConfig->javaVM->portLibrary)) ||
      (!JITaaSHelpers::isServerAvailable() && !JITaaSHelpers::shouldRetryConnection(OMRPORT_FROM_J9PORT(_jitConfig->javaVM->portLibrary))))
      doLocalComp = true;

   return doLocalComp;
   }

/**
 * @brief TR::CompilationInfoPerThreadBase::preCompilationTasks
 * @param vmThread Pointer to the current J9VMThread (input)
 * @param javaVM Pointer to the J9JavaVM (input)
 * @param entry Pointer to the TR_MethodToBeCompiled entry (input)
 * @param method Pointer to the J9Method (input)
 * @param aotCachedMethod Pointer to a pointer to the AOT code to be relocated (if it exists) (output)
 * @param trMemory Reference to the TR_Memory associated with this compilation (input)
 * @param canDoRelocatableCompile Reference to a bool; Indicates if the current compilation satisfies requirements for being AOT compiled (output)
 * @param eligibleForRelocatableCompile Reference to a bool; Indicates if the current compilation is eligible for AOT (output)
 * @param reloRuntime Pointer to a TR_RelocationRuntime; NULL if JVM does not have AOT support (input)
 *
 * This method is used to perform a set of tasks needed before attempting a compilation.
 *
 * This method can throw an exception.
 */
void
TR::CompilationInfoPerThreadBase::preCompilationTasks(J9VMThread * vmThread,
                                                      J9JavaVM *javaVM,
                                                      TR_MethodToBeCompiled *entry,
                                                      J9Method *method,
                                                      const void **aotCachedMethod,
                                                      TR_Memory &trMemory,
                                                      bool &canDoRelocatableCompile,
                                                      bool &eligibleForRelocatableCompile,
                                                      TR_RelocationRuntime *reloRuntime)
   {
   // Check to see if we find an AOT version in the shared cache
   //
   entry->setAotCodeToBeRelocated(NULL);  // make sure decision to load AOT comes from below and not previous compilation/relocation pass
   entry->_doAotLoad = false;

   if (entry->_methodIsInSharedCache == TR_yes &&    // possible AOT load
      !TR::CompilationInfo::isCompiled(method) &&
      !entry->_doNotUseAotCodeFromSharedCache &&
      !TR::Options::getAOTCmdLineOptions()->getOption(TR_NoLoadAOT) &&
      !(jitConfig->runtimeFlags & J9JIT_TOSS_CODE))
      {
      // Determine whether the compilation filters allows me to relocate
      // Filters should not be applied to out-of-process compilations
      // because decisions on what needs to be compiled are done at the client
      //
      TR_Debug *debug = TR::Options::getDebug();
      bool canRelocateMethod = true;
      if (debug && !entry->isOutOfProcessCompReq())
         {
         setCompilationShouldBeInterrupted(0); // zero the flag because createResolvedMethod calls
                                               // acquire/releaseVMaccessIfNeeded and may see the flag set by previous compilation
         TR_FilterBST *filter = NULL;
         TR_J9VMBase *fe = TR_J9VMBase::get(_jitConfig, vmThread);

         if (NULL == fe)
            {
            throw std::bad_alloc();
            }
           
         TR_ResolvedMethod *resolvedMethod = fe->createResolvedMethod(&trMemory, (TR_OpaqueMethodBlock *)method);
         if (!debug->methodCanBeRelocated(&trMemory, resolvedMethod, filter) ||
             !debug->methodCanBeCompiled(&trMemory, resolvedMethod, filter))
            canRelocateMethod = false;
         }

      if (canRelocateMethod && !entry->_oldStartPC)
         {
         // Find the AOT body in the SCC
         //
         *aotCachedMethod = entry->isOutOfProcessCompReq() ? // NOTE: AOT loads at the server are not yet working
            findAotBodyInSCC(vmThread, entry->getMethodDetails().getRomMethod()) :
            findAotBodyInSCC(vmThread, method);
         if (*aotCachedMethod)
            {
#ifdef COMPRESS_AOT_DATA
            TR_AOTMethodHeader *aotMethodHeader = (TR_AOTMethodHeader*)(((J9JITDataCacheHeader*)*aotCachedMethod) + 1);
            if (aotMethodHeader->flags & TR_AOTMethodHeader_CompressedMethodInCache)
               {
               /**
                 * For each AOT compiled method we store in the shareclass cache we have following layout.
                 * ---------------------------------------------------------------------------------------
                 * | CompiledMethodWrapper | J9JITDataCacheHeader | AOTMethodHeader | Metadata | Codedata|
                 * ---------------------------------------------------------------------------------------
                 * When we compile and store a method, it stores J9RomMethod for the method in the CompiledMethodWrapper along side
                 * size of metadata and codedata in the cache. Now when we request a method from shared class cache
                 * it returns address that points to J9JITDataCacheHeader. As while decompressing the data, we need information about
                 * the size of original data (We get that information from AOTMethodHeader) and size of deflated data in the cache (Which is stored in
                 * CompiledMethodWrapper)
                 * So to access data size in shared class cache, we need to access CompiledMethodWrapper.
                 */
               CompiledMethodWrapper *wrapper = ((CompiledMethodWrapper*)(*aotCachedMethod))-1;
               int sizeOfCompressedDataInCache = wrapper->dataLength;
               int originalDataSize = aotMethodHeader->compileMethodDataSize + aotMethodHeader->compileMethodCodeSize;
               void *originalData = trMemory.allocateHeapMemory(originalDataSize);
               int aotMethodHeaderSize = sizeof(J9JITDataCacheHeader) + sizeof(TR_AOTMethodHeader);
               memcpy(originalData, *aotCachedMethod, aotMethodHeaderSize);
               if (inflateBuffer((U_8 *)(*aotCachedMethod)+aotMethodHeaderSize, sizeOfCompressedDataInCache-aotMethodHeaderSize, (U_8* )(originalData)+aotMethodHeaderSize, originalDataSize-aotMethodHeaderSize) == DECOMPRESSION_FAILED)
                  {
                  if (TR::Options::getVerboseOption(TR_VerboseAOTCompression))
                     {
                     J9UTF8 *className;
                     J9UTF8 *name;
                     J9UTF8 *signature;
                     getClassNameSignatureFromMethod(method, className, name, signature);
                     TR_VerboseLog::writeLineLocked(TR_Vlog_AOTCOMPRESSION, "!%.*s.%.*s%.*s : Decompression of method data failed - Compressed Method Size = %d bytes, Stored original method size = %d bytes",
                        J9UTF8_LENGTH(className), (char *) J9UTF8_DATA(className),
                        J9UTF8_LENGTH(name), (char *) J9UTF8_DATA(name),
                        J9UTF8_LENGTH(signature), (char *) J9UTF8_DATA(signature),
                        sizeOfCompressedDataInCache, originalDataSize);
                     }
                  // If we can not inflate the method data, JIT compile the method.
                  canRelocateMethod = false;
                  *aotCachedMethod = NULL;
                  entry->_doNotUseAotCodeFromSharedCache = false;
                  }
               else
                  {
                  if (TR::Options::getVerboseOption(TR_VerboseAOTCompression))
                     {
                     J9UTF8 *className;
                     J9UTF8 *name;
                     J9UTF8 *signature;
                     getClassNameSignatureFromMethod(method, className, name, signature);
                     TR_VerboseLog::writeLineLocked(TR_Vlog_AOTCOMPRESSION, "%.*s.%.*s%.*s : Decompression of method data Successful - Compressed Method Size = %d bytes, Stored original method size = %d bytes",
                        J9UTF8_LENGTH(className), (char *) J9UTF8_DATA(className),
                        J9UTF8_LENGTH(name), (char *) J9UTF8_DATA(name),
                        J9UTF8_LENGTH(signature), (char *) J9UTF8_DATA(signature),
                        sizeOfCompressedDataInCache, originalDataSize);
                     }
                  *aotCachedMethod = originalData;
                  }
               }
            if (canRelocateMethod)
               {
               // All conditions are met for doing an AOT load. Mark this fact on the entry.
               entry->_doAotLoad = true;
               entry->setAotCodeToBeRelocated(*aotCachedMethod);
               reloRuntime->setReloStartTime(getTimeWhenCompStarted());
               }
#else
               // All conditions are met for doing an AOT load. Mark this fact on the entry.
               entry->_doAotLoad = true;
               entry->setAotCodeToBeRelocated(*aotCachedMethod);
               reloRuntime->setReloStartTime(getTimeWhenCompStarted());
#endif
            }
         }
      else if (entry->_oldStartPC)
         {
         J9JITExceptionTable *oldMetaData = jitConfig->jitGetExceptionTableFromPC(vmThread, (UDATA)entry->_oldStartPC);
         if (oldMetaData)
            {
            TR_PersistentJittedBodyInfo *oldBodyInfo = (TR_PersistentJittedBodyInfo *)oldMetaData->bodyInfo;
            TR_ASSERT(oldBodyInfo && oldBodyInfo->getIsInvalidated(), "entry is interpreted and has a non-NULL _oldStartPC but the old body has not been invalidated.");
            }
         }
      }

   if (!entry->isAotLoad()) // We don't/can't relocate this method
      {
      if (TR::Options::getCmdLineOptions()->getOption(TR_ActivateCompThreadWhenHighPriReqIsBlocked))
         {
         // AOT loads that failed may be retried as JIT compilations
         // Such requests inherit the high priority of the AOT load
         // Now that this request is out of the queue we can safely change its
         // priority to a lower value so that when another AOT load comes along
         // a secondary compilation thread will be activated

         // Is this a JIT retrial of a failed AOT load?
         if (entry->_methodIsInSharedCache == TR_yes &&  // possible AOT load
            entry->_doNotUseAotCodeFromSharedCache &&    // transformed into a JIT request
            entry->_compilationAttemptsLeft < MAX_COMPILE_ATTEMPTS) // because it failed previously
            {
            // Lower priority to CP_ASYNC_NORMAL
            if (entry->_priority > CP_ASYNC_NORMAL)
               entry->_priority = CP_ASYNC_NORMAL;
            }
         }
      }

   if (!entry->isAotLoad())
      {
      // Determine if we need to perform an AOT compilation
      //
      TR::IlGeneratorMethodDetails & details = entry->getMethodDetails();

      if (entry->isOutOfProcessCompReq())
         {
         // Since all of the preliminary checks have already been done at the client,
         // eligibleForRelocatableCompile should be true at the server side if the client
         // requests AOT.
         eligibleForRelocatableCompile = entry->_useAotCompilation;
         }
      else
         {
         eligibleForRelocatableCompile =
            TR::Options::sharedClassCache() &&
            !details.isNewInstanceThunk() &&
            !entry->isJNINative() &&
            !details.isMethodHandleThunk() &&
            !TR::CompilationInfo::isCompiled(method) &&
            !entry->isDLTCompile() &&
            !entry->_doNotUseAotCodeFromSharedCache &&
            reloRuntime->isRomClassForMethodInSharedCache(method, javaVM) &&
            !isMethodIneligibleForAot(method) &&
            (!TR::Options::getAOTCmdLineOptions()->getOption(TR_AOTCompileOnlyFromBootstrap) ||
               TR_J9VMBase::get(jitConfig, vmThread)->isClassLibraryMethod((TR_OpaqueMethodBlock *)method), true);
         }

      bool sharedClassTest = eligibleForRelocatableCompile &&
         !TR::Options::getAOTCmdLineOptions()->getOption(TR_NoStoreAOT);

      bool isSecondAOTRun =
         !TR::Options::getAOTCmdLineOptions()->getOption(TR_NoAotSecondRunDetection) &&
         TR::Options::sharedClassCache() &&
         _compInfo.numMethodsFoundInSharedCache() >= TR::Options::_aotMethodThreshold &&
         _compInfo._statNumAotedMethods <= TR::Options::_aotMethodCompilesThreshold;

      if (!TR::Options::canJITCompile() && isSecondAOTRun) // detect second AOT run
         {
         if (!TR::Options::getAOTCmdLineOptions()->getOption(TR_ForceAOT))
            TR::CompilationInfo::disableAOTCompilations();
         TR::Options::getCmdLineOptions()->setOption(TR_DisableInterpreterProfiling, true);
         }

      // Decide if we want an AOT vm or not
      if (sharedClassTest)
         {
         if (TR::Options::getAOTCmdLineOptions()->getOption(TR_ForceAOT) || entry->isOutOfProcessCompReq())
            {
            canDoRelocatableCompile = true;
            }
         else
            {
            // Heuristic: generate AOT only for downgraded compilations in the second run
            if (!isSecondAOTRun &&
                entry->_optimizationPlan->isOptLevelDowngraded())
               canDoRelocatableCompile = true;
            }
         }
      }

   // If we are allowed to do relocatable compilations, just do it
   if (canDoRelocatableCompile)
      {
      entry->_useAotCompilation = true;

      if (entry->isOutOfProcessCompReq())
         _vm = TR_J9VMBase::get(_jitConfig, vmThread, TR_J9VMBase::J9_SHARED_CACHE_SERVER_VM);
      else
         _vm = TR_J9VMBase::get(_jitConfig, vmThread, TR_J9VMBase::AOT_VM);

      if ((_compInfo.getPersistentInfo()->getRemoteCompilationMode() == JITServer::CLIENT) &&
         !shouldPerformLocalComp(entry))
         {
         entry->setRemoteCompReq();
         }
      }
   else if (!entry->isOutOfProcessCompReq())
      {
      // This is used for JIT compilations and AOT loads on the client side or non-JITServer
      _vm = TR_J9VMBase::get(_jitConfig, vmThread);
      entry->_useAotCompilation = false;

      // When both canDoRelocatableCompile and TR::Options::canJITCompile() are false,
      // remote compilation request should not be used.
      if ((_compInfo.getPersistentInfo()->getRemoteCompilationMode() == JITServer::CLIENT) &&
          TR::Options::canJITCompile())
         {
         bool doLocalCompilation = shouldPerformLocalComp(entry);

         // If this is a remote sync compilation, change it to a local sync compilation.
         // After the local compilation is completed successfully, a remote async compilation
         // will be scheduled in compileOnSeparateThread().
         TR::IlGeneratorMethodDetails & details = entry->getMethodDetails();
         if (!doLocalCompilation &&
             !entry->_async &&
             !entry->isJNINative() &&
             !details.isNewInstanceThunk() &&
             !details.isMethodHandleThunk() &&
             TR::Options::getCmdLineOptions()->getOption(TR_EnableJITServerHeuristics) &&
             !TR::Options::getCmdLineOptions()->getOption(TR_DisableUpgradingColdCompilations) &&
             TR::Options::getCmdLineOptions()->allowRecompilation())
            {
            doLocalCompilation = true;
            entry->_origOptLevel = entry->_optimizationPlan->getOptLevel();
            entry->_optimizationPlan->setOptLevel(cold);
            entry->_optimizationPlan->setOptLevelDowngraded(true);

            if (TR::Options::getVerboseOption(TR_VerboseCompilationDispatch))
               TR_VerboseLog::writeLineLocked(TR_Vlog_DISPATCH, "Changed the remote sync compilation to a local sync cold compilation: j9method=%p",
               entry->getMethodDetails().getMethod());
            }

         //TODO: Do we still need the following?
         // In another heuristic we could downgrade all first time compilations
         // that happen during startup.
         if (false)
            {
            if (!TR::CompilationInfo::isCompiled(method) &&  //recompilations should be sent remotely
               !TR::Options::getCmdLineOptions()->getOption(TR_DontDowngradeToCold) &&
               !TR::Options::getCmdLineOptions()->getOption(TR_DisableUpgradingColdCompilations) &&
               TR::Options::getCmdLineOptions()->allowRecompilation() &&
               entry->_optimizationPlan->getOptLevel() == warm)
               {
               doLocalCompilation = true;
               entry->_optimizationPlan->setOptLevel(cold);
               entry->_optimizationPlan->setOptLevelDowngraded(true);
               // JITaaS TODO: queue a remote upgrade right away, but at a lower priority
               // If so, disable GCR trees for those cold compilations.
               }
            }

         if (!doLocalCompilation)
            entry->setRemoteCompReq();
         }
      }
   else
      {
      // (entry->isOutOfProcessCompReq() && !canDoRelocatableCompile
      // AOT loads or JIT compilation on the server side.
      // Its _vm and entry->_useAotCompilation have been set in TR::CompilationInfoPerThreadRemote::processEntry
      TR_ASSERT(!entry->_useAotCompilation, "Client requests AOT compilation but server cannot do remote AOT compilation\n");
      }
   }

/**
 * @brief TR::CompilationInfoPerThreadBase::postCompilationTasks
 * @param vmThread Pointer to the current J9VMThread (input)
 * @param entry Pointer to the TR_MethodToBeCompiled entry (input)
 * @param method Pointer to the J9Method (input)
 * @param aotCachedMethod Pointer to the AOT code to be relocated (if it exists) (input)
 * @param metaData Pointer to the TR_MethodMetaData created on a successful compile (input)
 * @param canDoRelocatableCompile Indicates if the current compilation could be AOT compiled (input)
 * @param eligibleForRelocatableCompile Indicates if the current compilation was eligible for AOT (input)
 * @param reloRuntime Pointer to a TR_RelocationRuntime; NULL if JVM does not have AOT support (input)
 * @return StartPC of the compiled body, NULL if the compilation failed
 *
 * This method is used to perform a set of tasks needed after attempting a compilation; it handles
 * successful and failed compilations, performing the appropriate clean up needed.
 *
 * IMPORTANT: Because this method is called from both the try and catch blocks in
 * TR::CompilationInfoPerThreadBase::compile, an exception should never escape from this method.
 * Calls to this method could be guarded with a try/catch, but that defeats this
 * method's purpose - an escaped exception here would prevent necessary cleanup.
 */
void *
TR::CompilationInfoPerThreadBase::postCompilationTasks(J9VMThread * vmThread,
                                                       TR_MethodToBeCompiled *entry,
                                                       J9Method *method,
                                                       const void *aotCachedMethod,
                                                       TR_MethodMetaData *metaData,
                                                       bool canDoRelocatableCompile,
                                                       bool eligibleForRelocatableCompile,
                                                       TR_RelocationRuntime *reloRuntime)
   {
   // JITServer cleanup tasks
   if (_compInfo.getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER)
      {
      ((TR::CompilationInfoPerThread*)this)->getClassesThatShouldNotBeNewlyExtended()->clear();
      }
   else if (_compInfo.getPersistentInfo()->getRemoteCompilationMode() == JITServer::CLIENT)
      {
      if (entry->isRemoteCompReq())
         {
         TR::ClassTableCriticalSection commit(_vm);

         // clear bit for this compilation
         _compInfo.resetCHTableUpdateDone(getCompThreadId());

         // freshen up newlyExtendedClasses
         auto newlyExtendedClasses = _compInfo.getNewlyExtendedClasses();
         for (auto it = newlyExtendedClasses->begin(); it != newlyExtendedClasses->end();)
            {
            it->second &= _compInfo.getCHTableUpdateDone();
            if (it->second)
               ++it;
            else
               it = newlyExtendedClasses->erase(it);
            }
         }
      else // client executing a compilation locally
         {
         // The bit for this compilation thread should be 0 because we never sent any updates to the server
         TR_ASSERT((_compInfo.getCHTableUpdateDone() & (1 << getCompThreadId())) == 0, 
            "For local compilations _chTableUpdateFlags should not have the bit set for this comp ID");
         }
      }

   void *startPC = NULL;
   if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableUpdateAOTBytesSize) &&
       metaData &&                                                            // Compilation succeeded
       !canDoRelocatableCompile &&                                            // Non-AOT Compilation
       eligibleForRelocatableCompile &&                                       // Was eligible for AOT Compilation
       TR::Options::getAOTCmdLineOptions()->getOption(TR_NoStoreAOT) &&       // AOT Disabled
       TR_J9SharedCache::isSharedCacheDisabledBecauseFull(&_compInfo) == TR_yes) // AOT Compilation Disabled because SCC is full
      {
      // If the current compilation could have been an AOT compilation
      // had there been space in the SCC, send the code size to the VM
      // so that they can suggest an appropriate size to the user for a
      // subsequent run
      int32_t codeSize = _compInfo.calculateCodeSize(metaData);
      _compInfo.increaseUnstoredBytes(codeSize, 0);
      }

   // We may want to track the CPU time spent by compilation threads periodically
   if (_onSeparateThread)
      {
      TR::CompilationInfoPerThread *cipt = (TR::CompilationInfoPerThread*)this;
      if (cipt->getCompThreadCPU().update() && // returns true if an update happened
          cipt->getCompThreadCPU().isFunctional())
         {
         int32_t CPUmillis = cipt->getCompThreadCPU().getCpuTime() / 1000000;
         // May issue a trace point if enabled
         Trc_JIT_CompCPU(vmThread, getCompThreadId(), CPUmillis);

         // May issue a message in the vlog if requested
         if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCompilationThreads))
            TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "t=%6u CPU time spent so far in compThread:%d = %d ms",
                                        (uint32_t)_compInfo.getPersistentInfo()->getElapsedTime(), getCompThreadId(), CPUmillis);
         }
      }

   // If compilation failed, we must delete any assumptions that might still exist in persistent memory
   if (_compiler && metaData == 0) // compilation failed
      {
      // We may, or may not have metadata
      if (getMetadata())
         {
         getMetadata()->runtimeAssumptionList = NULL;
         }
      }

   // Re-acquire the compilation monitor so that we can update the J9Method.
   //
   _compInfo.debugPrint(vmThread, "\tacquiring compilation monitor\n");
   _compInfo.acquireCompMonitor(vmThread);
   _compInfo.debugPrint(vmThread, "+CM\n");
   if (_onSeparateThread)
      {
      // Acquire the queue slot monitor now
      //
      _compInfo.debugPrint(vmThread, "\tre-acquiring queue-slot monitor\n");
      entry->acquireSlotMonitor(vmThread);
      _compInfo.debugPrint(vmThread, "+AM-", entry);
      }

   if (TR::CompilationInfo::shouldAbortCompilation(entry, _compInfo.getPersistentInfo()))
      {
      metaData = 0;
      }
   else if (TR::CompilationInfo::shouldRetryCompilation(entry, _compiler))
      {
      startPC = entry->_oldStartPC; // startPC == oldStartPC means compilation failure
      entry->_tryCompilingAgain = true;
      }
   else // compilation will not be retried, either because it succeeded or because we don't want to
      {
      TR_PersistentJittedBodyInfo *bodyInfo;
      // JITServer: Can not acquire the jitted body info on the server
      if (!entry->isOutOfProcessCompReq() && entry->isDLTCompile() && !startPC && TR::CompilationInfo::isCompiled(method) && // DLT compilation that failed too many times
         (bodyInfo = TR::Recompilation::getJittedBodyInfoFromPC(method->extra)))  // do not use entry->_oldStartPC which is probably 0. Use the most up-to-date startPC
         bodyInfo->getMethodInfo()->setHasFailedDLTCompRetrials(true);

      startPC = TR::CompilationInfo::compilationEnd(
         vmThread,
         entry->getMethodDetails(),
         jitConfig,
         metaData ? reinterpret_cast<void *>(metaData->startPC) : 0,
         entry->_oldStartPC,
         _vm,
         entry,
         _compiler);

      if (TR::Options::getVerboseOption(TR_VerboseCompilationDispatch))
         TR_VerboseLog::writeLineLocked(TR_Vlog_DISPATCH, "CompilationEnd returning startPC=%p  metadata=%p", startPC, metaData);
      // AOT compilations can fail on purpose because we want to load
      // the AOT body later on. This case is signalled by having a metaData != 0
      // but a startPC == entry->_oldStartPC == 0
      if (metaData && !entry->_oldStartPC && !startPC && !entry->isOutOfProcessCompReq() && !entry->isRemoteCompReq())
         {
         TR_ASSERT(_vm->isAOT_DEPRECATED_DO_NOT_USE(), "compilationEnd() can fail only for relocating AOT compilations\n");
         TR_ASSERT(!entry->_oldStartPC, "We expect compilationEnd() to fail only for AOT compilations which are first time compilations\n");

         TR::CompilationInfo::replenishInvocationCount(method, _compiler);
         }

      if (!metaData && !entry->_oldStartPC && // First time compilation failed
         !entry->isOutOfProcessCompReq())
         {
         if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCompFailure))
            {
            TR_VerboseLog::vlogAcquire();
            TR_VerboseLog::writeLine(TR_Vlog_COMP, "Method ");
            CompilationInfo::printMethodNameToVlog(method);
            TR_VerboseLog::write(" will continue as interpreted");
            TR_VerboseLog::vlogRelease();
            }
         if (entry->_compErrCode != compilationRestrictedMethod && // do not look at methods excluded by filters
             entry->_compErrCode != compilationExcessiveSize) // do not look at failures due to code cache size
            {
            int32_t numSeriousFailures = _compInfo.incNumSeriousFailures();
            if (numSeriousFailures > TR::Options::_seriousCompFailureThreshold)
               {
               // Generate a trace point
               Trc_JIT_ManyCompFailures(vmThread, numSeriousFailures);
               }
            }
         }

      if (entry->isAotLoad() && entry->_oldStartPC == 0 && startPC != 0) // AOT load that succeeded
         {
         // We know this is a first time compilation (otherwise we would not attempt an AOT load)
         // Before grabbing the compilation monitor and loading the AOT body
         // let's check the AOT hints (optimistically assuming that the AOT load will succeed)
         if (_onSeparateThread && entry->_async &&  // KEN need to pass in onSeparateThread?
         (TR::Options::getAOTCmdLineOptions()->getEnableSCHintFlags() & (TR_HintUpgrade | TR_HintHot | TR_HintScorching)) &&
         _vm->sharedCache())
            {
            // read all hints at once because we may rely on more than just one type
            uint16_t hints = _vm->sharedCache()->getAllEnabledHints(method) & (TR_HintUpgrade | TR_HintHot | TR_HintScorching);
            // Now let's see if we need to schedule an AOT upgrade
            if (hints)
               {
               // must do this before queueing for an upgrade
               entry->_newStartPC = startPC;

               static char *disableQueueLPQAOTUpgrade = feGetEnv("TR_DisableQueueLPQAOTUpgrade");
               if (TR::Options::getAOTCmdLineOptions()->getOption(TR_EnableSymbolValidationManager)
                   && !disableQueueLPQAOTUpgrade)
                  {
                  _compInfo.getLowPriorityCompQueue().addUpgradeReqToLPQ(getMethodBeingCompiled());
                  }
               else
                  {
                  _compInfo.queueForcedAOTUpgrade(entry, hints, _vm);
                  }
               }
            }
         }

      // Check conditions for adding to JProfiling queue.
      // TODO: How should be AOT loads treated?
      if (_addToJProfilingQueue &&
         entry->_oldStartPC == 0 && startPC != 0)// Must be a first time compilation that succeeded
         {
         // Add request to JProfiling Queue
         TR::IlGeneratorMethodDetails details(method);
         getCompilationInfo()->getJProfilingCompQueue().createCompReqAndQueueIt(details, startPC);
         }


      if (entry->_compErrCode == compilationOK && _vm->isAOT_DEPRECATED_DO_NOT_USE())
         _compInfo._statNumAotedMethods++;
      }

   // compilation success can be detected by checking startPC && startPC != _oldStartPC

   if (TR::Options::getAOTCmdLineOptions()->getOption(TR_EnableAOTRelocationTiming) && entry->isAotLoad())
      {
      PORT_ACCESS_FROM_JITCONFIG(jitConfig);
      UDATA reloTime = j9time_usec_clock() - reloRuntime->reloStartTime();
      // We have the comp monitor, so the add does not run into sync issues
      _compInfo.setAotRelocationTime(_compInfo.getAotRelocationTime() + reloTime);
      }

   // Check to see if we need to print compilation information for perf tool on Linux
   if (TR::Options::getCmdLineOptions()->getOption(TR_PerfTool) && _compiler &&
       startPC != 0 && startPC != entry->_oldStartPC)
      {
      generatePerfToolEntry();
      }

   if (_compiler)
      {
      // Unreserve the code cache used for this compilation
      //
      // NOTE: Although unintuitive, it is possible to reach here without an allocated
      // CodeGenerator.  This can happen during AOT loads.  See the OMR::Compilation
      // constructor for details.
      //
      if (_compiler->cg() && _compiler->cg()->getCodeCache())
         {
         // For JITServer we should wipe this method from the code cache
         if (_compInfo.getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER)
            _compiler->cg()->getCodeCache()->resetCodeCache();
            
         _compiler->cg()->getCodeCache()->unreserve();
         _compiler->cg()->setCodeCache(0);
         }
      // Unreserve the data cache
      TR_DataCache *dataCache = (TR_DataCache*)_compiler->getReservedDataCache();
      if (dataCache)
         {
         // For AOT compilation failures with an error code of compilationAotCacheFullReloFailure
         // that are going to be retried we want to keep the reservation
         if (entry->_tryCompilingAgain && entry->_compErrCode == compilationAotCacheFullReloFailure)
            {
            TR_ASSERT(canDoRelocatableCompile, "Only AOT compilations can fail with compilationAotCacheFullReloFailure. entry=%p\n", entry);
            _reservedDataCache = dataCache;
            }
         else
            {
            TR_DataCacheManager::getManager()->makeDataCacheAvailable(dataCache);
            _compiler->setReservedDataCache(NULL);
            }
         }
      }
   // Safety net for when the compiler object cannot be created and we have
   // a dataCache reservation from the previous compilation.
   // If we don't intend to retry this compilation, release the cached dataCache
   if (_reservedDataCache && !entry->_tryCompilingAgain)
      {
      TR_DataCacheManager::getManager()->makeDataCacheAvailable(_reservedDataCache);
      _reservedDataCache = NULL;
      }

   // The KOT needs to survive at least until we're done committing virtual guards
   //
   if (_compiler && _compiler->getKnownObjectTable())
      _compiler->freeKnownObjectTable();

   if (_compiler && _compiler->getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER)
      {
      _compiler->getOptions()->closeLogFileForClientOptions();
      }

   if (_compiler)
      _compiler->~Compilation();

   setCompilation(NULL);

   _vm = NULL;
   _addToJProfilingQueue = false;

   return startPC;
   }


/**
 * @brief TR::CompilationInfoPerThreadBase::compile
 * @param vmThread Pointer to the current J9VMThread (input)
 * @param entry Pointer to the method to be compiled entry (input)
 * @param scratchSegmentProvider Reference to a J9::J9SegmentProvider (input)
 * @return StartPC of the compiled body, NULL if the compilation failed
 *
 * This is the top level method used to compile a Java method.
 * The compilation thread has VM access when it enters this method though
 * wrappedCompile() may release VM access.
 */
void *
TR::CompilationInfoPerThreadBase::compile(J9VMThread * vmThread,
                                         TR_MethodToBeCompiled *entry,
                                         J9::J9SegmentProvider &scratchSegmentProvider)
   {
   TR_ASSERT(!_compiler, "The previous compilation was not properly cleared.");

      {
      PORT_ACCESS_FROM_JITCONFIG(jitConfig);
      setTimeWhenCompStarted(j9time_usec_clock());
      }

   J9JavaVM *javaVM = vmThread->javaVM;
   TR_MethodMetaData *metaData = NULL;
   void *startPC = NULL;
   const void *aotCachedMethod = NULL;
   J9Method *method = entry->getMethodDetails().getMethod();
   bool canDoRelocatableCompile = false;
   bool eligibleForRelocatableCompile = false;
   bool eligibleForRemoteCompile = false;
   _qszWhenCompStarted = getCompilationInfo()->getMethodQueueSize();

   TR_RelocationRuntime *reloRuntime = NULL;
#if defined(J9VM_INTERP_AOT_RUNTIME_SUPPORT) && defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM))
   TR::CompilationInfoPerThreadBase *cp = _compInfo.getCompInfoForCompOnAppThread();
   reloRuntime = cp ? cp->reloRuntime() : entry->_compInfoPT->reloRuntime();
#endif

   UDATA oldState = vmThread->omrVMThread->vmState;
   vmThread->omrVMThread->vmState = J9VMSTATE_JIT_CODEGEN | 0x0000FFFF;
   vmThread->jitMethodToBeCompiled = method;

   try
      {
      TR::RawAllocator rawAllocator(javaVM);
      J9::SystemSegmentProvider defaultSegmentProvider(
         1 << 16,
         (0 != scratchSegmentProvider.getPreferredSegmentSize()) ? scratchSegmentProvider.getPreferredSegmentSize()
                                                                 : 1 << 24,
         TR::Options::getScratchSpaceLimit(),
         scratchSegmentProvider,
         rawAllocator
         );
      TR::DebugSegmentProvider debugSegmentProvider(
         1 << 16,
         rawAllocator
         );
      TR::SegmentAllocator &regionSegmentProvider =
         TR::Options::getCmdLineOptions()->getOption(TR_EnableScratchMemoryDebugging) ?
            static_cast<TR::SegmentAllocator &>(debugSegmentProvider) :
            static_cast<TR::SegmentAllocator &>(defaultSegmentProvider);
      TR::Region dispatchRegion(regionSegmentProvider, rawAllocator);
      TR_Memory trMemory(*_compInfo.persistentMemory(), dispatchRegion);

      preCompilationTasks(vmThread, javaVM, entry,
                          method, &aotCachedMethod, trMemory,
                          canDoRelocatableCompile, eligibleForRelocatableCompile, 
                          reloRuntime);

      CompileParameters compParam(
         this,
         _vm,
         vmThread,
         reloRuntime,
         entry->_optimizationPlan,
         regionSegmentProvider,
         dispatchRegion,
         trMemory,
         TR::CompileIlGenRequest(entry->getMethodDetails())
         );

   if (TR::Options::getVerboseOption(TR_VerboseCompilationDispatch))
      TR_VerboseLog::writeLineLocked(TR_Vlog_DISPATCH,
         "Compilation thread executing compile(): j9method=%p isAotLoad=%d canDoRelocatableCompile=%d eligibleForRelocatableCompile=%d isRemoteCompReq=%d _doNotUseAotCodeFromSharedCache=%d AOTfe=%d isDLT=%d",
          method, entry->isAotLoad(), canDoRelocatableCompile, eligibleForRelocatableCompile, entry->isRemoteCompReq(), entry->_doNotUseAotCodeFromSharedCache, _vm->isAOT_DEPRECATED_DO_NOT_USE(), entry->isDLTCompile());

      if (
          (TR::Options::canJITCompile()
           || canDoRelocatableCompile
           || entry->isAotLoad()
           || entry->isRemoteCompReq()
          )
#if defined(TR_HOST_ARM)
          && !TR::Options::getCmdLineOptions()->getOption(TR_FullSpeedDebug)
#endif
         )
         {
         // Compile the method
         //

         PORT_ACCESS_FROM_JITCONFIG(jitConfig);
         _compInfo.debugPrint("\tcompiling method", entry->getMethodDetails(), vmThread);

#if defined(J9VM_PORT_SIGNAL_SUPPORT)
         U_32 flags = J9PORT_SIG_FLAG_MAY_RETURN |
                      J9PORT_SIG_FLAG_SIGSEGV | J9PORT_SIG_FLAG_SIGFPE |
                      J9PORT_SIG_FLAG_SIGILL  | J9PORT_SIG_FLAG_SIGBUS;

         static char *noSignalWrapper = feGetEnv("TR_NoSignalWrapper");

         if (!noSignalWrapper && j9sig_can_protect(flags))
            {
            UDATA result = 0;

            // Attempt to request a compilation, while guarding against any crashes that occur
            // inside the compile request. For log compilations, simply return.
            //
            UDATA protectedResult;
            if (entry->getMethodDetails().isDumpMethod())
               {
               // NOTE:
               //       for intentional crashes, intentional traps cause the dump, and we
               //       are not protected against them (flag: J9PORT_SIG_FLAG_SIGTRAP)
               protectedResult = j9sig_protect((j9sig_protected_fn)wrappedCompile, static_cast<void *>(&compParam),
                                                  (j9sig_handler_fn)  blankDumpSignalHandler, vmThread,
                                                  flags, &result);
               }
            else
               {
               protectedResult = j9sig_protect((j9sig_protected_fn)wrappedCompile, static_cast<void*>(&compParam),
                                                  (j9sig_handler_fn)  jitSignalHandler, vmThread,
                                                  flags, &result);
               }

            if (protectedResult == 0) // successful completion of wrappedCompile
               metaData = reinterpret_cast<TR_MethodMetaData *>(result);
            else
               metaData = 0;
            }
         else
#endif
            metaData = wrappedCompile(privatePortLibrary, &compParam);
         }
      else
         {
         entry->_compErrCode = compilationRestrictedMethod;
         }

      // This method has to be called from within the try block,
      // otherwise, the TR_Memory, TR::Region, and TR::SegmentAllocator
      // objects go out of scope and get destroyed.
      startPC = postCompilationTasks(vmThread, entry, method,
                                     aotCachedMethod, metaData,
                                     canDoRelocatableCompile, eligibleForRelocatableCompile,
                                     reloRuntime);
      }
   catch (const std::exception &e)
      {
      entry->_compErrCode = compilationFailure;

      if (TR::Options::isAnyVerboseOptionSet(TR_VerboseCompileEnd, TR_VerboseCompFailure, TR_VerbosePerformance))
         {
         try
            {
            throw;
            }
         catch (const J9::JITShutdown)
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_FAILURE,"<EARLY TRANSLATION FAILURE: JIT Shutdown signaled>");
            }
         catch (const std::bad_alloc &e)
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_FAILURE,"<EARLY TRANSLATION FAILURE: out of scratch memory>");
            }
         catch (const std::exception &e)
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_FAILURE,"<EARLY TRANSLATION FAILURE: compilation aborted>");
            }
         }

      Trc_JIT_outOfMemory(vmThread);

      if (getCompilation() && getCompilation()->getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER)
         {
         getCompilation()->getOptions()->closeLogFileForClientOptions();
         }

      if (getCompilation())
         {
         getCompilation()->~Compilation();
         }
      setCompilation(NULL);

      // This method has to be called from within the catch block,
      // since moving it outside would result in it getting invoked
      // twice on a successful compilation.
      startPC = postCompilationTasks(vmThread, entry, method,
                                     aotCachedMethod, metaData,
                                     canDoRelocatableCompile, eligibleForRelocatableCompile,
                                     reloRuntime);
      }


   vmThread->omrVMThread->vmState = oldState;
   vmThread->jitMethodToBeCompiled = NULL;

   return startPC;
   }

// static method
TR_MethodMetaData *
TR::CompilationInfoPerThreadBase::wrappedCompile(J9PortLibrary *portLib, void * opaqueParameters)
   {
   CompileParameters * p = static_cast<CompileParameters *>(opaqueParameters);
   TR::Compilation     * volatile compiler = 0;
   TR::Options         *options  = 0;
   TR_ResolvedMethod  *compilee = 0;

   TR::CompilationInfoPerThreadBase *that = p->_compilationInfo; // static method, no this
   TR_J9VMBase        *vm   = p->_vm;
   J9VMThread         *vmThread = p->_vmThread;

   TR_RelocationRuntime *reloRuntime = p->_reloRuntime;

   J9JITConfig *jitConfig = that->_jitConfig;
   bool reducedWarm = false;

   TR::SegmentAllocator &scratchSegmentProvider = p->_scratchSegmentProvider;

   // cleanup the compilationShouldBeInterrupted flag.
   that->setCompilationShouldBeInterrupted(0);

   if (that->_methodBeingCompiled->isDLTCompile())
      {
      TR_J9SharedCache *sc = (TR_J9SharedCache *) (vm->sharedCache());
      if (sc)
        sc->addHint(that->_methodBeingCompiled->getMethodDetails().getMethod(), TR_HintDLT);
      }

   that->setMetadata(NULL);
   try
      {
      InterruptibleOperation generatingCompilationObject(*that);
      TR::IlGeneratorMethodDetails & details = that->_methodBeingCompiled->getMethodDetails();
      TR_OpaqueMethodBlock *method = (TR_OpaqueMethodBlock *) details.getMethod();

      // Create the compilee
      if (details.isMethodHandleThunk())
         {
         J9::MethodHandleThunkDetails &mhDetails = static_cast<J9::MethodHandleThunkDetails &>(details);
         compilee = vm->createMethodHandleArchetypeSpecimen(p->trMemory(), mhDetails.getHandleRef());
         TR_ASSERT(compilee, "Cannot queue a thunk compilation for a MethodHandle without a suitable archetype");
         }
      else if (details.isNewInstanceThunk())
         {
         J9::NewInstanceThunkDetails &niDetails = static_cast<J9::NewInstanceThunkDetails &>(details);
         compilee = vm->createResolvedMethod(p->trMemory(), method, NULL, (TR_OpaqueClassBlock *) niDetails.classNeedingThunk());
         }
      else
         {
         compilee = vm->createResolvedMethod(p->trMemory(), method);
         }

      if (that->_methodBeingCompiled->_optimizationPlan->isUpgradeRecompilation())
         {
         //TR_ASSERT(that->_methodBeingCompiled->_oldStartPC, "upgrade recompilations must have some oldstartpc");
         TR_PersistentJittedBodyInfo *bodyInfo = ((TR_ResolvedJ9Method*)compilee)->getExistingJittedBodyInfo();
         if (bodyInfo->getIsAotedBody() || bodyInfo->getHotness() <= cold)
            {
            TR_J9SharedCache *sc = (TR_J9SharedCache *) (vm->sharedCache());
            if (sc)
               sc->addHint(that->_methodBeingCompiled->getMethodDetails().getMethod(), TR_HintUpgrade);
            }
         }

      // See if this method can be compiled and check it against the method
      // filters to see if compilation is to be suppressed.

      TR_FilterBST *filterInfo = NULL;
      // JITServer: methodCanBeCompiled check should have been done on the client, skip it on the server.
      if (!details.isRemoteMethod() && !that->methodCanBeCompiled(p->trMemory(), vm, compilee, filterInfo))
         {
         that->_methodBeingCompiled->_compErrCode = compilationRestrictedMethod;

         TR::Options *options = TR::Options::getJITCmdLineOptions();
         if (vm->isAOT_DEPRECATED_DO_NOT_USE())
            options = TR::Options::getAOTCmdLineOptions();
         if (options->getVerboseOption(TR_VerboseCompileExclude))
            {
            TR_VerboseLog::vlogAcquire();
            if (jitConfig->runtimeFlags & J9JIT_TESTMODE)
               TR_VerboseLog::write("\n<JIT: translating %s -- CANNOT BE TRANSLATED>\n", compilee->signature(p->trMemory()));
            else
               TR_VerboseLog::writeLine(TR_Vlog_COMPFAIL,"%s cannot be translated", compilee->signature(p->trMemory()));
            TR_VerboseLog::vlogRelease();
            }
         Trc_JIT_noAttemptToJit(vmThread, compilee->signature(p->trMemory()));

         compilee = 0;
         }
      else if (jitConfig->runtimeFlags & (J9JIT_CODE_CACHE_FULL | J9JIT_DATA_CACHE_FULL))
         {
         // Optimization to disable future first time compilations from reaching the queue
         that->getCompilationInfo()->getPersistentInfo()->setDisableFurtherCompilation(true);

         if (TR::Options::isAnyVerboseOptionSet(TR_VerboseCompileEnd, TR_VerboseCompFailure))
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_INFO,"<WARNING: JIT CACHES FULL> Disable further compilation");
            }
         if (jitConfig->runtimeFlags & J9JIT_CODE_CACHE_FULL)
            Trc_JIT_cacheFull(vmThread);
         if (jitConfig->runtimeFlags & J9JIT_DATA_CACHE_FULL)
            Trc_JIT_dataCacheFull(vmThread);
         that->_methodBeingCompiled->_compErrCode = compilationExcessiveSize;
         compilee = 0;
         }
      else
         {
         int32_t optionSetIndex = filterInfo ? filterInfo->getOptionSet() : 0;
         int32_t lineNumber = filterInfo ? filterInfo->getLineNumber() : 0;

         TR_ASSERT(p->_optimizationPlan, "Must have an optimization plan");

         TR::CompilationInfo *compInfo = &that->_compInfo;
         // If the options come from a remote party, skip the setup options process
         if (that->_methodBeingCompiled->_clientOptions != NULL)
            {
            TR_ASSERT(that->_methodBeingCompiled->isOutOfProcessCompReq(), "Options are already provided only for JITServer");
            options = TR::Options::unpackOptions(that->_methodBeingCompiled->_clientOptions, that->_methodBeingCompiled->_clientOptionsSize, that, vm, p->trMemory());
            options->setLogFileForClientOptions();
            }
         else
            {
            // JITServer: we want to suppress log file for client mode
            // Client will get the log files from server.
            if (that->_methodBeingCompiled->isRemoteCompReq())
               {
               TR::Options::suppressLogFileBecauseDebugObjectNotCreated();
               }

            bool aotCompilationReUpgradedToWarm = false;
            if (that->_methodBeingCompiled->_useAotCompilation && !that->_methodBeingCompiled->isOutOfProcessCompReq())
               {
               // In some circumstances AOT compilations are performed at warm
               if ((TR::Options::getCmdLineOptions()->getAggressivityLevel() == TR::Options::AGGRESSIVE_AOT ||
                  that->getCompilationInfo()->importantMethodForStartup((J9Method*)method) ||
                  (!TR::Compiler->target.cpu.isPower() && // Temporary change until we figure out the AOT bug on PPC
                   !TR::Options::getAOTCmdLineOptions()->getOption(TR_DisableAotAtCheapWarm))) &&
                  p->_optimizationPlan->isOptLevelDowngraded() &&
                  p->_optimizationPlan->getOptLevel() == cold) // Is this test really needed?
                  {
                  p->_optimizationPlan->setOptLevel(warm);
                  p->_optimizationPlan->setOptLevelDowngraded(false);
                  aotCompilationReUpgradedToWarm = true;
                  }
               }

            // Set up options for this compilation. An option subset might apply
            // to the method, either via an option set index in the limitfile or
            // via a regular expression that matches the method.
            //
            options = new (p->trMemory(), heapAlloc) TR::Options(
                  p->trMemory(),
                  optionSetIndex,
                  lineNumber,
                  compilee,
                  that->_methodBeingCompiled->_oldStartPC,
                  p->_optimizationPlan,
                  (vm->isAOT_DEPRECATED_DO_NOT_USE() || that->_methodBeingCompiled->isAotLoad()),
                  that->getCompThreadId());

            // JITaaS TODO determine if we care to support annotations
            if (that->_methodBeingCompiled->isRemoteCompReq())
               options->setOption(TR_EnableAnnotations,false);

            // Determine if known annotations exist and if so, keep annotations enabled
            if (!that->_methodBeingCompiled->isAotLoad() && !vm->isAOT_DEPRECATED_DO_NOT_USE() && options->getOption(TR_EnableAnnotations))
               {
               if (!TR_AnnotationBase::scanForKnownAnnotationsAndRecord(&that->_compInfo, details.getMethod(), vmThread->javaVM, vm))
                  options->setOption(TR_EnableAnnotations,false);
               }

            if (vm->canUseSymbolValidationManager() && options->getOption(TR_EnableSymbolValidationManager))
               {
               options->setOption(TR_UseSymbolValidationManager);
               options->setOption(TR_DisableKnownObjectTable);
               }
            else if (!vm->canUseSymbolValidationManager())
               {
               // disable SVM in case it was enabled explicitly with -Xjit:useSymbolValidationManager
               options->setOption(TR_UseSymbolValidationManager, false);
               }

            // Set jitDump specific options
            TR::CompilationInfoPerThread *threadCompInfo = compInfo ? compInfo->getCompInfoForThread(vmThread) : NULL;
            if (threadCompInfo &&
                threadCompInfo->isDiagnosticThread() &&
                options->getDebug())
               {
               // Trace All
               options->setOption(TR_TraceAll);

               // Trace crashing optimization
               UDATA state = compInfo->getVMStateOfCrashedThread();
               uint32_t index = (state >> 16) & 0xFF;

               if ((isValidVmStateIndex(index)) &&
                   (index == ((J9VMSTATE_JIT_CODEGEN>>16) & 0xF)))
                  {
                  OMR::Optimizations opt = (OMR::Optimizations)((state >> 8) & 0xFF);
                  if (0 < opt && opt < OMR::numOpts)
                     {
                     // Trace the optimization that the crash happened in
                     options->enableTracing(opt);
                     }
                  }
               }

            // Adjust Options for AOT compilation
            if (vm->isAOT_DEPRECATED_DO_NOT_USE())
               {
               options->setOption(TR_AOT);

               // Disable dynamic literal pool for AOT because of an unresolved data snippet patching issue in which
               // the "Address Of Ref. Instruction" in the unresolved data snippet points to the wrong load instruction
               options->setOption(TR_DisableOnDemandLiteralPoolRegister);

               options->setOption(TR_DisableIPA);
               options->setOption(TR_DisableEDO);
               options->setDisabled(OMR::invariantArgumentPreexistence, true);
               options->setOption(TR_DisableHierarchyInlining);
               if (options->getInitialBCount() == 0 || options->getInitialCount() == 0)
                  options->setOption(TR_DisableDelayRelocationForAOTCompilations, true);

               // Perform less inlining if we artificially upgraded this AOT compilation to warm
               if (aotCompilationReUpgradedToWarm)
                  options->setInlinerOptionsForAggressiveAOT();

               TR_ASSERT(vm->isAOT_DEPRECATED_DO_NOT_USE(), "assertion failure");

               // Do not delay relocations for JITServer
               if (that->getCompilationInfo()->getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER)
                  options->setOption(TR_DisableDelayRelocationForAOTCompilations);
               }

            if (that->_methodBeingCompiled->_optimizationPlan->disableCHOpts())
               options->disableCHOpts();

            if (that->_methodBeingCompiled->_optimizationPlan->disableGCR())
               options->setOption(TR_DisableGuardedCountingRecompilations);

            if (options->getOption(TR_DisablePrexistenceDuringGracePeriod))
               {
               if (that->getCompilationInfo()->getPersistentInfo()->getElapsedTime() < that->getCompilationInfo()->getPersistentInfo()->getClassLoadingPhaseGracePeriod())
                  options->setDisabled(OMR::invariantArgumentPreexistence, true);
               }

            // RI Based Reduced Warm Compilation
            if (p->_optimizationPlan->isHwpDoReducedWarm())
               {
               options->setLocalAggressiveAOT();
               }

            // The following tweaks only apply for java compilations
            if (!that->_methodBeingCompiled->isAotLoad()) // exclude AOT loads
               {
               J9Method *method = details.getMethod();
               // See if we need to profile first level compilations and if we can do it, change the optimization plan
               //
               if(options->getOption(TR_FirstLevelProfiling) &&
                  !that->_methodBeingCompiled->isDLTCompile() && // filter out DLTs
                  !that->_methodBeingCompiled->isJNINative() &&
                  !that->_methodBeingCompiled->getMethodDetails().isNewInstanceThunk() &&
                  that->_methodBeingCompiled->_oldStartPC == 0 && // first time compilations
                  TR::CompilationController::getCompilationStrategy()->enableSwitchToProfiling() &&
                  options->canJITCompile() &&
                  !options->getOption(TR_DisableProfiling) &&
                  !options->getOption(TR_NoRecompile) &&
                  options->allowRecompilation() &&              // don't do it for fixed opt level
                  p->_optimizationPlan->isOptLevelDowngraded()) // only for classLoadPhase
                  // should we do it for bootstrap classes?
                  {
                  p->_optimizationPlan->setInsertInstrumentation(true);
                  p->_optimizationPlan->setUseSampling(false);
                  options->setOption(TR_QuickProfile); // to reduce the frequency/count for profiling to 100/2
                  }

               // Check if user allows us to do samplingJProfiling.
               // If so, enable it programmatically on a method by method basis
               //
               if (!options->getOption(TR_DisableSamplingJProfiling))
                  {
                  // Check other preconditions
                  if (!TR::CompilationInfo::isCompiled((J9Method*)method) &&
                     // GCR is needed for moving away from profiling
                     !options->getOption(TR_DisableGuardedCountingRecompilations) &&
                     // recompilation must be allowed to move away from profiling
                     options->allowRecompilation() && !options->getOption(TR_NoRecompile) &&
                     // exclude newInstance, methodHandle
                     (details.isOrdinaryMethod() || (details.isMethodInProgress() && options->getOption(TR_UseSamplingJProfilingForDLT))) &&
                     // exclude natives
                     !TR::CompilationInfo::isJNINative((J9Method*)method))
                     // TODO: should we prevent SamplingJProfiling for AOT bodies?
                     {
                     // Check which heuristic is enabled
                     if (options->getOption(TR_UseSamplingJProfilingForAllFirstTimeComps))
                        {
                        // Enable SamplingJprofiling
                        options->setDisabled(OMR::samplingJProfiling, false);
                        }
                     if (options->getOption(TR_UseSamplingJProfilingForLPQ) &&
                        that->_methodBeingCompiled->_reqFromSecondaryQueue)
                        {
                        // Enable SamplingJProfiling
                        options->setDisabled(OMR::samplingJProfiling, false);
                        // May want adjust the GCR count;
                        // TODO: Estimate the initial invocation count and subtract the current
                        // invocation count; this is how many invocations I am missing.
                        // Complication: interpreter sampling may have decremented the invocation count further.
                        options->setGCRCount(500); // add 500 more invocations
                        }
                     if (options->getOption(TR_UseSamplingJProfilingForDLT) &&
                        (details.isMethodInProgress() || p->_optimizationPlan->isInducedByDLT()))
                        {
                        // Enable SamplingJProfiling
                        options->setDisabled(OMR::samplingJProfiling, false);
                        }
                     if (options->getOption(TR_UseSamplingJProfilingForInterpSampledMethods))
                        {
                        int32_t skippedCount = that->getCompilationInfo()->getInterpSamplTrackingInfo()->findAndDelete(method);
                        if (skippedCount > 0)
                           {
                           // Enable SamplingJProfiling
                           options->setDisabled(OMR::samplingJProfiling, false);
                           // Determine the count
                           // Determine entry weight
                           J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
                           if (J9ROMMETHOD_HAS_BACKWARDS_BRANCHES(romMethod))
                              {
                              // Don't set a count too high for loopy methods
                              options->setGCRCount(std::min(250, skippedCount));
                              }
                           else
                              {
                              options->setGCRCount(std::min(1000, skippedCount));
                              }
                           //fprintf(stderr, "skipped count=%d\n", skippedCount);
                           }
                        }

                     // When using SamplingJProfiling downgrade to cold to avoid overhead
                     //
                     if (!options->isDisabled(OMR::samplingJProfiling) &&
                        // Check whether we are allowed to downgrade
                        !options->getOption(TR_DontDowngradeToCold))
                        {
                        p->_optimizationPlan->setOptLevel(cold);
                        p->_optimizationPlan->setDowngradedDueToSamplingJProfiling(true);
                        options->setOptLevel(cold);
                        // TODO: should we disable sampling to prevent upgrades before the method collects enough profiling info?
                        }
                     }
                  }

               bool doJProfile = false;
               if (that->_methodBeingCompiled->_reqFromJProfilingQueue)
                  {
                  doJProfile = true;
                  if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerbosePerformance))
                     TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, "t=%6u Processing req from JPQ", (uint32_t)that->getCompilationInfo()->getPersistentInfo()->getElapsedTime());
                  }
               else
                  {
                  // Is this request a candidate for JProfiling?
                  if (TR_JProfilingQueue::isJProfilingCandidate(that->_methodBeingCompiled, options, vm))
                     {
                     static char *disableFilterOnJProfiling = feGetEnv("TR_DisableFilterOnJProfiling");
                     // Apply the filter based on time
                     if (disableFilterOnJProfiling)
                        {
                        doJProfile = true;
                        }
                     else if (jitConfig->javaVM->phase != J9VM_PHASE_NOT_STARTUP ||
                        that->getCompilationInfo()->getPersistentInfo()->getJitState() == STARTUP_STATE)
                        {
                        // We want to JProfile this method, but maybe not just now
                        if (that->getCompilationInfo()->canProcessJProfilingRequest())
                           {
                           doJProfile = true;
                           }
                        else
                           {
                           that->_addToJProfilingQueue = true;
                           // Since we are going to recompile this method based on
                           // the JProfiling queue, disable any GCR recompilation
                           options->setOption(TR_DisableGuardedCountingRecompilations);
                           }
                        }
                     }
                  }

               // JProfiling may be enabled if TR_EnableJProfilingInProfilingCompilations is set and its a profiling compilation.
               // See optimizer/JProfilingBlock.cpp
               if (!doJProfile)
                  {
                  options->setOption(TR_EnableJProfiling, false);
                  }
               else // JProfiling bodies should not use GCR trees
                  {
                  options->setOption(TR_DisableGuardedCountingRecompilations);
                  }

               if (that->_methodBeingCompiled->_oldStartPC != 0)
                  {
                  TR_PersistentJittedBodyInfo *bodyInfo = TR::Recompilation::getJittedBodyInfoFromPC(that->_methodBeingCompiled->_oldStartPC);
                  if (bodyInfo)
                     {
                     TR_PersistentMethodInfo *methodInfo = bodyInfo->getMethodInfo();
                     if (methodInfo->getReasonForRecompilation() == TR_PersistentMethodInfo::RecompDueToInlinedMethodRedefinition)
                        methodInfo->incrementNumberOfInlinedMethodRedefinition();
                     if (methodInfo->getNumberOfInlinedMethodRedefinition() >= 2)
                        options->setOption(TR_DisableNextGenHCR);
                     }
                  }

               // Strategy tweaks during STARTUP and IDLE
               //
               if (jitConfig->javaVM->phase != J9VM_PHASE_NOT_STARTUP ||
                  that->getCompilationInfo()->getPersistentInfo()->getJitState() == IDLE_STATE)
                  {
                  // Disable idiomRecognition during startup of -Xquickstart runs to save memory
                  if (TR::Options::isQuickstartDetected())
                     options->setDisabled(OMR::idiomRecognition, true);

                  if (options->getOptLevel() < warm)
                     {
                     if (!vm->isAOT_DEPRECATED_DO_NOT_USE())
                        {
                        // Adjust DumbInliner cutoff parameter as to make it more conservative in constrained situations
                        // For AOT we can be more aggressive because the cost is payed only during first run
                        options->setDumbInlinerBytecodeSizeCutoff(that->getCompilationInfo()->computeDynamicDumbInlinerBytecodeSizeCutoff(options));
                        // Disable rematerialization to cut on compilation costs
                        if (!options->getOption(TR_DisableJava8StartupHeuristics))
                           options->setDisabled(OMR::rematerialization, true);
                        }
                     // Increase the trivial inliner max size for 'important methods' (could be bootstrap methods)
                     // We could filter by AOT only, or quickstart only
                     //if (that->getCompilationInfo()->importantMethodForStartup(method))
                     //   options->setTrivialInlinerMaxSize(40);
                     }

                  // Disable NextGenHCR during Startup Phase
                  if (jitConfig->javaVM->phase != J9VM_PHASE_NOT_STARTUP)
                     {
                     options->setOption(TR_DisableNextGenHCR);
                     }

                  // Do not allow switching to profiling if this is a big app
                  //
                  if (that->getCompilationInfo()->getPersistentInfo()->getNumLoadedClasses() >= TR::Options::_bigAppThreshold)
                     p->_optimizationPlan->setDoNotSwitchToProfiling(true);

                  // Disable optServer for some classes of compilations during STARTUP and IDLE
                  //
                  if (!options->getOption(TR_NoOptServer) && !options->getOption(TR_DisableSelectiveNoOptServer) && !options->getOption(TR_Server))
                     {
                     if (that->_methodBeingCompiled->_oldStartPC == 0) // first time compilations
                        {
                        // sync requests during startup in an asynchronous environment
                        if (that->_methodBeingCompiled->_priority >= CP_SYNC_MIN &&
                           that->getCompilationInfo()->asynchronousCompilation())
                           {
                           options->setOption(TR_NoOptServer);
                           reducedWarm = true;
                           }
                        // all first time compilations during startup
                        else if (jitConfig->javaVM->phase != J9VM_PHASE_NOT_STARTUP &&
                                 details.isOrdinaryMethod() &&
                                 !options->getOption(TR_DisableNoServerDuringStartup))
                           {
                           options->setOption(TR_NoOptServer);
                           reducedWarm = true;
                           // These guys should be compiled with GCR hooks so that we get the throughput back
                           options->setInsertGCRTrees();
                           }

                        }
                     else // recompilation requests
                        {
                        // Upgrades from cold need to be cheaper in startup or idle mode
                        if (p->_optimizationPlan->isUpgradeRecompilation())
                           {
                           options->setOption(TR_NoOptServer);
                           }
                        else // recompilations triggered through GCR need to be cheaper
                           {
                           // Note that we may have a warm compilation with NoOptServer that has embedded
                           // GCR trees to recompile without NoOptServer (thus better generated code)
                           // We want to make sure that the recompilation uses server mode in that case
                           //
                           TR_PersistentJittedBodyInfo *bodyInfo = TR::Recompilation::getJittedBodyInfoFromPC(that->_methodBeingCompiled->_oldStartPC);
                           if (bodyInfo->getMethodInfo()->getReasonForRecompilation() == TR_PersistentMethodInfo::RecompDueToGCR)
                              {
                              if (bodyInfo->getHotness() < options->getOptLevel()) // prevent warm+NoOptServer --> warm+NoOptServer transitions
                                 {
                                 options->setOption(TR_NoOptServer);
                                 }
                              }
                           }
                        }
                     }
                  } // Strategy tweaks during STARTUP and IDLE
               else
                  {
                  // Tweak inlining aggressiveness based on time. Only for non-AOT warm compilations and only in Xtune:virtualized mode.
                  if (options->getOption(TR_VaryInlinerAggressivenessWithTime))
                     {
                     int32_t inlAggr = that->getCompilationInfo()->getPersistentInfo()->getInliningAggressiveness();
                     if (inlAggr != 100 && options->getOptLevel() == warm && !vm->isAOT_DEPRECATED_DO_NOT_USE() &&
                        TR::Options::getCmdLineOptions()->getAggressivityLevel() == TR::Options::AGGRESSIVE_AOT)
                        {
                        options->setInlinerCGBorderFrequency(9800 - 8 * inlAggr);
                        options->setInlinerCGColdBorderFrequency(7500 - 25 * inlAggr);
                        options->setInlinerCGVeryColdBorderFrequency(5000 - 35 * inlAggr);
                        options->setInlinerBorderFrequency(9000 - 30 * inlAggr);
                        options->setInlinerVeryColdBorderFrequency(5500 - 40 * inlAggr);
                        if (inlAggr < 25)
                           options->setOption(TR_NoOptServer);
                        }
                     }
                  }
               // Do not try any GRA savings at hot and above or if AOT
               if (options->getOption(TR_EnableGRACostBenefitModel))
                  {
                  if (options->getOptLevel() > warm || vm->isAOT_DEPRECATED_DO_NOT_USE())
                     options->setOption(TR_EnableGRACostBenefitModel, false);
                  }

               // Disable AOT w/ SVM during startup
               if (jitConfig->javaVM->phase != J9VM_PHASE_NOT_STARTUP)
                  {
                  static char *dontDisableSVMDuringStartup = feGetEnv("TR_DontDisableSVMDuringStartup");
                  if (!dontDisableSVMDuringStartup)
                     options->setOption(TR_UseSymbolValidationManager, false);
                  }

               // See if we need to inset GCR trees
               if (!details.supportsInvalidation())
                  {
                  options->setOption(TR_DisableGuardedCountingRecompilations);
                  }
               else if (vm->isAOT_DEPRECATED_DO_NOT_USE() || (options->getOptLevel() < warm &&
                  !(that->_methodBeingCompiled->_jitStateWhenQueued == IDLE_STATE && that->getCompilationInfo()->getPersistentInfo()->getJitState() == IDLE_STATE)))
                  {
                  options->setInsertGCRTrees(); // This is a recommendation not a directive
                  }
               // Disable some expensive optimizations
               if (options->getOptLevel() <= warm && !options->getOption(TR_EnableExpensiveOptsAtWarm))
                  {
                  options->setOption(TR_DisableStoreSinking);
                  }
               } // end of compilation strategy tweaks for Java


            // If we are at the last retrial and the automatic logging feature is turned on
            // set TR_TraceAll options to generate a full log file for this compilation
            // (just in case it fails again). Note that the log file must be specified.
            //
            if (options->getOption(TR_EnableLastCompilationRetrialLogging) &&
                (that->_methodBeingCompiled->_compilationAttemptsLeft == 1))
               {
               if (options->getLogFile() != NULL)
                  options->setOption(TR_TraceAll);
               }

            TR_ASSERT(TR::comp() == NULL, "there seems to be a current TLS TR::Compilation object %p for this thread. At this point there should be no current TR::Compilation object", TR::comp());

            }
         compiler = new (p->trMemory(), heapAlloc) TR::Compilation(
               that->getCompThreadId(),
               vmThread,
               vm,
               compilee,
               p->_ilGenRequest,
               *options,
               p->_dispatchRegion,
               p->trMemory(),
               p->_optimizationPlan,
               reloRuntime);

         // JITaaS TODO: put info in optPlan so that compilation constructor can do this
         if (that->_methodBeingCompiled->isRemoteCompReq())
            compiler->setRemoteCompilation();
         else if (that->_methodBeingCompiled->isOutOfProcessCompReq())
            compiler->setOutOfProcessCompilation();

         p->trMemory()->setCompilation(compiler);
         that->setCompilation(compiler);

         TR_ASSERT(TR::comp() == compiler, "the TLS TR::Compilation object %p for this thread does not match the one %p just created.", TR::comp(), compiler);

#ifdef MCT_DEBUG
         fprintf(stderr, "Created new compiler %p ID=%d\n", compiler, compiler->getCompThreadID());
#endif
         if (compiler)
            {
            uint64_t proposedScratchMemoryLimit = (uint64_t)TR::Options::getScratchSpaceLimit();

            bool isJSR292 = TR::CompilationInfo::isJSR292(details.getRomMethod());

            // Check if the the method to be compiled is a JSR292 method
            if (isJSR292)
               {
               /* Set options */
               compiler->getOptions()->setOption(TR_Server);
               compiler->getOptions()->setOption(TR_ProcessHugeMethods);

               // Try to increase scratch space limit for JSR292 compilations
               proposedScratchMemoryLimit *= TR::Options::getScratchSpaceFactorWhenJSR292Workload();
               }

            // Check if the method to be compiled is a Thunk Archetype
            if (details.isMethodHandleThunk())
               {
               compiler->getOptions()->setAllowRecompilation(false);
               options->setOption(TR_DisableOSR);
               options->setOption(TR_EnableOSR, false);
               }

            // Check to see if there is sufficient physical memory available for this compilation
            if (compiler->getOption(TR_EnableSelfTuningScratchMemoryUsageBeforeCompile))
               {
               bool incompleteInfo = false;
               // Abort the compile if we don't have at least getScratchSpaceLowerBound()
               // available, plus some safe reserve
               // TODO: we may want to use a lower value for third parameter below if the
               // compilation is deemed cheap (JNI, thunks, cold small method)
               uint64_t physicalLimit = compInfo->computeFreePhysicalLimitAndAbortCompilationIfLow(compiler,
                                                                                                   incompleteInfo,
                                                                                                   TR::Options::getScratchSpaceLowerBound());
               // If we were able to get the memory information
               if (physicalLimit != OMRPORT_MEMINFO_NOT_AVAILABLE)
                  {
                  // If the proposed scratch space limit is greater than the available
                  // physical memory, we need to lower the scratch space limit
                  if (proposedScratchMemoryLimit > physicalLimit)
                     {
                     if (incompleteInfo)
                        {
                        // If we weren't able to get all the memory information
                        // only lower the limit for JSR292 compilations,
                        // but not beyond the default value for scratch memory
                        if (isJSR292)
                           {
                           proposedScratchMemoryLimit = (physicalLimit >= scratchSegmentProvider.allocationLimit()
                                                         ? physicalLimit
                                                         : scratchSegmentProvider.allocationLimit());
                           }
                        }
                     else // We have complete memory information
                        {
                        proposedScratchMemoryLimit = physicalLimit;
                        }
                     }
                  }
               }

            // Cap the limit for JIT to 4GB
            size_t proposedCapped = proposedScratchMemoryLimit > UINT_MAX ? UINT_MAX : (size_t)proposedScratchMemoryLimit;
            scratchSegmentProvider.setAllocationLimit(proposedCapped);
            }

         if (debug("traceInfo") && optionSetIndex > 0)
            {
            if (compiler->getOutFile() != NULL)
               diagnostic("Forced Option Set %d\n", optionSetIndex);
            }
         } // else
      }
   catch (const std::exception &e)
      {
      // TODO: we must handle OOM cases when we abort the compilation right from the start.
      // Or eliminate the code that throws (or the code could also look at how the expensive the compilation is)
      that->_methodBeingCompiled->_compErrCode = compilationFailure;

      if (TR::Options::isAnyVerboseOptionSet(TR_VerboseCompileEnd, TR_VerboseCompFailure, TR_VerbosePerformance))
         {
         try
            {
            throw;
            }
         catch (const J9::JITShutdown)
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_FAILURE,"<EARLY TRANSLATION FAILURE: JIT Shutdown signaled>");
            }
         catch (const std::bad_alloc &e)
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_FAILURE,"<EARLY TRANSLATION FAILURE: out of scratch memory>");
            }
         catch (const std::exception &e)
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_FAILURE,"<EARLY TRANSLATION FAILURE: compilation aborted>");
            }
         }

      Trc_JIT_outOfMemory(vmThread);

      if (compiler && compiler->getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER)
         {
         compiler->getOptions()->closeLogFileForClientOptions();
         }

      if (compiler)
         {
         compiler->~Compilation();
         }

      compiler = NULL;
      p->trMemory()->setCompilation(NULL);
      that->setCompilation(NULL);
      }

   TR_MethodMetaData * metaData = NULL;

   if (compiler)
      {
      if (compiler->getRecompilationInfo())
         {
         // if the compiler is created successfully and the method supports recompilation, we can set this field on the bodyInfo
         TR_PersistentJittedBodyInfo *bodyInfo = compiler->getRecompilationInfo()->getJittedBodyInfo();
         if (bodyInfo)
            {
            bodyInfo->setStartPCAfterPreviousCompile(that->_methodBeingCompiled->_oldStartPC);
            if (reducedWarm && options->getOptLevel() == warm)
               bodyInfo->setReducedWarm();
            }
         }

      const char *hotnessString = compiler->getHotnessName(compiler->getMethodHotness());

      Trc_JIT_compileStart(vmThread, hotnessString, compiler->signature());

      TR_ASSERT(compiler->getMethodHotness() != unknownHotness, "Trying to compile at unknown hotness level");

      metaData = that->compile(vmThread, compiler, compilee, *vm, p->_optimizationPlan, scratchSegmentProvider);

      }
#ifndef PUBLIC_BUILD
   // Store hints in the SCC
   TR_J9SharedCache *sc = (TR_J9SharedCache *) (vm->sharedCache());
   if (metaData  && !that->_methodBeingCompiled->isDLTCompile() &&
      !that->_methodBeingCompiled->isAotLoad() && sc) // skip AOT loads
      {
      J9Method *method = that->_methodBeingCompiled->getMethodDetails().getMethod();
      TR_J9VMBase *fej9 = (TR_J9VMBase *)(compiler->fej9());
      if (!fej9->isAOT_DEPRECATED_DO_NOT_USE())
         {
         bool isEDOCompilation = false;
         TR_CatchBlockProfileInfo * profileInfo = TR_CatchBlockProfileInfo::get(compiler);
         if (profileInfo && profileInfo->getCatchCounter() >= TR_CatchBlockProfileInfo::EDOThreshold)
            {
            isEDOCompilation = true;
            sc->addHint(method, TR_HintEDO);
            }

         // There is the possibility that a hot/scorching compilation happened outside
         // startup and with hints we move this expensive compilation during startup
         // thus affecting startup time
         // To minimize risk, add hot/scorching hints only if we are in startup mode
         if (jitConfig->javaVM->phase != J9VM_PHASE_NOT_STARTUP)
            {
            TR_Hotness hotness = that->_methodBeingCompiled->_optimizationPlan->getOptLevel();
            if (hotness == hot)
               {
               if (!isEDOCompilation)
                  sc->addHint(method, TR_HintHot);
               }
            else if (hotness == scorching)
               {
               sc->addHint(method, TR_HintScorching);
               }
            // We also want to add a hint about methods compiled (not AOTed) during startup
            // In subsequent runs we should give such method lower counts the idea being
            // that if I take the time to compile method, why not do it sooner
            sc->addHint(method, TR_HintMethodCompiledDuringStartup);
            }
         }

      // If this is a cold/warm compilation that takes too much memory
      // add a hint to avoid queuing a forced upgrade
      // Look only during startup to avoid creating too many hints.
      // If SCC is larger we could store hints for more methods
      //
      if (jitConfig->javaVM->phase != J9VM_PHASE_NOT_STARTUP)
         {
         if (scratchSegmentProvider.regionBytesAllocated() > TR::Options::_memExpensiveCompThreshold)
            {
            TR_Hotness hotness = that->_methodBeingCompiled->_optimizationPlan->getOptLevel();
            if (hotness <= cold)
               {
               sc->addHint(method, TR_HintLargeMemoryMethodC);
               }
            else if (hotness == warm)
               {
               sc->addHint(method, TR_HintLargeMemoryMethodW);
               }
            }

         if (that->getCompilationInfo()->getMethodQueueSize() - that->getQszWhenCompStarted() > TR::Options::_cpuExpensiveCompThreshold)
            {
            TR_Hotness hotness = that->_methodBeingCompiled->_optimizationPlan->getOptLevel();
            if (hotness <= cold)
               {
               sc->addHint(method, TR_HintLargeCompCPUC);
               }
            else if (hotness == warm)
               {
               sc->addHint(method, TR_HintLargeCompCPUW);
               }
            }
         }
      }
#endif // !PUBLIC_BUILD
   return metaData;
   }

static bool
foundJ9SharedDataForMethod(TR_OpaqueMethodBlock *method, TR::Compilation *comp, J9JITConfig *jitConfig)
   {
   unsigned char buffer[1000];
   uint32_t length = 1000;
   J9SharedDataDescriptor descriptor;
   if (!TR::Options::sharedClassCache())
      return false;
   J9SharedClassConfig * scConfig = jitConfig->javaVM->sharedClassConfig;
   descriptor.address = buffer;
   descriptor.length = length;
   descriptor.type = J9SHR_ATTACHED_DATA_TYPE_JITPROFILE;
   descriptor.flags = J9SHR_ATTACHED_DATA_NO_FLAGS;
   J9VMThread *vmThread = ((TR_J9VM *)comp->fej9())->getCurrentVMThread();
   J9ROMMethod * romMethod = (J9ROMMethod*)J9_ROM_METHOD_FROM_RAM_METHOD((J9Method *)method);
   IDATA dataIsCorrupt;
   void *store = (void *)scConfig->findAttachedData(vmThread, romMethod, &descriptor, &dataIsCorrupt);
   if (store != (void *)descriptor.address)  // a stronger check, as found can be error value
         return false;
   return store ? true : false;
   }

// Helper function to perform AOT Load process
TR_MethodMetaData *
TR::CompilationInfoPerThreadBase::performAOTLoad(
   J9VMThread * vmThread,
   TR::Compilation * compiler,
   TR_ResolvedMethod * compilee,
   TR_J9VMBase *vm,
   J9Method * method
   )
   {
   TR_FilterBST *filterInfo = NULL;
   TR_ASSERT(methodCanBeCompiled(compiler->trMemory(), vm, compilee, filterInfo), "This should already have failed in wrappedCompile");

   // Perform an AOT load
   //
   // Load the AOT body using a non shared cache VM (don't use vm because it's J9SharedCacheVM for the compile)
   TR_J9VMBase *loadvm = TR_J9VMBase::get(jitConfig, vmThread);
   if (TR::Options::getVerboseOption(TR_VerboseCompilationDispatch))
      {
      TR_VerboseLog::writeLineLocked(
         TR_Vlog_DISPATCH,
         "Loading previously AOT compiled body for %s @ %s",
         compiler->signature(),
         compiler->getHotnessName()
         );
      }
   TR_MethodMetaData *metaData = installAotCachedMethod(
      vmThread,
      _methodBeingCompiled->_aotCodeToBeRelocated,
      method,
      loadvm,
      compiler->getOptions(),
      compilee,
      _methodBeingCompiled,
      compiler
      );
   _methodBeingCompiled->_newStartPC = metaData ? reinterpret_cast<void *>(metaData->startPC) : 0;

   if (!metaData) // Failed to relocate!
      {
      if (TR::Options::getVerboseOption(TR_VerboseCompilationDispatch))
         {
         TR_VerboseLog::writeLineLocked(
            TR_Vlog_DISPATCH,
            "Failed to load previously AOT compiled body for %s @ %s",
            compiler->signature(),
            compiler->getHotnessName()
            );
         }

      compiler->failCompilation<J9::AOTRelocationFailed>("Failed to relocate");
      }
   else
      {
      if (TR::Options::getVerboseOption(TR_VerboseCompilationDispatch))
         {
         TR_VerboseLog::writeLineLocked(
            TR_Vlog_DISPATCH,
            "Successfully loaded previously AOT compiled body for %s @ %s",
            compiler->signature(),
            compiler->getHotnessName()
            );
         }
      // if we delayed relocation, persist iprofiler info here
      if (!compiler->getOption(TR_DisableDelayRelocationForAOTCompilations))
         {
         TR_SharedCache *sc = compiler->fej9()->sharedCache();
         if (sc && !foundJ9SharedDataForMethod(compilee->getPersistentIdentifier(), compiler, _jitConfig))
            {
            sc->persistIprofileInfo(NULL, compilee, compiler);
            TR_MethodMetaData *metaData = _methodBeingCompiled->_compInfoPT->getMetadata();
            U_32 numCallSites = getNumInlinedCallSites(metaData);
            TR::StackMemoryRegion stackMemoryRegion(*compiler->trMemory());
            for (int32_t i = 0; i < numCallSites; i++)
               {
               U_8 * inlinedCallSite = getInlinedCallSiteArrayElement(metaData, i);
               TR_OpaqueMethodBlock* j9method = (TR_OpaqueMethodBlock *)getInlinedMethod(inlinedCallSite);
               if(!isPatchedValue((J9Method *)j9method))
                  {
                  TR_ResolvedJ9Method resolvedj9method = TR_ResolvedJ9Method(j9method, compiler->fej9(), compiler->trMemory());
                  TR_ResolvedMethod *method = &resolvedj9method;
                  sc->persistIprofileInfo(NULL, method, compiler);
                  }
               }
            }
         }
      }
   return metaData;
   }

// This routine should only be called from wrappedCompile
TR_MethodMetaData *
TR::CompilationInfoPerThreadBase::compile(
   J9VMThread * vmThread,
   TR::Compilation * compiler,
   TR_ResolvedMethod * compilee,
   TR_J9VMBase &vm,
   TR_OptimizationPlan *optimizationPlan,
   TR::SegmentAllocator const &scratchSegmentProvider
   )
   {

   PORT_ACCESS_FROM_JAVAVM(_jitConfig->javaVM);
   J9JavaVM *javaVM = _jitConfig->javaVM;

   TR_MethodMetaData * metaData = NULL;

   if (_methodBeingCompiled->_priority >= CP_SYNC_MIN)
      ++_compInfo._numSyncCompilations;
   else
      ++_compInfo._numAsyncCompilations;

   if (_methodBeingCompiled->isDLTCompile())
      compiler->setDltBcIndex(static_cast<J9::MethodInProgressDetails &>(_methodBeingCompiled->getMethodDetails()).getByteCodeIndex());

   bool volatile haveLockedClassUnloadMonitor = false; // used for compilation without VM access

   try
      {
      InterruptibleOperation compilingMethodBody(*this);
#if defined(J9VM_INTERP_PROFILING_BYTECODES)
      TR_IProfiler *profiler = vm.getIProfiler();
      if (profiler)
         profiler->resetProfiler();
#endif

      TR::IlGeneratorMethodDetails & details = _methodBeingCompiled->getMethodDetails();
      J9Method *method = details.getMethod();

      TRIGGER_J9HOOK_JIT_COMPILING_START(_jitConfig->hookInterface, vmThread, method);

      // Prepare compilation
      //
      if (_jitConfig->tracingHook)
         compiler->setDebug(
            ((TR_CreateDebug_t)_jitConfig->tracingHook)(compiler)
         );

      if (!_methodBeingCompiled->isRemoteCompReq()) // remote compilations are performed elsewhere
         {
         // Print compiling method
         //
         struct CompilationTrace
            {
            CompilationTrace(TR::Compilation &compiler) : _compiler(compiler)
               {
               TR_ASSERT(_compiler.getHotnessName(_compiler.getMethodHotness()), "expected to have a hotness string");
               if (_compiler.getOutFile() != NULL && _compiler.getOption(TR_TraceAll))
                  traceMsg(&_compiler, "<compile\n"
                          "\tmethod=\"%s\"\n"
                          "\thotness=\"%s\"\n"
                          "\tisProfilingCompile=%d>\n",
                          _compiler.signature(),
                          _compiler.getHotnessName(_compiler.getMethodHotness()),
                          _compiler.isProfilingCompilation());
               }
            ~CompilationTrace() throw()
               {
               if (_compiler.getOutFile() != NULL && _compiler.getOption(TR_TraceAll))
                  traceMsg(&_compiler, "</compile>\n\n");
               }
         private:
            TR::Compilation &_compiler;
            };
         CompilationTrace compilationTrace(*compiler);
         }

      if (TR::Options::isAnyVerboseOptionSet(TR_VerbosePerformance, TR_VerboseCompileStart))
         {
         char compilationTypeString[15]={0};
         snprintf(compilationTypeString, sizeof(compilationTypeString), "%s%s",
                 vm.isAOT_DEPRECATED_DO_NOT_USE() ? "AOT " : "",
                 compiler->isProfilingCompilation() ? "profiled " : ""
                );
         bool incomplete;
         uint64_t freePhysicalMemorySizeB = _compInfo.computeAndCacheFreePhysicalMemory(incomplete);

         if (freePhysicalMemorySizeB != OMRPORT_MEMINFO_NOT_AVAILABLE)
            {
            TR_VerboseLog::writeLineLocked(
               TR_Vlog_COMPSTART,
               "(%s%s) Compiling %s %s %s j9m=%p t=%llu compThread=%d memLimit=%zu KB freePhysicalMemory=%llu MB",
               compilationTypeString,
               compiler->getHotnessName(compiler->getMethodHotness()),
               compiler->signature(),
               compiler->isDLT() ? "DLT" : "",
               details.name(),
               method,
               _compInfo.getPersistentInfo()->getElapsedTime(),
               compiler->getCompThreadID(),
               scratchSegmentProvider.allocationLimit() >> 10,
               freePhysicalMemorySizeB >> 20
               );
            }
         else
            {
            TR_VerboseLog::writeLineLocked(
               TR_Vlog_COMPSTART,
               "(%s%s) Compiling %s %s %s j9m=%p t=%llu compThread=%d memLimit=%zu KB",
               compilationTypeString,
               compiler->getHotnessName(compiler->getMethodHotness()),
               compiler->signature(),
               compiler->isDLT() ? "DLT" : "",
               details.name(),
               method,
               _compInfo.getPersistentInfo()->getElapsedTime(),
               compiler->getCompThreadID(),
               scratchSegmentProvider.allocationLimit() >> 10
               );
            }
         }

      // Precompile option appears in HRT documentation
      //
      if (TR::Options::getVerboseOption(TR_VerbosePrecompile))
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_PRECOMP,"%s", compiler->signature());
         }

      if (
         _methodBeingCompiled->_oldStartPC
         && compiler->getOption(TR_FailPreXRecompile)
         && TR::Recompilation::getJittedBodyInfoFromPC(_methodBeingCompiled->_oldStartPC)->getIsInvalidated()
         )
         {
         compiler->failCompilation<TR::CompilationException>("TR_FailPreXRecompile option");
         }

      // If we want to compile without VM access, now it's the time to release it
      // For the JITaaS client we must not enter this path. The class unload monitor 
      // will not be acquired/released and we'll only release VMaccess when 
      // waiting for a reply from the server
      if (!compiler->getOption(TR_DisableNoVMAccess) &&
          !_methodBeingCompiled->_aotCodeToBeRelocated &&
          !_methodBeingCompiled->isRemoteCompReq())
         {
#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
         bool doAcquireClassUnloadMonitor = true;
#else
         bool doAcquireClassUnloadMonitor = compiler->getOption(TR_EnableHCR) || compiler->getOption(TR_FullSpeedDebug);
#endif
         if (doAcquireClassUnloadMonitor)
            {
            _compInfo.debugPrint(NULL, "\tcompilation thread acquiring class unload monitor\n");
            TR::MonitorTable::get()->readAcquireClassUnloadMonitor(getCompThreadId());
            haveLockedClassUnloadMonitor = true;
            }
         releaseVMAccess(vmThread);
         // GC can go ahead now.
         }

      // The inlineFieldWatches flag is set when Field Watch is actually triggered at runtime.
      // When it happens, all the methods on stack are decompiled and those in
      // the compilation queue are invalidated. Set the option here to guarantee the
      // mode is detected at the right moment so that all methods compiled after respect the
      // data watch point.
      if (_jitConfig->inlineFieldWatches)
         compiler->setOption(TR_EnableFieldWatch);

      // Compile the method
      //

      // For local AOT and JITServer we need to reserve a data cache to ensure contiguous memory
      // For TOSS_CODE we need to reserve a data cache so that we know what dataCache
      // to reset when throwing the data away
      //
      if ((_jitConfig->runtimeFlags & J9JIT_TOSS_CODE)
            || compiler->getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER
#if defined(J9VM_INTERP_AOT_COMPILE_SUPPORT)
            || vm.isAOT_DEPRECATED_DO_NOT_USE() && !_methodBeingCompiled->isRemoteCompReq()
#endif //endif J9VM_INTERP_AOT_COMPILE_SUPPORT
        )
         {
         // Need to reserve a data cache
         // if this is a retrial we may already have a data cache that is reserved
         // by the previous compilation; check this scenario first
         //
         if (_reservedDataCache)
            {
            TR_ASSERT(_methodBeingCompiled->_compilationAttemptsLeft < MAX_COMPILE_ATTEMPTS, "reserved dataCache for a brand new compilation\n");
            compiler->setReservedDataCache(_reservedDataCache);
            _reservedDataCache = NULL; // we don't need this field anymore; It is only used to pass
                                       // the information about reserved data caches from one retrial
                                       // to the next
            }
         else
            {
            // TODO: find an appropriate datacache size
            TR_DataCache *dataCache = TR_DataCacheManager::getManager()->reserveAvailableDataCache(vmThread, 4*1024);
            compiler->setReservedDataCache(dataCache);
            if (!dataCache)
               {
               compiler->failCompilation<J9::DataCacheError>("Cannot reserve dataCache");
               }
            }

         TR_DataCache *dataCache = (TR_DataCache*)compiler->getReservedDataCache();
         dataCache->setAllocationMark();
         }
      else // non AOT, non TOSS_CODE
         {
         // Safety net; cancel the reservations from previous compilations
         if (_reservedDataCache)
            {
            TR_ASSERT(_methodBeingCompiled->_compilationAttemptsLeft < MAX_COMPILE_ATTEMPTS, "Reserved dcache must be saved only for a comp retrial");
            TR_DataCacheManager::getManager()->makeDataCacheAvailable(_reservedDataCache);
            _reservedDataCache = NULL;
            }
         }

      if (vm.isAOT_DEPRECATED_DO_NOT_USE() && !_methodBeingCompiled->isRemoteCompReq() || _methodBeingCompiled->isOutOfProcessCompReq())
         {
         TR_DataCache *dataCache = (TR_DataCache*)compiler->getReservedDataCache();
         TR_ASSERT(dataCache, "Must have a reserved dataCache for AOT compilations");
         compiler->setAotMethodDataStart((uint32_t *)dataCache->getCurrentHeapAlloc());

         // First allocate a TR_AOTMethodHeader
         //
         TR_AOTMethodHeader *cacheEntryAOTMethodHeader = pointer_cast<TR_AOTMethodHeader *>(vm.allocateRelocationData(compiler, sizeof(struct TR_AOTMethodHeader)));
         TR_ASSERT(cacheEntryAOTMethodHeader, "Must have cacheEntryAOTMethodHeader or throw an exception");
         memset(cacheEntryAOTMethodHeader, 0, sizeof(struct TR_AOTMethodHeader));
         // Go back to find the J9JITDataCacheHeader
         J9JITDataCacheHeader *cacheEntry = pointer_cast<J9JITDataCacheHeader *>(cacheEntryAOTMethodHeader) - 1;

         // the size is set up by allocateRelocationData which also does the rounding;
         cacheEntry->type = J9_JIT_DCE_AOT_METHOD_HEADER;
         compiler->setAotMethodDataStart(cacheEntry);
         cacheEntryAOTMethodHeader->majorVersion = TR_AOTMethodHeader_MajorVersion;        // AOT code and data major version
         cacheEntryAOTMethodHeader->minorVersion = TR_AOTMethodHeader_MinorVersion;        // AOT code and data minor version
         }

      intptr_t rtn = 0;

      if (!_methodBeingCompiled->isRemoteCompReq() || _methodBeingCompiled->isAotLoad())
         {
         if (!_methodBeingCompiled->isAotLoad())
            {
            if (TR::Options::getVerboseOption(TR_VerboseCompilationDispatch))
               {
               TR_VerboseLog::writeLineLocked(
                  TR_Vlog_DISPATCH,
                  "Compiling %s @ %s",
                  compiler->signature(),
                  compiler->getHotnessName()
                  );
               }

            if (compiler->getOption(TR_AlwaysSafeFatal)) {
               TR_ASSERT_SAFE_FATAL(false, "alwaysSafeFatal set");
                TR_VerboseLog::writeLineLocked(
                  TR_Vlog_INFO ,
                  "Bypassed TR_ASSERT_SAFE_FATAL %s @ %s",
                  compiler->signature(),
                  compiler->getHotnessName()
                  );
            }

            if (compiler->getOption(TR_AlwaysFatalAssert)) {
               TR_ASSERT_FATAL(false, "alwaysFatalAssert set");
            }

            if (vm.isAOT_DEPRECATED_DO_NOT_USE() &&
               compiler->getOption(TR_UseSymbolValidationManager))
               compiler->getSymbolValidationManager()->populateWellKnownClasses();

            rtn = compiler->compile();

            if ( TR::Options::getVerboseOption(TR_VerboseCompilationDispatch) && !rtn)
               {
               TR_VerboseLog::vlogAcquire();

               TR_VerboseLog::writeLine(
                  TR_Vlog_DISPATCH,
                  "Successfully created compiled body [" POINTER_PRINTF_FORMAT "-",
                  compiler->cg()->getCodeStart()
                  );

               TR_VerboseLog::write(
                  POINTER_PRINTF_FORMAT "] for %s @ %s",
                  compiler->cg()->getCodeEnd(),
                  compiler->signature(),
                  compiler->getHotnessName()
                  );

               TR_VerboseLog::vlogRelease();
               }
            }
         else // AOT load
            {
            metaData = performAOTLoad(vmThread, compiler, compilee, &vm, method);
            }
         }
      else if (_methodBeingCompiled->isRemoteCompReq()) // JITServer Client Mode
         {
         metaData = remoteCompile(vmThread, compiler, compilee, method, details, this);
         }


      // Re-acquire VM access if needed
      //
      if (!compiler->getOption(TR_DisableNoVMAccess))
         {
#if !defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
         if (compiler->getOption(TR_EnableHCR) || compiler->getOption(TR_FullSpeedDebug))
#endif
            {
            if (haveLockedClassUnloadMonitor)
               {
               TR::MonitorTable::get()->readReleaseClassUnloadMonitor(getCompThreadId());
               haveLockedClassUnloadMonitor = false; // Need this because an exception might happen during createMethodMetadata
               }                                     // and on the exception path we check haveLockedClassUnloadMonitor
            // Here the GC/HCR might happen
            }
         if (!_methodBeingCompiled->isRemoteCompReq() && !(vmThread->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS))
            acquireVMAccessNoSuspend(vmThread);

         // The GC could have unloaded some classes when we released the classUnloadMonitor, or HCR could have kicked in
         //
         if (compilationShouldBeInterrupted())
            {
            if (TR::Options::getVerboseOption(TR_VerboseCompilationDispatch))
               {
               TR_VerboseLog::writeLineLocked(
                  TR_Vlog_DISPATCH,
                  "Interrupting compilation of %s @ %s",
                  compiler->signature(),
                  compiler->getHotnessName()
                  );
               }
            compiler->failCompilation<TR::CompilationInterrupted>("Compilation interrupted");
            }
         }

      if (rtn != COMPILATION_SUCCEEDED)
         {
         TR_ASSERT(false, "Compiler returned non zero return code %d\n", rtn);
         compiler->failCompilation<TR::CompilationException>("Compilation Failure");
         }

      if (TR::Options::getVerboseOption(TR_VerboseCompYieldStats))
         {
         if (compiler->getMaxYieldInterval() > TR::Options::_compYieldStatsThreshold)
            {
            TR_VerboseLog::vlogAcquire();
            compiler->printCompYieldStats();
            TR_VerboseLog::vlogRelease();
            }
         }

      _methodBeingCompiled->_compErrCode = compilationOK;
      if (!_methodBeingCompiled->isAotLoad() && !_methodBeingCompiled->isRemoteCompReq())
         {
         class TraceMethodMetadata
            {
         public:
            TraceMethodMetadata(TR::Compilation &compiler) :
               _compiler(compiler),
               _trace(compiler.getOutFile() != NULL && compiler.getOption(TR_TraceAll))
               {
               if (_trace)
                  traceMsg(&_compiler, "<metadata>\n");
               }
            ~TraceMethodMetadata()
               {
               if (_trace)
                  traceMsg(&_compiler, "</metadata>\n");
               }
         private:
            TR::Compilation &_compiler;
            const bool _trace;
            };

         TraceMethodMetadata traceMetadata(*compiler);

         if (TR::Options::getVerboseOption(TR_VerboseCompilationDispatch))
            {
            TR_VerboseLog::writeLineLocked(
               TR_Vlog_DISPATCH,
               "Creating metadata for %s @ %s",
               compiler->signature(),
               compiler->getHotnessName()
               );
            }

         metaData = createMethodMetaData(vm, compilee, compiler);
         if (!metaData)
            {
            if (TR::Options::getVerboseOption(TR_VerboseCompilationDispatch))
               {
               TR_VerboseLog::writeLineLocked(
                  TR_Vlog_DISPATCH,
                  "Failed to create metadata for %s @ %s",
                  compiler->signature(),
                  compiler->getHotnessName()
                  );
               }
            compiler->failCompilation<J9::MetaDataCreationFailure>("Metadata creation failure");
            }

         if (TR::Options::getVerboseOption(TR_VerboseCompilationDispatch))
            {
            TR_VerboseLog::writeLineLocked(
               TR_Vlog_DISPATCH,
               "Successfully created metadata [" POINTER_PRINTF_FORMAT "] for %s @ %s",
               metaData,
               compiler->signature(),
               compiler->getHotnessName()
               );
            }

         setMetadata(metaData);

         // Put a metaData pointer into the Code Cache Header(s).
         //
         uint8_t *warmMethodHeader = compiler->cg()->getBinaryBufferStart() - sizeof(OMR::CodeCacheMethodHeader);
         memcpy( warmMethodHeader + offsetof(OMR::CodeCacheMethodHeader, _metaData), &metaData, sizeof(metaData) );
         if ( metaData->startColdPC )
            {
            uint8_t *coldMethodHeader = reinterpret_cast<uint8_t *>(metaData->startColdPC) - sizeof(OMR::CodeCacheMethodHeader);
            memcpy( coldMethodHeader + offsetof(OMR::CodeCacheMethodHeader, _metaData), &metaData, sizeof(metaData) );
            }

         // FAR: should we do postpone this copying until after CHTable commit?
         metaData->runtimeAssumptionList = *(compiler->getMetadataAssumptionList());

         // We don't need to delete the metadataAssumptionList from the compilation object,
         // and in fact it would be wrong to do so because code during chtable.commit is
         // expecting something in the compiler object
         }

      if (compiler->getOption(TR_BreakAfterCompile))
         {
         fprintf(stderr, "\n=== Finished compiling %s at %p ===\n", compiler->signature(), (void *)metaData->startPC);
         TR::Compiler->debug.breakPoint();
         }

      if (metaData &&
          compiler->getPersistentInfo()->isRuntimeInstrumentationEnabled())
         {
         // This must be called after createMethodMetaData because TR_PersistentJittedBodyInfo can change in that function.
         _compInfo.getHWProfiler()->registerRecords(metaData, compiler);
         }

         {
         TR::ClassTableCriticalSection chTableCommit(&vm);
         TR_ASSERT(!chTableCommit.acquiredVMAccess(), "We should have already acquired VM access at this point.");
         TR_CHTable *chTable = compiler->getCHTable();
         if (chTable && chTable->commit(compiler) == false)
            {
            // If we created a TR_PersistentMethodInfo, fix the next compilation level
            // because if we retry the compilation we will fail an assume later on
            // due to discrepancy in compilation level.
            if (metaData->startPC)
               {
               TR_PersistentJittedBodyInfo *bodyInfo = TR::Recompilation::getJittedBodyInfoFromPC(reinterpret_cast<void *>(metaData->startPC));
               if (bodyInfo)
                  {
                  bodyInfo->getMethodInfo()->setNextCompileLevel(bodyInfo->getHotness(), false);
                  }
               }
            if (TR::Options::isAnyVerboseOptionSet(TR_VerboseCompileEnd, TR_VerbosePerformance, TR_VerboseCompFailure))
               {
               TR_VerboseLog::writeLineLocked(TR_Vlog_FAILURE, "Failure while committing chtable for %s", compiler->signature());
               }

            if (!compiler->fej9()->isAOT_DEPRECATED_DO_NOT_USE())
               {
               TR_J9SharedCache *sc = (TR_J9SharedCache *) (compiler->fej9()->sharedCache());
               if (sc)
                  sc->addHint(compiler->getCurrentMethod(), TR_HintFailedCHTable);
               }
            compiler->failCompilation<J9::CHTableCommitFailure>("CHTable commit failure");
            }
         }

      // As the body is finished, mark its profile info as active so that the JProfiler thread will inspect it
      if (compiler->getRecompilationInfo())
         {
         TR_PersistentJittedBodyInfo *bodyInfo = compiler->getRecompilationInfo()->getJittedBodyInfo();
         if (bodyInfo && bodyInfo->getProfileInfo())
            {
            bodyInfo->getProfileInfo()->setActive();
            }
         }

      logCompilationSuccess(vmThread, vm, method, scratchSegmentProvider, compilee, compiler, metaData, optimizationPlan);

      TRIGGER_J9HOOK_JIT_COMPILING_END(_jitConfig->hookInterface, vmThread, method);
      }
   catch (const std::exception &e)
      {
      const char *exceptionName;

#if defined(J9ZOS390)
      // Compiling with -Wc,lp64 results in a crash on z/OS when trying
      // to call the what() virtual method of the exception.
      exceptionName = "std::exception";
#else
      exceptionName = e.what();
#endif

      printCompFailureInfo(compiler, exceptionName);
      processException(vmThread, scratchSegmentProvider, compiler, haveLockedClassUnloadMonitor, exceptionName);
      metaData = 0;
      }

   // At this point the compilation has either succeeded and compilation cannot be
   // interrupted anymore, or it has failed. In either case _compilationShouldBeinterrupted flag
   // is not needed anymore
   setCompilationShouldBeInterrupted(0);

   // We should not have the classTableMutex at this point
   TR_ASSERT_FATAL(!TR::MonitorTable::get()->getClassTableMutex()->owned_by_self(),
                   "Should not still own classTableMutex");

   // Increment the number of JIT compilations (either successful or not)
   // performed by this compilation thread
   //
   incNumJITCompilations();

   return metaData;
   }

static J9UTF8 *
getSourceFileName(J9JavaVM *&vm, J9Method* &_method)
   {
   J9Class *clazz = J9_CLASS_FROM_METHOD(_method);
   return getSourceFileNameForROMClass(vm, clazz->classLoader, clazz->romClass);
   }

static TR_OpaqueMethodBlock *
getMethodFromBCInfo(TR_ByteCodeInfo &bcInfo, TR::Compilation *comp)
   {
   TR_OpaqueMethodBlock *method = NULL;

   if (bcInfo.getCallerIndex() >= 0)
      method = comp->getInlinedCallSite(bcInfo.getCallerIndex())._methodInfo;
   else
      method = comp->getCurrentMethod()->getPersistentIdentifier();

   return method;
   }

// assumes that a debug server exists with getLineNumber()
static int32_t getLineNumberFromBCIndex(J9JavaVM *vm, J9Method *method, int32_t bcIndex)
   {
   int32_t lineNumber;

   // We don't always have lineNumber info in the class.  This allows us to use
   // bytecode offset in place of lineNumbers for tools like profile diffing.
   //
   // TODO: Add support for an indexable table of inlined methods.  Then pre-fix the bytecode
   //       offset with the index of the inlined method.
   //
   static char *useByteCodeOffsetForJVMPILineNums = feGetEnv("TR_UseBCOffsetForJVMPILineNums");
   if (useByteCodeOffsetForJVMPILineNums)
      lineNumber = bcIndex;
   else
      lineNumber = getLineNumberForROMClass(vm, method, bcIndex);
   return lineNumber;
   }

static inline U_8 *
writeU64(U_8 * bufferCursor, U_64 data)
   {
   *((U_64 *)bufferCursor) = data;
   return bufferCursor + sizeof(U_64);
   }

static inline U_8 *
writeU32(U_8 * bufferCursor, U_32 data)
   {
   *((U_32 *)bufferCursor) = data;
   return bufferCursor +sizeof(U_32);
   }

static inline U_8 *
writeI32(U_8 * bufferCursor, I_32 data)
   {
   *((I_32 *)bufferCursor) = data;
   return bufferCursor +sizeof(I_32);
   }

static inline U_8 *
writeU16(U_8 * bufferCursor, U_16 data)
   {
   *((U_16 *)bufferCursor) = data;
   return bufferCursor + sizeof(U_16);
   }

static inline U_8 *
writeI16(U_8 * bufferCursor, I_16 data)
   {
   *((I_16 *)bufferCursor) = data;
   return bufferCursor + sizeof(I_16);
   }

static inline U_8 *
writeU8(U_8 * bufferCursor, U_8 data)
   {
   *bufferCursor = data;
   return bufferCursor + sizeof(U_8);
   }

void
TR::CompilationInfo::emitJvmpiCallSites(TR::Compilation *&compiler, J9VMThread *vmThread, J9Method *&_method)
   {
   PORT_ACCESS_FROM_JAVAVM(_jitConfig->javaVM);

   // no point outputting an empty table
   if (compiler->getNumInlinedCallSites() < 1)
      return;

   // format:
   // <u32 eye-catcher> <u32 type> <u32 attributes> <i16 numCallSites>
   // <i16 callerIndex> <u32 callerLineNum> <i32 name length> <utf8 null-terminated calleeSourceFile>
   // <i16 callerIndex> <u32 callerLineNum> <i32 name length> <utf8 null-terminated calleeSourceFile>
   // ...
   // where attributes is:
   // bits 0-3 = version of format
   // ** if nameLength < 0; then the actual file name is the same as for record[-nameLength];

   uint8_t formatVersion = 0;
   uint32_t attributes = 0;    // no attributes, at this moment...
   attributes |= formatVersion;

   uint32_t bufferSize = 3*sizeof(uint32_t) + sizeof(int16_t); // eye catcher, type, attributes, # callsites

   // calculate the buffer size
   for (int16_t i = 0; i < (int16_t) compiler->getNumInlinedCallSites(); i++)
      {
      bufferSize += sizeof(int16_t);  // 2 bytes for calleeIndex
      bufferSize += sizeof(uint32_t); // 4 bytes for line number in caller
      bufferSize += sizeof(uint32_t); // 4 bytes for the length of the source file names

      TR_OpaqueMethodBlock * method = compiler->getInlinedCallSite(i)._methodInfo;

      J9UTF8 * calleeSourceFileUTF8 = getSourceFileName(_jitConfig->javaVM, _method);

      int32_t calleeSourceFileLen;
      if (calleeSourceFileUTF8)
         utf8Data(calleeSourceFileUTF8, calleeSourceFileLen);
      else
         calleeSourceFileLen = 0;

      bufferSize += calleeSourceFileLen + 1; // +1 for null terminator
      }

   U_8 *buffer = (U_8 *)j9mem_allocate_memory(bufferSize, J9MEM_CATEGORY_JIT);
   if (!buffer)
      return;

   U_8 *bufferCursor = buffer;

   bufferCursor = writeU32(bufferCursor, JITA2N_EXTENDED_DATA_EYE_CATCHER);
   bufferCursor = writeU32(bufferCursor, JITA2N_EXTENDED_DATA_CALLSITES);
   bufferCursor = writeU32(bufferCursor, attributes);
   bufferCursor = writeI16(bufferCursor, compiler->getNumInlinedCallSites());

   // Remember the last source file we had; this way, if the same file is
   // used more than once (fairly likely), we can just output a code to use
   // the same filename as the previous one.
   //
   const char *lastCalleeSourceFile = NULL;
   int32_t     lastCalleeSourceFileLen = -1;

   for (int16_t icsIndex = 0; icsIndex < (int16_t) compiler->getNumInlinedCallSites(); icsIndex++)
      {

      TR_ByteCodeInfo       bcInfo = compiler->getInlinedCallSite(icsIndex)._byteCodeInfo;
      TR_OpaqueMethodBlock *method = compiler->getInlinedCallSite(icsIndex)._methodInfo;
      J9UTF8               *calleeSourceFileUTF8 = getSourceFileName(_jitConfig->javaVM, _method);
      int32_t               calleeSourceFileLen = 0;
      const char           *calleeSourceFile = NULL;
      uint32_t              callerLineNumber = -1;
      int16_t               callerIcsIndex = bcInfo.getCallerIndex();

      if (calleeSourceFileUTF8)
         calleeSourceFile = utf8Data(calleeSourceFileUTF8, calleeSourceFileLen);
      else
         calleeSourceFileLen = 0;

      // quick little hack: if the last source file is the same as this one, output the negated index of
      // the last call site as this entry's length, then output a zero-length, null-terminated
      // string for this one's file name
      //
      if (lastCalleeSourceFile && lastCalleeSourceFileLen == calleeSourceFileLen &&
          strncmp(lastCalleeSourceFile, calleeSourceFile, calleeSourceFileLen) == 0)
         {
         // we don't have to update last*, since we're the same;
         // but we do change the output strings
         //
         calleeSourceFileLen = 0x80000000 | (icsIndex - 1);
         calleeSourceFile = "";
         }
      else
         {
         lastCalleeSourceFile = calleeSourceFile;
         lastCalleeSourceFileLen = calleeSourceFileLen;
         }

      callerLineNumber = getLineNumberFromBCIndex(_jitConfig->javaVM, (J9Method *) method, bcInfo.getByteCodeIndex());

      // and now, output our record
      bufferCursor = writeI16(bufferCursor, callerIcsIndex);
      bufferCursor = writeU32(bufferCursor, callerLineNumber);
      bufferCursor = writeI32(bufferCursor, calleeSourceFileLen);

      // if length < 0, then it's been overloaded to be an index, and we don't output the source file
      if (calleeSourceFileLen > 0)
         {
         strncpy((char *) bufferCursor, calleeSourceFile, calleeSourceFileLen); // does not copy null term
         bufferCursor += calleeSourceFileLen;
         }

      *bufferCursor++ = '\0';
      }

   ALWAYS_TRIGGER_J9HOOK_JIT_COMPILED_METHOD_LOAD(_jitConfig->hookInterface, vmThread, _method, buffer, bufferCursor - buffer, NULL);
   j9mem_free_memory(buffer);
   }

// Compilation descriptor:
// <u32 eye-catcher> <u32 type> <u32 attributes> <u32 descriptor> <u64 warm-cc start address> <u64 cold-cc start address>
// descriptor:
//              bit 0     1 => cold     compile
//              bit 1     1 => warm     compile
//              bit 2     1 => hot      compile
//              bit 3     1 => very-hot compile
//              bit 4     1 => scorching compile
//              bit 30    1 => profiled
//              bit 31    0 => warm code-cache, 1 => cold code-cache

int
TR::CompilationInfo::bufferSizeCompilationAttributes()
   {
   return(8 * sizeof(U_32));
   }

U_8 *
TR::CompilationInfo::bufferPopulateCompilationAttributes(U_8 *buffer, TR::Compilation *&compiler, TR_MethodMetaData *metaData)
   {
   U_8 *bufferCursor = buffer;

   // Compilation attributes
   //
   U_32 attributeAndVersionCompAttr = 0;

   bufferCursor = writeU32(bufferCursor, JITA2N_EXTENDED_DATA_EYE_CATCHER);
   bufferCursor = writeU32(bufferCursor, JITA2N_EXTENDED_DATA_COMPILATION_DESC);
   bufferCursor = writeU32(bufferCursor, attributeAndVersionCompAttr);

   U_32 compilationDescriptor = 0;
   switch (compiler->getMethodHotness())
     {
     case cold:
        compilationDescriptor = 0x01;   // Bit 0
     break;
     case warm:
        compilationDescriptor = 0x02;   // Bit 1
     break;
     case hot:
        compilationDescriptor = 0x04;   // Bit 2
     break;
     case veryHot:
        compilationDescriptor = 0x08;   // Bit 3
     break;
     case scorching:
        compilationDescriptor = 0x10;   // Bit 4
     break;
     default:
     break;
     }

   if (compiler->isProfilingCompilation())
      {
      compilationDescriptor |= 0x40000000;  // Bit 30
      }

   bufferCursor = writeU32(bufferCursor, compilationDescriptor);
   bufferCursor = writeU64(bufferCursor, (U_64) metaData->startPC);
   bufferCursor = writeU64(bufferCursor, (U_64) metaData->startColdPC);

   return bufferCursor;
   }

// Inline call site table
// <u32 eye-catcher> <u32 type> <u32 attributes> <u32 number of entries>
// <i32 nameLength> <null terminated callsite signature 0>
// <i32 nameLength> <null terminated callsite signature 1>
// <i32 nameLength> <null terminated callsite signature 2>

int
TR::CompilationInfo::bufferSizeInlinedCallSites(TR::Compilation *&compiler, TR_MethodMetaData *metaData)
   {
   int bufferSize = 0;

   // Callsite table
   if (compiler->getNumInlinedCallSites() > 0)
      {
      bufferSize += 4 * sizeof(U_32);                     // Inline callsite table header
      }

   // calculate the inlined method table buffer size
   for (int16_t i = 0; i < (int16_t) compiler->getNumInlinedCallSites(); i++)
      {
      bufferSize += sizeof(uint32_t); // 4 bytes for the length of the signature

      TR_OpaqueMethodBlock * method = compiler->getInlinedCallSite(i)._methodInfo;

      const char * calleeName = compiler->fej9()->sampleSignature(method); // FIXME we only need the length, not the signature

      int32_t calleeLen;
      if (calleeName)
         calleeLen = strlen(calleeName);
      else
         calleeLen = 0;

      bufferSize += calleeLen + 1;  // for null terminator
      }

   return bufferSize;
   }

uint8_t * TR::CompilationInfo::bufferPopulateInlinedCallSites(uint8_t *buffer, TR::Compilation *&compiler, TR_MethodMetaData *metaData)
   {
   U_8 *bufferCursor = buffer;

   // Inlined call sites
   //
   if (compiler->getNumInlinedCallSites() > 0)
      {
      U_32 sizeOfTable = compiler->getNumInlinedCallSites();
      U_32 attributeAndVersionCSI = 0;

      bufferCursor = writeU32(bufferCursor, JITA2N_EXTENDED_DATA_EYE_CATCHER);
      bufferCursor = writeU32(bufferCursor, JITA2N_EXTENDED_DATA_INLINETABLE);
      bufferCursor = writeU32(bufferCursor, attributeAndVersionCSI);
      bufferCursor = writeU32(bufferCursor, sizeOfTable);
      }

   // calculate the inlined method table buffer size
   for (int16_t i = 0; i < (int16_t) compiler->getNumInlinedCallSites(); i++)
      {
      TR_OpaqueMethodBlock * method = compiler->getInlinedCallSite(i)._methodInfo;

      const char * calleeName = compiler->fej9()->sampleSignature(method); // FIXME: we only need the length, not the signature

      int32_t calleeLen;
      if (calleeName)
         calleeLen = strlen(calleeName);
      else
         calleeLen = 0;

      bufferCursor = writeI32(bufferCursor, calleeLen);

      if (calleeLen > 0)
         {
         strncpy((char *) bufferCursor, calleeName, calleeLen); // does not copy null term
         bufferCursor += calleeLen;
         }

      *bufferCursor = '\0';
      bufferCursor++;
      }

   return bufferCursor;
   }

// Linenumber/callsite index table:
// <u32 eye-catcher> <u32 type> <u32 attributes> <i32 nameLength> <utf8 null-terminated name>
// <u8 num offsets> <i16 icsIndex> <u32 lineNum> <offset 1> <offset 2> <offset 3>...
// <u8 num offsets> <i16 icsIndex> <u32 lineNum> <offset 1> ...
// where attributes is
// bits 0-3 = version of format
// bits 4-7 = size of offset

int
TR::CompilationInfo::bufferSizeLineNumberTable(TR::Compilation *&compiler, TR_MethodMetaData *metaData, J9Method *&_method)
   {
   int bufferSize = 0;
   TR::Instruction * instruction;

   // figure out size of offset
   U_8 * base = (U_8 *) metaData->startPC;
   U_8 * end = (U_8 *) metaData->endWarmPC;
   UDATA length = end - base;

   short sizeOfOffset = 0;
   if (length <= UCHAR_MAX)
      sizeOfOffset = sizeof(U_8);
   else if (length <= USHRT_MAX)
      sizeOfOffset = sizeof(U_16);
   else if (length <= UINT_MAX)
      sizeOfOffset = sizeof(U_32);
   else if (length <= ULONG_MAX)
      sizeOfOffset = sizeof(U_64);

   // count instructions
   uint32_t numInstructions = 0;
   for (instruction = compiler->cg()->getFirstInstruction() ; instruction; instruction = instruction->getNext())
      {
      // skip preprologue and suchnot.
      U_8 * address = instruction->getBinaryEncoding();

      if (address < base || address > end)
         continue;

      numInstructions++;
      }

   // get the name of the source file
   J9UTF8 * sourceFileUTF8 = getSourceFileName(_jitConfig->javaVM, _method);

   I_32 sourceFileLen;
   const char *sourceFile;

   if (sourceFileUTF8)
      sourceFile = utf8Data(sourceFileUTF8, sourceFileLen);
   else
      sourceFileLen = 0;

   // Line number/callsite index table
   bufferSize += 4 * sizeof(U_32) + sourceFileLen + 1; // for the header (+1 for null term)
   bufferSize += numInstructions * (sizeOfOffset + sizeof(U_32) + sizeof(U_8) + sizeof(I_16));

   return bufferSize;
   }

uint8_t * TR::CompilationInfo::bufferPopulateLineNumberTable(uint8_t * buffer, TR::Compilation *&compiler, TR_MethodMetaData *metaData, J9Method *&_method)
   {
   U_8 *bufferCursor = buffer;
   TR::Instruction * instruction;

   U_8 formatVersion = 1;

   // figure out size of offset
   U_8 * base = (U_8 *) metaData->startPC;
   U_8 * end = (U_8 *) metaData->endWarmPC;
   UDATA length = end - base;

   short sizeOfOffset = 0;
   if (length <= UCHAR_MAX)
      sizeOfOffset = sizeof(U_8);
   else if (length <= USHRT_MAX)
      sizeOfOffset = sizeof(U_16);
   else if (length <= UINT_MAX)
      sizeOfOffset = sizeof(U_32);
   else if (length <= ULONG_MAX)
      sizeOfOffset = sizeof(U_64);

   U_32 attributes = 0;
   attributes |= formatVersion;
   attributes |= sizeOfOffset << 4;

   I_32 sourceFileLen;
   const char *sourceFile;

   // get the name of the source file
   J9UTF8 * sourceFileUTF8 = getSourceFileName(_jitConfig->javaVM, _method);
   if (sourceFileUTF8)
      sourceFile = utf8Data(sourceFileUTF8, sourceFileLen);
   else
      sourceFileLen = 0;

   // Line number table
   //
   bufferCursor = writeU32(bufferCursor, JITA2N_EXTENDED_DATA_EYE_CATCHER);
   bufferCursor = writeU32(bufferCursor, JITA2N_EXTENDED_DATA_LINENUM);
   bufferCursor = writeU32(bufferCursor, attributes);
   bufferCursor = writeI32(bufferCursor, sourceFileLen);

   if (sourceFileLen > 0)
      {
      strncpy((char *) bufferCursor, sourceFile, sourceFileLen); // does not copy null term
      bufferCursor += sourceFileLen;
      }
   *bufferCursor = '\0';
   bufferCursor++;

   TR_OpaqueMethodBlock *method;
   TR_ByteCodeInfo bcInfo;
   U_32 lineNumber, lastLineNumber = 0;
   UDATA offset, lastOffset = 0;
   U_8 *recordHead, *address;
   I_16 icsIndex, lastIcsIndex = -1;
   bool needNewRecord;

   U_8 numOffsets = 0;
   bool isFirstLoop = true;
   recordHead = bufferCursor;

   // dump instructions
   for (instruction = compiler->cg()->getFirstInstruction(); instruction; instruction = instruction->getNext())
      {
      address = instruction->getBinaryEncoding();

      // skip preprologue and suchnot.
      if (address < base || address > end)
         continue;

      offset =     address - base;
      bcInfo =     instruction->getNode()->getByteCodeInfo();
      icsIndex =   bcInfo.getCallerIndex();
      method =     getMethodFromBCInfo(bcInfo, compiler);
      lineNumber = getLineNumberFromBCIndex(_jitConfig->javaVM, (J9Method *) method, bcInfo.getByteCodeIndex());

      needNewRecord = (isFirstLoop || // this means we don't need to init lastLineNumber
                       numOffsets == UCHAR_MAX ||
                       lineNumber != lastLineNumber ||
                       lastIcsIndex != icsIndex);

      if (needNewRecord)
         {
         // we need to finish the last record
         if (!isFirstLoop)
            {

            // don't emit empty records
            if (numOffsets > 0)
               *recordHead = numOffsets;
            else
               bufferCursor = recordHead;

            }

         // and begin the next record
         recordHead = bufferCursor;
         bufferCursor++; // leave room for num offsets

         bufferCursor = writeI16(bufferCursor, icsIndex);
         bufferCursor = writeU32(bufferCursor, lineNumber);
         lastLineNumber = lineNumber;
         lastIcsIndex = icsIndex;
         numOffsets = 0;
         }

      // only output offset if it's new, or if we're starting a new record
      // again, if isFirstLoop, we don't need to init lastOffset -
      // short circuit evaluation means we don't test against lastOffset
      //
      if (isFirstLoop || offset != lastOffset)
         {
         switch (sizeOfOffset)
            {
            case sizeof(U_8):  bufferCursor = writeU8(bufferCursor, (U_8) offset); break;
            case sizeof(U_16): bufferCursor = writeU16(bufferCursor, (U_16) offset); break;
            case sizeof(U_32): bufferCursor = writeU32(bufferCursor, (U_32) offset); break;
            case sizeof(U_64): bufferCursor = writeU64(bufferCursor, (U_64) offset); break;
            default:
               // do nothing. shouldn't happen
               // if it does, then there'll be uninitialized data in this
               // position, which may be a security issue, but it will be
               // a greater "what's going on?!?" issue.
               TR_ASSERT(0, "Unknown offset size.");
               break;
            }
         lastOffset = offset;
         numOffsets++;
         }

      isFirstLoop = false;
      } // end foreach instruction

   // we have to still record the number of offsets for the last record.
   // if there are no offsets, then still record a 0
   //
   *recordHead = numOffsets;

   return bufferCursor;
   }

void
TR::CompilationInfo::emitJvmpiExtendedDataBuffer(TR::Compilation *&compiler, J9VMThread *vmThread, J9Method *&_method, TR_MethodMetaData *metaData)
   {
   PORT_ACCESS_FROM_JAVAVM(_jitConfig->javaVM);

   // bufferCursor is our main cursor, but when we need to print out addresses,
   U_8 *buffer, *bufferCursor, *bufferCursorStart;
   U_32 bufferSize = 0;

   // ... SIZES ...
   // .............

   // Compilation attributes
   bufferSize += bufferSizeCompilationAttributes();
   // Callsite table
   bufferSize += bufferSizeInlinedCallSites(compiler, metaData);
   // Line number/callsite index table
   bufferSize += bufferSizeLineNumberTable(compiler, metaData, _method);

   // ... BUY THE BUFFER ...
   // ....... ..............
   buffer = (U_8 *)j9mem_allocate_memory(bufferSize, J9MEM_CATEGORY_JIT);
   if (!buffer)
      return;

   // ... POPULATE ...
   // ................
   bufferCursorStart = bufferCursor = buffer;
   // Compilation attributes
   bufferCursor = bufferPopulateCompilationAttributes(bufferCursor, compiler, metaData);
   // Inlined call sites table
   bufferCursor = bufferPopulateInlinedCallSites(bufferCursor, compiler, metaData);
   // Linenumber table
   bufferCursor = bufferPopulateLineNumberTable(bufferCursor, compiler, metaData, _method);

   // Call event hook
   if (J9_EVENT_IS_HOOKED(_jitConfig->javaVM->hookInterface, J9HOOK_VM_DYNAMIC_CODE_LOAD))
      ALWAYS_TRIGGER_J9HOOK_VM_DYNAMIC_CODE_LOAD(_jitConfig->javaVM->hookInterface, vmThread, _method, buffer, bufferCursor - buffer, "JIT inlined body", NULL);
   j9mem_free_memory(buffer);
   }

#if defined(JIT_METHODHANDLE_TRANSLATED)
// TODO: Remove this once the VM supplies this declaration
extern J9_CFUNC void  jitMethodHandleTranslated (J9VMThread *currentThread, j9object_t methodHandle, j9object_t arg, void *jitEntryPoint, void *intrpEntryPoint);
#endif


// static method
void *
TR::CompilationInfo::compilationEnd(J9VMThread * vmThread, TR::IlGeneratorMethodDetails & details, J9JITConfig *jitConfig, void *startPC,
                                   void *oldStartPC, TR_FrontEnd *fe, TR_MethodToBeCompiled *entry, TR::Compilation *comp)
   {

   if (!comp)
      return NULL;

   // This method is only called with both VMAccess and CompilationMutex in hand.
   // Performs some necessary updates once a compilation has been attempted
   //
   PORT_ACCESS_FROM_JAVAVM(jitConfig->javaVM);
   TR_DataCache *dataCache = NULL;
   TR::CompilationInfo *compInfo = TR::CompilationInfo::get();

   if (details.isNewInstanceThunk())
      {
      if (comp->getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER)
         {
         J9::NewInstanceThunkDetails &mhDetails = static_cast<J9::NewInstanceThunkDetails &>(details);
         J9Class *clazz = mhDetails.classNeedingThunk();

         outOfProcessCompilationEnd(entry, comp);
         }
      else
         {
         J9::NewInstanceThunkDetails &mhDetails = static_cast<J9::NewInstanceThunkDetails &>(details);
         J9Class *clazz = mhDetails.classNeedingThunk();

         if (startPC)
            jitNewInstanceMethodTranslated     (vmThread, clazz, startPC);
         else
            jitNewInstanceMethodTranslateFailed(vmThread, clazz);
         }

      if (((jitConfig->runtimeFlags & J9JIT_TOSS_CODE) ||
           (compInfo->getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER)) &&
          comp)
         {
         if (jitConfig->runtimeFlags & J9JIT_TOSS_CODE)
            jitConfig->codeCache->heapAlloc = jitConfig->codeCache->heapBase;
         dataCache = (TR_DataCache*)comp->getReservedDataCache();
         if (dataCache)
            {
            dataCache->resetAllocationToMark();
            // TODO: make sure we didn't allocate a new dataCache (the mark was set in the old cache)
            TR_DataCacheManager::getManager()->makeDataCacheAvailable(dataCache);
            comp->setReservedDataCache(NULL);
            }
         }

      return startPC;
      }

   if (details.isMethodInProgress())
      {
#ifdef J9VM_JIT_DYNAMIC_LOOP_TRANSFER
      if (startPC)
         {
         if (comp->getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER)
            outOfProcessCompilationEnd(entry, comp);
         else
            {
            J9::MethodInProgressDetails & dltDetails = static_cast<J9::MethodInProgressDetails &>(details);
            TR::CompilationInfo *compInfo = TR::CompilationInfo::get();
            compInfo->insertDLTRecord(dltDetails.getMethod(), dltDetails.getByteCodeIndex(), startPC);
            jitMarkMethodReadyForDLT(vmThread, dltDetails.getMethod());
            }
         }
      else if (entry && entry->isOutOfProcessCompReq())
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
                  "compThreadID=%d has failed to compile a DLT method", entry->_compInfoPT->getCompThreadId());
            }
         int8_t compErrCode = entry->_compErrCode;
         if (compErrCode != compilationStreamInterrupted && compErrCode != compilationStreamFailure)
            entry->_stream->writeError(compErrCode);
         }
#endif // ifdef J9VM_JIT_DYNAMIC_LOOP_TRANSFER
      if (((jitConfig->runtimeFlags & J9JIT_TOSS_CODE) ||
           (compInfo->getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER)) &&
          comp)
         {
         if (jitConfig->runtimeFlags & J9JIT_TOSS_CODE)
            jitConfig->codeCache->heapAlloc = jitConfig->codeCache->heapBase;
         dataCache = (TR_DataCache *)comp->getReservedDataCache();
         if (dataCache)
            {
            dataCache->resetAllocationToMark();
            // TODO: make sure we didn't allocate a new dataCache (the mark was set in the old cache)
            TR_DataCacheManager::getManager()->makeDataCacheAvailable(dataCache);
            comp->setReservedDataCache(NULL);
            }
         }
      return startPC;
      }

   if (!fe)
      fe = TR_J9VMBase::get(jitConfig, vmThread);

   TR_J9VMBase *trvm = (TR_J9VMBase *)fe;

   if (details.isMethodHandleThunk())
      {
      if (startPC)
         {
         if (comp->getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER)
            {
            outOfProcessCompilationEnd(entry, comp);
            }
         else
            {
            J9::MethodHandleThunkDetails &mhDetails = static_cast<J9::MethodHandleThunkDetails &>(details);
            uintptrj_t thunks = trvm->getReferenceField(*mhDetails.getHandleRef(), "thunks", "Ljava/lang/invoke/ThunkTuple;");
            int64_t jitEntryPoint = (int64_t)(intptrj_t)startPC + TR_LinkageInfo::get(startPC)->getJitEntryOffset();

#if defined(JIT_METHODHANDLE_TRANSLATED)
            jitMethodHandleTranslated(vmThread, *mhDetails.getHandleRef(), mhDetails.getArgRef()? *mhDetails.getArgRef() : NULL, jitEntryPoint, startPC);
#else
            // This doesn't need to be volatile.  On 32-bit, we don't care about
            // word-tearing because it's a 32-bit address; on 64-bit, we don't get
            // word-tearing of aligned 64-bit stores on any platform we care about.
            //
            trvm->setInt64Field(thunks, "invokeExactThunk",    jitEntryPoint);
            trvm->setInt64Field(thunks, "i2jInvokeExactThunk", (intptrj_t)startPC);
#endif
            deleteMethodHandleRef(mhDetails, vmThread, trvm);
            }
         }
      else
         {
         // TODO:JSR292: Handle compile failures gracefully
         if (entry && entry->isOutOfProcessCompReq())
            {
            int8_t compErrCode = entry->_compErrCode;
            if (compErrCode != compilationStreamInterrupted && compErrCode != compilationStreamFailure)
               entry->_stream->writeError(compErrCode);
            }
         }
      return startPC;
      }

   J9Method *method = details.getMethod();

   if (startPC) // if successful compilation
      {
      //if (vmThread)
      TR_ASSERT(vmThread, "We must always have a vmThread in compilationEnd()\n");
      if (comp->getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER)
         {
         outOfProcessCompilationEnd(entry, comp);
         }
      else if (!(jitConfig->runtimeFlags & J9JIT_TOSS_CODE))
         {
         if (trvm->isAOT_DEPRECATED_DO_NOT_USE() && !entry->isRemoteCompReq())
            {
            // Committing AOT compilation that succeeded
            // We want to store AOT code in the cache
#if defined(J9VM_INTERP_AOT_COMPILE_SUPPORT) && defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM))
            if (TR::Options::sharedClassCache())
               {
               J9JITExceptionTable *relocatedMetaData = NULL;
               const U_8 *dataStart;
               const U_8 *codeStart;
               UDATA dataSize, codeSize;

               TR_ASSERT(comp, "AOT compilation that succeeded must have a compilation object");
               J9JITDataCacheHeader *aotMethodHeader      = (J9JITDataCacheHeader *)comp->getAotMethodDataStart();
               TR_ASSERT(aotMethodHeader, "The header must have been set");
               TR_AOTMethodHeader   *aotMethodHeaderEntry = (TR_AOTMethodHeader *)(aotMethodHeader + 1);

               dataStart = (U_8 *)aotMethodHeaderEntry->compileMethodDataStartPC;
               dataSize  = aotMethodHeaderEntry->compileMethodDataSize;
               codeStart = (U_8 *)aotMethodHeaderEntry->compileMethodCodeStartPC;
               codeSize  = aotMethodHeaderEntry->compileMethodCodeSize;

               aotMethodHeaderEntry->compileFirstClassLocation = (UDATA)jitConfig->javaVM->sharedClassConfig->cacheDescriptorList->romclassStartAddress;
               J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);

               TR::CompilationInfo::storeAOTInSharedCache(
                  vmThread,
                  romMethod,
                  dataStart,
                  dataSize,
                  codeStart,
                  codeSize,
                  comp,
                  jitConfig,
                  entry
                  );

#if defined(J9VM_INTERP_AOT_RUNTIME_SUPPORT)

               bool canRelocateMethod = TR::CompilationInfo::canRelocateMethod(comp);

               if (canRelocateMethod)
                  {
                  J9JITDataCacheHeader *cacheEntry;

                  TR_ASSERT_FATAL(comp->cg(), "CodeGenerator must be allocated");

                  // Use same code cache as AOT compile
                  //
                  TR::CodeCache *aotMCCRuntimeCodeCache = comp->cg()->getCodeCache();
                  TR_ASSERT(aotMCCRuntimeCodeCache, "Must have a reserved codeCache");
                  cacheEntry = (J9JITDataCacheHeader *)dataStart;

                  // If compilation and/or slot monitors are held, they must now be released in order to call the
                  // AOT relocation.
                  //
                  if (entry->getMonitor())
                     {
                     compInfo->debugPrint(vmThread, "\treleasing queue-slot monitor\n");
                     compInfo->debugPrint(vmThread, "-AM\n");
                     entry->releaseSlotMonitor(vmThread);
                     }

                  compInfo->debugPrint(vmThread, "\treleasing compilation monitor\n");
                  compInfo->debugPrint(vmThread, "-CM\n");
                  compInfo->releaseCompMonitor(vmThread);
                  int32_t returnCode = 0;

                  if (TR::Options::getVerboseOption(TR_VerboseCompilationDispatch))
                     {
                     TR_VerboseLog::writeLineLocked(
                        TR_Vlog_DISPATCH,
                        "Applying relocations to newly AOT compiled body for %s @ %s",
                        comp->signature(),
                        comp->getHotnessName()
                        );
                     }
                  try
                     {
                     TR::CompilationInfoPerThreadBase::InterruptibleOperation(*entry->_compInfoPT);
                     // need to get a non-shared cache VM to relocate
                     TR_J9VMBase *fe = TR_J9VMBase::get(jitConfig, vmThread);
                     TR_ResolvedMethod *compilee = fe->createResolvedMethod(comp->trMemory(), (TR_OpaqueMethodBlock *)method);
                     relocatedMetaData = entry->_compInfoPT->reloRuntime()->prepareRelocateAOTCodeAndData(
                        vmThread,
                        fe,
                        aotMCCRuntimeCodeCache,
                        cacheEntry,
                        method,
                        true,
                        comp->getOptions(),
                        comp,
                        compilee
                        );
                     returnCode = entry->_compInfoPT->reloRuntime()->returnCode();
                     }
                  catch (std::exception &e)
                     {
                     // Relocation Failure
                     returnCode = compilationAotRelocationInterrupted;
                     }

                  // Reacquire the compilation and/or slot monitors
                  //
                  compInfo->debugPrint(vmThread, "\tacquiring compilation monitor\n");
                  compInfo->acquireCompMonitor(vmThread);
                  compInfo->debugPrint(vmThread, "+CM\n");

                  if (entry->getMonitor())
                     {
                     // Acquire the queue slot monitor now
                     //
                     compInfo->debugPrint(vmThread, "\tre-acquiring queue-slot monitor\n");
                     entry->acquireSlotMonitor(vmThread);
                     compInfo->debugPrint(vmThread, "+AM-", entry);
                     }

                  if (relocatedMetaData)
                     {
                     if (TR::Options::getVerboseOption(TR_VerboseCompilationDispatch))
                        {
                        TR_VerboseLog::writeLineLocked(TR_Vlog_DISPATCH, "Successfully relocated metadata for %s", comp->signature());
                        }

                     startPC = reinterpret_cast<void *>(relocatedMetaData->startPC);

                     if (J9_EVENT_IS_HOOKED(jitConfig->javaVM->hookInterface, J9HOOK_VM_DYNAMIC_CODE_LOAD))
                        {
                        TR::CompilationInfo::addJ9HookVMDynamicCodeLoadForAOT(vmThread, method, jitConfig, relocatedMetaData);
                        }
                     jitMethodTranslated(vmThread, method, startPC);
                     }
                  else
                     {
                     entry->_doNotUseAotCodeFromSharedCache = true;
                     entry->_compErrCode = returnCode;
                     startPC = NULL;

                     if (entry->_compilationAttemptsLeft > 0)
                        {
                        entry->_tryCompilingAgain = true;
                        }

                     if (TR::Options::isAnyVerboseOptionSet(TR_VerboseCompileEnd, TR_VerbosePerformance, TR_VerboseCompFailure))
                        {
                        TR_VerboseLog::writeLineLocked(TR_Vlog_FAILURE,
                                                          "Failure while relocating for %s, return code = %d [%s]\n",
                                                          comp->signature(),
                                                          returnCode,
                                                          (returnCode >= 0) && (returnCode < compilationMaxError) ? compilationErrorNames[returnCode] : "unknown error");
                        }
                     }
                  }

               /*
                * Method cannot be relocated or relocation has failed.
                */
               if (!canRelocateMethod || !relocatedMetaData)
                  {
                  // Must reclaim code cache, metadata, jittedBodyInfo, persistentMethodInfo,
                  // assumptions and RI records that could have been allocated.
                  //
                  J9JITDataCacheHeader *aotMethodHeader      = (J9JITDataCacheHeader *)comp->getAotMethodDataStart();
                  TR_AOTMethodHeader   *aotMethodHeaderEntry = (TR_AOTMethodHeader *)(aotMethodHeader + 1);
                  J9JITDataCacheHeader *cacheEntry = (J9JITDataCacheHeader *)aotMethodHeaderEntry->compileMethodDataStartPC;
                  J9JITDataCacheHeader *exceptionTableCacheEntry = (J9JITDataCacheHeader *)((U_8 *)cacheEntry + aotMethodHeaderEntry->offsetToExceptionTable);
                  J9JITExceptionTable *metaData = (J9JITExceptionTable *) (exceptionTableCacheEntry + 1);

                  // The exception table is inserted in the AVL trees during the relocation
                  // Since we didn't performed the relocation, we don't have to artifactManager->removeArtifact(metaData);

                  // Delete any assumptions that might still exist in persistent memory
                  // The metadata parameter is NULL meaning that we want to delete ALL assumptions, including those for JBI
                  compInfo->getPersistentInfo()->getRuntimeAssumptionTable()->reclaimAssumptions(comp->getMetadataAssumptionList(), NULL);
                  metaData->runtimeAssumptionList = NULL;

                  // reclaim code memory so we can use it for something else
                  TR::CodeCacheManager::instance()->addFreeBlock(static_cast<void *>(metaData), reinterpret_cast<uint8_t *>(metaData->startPC));

                  metaData->constantPool = 0; // mark metadata as unloaded

                  TR_DataCache *dataCache = (TR_DataCache*)comp->getReservedDataCache();
                  TR_ASSERT(dataCache, "A dataCache must be reserved for AOT compilations\n");
                  // Make sure we didn't allocate a new dataCache (the mark was set in the old cache)
                  TR_ASSERT(dataCache->getAllocationMark() == (uint8_t*)comp->getAotMethodDataStart(),
                     "AllocationMark=%p does not match aotMethodDataStart=%p",
                     dataCache->getAllocationMark(), comp->getAotMethodDataStart());
                  dataCache->resetAllocationToMark();
                  // Reservation will be cancelled at end of compilation

                  // Inform that metadata is now NULL
                  if (compInfo->getCompInfoForCompOnAppThread())
                     {
                     compInfo->getCompInfoForCompOnAppThread()->setMetadata(NULL);
                     }
                  else
                     {
                     if (entry->_compInfoPT)
                        entry->_compInfoPT->setMetadata(NULL);
                     }
                  // mark that compilation failed
                  startPC = oldStartPC;
                  }
#else
               /* If in the unlikely circumstance that AOT runtime support is off but AOT compilation occurred,
                * we should fail the compilation */
               jitMethodFailedTranslation(vmThread, method);
#endif /* J9VM_INTERP_AOT_RUNTIME_SUPPORT */
               }
            else
#endif // defined(J9VM_INTERP_AOT_COMPILE_SUPPORT) && defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM))
               {
#ifdef AOT_DEBUG
               fprintf(stderr, "ERROR: AOT compiling but shared class is not available. Method will run interpreted\n");
#endif
               TR_ASSERT(0, "shared classes flag not enabled yet we got an AOT compilation"); // This is a branch that is never expected to enter
               entry->_doNotUseAotCodeFromSharedCache = true;
               startPC = NULL;

               if (entry->_compilationAttemptsLeft > 0)
                  {
                  entry->_tryCompilingAgain = true;
                  }

               if (TR::Options::isAnyVerboseOptionSet(TR_VerboseCompileEnd, TR_VerbosePerformance, TR_VerboseCompFailure))
                  {
                  TR_VerboseLog::writeLineLocked(TR_Vlog_INFO,"Shared Class not available for AOT compile for %s", comp->signature());
                  }
               }
            }
         else // Non-AOT compilation and JITServer remote JIT/AOT compilations
            {
            if (trvm->isAOT_DEPRECATED_DO_NOT_USE() && !TR::CompilationInfo::canRelocateMethod(comp))
               {
               // Handle the case when relocations are delayed.
               // Delete any assumptions that might still exist in persistent memory
               // The metadata parameter is NULL meaning that we want to delete ALL assumptions, including those for JBI
               J9JITExceptionTable *metaData = (J9JITExceptionTable *) comp->getAotMethodDataStart();
               if (metaData)
                  {
                  compInfo->getPersistentInfo()->getRuntimeAssumptionTable()->reclaimAssumptions(comp->getMetadataAssumptionList(), NULL);
                  metaData->runtimeAssumptionList = NULL;

                  metaData->constantPool = 0; // mark metadata as unloaded
                  }

               if (entry->_compInfoPT)
                  entry->_compInfoPT->setMetadata(NULL);
               startPC = oldStartPC;
               }
            else
               {
               jitMethodTranslated(vmThread, method, startPC);
               }
            }
         }

      // If this is a retranslation, fix up the old method to forward to the new one
      //
      if (oldStartPC)
         {
         if (TR::Options::getVerboseOption(TR_VerboseCodeCache))
            {
            OMR::CodeCacheMethodHeader *oldMethodHeader = getCodeCacheMethodHeader((char*)oldStartPC, 32, NULL);
            OMR::CodeCacheMethodHeader *recompiledMethodHeader = getCodeCacheMethodHeader((char*)startPC, 32, NULL);
            UDATA oldEndPC = oldMethodHeader ? oldMethodHeader->_metaData->endPC :  -1;
            UDATA endPC = recompiledMethodHeader ? recompiledMethodHeader->_metaData->endPC : -1;
            int oldMethodSize = oldMethodHeader ? (oldEndPC - (UDATA)oldStartPC + 1) : -1;
            int methodSize = recompiledMethodHeader ? (endPC - (UDATA)startPC + 1) : -1;

            TR_VerboseLog::writeLineLocked(TR_Vlog_INFO,"Recompile %s @ " POINTER_PRINTF_FORMAT "-" POINTER_PRINTF_FORMAT "(%d bytes) -> " POINTER_PRINTF_FORMAT "-" POINTER_PRINTF_FORMAT "(%d bytes)", comp->signature(), oldStartPC, oldEndPC, oldMethodSize, startPC, endPC, methodSize);
            }

         TR::Recompilation::methodHasBeenRecompiled(oldStartPC, startPC, trvm);
         }
      }
   else if (!oldStartPC)
      {
      // Tell the VM that a non-compiled method failed translation
      //
      if (vmThread && entry && !entry->isOutOfProcessCompReq())
         jitMethodFailedTranslation(vmThread, method);

      if (entry && entry->isOutOfProcessCompReq())
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
                  "compThreadID=%d has failed to compile", entry->_compInfoPT->getCompThreadId());
            }
         static bool breakAfterFailedCompile = feGetEnv("TR_breakAfterFailedCompile") != NULL;
         if (breakAfterFailedCompile)
            {
            fprintf(stderr, "\n=== Failed to compile %s  ===\n", comp->signature());
            TR::Compiler->debug.breakPoint();
            }
         int8_t compErrCode = entry->_compErrCode;
         if (compErrCode != compilationStreamInterrupted && compErrCode != compilationStreamFailure)
            entry->_stream->writeError(compErrCode);
         }
      }
   else
      {
      // If this is a retranslation, fix up the old method to not try to recompile
      // again and return the oldStartPC as the new startPC so the old (fixed up)
      // method body is run
      //
      if (entry && !entry->isOutOfProcessCompReq())
         TR::Recompilation::methodCannotBeRecompiled(oldStartPC, trvm);
      startPC = oldStartPC;

      if (entry && entry->isOutOfProcessCompReq())
         {
         if (TR::Options::getVerboseOption(TR_VerboseJITServer))
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer,
                  "compThreadID=%d has failed to recompile", entry->_compInfoPT->getCompThreadId());
            }
         int8_t compErrCode = entry->_compErrCode;
         if (compErrCode != compilationStreamInterrupted && compErrCode != compilationStreamFailure)
            entry->_stream->writeError(compilationNotNeeded);
         }
      }

   if (((jitConfig->runtimeFlags & J9JIT_TOSS_CODE) ||
       (compInfo->getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER)) &&
       comp)
      {
      if (jitConfig->runtimeFlags & J9JIT_TOSS_CODE)
         jitConfig->codeCache->heapAlloc = jitConfig->codeCache->heapBase;
      dataCache = (TR_DataCache *)comp->getReservedDataCache();
      if (dataCache)
         {
         dataCache->resetAllocationToMark();
         // TODO: make sure we didn't allocate a new dataCache (the mark was set in the old cache)
         TR_DataCacheManager::getManager()->makeDataCacheAvailable(dataCache);
         comp->setReservedDataCache(NULL);
         }
      }

   return startPC;
   }

void
TR::CompilationInfo::setProcessorByDebugOption()
   {
   if (debug("rios1"))
      TR::Compiler->target.cpu.setProcessor(TR_PPCrios1);
   else if (debug("rios2"))
      TR::Compiler->target.cpu.setProcessor(TR_PPCrios2);
   else if (debug("pwr403"))
      TR::Compiler->target.cpu.setProcessor(TR_PPCpwr403);
   else if (debug("pwr405"))
      TR::Compiler->target.cpu.setProcessor(TR_PPCpwr405);
   else if (debug("pwr601"))
      TR::Compiler->target.cpu.setProcessor(TR_PPCpwr601);
   else if (debug("pwr603"))
      TR::Compiler->target.cpu.setProcessor(TR_PPCpwr603);
   else if (debug("pwr604"))
      TR::Compiler->target.cpu.setProcessor(TR_PPCpwr604);
   else if (debug("pwr630"))
      TR::Compiler->target.cpu.setProcessor(TR_PPCpwr630);
   else if (debug("pwr620"))
      TR::Compiler->target.cpu.setProcessor(TR_PPCpwr620);
   else if (debug("nstar"))
      TR::Compiler->target.cpu.setProcessor(TR_PPCnstar);
   else if (debug("pulsar"))
      TR::Compiler->target.cpu.setProcessor(TR_PPCpulsar);
   else if (debug("gp"))
      TR::Compiler->target.cpu.setProcessor(TR_PPCgp);
   else if (debug("gpul"))
      TR::Compiler->target.cpu.setProcessor(TR_PPCgpul);
   else if (debug("gr"))
      TR::Compiler->target.cpu.setProcessor(TR_PPCgr);
   else if (debug("p6"))
      TR::Compiler->target.cpu.setProcessor(TR_PPCp6);
   else if (debug("p7"))
      TR::Compiler->target.cpu.setProcessor(TR_PPCp7);
   else if (debug("p8"))
      TR::Compiler->target.cpu.setProcessor(TR_PPCp8);
   else if (debug("p9"))
      TR::Compiler->target.cpu.setProcessor(TR_PPCp9);
   else if (debug("440GP"))
      TR::Compiler->target.cpu.setProcessor(TR_PPCpwr440);
   else if (debug("750FX"))
      TR::Compiler->target.cpu.setProcessor(TR_PPC7xx);
   }


void
TR::CompilationInfo::addJ9HookVMDynamicCodeLoadForAOT(J9VMThread* vmThread, J9Method* method, J9JITConfig* jitConfig, TR_MethodMetaData* relocatedMetaData)
   {
   OMR::CodeCacheMethodHeader *ccMethodHeader;
   ALWAYS_TRIGGER_J9HOOK_VM_DYNAMIC_CODE_LOAD(
      jitConfig->javaVM->hookInterface,
      vmThread,
      method,
      reinterpret_cast<void *>(relocatedMetaData->startPC),
      relocatedMetaData->endWarmPC - relocatedMetaData->startPC,
      "JIT warm body",
      relocatedMetaData
      );
   if (relocatedMetaData->startColdPC)
      {
      // Register the cold code section too
      //
      ALWAYS_TRIGGER_J9HOOK_VM_DYNAMIC_CODE_LOAD(
         jitConfig->javaVM->hookInterface,
         vmThread,
         method,
         reinterpret_cast<void *>(relocatedMetaData->startColdPC),
         relocatedMetaData->endPC - relocatedMetaData->startColdPC,
         "JIT cold body",
         relocatedMetaData
         );
      }
   ccMethodHeader = getCodeCacheMethodHeader(
      reinterpret_cast<char *>(relocatedMetaData->startPC),
      32,
      relocatedMetaData
      );
   if (ccMethodHeader && (relocatedMetaData->bodyInfo != NULL))
      {
      TR_LinkageInfo *linkageInfo = TR_LinkageInfo::get(reinterpret_cast<void *>(relocatedMetaData->startPC));
      if (linkageInfo->isRecompMethodBody())
         {
         ALWAYS_TRIGGER_J9HOOK_VM_DYNAMIC_CODE_LOAD(
            jitConfig->javaVM->hookInterface,
            vmThread,
            method,
            ccMethodHeader->_eyeCatcher + 4,
            relocatedMetaData->startPC - reinterpret_cast<uintptr_t>(ccMethodHeader->_eyeCatcher + 4),
            "JIT method header",
            relocatedMetaData
            );
         }
      }
   }

// Ensure that only methods whose name (prefix) matches names in
// the translation filter list are compiled.
//
bool
TR::CompilationInfoPerThreadBase::methodCanBeCompiled(TR_Memory *trMemory, TR_FrontEnd * vm, TR_ResolvedMethod* method, TR_FilterBST * & filter)
   {
   filter = NULL;

   static char *dontCompileNatives = feGetEnv("TR_DontCompile");

   if (dontCompileNatives && (method->isNewInstanceImplThunk() || method->isJNINative()))
      {
      printf("don't compile because JNI or thunk\n");
      return false;
      }

   if (!method->isCompilable(trMemory))
      return false;

   char * methodName = method->nameChars();
   UDATA methodNameLen = method->nameLength();
   char * methodSig = method->signatureChars();
   UDATA methodSigLen = method->signatureLength();

   if (!(_jitConfig->runtimeFlags & J9JIT_COMPILE_CLINIT) &&
       methodNameLen == 8 && !J9OS_STRNCMP(methodName, "<clinit>", 8))
      return false;

   if (_jitConfig->bcSizeLimit && (method->maxBytecodeIndex() > _jitConfig->bcSizeLimit))
      return false;

   // if we've translated something that has transformed a newInstanceImpl call to virtual call
   // to a thunk.  Then we have to compile the thunk.
   //
   if (method->isNewInstanceImplThunk())
      {
      TR_J9VMBase *fej9 = (TR_J9VMBase *)vm;
      // Do not compile thunks for arrays
      return !fej9->isClassArray(method->classOfMethod());
      }

   if (!TR::Options::getDebug())
      return true;

   return TR::Options::getDebug()->methodCanBeCompiled(trMemory, method, filter);
   }

void TR::CompilationInfoPerThreadBase::logCompilationSuccess(
   J9VMThread * vmThread,
   TR_J9VMBase &vm,
   J9Method * method,
   TR::SegmentAllocator const &scratchSegmentProvider,
   TR_ResolvedMethod * compilee,
   TR::Compilation * compiler,
   TR_MethodMetaData * metaData,
   TR_OptimizationPlan * optimizationPlan
   )
   {
   if (!_methodBeingCompiled->isAotLoad())
      {
      J9JavaVM * javaVM = _jitConfig->javaVM;
      // Dump mixed mode disassembly listing.
      //
      if (compiler->getOutFile() != NULL &&
          (compiler->getOption(TR_TraceAll) || compiler->getOptions()->getTraceCGOption(TR_TraceCGMixedModeDisassembly)))
         compiler->getDebug()->dumpMixedModeDisassembly();

      if (!vm.isAOT_DEPRECATED_DO_NOT_USE())
         {
         if (J9_EVENT_IS_HOOKED(javaVM->hookInterface, J9HOOK_VM_DYNAMIC_CODE_LOAD))
            {
            OMR::CodeCacheMethodHeader *ccMethodHeader;
            ALWAYS_TRIGGER_J9HOOK_VM_DYNAMIC_CODE_LOAD(javaVM->hookInterface, vmThread, method, (U_8*)metaData->startPC, metaData->endWarmPC - metaData->startPC, "JIT warm body", metaData);

            if (TR::Options::getCmdLineOptions()->getOption(TR_EnableJVMPILineNumbers))
               {
               _compInfo.emitJvmpiCallSites(compiler, vmThread, method);
               _compInfo.emitJvmpiExtendedDataBuffer(compiler, vmThread, method, metaData);
               }

            if (metaData->startColdPC)
               {
               // Register the cold code section too
               //
               ALWAYS_TRIGGER_J9HOOK_VM_DYNAMIC_CODE_LOAD(javaVM->hookInterface, vmThread, method, (U_8*)metaData->startColdPC, metaData->endPC - metaData->startColdPC, "JIT cold body", metaData);
               }
            ccMethodHeader = getCodeCacheMethodHeader((char *)metaData->startPC, 32, metaData);
            if (ccMethodHeader && (metaData->bodyInfo != NULL))
               {
               TR_LinkageInfo *linkageInfo = TR_LinkageInfo::get((void *)metaData->startPC);
               if (linkageInfo->isRecompMethodBody())
                  {
                  ALWAYS_TRIGGER_J9HOOK_VM_DYNAMIC_CODE_LOAD(javaVM->hookInterface, vmThread, method, (void *)((char*)ccMethodHeader->_eyeCatcher+4), metaData->startPC - (UDATA)((char*)ccMethodHeader->_eyeCatcher+4), "JIT method header", metaData);
                  }
               }
            }
         }

      PORT_ACCESS_FROM_JITCONFIG(_jitConfig);
      UDATA translationTime = j9time_usec_clock() - getTimeWhenCompStarted();
      if (TR::Options::_largeTranslationTime > 0 && translationTime > (UDATA)(TR::Options::_largeTranslationTime))
         {
         if (compiler->getOutFile() != NULL)
            trfprintf(compiler->getOutFile(), "Compilation took %d usec\n", (int32_t)translationTime);
         compiler->dumpMethodTrees("Post optimization trees for large computing method");
         }
      if (_onSeparateThread)
         {
         TR::CompilationInfoPerThread *cipt = (TR::CompilationInfoPerThread*)this;
         cipt->setLastCompilationDuration(translationTime / 1000);
         }

      UDATA gcDataBytes = _jitConfig->lastGCDataAllocSize;
      UDATA atlasBytes = _jitConfig->lastExceptionTableAllocSize;

      // Statistics for number of aoted methods that were recompiled
      //
      if (_methodBeingCompiled->_oldStartPC != 0) // this is a recompilation
         {
         TR_PersistentJittedBodyInfo *oldBodyInfo = TR::Recompilation::getJittedBodyInfoFromPC(_methodBeingCompiled->_oldStartPC);
         // because this is a recompilation, the persistentJittedBodyInfo must exist
         if (oldBodyInfo->getIsAotedBody())
            _compInfo._statNumAotedMethodsRecompiled++;

         // Statistics about Recompilations for bodies with Jprofiling
         if (oldBodyInfo->getUsesJProfiling())
            {
            if (compiler->getRecompilationInfo() &&
                compiler->getRecompilationInfo()->getJittedBodyInfo()->getMethodInfo()->getReasonForRecompilation() == TR_PersistentMethodInfo::RecompDueToJProfiling)
               _compInfo._statNumRecompilationForBodiesWithJProfiling++;
            }
         }
      // Statistics for compilations from LPQ
      if (_methodBeingCompiled->_reqFromSecondaryQueue)
         _compInfo.getLowPriorityCompQueue().incStatsCompFromLPQ(_methodBeingCompiled->_reqFromSecondaryQueue);

      // Statistics about SamplingJProfiling bodies
      if (compiler->getRecompilationInfo() &&
          compiler->getRecompilationInfo()->getJittedBodyInfo()->getUsesSamplingJProfiling())
         _compInfo._statNumSamplingJProfilingBodies++;

      // Statistics about JProfiling bodies
      if (compiler->getRecompilationInfo() &&
          compiler->getRecompilationInfo()->getJittedBodyInfo()->getUsesJProfiling())
         _compInfo._statNumJProfilingBodies++;

      // Statistics about number of methods taken from the queue with JProfiling requests
      if (_methodBeingCompiled->_reqFromJProfilingQueue)
         _compInfo._statNumMethodsFromJProfilingQueue++;

      if (_jitConfig->runtimeFlags & J9JIT_TESTMODE)
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, "JIT %s OK\n", compiler->signature());
         }
      else
         {
         char compilationTypeString[15]={0};
         snprintf(compilationTypeString, sizeof(compilationTypeString), "%s%s",
                 vm.isAOT_DEPRECATED_DO_NOT_USE() ? "AOT " : "",
                 compiler->isProfilingCompilation() ? "profiled " : ""
                );

         UDATA startPC = 0;
         UDATA endWarmPC = 0;
         UDATA startColdPC = 0;
         UDATA endPC = 0;
         if (metaData)
            {
            startPC = metaData->startPC;
            startColdPC = metaData->startColdPC;
            endWarmPC = metaData->endWarmPC;
            endPC = metaData->endPC;
            }

         TR_Hotness h = compiler->getMethodHotness();
         if (h < numHotnessLevels)
            _compInfo._statsOptLevels[(int32_t)h]++;
         if (compilee->isJNINative())
            _compInfo._statNumJNIMethodsCompiled++;
         const char * hotnessString = compiler->getHotnessName(h);
         TR_ASSERT(hotnessString, "expected to have a hotness string");

         const char * prexString = compiler->usesPreexistence() ? " prex" : "";

         int32_t profilingCount = 0, profilingFrequencey = 0, counter = 0;
         if (compiler->isProfilingCompilation())
            {
            TR::Recompilation *recompInfo = compiler->getRecompilationInfo();
            profilingCount      = recompInfo->getProfilingCount();
            profilingFrequencey = recompInfo->getProfilingFrequency();
            counter             = recompInfo->getJittedBodyInfo()->getCounter(); // profiling invocation count
            }

         char recompReason = '-';
         // For recompiled methods try to find the reason for recompilation
         if (_methodBeingCompiled->_oldStartPC)
            {
            TR_PersistentJittedBodyInfo *bodyInfo = TR::Recompilation::getJittedBodyInfoFromPC(_methodBeingCompiled->_oldStartPC);
            if (bodyInfo->getIsInvalidated())
               {
               recompReason = 'I'; // method was invalidated
               }
            else
               {
               switch (bodyInfo->getMethodInfo()->getReasonForRecompilation())
                  {
                  case TR_PersistentMethodInfo::RecompDueToThreshold:
                     recompReason = 'T'; break;
                  case TR_PersistentMethodInfo::RecompDueToCounterZero:
                     recompReason = 'Z'; break;
                  case TR_PersistentMethodInfo::RecompDueToMegamorphicCallProfile:
                     recompReason = 'M'; break;
                  case TR_PersistentMethodInfo::RecompDueToOptLevelUpgrade:
                     recompReason = 'C'; break;
                  case TR_PersistentMethodInfo::RecompDueToSecondaryQueue:
                     recompReason = 'S'; break;
                  case TR_PersistentMethodInfo::RecompDueToForcedAOTUpgrade:
                     recompReason = 'A'; break;
                  case TR_PersistentMethodInfo::RecompDueToRI:
                     recompReason = 'R'; break;
                  case TR_PersistentMethodInfo::RecompDueToJProfiling:
                     recompReason = 'J'; break;
                  case TR_PersistentMethodInfo::RecompDueToInlinedMethodRedefinition:
                     recompReason = 'H'; break;
                  case TR_PersistentMethodInfo::RecompDueToGCR:
                     if (compiler->getOption(TR_NoOptServer))
                        recompReason = 'g';// Guarded counting recompilation
                     else
                        recompReason = 'G';// Guarded counting recompilation
                     _compInfo._statNumGCRInducedCompilations++;
                     break;
                     // TODO: add here the EDO case if we ever modify the codegen to track that
                  } // end switch
               bodyInfo->getMethodInfo()->setReasonForRecompilation(0); // reset the flags
               }
            }

         // If set, print verbose compile results, gc/exception, and/or time.
         if (TR::Options::isAnyVerboseOptionSet(TR_VerboseCompileEnd, TR_VerboseGc, TR_VerboseRecompile, TR_VerbosePerformance, TR_VerboseOptimizer)
         || (compiler->getOption(TR_CountOptTransformations) && compiler->getVerboseOptTransformationCount() >= 1))
            {
            const uint32_t bytecodeSize = TR::CompilationInfo::getMethodBytecodeSize(method);
            const bool isJniNative = compilee->isJNINative();
            TR_VerboseLog::vlogAcquire();
            TR_VerboseLog::writeLine(TR_Vlog_COMP,"(%s%s) %s @ " POINTER_PRINTF_FORMAT "-" POINTER_PRINTF_FORMAT,
               compilationTypeString,
               hotnessString,
               compiler->signature(),
               startPC,
               startColdPC ? endWarmPC : endPC
               );
            if (startColdPC)
               {
               TR_VerboseLog::write("/" POINTER_PRINTF_FORMAT "-" POINTER_PRINTF_FORMAT, startColdPC, endPC);
               }

            j9jit_printf(_jitConfig, " %s", _methodBeingCompiled->getMethodDetails().name());

            // Print recompilation reason
            // For methods compiled through sample thresholds, print also the CPU utilization
            if (recompReason == 'T')
               TR_VerboseLog::write(" %.2f%%", optimizationPlan->getPerceivedCPUUtil() / 10.0);

            TR_VerboseLog::write(" %c", recompReason);

            TR_VerboseLog::write(" Q_SZ=%d Q_SZI=%d QW=%d", _compInfo.getMethodQueueSize(),
                                 _compInfo.getNumQueuedFirstTimeCompilations(), _compInfo.getQueueWeight());

            TR_VerboseLog::write(" j9m=%p bcsz=%u", method, bytecodeSize);

            if (_compInfo.useSeparateCompilationThread() && !_methodBeingCompiled->_async)
               TR_VerboseLog::write(" sync"); // flag the synchronous compilations

            if (isJniNative)
               TR_VerboseLog::write(" JNI"); // flag JNI compilations

            if (compiler->getOption(TR_EnableOSR))
               TR_VerboseLog::write(" OSR");

            if (compiler->getRecompilationInfo() && compiler->getRecompilationInfo()->getJittedBodyInfo()->getUsesGCR())
               TR_VerboseLog::write(" GCR");

            if (compiler->getRecompilationInfo() && (compiler->getRecompilationInfo()->getJittedBodyInfo()->getUsesSamplingJProfiling() || compiler->getRecompilationInfo()->getJittedBodyInfo()->getUsesJProfiling()))
               TR_VerboseLog::write(" JPROF");

            if (compiler->isDLT())
               TR_VerboseLog::write(" DLT@%d", compiler->getDltBcIndex());

            if (_methodBeingCompiled->_reqFromSecondaryQueue)
               TR_VerboseLog::write(" LPQ");

            if (_methodBeingCompiled->_reqFromJProfilingQueue)
               TR_VerboseLog::write(" JPQ");

            if (_methodBeingCompiled->isRemoteCompReq())
               TR_VerboseLog::write(" remote");

            if (TR::Options::getVerboseOption(TR_VerboseGc))
               {
               TR_VerboseLog::write(" gc=%d atlas=%d", gcDataBytes, atlasBytes);
               }

            if (TR::Options::getVerboseOption(TR_VerbosePerformance))
               TR_VerboseLog::write(" time=%dus", translationTime);

            if(TR::Options::getVerboseOption(TR_VerbosePerformance))
               {
               TR_VerboseLog::write(
                  " mem=[region=%llu system=%llu]KB",
                  static_cast<unsigned long long>(scratchSegmentProvider.regionBytesAllocated())/1024,
                  static_cast<unsigned long long>(scratchSegmentProvider.systemBytesAllocated())/1024
                  );
               }

#if defined(WINDOWS) && defined(TR_TARGET_32BIT)
            if (TR::Options::getVerboseOption(TR_VerboseVMemAvailable))
               {
               J9MemoryInfo memInfo;
               PORT_ACCESS_FROM_JITCONFIG(_jitConfig);
               if (j9sysinfo_get_memory_info(&memInfo) == 0 &&
               memInfo.availVirtual != J9PORT_MEMINFO_NOT_AVAILABLE)
                  TR_VerboseLog::write( " VMemAv=%u MB", (uint32_t)(memInfo.availVirtual >> 20));
               }
#endif
            if (TR::Options::getVerboseOption(TR_VerboseRecompile))
               {
               TR_VerboseLog::write("%s [profiling c(%d), f(%d), ivc(%d)]", prexString, profilingCount, profilingFrequencey, counter);
               }

            if (compiler->getOption(TR_CountOptTransformations) && compiler->getOption(TR_VerboseOptTransformations))
               {
               TR_VerboseLog::write(" transformations=%d", compiler->getVerboseOptTransformationCount());
               }

            if (TR::Options::getVerboseOption(TR_VerboseOptimizer))
               {
               TR_VerboseLog::write(" opts=%d.%d", compiler->getLastPerformedOptIndex(), compiler->getLastPerformedOptSubIndex());
               }

            if (TR::Options::isAnyVerboseOptionSet(TR_VerboseCompileEnd, TR_VerbosePerformance))
               {
               TR_VerboseLog::write(" compThread=%d",  compiler->getCompThreadID());
               }

            // print cached cpu usage (sampled elsewhere [samplerThreadProc])

            CpuUtilization *cpuUtil = _compInfo.getCpuUtil();
            if (cpuUtil->isFunctional())
               {
               TR_VerboseLog::write(" CpuLoad=%d%%(%d%%avg) JvmCpu=%d%%",
               cpuUtil->getCpuUsage(),
               cpuUtil->getAvgCpuUsage(),
               cpuUtil->getVmCpuUsage());
               }

            if (TR::Options::getVerboseOption(TR_VerboseCompilationThreads) && _onSeparateThread)
               {
               // CPU spent in comp thread is quite coarse (updated every 0.5 sec)
               // We could use getCpuTimeNow() if we wanted to be more accurate
               TR::CompilationInfoPerThread *cipt = (TR::CompilationInfoPerThread*)this;
               int32_t cpuUtil = cipt->getCompThreadCPU().getThreadLastCpuUtil();
               if (cpuUtil >= 0)
                  TR_VerboseLog::write(" compCPU=%d%%", cpuUtil);
               }
            TR_VerboseLog::vlogRelease();
            }

         char compilationAttributes[40] = { 0 };
         snprintf(compilationAttributes, sizeof(compilationAttributes), "%s %s %s %s %s %s %s",
                      prexString,
                      (_compInfo.useSeparateCompilationThread() && !_methodBeingCompiled->_async) ? "sync" : "",
                      compilee->isJNINative()? "JNI" : "",
                      compiler->getOption(TR_EnableOSR) ? "OSR" : "",
                      (compiler->getRecompilationInfo() && compiler->getRecompilationInfo()->getJittedBodyInfo()->getUsesGCR()) ? "GCR" : "",
                      compiler->isDLT() ? "DLT" : "",
                      TR::CompilationInfo::isJSR292(method) ? "JSR292" : ""
                      );
#if defined (_MSC_VER) && _MSC_VER < 1900
         // Microsoft has a compliant version of snprintf only starting with Visual Studio 2015
         // We should add the null terminator just in case
         * (compilationAttributes + sizeof(compilationAttributes)-1) = '\0';
#endif
         
         Trc_JIT_compileEnd15(vmThread, compilationTypeString, hotnessString, compiler->signature(),
                               startPC, endWarmPC, startColdPC, endPC,
                               translationTime, method, metaData,
                               recompReason, _compInfo.getMethodQueueSize(), TR::CompilationInfo::getMethodBytecodeSize(method),
                               static_cast<uint32_t>(
                                    (scratchSegmentProvider.systemBytesAllocated() / 1024) & static_cast<uint32_t>(-1)
                                    ),
                              compilationAttributes);

#ifdef TR_HOST_S390
         if (TR::Options::getVerboseOption(TR_VerboseMMap))
            {
            outputVerboseMMapEntries(compilee, metaData, compilationTypeString, hotnessString);
            }

#endif // ifdef TR_HOST_S390
         }
      }
   }

static void
printCompFailureInfo(TR::Compilation * comp, const char * reason)
   {
   if (comp && comp->getOptions()->getAnyOption(TR_TraceAll))
      traceMsg(comp, "\n=== EXCEPTION THROWN (%s) ===\n", reason);
   }

void
TR::CompilationInfoPerThreadBase::processException(
   J9VMThread *vmThread,
   TR::SegmentAllocator const &scratchSegmentProvider,
   TR::Compilation * compiler,
   volatile bool & haveLockedClassUnloadMonitor,
   const char *exceptionName
   ) throw()
   {
   if (TR::Options::getVerboseOption(TR_VerboseCompilationDispatch))
      {
      TR_VerboseLog::writeLineLocked(
         TR_Vlog_DISPATCH,
         "Failed to finalize compiled body for %s @ %s",
         compiler->signature(),
         compiler->getHotnessName()
         );
      }

   OMR::RuntimeAssumption **metadataAssumptionList = compiler->getMetadataAssumptionList();
   if (metadataAssumptionList && (*metadataAssumptionList))
      {
      compiler->getPersistentInfo()->getRuntimeAssumptionTable()->reclaimAssumptions(metadataAssumptionList, NULL);
      }

   // Defect 194113 - still holding schedule monitor at this point, release it!
   //
   while (((TR::Monitor *)getCompilationInfo()->_schedulingMonitor)->owned_by_self())
      getCompilationInfo()->_schedulingMonitor->exit(); //the if monitor is still held, release

   if (!compiler->getOption(TR_DisableNoVMAccess))
      {
      if (haveLockedClassUnloadMonitor)
         {
         // On the exception path the monitor may not be held
         // Do a test first, otherwise an internal assume will be triggered
         //
         if (TR::MonitorTable::get()->getClassUnloadMonitorHoldCount(getCompThreadId()) > 0)
            TR::MonitorTable::get()->readReleaseClassUnloadMonitor(getCompThreadId());
         }

      if (!(vmThread->publicFlags &  J9_PUBLIC_FLAGS_VM_ACCESS))
         acquireVMAccessNoSuspend(vmThread);
      }

   if (TR::Options::getVerboseOption(TR_VerboseCompYieldStats))
      {
      if (compiler->getMaxYieldInterval() > TR::Options::_compYieldStatsThreshold)
         {
         TR_VerboseLog::vlogAcquire();
         compiler->printCompYieldStats();
         TR_VerboseLog::vlogRelease();
         }
      }

   PORT_ACCESS_FROM_JITCONFIG(_jitConfig);

   bool shouldProcessExceptionCommonTasks = true;

   try
      {
      throw;
      }

   // The catch blocks below are ordered such that in each group of
   // blocks, the least derived type is at the end of the group.

   /* Allocation Exceptions Start */
   catch (const TR::RecoverableCodeCacheError &e)
      {
      if (compiler->compileRelocatableCode())
         _methodBeingCompiled->_compErrCode = compilationAotCacheFullReloFailure;
      else
         _methodBeingCompiled->_compErrCode = compilationRecoverableCodeCacheError;
      }
   catch (const TR::CodeCacheError &e)
      {
      _methodBeingCompiled->_compErrCode = compilationCodeCacheError;
      }
   catch (const J9::RecoverableDataCacheError &e)
      {
      TR_ASSERT(compiler->compileRelocatableCode(),
                "Only AOT compiles should throw a J9::RecoverableDataCacheError exception\n");
      if (compiler->compileRelocatableCode())
         _methodBeingCompiled->_compErrCode = compilationAotCacheFullReloFailure;
      else
         _methodBeingCompiled->_compErrCode = compilationDataCacheError;
      }
   catch (const J9::DataCacheError &e)
      {
      _methodBeingCompiled->_compErrCode = compilationDataCacheError;
      }
   catch (const TR::RecoverableTrampolineError &e)
      {
      _methodBeingCompiled->_compErrCode = compilationRecoverableTrampolineFailure;
      }
   catch (const TR::TrampolineError &e)
      {
      _methodBeingCompiled->_compErrCode = compilationTrampolineFailure;
      }
   catch (const J9::LowPhysicalMemory &e)
      {
      _methodBeingCompiled->_compErrCode = compilationLowPhysicalMemory;
      }
   catch (const std::bad_alloc &e)
      {
      _methodBeingCompiled->_compErrCode = compilationHeapLimitExceeded;
      }
   /* Allocation Exceptions End */

   /* IL Gen Exceptions Start */
   catch (const J9::AOTHasInvokeHandle &e)
      {
      _methodBeingCompiled->_compErrCode = compilationAotHasInvokehandle;
      }
   catch (const J9::AOTHasInvokeVarHandle &e)
      {
      _methodBeingCompiled->_compErrCode = compilationAotHasInvokeVarHandle;
      }
   catch (const J9::FSDHasInvokeHandle &e)
      {
      _methodBeingCompiled->_compErrCode = compilationRestrictedMethod;
      }
   catch (const TR::ILGenFailure &e)
      {
      _methodBeingCompiled->_compErrCode = compilationILGenFailure;
      }
   /* IL Gen Exceptions End */

   /* Runtime Failure Exceptions Start */
   catch (const J9::CHTableCommitFailure &e)
      {
      _methodBeingCompiled->_compErrCode = compilationCHTableCommitFailure;
      }
   catch (const J9::MetaDataCreationFailure &e)
      {
      _methodBeingCompiled->_compErrCode = compilationMetaDataFailure;
      }
   catch (const J9::AOTRelocationFailed &e)
      {
      // no need to set error code here because error code is set
      // in installAotCachedMethod when the relocation failed
      }
   /* Runtime Failure Exceptions End */

   /* Insufficiently Aggressive Compilation Exceptions Start */
   catch (const J9::LambdaEnforceScorching &e)
      {
      _methodBeingCompiled->_compErrCode = compilationLambdaEnforceScorching;
      }
   catch (const J9::EnforceProfiling &e)
      {
      _methodBeingCompiled->_compErrCode = compilationEnforceProfiling;
      }
   catch (const TR::InsufficientlyAggressiveCompilation &e)
      {
      _methodBeingCompiled->_compErrCode = compilationNeededAtHigherLevel;
      }
   /* Insufficiently Aggressive Compilation Exceptions End */

   /* Excessive Complexity Exceptions Start */
   catch (const TR::ExcessiveComplexity &e)
      {
      _methodBeingCompiled->_compErrCode = compilationExcessiveComplexity;
      }
   catch (const TR::MaxCallerIndexExceeded &e)
      {
      _methodBeingCompiled->_compErrCode = compilationMaxCallerIndexExceeded;
      }
   /* Excessive Complexity Exceptions End */

   catch (const TR::CompilationInterrupted &e)
      {
      _methodBeingCompiled->_compErrCode = compilationInterrupted;
      }
   catch (const TR::UnimplementedOpCode &e)
      {
      _methodBeingCompiled->_compErrCode = compilationRestrictedMethod;
      }
   catch (const TR::GCRPatchFailure &e)
      {
      _methodBeingCompiled->_compErrCode = compilationGCRPatchFailure;
      }
   catch (const J9::AOTSymbolValidationManagerFailure &e)
      {
      _methodBeingCompiled->_compErrCode = compilationSymbolValidationManagerFailure;
      }
   catch (const J9::AOTNoSupportForAOTFailure &e)
      {
      _methodBeingCompiled->_compErrCode = compilationAOTNoSupportForAOTFailure;
      }
   catch (const J9::ClassChainPersistenceFailure &e)
      {
      shouldProcessExceptionCommonTasks = false;
      _methodBeingCompiled->_compErrCode = compilationCHTableCommitFailure;
      if (TR::Options::isAnyVerboseOptionSet(TR_VerbosePerformance, TR_VerboseCompileEnd, TR_VerboseCompFailure))
         {
         uintptr_t translationTime = j9time_usec_clock() - getTimeWhenCompStarted(); //get the time it took to fail the compilation
         TR_VerboseLog::writeLineLocked(
            TR_Vlog_COMPFAIL,
            "%s time=%dus %s",
            compiler->signature(),
            translationTime,
            "Class chain persistence failure"
            );
         }
      Trc_JIT_compilationFailed(vmThread, compiler->signature(), -1);
      }
   catch (const TR::AssertionFailure &e)
      {
      shouldProcessExceptionCommonTasks = false;
      if (TR::Options::isAnyVerboseOptionSet(TR_VerbosePerformance, TR_VerboseCompileEnd, TR_VerboseCompFailure))
         {
         uintptr_t translationTime = j9time_usec_clock() - getTimeWhenCompStarted(); //get the time it took to fail the compilation
         // This assertion will only be generated in production, when softFailOnAssumes is set.
         TR_VerboseLog::writeLineLocked(
            TR_Vlog_COMPFAIL,
            "Assertion Failure: %s time=%dus",
            compiler->signature(),
            translationTime
            );
         }
         Trc_JIT_compilationFailed(vmThread, compiler->signature(), -1);
      }
   catch (const JITServer::StreamFailure &e)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "JITServer StreamFailure: %s", e.what());
      _methodBeingCompiled->_compErrCode = compilationStreamFailure;
      }
   catch (const JITServer::StreamInterrupted &e)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "JITServer StreamInterrupted: %s", e.what());
      _methodBeingCompiled->_compErrCode = compilationStreamInterrupted;
      }
   catch (const JITServer::StreamVersionIncompatible &e)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "JITServer StreamVersionIncompatible: %s", e.what());
      _methodBeingCompiled->_compErrCode = compilationStreamVersionIncompatible;
      }
   catch (const JITServer::StreamMessageTypeMismatch &e)
      {
      if (TR::Options::getVerboseOption(TR_VerboseJITServer))
         TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "JITServer StreamMessageTypeMismatch: %s", e.what());
      _methodBeingCompiled->_compErrCode = compilationStreamMessageTypeMismatch;
      }
   catch (const JITServer::ServerCompilationFailure &e)
      {
      // no need to set error code here because error code is set
      // in remoteCompile when the compilation failed
      }
   catch (...)
      {
      _methodBeingCompiled->_compErrCode = compilationFailure;
      }

   if (shouldProcessExceptionCommonTasks)
      processExceptionCommonTasks(vmThread, scratchSegmentProvider, compiler, exceptionName);

   TR::IlGeneratorMethodDetails & details = _methodBeingCompiled->getMethodDetails();
   J9Method *method = details.getMethod();
   TRIGGER_J9HOOK_JIT_COMPILING_END(_jitConfig->hookInterface, vmThread, method);
   }

void
TR::CompilationInfoPerThreadBase::processExceptionCommonTasks(
   J9VMThread *vmThread,
   TR::SegmentAllocator const &scratchSegmentProvider,
   TR::Compilation * compiler,
   const char *exceptionName
   )
   {
   PORT_ACCESS_FROM_JITCONFIG(_jitConfig);

   if (TR::Options::isAnyVerboseOptionSet(TR_VerbosePerformance, TR_VerboseCompileEnd, TR_VerboseCompFailure))
      {
      uintptr_t translationTime = j9time_usec_clock() - getTimeWhenCompStarted(); //get the time it took to fail the compilation
      TR_VerboseLog::vlogAcquire();
      if (_methodBeingCompiled->_compErrCode != compilationFailure)
         {
         if ((_jitConfig->runtimeFlags & J9JIT_TESTMODE) && _methodBeingCompiled->_compErrCode == compilationInterrupted)
            TR_VerboseLog::writeLine(TR_Vlog_FAILURE,"<JIT: translating %s -- Interrupted because of %s", compiler->signature(), exceptionName);
         else
            {
            bool incomplete;
            uint64_t freePhysicalMemorySizeB = _compInfo.computeAndCacheFreePhysicalMemory(incomplete);
            if (freePhysicalMemorySizeB != OMRPORT_MEMINFO_NOT_AVAILABLE)
               {
               TR_VerboseLog::writeLine(TR_Vlog_COMPFAIL,"%s time=%dus %s memLimit=%zu KB freePhysicalMemory=%llu MB",
                                           compiler->signature(),
                                           translationTime,
                                           compilationErrorNames[_methodBeingCompiled->_compErrCode],
                                           scratchSegmentProvider.allocationLimit() >> 10,
                                           freePhysicalMemorySizeB >> 20);
               }
            else
               {
               TR_VerboseLog::writeLine(TR_Vlog_COMPFAIL,"%s time=%dus %s memLimit=%zu KB",
                                           compiler->signature(),
                                           translationTime,
                                           compilationErrorNames[_methodBeingCompiled->_compErrCode],
                                           scratchSegmentProvider.allocationLimit() >> 10);
               }
            }
         }
      else
         {
         uintptr_t translationTime = j9time_usec_clock() - getTimeWhenCompStarted(); //get the time it took to fail the compilation
         TR_VerboseLog::writeLine(TR_Vlog_COMPFAIL,"%s time=%dus %s <TRANSLATION FAILURE: out of scratch memory>",
                                        compiler->signature(),
                                        translationTime,
                                        exceptionName);
         }

      if (TR::Options::getVerboseOption(TR_VerbosePerformance))
         {
         TR_VerboseLog::write(
            " mem=[region=%llu system=%llu]KB",
            static_cast<unsigned long long>(scratchSegmentProvider.regionBytesAllocated())/1024,
            static_cast<unsigned long long>(scratchSegmentProvider.systemBytesAllocated())/1024
            );
         }
      TR_VerboseLog::vlogRelease();
      }

   if(_methodBeingCompiled->_compErrCode == compilationFailure)
      {
      Trc_JIT_outOfMemory(vmThread);
      }
   else
      {
      Trc_JIT_compilationFailed(vmThread, compiler->signature(), -1);
      }
   }

void
TR::CompilationInfo::triggerOrderedCompiles(TR_FrontEnd * f, intptr_t tickCount)
   {

   TR_J9VMBase *fe = (TR_J9VMBase *)f;

   // A sampling point has been reached - see what happened last time at this
   // sampling point and try to emulate it as closely as possible.
   //
   bool logSampling = fe->isLogSamplingSet();
   TR_FilterBST *filter, **prev;
   for (prev = &TR::Options::getDebug()->getCompilationFilters()->samplingPoints, filter = *prev; filter; filter = *prev)
      {
      if (filter->getTickCount() > tickCount)
         break;

      J9Method * ramMethod = (J9Method *)fe->getMethodFromName(filter->getClass(), filter->getName(), filter->getSignature());
      if (!ramMethod)
         {
         // Can't find the ram method - leave the entry in the samplingPoints
         // list.
         //
         prev = filter->getNextSlot();
         continue;
         }

      // If the method is to have its initial invocation count reduced, make
      // sure that is a valid thing to do and then do it
      //
      if (filter->getFilterType() == TR_FILTER_SAMPLE_INTERPRETED)
         {
         if (!isCompiled(ramMethod))
            {
            int32_t count = getInvocationCount(ramMethod);
            if (count > filter->getSampleCount())
               {
               setInvocationCount(ramMethod, count, filter->getSampleCount());
               if (logSampling)
                  TR_VerboseLog::writeLineLocked(TR_Vlog_CR,"(%d) Interpreted %s.%s%s (SIMULATED) --> %d", tickCount, filter->getClass(), filter->getName(), filter->getSignature(), filter->getSampleCount());
               }
            }

         // Remove this entry from the samplingPoints list
         //
         *prev = filter->getNext();
         continue;
         }

      // For a compiled method, mark it for recompilation if it is currently
      // being sampled. If it is being profiled leave it to run its course.
      //
      if (isCompiled(ramMethod))
         {
         void *startPC = TR::CompilationInfo::getJ9MethodStartPC(ramMethod);

         TR_LinkageInfo           *linkageInfo = TR_LinkageInfo::get(startPC);

         if (linkageInfo->isRecompMethodBody())
            {
            // If we've already reached this level of compilation just remove the
            // entry from the samplingPoints list and continue.
            //
            TR_PersistentJittedBodyInfo *bodyInfo = TR::Recompilation::getJittedBodyInfoFromPC(startPC);
            TR_PersistentMethodInfo *methodInfo = bodyInfo->getMethodInfo();
            if (bodyInfo->getHotness() != unknownHotness &&
                bodyInfo->getHotness() >= filter->getSampleLevel())
               {
               *prev = filter->getNext();
               continue;
               }

            // We haven't reached this level of compilation yet - see if we can
            // force it now. If not, leave the entry in the list for next time.
            //
            if (linkageInfo->isSamplingMethodBody() &&
                !TR::Recompilation::isAlreadyBeingCompiled((TR_OpaqueMethodBlock *) methodInfo->getMethodInfo(), startPC, fe))
               {
               methodInfo->setNextCompileLevel((TR_Hotness)filter->getSampleLevel(),
                                               filter->getSampleProfiled());
               // Recompile the method
               //
               bool queued = false;
               if (fe->isAsyncCompilation())
                  fe->startAsyncCompile((TR_OpaqueMethodBlock *) ramMethod, startPC, &queued);
               else
                  TR::Recompilation::fixUpMethodCode(startPC);

               if (logSampling)
                  {
                  TR_VerboseLog::vlogAcquire();
                  TR_VerboseLog::writeLine(TR_Vlog_SAMPLING,"(%d) Compiled %s.%s%s (SIMULATED) recompile at level --> %d", tickCount, filter->getClass(),
                           filter->getName(), filter->getSignature(), filter->getSampleLevel());
                  if (filter->getSampleProfiled())
                     TR_VerboseLog::write(", profiled");
                  TR_VerboseLog::vlogRelease();
                  }

               // Remove this entry from the samplingPoints list
               //
               *prev = filter->getNext();
               continue;
               }
            }
         }

      // Leave this entry in the samplingPoints list
      //
      prev = filter->getNextSlot();
      }
   }

void TR::CompilationInfo::printCompQueue()
   {
   fprintf(stderr, "\nQueue:");
   for (TR_MethodToBeCompiled *cur = _methodQueue; cur; cur = cur->_next)
      {
      fprintf(stderr, " %p", cur);
      }
   fprintf(stderr, "\n");
   }

#if DEBUG
void
TR::CompilationInfo::debugPrint(char * debugString)
   {
   if (!_traceCompiling)
      return;
   fprintf(stderr, debugString);
   }

void
TR::CompilationInfo::debugPrint(J9VMThread * vmThread, char * debugString)
   {
   if (!_traceCompiling)
      return;
   fprintf(stderr, "%8x:", vmThread);
   fprintf(stderr, debugString);
   }

void
TR::CompilationInfo::debugPrint(char * debugString, intptr_t val)
   {
   if (!_traceCompiling)
      return;
   fprintf(stderr, debugString, val);
   }

void
TR::CompilationInfo::debugPrint(J9VMThread * vmThread, char * debugString, IDATA val)
   {
   if (!_traceCompiling)
      return;
   fprintf(stderr, "%8x:", vmThread);
   fprintf(stderr, debugString, val);
   }

void
TR::CompilationInfo::debugPrint(J9Method * method)
   {
   if (!_traceCompiling)
      return;
   if (!method)
      {
      fprintf(stderr, "none");
      return;
      }
   J9UTF8 *className;
   J9UTF8 *name;
   J9UTF8 *signature;
   getClassNameSignatureFromMethod(method, className, name, signature);
   fprintf(stderr, "%.*s.%.*s%.*s", J9UTF8_LENGTH(className), (char *) J9UTF8_DATA(className),
                                    J9UTF8_LENGTH(name), (char *) J9UTF8_DATA(name),
                                    J9UTF8_LENGTH(signature), (char *) J9UTF8_DATA(signature));
   }

void
TR::CompilationInfo::debugPrint(char * debugString, J9Method * method)
   {
   if (!_traceCompiling)
      return;
   fprintf(stderr, "%s for method ", debugString);
   debugPrint(method);
   fprintf(stderr, "\n");
   }

void
TR::CompilationInfo::debugPrint(char * debugString, TR::IlGeneratorMethodDetails & details, J9VMThread * vmThread)
   {
   if (!_traceCompiling)
      return;
   fprintf(stderr, "%8x:", vmThread);
   if (details.isNewInstanceThunk())
      {
      J9Class *classForNewInstance = static_cast<J9::NewInstanceThunkDetails &>(details).classNeedingThunk();
      fprintf(stderr, "%s for newInstanceThunk ", debugString);
      int32_t len;
      TR_J9VMBase * fej9 = TR_J9VMBase::get(_jitConfig, vmThread);
      char * className = fej9->getClassNameChars(fej9->convertClassPtrToClassOffset(classForNewInstance), len);
      fprintf(stderr, "%.*s.newInstancePrototype(Ljava/lang/Class;)Ljava/lang/Object;\n", len, className);
      }
   else
      {
      fprintf(stderr, "%s for method ",debugString);
      debugPrint(details.getMethod());
      fprintf(stderr, "\n");
      }
   }

void
TR::CompilationInfo::debugPrint(J9VMThread * vmThread, char * msg, TR_MethodToBeCompiled *entry)
   {
   if (!_traceCompiling)
      return;
   fprintf(stderr, "%8x:%s%d\n", vmThread, msg, entry->_index);
   }

// This method must be called with the Compilation Monitor in hand
//
void
TR::CompilationInfo::printQueue()
   {
   if (!_traceCompiling)
      return;

   fprintf(stderr, "\t\t\tActive: ");
   bool activeMethods = false;

   for (uint8_t i = 0; i < getNumTotalCompilationThreads(); i++)
      {
      TR::CompilationInfoPerThread *curCompThreadInfoPT = _arrayOfCompilationInfoPerThread[i];
      TR_ASSERT(curCompThreadInfoPT, "a thread's compinfo is missing\n");

      TR_MethodToBeCompiled * methodBeingCompiled = curCompThreadInfoPT->getMethodBeingCompiled();

      if (methodBeingCompiled)
         {
         activeMethods = true;
         fprintf(stderr, "(%4d) %d:", methodBeingCompiled->_numThreadsWaiting, methodBeingCompiled->_index);
         debugPrint(methodBeingCompiled->getMethodDetails().getMethod());
         }
      }
   if (!activeMethods)
      fprintf(stderr, "none");

   for (TR_MethodToBeCompiled *p = _methodQueue; p; p = p->_next)
      {
      fprintf(stderr, "\n\t\t\tQueued: (%4d) %d:", p->_numThreadsWaiting, p->_index);
      debugPrint(p->getMethodDetails().getMethod());
      }
   fprintf(stderr, "\n");
   }
#endif


TR_HWProfiler *
TR::CompilationInfo::getHWProfiler() const
   {
   return ((TR_JitPrivateConfig*)_jitConfig->privateConfig)->hwProfiler;
   }

TR_LMGuardedStorage *
TR::CompilationInfo::getLMGuardedStorage() const
   {
   return ((TR_JitPrivateConfig*)_jitConfig->privateConfig)->lmGuardedStorage;
   }

TR_JProfilerThread *
TR::CompilationInfo::getJProfilerThread() const
   {
   return ((TR_JitPrivateConfig*)_jitConfig->privateConfig)->jProfiler;
   }

int32_t
TR::CompilationInfo::calculateCodeSize(TR_MethodMetaData *metaData)
   {
   int32_t codeSize = 0;

   if (metaData)
      {
      codeSize = ((uint8_t*)metaData->endWarmPC) - (uint8_t*)metaData->startPC;
      if (metaData->startColdPC)
         codeSize += ((uint8_t*)metaData->endPC) - (uint8_t*)metaData->startColdPC;
      }

   return codeSize;
   }

void
TR::CompilationInfo::increaseUnstoredBytes(U_32 aotBytes, U_32 jitBytes)
   {
#if defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM))
   J9SharedClassConfig *scConfig = _jitConfig->javaVM->sharedClassConfig;
   if (scConfig && scConfig->increaseUnstoredBytes)
      scConfig->increaseUnstoredBytes(_jitConfig->javaVM, aotBytes, jitBytes);
#endif
   }


uint64_t
TR::CompilationInfo::computeFreePhysicalMemory(bool &incompleteInfo)
   {
   bool incomplete = false;
   PORT_ACCESS_FROM_JITCONFIG(_jitConfig);

   uint64_t freePhysicalMemory = OMRPORT_MEMINFO_NOT_AVAILABLE;
   J9MemoryInfo memInfo;
   if (0 == j9sysinfo_get_memory_info(&memInfo)
      && memInfo.availPhysical != OMRPORT_MEMINFO_NOT_AVAILABLE
      && memInfo.hostAvailPhysical != OMRPORT_MEMINFO_NOT_AVAILABLE)
      {
      freePhysicalMemory = memInfo.availPhysical;
      uint64_t freeHostPhysicalMemorySizeB = memInfo.hostAvailPhysical;

      if (memInfo.cached != OMRPORT_MEMINFO_NOT_AVAILABLE)
         freePhysicalMemory += memInfo.cached;
      else
         incomplete = !_cgroupMemorySubsystemEnabled;

      if (memInfo.hostCached != OMRPORT_MEMINFO_NOT_AVAILABLE)
         freeHostPhysicalMemorySizeB += memInfo.hostCached;
      else
         incomplete = true;
#if defined(LINUX)
      if (memInfo.buffered != OMRPORT_MEMINFO_NOT_AVAILABLE)
         freePhysicalMemory += memInfo.buffered;
      else
         incomplete = incomplete || !_cgroupMemorySubsystemEnabled;

      if (memInfo.hostBuffered != OMRPORT_MEMINFO_NOT_AVAILABLE)
         freeHostPhysicalMemorySizeB += memInfo.hostBuffered;
      else
         incomplete = true;
#endif
      // If we run in a container, freePhysicalMemory is the difference between
      // the container memory limit and how much physical memory the container used
      // It's possible that on the entire machine there is less physical memory
      // available because other processes have consumed it. Thus, we need to take
      // into account the available physical memory on the host
      if (freeHostPhysicalMemorySizeB < freePhysicalMemory)
         freePhysicalMemory = freeHostPhysicalMemorySizeB;
      }
   else
      {
      incomplete= true;
      freePhysicalMemory = OMRPORT_MEMINFO_NOT_AVAILABLE;
      }

   incompleteInfo = incomplete;
   return freePhysicalMemory;
   }


uint64_t
TR::CompilationInfo::computeAndCacheFreePhysicalMemory(bool &incompleteInfo, int64_t updatePeriodMs)
   {
   if (updatePeriodMs < 0)
      updatePeriodMs = (int64_t)TR::Options::getUpdateFreeMemoryMinPeriod();
   // If the OS ever gave us bad information, avoid future calls
   if (_cachedFreePhysicalMemoryB != OMRPORT_MEMINFO_NOT_AVAILABLE)
      {
      static int64_t lastUpdateTime = 0;
      int64_t crtElapsedTime = getPersistentInfo()->getElapsedTime();

      if (lastUpdateTime == 0
         || (crtElapsedTime - lastUpdateTime) >= updatePeriodMs)
         {
         // time to recompute freePhysicalMemory
         bool incomplete;
         uint64_t freeMem = computeFreePhysicalMemory(incomplete);

         // Cache the computed value for future reference
         // Synchronization issues can be ignored here
         _cachedIncompleteFreePhysicalMemory = incomplete;
         _cachedFreePhysicalMemoryB = freeMem;
         lastUpdateTime = crtElapsedTime;
         }
      }
   incompleteInfo = _cachedIncompleteFreePhysicalMemory;
   return _cachedFreePhysicalMemoryB;
   }


uint64_t
TR::CompilationInfo::computeFreePhysicalLimitAndAbortCompilationIfLow(TR::Compilation *comp,
                                                                      bool &incompleteInfo,
                                                                      size_t sizeToAllocate)
   {
   uint64_t freePhysicalMemorySizeB = computeAndCacheFreePhysicalMemory(incompleteInfo);
   if (OMRPORT_MEMINFO_NOT_AVAILABLE != freePhysicalMemorySizeB)
      {
      bool fail = false;
      // Avoid consuming the last drop of physical memory for JIT (leave 50 MB for emergency)
      // TODO: the emergency could be generated from SWAP
      if (freePhysicalMemorySizeB >= (uint64_t)TR::Options::getSafeReservePhysicalMemoryValue())
         {
         freePhysicalMemorySizeB -= (uint64_t)TR::Options::getSafeReservePhysicalMemoryValue();
         // Fail the compilation now if the remaining physical memory is not enough
         // to perform a warm compilation that may use up to 'sizeToAllocate' memory
         // However, do not fail if information about physical memory is incomplete
         if (!incompleteInfo && freePhysicalMemorySizeB < sizeToAllocate)
            fail = true;
         }
      else
         {
         fail = !incompleteInfo;
         }
      if (fail)
         {
         if (TR::Options::isAnyVerboseOptionSet(TR_VerbosePerformance, TR_VerboseCompileEnd, TR_VerboseCompFailure))
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_FAILURE, "Aborting Compilation: Low On Physical Memory %lld B", freePhysicalMemorySizeB);
            }

         comp->failCompilation<J9::LowPhysicalMemory>("Low Physical Memory");
         }
      }
   return freePhysicalMemorySizeB;
   }


#if defined(J9VM_INTERP_AOT_COMPILE_SUPPORT) && defined(J9VM_OPT_SHARED_CLASSES) && (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM))
void
TR::CompilationInfo::storeAOTInSharedCache(
   J9VMThread *vmThread,
   J9ROMMethod *romMethod,
   const U_8 *dataStart,
   UDATA dataSize,
   const U_8 *codeStart,
   UDATA codeSize,
   TR::Compilation *comp,
   J9JITConfig *jitConfig,
   TR_MethodToBeCompiled *entry
   ) 
   {
   bool safeToStore;
   const J9JITDataCacheHeader *storedCompiledMethod = NULL;
   PORT_ACCESS_FROM_JAVAVM(jitConfig->javaVM);
   TR::CompilationInfo *compInfo = TR::CompilationInfo::get();

   if (static_cast<TR_JitPrivateConfig *>(jitConfig->privateConfig)->aotValidHeader == TR_yes)
      {
      safeToStore = true;
      }
   else if (static_cast<TR_JitPrivateConfig *>(jitConfig->privateConfig)->aotValidHeader == TR_maybe)
      {
      // If validation has been performed, then a header already existed
      // or one was already been created in this JVM
      TR_J9SharedCacheVM *fe = (TR_J9SharedCacheVM *) TR_J9VMBase::get(jitConfig, vmThread, TR_J9VMBase::AOT_VM);
      safeToStore = entry->_compInfoPT->reloRuntime()->storeAOTHeader(jitConfig->javaVM, fe, vmThread);
      }
   else
      {
      safeToStore = false;
      }

   if (safeToStore)
      {
      const U_8 *metadataToStore = dataStart;
      const U_8 *codedataToStore = codeStart;
      int metadataToStoreSize = dataSize;
      int codedataToStoreSize = codeSize;
#ifdef COMPRESS_AOT_DATA
      try 
         {
         if (!comp->getOption(TR_DisableAOTBytesCompression))
            {
            /**
             * For each compiled method data we store in the shareclass cache looks like following.
             * -----------------------------------------------------------------------------------------------------
             * | CompiledMethodWrapper | J9JITDataCacheHeader | TR_AOTMethodHeader | Metadata | Compiled Code Data |
             * -----------------------------------------------------------------------------------------------------
             * 
             * When we compile a method, we send data from J9JITDataCacheHeader to store in the cache.
             * For each method Shared Class Cache API adds CompiledMethodWrapper header which holds the size of data in cache, 
             * as well as J9ROMMethod of compiled method. 
             * Size of the original data is extracted from the TR_AOTMethodHeader while size of compressed data is extracted from
             * CompiledMethodWrapper. 
             * That is why We do not compress header as we can extract the original data size 
             * which is useful information while inflating the method data while loading compiled method. 
             * 
             * Compressed data stored in the cache looks like following
             * ------------------------------------------------------------------------------------------------------------------------------------------------
             * | CompiledMethodWrapper | J9JITDataCacheHeader (UnCompressed)| TR_AOTMethodHeader (UnCompressed)| (Metadata + Compiled Code Data) (Compressed) |
             * ------------------------------------------------------------------------------------------------------------------------------------------------
             *  
             */
            void * originalData = comp->trMemory()->allocateHeapMemory(codeSize+dataSize);
            void * compressedData = comp->trMemory()->allocateHeapMemory(codeSize+dataSize);

            // Copy the metadata and codedata combined into single buffer so that we can compress it together
            memcpy(originalData, dataStart, dataSize);
            memcpy((U_8*)(originalData)+dataSize, codeStart, codeSize);
            int aotMethodHeaderSize = sizeof(J9JITDataCacheHeader)+sizeof(TR_AOTMethodHeader);
            int compressedDataSize = deflateBuffer((U_8*)(originalData)+aotMethodHeaderSize, dataSize+codeSize-aotMethodHeaderSize, (U_8*)(compressedData)+aotMethodHeaderSize, Z_DEFAULT_COMPRESSION);
            if (compressedDataSize != COMPRESSION_FAILED)
               {
               /**
                * We are going to store both metadata and code in contiguous memory in shared class cache.
                * In load run we get the whole buffer from the cache and use the information from header to
                * Get the relocation data and compiled code.
                * Because of this reason we copy both metadata and compiled code in one buffer and deflate it together.
                * TODO: We will always query the shared class cache to get full data for stored AOT compiled method 
                * that is combined meta data and code data. Even if compression of AOT bytes is disabled, we do not need
                * to send two different buffers to store in cache and also as no one queries either code data / metadata for
                * method, clean up the share classs cache API.
                */
               const J9JITDataCacheHeader *aotMethodHeader = reinterpret_cast<const J9JITDataCacheHeader *>(dataStart);
               TR_AOTMethodHeader *aotMethodHeaderEntry = const_cast<TR_AOTMethodHeader *>(reinterpret_cast<const TR_AOTMethodHeader *>(aotMethodHeader + 1));
               aotMethodHeaderEntry->flags |= TR_AOTMethodHeader_CompressedMethodInCache;
               memcpy(compressedData, dataStart, aotMethodHeaderSize);
               metadataToStore = (const U_8*) compressedData;
               metadataToStoreSize = compressedDataSize+aotMethodHeaderSize;
               codedataToStore = NULL;
               codedataToStoreSize = 0;  
               if (TR::Options::getVerboseOption(TR_VerboseAOTCompression))
                  {
                  TR_VerboseLog::writeLineLocked(TR_Vlog_AOTCOMPRESSION, "%s : Compression of method data Successful - Original method size = %d bytes, Compressed Method Size = %d bytes",
                     comp->signature(),
                     dataSize+codeSize, metadataToStoreSize);
                  }
               }
            else
               {
               if (TR::Options::getVerboseOption(TR_VerboseAOTCompression))
                  {
                  TR_VerboseLog::writeLineLocked(TR_Vlog_AOTCOMPRESSION, "!%s : Compression of method data Failed - Original method size = %d bytes",
                     comp->signature(),
                     dataSize+codeSize);
                  }
               }
            }
         }
      catch (const std::bad_alloc &allocationFailure)
         {
         // In case we can not allocate extra memory to hold temp data for compressing, we still the uncompressed data into the cache
         if (TR::Options::getVerboseOption(TR_VerboseAOTCompression))
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_AOTCOMPRESSION, "!%s : Method will not be compressed as necessary memory can not be allocated, Method Size = %d bytes",
               comp->signature(),
               dataSize+codeSize);   
            }   
         }
#endif
      storedCompiledMethod =
         reinterpret_cast<const J9JITDataCacheHeader*>(
            jitConfig->javaVM->sharedClassConfig->storeCompiledMethod(
               vmThread,
               romMethod,
               (const U_8*)metadataToStore,
               metadataToStoreSize,
               (const U_8*)codedataToStore,
               codedataToStoreSize,
               0));
      switch(reinterpret_cast<uintptr_t>(storedCompiledMethod))
         {
         case J9SHR_RESOURCE_STORE_FULL:
            {
            if (jitConfig->javaVM->sharedClassConfig->verboseFlags & J9SHR_VERBOSEFLAG_ENABLE_VERBOSE)
               j9nls_printf( PORTLIB, J9NLS_WARNING,  J9NLS_RELOCATABLE_CODE_STORE_FULL);
            TR_J9SharedCache::setSharedCacheDisabledReason(TR_J9SharedCache::SHARED_CACHE_FULL);
            disableAOTCompilations();
            }
            break;
         case J9SHR_RESOURCE_STORE_ERROR:
            {
            if (jitConfig->javaVM->sharedClassConfig->verboseFlags & J9SHR_VERBOSEFLAG_ENABLE_VERBOSE)
               j9nls_printf( PORTLIB, J9NLS_WARNING,  J9NLS_RELOCATABLE_CODE_STORE_ERROR);
            TR_J9SharedCache::setSharedCacheDisabledReason(TR_J9SharedCache::SHARED_CACHE_STORE_ERROR);
            TR::Options::getAOTCmdLineOptions()->setOption(TR_NoLoadAOT);
            disableAOTCompilations();
            }
         }
      }
   else
      {
      if (TR::Options::getAOTCmdLineOptions()->getVerboseOption(TR_VerboseCompileEnd))
         {
         PORT_ACCESS_FROM_JAVAVM(jitConfig->javaVM);
         TR_VerboseLog::writeLineLocked(TR_Vlog_FAILURE, " Failed AOT cache validation");
         }

      disableAOTCompilations();
      }
   }
#endif

bool
TR::CompilationInfo::canRelocateMethod(TR::Compilation *comp)
   {
   TR_Debug *debug = TR::Options::getDebug();
   bool canRelocateMethod = false;
   if (debug)
      {
      TR_FilterBST *filter = NULL;
      J9UTF8 *className = ((TR_ResolvedJ9Method*)comp->getCurrentMethod())->_className;
      J9UTF8 *name = ((TR_ResolvedJ9Method*)comp->getCurrentMethod())->_name;
      J9UTF8 *signature = ((TR_ResolvedJ9Method*)comp->getCurrentMethod())->_signature;
      char *methodSignature;
      char arr[1024];
      int32_t len = J9UTF8_LENGTH(className) + J9UTF8_LENGTH(name) + J9UTF8_LENGTH(signature) + 3;
      if (len < 1024)
         methodSignature = arr;
      else
         methodSignature = (char *) jitPersistentAlloc(len);

       if (methodSignature)
         {
         sprintf(methodSignature, "%.*s.%.*s%.*s", J9UTF8_LENGTH(className), utf8Data(className), J9UTF8_LENGTH(name), utf8Data(name), J9UTF8_LENGTH(signature), utf8Data(signature));
         //printf("methodSig: %s\n", methodSignature);

          if (debug->methodSigCanBeRelocated(methodSignature, filter))
            canRelocateMethod = true;
         }
      else
         canRelocateMethod = true;


       if (methodSignature && (len >= 1024))
         jitPersistentFree(methodSignature);
      }
   else
      {
      // Prevent the relocation if specific option is given
      if (!comp->getOption(TR_DisableDelayRelocationForAOTCompilations))
         canRelocateMethod = false;
      else
         canRelocateMethod = true;
      }
   return canRelocateMethod;
   }

void
TR::CompilationInfo::replenishInvocationCount(J9Method* method, TR::Compilation* comp)
   {
   // Replenish the counts of the method
   // We are holding the compilation monitor at this point
   //
   J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
   if (!(romMethod->modifiers & J9AccNative))// Never change the extra field of a native method
      {
      int32_t methodVMExtra = TR::CompilationInfo::getJ9MethodVMExtra(method);
      if (methodVMExtra == 1 || methodVMExtra == J9_JIT_QUEUED_FOR_COMPILATION)
         {
         // We want to use high counts unless the user specified counts on the command line
         // or he used useLowerMethodCounts (or Xquickstart)
         int32_t count;
         if (TR::Options::getCountsAreProvidedByUser() || TR::Options::startupTimeMatters() == TR_yes)
            count = getCount(romMethod, comp->getOptions(), comp->getOptions());
         else
            count = J9ROMMETHOD_HAS_BACKWARDS_BRANCHES(romMethod) ? TR_DEFAULT_INITIAL_BCOUNT : TR_DEFAULT_INITIAL_COUNT;

         TR::CompilationInfo::setInvocationCount(method, count);
         if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerbosePerformance))
            {
            // compiler must exist because startPC != NULL
            TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "Reencoding count=%d for %s j9m=%p ", count, comp->signature(), method);
            }
         }
      else
         {
         TR_ASSERT(false, "Unexpected value for method->extra = %p (method=%p)\n", TR::CompilationInfo::getJ9MethodExtra(method), method);
         }
      }
   }

//===========================================================
TR_LowPriorityCompQueue::TR_LowPriorityCompQueue()
   : _firstLPQentry(NULL), _lastLPQentry(NULL), _sizeLPQ(0), _LPQWeight(0),
     _trackingEnabled(false), _spine(NULL), _STAT_compReqQueuedByIProfiler(0), _STAT_conflict(0),
     _STAT_staleScrubbed(0), _STAT_bypass(0), _STAT_compReqQueuedByJIT(0), _STAT_LPQcompFromIprofiler(0),
     _STAT_LPQcompFromInterpreter(0), _STAT_LPQcompUpgrade(0), _STAT_compReqQueuedByInterpreter(0),
     _STAT_numFailedToEnqueueInLPQ(0)
   {
   }

void TR_LowPriorityCompQueue::startTrackingIProfiledCalls(int32_t threshold)
   {
   if (threshold > 0 && _compInfo->getCpuUtil() && _compInfo->getCpuUtil()->isFunctional()) // Need to have support for CPU utilization
      {
      // Must allocate the cache
      _spine = (Entry*)jitPersistentAlloc(HT_SIZE * sizeof(Entry));
      if (_spine)
         {
         memset(_spine, 0, HT_SIZE * sizeof(Entry));
         _threshold = threshold;
         if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerbosePerformance))
            {
            TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, "t=%6u Allocated the LPQ tracking hashtable", (uint32_t)_compInfo->getPersistentInfo()->getElapsedTime());
            }
         FLUSH_MEMORY(TR::Compiler->target.isSMP());
         _trackingEnabled = true;
         }
      }
   }

inline
bool TR_LowPriorityCompQueue::isTrackableMethod(J9Method *j9method) const
   {
   return !TR::CompilationInfo::isCompiled(j9method) && TR::CompilationInfo::getJ9MethodVMExtra(j9method) > 0;
   }

// Search given j9method in the hashtable;
// If method found, increment counter
//   if counter equals threshold, add compilation request to the "Idle queue"
void TR_LowPriorityCompQueue::tryToScheduleCompilation(J9VMThread *vmThread, J9Method *j9method)
   {
   // avoid mocking with startup heuristics
   if ((jitConfig->javaVM->phase != J9VM_PHASE_NOT_STARTUP && !TR::Options::getCmdLineOptions()->getOption(TR_EarlyLPQ)) ||
      // generating more compilations when there is plenty of work is counterproductive
      _compInfo->getNumQueuedFirstTimeCompilations() >= TR::Options::_qsziMaxToTrackLowPriComp ||
      // conserve idle time in the long run
      _compInfo->getPersistentInfo()->getElapsedTime() > 3600000 ||
      // Methods that are already compiled or queued for compilation should not be tracked
      !isTrackableMethod(j9method))
      return;

   // Find the j9method in the hashtable
   Entry *entry = _spine + TR_LowPriorityCompQueue::hash(j9method);
   if (entry->_j9method == (uintptrj_t)j9method) // found match
      {
      if (++entry->_count > _threshold)  // hot entry; possibly queue compilation
         {
         if (!entry->_queuedForCompilation)
            {
            // AOT compilations could be delayed in order to accumulate more
            // IProfiler information. For such methods we don't want to accelerate
            // the compilation. Because it is expensive to identify such methods
            // we only do it when we reached the threshold for compilation
            // TODO: can we actually detect first run?
            if (TR::Options::sharedClassCache() && // must have AOT enabled
                !TR::Options::getCmdLineOptions()->getOption(TR_DisableDelayRelocationForAOTCompilations) &&
               !TR::Options::getAOTCmdLineOptions()->getOption(TR_NoLoadAOT) &&
               vmThread->javaVM->sharedClassConfig->existsCachedCodeForROMMethod(vmThread, J9_ROM_METHOD_FROM_RAM_METHOD(j9method))
               )
               {
               // Invalidate the entry so that another j9method can use it
               entry->setInvalid();
               }
            else
               {
               // Check that the invocation count has been decremented enough to accumulate
               // some IP info and then queue a low priority compilation request
               // We only look at loopy methods because there is a greater chance
               // they will have the invocation count decremented very little.
               bool enoughIPInfo;
               J9ROMMethod * romMethod = (J9ROMMethod*)J9_ROM_METHOD_FROM_RAM_METHOD(j9method);
               bool isLoopy = J9ROMMETHOD_HAS_BACKWARDS_BRANCHES(romMethod);
               // Trying to guess the initial invocation count is error prone and adds overhead
               // We must use some quick and dirty solution. Always assume TR_DEFAULT_INITIAL_BCOUNT
               if (isLoopy)
                  {
                  // For AOT bodies we use scount
                  // For bootstrap romMethods present in SCC (but no AOT body) and loaded during startup, use low counts
                  // For everybody else use high counts
                  int32_t guessedInitialCount = TR::Options::getCountsAreProvidedByUser() ?
                     TR::Options::getCmdLineOptions()->getInitialBCount() :
                     TR_DEFAULT_INITIAL_BCOUNT;
                  int32_t crtInvocationCount = (int32_t)TR::CompilationInfo::getInvocationCount(j9method);
                  enoughIPInfo = (crtInvocationCount + TR::Options::_invocationThresholdToTriggerLowPriComp < guessedInitialCount);
                  }
               else
                  {
                  enoughIPInfo = true;
                  }
               if (enoughIPInfo)
                  {
                  entry->_queuedForCompilation = true;

                  _compInfo->getCompilationMonitor()->enter();
                  bool enqueued = addFirstTimeCompReqToLPQ(j9method, TR_MethodToBeCompiled::REASON_IPROFILER_CALLS);
                  // If conditions allow for processing of an LPQ request
                  // we may have to notify a sleeping thread or activate a new one
                  if (enqueued && _compInfo->canProcessLowPriorityRequest())
                     {
                     if (_compInfo->getNumCompThreadsJobless() > 0)
                        {
                        _compInfo->getCompilationMonitor()->notifyAll();
                        if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCompilationThreads))
                           {
                           TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, "t=%6u LPQ logic waking up a sleeping comp thread. Jobless=%d",
                                                          (uint32_t)_compInfo->getPersistentInfo()->getElapsedTime(),
                                                          _compInfo->getNumCompThreadsJobless());
                           }
                        }
                     else  // At least one thread is active.
                        {
                        // Do we need to activate another compilation thread?
                        int32_t numCompThreadsSuspended = _compInfo->getNumUsableCompilationThreads() - _compInfo->getNumCompThreadsActive();
                        // Do not activate the last suspended thread
                        if (numCompThreadsSuspended > 1)
                           {
                           TR_YesNoMaybe activate = _compInfo->shouldActivateNewCompThread();
                           // If the answer is TR_maybe we may have to look at how many
                           // threads are allowed to work on LPQ requests
                           if (activate == TR_yes ||
                                (activate == TR_maybe &&
                                 TR::Options::getCmdLineOptions()->getOption(TR_ConcurrentLPQ) &&
                                 jitConfig->javaVM->phase == J9VM_PHASE_NOT_STARTUP && // ConcurrentLPQ is too damaging to startup
                                 _compInfo->getNumCompThreadsActive() + 2 < _compInfo->getNumTargetCPUs()
                                )
                              )
                              {
                              TR::CompilationInfoPerThread *compInfoPT = _compInfo->getFirstSuspendedCompilationThread();
                              compInfoPT->resumeCompilationThread();
                              if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCompilationThreads))
                                 {
                                 TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, "t=%6u Activate compThread %d to handle LPQ request. Qweight=%d active=%d",
                                                                (uint32_t)_compInfo->getPersistentInfo()->getElapsedTime(),
                                                                compInfoPT->getCompThreadId(),
                                                                _compInfo->getQueueWeight(),
                                                                _compInfo->getNumCompThreadsActive());
                                 }
                              }
                           }
                        }
                     }
                  _compInfo->getCompilationMonitor()->exit();

                  if (enqueued)
                     {
                     if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCompileRequest))
                        {
                        TR_VerboseLog::writeLineLocked(TR_Vlog_CR, "t=%u Compile request to LPQ for j9m=%p loopy=%d smpl=%u cnt=%d Q_SZ=%d Q_SZI=%d LPQ_SZ=%d CPU=%d%% JVM_CPU=%d%%",
                           (uint32_t)_compInfo->getPersistentInfo()->getElapsedTime(),
                           j9method,
                           isLoopy,
                           entry->_count,
                           (int32_t)TR::CompilationInfo::getInvocationCount(j9method),
                           _compInfo->getMethodQueueSize(),
                           _compInfo->getNumQueuedFirstTimeCompilations(),
                           getLowPriorityQueueSize(),
                           _compInfo->getCpuUtil()->getCpuUsage(),
                           _compInfo->getCpuUtil()->getVmCpuUsage());
                        }
                     }
                  else // could not generate a comp req, maybe because of OOM
                     {
                     entry->setInvalid();
                     }
                  }
               }
            }
         }
      }
   else // no match
      {
      if (entry->isInvalid()) // entry is invalid, so we can reuse it
         {
         entry->initialize(j9method, 1);
         }
      else
         {
         // Scrub for stale entries that hold compiled methods
         // To make sure that entry->_j9method is actually valid we need to purge entries on class unload
         if (!isTrackableMethod((J9Method*)entry->_j9method))
            {
            entry->setInvalid();
            _STAT_staleScrubbed++;
            entry->initialize(j9method, 1);
            }
         else
            {
            _STAT_conflict++;
            // An idea is to push a compilation for the existing entry if it's close to
            // reaching its compilation threshold
            //fprintf(stderr, "Conflict j9method=%p locationID=%u\n", j9method, TR_LowPriorityCompQueue::hash(j9method));
            }
         }
      }
   }


void TR_LowPriorityCompQueue::stopTrackingMethod(J9Method *j9method)
   {
   Entry *entry = _spine + TR_LowPriorityCompQueue::hash(j9method);
   if (entry->_j9method == (uintptrj_t)j9method)
      {
      //TR_ASSERT(entry->_queuedForCompilation, "Entry %p, j9method %p j9m=%p, count=%u queued=%d (should be 1)\n", entry, entry->_j9method, j9method, entry->_count, entry->_queuedForCompilation);
      entry->setInvalid();
      }
   }
// Bad scenario: put method in LPQ. Method gets compiled normally and entry becomes scrubabble.
// Then the same method (running interpreted) finds the same entry usable and stores something in there.


void TR_LowPriorityCompQueue::purgeEntriesOnClassLoaderUnloading(J9ClassLoader *j9classLoader)
   {
   if (isTrackingEnabled())
      {
      TR_ASSERT(_spine, "_spine must non-null once tracking is enabled");
      Entry *endOfHT = _spine + HT_SIZE;
      for (Entry *entry = _spine; entry < endOfHT; entry++)
         {
         if (entry->_j9method && J9_CLASS_FROM_METHOD((J9Method *)entry->_j9method)->classLoader == j9classLoader)
            entry->setInvalid();
         }
      }
   }

void TR_LowPriorityCompQueue::purgeEntriesOnClassRedefinition(J9Class *j9class)
   {
   if (isTrackingEnabled())
      {
      TR_ASSERT(_spine, "_spine must non-null once tracking is enabled");
      Entry *endOfHT = _spine + HT_SIZE;
      for (Entry *entry = _spine; entry < endOfHT; entry++)
         {
         if (entry->_j9method && J9_CLASS_FROM_METHOD((J9Method *)entry->_j9method) == j9class)
            entry->setInvalid();
         }
      }
   }

void TR_LowPriorityCompQueue::incStatsCompFromLPQ(uint8_t reason)
   {
   switch (reason) {
      case TR_MethodToBeCompiled::REASON_IPROFILER_CALLS:
         _STAT_LPQcompFromIprofiler++; break;
      case TR_MethodToBeCompiled::REASON_LOW_COUNT_EXPIRED:
         _STAT_LPQcompFromInterpreter++; break;
      case TR_MethodToBeCompiled::REASON_UPGRADE:
         _STAT_LPQcompUpgrade++; break;
      default:
         TR_ASSERT(false, "No other known reason for LPQ compilations\n");
      }
   }

void TR_LowPriorityCompQueue::incStatsReqQueuedToLPQ(uint8_t reason)
   {
   switch (reason) {
      case TR_MethodToBeCompiled::REASON_IPROFILER_CALLS:
         _STAT_compReqQueuedByIProfiler++; break;
      case TR_MethodToBeCompiled::REASON_LOW_COUNT_EXPIRED:
         _STAT_compReqQueuedByInterpreter++; break;
      case TR_MethodToBeCompiled::REASON_UPGRADE:
         _STAT_compReqQueuedByJIT++; break;
      default:
         TR_ASSERT(false, "No other known reason for LPQ compilations\n");
      }
   }

void TR_LowPriorityCompQueue::printStats() const
   {
   fprintf(stderr, "Stats for LPQ:\n");

   fprintf(stderr, "   Requests for LPQ = %4u (Sources: IProfiler=%3u Interpreter=%3u JIT=%3u)\n",
      _STAT_compReqQueuedByIProfiler + _STAT_compReqQueuedByInterpreter + _STAT_compReqQueuedByJIT,
      _STAT_compReqQueuedByIProfiler, _STAT_compReqQueuedByInterpreter, _STAT_compReqQueuedByJIT);
   fprintf(stderr, "   Comps.  from LPQ = %4u (Sources: IProfiler=%3u Interpreter=%3u JIT=%3u)\n",
      _STAT_LPQcompFromIprofiler + _STAT_LPQcompFromInterpreter + _STAT_LPQcompUpgrade,
      _STAT_LPQcompFromIprofiler, _STAT_LPQcompFromInterpreter, _STAT_LPQcompUpgrade);

   fprintf(stderr, "   Conflicts        = %4u (tried to cache j9method that didn't have space)\n", _STAT_conflict);
   fprintf(stderr, "   Stale entries    = %4u\n", _STAT_staleScrubbed); // we want very few of these, hopefully 0
   fprintf(stderr, "   Bypass ocurrences= %4u (normal comp req hapened before the fast LPQ comp req)\n", _STAT_bypass);
   }

void TR_LowPriorityCompQueue::invalidateRequestsForUnloadedMethods(J9Class * unloadedClass)
   {
   TR_MethodToBeCompiled *cur = _firstLPQentry;
   TR_MethodToBeCompiled *prev = NULL;
   bool verbose = TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseHooks);
   while (cur)
      {
      TR_MethodToBeCompiled *next = cur->_next;
      TR::IlGeneratorMethodDetails & details = cur->getMethodDetails();
      J9Method *method = details.getMethod();
      if (method)
         {
         if (J9_CLASS_FROM_METHOD(method) == unloadedClass ||
            details.isNewInstanceThunk() &&
            static_cast<J9::NewInstanceThunkDetails &>(details).isThunkFor(unloadedClass))
            {
            TR_ASSERT(cur->_priority < CP_SYNC_MIN,
               "Unloading cannot happen for SYNC requests");
            if (verbose)
               TR_VerboseLog::writeLineLocked(TR_Vlog_HK, "Invalidating compile request from LPQ for method=%p class=%p", method, unloadedClass);

            // detach from queue
            if (prev)
               prev->_next = cur->_next;
            else
               _firstLPQentry = cur->_next;
            if (cur == _lastLPQentry)
               _lastLPQentry = prev;
            _sizeLPQ--;
            decreaseLPQWeightBy(cur->_weight);

            // put back into the pool
            _compInfo->recycleCompilationEntry(cur);
            }
         else
            {
            prev = cur;
            }
         }
      cur = next;
      }
   }

void TR_LowPriorityCompQueue::purgeLPQ()
   {
   while (_firstLPQentry)
      {
      TR_MethodToBeCompiled *cur = _firstLPQentry;
      _firstLPQentry = _firstLPQentry->_next;
      _sizeLPQ--;
      decreaseLPQWeightBy(cur->_weight);
      _compInfo->recycleCompilationEntry(cur);
      }
   _lastLPQentry = NULL;
   }

TR_MethodToBeCompiled * TR_LowPriorityCompQueue::extractFirstLPQRequest()
   {
   // take the top method out of the queue
   TR_MethodToBeCompiled *m = _firstLPQentry;
   _firstLPQentry = _firstLPQentry->_next;
   _sizeLPQ--;
   decreaseLPQWeightBy(m->_weight);
   if (!_firstLPQentry)
      _lastLPQentry = 0;
   return m;
   }

// Searches the entire LPQ queue for an entry with the given method and given "reason"
// If method is not found we return NULL.
// If given method is found but the "reason" does not match, the entry is returned
// and the "dequeued" flag is set to false to indicate that the entry is still part of the LPQ
// If method is found and the "reason" matches, the entry is detached and returned to the caller
// while the "dequeued" flag is set to true.
// Needs compilation queue monitor in hand
TR_MethodToBeCompiled *
TR_LowPriorityCompQueue::findAndDequeueFromLPQ(TR::IlGeneratorMethodDetails &details,
                                               uint8_t reason, TR_J9VMBase *fe, bool & dequeued)
   {
   dequeued = false;
   TR_MethodToBeCompiled *cur = _firstLPQentry, *prev = NULL;
   while (cur)
      {
      if (cur->getMethodDetails().sameAs(details, fe))
         {
         if (cur->_reqFromSecondaryQueue == reason)
            {
            // detach from queue
            if (prev)
               prev->_next = cur->_next;
            else
               _firstLPQentry = cur->_next;
            if (cur == _lastLPQentry)
               _lastLPQentry = prev;
            _sizeLPQ--;
            decreaseLPQWeightBy(cur->_weight);
            dequeued = true;
            break;
            }
         else
            {
            break; // Will return un-detached entry
            }
         }

      prev = cur;
      cur = cur->_next;
      }

   return cur;
   }

TR_CompilationErrorCode
TR::CompilationInfo::scheduleLPQAndBumpCount(TR::IlGeneratorMethodDetails &details, TR_J9VMBase *fe)
   {
   J9Method *method = details.getMethod();
   // Search the LPQ and if not found, add method to LPQ and bump invocation count
   // If method is found, move it to main queue
   // We prevent concurrency issues by making sure the invocation count is 0 when adding to LPQ
   // We must make sure that if the method is not found in LPQ there is absolutely no way
   // it can be present in main queue (invocation count being 0 should guarantee us that
   // because a method waiting in main queue should be marked QUEUED_FOR_COMPILATION)
   // We should put an assert that all ordinary async first time compilations in the main
   // queue are marked QUEUED_FOR_COMPILATION
   // To summarize: methods in LPQ can have any invocation count, but methods
   // in main queue must have an invocation count of QUEUED_FOR_COMPILATION
   intptrj_t count = getInvocationCount(method);
   if (count == 0)
      {
      bool dequeued = false;
      TR_MethodToBeCompiled *entry = getLowPriorityCompQueue().findAndDequeueFromLPQ(details, TR_MethodToBeCompiled::REASON_LOW_COUNT_EXPIRED, fe, dequeued);
      if (!dequeued) // Early compilation request
         {
         // Place request in LPQ if it doesn't already exist
         bool queued = true;
         if (entry)
            {
            // Change the reason so that when the counter expires again we move the entry to the main queue
            entry->_reqFromSecondaryQueue = TR_MethodToBeCompiled::REASON_LOW_COUNT_EXPIRED;
            }
         else // try to queue
            {
            queued = getLowPriorityCompQueue().addFirstTimeCompReqToLPQ(method, TR_MethodToBeCompiled::REASON_LOW_COUNT_EXPIRED);
            }
         if (queued)
            {
            // We must replenish the invocation count
            J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
            TR_ASSERT((romMethod->modifiers & J9AccNative) == 0, "Should not change invocation count for natives");

            // Decide what value to add
            // TODO: look at options sets

            intptrj_t newCount = getCount(romMethod, TR::Options::getCmdLineOptions(), TR::Options::getAOTCmdLineOptions());
            newCount = newCount * (100 - TR::Options::_countPercentageForEarlyCompilation) / 100; // TODO: verify we don't have negative numbers
            if (TR::CompilationInfo::setInvocationCount(method, count, newCount))
               {
               if (TR::Options::getJITCmdLineOptions()->getVerboseOption(TR_VerboseCompileRequest))
                  TR_VerboseLog::writeLineLocked(TR_Vlog_CR, "j9m=%p     Enqueued in LPQ. LPQ_SZ=%d. Count-->%d",
                     method, getLowPriorityCompQueue().getLowPriorityQueueSize(), (int)newCount);
               return compilationNotNeeded;
               }
            else
               {
               // We couldn't set the count maybe because another thread set it to another value
               // Very unlikely to happen
               // Take the request out from LPQ and move it to main queue
               bool removed;
               TR_MethodToBeCompiled *req = getLowPriorityCompQueue().findAndDequeueFromLPQ(details, TR_MethodToBeCompiled::REASON_LOW_COUNT_EXPIRED, fe, removed);
               if (req && removed)
                  {
                  recycleCompilationEntry(req); // Put the request back into the pool
                  }
               else
                  {
                  TR_ASSERT(false, "We must have found the entry we just added");
                  // TODO: add some stats for this case
                  }
               }
            }
         else // Couldn't queue to LPQ
            {
            // We must attempt to put it in the main queue
            getLowPriorityCompQueue().incNumFailuresToEnqueue();
            }
         }
      else // Invocation counter expired for a second time
         {
         // Delete this entry and create another one for main queue
         recycleCompilationEntry(entry); // Put the request back into the pool
         }
      }
   else // count is not 0
      {
      // This case can happen if two threads call compileMethod() at about the same time
      // The first thread may put a request in LPQ and increase the count
      // The second thread should just ignore the request.
      // We have to make sure that there is no other part of the code
      // that increases the invocation count
      // Release the compilation lock before returning
      return compilationNotNeeded;
      }

   return compilationOK;
   }


void TR_JitSampleInfo::update(uint64_t crtTime, uint32_t crtGlobalSampleCounter)
   {
   if (crtTime > _timestampOfLastInterval) // avoid races
      {
      // Compute size of last observation interval
      _sizeOfLastInterval = crtTime - _timestampOfLastInterval;
      // Compute nr of samples seen in last observation interval
      uint32_t diffSamples = crtGlobalSampleCounter - _globalSampleCounterInLastInterval;
      // Compute density of samples
      _samplesPerSecondDuringLastInterval = diffSamples * 1000 / _sizeOfLastInterval;
      // Prepare for next interval
      _timestampOfLastInterval = crtTime;
      _globalSampleCounterInLastInterval = crtGlobalSampleCounter;

      if (_samplesPerSecondDuringLastInterval > _maxSamplesPerSecond) // update maximum
         {
         _maxSamplesPerSecond = _samplesPerSecondDuringLastInterval;
         // May have to change thresholds
         // For 0-199 samples/sec we want a threshold of 30
         // Anything above that will increase the window by 30 for each extra 400 samples/sec
         uint32_t increaseFactor = 1;
         if (_maxSamplesPerSecond >= TR::Options::_sampleDensityBaseThreshold)
            {
            increaseFactor = 2 + ((_maxSamplesPerSecond - TR::Options::_sampleDensityBaseThreshold) / TR::Options::_sampleDensityIncrementThreshold);
            }
         if (increaseFactor != _increaseFactor)
            {
            // Change the _increasefactor, but watch for overflow on 8 bit  (Observation window must fit on 8 bits)
            uint32_t maxIncreaseFactor = 0xff / TR::Options::_sampleInterval;
            if (increaseFactor > maxIncreaseFactor)
               increaseFactor = maxIncreaseFactor;
            _increaseFactor = increaseFactor;
            }
         }
      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseSampleDensity))
         TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, "t=%6u globalSamplesDensity: %u/%u=%u samples/sec  max=%u samples/sec increaseFactor=%u",
            crtTime, diffSamples, _sizeOfLastInterval, _samplesPerSecondDuringLastInterval, _maxSamplesPerSecond, _increaseFactor);
      }
   }


void TR_InterpreterSamplingTracking::addOrUpdate(J9Method *method, int32_t cnt)
   {
   // get the compilation queue monitor
   OMR::CriticalSection mon(_compInfo->getCompilationMonitor());

   // search my method in the container
   TR_MethodCnt *item = _container;
   for (; item; item = item->_next)
      {
      if (item->_method == method)
         {
         item->_skippedCount += cnt; // update the skipped count
         break;
         }
      }
   // If method not in the collection yet, add it now
   if (!item)
      {
      item = new (PERSISTENT_NEW) TR_MethodCnt(method, cnt);
      if (item)
         {
         item->_next = _container;
         _container = item;
         _size++;
         if (_size > _maxElements)
            {
            _maxElements = _size;
            //fprintf(stderr, "Max size=%u\n", _maxElements);
            // _maxElements is about 60 for Liberty startup with no SCC/AOT
            }
         }
      }
   // compilation queue monitor automatically released
   }

int32_t TR_InterpreterSamplingTracking::findAndDelete(J9Method *method)
   {
   int32_t retValue = 0;
   TR_MethodCnt *itemToBeDeleted = NULL;

   { // Open a new context
   // get the compilation queue monitor.
   OMR::CriticalSection mon(_compInfo->getCompilationMonitor());
   TR_MethodCnt *item = _container;
   // search my method in the container
   for (TR_MethodCnt *prev = NULL; item; prev = item, item = item->_next)
      {
      if (item->_method == method)
         {
         // detach
         if (prev)
            prev->_next = item->_next;
         else
            _container = item->_next;
         retValue = item->_skippedCount;
         itemToBeDeleted = item;
         _size--;
         break;
         }
      }
   }
   // If element was found, free the memory
   if (itemToBeDeleted)
      jitPersistentFree(itemToBeDeleted);
   return retValue;
   }


J9Method_HT::HT_Entry::HT_Entry(J9Method *j9method, uint64_t timestamp)
   :_next(NULL), _j9method(j9method), _count(TR::CompilationInfo::getInvocationCount(j9method)), _seqID(0), _timestamp(timestamp) {}

J9Method_HT::J9Method_HT(TR::PersistentInfo *persistentInfo)
   {
   memset(_spine, 0, HT_SIZE * sizeof(HT_Entry*));
   _persistentInfo = persistentInfo;
   _numEntries = 0;
   }

J9Method_HT::HT_Entry * J9Method_HT::find(J9Method *j9method) const
   {
   // Search DLT_HT for this method
   size_t bucketID = hash(j9method) & MASK;
   HT_Entry *entry = _spine[bucketID];
   for (; entry; entry = entry->_next)
      if (entry->_j9method == j9method)
         break;
   return entry;
   }


// onClassUnloading is executed when all threads are stopped
// so there are no synchronization issues
void J9Method_HT::onClassUnloading(J9ClassLoader *j9classLoader)
   {
   // Scan the entire DLT_HT and delete entries matching the given classloader
   // Also free invalid entries that have j9method==NULL
   for (size_t bucketID = 0; bucketID < HT_SIZE; bucketID++)
      {
      HT_Entry *entry = _spine[bucketID];
      HT_Entry *prev = NULL;
      for (; entry; prev = entry, entry = entry->_next)
         {
         if (NULL == entry->_j9method
            || J9_CLASS_FROM_METHOD(entry->_j9method)->classLoader == j9classLoader)
            {
            if (prev)
               prev->_next = entry->_next;
            else
               _spine[bucketID] = entry->_next;
            entry->_next = NULL; // for safety
            jitPersistentFree(entry);
            _numEntries--;
            }
         }
      }
   }

bool J9Method_HT::addNewEntry(J9Method *j9method, uint64_t timestamp)
   {
   bool added = false;
   bool alreadyCompiled = TR::CompilationInfo::isCompiled(j9method);

   if (_numEntries < 1000 // prevent pathological cases
      && !alreadyCompiled)
      {
      size_t bucketID = hash(j9method) & MASK;
      HT_Entry * newEntry = new (PERSISTENT_NEW) HT_Entry(j9method, timestamp);
      if (newEntry) // test for OOM
         {
         newEntry->_next = _spine[bucketID];
         if (newEntry->_count < 0)
            newEntry->_count = 0;
         FLUSH_MEMORY(TR::Compiler->target.isSMP());
         _spine[bucketID] = newEntry;
         _numEntries++; // benign multithreading issue
         added = true;
         }
      }
   if (TR::Options::getVerboseOption(TR_VerboseSampling))
      TR_VerboseLog::writeLineLocked(TR_Vlog_SAMPLING, "t=%6u J9MethodTracking: j9m=%p Adding new entry. compiled:%d success=%d totalEntries=%u",
         (unsigned)getPersistentInfo()->getElapsedTime(), j9method, alreadyCompiled, added, _numEntries);
   return added;
   }





bool DLTTracking::shouldIssueDLTCompilation(J9Method *j9method, int32_t numHitsInDLTBuffer)
   {
   bool shouldIssueDLT = false;
   int32_t methodCount = TR::CompilationInfo::getInvocationCount(j9method);
   if (methodCount <= 0 || numHitsInDLTBuffer >= TR::Options::_numDLTBufferMatchesToEagerlyIssueCompReq)
      {
      if (TR::Options::getVerboseOption(TR_VerboseSampling))
         TR_VerboseLog::writeLineLocked(TR_Vlog_SAMPLING, "t=%6u DLTTracking: j9m=%p Issue DLT because methodCount=%d numHitsInDLTBuffer=%d",
            (unsigned)getPersistentInfo()->getElapsedTime(), j9method, methodCount, numHitsInDLTBuffer);
      return true;
      }
   // Search DLT_HT for this method
   HT_Entry *entry = find(j9method);

   if (entry) // if method already exists in DLT_HT
      {
      // If the method's invocation count is the same as the one in the
      // entry, the thread could be stuck in this body. If we encounter
      // this situation several times in succession, we must issue a DLT request
      int32_t entryCount = entry->_count;

      if (entryCount == methodCount)
         {
         entry->_seqID += 1;
         if (entry->_seqID >= TR::Options::_dltPostponeThreshold)
            {
            shouldIssueDLT = true;
            // TODO: should we invalidate this entry?
            }
         }
      else
         {
         // If the invocation count has shrunk since the last inspection
         // we must avoid issuing a DLT request because it's possible that
         // some threads have actually left the method (they are not stuck)
         if (methodCount < entryCount)
            {
            // Update the entry count with the invocation count seen when we last checked for DLT
            // Use a CAS construct get around synchronization issues
            int32_t oldValue;
            int32_t newValue;
            do {
               oldValue = entry->_count;
               newValue = TR::CompilationInfo::getInvocationCount(j9method);
               if (newValue < 0)
                  newValue = 0;
               } while (oldValue != VM_AtomicSupport::lockCompareExchangeU32((uint32_t*)&entry->_count, oldValue, newValue));

               entry->_seqID = 0; // should we reset this?
            }
         }
      if (TR::Options::getVerboseOption(TR_VerboseSampling))
         TR_VerboseLog::writeLineLocked(TR_Vlog_SAMPLING, "t=%6u DLTTracking: j9m=%p issueDLT=%d entry=%p entryCount=%d methodCount=%d seqID=%d",
            (unsigned)getPersistentInfo()->getElapsedTime(), j9method, shouldIssueDLT, entry, entryCount, methodCount, entry->_seqID);
      }
   else // method does not exist in DLT_HT
      {
      // Add method to DLT_HT
      // If adding is successful, do not issue DLT request
      shouldIssueDLT = !addNewEntry(j9method, getPersistentInfo()->getElapsedTime());
      if (TR::Options::getVerboseOption(TR_VerboseSampling))
         TR_VerboseLog::writeLineLocked(TR_Vlog_SAMPLING, "t=%6u DLTTracking: j9m=%p issueDLT=%d entry=%p",
            (unsigned)getPersistentInfo()->getElapsedTime(), j9method, shouldIssueDLT, entry);
      }

   return shouldIssueDLT;
   }



void DLTTracking::adjustStoredCounterForMethod(J9Method *j9method, int32_t countDiff)
   {
   // Search DLT_HT for this method
   HT_Entry *entry = find(j9method);
   if (entry)
      {
      // Subtract from the stored count the value given by the second parameter
      // Use a CAS construct to get around synchronization issues
      int32_t oldValue;
      int32_t newValue;
      do {
         oldValue = entry->_count;
         newValue = oldValue - countDiff;
         if (newValue < 0)
            newValue = 0;
         } while (oldValue != VM_AtomicSupport::lockCompareExchangeU32((uint32_t*)&entry->_count, oldValue, newValue));
      if (TR::Options::getVerboseOption(TR_VerboseSampling))
         TR_VerboseLog::writeLineLocked(TR_Vlog_SAMPLING, "t=%6u DLTTracking: j9m=%p entry=%p adjusting entry count to %d",
            (unsigned)getPersistentInfo()->getElapsedTime(), j9method, entry, newValue);
      }
   }



// Must have compMonitor in hand
void TR_JProfilingQueue::enqueueCompReq(TR_MethodToBeCompiled *compReq)
   {
   // add at the end of queue
   if (_lastQentry)
      _lastQentry->_next = compReq;
   else
      _firstQentry = compReq;

   _lastQentry = compReq;
   _size++;
   increaseQWeightBy(compReq->_weight);
   }

// Must have compMonitor in hand
TR_MethodToBeCompiled *TR_JProfilingQueue::extractFirstCompRequest()
   {
   // take the top method out of the queue
   TR_MethodToBeCompiled *m = _firstQentry;
   _firstQentry = _firstQentry->_next;
   if (!_firstQentry)
      _lastQentry = NULL;
   _size--;
   decreaseQWeightBy(m->_weight);
   return m;
   }

// Must have compMonitor in hand
bool TR_JProfilingQueue::createCompReqAndQueueIt(TR::IlGeneratorMethodDetails &details, void *startPC)
   {
   TR_OptimizationPlan *plan = TR_OptimizationPlan::alloc(warm); // TODO: pick first opt level
   if (!plan)
      return false; // OOM

   TR_MethodToBeCompiled * compReq = _compInfo->getCompilationQueueEntry();
   if (!compReq)
      {
      TR_OptimizationPlan::freeOptimizationPlan(plan);
      return false; // OOM
      }
   compReq->initialize(details, 0, CP_ASYNC_ABOVE_MIN, plan);
   compReq->_reqFromJProfilingQueue = true;
   compReq->_jitStateWhenQueued = _compInfo->getPersistentInfo()->getJitState();
   compReq->_oldStartPC = startPC;
   compReq->_async = true; // app threads are not waiting for it

   // Determine entry weight
   J9Method *j9method = details.getMethod();
   J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(j9method);
   compReq->_weight = (J9ROMMETHOD_HAS_BACKWARDS_BRANCHES(romMethod)) ? TR::CompilationInfo::WARM_LOOPY_WEIGHT : TR::CompilationInfo::WARM_LOOPLESS_WEIGHT;
   // add at the end of queue
   enqueueCompReq(compReq);
   if (TR::Options::getJITCmdLineOptions()->getVerboseOption(TR_VerboseCompileRequest))
      TR_VerboseLog::writeLineLocked(TR_Vlog_CR, "t=%u j9m=%p enqueued in JPQ. JPQ_SZ=%d",
      (uint32_t)_compInfo->getPersistentInfo()->getElapsedTime(), j9method, getQSize());

   //incStatsReqQueuedToLPQ(reason);
   return true;
   }

bool TR_JProfilingQueue::isJProfilingCandidate(TR_MethodToBeCompiled *entry, TR::Options *options, TR_J9VMBase* fej9)
   {
   if (!options->getOption(TR_EnableJProfiling) ||
      entry->isJNINative() ||
      entry->_oldStartPC != 0 || // reject non first time compilations
      !entry->getMethodDetails().isOrdinaryMethod() ||
      entry->_optimizationPlan->insertInstrumentation() ||
      !options->canJITCompile() ||
      options->getOption(TR_NoRecompile) ||
      !options->allowRecompilation())
      return false;

   static char *disableFilterOnJProfiling = feGetEnv("TR_DisableFilterOnJProfiling");
   if (!disableFilterOnJProfiling &&
      !fej9->isClassLibraryMethod((TR_OpaqueMethodBlock *)entry->getMethodDetails().getMethod(), true))
      return false;

   return true;
   }

// Must have compMonitor in hand
void TR_JProfilingQueue::invalidateRequestsForUnloadedMethods(J9Class * unloadedClass)
   {
   TR_MethodToBeCompiled *cur = _firstQentry;
   TR_MethodToBeCompiled *prev = NULL;
   bool verbose = TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseHooks);
   while (cur)
      {
      TR_MethodToBeCompiled *next = cur->_next;
      TR::IlGeneratorMethodDetails & details = cur->getMethodDetails();
      J9Method *method = details.getMethod();
      if (method)
         {
         if (J9_CLASS_FROM_METHOD(method) == unloadedClass ||
            details.isNewInstanceThunk() &&
            static_cast<J9::NewInstanceThunkDetails &>(details).isThunkFor(unloadedClass))
            {
            TR_ASSERT(cur->_priority < CP_SYNC_MIN,
               "Unloading cannot happen for SYNC requests");
            if (verbose)
               TR_VerboseLog::writeLineLocked(TR_Vlog_HK, "Invalidating compile request from JPQ for method=%p class=%p", method, unloadedClass);

            // detach from queue
            if (prev)
               prev->_next = cur->_next;
            else
               _firstQentry = cur->_next;
            if (cur == _lastQentry)
               _lastQentry = prev;
            _size--;
            decreaseQWeightBy(cur->_weight);

            // put back into the pool
            _compInfo->recycleCompilationEntry(cur);
            }
         else
            {
            prev = cur;
            }
         }
      cur = next;
      }
   }

// Must have compMonitor in hand
void TR_JProfilingQueue::purge()
   {
   while (_firstQentry)
      {
      TR_MethodToBeCompiled *cur = _firstQentry;
      _firstQentry = _firstQentry->_next;
      _size--;
      decreaseQWeightBy(cur->_weight);
      _compInfo->recycleCompilationEntry(cur);
      }
   _lastQentry = NULL;
   }


// This method returns true when the JIT thinks it's a good
// time to allow the generation of JProfiling bodies
bool TR::CompilationInfo::canProcessJProfilingRequest()
   {
   // Once we allow the generation of JProfiling bodies
   // we will not go back
   if (getJProfilingCompQueue().getAllowProcessing())
      {
      return true;
      }
   else
      {
      // Prevent processing during JVM startup or JIT startp/rampup state
      if (_jitConfig->javaVM->phase != J9VM_PHASE_NOT_STARTUP ||
         getPersistentInfo()->getJitState() == STARTUP_STATE ||
         getPersistentInfo()->getJitState() == RAMPUP_STATE)
         return false;
      // A sufficiently large number of samples in jitted code must have been seen.
      // This means that the jitted code has run for a while
      if (TR::Recompilation::globalSampleCount < TR::Options::_jProfilingEnablementSampleThreshold)
         {
         return false;
         }
      getJProfilingCompQueue().setAllowProcessing();
      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerbosePerformance))
         {
         TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, "t=%6u Allowing generation of JProfiling bodies",
            (uint32_t)getPersistentInfo()->getElapsedTime());
         }
      return true;
      }
   }

void
TR::CompilationInfoPerThread::updateLastLocalGCCounter()
   {
   _lastLocalGCCounter = getCompilationInfo()->getLocalGCCounter();
   }

// This method is executed by the JITServer to queue a placeholder for
// a compilation request received from the client. At the time the new
// entry is queued we do not know any details about the compilation request.
// The method needs to be executed with compilation monitor in hand
TR_MethodToBeCompiled *
TR::CompilationInfo::addOutOfProcessMethodToBeCompiled(JITServer::ServerStream *stream)
   {
   TR_MethodToBeCompiled *entry = getCompilationQueueEntry(); // allocate a new entry
   if (entry)
      {
      // Initialize the entry with defaults (some, like methodDetails, are bogus)
      TR::IlGeneratorMethodDetails details;
      entry->initialize(details, NULL, CP_SYNC_NORMAL, NULL);
      entry->_entryTime = getPersistentInfo()->getElapsedTime(); // cheaper version
      entry->_stream = stream; // Add the stream to the entry
      incrementMethodQueueSize(); // one more method added to the queue
      _numQueuedFirstTimeCompilations++; // otherwise an assert triggers when we dequeue
      queueEntry(entry);

     // Determine whether we need to activate another compilation thread from the pool
      TR_YesNoMaybe activate = TR_maybe;
      if (getNumCompThreadsActive() <= 0)
         {
         activate = TR_yes;
         }
      else if (getNumCompThreadsJobless() > 0)
         {
         activate = TR_no; // just wake the jobless one
         }
      else
         {
         int32_t numCompThreadsSuspended = getNumUsableCompilationThreads() - getNumCompThreadsActive();
         TR_ASSERT(numCompThreadsSuspended >= 0, "Accounting error for suspendedCompThreads usable=%d active=%d\n", 
            getNumUsableCompilationThreads(), getNumCompThreadsActive());
         // Cannot activate if there is nothing to activate
         activate = (numCompThreadsSuspended <= 0) ? TR_no : TR_yes;
         }
      if (activate == TR_yes)
         {
         // Must find one that is SUSPENDED/SUSPENDING
         TR::CompilationInfoPerThread *compInfoPT = getFirstSuspendedCompilationThread();
         if (compInfoPT)
            {
            compInfoPT->resumeCompilationThread();
            if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCompilationThreads))
               {
               TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, "t=%6u Activate compThread %d Qweight=%d active=%d",
                  (uint32_t)getPersistentInfo()->getElapsedTime(), compInfoPT->getCompThreadId(), getQueueWeight(), getNumCompThreadsActive());
               }
            }
         }
      }
   return entry;
   }

void
TR::CompilationInfo::requeueOutOfProcessEntry(TR_MethodToBeCompiled *entry)
   {
   TR_ASSERT(getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER, "Should be called in JITServer server mode only");

   recycleCompilationEntry(entry);
   // we do not requeue outOfProcessEntry for the Per-Compilation-Connection Mode
   if (feGetEnv("TR_EnableJITaaSPerCompConn"))
      return;

   if (entry->_stream && addOutOfProcessMethodToBeCompiled(entry->_stream))
      {
      // successfully queued the new entry, so notify a thread
      getCompilationMonitor()->notifyAll();
      }
   }


