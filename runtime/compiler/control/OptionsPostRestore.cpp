/*******************************************************************************
 * Copyright IBM Corp. and others 2023
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

#include "j9.h"
#include "j9nonbuilder.h"
#include "jvminit.h"
#include "j9comp.h"
#include "j9jitnls.h"
#include "jitprotos.h"
#include "env/CompilerEnv.hpp"
#include "env/TRMemory.hpp"
#include "compile/Method.hpp"
#include "control/CompilationRuntime.hpp"
#include "control/CompilationThread.hpp"
#include "control/Options.hpp"
#include "control/OptionsPostRestore.hpp"
#include "control/J9Recompilation.hpp"
#include "env/J9PersistentInfo.hpp"
#include "env/SystemSegmentProvider.hpp"
#include "env/RawAllocator.hpp"
#include "env/Region.hpp"
#include "env/VerboseLog.hpp"
#include "env/TRMemory.hpp"
#include "env/VMJ9.h"
#if defined(J9VM_OPT_JITSERVER)
#include "net/ClientStream.hpp"
#endif
#include "runtime/CodeRuntime.hpp"
#include "runtime/CRRuntime.hpp"

#define FIND_AND_CONSUME_RESTORE_ARG(match, optionName, optionValue) FIND_AND_CONSUME_ARG(vm->checkpointState.restoreArgsList, match, optionName, optionValue)
#define FIND_ARG_IN_RESTORE_ARGS(match, optionName, optionValue) FIND_ARG_IN_ARGS(vm->checkpointState.restoreArgsList, match, optionName, optionValue)
#define GET_INTEGER_VALUE_RESTORE_ARGS(element, optname, result) GET_INTEGER_VALUE_ARGS(vm->checkpointState.restoreArgsList, element, optname, result)
#define GET_OPTION_VALUE_RESTORE_ARGS(element, delimChar, resultPtr) GET_OPTION_VALUE_ARGS(vm->checkpointState.restoreArgsList, element, delimChar, resultPtr)

J9::OptionsPostRestore::OptionsPostRestore(J9VMThread *vmThread, J9JITConfig *jitConfig, TR::CompilationInfo *compInfo, TR::Region &region)
   :
   _jitConfig(jitConfig),
   _vmThread(vmThread),
   _compInfo(compInfo),
   _region(region),
   _privateConfig((TR_JitPrivateConfig*)_jitConfig->privateConfig),
   _oldVLogFileName(_privateConfig->vLogFileName),
   _oldRtLogFileName(_privateConfig->rtLogFileName),
   _asyncCompilationPreCheckpoint(_compInfo->asynchronousCompilation()),
   _argIndexXjit(-1),
   _argIndexXjitcolon(-1),
   _argIndexXnojit(-1),
   _argIndexXaot(-1),
   _argIndexXaotcolon(-1),
   _argIndexXnoaot(-1),
   _argIndexMergeOptionsEnabled(-1),
   _argIndexMergeOptionsDisabled(-1),
   _argIndexPrintCodeCache(-1),
   _argIndexDisablePrintCodeCache(-1),
   _argIndexUseJITServer(-1),
   _argIndexDisableUseJITServer(-1),
   _argIndexJITServerAddress(-1),
   _argIndexJITServerAOTCacheName(-1),
   _argIndexIProfileDuringStartupPhase(-1),
   _argIndexDisableIProfileDuringStartupPhase(-1)
   {
   J9JavaVM *vm = jitConfig->javaVM;
   if (vm->sharedClassConfig)
      _disableAOTPostRestore = (vm->sharedCacheAPI->xShareClassCacheDisabledOnCRIURestore == TRUE);
   else
      _disableAOTPostRestore = false;

   TR::Options *options = TR::Options::getCmdLineOptions();
   _disableTrapsPreCheckpoint
      = J9::Options::_xrsSync
        || options->getOption(TR_NoResumableTrapHandler)
        || options->getOption(TR_DisableTraps);

   _enableCodeCacheDisclaimingPreCheckpoint = options->getOption(TR_EnableCodeCacheDisclaiming);
   }

void
J9::OptionsPostRestore::iterateOverExternalOptions()
   {
   // Needed for FIND_AND_CONSUME_RESTORE_ARG
   J9JavaVM *vm = _jitConfig->javaVM;

   int32_t start = static_cast<int32_t>(J9::ExternalOptions::TR_FirstExternalOption);
   int32_t end = static_cast<int32_t>(J9::ExternalOptions::TR_NumExternalOptions);
   for (int32_t option = start; option < end; option++)
      {
      const char *optString = J9::Options::_externalOptionStrings[option];
      switch (option)
         {
         case J9::ExternalOptions::Xjit:
         case J9::ExternalOptions::Xjitcolon:
         case J9::ExternalOptions::Xnojit:
         case J9::ExternalOptions::Xaot:
         case J9::ExternalOptions::Xaotcolon:
         case J9::ExternalOptions::Xnoaot:
         case J9::ExternalOptions::XXplusMergeCompilerOptions:
         case J9::ExternalOptions::XXminusMergeCompilerOptions:
            {
            // These will have already been consumed
            }
            break;

         case J9::ExternalOptions::XXJITServerPortOption:
         case J9::ExternalOptions::XXJITServerTimeoutOption:
         case J9::ExternalOptions::XXJITServerSSLKeyOption:
         case J9::ExternalOptions::XXJITServerSSLCertOption:
         case J9::ExternalOptions::XXJITServerSSLRootCertsOption:
         case J9::ExternalOptions::XXplusJITServerUseAOTCacheOption:
         case J9::ExternalOptions::XXminusJITServerUseAOTCacheOption:
         case J9::ExternalOptions::XXplusRequireJITServerOption:
         case J9::ExternalOptions::XXminusRequireJITServerOption:
         case J9::ExternalOptions::XXplusJITServerLogConnections:
         case J9::ExternalOptions::XXminusJITServerLogConnections:
         case J9::ExternalOptions::XXJITServerAOTmxOption:
         case J9::ExternalOptions::XXplusJITServerLocalSyncCompilesOption:
         case J9::ExternalOptions::XXminusJITServerLocalSyncCompilesOption:
            {
            // These will be processed in processJitServerOptions
            }
            break;

         case J9::ExternalOptions::Xnodfpbd:
         case J9::ExternalOptions::Xdfpbd:
         case J9::ExternalOptions::Xhysteresis:
         case J9::ExternalOptions::Xnoquickstart:
         case J9::ExternalOptions::Xquickstart:
         case J9::ExternalOptions::XtlhPrefetch:
         case J9::ExternalOptions::XnotlhPrefetch:
         case J9::ExternalOptions::XlockReservation:
         case J9::ExternalOptions::XjniAcc:
         case J9::ExternalOptions::Xnoclassgc:
         case J9::ExternalOptions::Xlp:
         case J9::ExternalOptions::Xlpcodecache:
         case J9::ExternalOptions::Xcodecache:
         case J9::ExternalOptions::Xcodecachetotal:
         case J9::ExternalOptions::XXcodecachetotal:
         case J9::ExternalOptions::XXdeterministic:
         case J9::ExternalOptions::XXplusRuntimeInstrumentation:
         case J9::ExternalOptions::XXminusRuntimeInstrumentation:
         case J9::ExternalOptions::XXplusPerfTool:
         case J9::ExternalOptions::XXminusPerfTool:
         case J9::ExternalOptions::XXdoNotProcessJitEnvVars:
         case J9::ExternalOptions::XXplusJITServerTechPreviewMessageOption:
         case J9::ExternalOptions::XXminusJITServerTechPreviewMessageOption:
         case J9::ExternalOptions::XXplusMetricsServer:
         case J9::ExternalOptions::XXminusMetricsServer:
         case J9::ExternalOptions::XXJITServerMetricsPortOption:
         case J9::ExternalOptions::XXJITServerMetricsSSLKeyOption:
         case J9::ExternalOptions::XXJITServerMetricsSSLCertOption:
         case J9::ExternalOptions::XXplusJITServerShareROMClassesOption:
         case J9::ExternalOptions::XXminusJITServerShareROMClassesOption:
         case J9::ExternalOptions::XXplusJITServerAOTCachePersistenceOption:
         case J9::ExternalOptions::XXminusJITServerAOTCachePersistenceOption:
         case J9::ExternalOptions::XXJITServerAOTCacheDirOption:
         case J9::ExternalOptions::XXcodecachetotalMaxRAMPercentage:
         case J9::ExternalOptions::XXplusJITServerAOTCacheDelayMethodRelocation:
         case J9::ExternalOptions::XXminusJITServerAOTCacheDelayMethodRelocation:
         case J9::ExternalOptions::XXplusJITServerAOTCacheIgnoreLocalSCC:
         case J9::ExternalOptions::XXminusJITServerAOTCacheIgnoreLocalSCC:
         case J9::ExternalOptions::XXplusHealthProbes:
         case J9::ExternalOptions::XXminusHealthProbes:
         case J9::ExternalOptions::XXJITServerHealthProbePortOption:
         case J9::ExternalOptions::XXplusTrackAOTDependencies:
         case J9::ExternalOptions::XXminusTrackAOTDependencies:
            {
            // do nothing, consume them to prevent errors
            FIND_AND_CONSUME_RESTORE_ARG(OPTIONAL_LIST_MATCH, optString, 0);
            }
            break;

         case J9::ExternalOptions::XsamplingExpirationTime:
            {
            int32_t argIndex = FIND_AND_CONSUME_RESTORE_ARG(EXACT_MEMORY_MATCH, optString, 0);
            if (argIndex >= 0)
               {
               UDATA expirationTime;
               IDATA ret = GET_INTEGER_VALUE_RESTORE_ARGS(argIndex, optString, expirationTime);
               if (ret == OPTION_OK)
                  TR::Options::_samplingThreadExpirationTime = expirationTime;
               }
            }
            break;

         case J9::ExternalOptions::XcompilationThreads:
            {
            int32_t argIndex = FIND_AND_CONSUME_RESTORE_ARG(EXACT_MEMORY_MATCH, optString, 0);
            if (argIndex >= 0)
               {
               UDATA numCompThreads;
               IDATA ret = GET_INTEGER_VALUE_RESTORE_ARGS(argIndex, optString, numCompThreads);

               if (ret == OPTION_OK && numCompThreads > 0)
                  TR::Options::_numUsableCompilationThreads = numCompThreads;
               }
            }
            break;

         case J9::ExternalOptions::XaggressivenessLevel:
            {
            // maybe call preProcessMode after this loop
            }
            break;

         case J9::ExternalOptions::XXLateSCCDisclaimTimeOption:
            {
            int32_t argIndex = FIND_AND_CONSUME_RESTORE_ARG(STARTSWITH_MATCH, optString, 0);
            if (argIndex >= 0)
               {
               UDATA disclaimMs = 0;
               IDATA ret = GET_INTEGER_VALUE_RESTORE_ARGS(argIndex, optString, disclaimMs);
               if (ret == OPTION_OK)
                  {
                  _compInfo->getPersistentInfo()->setLateSCCDisclaimTime(((uint64_t) disclaimMs) * 1000000);
                  }
               }
            }
            break;

         case J9::ExternalOptions::XXplusPrintCodeCache:
            {
            _argIndexPrintCodeCache = FIND_ARG_IN_RESTORE_ARGS(EXACT_MATCH, optString, 0);
            }
            break;

         case J9::ExternalOptions::XXminusPrintCodeCache:
            {
            _argIndexDisablePrintCodeCache = FIND_ARG_IN_RESTORE_ARGS(EXACT_MATCH, optString, 0);
            }
            break;

         case J9::ExternalOptions::XXplusUseJITServerOption:
            {
            _argIndexUseJITServer = FIND_ARG_IN_RESTORE_ARGS(EXACT_MATCH, optString, 0);
            }
            break;

         case J9::ExternalOptions::XXminusUseJITServerOption:
            {
            _argIndexDisableUseJITServer = FIND_ARG_IN_RESTORE_ARGS(EXACT_MATCH, optString, 0);
            }
            break;

         case J9::ExternalOptions::XXJITServerAddressOption:
            {
            _argIndexJITServerAddress = FIND_ARG_IN_RESTORE_ARGS(STARTSWITH_MATCH, optString, 0);
            }
            break;

         case J9::ExternalOptions::XXJITServerAOTCacheNameOption:
            {
            _argIndexJITServerAOTCacheName = FIND_ARG_IN_RESTORE_ARGS(STARTSWITH_MATCH, optString, 0);
            }
            break;

         case J9::ExternalOptions::Xlockword:
            {
            // TBD
            }
            break;

         case J9::ExternalOptions::Xtuneelastic:
            {
            // TODO
            }
            break;

         case J9::ExternalOptions::XXplusIProfileDuringStartupPhase:
            {
            _argIndexIProfileDuringStartupPhase = FIND_ARG_IN_RESTORE_ARGS(EXACT_MATCH, optString, 0);
            }
            break;

         case J9::ExternalOptions::XXminusIProfileDuringStartupPhase:
            {
            _argIndexDisableIProfileDuringStartupPhase = FIND_ARG_IN_RESTORE_ARGS(EXACT_MATCH, optString, 0);
            }
            break;

         default:
            TR_ASSERT_FATAL(false, "Option %s not addressed post restore\n", TR::Options::_externalOptionStrings[option]);
         }
      }
   }

void
J9::OptionsPostRestore::processJitServerOptions()
   {
#if defined(J9VM_OPT_JITSERVER)
   bool jitserverEnabled
      = ((_argIndexUseJITServer > _argIndexDisableUseJITServer)
          && !_compInfo->getCRRuntime()->remoteCompilationExplicitlyDisabledAtBootstrap())
        || ((_argIndexUseJITServer == _argIndexDisableUseJITServer)
             && _compInfo->getCRRuntime()->remoteCompilationRequestedAtBootstrap());

   if (jitserverEnabled)
      {
      // Needed for GET_OPTION_VALUE_RESTORE_ARGS
      J9JavaVM *vm = _jitConfig->javaVM;

      // Parse common options
      if (!TR::Options::JITServerParseCommonOptions(vm->checkpointState.restoreArgsList, vm, _compInfo))
         {
         // TODO: Error condition
         }

      // Parse local sync compiles
      TR::Options::JITServerParseLocalSyncCompiles(vm->checkpointState.restoreArgsList,
                                                   vm,
                                                   _compInfo,
                                                   TR::Options::getCmdLineOptions()->getOption(TR_FullSpeedDebug));

      if (_argIndexJITServerAddress >= 0)
         {
         char *address = NULL;
         GET_OPTION_VALUE_RESTORE_ARGS(_argIndexJITServerAddress, '=', &address);
         _compInfo->getPersistentInfo()->setJITServerAddress(address);
         }

      if (_argIndexJITServerAOTCacheName >= 0)
         {
         char *name = NULL;
         GET_OPTION_VALUE_RESTORE_ARGS(_argIndexJITServerAOTCacheName, '=', &name);
         _compInfo->getPersistentInfo()->setJITServerAOTCacheName(name);
         }

      uint64_t oldClientUID = _compInfo->getPersistentInfo()->getClientUID();

      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCheckpointRestore))
         TR_VerboseLog::writeLineLocked(TR_Vlog_CHECKPOINT_RESTORE, "oldClientUID = %llu", oldClientUID);

      // Re-compute client UID post restore
      uint64_t clientUID = JITServerHelpers::generateUID();
      while(clientUID == oldClientUID)
         clientUID = JITServerHelpers::generateUID();

      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCheckpointRestore))
         TR_VerboseLog::writeLineLocked(TR_Vlog_CHECKPOINT_RESTORE, "clientUID = %llu", clientUID);

      _jitConfig->clientUID = clientUID;
      _compInfo->getPersistentInfo()->setClientUID(clientUID);
      _compInfo->getPersistentInfo()->setServerUID(0);
      _compInfo->getCRRuntime()->setCanPerformRemoteCompilationInCRIUMode(true);

      // If encryption is desired, load and initialize the SSL
      if (_compInfo->useSSL())
         {
         bool loaded = JITServer::loadLibsslAndFindSymbols();
         TR_ASSERT_FATAL(loaded, "Terminating the JVM because it failed to load the SSL library");

         int rc = JITServer::ClientStream::static_init(_compInfo);
         TR_ASSERT_FATAL(rc == 0, "Terminating the JVM because it failed to initialize the SSL library");
         }
      }
   else
      {
      _compInfo->getCRRuntime()->setCanPerformRemoteCompilationInCRIUMode(false);
      _compInfo->getPersistentInfo()->setClientUID(0);
      _compInfo->getPersistentInfo()->setServerUID(0);
      _jitConfig->clientUID = 0;
      _jitConfig->serverUID = 0;
      J9::PersistentInfo::_remoteCompilationMode = JITServer::NONE;
      }
#endif // defined(J9VM_OPT_JITSERVER)
   }

void
J9::OptionsPostRestore::processInternalCompilerOptions(bool isAOT)
   {
   // Needed for FIND_ARG_IN_RESTORE_ARGS
   J9JavaVM *vm = _jitConfig->javaVM;

   char *commandLineOptions = NULL;
   bool mergeCompilerOptions = (_argIndexMergeOptionsEnabled > _argIndexMergeOptionsDisabled);

   // TODO: ensure the global cmdline options exist
   TR::Options *options = isAOT ? TR::Options::getAOTCmdLineOptions() : TR::Options::getCmdLineOptions();

   int32_t argIndex;
   if (isAOT)
      argIndex = FIND_ARG_IN_RESTORE_ARGS( STARTSWITH_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::Xaotcolon], 0);
   else
      argIndex = FIND_ARG_IN_RESTORE_ARGS( STARTSWITH_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::Xjitcolon], 0);

   if (argIndex >= 0)
      {
      intptr_t rc = initializeCompilerArgsPostRestore(_jitConfig->javaVM, argIndex, &commandLineOptions, !isAOT, mergeCompilerOptions);
      if (rc)
         {
         // TODO: Error condition
         }
      }

   if (commandLineOptions)
      {
      const char *remainder = TR::Options::processOptionSetPostRestore(_jitConfig, commandLineOptions, options, isAOT);
      if (*remainder)
         {
         // TODO: Error condition
         }
      }
   }

void
J9::OptionsPostRestore::invalidateCompiledMethod(J9Method *method, TR_J9VMBase *fej9)
   {
   void *startPC = _compInfo->getPCIfCompiled(method);
   TR_PersistentJittedBodyInfo *bodyInfo = TR::Recompilation::getJittedBodyInfoFromPC(startPC);

   if (bodyInfo)
      {
      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCheckpointRestoreDetails))
         {
         TR_VerboseLog::CriticalSection();
         TR_VerboseLog::write(TR_Vlog_CHECKPOINT_RESTORE, "Invalidating ");
         _compInfo->printMethodNameToVlog(method);
         TR_VerboseLog::writeLine(" (%p)", method);
         }

      TR_PersistentMethodInfo *pmi = bodyInfo->getMethodInfo();
      pmi->setIsExcludedPostRestore();

      TR::Recompilation::invalidateMethodBody(startPC, fej9);

      // TODO: add method to a list to check the stack of java threads to print out message
      }
   else
      {
      bool isNative =_J9ROMMETHOD_J9MODIFIER_IS_SET((J9_ROM_METHOD_FROM_RAM_METHOD(method)), J9AccNative);
      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCheckpointRestoreDetails))
         {
         TR_VerboseLog::CriticalSection();
         TR_VerboseLog::write(TR_Vlog_CHECKPOINT_RESTORE, "Unable to invalidate %smethod ", isNative ? "native " : "");
         _compInfo->printMethodNameToVlog(method);
         TR_VerboseLog::writeLine(" (%p)", method);
         }
      }
   }

bool
J9::OptionsPostRestore::shouldInvalidateCompiledMethod(J9Method *method, TR_J9VMBase *fej9, bool compilationFiltersExist)
   {
   bool shouldFilterMethod = false;

   if (compilationFiltersExist)
      {
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
         methodSignature = (char *)_region.allocate(len);

      if (methodSignature)
         {
         snprintf(methodSignature, len, "%.*s.%.*s%.*s",
               J9UTF8_LENGTH(className), utf8Data(className),
               J9UTF8_LENGTH(name), utf8Data(name),
               J9UTF8_LENGTH(signature), utf8Data(signature));

         TR_FilterBST *filter = NULL;
         shouldFilterMethod = !TR::Options::getDebug()->methodSigCanBeCompiled(methodSignature, filter, TR::Method::J9);
         }
      }

   return shouldFilterMethod;
   }

void
J9::OptionsPostRestore::invalidateCompiledMethodsIfNeeded(bool invalidateAll)
   {
   TR_J9VMBase *fej9 = TR_J9VMBase::get(_jitConfig, _vmThread);
   bool compilationFiltersExist = (TR::Options::getDebug() && TR::Options::getDebug()->getCompilationFilters());

   if (invalidateAll || compilationFiltersExist)
      {
      J9JavaVM *javaVM = _jitConfig->javaVM;
      PORT_ACCESS_FROM_JAVAVM(javaVM);

      TR_Memory trMemory(*_compInfo->persistentMemory(), _region);

      J9ClassWalkState classWalkState;
      J9Class *j9clazz = javaVM->internalVMFunctions->allClassesStartDo(&classWalkState, javaVM, NULL);
      while (j9clazz)
         {
         TR_OpaqueClassBlock *clazz = reinterpret_cast<TR_OpaqueClassBlock *>(j9clazz);
         uint32_t numMethods = fej9->getNumMethods(clazz);
         J9Method *ramMethods = reinterpret_cast<J9Method *>(fej9->getMethods(clazz));

         for (uint32_t index = 0; index < numMethods; index++)
            {
            J9Method *method = &ramMethods[index];
            if (TR::CompilationInfo::isCompiled(method))
               {
               if (invalidateAll ||
                   shouldInvalidateCompiledMethod(method, fej9, compilationFiltersExist))
                  {
                  invalidateCompiledMethod(method, fej9);
                  if (!invalidateAll)
                     _compInfo->getCRRuntime()->removeMethodsFromMemoizedCompilations<J9Method>(method);
                  }
               }
            }

         j9clazz = javaVM->internalVMFunctions->allClassesNextDo(&classWalkState);
         }
      javaVM->internalVMFunctions->allClassesEndDo(&classWalkState);

      if (invalidateAll)
         {
         _compInfo->getCRRuntime()->purgeMemoizedCompilations();
         _compInfo->getCRRuntime()->resetJNIAddr();
         }

      j9nls_printf(PORTLIB, (UDATA) J9NLS_WARNING, J9NLS_JIT_CHECKPOINT_RESTORE_CODE_INVALIDATED);
      }
   }

void
J9::OptionsPostRestore::disableAOTCompilation(bool disabledPreCheckpoint)
   {
   static bool aotDisabled = false;
   if (aotDisabled)
      return;

   // Ideally when disabling AOT, the vmThread->aotVMwithThreadInfo->_sharedCache
   // would also be freed and set to NULL. However, it isn't obvious whether non
   // compilation and java threads that could be running at the moment could make
   // use of it, in which case there could be a race. To avoid this, it's safer
   // to just leave _sharedCache alone; at worst some SCC API could be invoked,
   // but not during a compilation as all compilation threads are suspended at
   // this point, and so it won't impact AOT code.

   PORT_ACCESS_FROM_JITCONFIG(_jitConfig);

   if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCheckpointRestore))
      TR_VerboseLog::writeLineLocked(TR_Vlog_CHECKPOINT_RESTORE, "Disabling AOT Compilation and Load");

   TR::Options::getAOTCmdLineOptions()->setOption(TR_NoLoadAOT);
   TR::Options::getAOTCmdLineOptions()->setOption(TR_NoStoreAOT);
   TR::Options::setSharedClassCache(false);
   TR_J9SharedCache::setSharedCacheDisabledReason(TR_J9SharedCache::AOT_DISABLED);

   aotDisabled = true;
   _disableAOTPostRestore = true;

   if (disabledPreCheckpoint)
      {
      j9nls_printf(PORTLIB, (UDATA) J9NLS_WARNING, J9NLS_JIT_CHECKPOINT_RESTORE_AOT_DISABLED_PRE_CHECKPOINT);
      }
   else
      {
      j9nls_printf(PORTLIB, (UDATA) J9NLS_WARNING, J9NLS_JIT_CHECKPOINT_RESTORE_AOT_DISABLED);
      }
   }

void
J9::OptionsPostRestore::openNewVlog(char *vLogFileName)
   {
   TR_VerboseLog::vlogAcquire();

   if (_oldVLogFileName)
      {
      TR_ASSERT_FATAL(vLogFileName,
                      "vlogFileName cannot be NULL if _oldVLogFileName (%s) is not NULL\n",
                      _oldVLogFileName);

      TR_ASSERT_FATAL(_privateConfig->vLogFile,
                      "_privateConfig->vLogFile should not be NULL if _oldVLogFileName (%s) is not NULL\n",
                      _oldVLogFileName);

      j9jit_fclose(_privateConfig->vLogFile);
      TR::Options::jitPersistentFree(_oldVLogFileName);
      _oldVLogFileName = NULL;
      }

   _privateConfig->vLogFile = fileOpen(TR::Options::getCmdLineOptions(), _jitConfig, vLogFileName, "wb", true);
   TR::Options::setVerboseOptions(_privateConfig->verboseFlags);

   TR_VerboseLog::vlogRelease();
   }

void
J9::OptionsPostRestore::openNewRTLog(char *rtLogFileName)
   {
   bool closeOldFile = (_oldRtLogFileName != NULL);

   JITRT_LOCK_LOG(_jitConfig);

   if (closeOldFile)
      {
      TR_ASSERT_FATAL(rtLogFileName,
                      "rtLogFileName cannot be NULL if _oldRtLogFileName (%s) is not NULL\n",
                      _oldRtLogFileName);

      TR_ASSERT_FATAL(_privateConfig->rtLogFile,
                      "_privateConfig->rtLogFile should not be NULL if _oldRtLogFileName (%s) is not NULL\n",
                      _oldRtLogFileName);

      j9jit_fclose(_privateConfig->rtLogFile);
      TR::Options::jitPersistentFree(_oldRtLogFileName);
      _oldRtLogFileName = NULL;
      }

   _privateConfig->rtLogFile = fileOpen(TR::Options::getCmdLineOptions(), _jitConfig, rtLogFileName, "wb", true);

   JITRT_UNLOCK_LOG(_jitConfig);

   TR::CompilationInfoPerThread * const * arrayOfCompInfoPT = _compInfo->getArrayOfCompilationInfoPerThread();
   for (int32_t i = _compInfo->getFirstCompThreadID(); i <= _compInfo->getLastCompThreadID(); i++)
      {
      TR::CompilationInfoPerThread *compThreadInfoPT = arrayOfCompInfoPT[i];
      if (closeOldFile)
         {
         compThreadInfoPT->closeRTLogFile();
         }
      compThreadInfoPT->openRTLogFile();
      }
   }

void
J9::OptionsPostRestore::openLogFilesIfNeeded()
   {
   _privateConfig->vLogFileName = _jitConfig->vLogFileName;

   char *vLogFileName = _privateConfig->vLogFileName;
   char *rtLogFileName = _privateConfig->rtLogFileName;

   // if _oldVLogFileName != vLogFileName, it is not possible that
   // _oldVLogFileName != NULL && vLogFileName == NULL
   if (_oldVLogFileName != vLogFileName)
      {
      openNewVlog(vLogFileName);
      }

   // if _oldRtLogFileName != rtLogFileName, it is not possible that
   // _oldRtLogFileName != NULL && rtLogFileName == NULL
   if (_oldRtLogFileName != rtLogFileName)
      {
      openNewRTLog(rtLogFileName);
      }
   }

void
J9::OptionsPostRestore::preProcessInternalCompilerOptions()
   {
   // Needed for FIND_AND_CONSUME_RESTORE_ARG
   J9JavaVM *vm = _jitConfig->javaVM;

   // Re-initialize the number of processors
   _compInfo->initCPUEntitlement();
   uint32_t numProc = _compInfo->getNumTargetCPUs();
   TR::Compiler->host.setNumberOfProcessors(numProc);
   TR::Compiler->target.setNumberOfProcessors(numProc);
   TR::Compiler->relocatableTarget.setNumberOfProcessors(numProc);

   // Find and consume -XX:[+|-]MergeCompilerOptions
   _argIndexMergeOptionsEnabled = FIND_AND_CONSUME_RESTORE_ARG(EXACT_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::XXplusMergeCompilerOptions], 0);
   _argIndexMergeOptionsDisabled = FIND_AND_CONSUME_RESTORE_ARG(EXACT_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::XXminusMergeCompilerOptions], 0);
   }

void
J9::OptionsPostRestore::postProcessInternalCompilerOptions()
   {
   J9JavaVM *vm = _jitConfig->javaVM;
   // TODO: Based on whether the following is enabled, do necessary compensation
   // - disabling recompilation (optLevel=, inhibit recomp, etc)
   // - OMR::Options::_logFile (both global and subsets)
   //    - May have to close an existing file and open a new one?

   // Ensure that log files are not suppressed if tracing is enabled post restore
   if (TR::Options::requiresDebugObject())
      TR::Options::suppressLogFileBecauseDebugObjectNotCreated(false);

   // Need to set number of usable threads before opening logs;
   // there is one rtLog per comp thread
   if (TR::Options::_numUsableCompilationThreads != _compInfo->getNumUsableCompilationThreads())
      _compInfo->setNumUsableCompilationThreadsPostRestore(TR::Options::_numUsableCompilationThreads);

   // Open vlog and rtLog if applicable
   openLogFilesIfNeeded();

   // If -Xrs, -Xtrace, -Xdump, or disabling traps is specified
   // post-restore (and not pre-checkpoint), invalidate all method bodies
   bool invalidateAll = false;
   bool disableAOT = _disableAOTPostRestore;
   bool exceptionCatchEventHooked
      = J9_EVENT_IS_HOOKED(vm->hookInterface, J9HOOK_VM_EXCEPTION_CATCH)
        || J9_EVENT_IS_RESERVED(vm->hookInterface, J9HOOK_VM_EXCEPTION_CATCH);
   bool exceptionThrowEventHooked
      = J9_EVENT_IS_HOOKED(vm->hookInterface, J9HOOK_VM_EXCEPTION_THROW)
        || J9_EVENT_IS_RESERVED(vm->hookInterface, J9HOOK_VM_EXCEPTION_THROW);

   if (!_disableTrapsPreCheckpoint
       && (J9_ARE_ALL_BITS_SET(vm->sigFlags, J9_SIG_XRS_SYNC)
           || TR::Options::getCmdLineOptions()->getOption(TR_NoResumableTrapHandler)
           || TR::Options::getCmdLineOptions()->getOption(TR_DisableTraps)))
      {
      J9:Options::_xrsSync = J9_ARE_ALL_BITS_SET(vm->sigFlags, J9_SIG_XRS_SYNC);
      invalidateAll = true;
      disableAOT = true;
      }
   else if (!_compInfo->getCRRuntime()->isVMMethodTraceEnabled()
            && (vm->extendedRuntimeFlags & J9_EXTENDED_RUNTIME_METHOD_TRACE_ENABLED))
      {
      _compInfo->getCRRuntime()->setVMMethodTraceEnabled(true);
      invalidateAll = true;
      disableAOT = true;
      }
   else if (!_compInfo->getCRRuntime()->isVMExceptionEventsHooked()
            && (exceptionCatchEventHooked || exceptionThrowEventHooked))
      {
      if (exceptionCatchEventHooked)
         _jitConfig->jitExceptionCaught = jitExceptionCaught;
      _compInfo->getCRRuntime()->setVMExceptionEventsHooked(true);
      invalidateAll = true;
      disableAOT = true;
      }

   bool doAOT = !disableAOT;
   TR::Options::FSDInitStatus fsdStatus = TR::Options::resetFSD(vm, _vmThread, doAOT);
   disableAOT = !doAOT;

   if (!_compInfo->getCRRuntime()->isFSDEnabled()
       && fsdStatus == TR::Options::FSDInitStatus::FSDInit_Initialized)
      {
      _compInfo->getCRRuntime()->setFSDEnabled(true);
      invalidateAll = true;
      disableAOT = true;
      }

   // Invalidate method bodies if needed
   invalidateCompiledMethodsIfNeeded(invalidateAll);

   // Disable AOT compilation if needed
   if (disableAOT)
      {
      disableAOTCompilation();
      }
   // If at this point AOT is not disabled, then reset all the options and
   // validate the AOT Header if the validation was delayed pre-checkpoint
   else if (TR_J9SharedCache::aotHeaderValidationDelayed())
      {
      _jitConfig->javaVM->sharedClassConfig->runtimeFlags |= J9SHR_RUNTIMEFLAG_ENABLE_AOT;
      TR::Options::getAOTCmdLineOptions()->setOption(TR_NoLoadAOT, false);
      TR::Options::getAOTCmdLineOptions()->setOption(TR_NoStoreAOT, false);
      TR::Options::setSharedClassCache(true);
      TR_J9SharedCache::setSharedCacheDisabledReason(TR_J9SharedCache::NOT_DISABLED);
      TR_J9SharedCache::resetAOTHeaderValidationDelayed();

      TR_J9SharedCache::validateAOTHeader(_jitConfig, _vmThread, _compInfo);

      if (TR::Options::getAOTCmdLineOptions()->getOption(TR_NoStoreAOT))
         {
         _jitConfig->javaVM->sharedClassConfig->runtimeFlags &= ~J9SHR_RUNTIMEFLAG_ENABLE_AOT;
         TR_J9SharedCache::setSharedCacheDisabledReason(TR_J9SharedCache::AOT_DISABLED);
         }
      }

   // Set option to print code cache if necessary
   if (_argIndexPrintCodeCache > _argIndexDisablePrintCodeCache)
      TR::Options::getCmdLineOptions()->setOption(TR_PrintCodeCacheUsage);

   // If pre-checkpoint, the JVM was run with count=0, then if post-restore
   // the count is > 0, compilations will still happen synchronously. Also,
   // because there is no enableAsyncCompilation, if pre-checkpoint the JVM
   // was run with disableAsyncCompilation, post-restore will continue to have
   // sync compilations.
   //
   // On the other hand, if pre-checkpoint the JVM was NOT run with
   // disableAsyncCompilation OR count=0, then post-restore if these options
   // are specified, they will NOT (for now) result in synchronous compilations.
   if (_asyncCompilationPreCheckpoint)
      {
      if (TR::Options::getCmdLineOptions()->getOption(TR_DisableAsyncCompilation))
         TR::Options::getCmdLineOptions()->setOption(TR_DisableAsyncCompilation, false);
      }

   // Set/Reset TR_NoIProfilerDuringStartupPhase if -XX:[+/-]IProfileDuringStartupPhase is used
   // Otherwise use the default logic to determine if TR_NoIProfilerDuringStartupPhase is set
   if ((_argIndexIProfileDuringStartupPhase >= 0) || (_argIndexDisableIProfileDuringStartupPhase >= 0))
      {
      bool IProfileDuringStartupPhase = (_argIndexIProfileDuringStartupPhase > _argIndexDisableIProfileDuringStartupPhase);
      TR::Options::getCmdLineOptions()->setOption(TR_NoIProfilerDuringStartupPhase, !IProfileDuringStartupPhase);
      }
   else if (!disableAOT)
      {
      if (!TR::Options::getCmdLineOptions()->getOption(TR_DisablePersistIProfile) &&
         J9_ARE_ALL_BITS_SET(vm->sharedClassConfig->runtimeFlags, J9SHR_RUNTIMEFLAG_ENABLE_CACHE_NON_BOOT_CLASSES))
         {
         if (_compInfo->isWarmSCC() == TR_yes)
            {
            TR::Options::getCmdLineOptions()->setOption(TR_NoIProfilerDuringStartupPhase);
            }
         }
      }


   if (!_enableCodeCacheDisclaimingPreCheckpoint &&
       TR::Options::getCmdLineOptions()->getOption(TR_EnableCodeCacheDisclaiming))
      {
      TR::Options::getCmdLineOptions()->setOption(TR_EnableCodeCacheDisclaiming, false);

      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerbosePerformance))
          TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "WARNING: Code Cache disclaiming disabled since it was disabled before checkpoint");
      }

   if (!TR::Options::getCmdLineOptions()->getOption(TR_DisableDataCacheDisclaiming) ||
       !TR::Options::getCmdLineOptions()->getOption(TR_DisableIProfilerDataDisclaiming) ||
       TR::Options::getCmdLineOptions()->getOption(TR_EnableCodeCacheDisclaiming) ||
       TR::Options::getCmdLineOptions()->getOption(TR_EnableSharedCacheDisclaiming))
      {
      TR::Options::disableMemoryDisclaimIfNeeded(_jitConfig);
      }
   }

void
J9::OptionsPostRestore::processCompilerOptions()
   {
   // Needed for FIND_AND_CONSUME_RESTORE_ARG
   J9JavaVM *vm = _jitConfig->javaVM;
   PORT_ACCESS_FROM_JAVAVM(vm);

   bool aotEnabledPreCheckpoint = TR_J9SharedCache::aotHeaderValidationDelayed() ? true : TR::Options::sharedClassCache();

   bool jitEnabled = TR::Options::canJITCompile();
   bool aotEnabled = aotEnabledPreCheckpoint;

   _argIndexXjit = FIND_AND_CONSUME_RESTORE_ARG(OPTIONAL_LIST_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::Xjit], 0);
   _argIndexXnojit = FIND_AND_CONSUME_RESTORE_ARG(OPTIONAL_LIST_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::Xnojit], 0);
   _argIndexXaot = FIND_AND_CONSUME_RESTORE_ARG(OPTIONAL_LIST_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::Xaot], 0);
   _argIndexXnoaot = FIND_AND_CONSUME_RESTORE_ARG(OPTIONAL_LIST_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::Xnoaot], 0);

   if (_argIndexXjit != _argIndexXnojit)
      jitEnabled = (_argIndexXjit > _argIndexXnojit);

   // If -Xnoaot was specified pre-checkpoint, there is a lot of infrastructure
   // that needs to be set up. For now, ignore -Xaot post-restore if -Xnoaot
   // was specified pre-checkpoint.
   if (aotEnabledPreCheckpoint)
      aotEnabled = (_argIndexXaot >= _argIndexXnoaot);

   if (!aotEnabled)
      {
      _disableAOTPostRestore = true;
      }

   if (_disableAOTPostRestore)
      {
      aotEnabled = false;
      disableAOTCompilation(!aotEnabledPreCheckpoint);
      }

   if (!jitEnabled)
      {
      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCheckpointRestore))
         TR_VerboseLog::writeLineLocked(TR_Vlog_CHECKPOINT_RESTORE, "Disabling JIT Compilation");

      TR::Options::setCanJITCompile(false);
      invalidateCompiledMethodsIfNeeded(true);

      j9nls_printf(PORTLIB, (UDATA) J9NLS_WARNING, J9NLS_JIT_CHECKPOINT_RESTORE_JIT_COMPILATION_DISABLED);
      }
   else
      {
      TR::Options::setCanJITCompile(true);
      }

   if (jitEnabled || aotEnabled)
      {
      // Preprocess compiler options
      preProcessInternalCompilerOptions();

      // Process -Xjit / -Xaot options
      if (aotEnabled)
         processInternalCompilerOptions(true);
      if (jitEnabled)
         processInternalCompilerOptions(false);

      // TODO: Look into
      // - -Xrs
      // - -Xtune:virtualized

      // Iterate over external options
      iterateOverExternalOptions();

      // Process JITServer Options
      if (jitEnabled)
         processJitServerOptions();

      // Postprocess compiler options
      postProcessInternalCompilerOptions();
      }
   }

void
J9::OptionsPostRestore::processOptionsPostRestore(J9VMThread *vmThread, J9JITConfig *jitConfig, TR::CompilationInfo *compInfo)
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
      TR::Region postRestoreOptionsRegion(regionSegmentProvider, rawAllocator);

      J9::OptionsPostRestore optsPostRestore = J9::OptionsPostRestore(vmThread, jitConfig, compInfo, postRestoreOptionsRegion);
      optsPostRestore.processCompilerOptions();
      }
   catch (const std::exception &e)
      {
      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCheckpointRestore))
         TR_VerboseLog::writeLineLocked(TR_Vlog_CHECKPOINT_RESTORE, "Failed to process options post restore");
      }
   }
