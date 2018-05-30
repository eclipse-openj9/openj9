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

#ifndef J9_PERSISTENT_ALLOCATOR_HPP
#define J9_PERSISTENT_ALLOCATOR_HPP

#pragma once

namespace J9 { class PersistentAllocator; }
namespace TR { using J9::PersistentAllocator; }

#include <new>
#include "env/PersistentAllocatorKit.hpp"
#include "env/RawAllocator.hpp"
#include "env/TypedAllocator.hpp"
#include "env/J9SegmentAllocator.hpp"
#include "infra/ReferenceWrapper.hpp"
#include "env/MemorySegment.hpp"
#include <deque>

extern "C" {
struct J9MemorySegment;
}

namespace J9 {

class PersistentAllocator
   {
public:
   PersistentAllocator(const PersistentAllocatorKit &creationKit);
   ~PersistentAllocator() throw();

   void *allocate(size_t size, const std::nothrow_t tag, void * hint = 0) throw();
   void *allocate(size_t size, void * hint = 0);
   void deallocate(void * p, size_t sizeHint = 0) throw();

   friend bool operator ==(const PersistentAllocator &left, const PersistentAllocator &right)
      {
      return &left == &right;
      }
   friend bool operator !=(const PersistentAllocator &left, const PersistentAllocator &right)
      {
      return !operator ==(left, right);
      }

private:

   // Persistent block header
   //
   struct Block
      {
      size_t _size;
      Block * _next;

      explicit Block(size_t size, Block * next = 0) : _size(size), _next(next) {}
      Block * next() { return reinterpret_cast<Block *>( (reinterpret_cast<uintptr_t>(_next) & ~0x1)); }
      };

   static const size_t PERSISTANT_BLOCK_SIZE_BUCKETS = 12;
   static size_t freeBlocksIndex(size_t const blockSize)
      {
      size_t const adjustedBlockSize = blockSize - sizeof(Block);
      size_t const candidateBucket = adjustedBlockSize / sizeof(void *);
      return candidateBucket < PERSISTANT_BLOCK_SIZE_BUCKETS ?
         candidateBucket :
         0;
      }

   void * allocateLocked(size_t);
   void freeBlock(Block *);

   J9MemorySegment * findUsableSegment(size_t requiredSize);

   static void * allocate(J9MemorySegment &memorySegment, size_t size) throw();
   static size_t remainingSpace(J9MemorySegment &memorySegment) throw();

   size_t const _minimumSegmentSize;
   SegmentAllocator _segmentAllocator;
   Block * _freeBlocks[PERSISTANT_BLOCK_SIZE_BUCKETS];
   typedef TR::typed_allocator<TR::reference_wrapper<J9MemorySegment>, TR::RawAllocator> SegmentContainerAllocator;
   typedef std::deque<TR::reference_wrapper<J9MemorySegment>, SegmentContainerAllocator> SegmentContainer;
   SegmentContainer _segments;
   };

}

void *operator new(size_t, J9::PersistentAllocator &);
void *operator new[](size_t, J9::PersistentAllocator &);
void operator delete(void *, J9::PersistentAllocator &) throw();
void operator delete[](void *, J9::PersistentAllocator &) throw();

#endif // J9_PERSISTENT_ALLOCATOR_HPP

