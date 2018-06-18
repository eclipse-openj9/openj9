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

#include <stdint.h>
#include "jilconsts.h"
#include "compile/Compilation.hpp"
#include "env/jittypes.h"
#include "infra/Monitor.hpp"
#include "runtime/RuntimeAssumptions.hpp"
#include "control/CompilationRuntime.hpp"
#include "control/CompilationThread.hpp"
#include "env/J9JitMemory.hpp"
#include "env/VMJ9.h"

void preventAllocationOfBTLMemory(J9MemorySegment * &segment, J9JavaVM * javaVM, TR_AllocationKind segmentType, bool freeSegmentOverride)
   {
#if  defined(J9ZOS390)
   // Special code for zOS. If we allocated BTL memory (first 16MB), then we must
   // release this segment, failing the compilation and forcing to use only one compilation thread
   if (TR::Options::getCmdLineOptions()->getOption(TR_DontAllocateScratchBTL) &&
      segment && ((uintptrj_t)(segment->heapBase) < (uintptrj_t)(1 << 24)))
      {
      // If applicable, reduce the number of compilation threads to 1
      TR::CompilationInfo * compInfo = TR::CompilationInfo::get((J9JITConfig*)jitConfig);
      if (compInfo)
         {
         if (!compInfo->getRampDownMCT())
            {
            compInfo->setRampDownMCT();
            if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerboseCompilationThreads))
               {
               TR_VerboseLog::writeLineLocked(TR_Vlog_INFO, "t=%u setRampDownMCT because JIT allocated BTL memory", (uint32_t)compInfo->getPersistentInfo()->getElapsedTime());
               }
            }
         else
            {
            // Perhaps we should consider lowering the compilation aggressiveness
            if (!TR::Options::getAOTCmdLineOptions()->getOption(TR_NoOptServer))
               {
               TR::Options::getAOTCmdLineOptions()->setOption(TR_NoOptServer);
               }
            if (!TR::Options::getJITCmdLineOptions()->getOption(TR_NoOptServer))
               {
               TR::Options::getJITCmdLineOptions()->setOption(TR_NoOptServer);
               }
            }

         // For scratch memory refuse to return memory below the line. Free the segment and let the compilation fail
         // Compilation will be retried at lower opt level.
         if (freeSegmentOverride || segmentType == heapAlloc || segmentType == stackAlloc)
            {
            // We should not reject requests coming from hooks. Test if this is a comp thread
            if (compInfo->useSeparateCompilationThread())
               {
               J9VMThread *crtVMThread = javaVM->internalVMFunctions->currentVMThread(javaVM);
               if (compInfo->getCompInfoForThread(crtVMThread))
                  {
                  javaVM->internalVMFunctions->freeMemorySegment(javaVM, segment, TRUE);
                  segment = NULL;
                  }
               }
            }
         }
      }
#endif // defined(J9ZOS390)
   }
