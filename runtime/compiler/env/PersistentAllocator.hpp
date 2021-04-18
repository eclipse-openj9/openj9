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

#ifndef J9_PERSISTENT_ALLOCATOR_HPP
#define J9_PERSISTENT_ALLOCATOR_HPP

#pragma once

namespace J9 { class PersistentAllocator; }
namespace TR { using J9::PersistentAllocator; }

#include <new>
#include "j9cfg.h"
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

#if defined(J9VM_OPT_JITSERVER)
static constexpr size_t floorLog2(size_t n) { return n <= 1 ? 0 : 1 + floorLog2(n >> 1); }
#endif

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

   // Enable automatic conversion into a form compatible with C++ standard library containers
   template<typename T> operator TR::typed_allocator<T, PersistentAllocator& >()
      {
      return TR::typed_allocator<T, PersistentAllocator& >(*this);
      }

private:

   // Persistent block header
   //
   struct Block
      {
      size_t _size;
      Block * _next;

      explicit Block(size_t size, Block * next = 0) : _size(size), _next(next) {}
      size_t size() const { return _size; }
      void setSize(size_t sz) { _size = sz; }
      Block * next() const { return _next; }
      void setNext(Block *b) { _next = b; }
      };

   // Monitor to protect access to the linked lists of (small) fixed-size blocks
   // The variable-size block list (which takes longer to access) will continue
   // to be protected by memoryAllocMonitor. This arrangement prevents a fast
   // fixed-size block list access to be delayed by a slow variable-size block
   // list access
   J9ThreadMonitor * _smallBlockListsMonitor;

   static const size_t PERSISTANT_BLOCK_SIZE_BUCKETS = 16;
   // first list/bucket is for large blocks of variable size
   static const size_t LARGE_BLOCK_LIST_INDEX = 0;
   static size_t freeBlocksIndex(size_t const blockSize)
      {
      size_t const adjustedBlockSize = blockSize - sizeof(Block);
      size_t const candidateBucket = adjustedBlockSize / sizeof(void *);
      return candidateBucket < PERSISTANT_BLOCK_SIZE_BUCKETS ?
         candidateBucket :
         LARGE_BLOCK_LIST_INDEX;
      }

   void * allocateInternal(size_t);
   Block * allocateFromVariableSizeListLocked(size_t allocSize);
   void * allocateFromSegmentLocked(size_t allocSize);
   void freeFixedSizeBlock(Block * block);
   void freeVariableSizeBlock(Block * block);
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

#if defined(J9VM_OPT_JITSERVER)
   // For JITServer, large freed blocks are put in a doubly linked list ordered by size.
   // A node in this list (of type ExtendedBlock) could be a single freed block, 
   // or a list of freed blocks of same size.
   struct ExtendedBlock
      {
      Block _block; // Must be first entity in the ExtendedBlock
      ExtendedBlock * _previous; // link to previous block in the freed list (smaller in size)
      ExtendedBlock * _nextBlockSameSize; // link to a block of same size as this one

      size_t size() const { return _block.size(); }
      void setSize(size_t sz) { _block.setSize(sz); }
      ExtendedBlock * next() const { return reinterpret_cast<ExtendedBlock *>(_block.next()); }
      void setNext(ExtendedBlock *b) { _block.setNext(reinterpret_cast<Block *>(b)); }
      ExtendedBlock * previous() const { return _previous; }
      void setPrevious(ExtendedBlock *b) { _previous = b; }
      ExtendedBlock * nextBlockSameSize() const { return _nextBlockSameSize; }
      void setNextBlockSameSize(ExtendedBlock *b) { _nextBlockSameSize = b; }
      void init()
         {
         // Don't change the size
         setNext(NULL);
         setPrevious(NULL);
         setNextBlockSameSize(NULL);
         }
      };
   /**
    * @brief Compute the power-of-two interval to which this block size belongs
    */
   size_t getInterval(size_t blockSize);
   Block * allocateFromIndexedListLocked(size_t allocSize);
   void freeBlockToIndexedList(Block *);
   void checkIntegrity(const char msg[]); // for debugging purposes
   
   const bool _isJITServer;
   // Since the list of variable-size blocks (the large ones) can become very big,
   // it may generate a lot of overhead during allocation and deallocation because
   // we need to find the right place in this list which is ordered by block size.
   // To speed-up searching, we logically split the list into several "intervals"
   // by keeping pointers at nodes whose block sizes are power of two. 
   // For instance, we will have pointers for nodes that are 128,
   // 256, 512, ..., 8K, 16K in size. If we search for a block that is 768
   // bytes in size, we don't have to look from the beginning of the list
   // but rather start from the pointer pointing to the 512 bytes block.
   // To simplify the implementation we will keep two pointers for each interval
   // in the list, indicating the smallest and respectively the largest block in
   // that interval. If there is just one block in an interval, these two pointers
   // will point to the same block. If there is no block in an interval, these
   // two pointers are NULL.
   static const size_t NUM_INTERVALS = 8;
   static const size_t SIZE_FIRST_LARGE_BLOCK = PERSISTANT_BLOCK_SIZE_BUCKETS * sizeof(void *);
   static const size_t BITS_TO_SHIFT_FIRST_INTERVAL = floorLog2(SIZE_FIRST_LARGE_BLOCK);
   ExtendedBlock * _startInterval[NUM_INTERVALS] = {}; // Acts like an index to help us find faster the block of appropriate size
                                                       // First shortcut points to blocks that are [128, 256) bytes in size
                                                       // Second shortcut points to blocks [256, 512) bytes in size, etc
   ExtendedBlock *_endInterval[NUM_INTERVALS] = {};
#endif /* defined(J9VM_OPT_JITSERVER) */
   }; // class PersistentAllocator
} // namespace J9

void *operator new(size_t, J9::PersistentAllocator &);
void *operator new[](size_t, J9::PersistentAllocator &);
void operator delete(void *, J9::PersistentAllocator &) throw();
void operator delete[](void *, J9::PersistentAllocator &) throw();

#endif // J9_PERSISTENT_ALLOCATOR_HPP

