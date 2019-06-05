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

#include "runtime/codertinit.hpp"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "aotarch.h"
#include "jitprotos.h"
#include "j9.h"
#include "j9cfg.h"
#include "j9consts.h"
#include "j9protos.h"
#include "stackwalk.h"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "env/ProcessorInfo.hpp"
#include "infra/Monitor.hpp"
#include "infra/MonitorTable.hpp"
#include "runtime/ArtifactManager.hpp"
#include "runtime/CodeCacheManager.hpp"
#include "runtime/CodeRuntime.hpp"
#include "runtime/DataCache.hpp"
#include "runtime/J9Runtime.hpp"
#include "runtime/MethodMetaData.h"
#include "runtime/asmprotos.h"

char * feGetEnv(const char * s);

TR::Monitor * assumptionTableMutex = NULL;

JIT_HELPER(icallVMprJavaSendVirtual0);
JIT_HELPER(icallVMprJavaSendVirtual1);
JIT_HELPER(icallVMprJavaSendVirtualJ);
JIT_HELPER(icallVMprJavaSendVirtualF);
JIT_HELPER(icallVMprJavaSendVirtualD);

extern "C" {

extern void init_codert_vm_fntbl(J9JavaVM * vm);

#ifdef J9VM_ENV_DIRECT_FUNCTION_POINTERS
   #ifndef J9SW_NEEDS_JIT_2_INTERP_THUNKS
      extern void * jit2InterpreterSendTargetTable;
   #endif
#endif


#if defined(TR_HOST_X86) && !defined(J9HAMMER)

enum TR_PatchingFenceTypes
   {
   // Make sure these match IA32PicBuilder.asm
   TR_NoPatchingFence=0,
   TR_CPUIDPatchingFence=1,
   TR_CLFLUSHPatchingFence=2,
   };

#endif // defined(TR_HOST_X86) && !defined(J9HAMMER)

#if defined(TR_HOST_X86) && !defined(TR_HOST_64BIT)
U_32 vmThreadTLSKey;
#endif

#if defined(J9VM_PORT_SIGNAL_SUPPORT) && defined(J9VM_INTERP_NATIVE_SUPPORT)
#if defined(TR_HOST_X86) && defined(TR_TARGET_X86) && !defined(TR_TARGET_64BIT)
extern "C" UDATA jitX86Handler(J9VMThread* vmThread, U_32 sigType, void* sigInfo);
#elif defined(TR_HOST_POWER) && defined(TR_TARGET_POWER)
extern "C" UDATA jitPPCHandler(J9VMThread* vmThread, U_32 sigType, void* sigInfo);
#elif defined(TR_HOST_S390) && defined(TR_TARGET_S390)
extern "C" UDATA jit390Handler(J9VMThread* vmThread, U_32 sigType, void* sigInfo);
#elif defined(TR_HOST_X86) && defined(TR_TARGET_X86) && defined(TR_TARGET_64BIT)
extern "C" UDATA jitAMD64Handler(J9VMThread* vmThread, U_32 sigType, void* sigInfo);
#endif
#endif

#if (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM))
extern "C" void jitClassesRedefined(J9VMThread * currentThread, UDATA classCount, J9JITRedefinedClass *classList);
extern "C" void jitFlushCompilationQueue(J9VMThread * currentThread, J9JITFlushCompilationQueueReason reason);
#endif

extern "C" void jitMethodBreakpointed(J9VMThread * currentThread, J9Method *j9method);

extern "C" void jitIllegalFinalFieldModification(J9VMThread *currentThread, J9Class *fieldClass);

extern "C" void jitDiscardPendingCompilationsOfNatives(J9VMThread *vmThread, J9Class *clazz);

#if defined(J9VM_JIT_DYNAMIC_LOOP_TRANSFER)
extern "C" void *jitLookupDLT(J9VMThread *currentThread, J9Method *method, UDATA bcIndex);
#endif

}

