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

#include <new>
#include "env/J9SegmentAllocator.hpp"
#include "infra/Assert.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "control/CompilationRuntime.hpp"
#include "j9.h"
#undef min
#undef max

namespace J9 {

SegmentAllocator::SegmentAllocator(
   int32_t segmentType,
   J9JavaVM &javaVM
   ) throw() :
   _segmentType(segmentType),
   _javaVM(javaVM)
   {
   TR_ASSERT(((pageSize() & (pageSize()-1)) == 0), "Page size is not a power of 2, %llu", static_cast<unsigned long long>(pageSize()) );
   }

SegmentAllocator::~SegmentAllocator() throw()
   {
   }

J9MemorySegment *
SegmentAllocator::allocate(const size_t segmentSize, const std::nothrow_t &tag) throw()
   {
   size_t const alignedSize = pageAlign(segmentSize);

   // If low on physical memory we may want to return NULL when allocating scratch segments
   if (_segmentType & MEMORY_TYPE_JIT_SCRATCH_SPACE)
      {
      bool incomplete;
      TR::CompilationInfo *compInfo = TR::CompilationInfo::get(_javaVM.jitConfig);
      uint64_t freePhysicalMemory = compInfo->computeAndCacheFreePhysicalMemory(incomplete, 20);
      if (freePhysicalMemory != OMRPORT_MEMINFO_NOT_AVAILABLE && !incomplete)
         {
         if (freePhysicalMemory < (TR::Options::getSafeReservePhysicalMemoryValue() + segmentSize))
            {
            // Set a flag that will determine one compilation thread to suspend itself
            // Even if multiple threads enter his path we still want to suspend only
            // one thread, not several. This is why we use a flag and not a counter.
            //
            // We allow a small race condition: it is possible that between the test
            // for available physical memory and setting of the flag below, another
            // compilation thread has suspended itself and reset the flag. The code 
            // below is going to set the flag again, possibly resulting into two
            // compilation threads being suspended. This is still fine, because, if
            // needed, a new compilation thread will be activated when a compilation
            // request is queued
            compInfo->setSuspendThreadDueToLowPhysicalMemory(true);
            return NULL;
            }
         }
      }
   J9MemorySegment * newSegment =
      _javaVM.internalVMFunctions->allocateMemorySegment(
         &_javaVM,
         alignedSize,
         _segmentType,
         J9MEM_CATEGORY_JIT
      );
   TR_ASSERT(
      !newSegment || (newSegment->heapAlloc == newSegment->heapBase),
      "Segment @ %p { heapBase: %p, heapAlloc: %p, heapTop: %p } is stale",
      newSegment,
      newSegment->heapBase,
      newSegment->heapAlloc,
      newSegment->heapTop
      );
   preventAllocationOfBTLMemory(newSegment, &_javaVM, _segmentType);
   return newSegment;
   }

J9MemorySegment &
SegmentAllocator::allocate(size_t const segmentSize)
   {
   J9MemorySegment * newSegment = allocate(segmentSize, std::nothrow);
   if (!newSegment) throw std::bad_alloc();
   return *newSegment;
   }

void
SegmentAllocator::deallocate(J9MemorySegment &unusedSegment) throw()
   {
   _javaVM.internalVMFunctions->freeMemorySegment(&_javaVM, &unusedSegment, TRUE);
   }

J9MemorySegment &
SegmentAllocator::request(size_t segmentSize)
   {
   return allocate(segmentSize);
   }

void
SegmentAllocator::release(J9MemorySegment &unusedSegment) throw()
   {
   deallocate(unusedSegment);
   }

size_t
SegmentAllocator::pageSize() throw()
   {
   PORT_ACCESS_FROM_JAVAVM(&_javaVM);
   static const size_t pageSize = j9vmem_supported_page_sizes()[0];
   return pageSize;
   }

size_t
SegmentAllocator::pageAlign(const size_t requestedSize) throw()
   {
   size_t const pageSize = this->pageSize();
   size_t alignedSize = (requestedSize + pageSize - 1) & ~(pageSize - 1);
   return alignedSize;
   }


void
SegmentAllocator::preventAllocationOfBTLMemory(J9MemorySegment * &segment, J9JavaVM * javaVM, int32_t segmentType)
   {
#if  defined(J9ZOS390)
   // Special code for zOS. If we allocated BTL memory (first 16MB), then we must
   // release this segment, failing the compilation and forcing to use only one compilation thread
   if (TR::Options::getCmdLineOptions()->getOption(TR_DontAllocateScratchBTL) &&
      segment && ((uintptrj_t)(segment->heapBase) < (uintptrj_t)(1 << 24)))
      {
      // If applicable, reduce the number of compilation threads to 1
      TR::CompilationInfo * compInfo = TR::CompilationInfo::get();
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
         if (segmentType & MEMORY_TYPE_VIRTUAL)
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

}
