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
#ifndef OPTIONS_POST_RESTORE_HPP
#define OPTIONS_POST_RESTORE_HPP

struct J9JITConfig;
struct J9VMThread;
struct J9Method;
namespace TR { class CompilationInfo; }
namespace TR { struct Region; }
struct TR_Memory;
struct TR_J9VMBase;
struct TR_JitPrivateConfig;

namespace J9
{

class OptionsPostRestore
   {
   public:
   /**
    * \brief OptionsPostRestore constructor
    *
    * \param vmThread The J9VMThread
    * \param jitConfig The J9JITConfig
    * \param compInfo The TR::CompilationInfo
    * \param region Reference to a TR::Region
    */
   OptionsPostRestore(J9VMThread *vmThread, J9JITConfig *jitConfig, TR::CompilationInfo *compInfo, TR::Region &region);

   /**
    * \brief  API to process options post restore
    *
    * \param vmThread The J9VMThread
    * \param jitConfig The J9JITConfig
    * \param compInfo The TR::CompilationInfo
    */
   static void processOptionsPostRestore(J9VMThread *vmThread, J9JITConfig *jitConfig, TR::CompilationInfo *compInfo);

   private:

   /**
    * \brief Close the old vlog if needed and open the vlog.
    *
    * \param vLogFileName The name of the vlog specified in the
    *                     post restore options
    */
   void openNewVlog(char *vLogFileName);

   /**
    * \brief Close the old RT log if needed (including the RT log
    *        per compilation thread), and open the new RT log
    *        (including the RT log per compilation thread).
    *
    * \param rtLogFileName The name of the rtLog specified in the
    *                      post restore options
    */
   void openNewRTLog(char *rtLogFileName);

   /**
    * \brief Open vlog and rtLog if specified in the restore run options.
    *        If specified in both, the old log file is closed, and the
    *        new log file is opened.
    */
   void openLogFilesIfNeeded();

   /**
    * \brief Method to iterate and set indices of external options
    *        found in the Restore VM Args Array
    */
   void iterateOverExternalOptions();

   /**
    * \brief Invalidate an existing method body if possible. JNI methods
    *        do not have a bodyinfo/methodinfo and therefore cannot be
    *        invalidated.
    *
    * \param method The J9Method of the method to be filtered
    * \param fej9 The TR_J9VMBase front end
    */
   void invalidateCompiledMethod(J9Method *method, TR_J9VMBase *fej9);

   /**
    * \brief Determine whether a method should be invalidated because the
    *        method should be excluded because of the -Xjit:exclude option
    *
    * \param method The J9Method of the method to be filtered
    * \param fej9 The TR_J9VMBase front end
    * \param compilationFiltersExist bool to indicate whether there are any
    *                                compilation filters
    *
    * \return true if the method should be invalidated, false otherwise.
    */
   bool shouldInvalidateCompiledMethod(J9Method *method, TR_J9VMBase *fej9, bool compilationFiltersExist);

   /**
    * \brief Invalidate methods if needed.
    */
   void invalidateCompiledMethodsIfNeeded(bool invalidateAll = false);

   /**
    * \brief Helper method to disable further AOT compilation.
    *
    * \param disabledPreCheckpoint bool to indicate whther AOT was disable post
    *                              restore or pre checkpoint.
    */
   void disableAOTCompilation(bool disabledPreCheckpoint = false);

   /**
    * \brief Helper method to perform tasks prior to processing
    *        the internal compiler options.
    */
   void preProcessInternalCompilerOptions();

   /**
    * \brief Helper method to post process internal compiler options.
    */
   void postProcessInternalCompilerOptions();

   /**
    * \brief Helper method to process JITServer options post restore.
    */
   void processJitServerOptions();

   /**
    * \brief Helper method to get compiler options (such as -Xjit, -Xaot, etc.)
    *        from the Restore VM Args Array and process them.
    *
    * \param isAOT bool to determine whether the AOT or JIT options should be
    *              processed.
    */
   void processInternalCompilerOptions(bool isAOT);

   /**
    * \brief Method to process compiler options post restore.
    */
   void processCompilerOptions();

   J9JITConfig *_jitConfig;
   J9VMThread *_vmThread;
   TR::CompilationInfo *_compInfo;
   TR::Region &_region;
   TR_JitPrivateConfig *_privateConfig;

   char *_oldVLogFileName;
   char *_oldRtLogFileName;

   bool _asyncCompilationPreCheckpoint;
   bool _disableTrapsPreCheckpoint;
   bool _disableAOTPostRestore;
   bool _enableCodeCacheDisclaimingPreCheckpoint;

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
   int32_t _argIndexJITServerAddress;
   int32_t _argIndexJITServerAOTCacheName;
   int32_t _argIndexIProfileDuringStartupPhase;
   int32_t _argIndexDisableIProfileDuringStartupPhase;
   };

}

#endif // OPTIONS_POST_RESTORE_HPP
