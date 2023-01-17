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
#ifndef OPTIONS_POST_RESTORE_HPP
#define OPTIONS_POST_RESTORE_HPP

#include "j9cfg.h"

#if defined(J9VM_OPT_CRIU_SUPPORT)

struct J9JITConfig;
namespace TR { class CompilationInfo; }

namespace J9
{

class OptionsPostRestore
   {
   public:
   /**
    * \brief OptionsPostRestore constructor
    *
    * \param jitConfig The J9JITConfig
    * \param compInfo The TR::CompilationInfo
    */
   OptionsPostRestore(J9JITConfig *jitConfig, TR::CompilationInfo *compInfo);

   /**
    * Public API to process options post restore
    *
    * \param jitConfig The J9JITConfig
    * \param compInfo The TR::CompilationInfo
    */
   static void processOptionsPostRestore(J9JITConfig *jitConfig, TR::CompilationInfo *compInfo);

   private:

   /**
    * Helper Method to iterate and set indices of external options
    * found in the Restore VM Args Array
    */
   void iterateOverExternalOptions();

   /**
    * Helper method to process JITServer options post restore.
    */
   void processJitServerOptions();

   /**
    * Helper method to get compiler options (such as -Xjit, -Xaot, etc.)
    * from the Restore VM Args Array and process them.
    */
   void processInternalCompilerOptions(bool enabled, bool isAOT);

   /**
    * Method to process compiler options post restore.
    */
   void processCompilerOptions();

   J9JITConfig *_jitConfig;
   TR::CompilationInfo *_compInfo;

   int32_t _argIndexXjit;
   int32_t _argIndexXjitcolon;
   int32_t _argIndexXnojit;
   int32_t _argIndexXaot;
   int32_t _argIndexXaotcolon;
   int32_t _argIndexXnoaot;
   int32_t _argIndexMergeOptionsEnabled;
   int32_t _argIndexMergeOptionsDisabled;
   int32_t _argIndexPrintCodeCache;
   int32_t _argIndexDisablePrintCodeCache;
   int32_t _argIndexUseJITServer;
   int32_t _argIndexDisableUseJITServer;
   int32_t _argIndexJITServerSSLKey;
   int32_t _argIndexJITServerSSLCert;
   int32_t _argIndexJITServerUseAOTCache;
   int32_t _argIndexDisableJITServerUseAOTCache;
   int32_t _argIndexRequireJITServer;
   int32_t _argIndexDisableRequireJITServer;
   int32_t _argIndexJITServerLogConnections;
   int32_t _argIndexDisableJITServerLogConnections;
   int32_t _argIndexJITServerAOTmx;
   int32_t _argIndexJITServerLocalSyncCompiles;
   int32_t _argIndexDisableJITServerLocalSyncCompiles;
   int32_t _argIndexJITServerAOTCachePersistence;
   int32_t _argIndexDisableJITServerAOTCachePersistence;
   int32_t _argIndexJITServerAOTCacheDir;
   int32_t _argIndexJITServerAOTCacheName;
   int32_t _argIndexJITServerAddress;
   int32_t _argIndexJITServerPort;
   int32_t _argIndexJITServerTimeout;
   };

}

#endif // J9VM_OPT_CRIU_SUPPORT

#endif // OPTIONS_POST_RESTORE_HPP
