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

#include "env/PersistentAllocator.hpp"
#include "il/DataTypes.hpp"
#include "infra/Monitor.hpp"

extern TR::Monitor *memoryAllocMonitor;

namespace J9 {

PersistentAllocator::PersistentAllocator(const PersistentAllocatorKit &creationKit) :
   _minimumSegmentSize(creationKit.minimumSegmentSize),
   _segmentAllocator(MEMORY_TYPE_JIT_PERSISTENT, creationKit.javaVM),
   _freeBlocks(),
   _segments(SegmentContainerAllocator(RawAllocator(&creationKit.javaVM)))
   {
   }

PersistentAllocator::~PersistentAllocator() throw()
   {
   while (!_segments.empty())
      {
      J9MemorySegment &segment = _segments.front();
      _segments.pop_front();
      _segmentAllocator.deallocate(segment);
      }
   }

void *
PersistentAllocator::allocate(size_t size, const std::nothrow_t tag, void * hint) throw()
   {
   if (::memoryAllocMonitor)
      ::memoryAllocMonitor->enter();

   void * result = allocateLocked(size);

   if (::memoryAllocMonitor)
      ::memoryAllocMonitor->exit();

   return result;
   }

void *
PersistentAllocator::allocate(size_t size, void *hint)
   {
   void *alloc = allocate(size, std::nothrow, hint);
   if (!alloc) throw std::bad_alloc();
   return alloc;
   }

void *
PersistentAllocator::allocateLocked(size_t requestedSize)
   {
   TR_ASSERT( sizeof(Block) == mem_round( sizeof(Block) ),"Persistent block size will prevent us from properly aligning allocations.");
   size_t const dataSize = mem_round(requestedSize);
   size_t const allocSize = sizeof(Block) + dataSize;

   TR::AllocatedMemoryMeter::update_allocated(allocSize, persistentAlloc);

   // If this is a small block try to allocate it from the appropriate
   // fixed-size-block chain.
   //
   size_t const index = freeBlocksIndex(allocSize);
   Block * block = 0;
   Block * prev = 0;
   for (
      block = _freeBlocks[index];
      block && block->_size < allocSize;
      prev = block, block = prev->next()
      )
      { TR_ASSERT(index == 0, "Iterating through a fixed-size block bin."); }

   if (block)
      {

      TR_ASSERT(
         ( index == 0 ) || ( block->_size == allocSize ),
         "block %p in chain for index %d has size %d (not %d)\n",
         block,
         index,
         block->_size,
         (index * sizeof(void *)) + sizeof(Block)
         );

      if (prev)
         prev->_next = block->next();
      else
         _freeBlocks[index] = block->next();

      block->_next = NULL;

      size_t const excess = block->_size - allocSize;

      if (excess > sizeof(Block))
         {
         block->_size = allocSize;
         freeBlock( new (pointer_cast<uint8_t *>(block) + allocSize) Block(excess) );
         }

      return block + 1;
      }

   // Find the first persistent segment with enough free space
   //
   J9MemorySegment *segment = findUsableSegment(allocSize);
   if (!segment)
      {
      size_t const segmentSize = allocSize < _minimumSegmentSize ? _minimumSegmentSize : allocSize;
      segment = _segmentAllocator.allocate(segmentSize, std::nothrow);
      if (!segment) return 0;
      try
         {
         _segments.push_front(TR::ref(*segment));
         }
      catch(const std::exception &e)
         {
         _segmentAllocator.deallocate(*segment);
         return 0;
         }
      }
   TR_ASSERT(segment && remainingSpace(*segment) >= allocSize, "Failed to acquire a segment");
   block = new(operator new(allocSize, *segment)) Block(allocSize);
   return block + 1;
   }

J9MemorySegment *
PersistentAllocator::findUsableSegment(size_t requiredSize)
   {
   for (auto i = _segments.begin(); i != _segments.end(); ++i)
      {
      J9MemorySegment &candidate = *i;
      if ( remainingSpace(candidate) >= requiredSize )
         {
         return &candidate;
         }
      }
   return 0;
   }

size_t
PersistentAllocator::remainingSpace(J9MemorySegment &segment) throw()
   {
   return (segment.heapTop - segment.heapAlloc);
   }

void
PersistentAllocator::freeBlock(Block * block)
   {
   TR_ASSERT(block->_size > 0, "Block size is non-positive");
   TR_ASSERT(block->_next == NULL, "In-use persistent memory block @ belongs to a free block chain.", block);
   block->_next = NULL;

   // If this is a small block, add it to the appropriate fixed-size-block
   // chain. Otherwise add it to the variable-size-block chain which is in
   // ascending size order.
   //
   size_t const index = freeBlocksIndex(block->_size);
   Block * blockIterator = _freeBlocks[index];
   if (!blockIterator || !(blockIterator->_size < block->_size) )
      {
      block->_next = _freeBlocks[index];
      _freeBlocks[index] = block;
      }
   else
      {
      TR_ASSERT(index == 0, "Iterating through what should be fixed-size blocks.");
      while (blockIterator->next() && blockIterator->next()->_size < block->_size)
         {
         blockIterator = blockIterator->next();
         }
      block->_next = blockIterator->next();
      blockIterator->_next = block;
      }
   }

void
PersistentAllocator::deallocate(void * mem, size_t) throw()
   {
   if (::memoryAllocMonitor)
      ::memoryAllocMonitor->enter();


   Block * block = static_cast<Block *>(mem) - 1;

   // adjust the used persistent memory here and not in freePersistentmemory(block, size)
   // because that call is also used to free memory that wasn't actually committed
   TR::AllocatedMemoryMeter::update_freed(block->_size, persistentAlloc);

   freeBlock(block);

   if (::memoryAllocMonitor)
      ::memoryAllocMonitor->exit();
   }

}

void *
operator new(size_t size, J9::PersistentAllocator &persistentAllocator)
   {
   return persistentAllocator.allocate(size);
   }

void *operator new[](size_t size, J9::PersistentAllocator &persistentAllocator)
   {
   return operator new(size, persistentAllocator);
   }

void operator delete(void *garbage, J9::PersistentAllocator &persistentAllocator) throw()
   {
   persistentAllocator.deallocate(garbage);
   }

void operator delete[](void *ptr, J9::PersistentAllocator &persistentAllocator) throw()
   {
   operator delete(ptr, persistentAllocator);
   }
