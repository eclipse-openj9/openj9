/*******************************************************************************
 * Copyright (c) 2000, 2019 IBM Corp. and others
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

#ifndef PPCHWPROFILER_INCL
#define PPCHWPROFILER_INCL

#include "runtime/HWProfiler.hpp"

#include <stdint.h>
#include "env/jittypes.h"

class TR_J9VMBase;
struct TR_PPCHWProfilerEBBContext;
class TR_PPCHWProfiler : public TR_HWProfiler
   {
public:
   TR_PERSISTENT_ALLOC(TR_Memory::PPCHWProfiler);

   /**
    * Constructor.
    * @param jitConfig the J9JITConfig
    */
   TR_PPCHWProfiler(J9JITConfig *jitConfig);


   // --------------------------------------------------------------------------------------
   // HW Profiler Management Methods

   /**
    * Static method used to allocate the HW Profiler
    * @param jitConfig The J9JITConfig
    * @return pointer to the HWPRofiler
    */
   static TR_PPCHWProfiler* allocate(J9JITConfig *jitConfig);

   /**
    * Initialize hardware profiling on given app thread.
    * @param vmThread The VM thread to initialize profiling.
    * @return true if initialization is successful; false otherwise.
    */
   virtual bool initializeThread(J9VMThread *vmThread);

   /**
    * Deinitialize hardware profiling on given app thread
    * @param vmThread The VM thread to deinitialize profiling.
    * @return true if deinitialization is successful; false otherwise.
    */
   virtual bool deinitializeThread(J9VMThread *vmThread);


   // --------------------------------------------------------------------------------------
   // HW Profiler Buffer Processing Methods

   /**
    * Method to process buffers from a given app thread.
    * @param vmThread The VM thread to query
    * @param fe The Front End
    * @return false if the thread cannot be used for HW Profiling; true otherwise.
    */
   virtual bool processBuffers(J9VMThread *vmThread, TR_J9VMBase *fe);

   /**
    * Method to process the data in the buffers.
    * @param vmThread The VM thread
    * @param dataStart The start of the data buffer.
    * @param size      Size of the data buffer.
    * @param bufferFilledSize The amount of the buffer that is filled
    * @param dataTag   An optional platform-dependent tag for the data in the buffer.
    */
   virtual void processBufferRecords(J9VMThread *vmThread,
                                     uint8_t *bufferStart,
                                     uintptrj_t size,
                                     uintptrj_t bufferFilledSize,
                                     uint32_t dataTag = 0);

   /**
    * Method to allocate a buffer for HW Profiling.
    * There is a maximum amount of memory the HW Profiler is allowed to allocate. It first tries to
    * pull a buffer from TR_HWPRofiler::_freeBufferList. If there are no free buffers, it uses
    * TR_Memory::jitPersistentAlloc to allocate a buffer.
    * @param size The size of the buffer to be allocated
    * @return a pointer to the buffer
    */
   virtual void* allocateBuffer(uint64_t size);

   /**
    * Method to free a buffer allocated for HW Profiling (places it into the free list).
    * @param buffer The buffer to be freed
    * @param Parameter for the size of the buffer to be freed
    */
   virtual void freeBuffer(void * buffer, uint64_t size = 0);


   // --------------------------------------------------------------------------------------
   // HW Profiler Miscellaneous Helper Methods

   /**
    * Prints out HW Profiler stats. This method prints out Z HW Profiler stats first and then
    * calls TR_HWProfiler::printStats()
    */
   virtual void printStats();

protected:

   // Buffer Memory Allocated
   uint64_t                 _ppcHWProfilerBufferMemoryAllocated;
   uint64_t                 _ppcHWProfilerBufferMaximumMemory;
   };

#endif /* PPCHWPROFILER_INCL */

