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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
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
#include "env/ClassUnloadMonitorCriticalSection.hpp"
#include "env/Region.hpp"
#include "env/VerboseLog.hpp"
#include "env/VMJ9.h"
#include "ilgen/IlGeneratorMethodDetails.hpp"

TR::CompileBeforeCheckpoint::CompileBeforeCheckpoint(TR::Region &region, J9VMThread *vmThread, TR_J9VMBase *fej9, TR::CompilationInfo *compInfo) :
   _region(region),
   _vmThread(vmThread),
   _fej9(fej9),
   _compInfo(compInfo),
   _methodsSet(std::less<TR_OpaqueMethodBlock*>(), _region)
   {}

void
TR::CompileBeforeCheckpoint::addMethodToList(TR_OpaqueMethodBlock *method)
   {
   if (method && _methodsSet.find(method) == _methodsSet.end())
      {
      _methodsSet.insert(method);
      }
   }

void
TR::CompileBeforeCheckpoint::queueMethodsForCompilationBeforeCheckpoint()
   {
   for (auto iter = _methodsSet.begin(); iter != _methodsSet.end(); iter++)
      {
      TR_OpaqueMethodBlock *method = *iter;
      J9Method *j9method = reinterpret_cast<J9Method *>(method);

      J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(j9method);
      if (
          /* Method is interpreted */
          !_compInfo->isCompiled(j9method)

          /* Method is not a thunk archetype */
          && !_J9ROMMETHOD_J9MODIFIER_IS_SET(romMethod, J9AccMethodFrameIteratorSkip)

          /* Method is not abstract */
          && !_J9ROMMETHOD_J9MODIFIER_IS_SET(romMethod, J9AccAbstract)
         )
         {
         if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCheckpointRestoreDetails))
            {
            TR_VerboseLog::CriticalSection cs;
            TR_VerboseLog::write(TR_Vlog_CHECKPOINT_RESTORE, "Attempting to queue ");
            _compInfo->printMethodNameToVlog(j9method);
            TR_VerboseLog::writeLine(" (%p) for compilation", j9method);
            }

         TR_MethodEvent event;
         event._eventType = TR_MethodEvent::CompilationBeforeCheckpoint;
         event._j9method = j9method;
         event._oldStartPC = 0;
         event._vmThread = _vmThread;
         event._classNeedingThunk = 0;
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
               _compInfo->compileMethod(_vmThread, details, NULL, TR_maybe, NULL, &queued, plan);
               }

            if (!queued && newPlanCreated)
               TR_OptimizationPlan::freeOptimizationPlan(plan);
            }
         }
      }
   }

void
TR::CompileBeforeCheckpoint::collectAndCompileMethodsBeforeCheckpoint()
   {
   /* Acquire Class Unload Monitor to prevent class unloading */
   TR::ClassUnloadMonitorCriticalSection collectMethodsCriticalSection(true);

   collectMethodsForCompilationBeforeCheckpoint();
   queueMethodsForCompilationBeforeCheckpoint();
   }

#endif
