/*******************************************************************************
 * Copyright (c) 2000, 2020 IBM Corp. and others
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

#ifndef J9_VMENV_INCL
#define J9_VMENV_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_VMENV_CONNECTOR
#define J9_VMENV_CONNECTOR
namespace J9 { class VMEnv; }
namespace J9 { typedef J9::VMEnv VMEnvConnector; }
#endif

#include "env/OMRVMEnv.hpp"
#include "env/jittypes.h"
#include "infra/Annotations.hpp"
#include "j9.h"

struct OMR_VMThread;
class TR_J9VMBase;
class TR_FrontEnd;
namespace TR { class Compilation; }

namespace J9
{

class OMR_EXTENSIBLE VMEnv : public OMR::VMEnvConnector
   {
public:

   int64_t maxHeapSizeInBytes();

   uintptr_t heapBaseAddress();

   uintptr_t thisThreadGetPendingExceptionOffset();

   bool hasResumableTrapHandler(TR::Compilation *comp);
   bool hasResumableTrapHandler(OMR_VMThread *omrVMThread);

   using OMR::VMEnvConnector::getUSecClock;
   uint64_t getUSecClock(TR::Compilation *comp);
   uint64_t getUSecClock(OMR_VMThread *omrVMThread);

   uint64_t getHighResClock(TR::Compilation *comp);
   uint64_t getHighResClock(OMR_VMThread *omrVMThread);

   uint64_t getHighResClockResolution(TR::Compilation *comp);
   uint64_t getHighResClockResolution(OMR_VMThread *omrVMThread);
   static uint64_t getHighResClockResolution(TR_FrontEnd *fej9);
   uint64_t getHighResClockResolution();

   bool hasAccess(OMR_VMThread *omrVMThread);
   bool hasAccess(J9VMThread *j9VMThread);
   bool hasAccess(TR::Compilation *comp);

   bool acquireVMAccessIfNeeded(OMR_VMThread *omrVMThread);
   bool acquireVMAccessIfNeeded(TR_J9VMBase *fej9);
   bool acquireVMAccessIfNeeded(TR::Compilation *comp);

   bool tryToAcquireAccess(TR::Compilation *, bool *);
   bool tryToAcquireAccess(OMR_VMThread *, bool *);

   void releaseVMAccessIfNeeded(TR::Compilation *comp, bool haveAcquiredVMAccess);
   void releaseVMAccessIfNeeded(OMR_VMThread *, bool haveAcquiredVMAccess);
   void releaseVMAccessIfNeeded(TR_J9VMBase *, bool haveAcquiredVMAccess);

   void releaseAccess(TR::Compilation *comp);
   void releaseAccess(OMR_VMThread *omrVMThread);
   void releaseAccess(TR_J9VMBase *fej9);

   J9VMThread *J9VMThreadFromOMRVMThread(OMR_VMThread *omrVMThread)
      {
      return (J9VMThread *)omrVMThread->_language_vmthread;
      }

   bool canMethodEnterEventBeHooked(TR::Compilation *comp);
   bool canMethodExitEventBeHooked(TR::Compilation *comp);
   bool isSelectiveMethodEnterExitEnabled(TR::Compilation *comp);

   uintptr_t getOverflowSafeAllocSize(TR::Compilation *comp);

   int64_t cpuTimeSpentInCompilationThread(TR::Compilation *comp);

   // On-stack replacement
   //
   uintptr_t OSRFrameHeaderSizeInBytes(TR::Compilation *comp);
   uintptr_t OSRFrameSizeInBytes(TR::Compilation *comp, TR_OpaqueMethodBlock* method);
   bool ensureOSRBufferSize(TR::Compilation *comp, uintptr_t osrFrameSizeInBytes, uintptr_t osrScratchBufferSizeInBytes, uintptr_t osrStackFrameSizeInBytes);
   uintptr_t thisThreadGetOSRReturnAddressOffset(TR::Compilation *comp);

   uintptr_t thisThreadGetGSIntermediateResultOffset(TR::Compilation *comp);
   uintptr_t thisThreadGetConcurrentScavengeActiveByteAddressOffset(TR::Compilation *comp);
   uintptr_t thisThreadGetEvacuateBaseAddressOffset(TR::Compilation *comp);
   uintptr_t thisThreadGetEvacuateTopAddressOffset(TR::Compilation *comp);
   uintptr_t thisThreadGetGSOperandAddressOffset(TR::Compilation *comp);
   uintptr_t thisThreadGetGSHandlerAddressOffset(TR::Compilation *comp);
   size_t getInterpreterVTableOffset();

   };

}

#endif
