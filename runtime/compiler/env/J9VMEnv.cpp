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

#include "compile/Compilation.hpp"
#include "control/Options.hpp"
#include "control/CompilationRuntime.hpp"
#include "control/CompilationThread.hpp"
#include "env/VMEnv.hpp"
#include "env/CompilerEnv.hpp"
#include "env/VMJ9.h"
#include "exceptions/JITShutDown.hpp"
#include "infra/Assert.hpp"
#include "infra/Monitor.hpp"
#include "infra/MonitorTable.hpp"
#include "runtime/J9VMAccess.hpp"
#include "j9.h"
#include "j9cfg.h"
#include "jilconsts.h"
#include "vmaccess.h"

int64_t
J9::VMEnv::maxHeapSizeInBytes()
   {
   J9JavaVM *jvm = TR::Compiler->javaVM;

   if (!jvm)
      return -1;

   J9MemoryManagerFunctions * mmf = jvm->memoryManagerFunctions;
   return (int64_t) mmf->j9gc_get_maximum_heap_size(jvm);
   }


UDATA
J9::VMEnv::heapBaseAddress()
   {
   return 0;
   }


bool
J9::VMEnv::hasAccess(OMR_VMThread *omrVMThread)
   {
   return TR::Compiler->vm.hasAccess(self()->J9VMThreadFromOMRVMThread(omrVMThread));
   }


bool
J9::VMEnv::hasAccess(J9VMThread *j9VMThread)
   {
   return (j9VMThread->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS) ? true : false;
   }


bool
J9::VMEnv::hasAccess(TR::Compilation *comp)
   {
   return comp->fej9()->haveAccess(comp);
   }


// Points to address: what if vmThread is not the compilation thread?
// What if the compilation thread does not have the classUnloadMonitor?
// What if jitConfig does not exist?
// What if we already have the compilationShouldBeInterrupted flag set?
// How do we set the error code when we throw an exception?

