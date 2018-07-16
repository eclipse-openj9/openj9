/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

#include "control/Options.hpp"
#include "env/ClassLoaderTable.hpp"
#include "env/annotations/AnnotationBase.hpp"
#include "control/CompilationRuntime.hpp"
#include "control/CompilationThread.hpp"
#include "env/VMJ9.h"
#include "runtime/IProfiler.hpp"
#include "runtime/J9Profiler.hpp"
#include "runtime/codertinit.hpp"
#include "rossa.h"

#include "j9.h"
#include "jvminit.h"

// NOTE: Any changes made to J9VMDllMain that relate to AOT compile time
// should be put instead in onLoadInternal().
//

#ifdef AOT_COMPILE_TIME_VERSION
  #define THIS_DLL_NAME  J9_AOT_DLL_NAME
#else
  #define THIS_DLL_NAME  J9_JIT_DLL_NAME
#endif

extern bool initializeJIT(J9JavaVM *vm);

extern bool isQuickstart;

IDATA J9VMDllMain(J9JavaVM* vm, IDATA stage, void * reserved)
   {
   J9JITConfig * jitConfig = 0;
   UDATA initialFlags = 0;
   J9VMDllLoadInfo* loadInfo = FIND_DLL_TABLE_ENTRY( THIS_DLL_NAME );
   char* xjitCommandLineOptions = "";
   char* xaotCommandLineOptions = "";
   IDATA fullSpeedDebugSet = FALSE;
   IDATA argIndex = 0;

   IDATA tlhPrefetch = 0;
   IDATA notlhPrefetch = 0;
   IDATA lockReservation = 0;
   IDATA argIndexXjit = 0;
   IDATA argIndexXaot = 0;
   IDATA argIndexXnojit = 0;
   IDATA argIndexXnoaot = 0;

   IDATA argIndexClient = 0;
   IDATA argIndexServer = 0;
   IDATA argIndexQuickstart = 0;

   IDATA argIndexRIEnabled = 0;
   IDATA argIndexRIDisabled = 0;

   static bool isJIT = false;
   static bool isAOT = false;

   static bool jitInitialized = false;
   static bool aotrtInitialized = false;

   PORT_ACCESS_FROM_JAVAVM(vm);

   #define AOT_INIT_STAGE SYSTEM_CLASSLOADER_SET            /* defined separately to ensure dependencies below */

   switch(stage)
      {

      case DLL_LOAD_TABLE_FINALIZED :
         {

         // Bootstrap JIT initialization
         //
         if (!initializeJIT(vm))
            {
            return J9VMDLLMAIN_FAILED;
            }

         /* Find and consume these before the library might be unloaded */
         FIND_AND_CONSUME_ARG(EXACT_MATCH, "-Xnodfpbd", 0);
         if (FIND_ARG_IN_VMARGS(EXACT_MATCH, "-Xdfpbd", 0) >= 0)
            {
            FIND_AND_CONSUME_ARG( EXACT_MATCH, "-Xhysteresis", 0);
            }
         FIND_AND_CONSUME_ARG( EXACT_MATCH, "-Xnoquickstart", 0); // deprecated
         FIND_AND_CONSUME_ARG(STARTSWITH_MATCH, "-Xtune:elastic", 0);
         argIndexQuickstart = FIND_AND_CONSUME_ARG( EXACT_MATCH, "-Xquickstart", 0);
         argIndexClient = FIND_AND_CONSUME_ARG( EXACT_MATCH, "-client", 0);
         argIndexServer = FIND_AND_CONSUME_ARG( EXACT_MATCH, "-server", 0);
         tlhPrefetch = FIND_AND_CONSUME_ARG(EXACT_MATCH, "-XtlhPrefetch", 0);
         notlhPrefetch = FIND_AND_CONSUME_ARG(EXACT_MATCH, "-XnotlhPrefetch", 0);
         lockReservation = FIND_AND_CONSUME_ARG(EXACT_MATCH, "-XlockReservation", 0);
         FIND_AND_CONSUME_ARG(EXACT_MEMORY_MATCH, "-Xcodecache", 0);
         FIND_AND_CONSUME_ARG(STARTSWITH_MATCH, "-XjniAcc:", 0);
         FIND_AND_CONSUME_ARG(EXACT_MEMORY_MATCH, "-Xcodecachetotal", 0);
         FIND_AND_CONSUME_ARG(EXACT_MEMORY_MATCH, "-XX:codecachetotal=", 0);

         FIND_AND_CONSUME_ARG(STARTSWITH_MATCH, "-Xlp:codecache:", 0);

         FIND_AND_CONSUME_ARG(EXACT_MEMORY_MATCH, "-XsamplingExpirationTime", 0);
         FIND_AND_CONSUME_ARG(EXACT_MEMORY_MATCH, "-XcompilationThreads", 0);
         FIND_AND_CONSUME_ARG(EXACT_MEMORY_MATCH, "-XaggressivenessLevel", 0);
         argIndexXjit = FIND_AND_CONSUME_ARG(OPTIONAL_LIST_MATCH, "-Xjit", 0);
         argIndexXaot = FIND_AND_CONSUME_ARG(OPTIONAL_LIST_MATCH, "-Xaot", 0);
         argIndexXnojit = FIND_AND_CONSUME_ARG(OPTIONAL_LIST_MATCH, "-Xnojit", 0);
         argIndexXnoaot = FIND_AND_CONSUME_ARG(OPTIONAL_LIST_MATCH, "-Xnoaot", 0);

         argIndexRIEnabled = FIND_AND_CONSUME_ARG(EXACT_MATCH, "-XX:+RuntimeInstrumentation", 0);
         argIndexRIDisabled = FIND_AND_CONSUME_ARG(EXACT_MATCH, "-XX:-RuntimeInstrumentation", 0);

         // Determine if user disabled Runtime Instrumentation
         if (argIndexRIEnabled >= 0 || argIndexRIDisabled >= 0)
            TR::Options::_hwProfilerEnabled = (argIndexRIDisabled > argIndexRIEnabled) ? TR_no : TR_yes;

         TR::Options::_doNotProcessEnvVars = (FIND_AND_CONSUME_ARG(EXACT_MATCH, "-XX:doNotProcessJitEnvVars", 0) >= 0);

         // Determine if quickstart mode or not; -client, -server, -Xquickstart can all be specified
         // on the command line. The last appearance wins
         if (argIndexServer < argIndexClient || argIndexServer < argIndexQuickstart)
            {
            isQuickstart = true;
            }
         else if (argIndexServer > 0)
            {
            TR::Options::_bigAppThreshold = 1;
            }

#ifdef TR_HOST_X86
         // By default, disallow reservation of objects' monitors for which a
         // previous reservation has been cancelled (issue #1124). But allow it
         // again if -XlockReservation was specified.
         TR::Options::_aggressiveLockReservation = lockReservation >= 0;
#else
         // Keep the always aggressive behaviour until codegen is adjusted.
         TR::Options::_aggressiveLockReservation = true;
#endif

         /* setup field to indicate whether we will allow JIT compilation */
         if (argIndexXjit >= argIndexXnojit)
            {
            isJIT = true;
            }

         /* setup field to indicate whether we will allow AOT compilation and perform AOT runtime
          * initializations to allow AOT code to be loaded from the shared cache as well as from JXEs */
         if (argIndexXaot >= argIndexXnoaot)
            {
            isAOT = true;
            }

         static char *disableAOT = feGetEnv2("TR_disableAOT", (void *)vm);
         if (disableAOT)
            isAOT = false;

         /* If debuginfoserver has been loaded and no FSD support, unload this library and aot */
         #ifdef J9VM_JIT_FULL_SPEED_DEBUG
         fullSpeedDebugSet = TRUE;
         #endif

         static bool TR_DisableFullSpeedDebug = feGetEnv2("TR_DisableFullSpeedDebug", (void *)vm)?1:0;
         if (!fullSpeedDebugSet || TR_DisableFullSpeedDebug)
            {
            loadInfo->loadFlags |= FORCE_UNLOAD;
            break;
            }

#if defined(J9VM_GC_BATCH_CLEAR_TLH)
            /* The order is important: we have to request it before the first TLH is created. */
            /* Platform detection seems difficult at this point.                              */
            {
#ifdef TR_HOST_X86
            static char *enableBatchClear = feGetEnv2("TR_EnableBatchClear", (void *)vm);

#else //ppc and s390
            static char *disableBatchClear = feGetEnv2("TR_DisableBatchClear", (void *)vm);
#ifdef TR_HOST_POWER
            static char *disableDualTLH   = feGetEnv2("TR_DisableDualTLH", (void *)vm);

            //if disableDualTLH is specified, revert back to old semantics.
            // Do not batch clear, JIT has to zeroinit all code on TLH.
            //Non P6, P7 and up are allowed to batch clear however.
            TR_Processor ptype = TR_J9VMBase::getPPCProcessorType();
            static bool disableZeroedTLHPages =  disableDualTLH && (((notlhPrefetch >= 0) || (ptype != TR_PPCp6 && ptype != TR_PPCp7)));
#endif//TR_HOST_POWER
#endif//TR_HOST_X86
           /*in testmode, the JIT will be loaded by a native. At this point it's too late to change
            * the TLH mode, and it won't matter anyway, since JIT'ed code won't be run.
            * See CMVC 70078
            */
            if (
#ifdef TR_HOST_X86
                 enableBatchClear
#else//ppc and s390
                 disableBatchClear==0
#ifdef TR_HOST_POWER
                  &&!disableZeroedTLHPages
#endif//TR_HOST_POWER
#endif//TR_HOST_X86
            )
               {
               J9VMDllLoadInfo* gcLoadInfo = FIND_DLL_TABLE_ENTRY( J9_GC_DLL_NAME );
               if (!IS_STAGE_COMPLETED(gcLoadInfo->completedBits, JCL_INITIALIZED) )//&& vm->memoryManagerFunctions)
                  {
                  vm->memoryManagerFunctions->allocateZeroedTLHPages(vm, true);
                  }
               }
            }
#endif//J9VM_GC_BATCH_CLEAR_TLH

         break;
         }

      case AOT_INIT_STAGE :
         if (isAOT)
            {
            /* Perform initializations for AOT runtime */
#if defined(J9VM_INTERP_AOT_RUNTIME_SUPPORT)
            // TODO: This now acts as a general "Is the jit shutting down" flag, should it be renamed / changed for something different?
            if (vm->jitConfig)
               vm->jitConfig->runtimeFlags |= J9JIT_AOT_ATTACHED;
#endif
               aotrtInitialized = true;
            }
         break;

      // createThreadWithCategory compiling threads in this stage
      case JIT_INITIALIZED :
         if (isJIT || isAOT)
            {
            /* We need to initialize the following if we allow JIT compilation, AOT compilation or AOT relocation to be done */
            try
               {
               /*
                * Note that the option prefix we need to match includes the colon.
                */
               argIndexXjit = FIND_ARG_IN_VMARGS( STARTSWITH_MATCH, "-Xjit:", 0);
               argIndexXaot = FIND_ARG_IN_VMARGS( STARTSWITH_MATCH, "-Xaot:", 0);

               /* do initializations for -Xjit options */
               if (isJIT && argIndexXjit >= 0)
                  {
                  IDATA returnVal = 0, size = 128;
                  xjitCommandLineOptions = 0;
                  do
                     {
                     size = size * 2;
                     if (xjitCommandLineOptions)
                        j9mem_free_memory(xjitCommandLineOptions);
                     if (!(xjitCommandLineOptions = (char*)j9mem_allocate_memory(size * sizeof(char), J9MEM_CATEGORY_JIT)))
                        return J9VMDLLMAIN_FAILED;
                     returnVal = GET_COMPOUND_VALUE(argIndexXjit, ':', &xjitCommandLineOptions, size);
                     } while (returnVal == OPTION_BUFFER_OVERFLOW);

                  if (!* xjitCommandLineOptions)
                     {
                     j9mem_free_memory(xjitCommandLineOptions);
                     loadInfo->fatalErrorStr = "no arguments for -Xjit:";
                     return J9VMDLLMAIN_FAILED;
                     }
                  }

               codert_onload(vm);

               /* do initializations for -Xaot options */
               if (isAOT && argIndexXaot >= 0)
                  {
                  IDATA returnVal = 0, size = 128;
                  xaotCommandLineOptions = 0;
                  do
                     {
                     size = size * 2;
                     if (xaotCommandLineOptions)
                        j9mem_free_memory(xaotCommandLineOptions);
                     if (!(xaotCommandLineOptions = (char*)j9mem_allocate_memory(size * sizeof(char), J9MEM_CATEGORY_JIT)))
                        return J9VMDLLMAIN_FAILED;
                     returnVal = GET_COMPOUND_VALUE(argIndexXaot, ':', &xaotCommandLineOptions, size);
                     } while (returnVal == OPTION_BUFFER_OVERFLOW);

                  if (!* xaotCommandLineOptions)
                     {
                     j9mem_free_memory(xaotCommandLineOptions);
                     loadInfo->fatalErrorStr = "no arguments for -Xaot:";
                     return J9VMDLLMAIN_FAILED;
                     }
                  }

               jitConfig = vm->jitConfig;

               if (!jitConfig)
                  {
                  loadInfo->fatalErrorStr = "cannot initialize JIT: no jitconfig";
                  return J9VMDLLMAIN_FAILED;
                  }

               if (isQuickstart)
                  jitConfig->runtimeFlags |= J9JIT_QUICKSTART;

               if (aotrtInitialized)
                  jitConfig->runtimeFlags |= J9JIT_AOT_ATTACHED;

               if (jitConfig->runtimeFlags & J9JIT_JIT_ATTACHED)
                  goto _abort;
               if (onLoadInternal(vm, jitConfig, xjitCommandLineOptions, xaotCommandLineOptions, initialFlags, reserved, isJIT?0:1))
                  goto _abort;

               if (isJIT)
                  jitConfig->runtimeFlags |= J9JIT_JIT_ATTACHED;

               // Option string is no longer needed
               /* The following code causes problems on zLinux (only) when using a tracing option
                  The code is commented out until we can figure out the root cause
               if (isJIT && argIndexXjit >= 0 && xjitCommandLineOptions)
                  {
                  j9mem_free_memory(xjitCommandLineOptions);
                  xjitCommandLineOptions = 0;
                  }
               if (isAOT && argIndexXaot >= 0 && xaotCommandLineOptions)
                  {
                  j9mem_free_memory(xaotCommandLineOptions);
                  xaotCommandLineOptions = 0;
                  }
               */
               jitInitialized = true;
               return J9VMDLLMAIN_OK;
               }
            catch (const std::exception &e) {}
            _abort:
            freeJITConfig(jitConfig);
            if (!loadInfo->fatalErrorStr || strlen(loadInfo->fatalErrorStr)==0)
               loadInfo->fatalErrorStr = "cannot initialize JIT";
            return J9VMDLLMAIN_FAILED;
            }
         break;

      // createThreadWithCategory sampling and profiling threads in this stage
      case ABOUT_TO_BOOTSTRAP:
         {
#if defined(J9VM_OPT_SHARED_CLASSES)
         // ENABLE_AOT must be set AND the shared class must be properly initialized
         UDATA aotFlags = J9SHR_RUNTIMEFLAG_CACHE_INITIALIZATION_COMPLETE;
         if (vm->sharedClassConfig && ((vm->sharedClassConfig->runtimeFlags & aotFlags) == aotFlags))
            {
            TR::Options::setSharedClassCache(true); // Set to true as long as cache is present and initialized

            TR_J9VMBase *feWithoutThread = TR_J9VMBase::get(vm->jitConfig, 0);
            TR_J9SharedCache *sharedCache = new (PERSISTENT_NEW) TR_J9SharedCache((TR_J9VMBase *)feWithoutThread);
            if (sharedCache != NULL)
               {
               TR_PersistentMemory * persistentMemory = (TR_PersistentMemory *)(vm->jitConfig->scratchSegment);
               TR_PersistentClassLoaderTable *loaderTable = persistentMemory->getPersistentInfo()->getPersistentClassLoaderTable();
               sharedCache->setPersistentClassLoaderTable(loaderTable);
               loaderTable->setSharedCache(sharedCache);
               }
            }
         else
#endif
            TR::Options::setSharedClassCache(false);

         if (!isAOT)
            {
            /* turn off internal flags if user specified AOT to be turned OFF */
#if defined(J9VM_OPT_SHARED_CLASSES)
            if (vm->sharedClassConfig && (vm->sharedClassConfig->runtimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_AOT))
               {
               // doesn't matter if J9SHR_RUNTIMEFLAG_CACHE_INITIALIZATION_COMPLETE is set or not
               // we're only clearing the ENABLE_AOT flag
               vm->sharedClassConfig->runtimeFlags &= ~J9SHR_RUNTIMEFLAG_ENABLE_AOT;
               }
#endif
            if ( TR::Options::getAOTCmdLineOptions() )
               {
               TR::Options::getAOTCmdLineOptions()->setOption(TR_NoLoadAOT);
               TR::Options::getAOTCmdLineOptions()->setOption(TR_NoStoreAOT);
               TR::Options::setSharedClassCache(false);
               }
            }

         if (TR::Options::sharedClassCache())
            TR_J9SharedCache::setSharedCacheDisabledReason(TR_J9SharedCache::NOT_DISABLED);
         else
            TR_J9SharedCache::setSharedCacheDisabledReason(TR_J9SharedCache::AOT_DISABLED);

         if  (isJIT || isAOT)
            {

            /* Although we've initialized the compiler, we have to tell the compiler whether it can perform JIT compiles
             * This is because the compiler can be configured to only AOT compile. */
            TR::Options::setCanJITCompile(isJIT);

            int32_t rv = aboutToBootstrap(vm, vm->jitConfig);

            jitConfig = vm->jitConfig;

            if (isAOT && !isJIT && TR::Options::getAOTCmdLineOptions()->getOption(TR_NoStoreAOT))
                 TR::Options::getCmdLineOptions()->setOption(TR_DisableInterpreterProfiling, true);


            if (!TR::Options::canJITCompile()) // -Xnojit, then recompilation is not supported
                                              // Ensure JIT and AOT flags are set appropriately
              {
              TR::Options::getAOTCmdLineOptions()->setAllowRecompilation(false);
              TR::Options::getAOTCmdLineOptions()->setOption(TR_DisableCHOpts);

              TR::Options::getJITCmdLineOptions()->setAllowRecompilation(false);
              TR::Options::getJITCmdLineOptions()->setOption(TR_DisableCHOpts);
              }

            if (!isJIT)
               {
               TR::Options::getAOTCmdLineOptions()->setOption(TR_DisableGuardedCountingRecompilations);
               }

            if (rv == -1)
               {
               // cannot free JIT config because shutdown stage expects it to exist
               vm->runtimeFlags &= ~J9_RUNTIME_JIT_ACTIVE;
               if (!loadInfo->fatalErrorStr || strlen(loadInfo->fatalErrorStr)==0)
                  loadInfo->fatalErrorStr = "cannot initialize JIT";
               return J9VMDLLMAIN_FAILED;
               }

            if (rv == 1)
               {
               // cannot free JIT config because shutdown stage expects it to exist
               vm->runtimeFlags &= ~J9_RUNTIME_JIT_ACTIVE;
               printf("Non-Fatal Error: cannot initialize JIT: JVMTI with FSD disabled\n");
               }

            /* If the return value is 0, then continue normally */
            TR::Options::setIsFullyInitialized();
            }

         break;
         }

      case VM_INITIALIZATION_COMPLETE:
         {
         // When the compilation thread was created, it was too early to create an
         // instance of java.lang.Thread (we need to be assume that JCL is ready)
         // Its safe to do this now.
         //
         if  (isJIT || isAOT)
            {
            J9VMThread                          *curThread         = vm->internalVMFunctions->currentVMThread(vm);
            TR::CompilationInfo                  *compInfo          = getCompilationInfo(vm->jitConfig);
            TR::CompilationInfoPerThread * const *arrayOfCompInfoPT = compInfo->getArrayOfCompilationInfoPerThread();
            TR_ASSERT(arrayOfCompInfoPT, "TR::CompilationInfo::_arrayOfCompilationInfoPerThread is null\n");

            for (uint8_t i = 0; i < compInfo->getNumTotalCompilationThreads(); i++)
               {
               TR::CompilationInfoPerThread *curCompThreadInfoPT = arrayOfCompInfoPT[i];
               TR_ASSERT(curCompThreadInfoPT, "a thread's compinfo is missing\n");

               J9VMThread *compThread = curCompThreadInfoPT->getCompilationThread();
               if (!compThread)
                  continue;

               //char threadName[32]; // make sure the name below does not exceed 32 chars
               //sprintf(threadName, "JIT Compilation Thread-%d", curCompThreadInfoPT->getCompThreadId());

               char *threadName = (
                  curCompThreadInfoPT->compilationThreadIsActive() ?
                     curCompThreadInfoPT->getActiveThreadName() :
                     curCompThreadInfoPT->getSuspendedThreadName()
                  );

               vm->internalVMFunctions->initializeAttachedThread(
                  curThread,
                  threadName,
                  vm->systemThreadGroupRef,
                  ((compThread->privateFlags & J9_PRIVATE_FLAGS_DAEMON_THREAD) != 0),
                  compThread
                  );

               if ((curThread->currentException != NULL) || (curThread->threadObject == NULL))
                  {
                  if (!loadInfo->fatalErrorStr || strlen(loadInfo->fatalErrorStr)==0)
                     loadInfo->fatalErrorStr = "cannot create the jit Thread object";
                  return J9VMDLLMAIN_FAILED;
                  }

               TRIGGER_J9HOOK_VM_THREAD_STARTED(vm->hookInterface, curThread, compThread);
               } // end for

            // Give a name to the sampling thread as well
            // The sampling thread (if any) has been created and attached in stage 13
            // It cannot be stopped because that happens in stage 17 while here we are in stage 15
            // In this case we can access the samplingThreadLifetimeState without a monitor because
            // no other thread tries to read or write to it.
            if (compInfo->getSamplerThread())
               {
               TR_ASSERT(compInfo->getSamplingThreadLifetimeState() == TR::CompilationInfo::SAMPLE_THR_ATTACHED,
                  "Sampling thread must be already attached in stage 15\n");
               vm->internalVMFunctions->initializeAttachedThread(
                  curThread,
                  "JIT-SamplerThread",
                  vm->systemThreadGroupRef,
                  ((compInfo->getSamplerThread()->privateFlags & J9_PRIVATE_FLAGS_DAEMON_THREAD) != 0),
                  compInfo->getSamplerThread()
                  );
               if ((curThread->currentException != NULL) || (curThread->threadObject == NULL))
                  {
                  if (!loadInfo->fatalErrorStr || strlen(loadInfo->fatalErrorStr)==0)
                     loadInfo->fatalErrorStr = "cannot create the jit Sampler Thread object";
                  return J9VMDLLMAIN_FAILED;
                  }
               compInfo->setSamplingThreadLifetimeState(TR::CompilationInfo::SAMPLE_THR_INITIALIZED);
               TRIGGER_J9HOOK_VM_THREAD_STARTED(vm->hookInterface, curThread, compInfo->getSamplerThread());
               }

            TR_J9VMBase *fe = TR_J9VMBase::get(vm->jitConfig, 0);
            if (!fe->isAOT_DEPRECATED_DO_NOT_USE())
               TR_AnnotationBase::loadExpectedAnnotationClasses(curThread);

            // Also set name for interpreter profiler thread if it exists
#if defined (J9VM_INTERP_PROFILING_BYTECODES)
            TR_IProfiler *iProfiler = fe->getIProfiler();
            if (iProfiler)
               {
               J9VMThread *iProfilerThread = iProfiler->getIProfilerThread();
               if (iProfilerThread)
                  {
                  vm->internalVMFunctions->initializeAttachedThread
                      (curThread, "IProfiler", vm->systemThreadGroupRef,
                      ((iProfilerThread->privateFlags & J9_PRIVATE_FLAGS_DAEMON_THREAD) != 0),
                      iProfilerThread);
                  if ((curThread->currentException != NULL) || (curThread->threadObject == NULL))
                     {
                     if (!loadInfo->fatalErrorStr || strlen(loadInfo->fatalErrorStr)==0)
                        loadInfo->fatalErrorStr = "cannot create the iProfiler Thread object";
                     return J9VMDLLMAIN_FAILED;
                     }
                  TRIGGER_J9HOOK_VM_THREAD_STARTED(vm->hookInterface, curThread, iProfilerThread);
                  }
               }
#endif

            TR_JProfilerThread *jProfiler = ((TR_JitPrivateConfig*)(vm->jitConfig->privateConfig))->jProfiler;
            if (jProfiler)
               {
               J9VMThread *jProfilerThread = jProfiler->getJProfilerThread();
               if (jProfilerThread)
                  {
                  vm->internalVMFunctions->initializeAttachedThread
                      (curThread, "JProfiler", vm->systemThreadGroupRef,
                      ((jProfilerThread->privateFlags & J9_PRIVATE_FLAGS_DAEMON_THREAD) != 0),
                      jProfilerThread);
                  if ((curThread->currentException != NULL) || (curThread->threadObject == NULL))
                     {
                     if (!loadInfo->fatalErrorStr || strlen(loadInfo->fatalErrorStr)==0)
                        loadInfo->fatalErrorStr = "cannot create the jProfiler Thread object";
                     return J9VMDLLMAIN_FAILED;
                     }
                  TRIGGER_J9HOOK_VM_THREAD_STARTED(vm->hookInterface, curThread, jProfilerThread);
                  }
               }
            }
         }
         break;

      case INTERPRETER_SHUTDOWN:

         if (isJIT || isAOT)
            {
            if (vm->jitConfig)
               {
               TR_J9VMBase *trvm = TR_J9VMBase::get(vm->jitConfig, 0);
               if (!trvm->isAOT_DEPRECATED_DO_NOT_USE() && trvm->_compInfo)
                  {
                  trvm->_compInfo->stopCompilationThreads();
                  }
               JitShutdown(vm->jitConfig);
               }
            }
         break;

      case LIBRARIES_ONUNLOAD :
      case JVM_EXIT_STAGE :
         if (jitInitialized)
            {
            jitConfig = vm->jitConfig;
            if (jitConfig && stage == JVM_EXIT_STAGE)
               JitShutdown(jitConfig);
            //TR_FrontEnd * vm = TR_J9VMBase::get(jitConfig, 0);
            //TR::Compilation::shutdown(vm); // already done in JitShutdown
            j9jit_fclose(((TR_JitPrivateConfig*)jitConfig->privateConfig)->vLogFile);
            ((TR_JitPrivateConfig*)jitConfig->privateConfig)->vLogFile = 0;
            j9jit_fclose(((TR_JitPrivateConfig*)jitConfig->privateConfig)->rtLogFile);
            ((TR_JitPrivateConfig*)jitConfig->privateConfig)->rtLogFile = 0;
            j9jit_fcloseId(jitConfig->tLogFile);
            jitConfig->tLogFile = -1;
            j9jit_fcloseId(jitConfig->tLogFileTemp);
            jitConfig->tLogFileTemp = -1;

            static char * printIPFanInStats = feGetEnv("TR_PrintIPFanInStats");
            if (printIPFanInStats)
               ((TR_JitPrivateConfig*)(jitConfig->privateConfig))->iProfiler->checkMethodHashTable();

            if ( stage != JVM_EXIT_STAGE ) /* If not shutdownDueToExit */
               {
               freeJITConfig(jitConfig);
               }
            jitInitialized = false;
            }

         if (aotrtInitialized)
            {
#if defined(J9VM_INTERP_AOT_RUNTIME_SUPPORT)
            jitConfig = vm->jitConfig;
            if (jitConfig)
               jitConfig->runtimeFlags &= ~J9JIT_AOT_ATTACHED;
#endif
            if ( stage != JVM_EXIT_STAGE )
               codert_OnUnload(vm);
            aotrtInitialized = false;
            }
         break;
      }
   return J9VMDLLMAIN_OK;
   }
