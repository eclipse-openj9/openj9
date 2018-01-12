/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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

#ifndef J9_SEGMENT_ALLOCATOR_HPP
#define J9_SEGMENT_ALLOCATOR_HPP

#include <stddef.h>
#include <stdint.h>
#include <new>
#include "env/J9SegmentProvider.hpp"

extern "C" {
struct J9JavaVM;
struct J9MemorySegment;
}

namespace J9 {

class SegmentAllocator : public J9SegmentProvider
   {
public:
   SegmentAllocator(int32_t segmentType, J9JavaVM &javaVM) throw();
   ~SegmentAllocator() throw();
   J9MemorySegment &allocate(const size_t segmentSize);
   J9MemorySegment *allocate(const size_t segmentSize, const std::nothrow_t &tag) throw();
   void deallocate(J9MemorySegment &unusedSegment) throw();

   J9MemorySegment &request(size_t segmentSize);
   void release(J9MemorySegment &unusedSegment) throw();

   size_t pageSize() throw();
   size_t pageAlign(const size_t requestedSize) throw();

private:
   void preventAllocationOfBTLMemory(J9MemorySegment * &segment, J9JavaVM * javaVM, int32_t segmentType);

   const int32_t _segmentType;
   J9JavaVM &_javaVM;
   };

}

#endif // SEGMENT_ALLOCATOR_HPP