// IMPORTANT: acquireVMAccessIfNeeded could throw an exception,
// hence it is important to call this within a try block.
//
static bool
acquireVMaccessIfNeededInner(J9VMThread *vmThread, TR_YesNoMaybe isCompThread)
   {
   bool haveAcquiredVMAccess = false;

   if (TR::Options::getCmdLineOptions() == 0 || // if options haven't been created yet, there is no danger
       TR::Options::getCmdLineOptions()->getOption(TR_DisableNoVMAccess))
      {
      return false; // don't need to acquire VM access
      }

   // we need to test if the thread has VM access
   TR_ASSERT(vmThread, "vmThread must be not null");

   // We need to acquire VMaccess only for the compilation thread
   if (isCompThread == TR_no)
      {
      TR_ASSERT((vmThread->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS), "Must have vm access if this is not a compilation thread");
      return false;
      }

   // scan all compilation threads
   J9JITConfig *jitConfig = vmThread->javaVM->jitConfig;
   TR::CompilationInfo *compInfo = TR::CompilationInfo::get(jitConfig);
   TR::CompilationInfoPerThread *compInfoPT = compInfo->getCompInfoForThread(vmThread);

   // We need to acquire VMaccess only for the compilation thread
   if (isCompThread == TR_maybe)
      {
      if (!compInfoPT)
         {
         TR_ASSERT((vmThread->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS), "Must have vm access if this is not a compilation thread");
         return false;
         }
      }

   // At this point we know we deal with a compilation thread
   //
   TR_ASSERT(compInfoPT, "A compilation thread must have an associated compilationInfo");

   if (!(vmThread->publicFlags &  J9_PUBLIC_FLAGS_VM_ACCESS)) // I don't already have VM access
      {
      if (0 == vmThread->javaVM->internalVMFunctions->internalTryAcquireVMAccessWithMask(vmThread, J9_PUBLIC_FLAGS_HALT_THREAD_ANY_NO_JAVA_SUSPEND))
         {
         haveAcquiredVMAccess = true;
         }
      else // the GC is having exclusive VM access
         {
         // compilationShouldBeInterrupted flag is reset by the compilation
         // thread when a new compilation starts.

         // must test if we have the class unload monitor
         //vmThread->osThread == classUnloadMonitor->owner()

         bool hadClassUnloadMonitor = false;
#if !defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
         if (TR::Options::getCmdLineOptions()->getOption(TR_EnableHCR) || TR::Options::getCmdLineOptions()->getOption(TR_FullSpeedDebug))
#endif
            hadClassUnloadMonitor = TR::MonitorTable::get()->readReleaseClassUnloadMonitor(compInfoPT->getCompThreadId()) >= 0;

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
         // We must have had classUnloadMonitor by the way we architected the application
         TR_ASSERT(hadClassUnloadMonitor, "Comp thread must hold classUnloadMonitor when compiling without VMaccess");
#endif

         //--- GC CAN INTERVENE HERE ---

         // fprintf(stderr, "Have released class unload monitor temporarily\n"); fflush(stderr);

         // At this point we must not hold any JIT monitors that can also be accessed by the GC
         // As we don't know how the GC code will be modified in the future we will
         // scan the entire list of known monitors
#if defined(DEBUG) || defined(PROD_WITH_ASSUMES)
         TR::Monitor * heldMonitor = TR::MonitorTable::get()->monitorHeldByCurrentThread();
         TR_ASSERT(!heldMonitor, "Current thread must not hold TR::Monitor %p called %s when trying to acquire VM access\n",
                    heldMonitor, TR_J9VMBase::get(jitConfig, NULL)->getJ9MonitorName((J9ThreadMonitor*)heldMonitor->getVMMonitor()));
#endif // #if defined(DEBUG) || defined(PROD_WITH_ASSUMES)

         if (TR::Options::getCmdLineOptions()->realTimeGC())
            compInfoPT->waitForGCCycleMonitor(false); // used only for real-time

         acquireVMAccessNoSuspend(vmThread);   // blocking. Will wait for the entire GC

         if (hadClassUnloadMonitor)
            {
            TR::MonitorTable::get()->readAcquireClassUnloadMonitor(compInfoPT->getCompThreadId());
            //fprintf(stderr, "Have acquired class unload monitor again\n"); fflush(stderr);
            }

         // Now we can check if the GC has done some unloading or redefinition happened
         if (compInfoPT->compilationShouldBeInterrupted())
            {
            // bail out
            // fprintf(stderr, "Released classUnloadMonitor and will throw an exception because GC unloaded classes\n"); fflush(stderr);
            //TR::MonitorTable::get()->readReleaseClassUnloadMonitor(0); // Main code should do it.
            // releaseVMAccess(vmThread);

            TR::Compilation *comp = compInfoPT->getCompilation();
            if (comp)
               {
               comp->failCompilation<TR::CompilationInterrupted>("Compilation interrupted by GC unloading classes");
               }
            else // I am not sure we are in a compilation; better release the monitor
               {
               if (hadClassUnloadMonitor)
                  TR::MonitorTable::get()->readReleaseClassUnloadMonitor(compInfoPT->getCompThreadId()); // Main code should do it.
               throw TR::CompilationInterrupted();
               }
            }
         else // GC did not do any unloading
            {
            haveAcquiredVMAccess = true;
            }
         }
      }

   /*
    * At shutdown time the compilation thread executes Java code and it may receive a sample (see D174900)
    * Only abort the compilation if we're explicitly prepared to handle it.
    */
   if (compInfoPT->compilationCanBeInterrupted() && compInfoPT->compilationShouldBeInterrupted())
      {
      TR_ASSERT(compInfoPT->compilationShouldBeInterrupted() != GC_COMP_INTERRUPT, "GC should not have cut in _compInfoPT=%p\n", compInfoPT);

      // in prod builds take some corrective action
      throw J9::JITShutdown();
      }

   return haveAcquiredVMAccess;
   }


bool
J9::VMEnv::acquireVMAccessIfNeeded(OMR_VMThread *omrVMThread)
   {
   TR_ASSERT(0, "not implemented"); return false;
   }


