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

#include "control/CompilationRuntime.hpp"
#include "control/Options.hpp"
#include "control/OptionsPostRestore.hpp"

J9::OptionsPostRestore::OptionsPostRestore(J9JITConfig *jitConfig, TR::CompilationInfo *compInfo)
   :
   _jitConfig(jitConfig),
   _compInfo(compInfo),
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
J9::OptionsPostRestore::processCompilerOptions()
   {
   // TODO: Check existing config from before checkpoint
   bool jitEnabled = true;
   bool aotEnabled = true;

   // TODO: find and consume
   // - J9::ExternalOptions::Xjit
   // - J9::ExternalOptions::Xnojit
   // - J9::ExternalOptions::Xaot
   // - J9::ExternalOptions::Xnoaot

   if (_argIndexXjit < _argIndexXnojit)
      jitEnabled = false;

   if (_argIndexXaot < _argIndexXnoaot)
      aotEnabled = false;

   if (jitEnabled || aotEnabled)
      {
      // TODO: Process -Xjit:/-Xaot: options

      // TODO: Based on whether the following is enabled, do necessary compensation
      // - exclude=
      // - limit=
      // - compilationThreads=
      // - disableAsyncCompilation
      // - disabling recompilation (optLevel=, inhibit recomp, etc)

      // TODO: Look into
      // - -Xrs
      // - -Xtune:virtualized

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
   else
      {
      // TODO: Look into what needs to be done if both -Xnojit and -Xaot

      if (!aotEnabled)
         {
         // do necessary work to disable aot compilation
         }

      if (!jitEnabled)
         {
         // do necessary work to disable jit compilation
         }
      }
   }

void
J9::OptionsPostRestore::processOptionsPostRestore(J9JITConfig *jitConfig, TR::CompilationInfo *compInfo)
   {
   J9::OptionsPostRestore optsPostRestore = J9::OptionsPostRestore(jitConfig, compInfo);
   optsPostRestore.processCompilerOptions();
   }

#endif // J9VM_OPT_CRIU_SUPPORT