static void codertOnBootstrap(J9HookInterface * * hookInterface, UDATA eventNum, void * eventData, void * userData)
   {
   J9VMAboutToBootstrapEvent * boostrapEvent = (J9VMAboutToBootstrapEvent*)eventData;
   J9VMThread * vmThread = boostrapEvent->currentThread;
   J9JavaVM * javaVM = vmThread->javaVM;
   J9JITConfig * jitConfig = javaVM->jitConfig;
   PORT_ACCESS_FROM_JAVAVM(javaVM);

   if (!jitConfig)
      return;

   /* We know that the JIT will be running -- set up stack walk pointers */
   if (!javaVM->jitWalkStackFrames)
      {
      javaVM->jitWalkStackFrames = jitWalkStackFrames;
      javaVM->jitExceptionHandlerSearch = jitExceptionHandlerSearch;
      javaVM->jitGetOwnedObjectMonitors = jitGetOwnedObjectMonitors;
      }

   return;
   }


static void codertShutdown(J9HookInterface * * hook, UDATA eventNum, void * eventData, void * userData)
   {
   J9VMShutdownEvent * event = (J9VMShutdownEvent *)eventData;
   J9VMThread * vmThread = event->vmThread;
   IDATA rc = event->exitCode;

   J9JavaVM * javaVM = vmThread->javaVM;
   PORT_ACCESS_FROM_JAVAVM(javaVM);
   return;
   }

void codert_OnUnload(J9JavaVM * javaVM)
   {
   codert_freeJITConfig(javaVM);
   }

extern "C" {
void * getRuntimeHelperValue(int32_t h)
   {
   return runtimeHelperValue((TR_RuntimeHelper) h);
   }

}

#ifdef LINUX
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#endif

J9JITConfig * codert_onload(J9JavaVM * javaVM)
   {
   J9JITConfig * jitConfig = NULL;
   PORT_ACCESS_FROM_JAVAVM(javaVM);
   OMRPORT_ACCESS_FROM_J9PORT(PORTLIB);

   #if defined(TR_HOST_X86) && !defined(TR_HOST_64BIT)
      vmThreadTLSKey = (U_32) javaVM->omrVM->_vmThreadKey;
   #endif

   J9HookInterface * * vmHooks = javaVM->internalVMFunctions->getVMHookInterface(javaVM);

   #if defined(LINUX)
   static char * sigstopOnLoad = feGetEnv("TR_SIGSTOPOnLoad");
   if (sigstopOnLoad)
      {
      uint32_t pid = getpid();
      fprintf(stderr, "JIT: sleeping to allow debugger to attach. Execute:\n"
              "(sleep 2; kill -CONT %d) & gdb --pid=%d\n" , pid, pid);
      raise(SIGSTOP);
      }
   #endif

   // Attempt to allocate a table of mutexes
   if (!TR::MonitorTable::init(privatePortLibrary, javaVM))
      goto _abort;

   TR_ASSERT(!javaVM->jitConfig, "jitConfig already initialized.");

#if defined(J9VM_INTERP_AOT_COMPILE_SUPPORT)
   javaVM->jitConfig = (J9JITConfig *) j9mem_allocate_memory(sizeof(J9AOTConfig), J9MEM_CATEGORY_JIT);
#else
   javaVM->jitConfig = (J9JITConfig *) j9mem_allocate_memory(sizeof(J9JITConfig), J9MEM_CATEGORY_JIT);
#endif

   if (!javaVM->jitConfig)
      goto _abort;

#if defined(J9VM_INTERP_AOT_COMPILE_SUPPORT)
   memset(javaVM->jitConfig, 0, sizeof(J9AOTConfig));
#else
   memset(javaVM->jitConfig, 0, sizeof(J9JITConfig));
#endif
   jitConfig = javaVM->jitConfig;
   jitConfig->sampleInterruptHandlerKey = -1;

   /* setup the hook interface */
   if (J9HookInitializeInterface(J9_HOOK_INTERFACE(jitConfig->hookInterface), OMRPORTLIB, sizeof(jitConfig->hookInterface)))
      goto _abort;

   if (j9ThunkTableAllocate(javaVM))
      goto _abort;

   /* initialize assumptionTableMutex */
   if (!assumptionTableMutex)
      {
      if (!(assumptionTableMutex = TR::Monitor::create("JIT-AssumptionTableMutex")))
         goto _abort;
      }

   /* Should use portlib */
#if defined(TR_HOST_X86)
   jitConfig->codeCacheAlignment = 32;
#elif defined(TR_HOST_64BIT) || defined(TR_HOST_S390)
   // 390 31-bit may generate 64-bit instruction (i.e. CGRL) which requires
   // doubleword alignment for its operands
   jitConfig->codeCacheAlignment = 8;
#else
   jitConfig->codeCacheAlignment = 4;
#endif

   jitConfig->translationArtifacts = jit_allocate_artifacts(javaVM->portLibrary);
   if (!jitConfig->translationArtifacts)
      goto _abort;

   /* Setup bootstrap and shutdown hooks */

   (*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_ABOUT_TO_BOOTSTRAP, codertOnBootstrap, OMR_GET_CALLSITE(), NULL);

   if ((*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_SHUTTING_DOWN, codertShutdown, OMR_GET_CALLSITE(), NULL))
      {
      j9tty_printf(PORTLIB, "Error: Unable to install vm shutting down hook\n");
      goto _abort;
      }

#if defined(TR_HOST_X86) && !defined(J9HAMMER)
   /*
    * VM guarantees SSE/SSE2 are available
    */
   jitConfig->runtimeFlags |=  J9JIT_PATCHING_FENCE_TYPE;
#endif

#if defined(J9VM_INTERP_AOT_RUNTIME_SUPPORT)
   jitConfig->aotrt_getRuntimeHelper = getRuntimeHelperValue;
   jitConfig->aotrt_lookupSendTargetForThunk = lookupSendTargetForThunk;
#endif  /* J9VM_INTERP_AOT_RUNTIME_SUPPORT */

   jitConfig->osrFramesMaximumSize = (UDATA) 8192;
   jitConfig->osrScratchBufferMaximumSize = (UDATA) 1024;
   jitConfig->osrStackFrameMaximumSize = (UDATA) 8192;

   return jitConfig;

   _abort:
   codert_freeJITConfig(javaVM);
   return NULL;
   }

