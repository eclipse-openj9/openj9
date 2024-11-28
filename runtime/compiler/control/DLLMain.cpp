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

#define J9_EXTERNAL_TO_VM

#include "control/Options.hpp"
#include "env/ClassLoaderTable.hpp"
#include "env/DependencyTable.hpp"
#include "env/annotations/AnnotationBase.hpp"
#include "env/ut_j9jit.h"
#include "control/CompilationRuntime.hpp"
#include "control/CompilationThread.hpp"
#include "env/VMJ9.h"
#include "runtime/IProfiler.hpp"
#include "runtime/J9Profiler.hpp"
#if defined(J9VM_OPT_JITSERVER)
#include "runtime/Listener.hpp"
#include "runtime/MetricsServer.hpp"
#endif /* J9VM_OPT_JITSERVER */
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

static IDATA initializeCompilerArgs(J9JavaVM* vm,
                                    J9VMDllLoadInfo* loadInfo,
                                    J9VMInitArgs* j9vmArgs,
                                    IDATA argIndex,
                                    char** xCommandLineOptionsPtr,
                                    bool isXjit,
                                    bool mergeCompilerOptions)
   {
   PORT_ACCESS_FROM_JAVAVM(vm);

   char* xCommandLineOptions = NULL;

   const char *VMOPT_WITH_COLON;
   const char *fatalErrorStr = NULL;
   if (isXjit)
      {
      VMOPT_WITH_COLON = J9::Options::_externalOptionStrings[J9::ExternalOptions::Xjitcolon];
      fatalErrorStr = "no arguments for -Xjit:";
      }
   else
      {
      VMOPT_WITH_COLON = J9::Options::_externalOptionStrings[J9::ExternalOptions::Xaotcolon];
      fatalErrorStr = "no arguments for -Xaot:";
      }

   if (mergeCompilerOptions)
      {
      char *xOptions = NULL;
      uint32_t sizeOfOption = 0;
      bool firstOpt = true;

      /* Find first option with colon */
      argIndex = FIND_ARG_IN_ARGS_FORWARD(j9vmArgs, STARTSWITH_MATCH, VMOPT_WITH_COLON, NULL);

      /* Determine size of xCommandLineOptions string */
      while (argIndex >= 0)
         {
         CONSUME_ARG(vm->vmArgsArray, argIndex);
         GET_OPTION_VALUE_ARGS(j9vmArgs, argIndex, ':', &xOptions);

         size_t partialOptLen = 0;

         if (xOptions)
            {
            partialOptLen = strlen(xOptions);
            sizeOfOption += partialOptLen;

            /* Ignore empty options with colon */
            if (!firstOpt && partialOptLen)
               sizeOfOption += 1; // "," needed to combine multiple options with colon
            }

         /* Ignore empty options with colon */
         if (firstOpt && partialOptLen)
            firstOpt = false;

         /* Find next option with colon */
         argIndex = FIND_NEXT_ARG_IN_ARGS_FORWARD(j9vmArgs, STARTSWITH_MATCH, VMOPT_WITH_COLON, NULL, argIndex);
         }

      /* Concatenate options into xCommandLineOptions string */
      if (sizeOfOption)
         {
         size_t partialOptLen = 1;  // \0
         sizeOfOption += partialOptLen;

         if (!(xCommandLineOptions = (char*)j9mem_allocate_memory(sizeOfOption*sizeof(char), J9MEM_CATEGORY_JIT)))
            return J9VMDLLMAIN_FAILED;

         char *cursor = xCommandLineOptions;
         firstOpt = true;

         /* Find first option with colon */
         argIndex = FIND_ARG_IN_ARGS_FORWARD(j9vmArgs, STARTSWITH_MATCH, VMOPT_WITH_COLON, NULL);
         while (argIndex >= 0)
            {
            CONSUME_ARG(j9vmArgs, argIndex);
            GET_OPTION_VALUE_ARGS(j9vmArgs, argIndex, ':', &xOptions);

            partialOptLen = 0;

            if (xOptions)
               {
               partialOptLen = strlen(xOptions);

               /* Ignore empty options with colon */
               if (!firstOpt && partialOptLen)
                  {
                  TR_ASSERT_FATAL((cursor - xCommandLineOptions + 1) < sizeOfOption,
                                  "%s Insufficient space to memcpy \",\";"
                                  "cursor=%p, xCommandLineOptions=%p, sizeOfOption=%d\n",
                                  VMOPT_WITH_COLON, cursor, xCommandLineOptions, sizeOfOption);
                  memcpy(cursor, ",", 1);
                  cursor += 1;
                  }

               TR_ASSERT_FATAL((cursor - xCommandLineOptions + partialOptLen) < sizeOfOption,
                               "%s Insufficient space to memcpy \"%s\";"
                               "cursor=%p, xCommandLineOptions=%p, sizeOfOption=%d\n",
                               VMOPT_WITH_COLON, xOptions, cursor, xCommandLineOptions, sizeOfOption);
               memcpy(cursor, xOptions, partialOptLen);
               cursor += partialOptLen;
               }

            /* Ignore empty options with colon */
            if (firstOpt && partialOptLen)
               firstOpt = false;

            /* Find next option with colon */
            argIndex = FIND_NEXT_ARG_IN_ARGS_FORWARD(j9vmArgs, STARTSWITH_MATCH, VMOPT_WITH_COLON, NULL, argIndex);
            }

         /* At this point, the cursor should be at exactly the last array entry */
         TR_ASSERT_FATAL(cursor == &xCommandLineOptions[sizeOfOption-1],
                        "%s cursor=%p, xCommandLineOptions=%p, sizeOfOption=%d\n",
                        VMOPT_WITH_COLON, cursor, xCommandLineOptions, sizeOfOption);

         /* Add NULL terminator */
         xCommandLineOptions[sizeOfOption-1] = '\0';
         }
      /* If sizeOfOption is 0 then there have been no arguments for (potentially multiple) -Xjit: / -Xaot: */
      else
         {
         vm->internalVMFunctions->setErrorJ9dll(PORTLIB, loadInfo, fatalErrorStr, FALSE);
         return J9VMDLLMAIN_FAILED;
         }
      }
   else
      {
      IDATA returnVal = 0, size = 128;
      do
         {
         size = size * 2;
         if (xCommandLineOptions)
            j9mem_free_memory(xCommandLineOptions);
         if (!(xCommandLineOptions = (char*)j9mem_allocate_memory(size * sizeof(char), J9MEM_CATEGORY_JIT)))
            return J9VMDLLMAIN_FAILED;
         returnVal = GET_COMPOUND_VALUE_ARGS(j9vmArgs, argIndex, ':', &xCommandLineOptions, size);
         } while (returnVal == OPTION_BUFFER_OVERFLOW);

      if (!* xCommandLineOptions)
         {
         j9mem_free_memory(xCommandLineOptions);
         vm->internalVMFunctions->setErrorJ9dll(PORTLIB, loadInfo, fatalErrorStr, FALSE);
         return J9VMDLLMAIN_FAILED;
         }
      }

   *xCommandLineOptionsPtr = xCommandLineOptions;
   return 0;
   }

