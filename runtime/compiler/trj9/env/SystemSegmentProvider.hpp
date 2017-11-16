/*******************************************************************************
 * Copyright (c) 2000, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#ifndef J9_SYSTEMSEGMENTPROVIDER_H
#define J9_SYSTEMSEGMENTPROVIDER_H

#include <set>
#include <deque>
#include "env/SegmentAllocator.hpp"
#include "env/MemorySegment.hpp"

#include "env/J9SegmentAllocator.hpp"
#include "env/TypedAllocator.hpp"
#include "infra/ReferenceWrapper.hpp"
#include "env/RawAllocator.hpp"

namespace J9 {

class SystemSegmentProvider : public TR::SegmentAllocator
   {
public:
   SystemSegmentProvider(size_t defaultSegmentSize, size_t systemSegmentSize, size_t allocationLimit, J9::J9SegmentProvider &segmentAllocator, TR::RawAllocator rawAllocator);
   ~SystemSegmentProvider() throw();
   virtual TR::MemorySegment &request(size_t requiredSize);
   virtual void release(TR::MemorySegment &segment) throw();
   size_t systemBytesAllocated() const throw();
   size_t regionBytesAllocated() const throw();
   size_t bytesAllocated() const throw();
   size_t allocationLimit() const throw();
   void setAllocationLimit(size_t allocationLimit);

private:
   size_t round(size_t requestedSize);
   ptrdiff_t remaining(const J9MemorySegment &memorySegment);
   TR::MemorySegment &allocateNewSegment(size_t size);
   TR::MemorySegment &createSegmentFromArea(size_t size, void * segmentArea);

   size_t const _systemSegmentSize;
   size_t _allocationLimit;
   size_t _systemBytesAllocated;
   size_t _regionBytesAllocated;
   J9::J9SegmentProvider & _systemSegmentAllocator;

   typedef TR::typed_allocator<
      TR::reference_wrapper<J9MemorySegment>,
      TR::RawAllocator
      > SystemSegmentDequeAllocator;

   std::deque<
      TR::reference_wrapper<J9MemorySegment>,
      SystemSegmentDequeAllocator
      > _systemSegments;

   typedef TR::typed_allocator<
      TR::MemorySegment,
      TR::RawAllocator
      > SegmentSetAllocator;

   std::set<
      TR::MemorySegment,
      std::less< TR::MemorySegment >,
      SegmentSetAllocator
      > _segments;

   typedef TR::typed_allocator<
      TR::reference_wrapper<TR::MemorySegment>,
      TR::RawAllocator
      > FreeSegmentDequeAllocator;

   std::deque<
      TR::reference_wrapper<TR::MemorySegment>,
      FreeSegmentDequeAllocator
      > _freeSegments;

   TR::reference_wrapper<J9MemorySegment> _currentSystemSegment;

   };

} // namespace J9

#endif // J9_SYSTEMSEGMENTPROVIDER_H