void codert_freeJITConfig(J9JavaVM * javaVM)
   {
   J9JITConfig * jitConfig = javaVM->jitConfig;

   if (jitConfig)
      {
      PORT_ACCESS_FROM_JAVAVM(javaVM);

      j9ThunkTableFree(javaVM);

      if (jitConfig->translationArtifacts)
         avl_jit_artifact_free_all(javaVM, jitConfig->translationArtifacts);

      if (jitConfig->codeCacheList)
         javaVM->internalVMFunctions->freeMemorySegmentList(javaVM, jitConfig->codeCacheList);

#if defined(TR_TARGET_S390)
      if (jitConfig->pseudoTOC)
         j9mem_free_memory(jitConfig->pseudoTOC);
#endif

      if (jitConfig->compilationInfo)
         {
         TR_J9VMBase *fej9 = (TR_J9VMBase *)jitConfig->compilationInfo;
         fej9->freeSharedCache();
         jitConfig->compilationInfo = 0;
         }

#if defined(J9VM_INTERP_AOT_COMPILE_SUPPORT)
      if (((J9AOTConfig*)jitConfig)->aotCompilationInfo)
         {
         TR_J9VMBase *fej9 = (TR_J9VMBase *)(((J9AOTConfig*)jitConfig)->aotCompilationInfo);
         fej9->freeSharedCache();
         ((J9AOTConfig*)jitConfig)->aotCompilationInfo = 0;
         }
#endif

/*  Commented out because of rare failure during shutdown: 90700: JIT_Sanity.TestSCCMLTests1.Mode110 Crash during freeJITconfig()
    Deallocating malloced memory is not going to very useful even for zOS

      // Free the caches for fast stack walk mechanism
      // before freeing the data caches or code caches
      J9MemorySegment *dataCacheSeg = jitConfig->dataCacheList->nextSegment;
      while (dataCacheSeg) // Iterate over all the data cache segments
         {
         // Iterate over all records in the data cache segment
         UDATA current = (UDATA)dataCacheSeg->heapBase;
         UDATA end = (UDATA)dataCacheSeg->heapAlloc;
         while (current < end)
            {
            J9JITDataCacheHeader * hdr = (J9JITDataCacheHeader *)current;
            if (hdr->type == J9DataTypeExceptionInfo)
               {
               J9JITExceptionTable * metaData = (J9JITExceptionTable *)(current + sizeof(J9JITDataCacheHeader));
               // Don't look at unloaded metaData
               if (metaData->constantPool != NULL)
                  {
                  if (metaData->bodyInfo != NULL)
                     {
                     void *mapTablePtr = ((TR_PersistentJittedBodyInfo *)metaData->bodyInfo)->getMapTable();
                     if (mapTablePtr != (void*)-1 && mapTablePtr != NULL)
                        {
                        j9mem_free_memory(mapTablePtr);
                        ((TR_PersistentJittedBodyInfo *)metaData->bodyInfo)->setMapTable(NULL);
                        }
                     }
                  }
               }
            current += hdr->size;
            }
         dataCacheSeg = dataCacheSeg->nextSegment;
         } // end while (dataCacheSeg)

*/
      TR::CodeCacheManager *manager = TR::CodeCacheManager::instance();
      if (manager)
         manager->destroy();

      TR_DataCacheManager::destroyManager();


      // Destroy faint blocks
      OMR::FaintCacheBlock *currentFaintBlock = (OMR::FaintCacheBlock *)jitConfig->methodsToDelete;
      while (currentFaintBlock)
         {
         PORT_ACCESS_FROM_JITCONFIG(jitConfig);
         OMR::FaintCacheBlock *nextFaintBlock = currentFaintBlock->_next;
         j9mem_free_memory(currentFaintBlock);
         currentFaintBlock = nextFaintBlock;
         }
      jitConfig->methodsToDelete = NULL;


      J9HookInterface** hookInterface = J9_HOOK_INTERFACE(jitConfig->hookInterface);
      if (*hookInterface)
         (*hookInterface)->J9HookShutdownInterface(hookInterface);

      // TEMP FIX for 92051 - do not free the jit config
      // because some runtime code might still be in the process of running
      // No longer the case. This code is called only when using j9 and it happens
      // in stage 16 after INTERPRETER_SHUTDOWN
      if (jitConfig->privateConfig)
         {
         if (((TR_JitPrivateConfig*)jitConfig->privateConfig)->aotStats)
            j9mem_free_memory(((TR_JitPrivateConfig*)jitConfig->privateConfig)->aotStats);

         j9mem_free_memory(jitConfig->privateConfig);
         jitConfig->privateConfig = NULL;
         }
      j9mem_free_memory(jitConfig);

      /* Finally break the connection between the VM and the JIT */
      javaVM->jitConfig = NULL;

      // TEMP FIX for 97269, re-enable when the similar problem for 92051
      // above is fixed.

      TR::MonitorTable::get()->free();
      }
   }