#if defined(J9VM_OPT_CRIU_SUPPORT)
uintptr_t
initializeCompilerArgsPostRestore(J9JavaVM* vm, intptr_t argIndex, char** xCommandLineOptionsPtr, bool isXjit, bool mergeCompilerOptions)
   {
   J9VMDllLoadInfo* loadInfo = FIND_DLL_TABLE_ENTRY( THIS_DLL_NAME );
   return initializeCompilerArgs(vm, loadInfo, vm->checkpointState.restoreArgsList, argIndex, xCommandLineOptionsPtr, isXjit, mergeCompilerOptions);
   }
#endif

IDATA J9VMDllMain(J9JavaVM* vm, IDATA stage, void * reserved)
   {
   J9JITConfig * jitConfig = 0;
   UDATA initialFlags = 0;
   J9VMDllLoadInfo* loadInfo = FIND_DLL_TABLE_ENTRY( THIS_DLL_NAME );
   char *xjitCommandLineOptions = const_cast<char *>("");
   char *xaotCommandLineOptions = const_cast<char *>("");
   IDATA fullSpeedDebugSet = FALSE;
   IDATA argIndex = 0;

   IDATA tlhPrefetch = 0;
   IDATA notlhPrefetch = 0;
   IDATA lockReservation = 0;
   IDATA argIndexXjit = 0;
   IDATA argIndexXaot = 0;
   IDATA argIndexXnojit = 0;

   IDATA argIndexClient = 0;
   IDATA argIndexServer = 0;
   IDATA argIndexQuickstart = 0;

   IDATA argIndexRIEnabled = 0;
   IDATA argIndexRIDisabled = 0;

   IDATA argIndexPerfEnabled = 0;
   IDATA argIndexPerfDisabled = 0;

   IDATA argIndexMergeOptionsEnabled = 0;
   IDATA argIndexMergeOptionsDisabled = 0;

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
         FIND_AND_CONSUME_VMARG(EXACT_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::Xnodfpbd], 0);
         if (FIND_ARG_IN_VMARGS(EXACT_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::Xdfpbd], 0) >= 0)
            {
            FIND_AND_CONSUME_VMARG( EXACT_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::Xhysteresis], 0);
            }
         FIND_AND_CONSUME_VMARG( EXACT_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::Xnoquickstart], 0); // deprecated
         FIND_AND_CONSUME_VMARG(STARTSWITH_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::Xtuneelastic], 0);
         argIndexQuickstart = FIND_AND_CONSUME_VMARG( EXACT_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::Xquickstart], 0);
         tlhPrefetch = FIND_AND_CONSUME_VMARG(EXACT_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::XtlhPrefetch], 0);
         notlhPrefetch = FIND_AND_CONSUME_VMARG(EXACT_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::XnotlhPrefetch], 0);
         lockReservation = FIND_AND_CONSUME_VMARG(EXACT_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::XlockReservation], 0);
         FIND_AND_CONSUME_VMARG(EXACT_MEMORY_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::Xcodecache], 0);
         FIND_AND_CONSUME_VMARG(STARTSWITH_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::XjniAcc], 0);
         FIND_AND_CONSUME_VMARG(EXACT_MEMORY_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::Xcodecachetotal], 0);
         FIND_AND_CONSUME_VMARG(EXACT_MEMORY_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::XXcodecachetotal], 0);

         FIND_AND_CONSUME_VMARG(STARTSWITH_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::Xlpcodecache], 0);

         FIND_AND_CONSUME_VMARG(EXACT_MEMORY_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::XsamplingExpirationTime], 0);
         FIND_AND_CONSUME_VMARG(EXACT_MEMORY_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::XcompilationThreads], 0);
         FIND_AND_CONSUME_VMARG(EXACT_MEMORY_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::XaggressivenessLevel], 0);
         argIndexXjit = FIND_AND_CONSUME_VMARG(OPTIONAL_LIST_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::Xjit], 0);
         argIndexXaot = FIND_AND_CONSUME_VMARG(OPTIONAL_LIST_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::Xaot], 0);
         argIndexXnojit = FIND_AND_CONSUME_VMARG(OPTIONAL_LIST_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::Xnojit], 0);

         argIndexRIEnabled = FIND_AND_CONSUME_VMARG(EXACT_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::XXplusRuntimeInstrumentation], 0);
         argIndexRIDisabled = FIND_AND_CONSUME_VMARG(EXACT_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::XXminusRuntimeInstrumentation], 0);

         // Determine if user disabled Runtime Instrumentation
         if (argIndexRIEnabled >= 0 || argIndexRIDisabled >= 0)
            TR::Options::_hwProfilerEnabled = (argIndexRIDisabled > argIndexRIEnabled) ? TR_no : TR_yes;

         argIndexPerfEnabled = FIND_AND_CONSUME_VMARG(EXACT_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::XXplusPerfTool], 0);
         argIndexPerfDisabled = FIND_AND_CONSUME_VMARG(EXACT_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::XXminusPerfTool], 0);

         // Determine if user disabled PerfTool
         if (argIndexPerfEnabled >= 0 || argIndexPerfDisabled >= 0)
            TR::Options::_perfToolEnabled = (argIndexPerfDisabled > argIndexPerfEnabled) ? TR_no : TR_yes;

         TR::Options::_doNotProcessEnvVars = (FIND_AND_CONSUME_VMARG(EXACT_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::XXdoNotProcessJitEnvVars], 0) >= 0);

         isQuickstart = J9_ARE_ANY_BITS_SET(vm->extendedRuntimeFlags2, J9_EXTENDED_RUNTIME2_TUNE_QUICKSTART);

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
         if ((J9_ARE_ANY_BITS_SET(vm->extendedRuntimeFlags2, J9_EXTENDED_RUNTIME2_ENABLE_AOT)))
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
            bool disableZeroedTLHPages = disableDualTLH && (((notlhPrefetch >= 0) || (!TR::Compiler->target.cpu.is(OMR_PROCESSOR_PPC_P6) && !TR::Compiler->target.cpu.is(OMR_PROCESSOR_PPC_P7))));
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
               J9VMDllLoadInfo *gcLoadInfo = getGCDllLoadInfo(vm);

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
               argIndexMergeOptionsEnabled = FIND_AND_CONSUME_VMARG(EXACT_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::XXplusMergeCompilerOptions], 0);
               argIndexMergeOptionsDisabled = FIND_AND_CONSUME_VMARG(EXACT_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::XXminusMergeCompilerOptions], 0);

               // Determine if user wants to merge compiler options
               bool mergeCompilerOptions = false;
               if (argIndexMergeOptionsEnabled >= 0 || argIndexMergeOptionsDisabled >= 0)
                  mergeCompilerOptions = (argIndexMergeOptionsEnabled > argIndexMergeOptionsDisabled);

               /*
                * Note that the option prefix we need to match includes the colon.
                */
               argIndexXjit = FIND_ARG_IN_VMARGS( STARTSWITH_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::Xjitcolon], 0);
               argIndexXaot = FIND_ARG_IN_VMARGS( STARTSWITH_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::Xaotcolon], 0);

               /* do initializations for -Xjit options */
               if (isJIT && argIndexXjit >= 0)
                  {
                  IDATA rc = initializeCompilerArgs(vm, loadInfo, vm->vmArgsArray, argIndexXjit, &xjitCommandLineOptions, true, mergeCompilerOptions);
                  if (rc)
                     return rc;
                  }

               codert_onload(vm);

               /* do initializations for -Xaot options */
               if (isAOT && argIndexXaot >= 0)
                  {
                  IDATA rc = initializeCompilerArgs(vm, loadInfo, vm->vmArgsArray, argIndexXaot, &xaotCommandLineOptions, false, mergeCompilerOptions);
                  if (rc)
                     return rc;
                  }

               jitConfig = vm->jitConfig;

               if (!jitConfig)
                  {
                  vm->internalVMFunctions->setErrorJ9dll(PORTLIB, loadInfo, "cannot initialize JIT: no jitconfig", FALSE);
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
               vm->internalVMFunctions->setErrorJ9dll(PORTLIB, loadInfo, "cannot initialize JIT", FALSE);
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
               TR_PersistentMemory *persistentMemory = (TR_PersistentMemory *)(vm->jitConfig->scratchSegment);
               auto persistentInfo = persistentMemory->getPersistentInfo();
               TR_PersistentClassLoaderTable *loaderTable = persistentInfo->getPersistentClassLoaderTable();
               sharedCache->setPersistentClassLoaderTable(loaderTable);
               loaderTable->setSharedCache(sharedCache);

#if !defined(PERSISTENT_COLLECTIONS_UNSUPPORTED)
               if (persistentInfo->getTrackAOTDependencies() && !TR::Options::getCmdLineOptions()->getOption(TR_DisableCHOpts))
                  {
                  TR_AOTDependencyTable *dependencyTable = new (PERSISTENT_NEW) TR_AOTDependencyTable(sharedCache);
                  persistentInfo->setAOTDependencyTable(dependencyTable);
                  }
#endif /* !defined(PERSISTENT_COLLECTIONS_UNSUPPORTED) */
               if (!persistentInfo->getAOTDependencyTable())
                  persistentInfo->setTrackAOTDependencies(false);
               }
            }
         else
