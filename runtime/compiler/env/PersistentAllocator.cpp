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
   j9thread_monitor_init_with_name(&_smallBlockListsMonitor, 0, "JIT-SmallBlockListsMonitor");
   if (!_smallBlockListsMonitor)
      throw std::bad_alloc();
   }

PersistentAllocator::~PersistentAllocator() throw()
   {
   while (!_segments.empty())
      {
      J9MemorySegment &segment = _segments.front();
      _segments.pop_front();
      _segmentAllocator.deallocate(segment);
      }
   j9thread_monitor_destroy(_smallBlockListsMonitor);
   _smallBlockListsMonitor = NULL;
   }

void *
PersistentAllocator::allocate(size_t size, const std::nothrow_t tag, void * hint) throw()
   {
   return allocateInternal(size);
   }

void *
PersistentAllocator::allocate(size_t size, void *hint)
   {
   void *alloc = allocate(size, std::nothrow, hint);
   if (!alloc) throw std::bad_alloc();
   return alloc;
   }

PersistentAllocator::Block *
PersistentAllocator::allocateFromVariableSizeListLocked(size_t allocSize)
   {
   Block * block = NULL;
   Block * prev = NULL;
   for (block = _freeBlocks[LARGE_BLOCK_LIST_INDEX]; block && block->_size < allocSize; prev = block, block = prev->next())
      {}

   if (block)
      {
      if (prev)
         prev->_next = block->next();
      else
         _freeBlocks[LARGE_BLOCK_LIST_INDEX] = block->next();

      block->_next = NULL;
      }
   return block;
   }

void *
PersistentAllocator::allocateInternal(size_t requestedSize)
   {
   TR_ASSERT( sizeof(Block) == mem_round( sizeof(Block) ),"Persistent block size will prevent us from properly aligning allocations.");
   size_t const dataSize = mem_round(requestedSize);
   size_t const allocSize = sizeof(Block) + dataSize;
   void * allocation = NULL;

   if (TR::AllocatedMemoryMeter::_enabled & persistentAlloc)
      {
      // Use _smallBlockListsMonitor to protect TR::AllocatedMemoryMeter::update_allocated
      // because accessing the variable-size-block list protected by ::memoryAllocMonitor
      // takes longer and we may be penalizing access to fixed size block which should be very fast
      j9thread_monitor_enter(_smallBlockListsMonitor);
      TR::AllocatedMemoryMeter::update_allocated(allocSize, persistentAlloc);
      j9thread_monitor_exit(_smallBlockListsMonitor);
      }

   // If this is a small block try to allocate it from the appropriate
   // fixed-size-block chain.
   //
   size_t const index = freeBlocksIndex(allocSize);
   if (index != LARGE_BLOCK_LIST_INDEX) // fixed-size-block chain
      {
      j9thread_monitor_enter(_smallBlockListsMonitor);
      Block * block = _freeBlocks[index];
      if (block)
         {
         _freeBlocks[index] = block->next();
         block->_next = NULL;
         j9thread_monitor_exit(_smallBlockListsMonitor);
         allocation = block + 1; // Return pointer after the header
         }
      else // Couldn't find suitable free block; need to allocate from segment
         {
         j9thread_monitor_exit(_smallBlockListsMonitor);

         // Find the first persistent segment with enough free space
         if (::memoryAllocMonitor)
            ::memoryAllocMonitor->enter();
         allocation = allocateFromSegmentLocked(allocSize);
         if (::memoryAllocMonitor)
            ::memoryAllocMonitor->exit();
         }
      }
   else // Variable size block allocation
      {
      if (::memoryAllocMonitor)
         ::memoryAllocMonitor->enter();
      Block * block = allocateFromVariableSizeListLocked(allocSize);
      if (block)
         {
         // If the block I found is bigger than what I need,
         // split it and put the remaining block back onto the free list
         size_t const excess = block->_size - allocSize;
         if (excess > sizeof(Block))
            {
            block->_size = allocSize;
            const size_t excessIndex = freeBlocksIndex(excess);
            if (excessIndex > LARGE_BLOCK_LIST_INDEX)
               {
               // Exit the variable size list monitor and grab a fixed size list monitor
               if (::memoryAllocMonitor)
                  ::memoryAllocMonitor->exit();

               j9thread_monitor_enter(_smallBlockListsMonitor);
               freeFixedSizeBlock( new (pointer_cast<uint8_t *>(block) + allocSize) Block(excess) );
               j9thread_monitor_exit(_smallBlockListsMonitor);
               }
            else
               {
               freeVariableSizeBlock( new (pointer_cast<uint8_t *>(block) + allocSize) Block(excess) );
               if (::memoryAllocMonitor)
                  ::memoryAllocMonitor->exit();
               }
            }
         else // No block splitting required; just release memoryAllocMonitor
            {
            if (::memoryAllocMonitor)
               ::memoryAllocMonitor->exit();
            }
         allocation = block + 1; // Return pointer after the header
         }
      else // Must allocate block from segment
         {
         // memoryAllocMonitor is already acquired
         allocation = allocateFromSegmentLocked(allocSize);
         if (::memoryAllocMonitor)
            ::memoryAllocMonitor->exit();
         }
      }
   return allocation;
   }