extern "C" UDATA trJitGOT();

#if defined(J9VM_INTERP_NATIVE_SUPPORT) && defined(J9VM_ENV_SHARED_LIBS_USE_GLOBAL_TABLE)
static void
initJitTOCForAllThreads(J9JavaVM* javaVM, UDATA jitTOC)
   {
   j9thread_monitor_enter(javaVM->vmThreadListMutex);

   if (javaVM->mainThread)
      {
      J9VMThread* currThread = javaVM->mainThread;
      do {
         currThread->jitTOC = jitTOC;
         }
      while( (currThread=currThread->linkNext) != javaVM->mainThread );
      }

   j9thread_monitor_exit (javaVM->vmThreadListMutex);
   }
#endif


void codert_init_helpers_and_targets(J9JITConfig * jitConfig, char isSMP)
   {
   J9JavaVM *javaVM = jitConfig->javaVM;
   PORT_ACCESS_FROM_PORT(javaVM->portLibrary);

#ifdef J9VM_ENV_CALL_VIA_TABLE
   init_codert_vm_fntbl(javaVM);
#else
   {
   /* set up TOC / GOT on PPC platforms */
   #if defined(AIXPPC)
   /* get the TOC from a function descriptor */
   javaVM->jitTOC = ((UDATA *) codert_init_helpers_and_targets)[1];
   #elif defined(LINUXPPC64)
      #if defined(__LITTLE_ENDIAN__)
          javaVM->jitTOC = trJitGOT();
      #else
          javaVM->jitTOC = ((UDATA *) codert_init_helpers_and_targets)[1];
      #endif
   #elif defined(LINUXPPC)
   {
   // Replaced this code for two reasons:
   // 1. gr2 is now never used on Linux PPC 32 (Bugzilla 100451)
   //    so we don't need to save a magicLinkageValue.
   // 2. We want to be able to build Linux PPC with the xlC compiler, so we
   //    don't want to use __asm__.
   //        UDATA gotReg = 0xDEADBABE;
   //        UDATA reservedReg = 0xDEADFACE;
   //
   //        __asm__("bl _GLOBAL_OFFSET_TABLE_@local-4; mflr %0; or %1,2,2":"=r"(gotReg), "=r"(reservedReg)
   //        :  /* no inputs */ );
   //
   //        javaVM->jitTOC = gotReg;
   //        javaVM->magicLinkageValue = reservedReg;

   javaVM->jitTOC = trJitGOT();
   }
   #endif

   #if defined(J9VM_INTERP_NATIVE_SUPPORT) && defined(J9VM_ENV_SHARED_LIBS_USE_GLOBAL_TABLE)
   initJitTOCForAllThreads(javaVM,javaVM->jitTOC);
   #endif
   }
#endif

   jitConfig->jitGetExceptionTableFromPC = jitGetExceptionTableFromPC;
   jitConfig->jitGetStackMapFromPC = getStackMapFromJitPC;
   jitConfig->jitGetInlinerMapFromPC = jitGetInlinerMapFromPC;
   jitConfig->getJitInlineDepthFromCallSite = getJitInlineDepthFromCallSite;
   jitConfig->getJitInlinedCallInfo = getJitInlinedCallInfo;
   jitConfig->getStackMapFromJitPC = getStackMapFromJitPC;
   jitConfig->getFirstInlinedCallSite = getFirstInlinedCallSite;
   jitConfig->getNextInlinedCallSite = getNextInlinedCallSite;
   jitConfig->hasMoreInlinedMethods = hasMoreInlinedMethods;
   jitConfig->getInlinedMethod = getInlinedMethod;
   jitConfig->getByteCodeIndex = getByteCodeIndex;
   jitConfig->getByteCodeIndexFromStackMap = getByteCodeIndexFromStackMap;
   jitConfig->getCurrentByteCodeIndexAndIsSameReceiver = getCurrentByteCodeIndexAndIsSameReceiver;
   jitConfig->getJitRegisterMap = getJitRegisterMap;
   jitConfig->jitReportDynamicCodeLoadEvents = jitReportDynamicCodeLoadEvents;
#if (defined(TR_HOST_X86) || defined(TR_HOST_POWER) || defined(TR_HOST_S390) || defined(TR_HOST_ARM))
   jitConfig->jitClassesRedefined = jitClassesRedefined;
   jitConfig->jitFlushCompilationQueue = jitFlushCompilationQueue;
#endif
   jitConfig->jitDiscardPendingCompilationsOfNatives = jitDiscardPendingCompilationsOfNatives;
   jitConfig->jitMethodBreakpointed = jitMethodBreakpointed;
   jitConfig->jitIllegalFinalFieldModification = jitIllegalFinalFieldModification;

   initializeCodertFunctionTable(javaVM);

   #ifndef J9SW_NEEDS_JIT_2_INTERP_THUNKS
      #ifdef J9VM_ENV_DIRECT_FUNCTION_POINTERS
         jitConfig->jitSendTargetTable = &jit2InterpreterSendTargetTable;
      #else
         jitConfig->jitSendTargetTable = jitConfig->javaVM->internalVMLabels->jit2InterpreterSendTargetTable;
      #endif
   #endif

   #if defined(J9VM_PORT_SIGNAL_SUPPORT) && defined(J9VM_INTERP_NATIVE_SUPPORT)
      #if defined(TR_HOST_X86) && defined(TR_TARGET_X86) && !defined(TR_TARGET_64BIT)
         jitConfig->jitSignalHandler = jitX86Handler;
      #elif defined(TR_HOST_POWER) && defined(TR_TARGET_POWER)
         jitConfig->jitSignalHandler = jitPPCHandler;
      #elif defined(TR_HOST_S390) && defined(TR_TARGET_S390)
         jitConfig->jitSignalHandler = jit390Handler;
      #elif defined(TR_HOST_X86) && defined(TR_TARGET_X86) && defined(TR_TARGET_64BIT)
         jitConfig->jitSignalHandler = jitAMD64Handler;
      #endif
   #endif


#if defined(J9VM_JIT_DYNAMIC_LOOP_TRANSFER)
   jitConfig->isDLTReady = jitLookupDLT;
#endif

   // OSR hooks
   //jitConfig->jitOSRPatchMethod = preOSR;
   //jitConfig->jitOSRUnpatchMethod = postOSR;
   //jitConfig->jitCanResumeAtOSRPoint = jitCanResumeAtOSRPoint;
   //jitConfig->jitUsesOSR = usesOSR;

   /* Initialize the runtime helper table lookup */
   initializeCodeRuntimeHelperTable(jitConfig, isSMP);

   ::jitConfig = jitConfig;
   }