#endif /* defined(J9VM_OPT_SHARED_CLASSES) */
            {
            TR::Options::setSharedClassCache(false);
            }

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

            if (isAOT
                && TR::Options::getAOTCmdLineOptions()->getOption(TR_EnableClassChainValidationCaching)
                && (TR::Options::getCmdLineOptions()->getOption(TR_DisableCHOpts)))
               {
               TR::Options::getAOTCmdLineOptions()->setOption(TR_EnableClassChainValidationCaching, false);
               }

            if (rv == -1)
               {
               // cannot free JIT config because shutdown stage expects it to exist
               vm->runtimeFlags &= ~J9_RUNTIME_JIT_ACTIVE;
               if (!loadInfo->fatalErrorStr || strlen(loadInfo->fatalErrorStr)==0)
                  vm->internalVMFunctions->setErrorJ9dll(PORTLIB, loadInfo, "cannot initialize JIT", FALSE);
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

            for (int32_t i = 0; i < compInfo->getNumTotalAllocatedCompilationThreads(); i++)
               {
               TR::CompilationInfoPerThread *curCompThreadInfoPT = arrayOfCompInfoPT[i];
               TR_ASSERT(curCompThreadInfoPT, "a thread's compinfo is missing\n");

               J9VMThread *compThread = curCompThreadInfoPT->getCompilationThread();
               if (!compThread)
                  continue;

               //char threadName[32]; // make sure the name below does not exceed 32 chars
               //snprintf(threadName, sizeof(threadName), "JIT Compilation Thread-%d", curCompThreadInfoPT->getCompThreadId());

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
                  if ((NULL == loadInfo->fatalErrorStr) || ('\0' == loadInfo->fatalErrorStr[0]))
                     vm->internalVMFunctions->setErrorJ9dll(PORTLIB, loadInfo, "cannot create the jit Thread object", FALSE);
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
                  if ((NULL == loadInfo->fatalErrorStr) || ('\0' == loadInfo->fatalErrorStr[0]))
                     vm->internalVMFunctions->setErrorJ9dll(PORTLIB, loadInfo, "cannot create the jit Sampler Thread object", FALSE);
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
                     if ((NULL == loadInfo->fatalErrorStr) || ('\0' == loadInfo->fatalErrorStr[0]))
                        vm->internalVMFunctions->setErrorJ9dll(PORTLIB, loadInfo, "cannot create the iProfiler Thread object", FALSE);
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
                     if ((NULL == loadInfo->fatalErrorStr) || ('\0' == loadInfo->fatalErrorStr[0]))
                        vm->internalVMFunctions->setErrorJ9dll(PORTLIB, loadInfo, "cannot create the jProfiler Thread object", FALSE);
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
#if defined(J9VM_OPT_JITSERVER)
                  if (trvm->_compInfo->getPersistentInfo()->getRemoteCompilationMode() == JITServer::SERVER)
                     {
                     TR_Listener *listener = ((TR_JitPrivateConfig*)(vm->jitConfig->privateConfig))->listener;
                     if (listener)
                        {
                        listener->stop();
                        }
                     MetricsServer *metricsServer = ((TR_JitPrivateConfig*)(vm->jitConfig->privateConfig))->metricsServer;
                     if (metricsServer)
                        {
                        metricsServer->stop();
                        }
                     }

#endif /* defined(J9VM_OPT_JITSERVER) */
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
