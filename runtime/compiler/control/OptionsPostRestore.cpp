/*******************************************************************************
 * Copyright (c) 2023, 2023 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include "j9cfg.h"

#if defined(J9VM_OPT_CRIU_SUPPORT)

#include "j9.h"
#include "j9nonbuilder.h"
#include "jvminit.h"
#include "env/TRMemory.hpp"
#include "compile/Method.hpp"
#include "control/CompilationRuntime.hpp"
#include "control/Options.hpp"
#include "control/OptionsPostRestore.hpp"
#include "control/J9Recompilation.hpp"
#include "env/SystemSegmentProvider.hpp"
#include "env/RawAllocator.hpp"
#include "env/Region.hpp"
#include "env/VerboseLog.hpp"
#include "env/TRMemory.hpp"
#include "env/VMJ9.h"

#define FIND_AND_CONSUME_RESTORE_ARG(match, optionName, optionValue) FIND_AND_CONSUME_ARG(vm->checkpointState.restoreArgsList, match, optionName, optionValue)
#define FIND_ARG_IN_RESTORE_ARGS(match, optionName, optionValue) FIND_ARG_IN_ARGS(vm->checkpointState.restoreArgsList, match, optionName, optionValue)

J9::OptionsPostRestore::OptionsPostRestore(J9VMThread *vmThread, J9JITConfig *jitConfig, TR::CompilationInfo *compInfo, TR::Region &region)
   :
   _jitConfig(jitConfig),
   _vmThread(vmThread),
   _compInfo(compInfo),
   _region(region),
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
   _argIndexJITServerSSLKey(-1),
   _argIndexJITServerSSLCert(-1),
   _argIndexJITServerUseAOTCache(-1),
   _argIndexDisableJITServerUseAOTCache(-1),
   _argIndexRequireJITServer(-1),
   _argIndexDisableRequireJITServer(-1),
   _argIndexJITServerLogConnections(-1),
   _argIndexDisableJITServerLogConnections(-1),
   _argIndexJITServerAOTmx(-1),
   _argIndexJITServerLocalSyncCompiles(-1),
   _argIndexDisableJITServerLocalSyncCompiles(-1),
   _argIndexJITServerAOTCachePersistence(-1),
   _argIndexDisableJITServerAOTCachePersistence(-1),
   _argIndexJITServerAOTCacheDir(-1),
   _argIndexJITServerAOTCacheName(-1),
   _argIndexJITServerAddress(-1),
   _argIndexJITServerPort(-1),
   _argIndexJITServerTimeout(-1)
   {}

void
J9::OptionsPostRestore::iterateOverExternalOptions()
   {
   int32_t start = static_cast<int32_t>(J9::ExternalOptions::TR_FirstExternalOption);
   int32_t end = static_cast<int32_t>(J9::ExternalOptions::TR_NumExternalOptions);
   for (int32_t option = start; option < end; option++)
      {
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

         case J9::ExternalOptions::Xnodfpbd:
         case J9::ExternalOptions::Xdfpbd:
         case J9::ExternalOptions::Xhysteresis:
         case J9::ExternalOptions::Xnoquickstart:
         case J9::ExternalOptions::Xquickstart:
         case J9::ExternalOptions::XtlhPrefetch:
         case J9::ExternalOptions::XnotlhPrefetch:
         case J9::ExternalOptions::XlockReservation:
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
         case J9::ExternalOptions::XXplusJITServerTechPreviewMessageOption:
         case J9::ExternalOptions::XXminusJITServerTechPreviewMessageOption:
         case J9::ExternalOptions::XXplusMetricsServer:
         case J9::ExternalOptions::XXminusMetricsServer:
         case J9::ExternalOptions::XXJITServerMetricsPortOption:
         case J9::ExternalOptions::XXJITServerMetricsSSLKeyOption:
         case J9::ExternalOptions::XXJITServerMetricsSSLCertOption:
         case J9::ExternalOptions::XXplusJITServerShareROMClassesOption:
         case J9::ExternalOptions::XXminusJITServerShareROMClassesOption:
            {
            // do nothing, maybe consume them to prevent errors
            }
            break;

         case J9::ExternalOptions::XjniAcc:
            {
            // call preProcessJniAccelerator
            }
            break;

         case J9::ExternalOptions::XsamplingExpirationTime:
            {
            // call preProcessSamplingExpirationTime
            }
            break;

         case J9::ExternalOptions::XcompilationThreads:
            {
            // Need to allocate more if needed
            }
            break;

         case J9::ExternalOptions::XaggressivenessLevel:
            {
            // maybe call preProcessMode after this loop
            }
            break;

         case J9::ExternalOptions::XXLateSCCDisclaimTimeOption:
            {
            // set compInfo->getPersistentInfo()->setLateSCCDisclaimTime
            }
            break;

         case J9::ExternalOptions::XXplusPrintCodeCache:
            {
            // set xxPrintCodeCacheArgIndex
            }
            break;

         case J9::ExternalOptions::XXminusPrintCodeCache:
            {
            // set xxDisablePrintCodeCacheArgIndex
            }
            break;

         case J9::ExternalOptions::XXdoNotProcessJitEnvVars:
            {
            // set _doNotProcessEnvVars;
            }
            break;

         case J9::ExternalOptions::XXplusUseJITServerOption:
            {
            // set xxUseJITServerArgIndex
            }
            break;

         case J9::ExternalOptions::XXminusUseJITServerOption:
            {
            // set xxDisableUseJITServerArgIndex
            }
            break;

         case J9::ExternalOptions::XXJITServerAddressOption:
            {
            // set _argIndexJITServerAddress
            }
            break;

         case J9::ExternalOptions::XXJITServerPortOption:
            {
            // set _argIndexJITServerPort
            }
            break;

         case J9::ExternalOptions::XXJITServerTimeoutOption:
            {
            // set _argIndexJITServerTimeout
            }
            break;

         case J9::ExternalOptions::XXJITServerSSLKeyOption:
            {
            // set xxJITServerSSLKeyArgIndex
            }
            break;

         case J9::ExternalOptions::XXJITServerSSLCertOption:
            {
            // set xxJITServerSSLCertArgIndex
            }
            break;

         case J9::ExternalOptions::XXJITServerSSLRootCertsOption:
            {
            // set compInfo->setJITServerSslRootCerts
            }
            break;

         case J9::ExternalOptions::XXplusJITServerUseAOTCacheOption:
            {
            // set xxJITServerUseAOTCacheArgIndex
            }
            break;

         case J9::ExternalOptions::XXminusJITServerUseAOTCacheOption:
            {
            // set xxDisableJITServerUseAOTCacheArgIndex
            }
            break;

         case J9::ExternalOptions::XXplusRequireJITServerOption:
            {
            // set xxRequireJITServerArgIndex
            }
            break;

         case J9::ExternalOptions::XXminusRequireJITServerOption:
            {
            // set xxDisableRequireJITServerArgIndex
            }
            break;

         case J9::ExternalOptions::XXplusJITServerLogConnections:
            {
            // set xxJITServerLogConnectionsArgIndex
            }
            break;

         case J9::ExternalOptions::XXminusJITServerLogConnections:
            {
            // set xxDisableJITServerLogConnectionsArgIndex
            }
            break;

         case J9::ExternalOptions::XXJITServerAOTmxOption:
            {
            // set xxJITServerAOTmxArgIndex
            }
            break;

         case J9::ExternalOptions::XXplusJITServerLocalSyncCompilesOption:
            {
            // set xxJITServerLocalSyncCompilesArgIndex
            }
            break;

         case J9::ExternalOptions::XXminusJITServerLocalSyncCompilesOption:
            {
            // set xxDisableJITServerLocalSyncCompilesArgIndex
            }
            break;

         case J9::ExternalOptions::XXplusJITServerAOTCachePersistenceOption:
            {
            // set xxJITServerAOTCachePersistenceArgIndex
            }
            break;

         case J9::ExternalOptions::XXminusJITServerAOTCachePersistenceOption:
            {
            // set xxDisableJITServerAOTCachePersistenceArgIndex
            }
            break;

         case J9::ExternalOptions::XXJITServerAOTCacheDirOption:
            {
            // set xxJITServerAOTCacheDirArgIndex
            }
            break;

         case J9::ExternalOptions::XXJITServerAOTCacheNameOption:
            {
            // set xxJITServerAOTCacheNameArgIndex
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

         default:
            TR_ASSERT_FATAL(false, "Option %s not addressed post restore\n", TR::Options::_externalOptionStrings[option]);
         }
      }
   }

void
J9::OptionsPostRestore::processJitServerOptions()
   {
   if (_argIndexUseJITServer >= _argIndexDisableUseJITServer)
      {
      // Do JITServer init

      if (_argIndexJITServerAddress >= 0)
         {
         // set compInfo->getPersistentInfo()->setJITServerAddress
         }

      if (_argIndexJITServerPort >= 0)
         {
         // set compInfo->getPersistentInfo()->setJITServerPort
         }

      if (_argIndexJITServerTimeout >= 0)
         {
         // set compInfo->getPersistentInfo()->setSocketTimeout
         }

      if ((_argIndexJITServerSSLKey >= 0) && (_argIndexJITServerSSLCert >= 0))
         {
         // Init SSL key and cert
         }

      if (_argIndexJITServerUseAOTCache > _argIndexDisableJITServerUseAOTCache)
         {
         // compInfo->getPersistentInfo()->setJITServerUseAOTCache(true);
         }
      else
         {
         // compInfo->getPersistentInfo()->setJITServerUseAOTCache(false);
         }

      if (_argIndexRequireJITServer > _argIndexDisableRequireJITServer)
         {
         // compInfo->getPersistentInfo()->setRequireJITServer(true);
         // compInfo->getPersistentInfo()->setSocketTimeout(60000);
         }

      if (_argIndexJITServerLogConnections > _argIndexDisableJITServerLogConnections)
         {
         TR::Options::setVerboseOption(TR_VerboseJITServerConns);
         }

      if (_argIndexJITServerAOTmx > 0)
         {
         // JITServerAOTCacheMap::setCacheMaxBytes
         }

      if (_argIndexJITServerLocalSyncCompiles > _argIndexDisableJITServerLocalSyncCompiles)
         {
         // compInfo->getPersistentInfo()->setLocalSyncCompiles
         }

      if (_argIndexJITServerAOTCachePersistence > _argIndexDisableJITServerAOTCachePersistence)
         {
         // compInfo->getPersistentInfo()->setJITServerUseAOTCachePersistence(true);

         if (_argIndexJITServerAOTCacheDir > 0)
            {
            // compInfo->getPersistentInfo()->setJITServerAOTCacheDir(directory);
            }
         }

      if (_argIndexJITServerAOTCacheName >= 0)
         {
         // compInfo->getPersistentInfo()->setJITServerAOTCacheName(name);
         }
      }
   else
      {
      // Disable JITServer
      }
   }

void
J9::OptionsPostRestore::processInternalCompilerOptions(bool enabled, bool isAOT)
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

   if (enabled && argIndex >= 0)
      {
      intptr_t rc = initializeCompilerArgsPostRestore(_jitConfig->javaVM, argIndex, &commandLineOptions, !isAOT, mergeCompilerOptions);
      if (rc)
         {
         // TODO: Error condition
         }
      }

   if (commandLineOptions)
      {
      char *remainder = TR::Options::processOptionSetPostRestore(_jitConfig, commandLineOptions, options, isAOT);
      if (*remainder)
         {
         // TODO: Error condition
         }
      }
   }

void
J9::OptionsPostRestore::filterMethod(TR_J9VMBase *fej9, J9Method *method)
   {
   if (TR::CompilationInfo::isCompiled(method))
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
         sprintf(methodSignature, "%.*s.%.*s%.*s",
                 J9UTF8_LENGTH(className), utf8Data(className),
                 J9UTF8_LENGTH(name), utf8Data(name),
                 J9UTF8_LENGTH(signature), utf8Data(signature));

         TR_FilterBST *filter = NULL;
         if (!TR::Options::getDebug()->methodSigCanBeCompiled(methodSignature, filter, TR::Method::J9))
            {
            if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCheckpointRestoreDetails))
               TR_VerboseLog::writeLineLocked(TR_Vlog_CHECKPOINT_RESTORE, "Invalidating %s (%p)", methodSignature, method);

            TR::Recompilation::invalidateMethodBody(method->extra, fej9);
            // TODO: add method to a list to check the stack of java threads to print out message
            }
         }
      }
   }

void
J9::OptionsPostRestore::filterMethods()
   {
   PORT_ACCESS_FROM_JITCONFIG(_jitConfig);
   J9JavaVM *javaVM = _jitConfig->javaVM;
   TR_J9VMBase *fej9 = TR_J9VMBase::get(_jitConfig, _vmThread);
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
         filterMethod(fej9, &ramMethods[index]);
         }

      j9clazz = javaVM->internalVMFunctions->allClassesNextDo(&classWalkState);
      }
   javaVM->internalVMFunctions->allClassesEndDo(&classWalkState);
   }

void
J9::OptionsPostRestore::postProcessInternalCompilerOptions()
   {
   // TODO: Based on whether the following is enabled, do necessary compensation
   // - compilationThreads=
   // - disableAsyncCompilation
   // - disabling recompilation (optLevel=, inhibit recomp, etc)
   // - OMR::Options::_logFile (both global and subsets)
   //    - May have to close an existing file and open a new one?

   if (TR::Options::getDebug())
      filterMethods();
   }

void
J9::OptionsPostRestore::processCompilerOptions()
   {
   // Needed for FIND_AND_CONSUME_RESTORE_ARG
   J9JavaVM *vm = _jitConfig->javaVM;

   // TODO: Check existing config from before checkpoint
   bool jitEnabled = true;
   bool aotEnabled = true;

   _argIndexXjit = FIND_AND_CONSUME_RESTORE_ARG(OPTIONAL_LIST_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::Xjit], 0);
   _argIndexXnojit = FIND_AND_CONSUME_RESTORE_ARG(OPTIONAL_LIST_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::Xnojit], 0);
   _argIndexXaot = FIND_AND_CONSUME_RESTORE_ARG(OPTIONAL_LIST_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::Xaot], 0);
   _argIndexXnoaot = FIND_AND_CONSUME_RESTORE_ARG(OPTIONAL_LIST_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::Xnoaot], 0);

   if (_argIndexXjit < _argIndexXnojit)
      jitEnabled = false;

   if (_argIndexXaot < _argIndexXnoaot)
      aotEnabled = false;

   // TODO: Look into what needs to be done if both -Xnojit and -Xaot

   if (!aotEnabled)
      {
      // do necessary work to disable aot compilation
      }

   if (!jitEnabled)
      {
      // do necessary work to disable jit compilation
      }

   if (jitEnabled || aotEnabled)
      {
      _argIndexMergeOptionsEnabled = FIND_AND_CONSUME_RESTORE_ARG(EXACT_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::XXplusMergeCompilerOptions], 0);
      _argIndexMergeOptionsDisabled = FIND_AND_CONSUME_RESTORE_ARG(EXACT_MATCH, J9::Options::_externalOptionStrings[J9::ExternalOptions::XXminusMergeCompilerOptions], 0);

      processInternalCompilerOptions(aotEnabled, true);
      processInternalCompilerOptions(jitEnabled, false);

      postProcessInternalCompilerOptions();

      // TODO: Look into
      // - -Xrs
      // - -Xtune:virtualized
      // - TR::Compiler->target.numberOfProcessors

      iterateOverExternalOptions();

      if (_argIndexPrintCodeCache > _argIndexDisablePrintCodeCache)
         {
         // self()->setOption(TR_PrintCodeCacheUsage);
         }

      if (jitEnabled)
         {
         processJitServerOptions();
         }
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

#endif // J9VM_OPT_CRIU_SUPPORT