bool
J9::VMEnv::acquireVMAccessIfNeeded(TR_J9VMBase *fej9)
   {
   // This interface should be deprecated when the FrontEnd is redesigned and
   // eliminated.  The trouble with the CompilerEnv interface is that it doesn't
   // yet understand compilation contexts (i.e., a 'regular' compile, an AOT
   // compile, a FrontEnd cooked up for a debugger extension, etc.).  While
   // this can be sorted out, the safest thing to do until that happens is to
   // simply call the equivalent J9 FrontEnd virtual function to ensure the
   // correct call is being made.
   //
   //   return acquireVMaccessIfNeededInner(fej9->vmThread(), fej9->vmThreadIsCompilationThread());

   return fej9->acquireVMAccessIfNeeded();
   }


bool
J9::VMEnv::acquireVMAccessIfNeeded(TR::Compilation *comp)
   {
   return TR::Compiler->vm.acquireVMAccessIfNeeded(comp->fej9());
   }


bool
J9::VMEnv::tryToAcquireAccess(TR::Compilation *comp, bool *haveAcquiredVMAccess)
   {
   return comp->fej9()->tryToAcquireAccess(comp, haveAcquiredVMAccess);
   }


bool
J9::VMEnv::tryToAcquireAccess(OMR_VMThread *omrVMThread, bool *haveAcquiredVMAccess)
   {
   TR_ASSERT(0, "not implemented"); return false;
   }


static void
releaseVMaccessIfNeededInner(J9VMThread *vmThread, bool haveAcquiredVMAccess)
   {
   if (haveAcquiredVMAccess)
      {
      TR_ASSERT((vmThread->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS), "Must have VM access");
      releaseVMAccess(vmThread);
      }
   }


void
J9::VMEnv::releaseVMAccessIfNeeded(TR::Compilation *comp, bool haveAcquiredVMAccess)
   {
   comp->fej9()->releaseVMAccessIfNeeded(haveAcquiredVMAccess);
   }


void
J9::VMEnv::releaseVMAccessIfNeeded(OMR_VMThread *omrVMThread, bool haveAcquiredVMAccess)
   {
   releaseVMaccessIfNeededInner(self()->J9VMThreadFromOMRVMThread(omrVMThread), haveAcquiredVMAccess);
   }


void
J9::VMEnv::releaseVMAccessIfNeeded(TR_J9VMBase *fej9, bool haveAcquiredVMAccess)
   {
   fej9->releaseVMAccessIfNeeded(haveAcquiredVMAccess);
   }


void
J9::VMEnv::releaseAccess(TR::Compilation *comp)
   {
   comp->fej9()->releaseAccess(comp);
   }


void
J9::VMEnv::releaseAccess(OMR_VMThread *omrVMThread)
   {
   TR_ASSERT(0, "not implemented");
   }


void
J9::VMEnv::releaseAccess(TR_J9VMBase *fej9)
   {
   if (fej9->vmThread()->publicFlags & J9_PUBLIC_FLAGS_VM_ACCESS)
      {
      releaseVMAccess(fej9->vmThread());
      }
   }

uintptrj_t
J9::VMEnv::thisThreadGetPendingExceptionOffset()
   {
   return offsetof(J9VMThread, jitException);
   }

bool
J9::VMEnv::hasResumableTrapHandler(TR::Compilation *comp)
   {
   return comp->fej9()->hasResumableTrapHandler();
   }

bool
J9::VMEnv::hasResumableTrapHandler(OMR_VMThread *omrVMThread)
   {
   TR_ASSERT(0, "not implemented"); return false;
   }

uint64_t
J9::VMEnv::getUSecClock(TR::Compilation *comp)
   {
   return comp->fej9()->getUSecClock();
   }

uint64_t
J9::VMEnv::getUSecClock(OMR_VMThread *omrVMThread)
   {
   TR_ASSERT(0, "not implemented"); return 0;
   }

uint64_t
J9::VMEnv::getHighResClock(TR::Compilation *comp)
   {
   return comp->fej9()->getHighResClock();
   }

uint64_t
J9::VMEnv::getHighResClock(OMR_VMThread *omrVMThread)
   {
   TR_ASSERT(0, "not implemented"); return 0;
   }