UDATA lookupSendTargetForThunk(J9JavaVM * javaVM, int thunkNumber)
   {
#if defined(J9ZOS390)
   #define VIRTUAL_TARGET(x) TOC_UNWRAP_ADDRESS(x)
#else
#ifdef J9VM_ENV_DIRECT_FUNCTION_POINTERS
   #define VIRTUAL_TARGET(x) (x)
#else
   #define VIRTUAL_TARGET(x) javaVM->internalVMLabels->x
#endif
#endif

   /* Use the internal function table which knows how to reference the code part of an intermodule reference */
   #if !defined(TR_TARGET_32BIT) || !defined(TR_TARGET_X86)
      switch (thunkNumber)
         {
         case 0:
            return (UDATA) (UDATA)VIRTUAL_TARGET(icallVMprJavaSendVirtual0);
         case 1:
            return (UDATA) 0;
         case 2:
            return (UDATA) 0;
         case 3:
            return (UDATA) 0;
         case 4:
            return (UDATA) 0;
         case 5:
            return (UDATA) (UDATA)VIRTUAL_TARGET(icallVMprJavaSendVirtualF);
         case 6:
            return (UDATA) (UDATA)VIRTUAL_TARGET(icallVMprJavaSendVirtual1);
         case 7:
            return (UDATA) (UDATA)VIRTUAL_TARGET(icallVMprJavaSendVirtualD);
         case 8:
            return (UDATA) (UDATA)VIRTUAL_TARGET(icallVMprJavaSendVirtualJ);
         case 9:
            return (UDATA) 0;
         }
   #endif /* TR_HOST_X86 */

   return 0;

#undef VIRTUAL_TARGET
   }


TR_X86CPUIDBuffer *queryX86TargetCPUID(void * javaVM);

#ifdef TR_HOST_X86
#include "x/runtime/X86Runtime.hpp"
#endif

static TR_X86CPUIDBuffer *initializeX86CPUIDBuffer(void * javaVM)
   {
   static TR_X86CPUIDBuffer buf = { {'U','n','k','n','o','w','n','B','r','a','n','d'} };
   J9JITConfig *jitConfig = ((J9JavaVM *)javaVM)->jitConfig;

   PORT_ACCESS_FROM_PORT(((J9JavaVM *)javaVM)->portLibrary);

#if defined(TR_HOST_X86)
   // jitConfig can be NULL during AOT
   if (jitConfig && jitConfig->processorInfo == NULL)
      {
      jitGetCPUID(&buf);
      jitConfig->processorInfo = &buf;
      }
#endif

   return &buf;
   }

TR_X86CPUIDBuffer *queryX86TargetCPUID(void * javaVM)
   {
   static TR_X86CPUIDBuffer *result = initializeX86CPUIDBuffer(javaVM);
   return result;
   }