void *
PersistentAllocator::allocateFromSegmentLocked(size_t allocSize)
   {
   J9MemorySegment *segment = findUsableSegment(allocSize);
   if (!segment)
      {
      size_t const segmentSize = allocSize < _minimumSegmentSize ? _minimumSegmentSize : allocSize;
      segment = _segmentAllocator.allocate(segmentSize, std::nothrow);
      if (!segment) 
         return NULL;
      try
         {
         _segments.push_front(TR::ref(*segment));
         }
      catch(const std::exception &e)
         {
         _segmentAllocator.deallocate(*segment);
         return NULL;
         }
      }
   TR_ASSERT(segment && remainingSpace(*segment) >= allocSize, "Failed to acquire a segment");
   Block * block = new(operator new(allocSize, *segment)) Block(allocSize);
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
PersistentAllocator::freeFixedSizeBlock(Block * block)
   {
   // Appropriate lock should have been obtained
   TR_ASSERT(block->_size > 0, "Block size is non-positive");
   size_t const index = freeBlocksIndex(block->_size);
   TR_ASSERT(index != LARGE_BLOCK_LIST_INDEX, "freeFixedSizeBlock should be used for small blocks, so index cannot be LARGE_BLOCK_LIST_INDEX");
   block->_next = _freeBlocks[index];
   _freeBlocks[index] = block;
   }

void
PersistentAllocator::freeVariableSizeBlock(Block * block)
   {
   // Appropriate lock should have been obtained
   TR_ASSERT(block->_size > 0, "Block size is non-positive");
   block->_next = NULL;
   // Add block to the variable-size-block chain which is in ascending size order.
   //
   TR_ASSERT(freeBlocksIndex(block->_size) == LARGE_BLOCK_LIST_INDEX, "freeVariableSizeBlock should be used for large blocks, so index should be LARGE_BLOCK_LIST_INDEX");
   Block * blockIterator = _freeBlocks[LARGE_BLOCK_LIST_INDEX];
   if (!blockIterator || !(blockIterator->_size < block->_size) )
      {
      block->_next = _freeBlocks[LARGE_BLOCK_LIST_INDEX];
      _freeBlocks[LARGE_BLOCK_LIST_INDEX] = block;
      }
   else
      {
      while (blockIterator->next() && blockIterator->next()->_size < block->_size)
         {
         blockIterator = blockIterator->next();
         }
      block->_next = blockIterator->next();
      blockIterator->_next = block;
      }
   }

void
PersistentAllocator::freeBlock(Block * block)
   {
   TR_ASSERT(block->_size > 0, "Block size is non-positive");

   // Adjust the used persistent memory here and not in freePersistentmemory(block, size)
   // because that call is also used to free memory that wasn't actually committed
   if (TR::AllocatedMemoryMeter::_enabled & persistentAlloc)
      {
      j9thread_monitor_enter(_smallBlockListsMonitor);
      TR::AllocatedMemoryMeter::update_freed(block->_size, persistentAlloc);
      j9thread_monitor_exit(_smallBlockListsMonitor);
      }
  
   // If this is a small block, add it to the appropriate fixed-size-block
   // chain. Otherwise add it to the variable-size-block chain which is in
   // ascending size order.
   //
   size_t const index = freeBlocksIndex(block->_size);
   if (index > LARGE_BLOCK_LIST_INDEX)
      {
      j9thread_monitor_enter(_smallBlockListsMonitor);
      freeFixedSizeBlock(block);
      j9thread_monitor_exit(_smallBlockListsMonitor);
      }
   else
      {
      if (::memoryAllocMonitor)
         ::memoryAllocMonitor->enter();
      freeVariableSizeBlock(block);
      if (::memoryAllocMonitor)
         ::memoryAllocMonitor->exit();
      }
   }

void
PersistentAllocator::deallocate(void * mem, size_t) throw()
   {
   Block * block = static_cast<Block *>(mem) - 1;
   TR_ASSERT_FATAL(block->_next == NULL, "Freeing a block that is already on the free list. block=%p next=%p", block, block->_next);
   freeBlock(block);
   }

} // namespace J9

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
