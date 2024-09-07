/*******************************************************************************
 * Copyright IBM Corp. and others 2022
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

#include "j9cfg.h"

#if defined(J9VM_OPT_CRIU_SUPPORT)

#include "j9protos.h"
#include "j9nonbuilder.h"
#include "control/CompilationStrategy.hpp"
#include "control/CompilationController.hpp"
#include "control/CompilationRuntime.hpp"
#include "control/CompileBeforeCheckpoint.hpp"
#include "control/OptimizationPlan.hpp"
#include "control/Recompilation.hpp"
#include "control/RecompilationInfo.hpp"
#include "env/ClassUnloadMonitorCriticalSection.hpp"
#include "env/Region.hpp"
#include "env/VerboseLog.hpp"
#include "env/VMAccessCriticalSection.hpp"
#include "env/VMJ9.h"
#include "ilgen/IlGeneratorMethodDetails.hpp"
#include "infra/CriticalSection.hpp"
#include "runtime/CRRuntime.hpp"
#include "runtime/J9VMAccess.hpp"

TR::CompileBeforeCheckpoint::CompileBeforeCheckpoint(TR::Region &region, J9VMThread *vmThread, TR_J9VMBase *fej9, TR::CompilationInfo *compInfo) :
   _region(region),
   _vmThread(vmThread),
   _fej9(fej9),
   _compInfo(compInfo)
   {}

void
TR::CompileBeforeCheckpoint::queueMethodForCompilationBeforeCheckpoint(J9Method *j9method, bool recomp)
   {
   /* Do this before releasing the CRRuntime Monitor */
   if (_compInfo->isJNINative(j9method))
      {
      _compInfo->getCRRuntime()->pushJNIAddr(j9method, j9method->extra);
      }

   /* Release CR Runtime Monitor since compileMethod below will acquire the
    * Comp Monitor.
    */
   _compInfo->getCRRuntime()->releaseCRRuntimeMonitor();

   J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(j9method);
   if (
         /* Method is interpreted or recomp */
         (!_compInfo->isCompiled(j9method) || recomp)

#if !defined(J9VM_OPT_OPENJDK_METHODHANDLE)
         /* Method is not a thunk archetype */
         && !_J9ROMMETHOD_J9MODIFIER_IS_SET(romMethod, J9AccMethodFrameIteratorSkip)
#endif /* !defined(J9VM_OPT_OPENJDK_METHODHANDLE) */

         /* Method is not abstract */
         && !_J9ROMMETHOD_J9MODIFIER_IS_SET(romMethod, J9AccAbstract)
      )
      {
      if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCheckpointRestoreDetails))
         {
         TR_VerboseLog::CriticalSection cs;
         TR_VerboseLog::write(TR_Vlog_CHECKPOINT_RESTORE, "Attempting to queue ");
         _compInfo->printMethodNameToVlog(j9method);
         TR_VerboseLog::writeLine(" (%p) for %scompilation", j9method, recomp ? "re" : "");
         }

      TR_MethodEvent event;
      if (recomp)
         {
         event._eventType = TR_MethodEvent::ForcedRecompilationPostRestore;
         event._j9method = j9method;
         event._oldStartPC = j9method->extra;
         event._vmThread = _vmThread;
         event._classNeedingThunk = 0;
         }
      else
         {
         event._eventType = TR_MethodEvent::CompilationBeforeCheckpoint;
         event._j9method = j9method;
         event._oldStartPC = 0;
         event._vmThread = _vmThread;
         event._classNeedingThunk = 0;
         }

      bool newPlanCreated;
      TR_OptimizationPlan *plan = TR::CompilationController::getCompilationStrategy()->processEvent(&event, &newPlanCreated);
      // the plan needs to be created only when async compilation is possible
      // Otherwise the compilation will be triggered on next invocation
      if (plan)
         {
         bool queued = false;

            // scope for details
            {
            TR::IlGeneratorMethodDetails details(j9method);
            if (recomp)
               {
               TR_PersistentJittedBodyInfo *bodyInfo = TR::Recompilation::getJittedBodyInfoFromPC(j9method->extra);
               TR_PersistentMethodInfo *methodInfo = bodyInfo->getMethodInfo();
               methodInfo->setReasonForRecompilation(TR_PersistentMethodInfo::RecompDueToCRIU);

               TR::Recompilation::induceRecompilation(_fej9, j9method->extra, &queued, plan);
               }
            else
               {
               _compInfo->compileMethod(_vmThread, details, NULL, TR_maybe, NULL, &queued, plan);
               }
            }

         if (!queued && newPlanCreated)
            TR_OptimizationPlan::freeOptimizationPlan(plan);
         }
      }

   /* Reacquire the CR Runtime Monitor */
   _compInfo->getCRRuntime()->acquireCRRuntimeMonitor();
   }

void
TR::CompileBeforeCheckpoint::queueMethodsForCompilationBeforeCheckpoint()
   {
   /* Acquire VMAccess to prevent class unloading. VMAccess is also needed
    * when a thread queues a method for synchronous compilation.
    */
   TR::VMAccessCriticalSection vmaCollectMethods(_fej9);

   /* Acquire the CRRuntime Monitor */
   OMR::CriticalSection collectMethods(_compInfo->getCRRuntime()->getCRRuntimeMonitor());

   J9Method *method;
   while ((method = _compInfo->getCRRuntime()->popImportantMethodForCR()))
      {
      queueMethodForCompilationBeforeCheckpoint(method);
      }

   while ((method = _compInfo->getCRRuntime()->popFailedCompilation()))
      {
      if (_compInfo->getJ9MethodVMExtra(method) == J9_JIT_NEVER_TRANSLATE)
         {
         TR::CompilationInfo::setInvocationCount(method, 0);
         }
      queueMethodForCompilationBeforeCheckpoint(method);
      }

   while ((method = _compInfo->getCRRuntime()->popForcedRecompilation()))
      {
      queueMethodForCompilationBeforeCheckpoint(method, true);
      }
   }

void
TR::CompileBeforeCheckpoint::compileMethodsBeforeCheckpoint()
   {
   J9JavaVM *javaVM = _compInfo->getJITConfig()->javaVM;

   /* If running portable CRIU, don't bother compiling proactively */
   if (!javaVM->internalVMFunctions->isNonPortableRestoreMode(_vmThread))
      return;

   /* Release the Comp Monitor, since compileMethod should be called without it
    * in hand. If running in sync mode, having the monitor in hand before
    * calling compileMethod will result in a deadlock; although
    * compileOnSeparateThread releases the compMonitor, the entry count at that
    * point will be 1, and so this thread will still be the owner of the
    * monitor.
    */
   _compInfo->releaseCompMonitor(_vmThread);

   /* Queue methods for proactive compilation */
   queueMethodsForCompilationBeforeCheckpoint();

   /* Reacquire the Comp Monitor */
   _compInfo->acquireCompMonitor(_vmThread);
   }

#endif