uint64_t
J9::VMEnv::getHighResClockResolution(TR::Compilation *comp)
   {
   return comp->fej9()->getHighResClockResolution();
   }

uint64_t
J9::VMEnv::getHighResClockResolution(OMR_VMThread *omrVMThread)
   {
   TR_ASSERT(0, "not implemented"); return 0;
   }

uint64_t
J9::VMEnv::getHighResClockResolution(TR_FrontEnd *fej9)
   {
   return ((TR_J9VMBase*)fej9)->getHighResClockResolution();
   }

uint64_t
J9::VMEnv::getHighResClockResolution()
   {
   PORT_ACCESS_FROM_PORT(TR::Compiler->portLib);
   return j9time_hires_frequency();
   }

bool
J9::VMEnv::canMethodEnterEventBeHooked(TR::Compilation *comp)
   {
   return comp->fej9()->canMethodEnterEventBeHooked();
   }

bool
J9::VMEnv::canMethodExitEventBeHooked(TR::Compilation *comp)
   {
   return comp->fej9()->canMethodExitEventBeHooked();
   }

uintptrj_t
J9::VMEnv::getOverflowSafeAllocSize(TR::Compilation *comp)
   {
   return comp->fej9()->getOverflowSafeAllocSize();
   }


int64_t
J9::VMEnv::cpuTimeSpentInCompilationThread(TR::Compilation *comp)
   {
   return comp->fej9()->getCpuTimeSpentInCompThread(comp);
   }


uintptrj_t
J9::VMEnv::OSRFrameHeaderSizeInBytes(TR::Compilation *comp)
   {
   return comp->fej9()->getOSRFrameHeaderSizeInBytes();
   }


uintptrj_t
J9::VMEnv::OSRFrameSizeInBytes(TR::Compilation *comp, TR_OpaqueMethodBlock* method)
   {
   return comp->fej9()->getOSRFrameSizeInBytes(method);
   }


bool
J9::VMEnv::ensureOSRBufferSize(TR::Compilation *comp, uintptrj_t osrFrameSizeInBytes, uintptrj_t osrScratchBufferSizeInBytes, uintptrj_t osrStackFrameSizeInBytes)
   {
   return comp->fej9()->ensureOSRBufferSize(osrFrameSizeInBytes, osrScratchBufferSizeInBytes, osrStackFrameSizeInBytes);
   }

uintptrj_t
J9::VMEnv::thisThreadGetOSRReturnAddressOffset(TR::Compilation *comp)
   {
   return comp->fej9()->thisThreadGetOSRReturnAddressOffset();
   }

uintptrj_t
J9::VMEnv::thisThreadGetGSIntermediateResultOffset(TR::Compilation *comp)
   {
   return comp->fej9()->thisThreadGetGSIntermediateResultOffset();
   }

uintptrj_t
J9::VMEnv::thisThreadGetConcurrentScavengeActiveByteAddressOffset(TR::Compilation *comp)
   {
   return comp->fej9()->thisThreadGetConcurrentScavengeActiveByteAddressOffset();
   }

uintptrj_t
J9::VMEnv::thisThreadGetEvacuateBaseAddressOffset(TR::Compilation *comp)
   {
   return comp->fej9()->thisThreadGetEvacuateBaseAddressOffset();
   }

uintptrj_t
J9::VMEnv::thisThreadGetEvacuateTopAddressOffset(TR::Compilation *comp)
   {
   return comp->fej9()->thisThreadGetEvacuateTopAddressOffset();
   }

uintptrj_t
J9::VMEnv::thisThreadGetGSOperandAddressOffset(TR::Compilation *comp)
   {
   return comp->fej9()->thisThreadGetGSOperandAddressOffset();
   }

uintptrj_t
J9::VMEnv::thisThreadGetGSHandlerAddressOffset(TR::Compilation *comp)
   {
   return comp->fej9()->thisThreadGetGSHandlerAddressOffset();
   }

/*
 * Method enter/exit hooks are only enabled for some specific methods
 */
bool
J9::VMEnv::isSelectiveMethodEnterExitEnabled(TR::Compilation *comp)
   {
   return comp->fej9()->isSelectiveMethodEnterExitEnabled();
   }
