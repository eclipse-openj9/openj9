/*******************************************************************************
 * Copyright IBM Corp. and others 2000
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "OMR/Bytes.hpp"
#include "control/CompilationRuntime.hpp"
#include "control/Options.hpp"
#include "control/Options_inlines.hpp"
#include "env/PersistentAllocator.hpp"
#include "env/VerboseLog.hpp"
#include "il/DataTypes.hpp"
#include "infra/Monitor.hpp"

#ifdef LINUX
#include <sys/mman.h> // for madvise
#ifndef MADV_NOHUGEPAGE
#define MADV_NOHUGEPAGE  15
#endif // MADV_NOHUGEPAGE
#ifndef MADV_PAGEOUT
#define MADV_PAGEOUT     21
#endif // MADV_PAGEOUT
#endif // LINUX

namespace J9 {

PersistentAllocator::PersistentAllocator(const PersistentAllocatorKit &creationKit) :
   _minimumSegmentSize(creationKit.minimumSegmentSize),
   _segmentAllocator(
#if defined(J9VM_OPT_JITSERVER)
                     creationKit.javaVM.internalVMFunctions->isJITServerEnabled(&creationKit.javaVM) ?
                     MEMORY_TYPE_VIRTUAL | MEMORY_TYPE_JIT_PERSISTENT : // use virtual memory for JITServer because we need to release memory
                                                                        // back into the system once allocator is destroyed, which is not
                                                                        // guaranteed with regular malloc
#endif
                     MEMORY_TYPE_JIT_PERSISTENT | creationKit.memoryType,
                     creationKit.javaVM),
   _freeBlocks(),
#if defined(J9VM_OPT_JITSERVER)
   _isJITServer(creationKit.javaVM.internalVMFunctions->isJITServerEnabled(&creationKit.javaVM)),
#endif
   _segments(SegmentContainerAllocator(RawAllocator(&creationKit.javaVM))),
   _numSegments(0),
   _javaVM(creationKit.javaVM)
   {
   _disclaimEnabled =
#if defined(J9VM_OPT_JITSERVER)
                    !_isJITServer &&
#endif
                    (creationKit.memoryType & MEMORY_TYPE_VIRTUAL);
   j9thread_monitor_init_with_name(&_smallBlockMonitor, 0, "JIT-PersistentAllocatorSmallBlockMonitor");
   j9thread_monitor_init_with_name(&_largeBlockMonitor, 0, "JIT-PersistentAllocatorLargeBlockMonitor");
   j9thread_monitor_init_with_name(&_segmentMonitor, 0, "JIT-PersistentAllocatorSegmentMonitor");
   if (!_smallBlockMonitor || !_largeBlockMonitor || !_segmentMonitor)
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
   j9thread_monitor_destroy(_smallBlockMonitor);
   _smallBlockMonitor = NULL;
   j9thread_monitor_destroy(_largeBlockMonitor);
   _largeBlockMonitor = NULL;
   j9thread_monitor_destroy(_segmentMonitor);
   _segmentMonitor = NULL;
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
   for (block = _freeBlocks[LARGE_BLOCK_LIST_INDEX]; block && block->size() < allocSize; prev = block, block = prev->next())
      {}

   if (block)
      {
      if (prev)
         prev->setNext(block->next());
      else
         _freeBlocks[LARGE_BLOCK_LIST_INDEX] = block->next();

      block->setNext(NULL);
      }
   return block;
   }

#if defined(J9VM_OPT_JITSERVER)
PersistentAllocator::Block *
PersistentAllocator::allocateFromIndexedListLocked(size_t allocSize)
   {
   checkIntegrity("allocateFromIndexedListLocked start");
   ExtendedBlock * block = NULL;
   // Find the interval this allocation should come from
   size_t index = getInterval(allocSize);
   ExtendedBlock *startBlock = NULL;
   // Need to check larger intervals as well
   for (size_t i = index; i < NUM_INTERVALS; i++)
      {
      startBlock = _startInterval[i];
      if (startBlock)
         break;
      }
   if (!startBlock)
      {
      return NULL;
      }
   // Search for a block big enough
   for (block = startBlock; block && block->size() < allocSize; block = block->next())
      {}
   if (block)
      {
      // If this entry is a chain of 2 or more blocks of same size, detach the next entry
      ExtendedBlock * nextBlock = block->nextBlockSameSize();
      if (nextBlock)
         {
         block->setNextBlockSameSize(nextBlock->nextBlockSameSize());
         block = nextBlock; // This is what I am going to return
         }
      else
         {
         // Detach block from doubly linked list
         if (block->previous())
            block->previous()->setNext(block->next());
         else
            _freeBlocks[LARGE_BLOCK_LIST_INDEX] = reinterpret_cast<Block*>(block->next());
         if (block->next())
            block->next()->setPrevious(block->previous());

         // The block that was detached belonged to an interval 'index'.
         // Adjust _startInterval and _endInterval pointers if needed
         index = getInterval(block->size());
         TR_ASSERT(_startInterval[index] && _endInterval[index], "Error with boundaries for interval %zu\n", index);
         if (_startInterval[index] == block)
            {
            if (_endInterval[index] == block)
               {
               // This is the only block in this interval
               _startInterval[index] = NULL;
               _endInterval[index] = NULL;
               }
            else
               {
               // This is the first block in this interval and there are other blocks as well
               _startInterval[index] = block->next();
               TR_ASSERT(block->next(), "block must have a successor");
               }
            }
         else
            {
            if (_endInterval[index] == block)
               {
               // This is the last block in this interval and there are other blocks before it
               _endInterval[index] = block->previous();
               TR_ASSERT(block->previous(), "block must have a predecessor");
               }
            }
         }
      block->setNext(NULL);
      block->setPrevious(NULL);
      block->setNextBlockSameSize(NULL);
      }
   checkIntegrity("allocateFromVariabileSizeListLocked end");
   return reinterpret_cast<Block*>(block);
   }
#endif /* defined(J9VM_OPT_JITSERVER) */

void *
PersistentAllocator::allocateInternal(size_t requestedSize)
   {
   // Workaround for when user wants to allocate 0 bytes
   if (requestedSize == 0)
      requestedSize = sizeof(void *);
   TR_ASSERT( sizeof(Block) == mem_round( sizeof(Block) ),"Persistent block size will prevent us from properly aligning allocations.");

   size_t const dataSize = mem_round(requestedSize);
   size_t const allocSize = sizeof(Block) + dataSize;
   void * allocation = NULL;

   if (TR::AllocatedMemoryMeter::_enabled & persistentAlloc)
      {
      // Use _smallBlockMonitor to protect TR::AllocatedMemoryMeter::update_allocated
      // because accessing the variable-size-block list protected by _largeBlockMonitor
      // takes longer and we may be penalizing access to fixed size block which should be very fast
      j9thread_monitor_enter(_smallBlockMonitor);
      TR::AllocatedMemoryMeter::update_allocated(allocSize, persistentAlloc);
      j9thread_monitor_exit(_smallBlockMonitor);
      }

   // If this is a small block try to allocate it from the appropriate
   // fixed-size-block chain.
   //
   size_t const index = freeBlocksIndex(allocSize);
   if (index != LARGE_BLOCK_LIST_INDEX) // fixed-size-block chain
      {
      j9thread_monitor_enter(_smallBlockMonitor);
      Block *block = _freeBlocks[index];
      if (block)
         {
         _freeBlocks[index] = block->next();
         block->setNext(NULL);
         j9thread_monitor_exit(_smallBlockMonitor);
         allocation = block + 1; // Return pointer after the header
         }
      else // Couldn't find suitable free block; need to allocate from segment
         {
         j9thread_monitor_exit(_smallBlockMonitor);

         // Find the first persistent segment with enough free space
         j9thread_monitor_enter(_segmentMonitor);
         allocation = allocateFromSegmentLocked(allocSize);
         j9thread_monitor_exit(_segmentMonitor);
         }
      }
   else // Variable size block allocation
      {
      j9thread_monitor_enter(_largeBlockMonitor);
      Block *block =
#if defined(J9VM_OPT_JITSERVER)
         _isJITServer ? allocateFromIndexedListLocked(allocSize) :
#endif
                        allocateFromVariableSizeListLocked(allocSize);
      if (block)
         {
         // If the block I found is bigger than what I need,
         // split it and put the remaining block back onto the free list
         size_t const excess = block->size() - allocSize;
         if (excess > sizeof(Block))
            {
            block->setSize(allocSize);
            const size_t excessIndex = freeBlocksIndex(excess);
            if (excessIndex > LARGE_BLOCK_LIST_INDEX)
               {
               // Exit the variable size list monitor and grab the fixed size list monitor
               j9thread_monitor_exit(_largeBlockMonitor);

               j9thread_monitor_enter(_smallBlockMonitor);
               freeFixedSizeBlock(new (pointer_cast<uint8_t *>(block) + allocSize) Block(excess));
               j9thread_monitor_exit(_smallBlockMonitor);
               }
            else
               {
#if defined(J9VM_OPT_JITSERVER)
               if (_isJITServer)
                  freeBlockToIndexedList(new (pointer_cast<uint8_t *>(block) + allocSize) Block(excess));
               else
#endif
                  freeVariableSizeBlock(new (pointer_cast<uint8_t *>(block) + allocSize) Block(excess));
               j9thread_monitor_exit(_largeBlockMonitor);
               }
            }
         else // No block splitting required; just release _largeBlockMonitor
            {
            j9thread_monitor_exit(_largeBlockMonitor);
            }
         allocation = block + 1; // Return pointer after the header
         }
      else // Must allocate block from segment
         {
         // Exit the variable size list monitor and grab the segment monitor
         j9thread_monitor_exit(_largeBlockMonitor);

         j9thread_monitor_enter(_segmentMonitor);
         allocation = allocateFromSegmentLocked(allocSize);
         j9thread_monitor_exit(_segmentMonitor);
         }
      }
#if defined(J9VM_OPT_JITSERVER)
   if (_isJITServer && allocation)
      {
      // keep track of which allocator this block belongs to (global/per client)
      Block *block = reinterpret_cast<Block *>(allocation) - 1;
      block->setNext(reinterpret_cast<Block *>(this));
      }
#endif
   return allocation;
   }

void *
PersistentAllocator::allocateFromSegmentLocked(size_t allocSize)
   {
   J9MemorySegment *segment = findUsableSegment(allocSize);
   if (!segment)
      {
      size_t segmentSize = allocSize < _minimumSegmentSize ? _minimumSegmentSize : allocSize;
#ifdef LINUX
   UDATA defaultPageSize = 0;
   if (_disclaimEnabled)
      {
      PORT_ACCESS_FROM_JAVAVM(&_javaVM);
      defaultPageSize = j9vmem_supported_page_sizes()[0];
      segmentSize = OMR::align(segmentSize, (size_t)defaultPageSize); // Round up to a multiple of default page size if needed
      }
#endif // LINUX
      segment = _segmentAllocator.allocate(segmentSize, std::nothrow);
      if (!segment)
         return NULL;
#ifdef LINUX
      if (_disclaimEnabled)
         {
         // The start address must be page aligned because we obtained it from mmap
         TR_ASSERT_FATAL(OMR::alignedNoCheck((uintptr_t)segment->heapBase, defaultPageSize),
                         "Start address of the persistent segment is not page aligned");
         size_t segLength = segment->heapTop - segment->heapBase;
         if (madvise(segment->heapBase, segLength, MADV_NOHUGEPAGE) != 0)
            {
            if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerbosePerformance))
               TR_VerboseLog::writeLine(TR_Vlog_INFO, "Failed to set MADV_HUGEPAGE for persistent memory segment");
            }
         // If the memory segment is backed by a file, disable read-ahead
         // so that touching one byte brings a single page in
         if (segment->vmemIdentifier.allocator == OMRPORT_VMEM_RESERVE_USED_MMAP_SHM)
            {
            // Disable read-ahead so that touching one byte brings a single page in
            if (madvise(segment->heapBase, segLength, MADV_RANDOM) != 0)
               {
               if (TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerbosePerformance))
                  TR_VerboseLog::writeLine(TR_Vlog_INFO, "Failed to set MADV_RANDOM for persistent memory segment");
               }
            }
         }
#endif // LINUX
      _numSegments++;
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
   TR_ASSERT(block->size() > 0, "Block size is non-positive");
   size_t const index = freeBlocksIndex(block->size());
   TR_ASSERT(index != LARGE_BLOCK_LIST_INDEX, "freeFixedSizeBlock should be used for small blocks, so index cannot be LARGE_BLOCK_LIST_INDEX");
   block->setNext(_freeBlocks[index]);
   _freeBlocks[index] = block;
   }

void
PersistentAllocator::freeVariableSizeBlock(Block * block)
   {
   // Appropriate lock should have been obtained
   TR_ASSERT(block->size() > 0, "Block size is non-positive");
   block->setNext(NULL);
   // Add block to the variable-size-block chain which is in ascending size order.
   //
   TR_ASSERT(freeBlocksIndex(block->size()) == LARGE_BLOCK_LIST_INDEX, "freeVariableSizeBlock should be used for large blocks, so index should be LARGE_BLOCK_LIST_INDEX");
   Block * blockIterator = _freeBlocks[LARGE_BLOCK_LIST_INDEX];
   if (!blockIterator || !(blockIterator->size() < block->size()) )
      {
      block->setNext(_freeBlocks[LARGE_BLOCK_LIST_INDEX]);
      _freeBlocks[LARGE_BLOCK_LIST_INDEX] = block;
      }
   else
      {
      while (blockIterator->next() && blockIterator->next()->size() < block->size())
         {
         blockIterator = blockIterator->next();
         }
      block->setNext(blockIterator->next());
      blockIterator->setNext(block);
      }
   }

#if defined(J9VM_OPT_JITSERVER)
size_t
PersistentAllocator::getInterval(size_t blockSize)
   {
   // Find the power-of-two interval that this block size belongs to
   TR_ASSERT(blockSize >= PERSISTENT_BLOCK_SIZE_BUCKETS * sizeof(void *),
             "getInterval should be used only on big blocks. blockSize=%zu", blockSize);
   // If very large block
   if (blockSize >= (1 << (BITS_TO_SHIFT_FIRST_INTERVAL + NUM_INTERVALS - 1)))
      return NUM_INTERVALS - 1; // last one
   blockSize >>= BITS_TO_SHIFT_FIRST_INTERVAL;
   // log2 implementation that uses a lookup table of 16 entries
   // The value to compute the log on cannot be larger than 1 byte
   static_assert(NUM_INTERVALS <= 9, "For large values of NUM_INTERVALS the log2 implementation below will not work");
   TR_ASSERT(blockSize < (1 << 8), "blockSize is too large %zu to apply the log2 implementation below", blockSize);
   static const uint8_t _logTable[16] = { 0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3 };
   size_t upper;
   return (upper = blockSize >> 4) ? 4 + _logTable[upper] : _logTable[blockSize];
   }

void
PersistentAllocator::freeBlockToIndexedList(Block * blockToBeFreed)
   {
   checkIntegrity("freeVariableSizeBlock start");
   TR_ASSERT_FATAL(blockToBeFreed->next() == NULL, "Double free detected %p", blockToBeFreed);
   TR_ASSERT(blockToBeFreed->size() > 0, "Block size is non-positive");
   TR_ASSERT(freeBlocksIndex(blockToBeFreed->size()) == LARGE_BLOCK_LIST_INDEX, "We must be working on the variable size block list");

   ExtendedBlock *block = reinterpret_cast<ExtendedBlock *>(blockToBeFreed);
   block->init(); // reset links to other nodes

   size_t index = getInterval(block->size());

   ExtendedBlock * blockIterator = reinterpret_cast<ExtendedBlock*>(_freeBlocks[LARGE_BLOCK_LIST_INDEX]);
   if (!blockIterator || blockIterator->size() > block->size())
      {
      // Add at the beginning of the list
      block->setNext(reinterpret_cast<ExtendedBlock*>(_freeBlocks[LARGE_BLOCK_LIST_INDEX]));
      _freeBlocks[LARGE_BLOCK_LIST_INDEX] = reinterpret_cast<Block*>(block);
      if (block->next())
         block->next()->setPrevious(block);
      // Adjust the interval bounderies
      _startInterval[index] = block;
      if (!_endInterval[index])
         _endInterval[index] = block;
      }
   else // This will not be the very first block in the variable size block list
      {
      // Find position in the list
      if (_startInterval[index])
         {
         // Other blocks are present in this interval
         ExtendedBlock *startBlock = _startInterval[index];
         ExtendedBlock *blockIterator = startBlock;

         ExtendedBlock *prev = NULL;
         while (blockIterator && blockIterator->size() < block->size())
            {
            prev = blockIterator;
            blockIterator = blockIterator->next();
            }
         if (blockIterator)
            {
            // Insert at or before blockIterator
            if (blockIterator->size() == block->size())
               {
               // Add the block to the list of blocks of same size
               block->setNextBlockSameSize(blockIterator->nextBlockSameSize());
               blockIterator->setNextBlockSameSize(block);
               }
            else // Insert before blockIterator
               {
               block->setNext(blockIterator);
               block->setPrevious(blockIterator->previous());
               TR_ASSERT(blockIterator->previous(), "blockIterator->previous() must exist because we already treated the case where we insert at the beginning of the list");
               blockIterator->previous()->setNext(block);
               blockIterator->setPrevious(block);
               // Adjust the interval bounderies
               if (_startInterval[index]->size() > block->size())
                  {
                  // block becomes the first entry in this interval
                  TR_ASSERT(_startInterval[index] == blockIterator, "blockInterator must be first block in this interval");
                  _startInterval[index] = block;
                  }
               else // Insert in the middle or end of interval
                  {
                  // blockIterator may belong to the next interval in
                  // which case _endInterval needs to be changed to block
                  if (_endInterval[index]->size() < block->size())
                     {
                     TR_ASSERT(getInterval(blockIterator->size()) > index, "blockInterator must be first block in the next interval: index=%zu b->sz=%zu bi->sz=%zu", index, block->size(), blockIterator->size());
                     _endInterval[index] = block;
                     }
                  }
               }
            }
         else // Insert after 'prev'; block will be the very last entry in the list
            {
            TR_ASSERT(prev, "prev must exist");
            TR_ASSERT(!prev->next(), "this must be the last element in the list");
            block->setPrevious(prev);
            block->setNext(NULL);
            prev->setNext(block);
            _endInterval[index] = block;
            }
         }
      else // There are no other blocks in this interval
         {
         // Find the closest interval that has a block
         // I verified that the block will not be attached at the beginning of the list.
         // Thus, there must be a previous interval with some block in it
         TR_ASSERT(index > 0, "Index must be greater than 0");
         for (int i = (int)index - 1; i >= 0; i--)
            {
            if (_endInterval[i])
               {
               block->setPrevious(_endInterval[i]);
               block->setNext(_endInterval[i]->next());
               TR_ASSERT(block->size() > _endInterval[i]->size(), "wrong sizes previous");
               if (block->next())
                  {
                  block->next()->setPrevious(block);
                  TR_ASSERT(block->size() < block->next()->size(), "wrong sizes after block->size()=%zu next->size()=%zu", block->size(), block->next()->size());
                  }
               _endInterval[i]->setNext(block);
               break;
               }
            }
         TR_ASSERT(block->previous(), "I must have attached my block to the list");
         _startInterval[index] = block;
         _endInterval[index] = block;
         }
      }
      checkIntegrity("freeVariableSizeBlock end");
   }

void
PersistentAllocator::checkIntegrity(const char msg[])
   {
#ifdef CHECK_PERSISTENT_ALLOCATOR_INTEGRITY
   ExtendedBlock *firstBlock = reinterpret_cast<ExtendedBlock*>(_freeBlocks[LARGE_BLOCK_LIST_INDEX]);
   ExtendedBlock *prevBlock = NULL;
   TR_ASSERT_FATAL(!firstBlock || firstBlock->previous() == NULL, "Error for first block");
   for (ExtendedBlock *block = firstBlock; block; prevBlock = block, block = block->next())
      {
      if (prevBlock)
         {
         TR_ASSERT_FATAL(prevBlock->size() < block->size(), "Blocks are not correctly ordered");
         TR_ASSERT_FATAL(block->previous() == prevBlock, "block->previous() != prevBlock");
         }
      }

   for (size_t i = 0; i < NUM_INTERVALS; i++)
      {
      if (_startInterval[i])
         {
         TR_ASSERT_FATAL(_endInterval[i], "If startInterval exist, then endInterval must exist as well");
         if (i == 0)
            TR_ASSERT_FATAL(_startInterval[i] == reinterpret_cast<ExtendedBlock*>(_freeBlocks[LARGE_BLOCK_LIST_INDEX]), "Error with first block");
         size_t index = getInterval(_startInterval[i]->size());
         TR_ASSERT_FATAL(i == index, "startInterval block is not in the correct interval");
         // Previous block must belong to a previous interval
         ExtendedBlock *previousBlock = _startInterval[i]->previous();
         if (previousBlock)
            {
            TR_ASSERT_FATAL(i > 0, "I cannot be at the first interval");
            index = getInterval(previousBlock->size());
            TR_ASSERT_FATAL(_endInterval[index] == previousBlock, "Error with intevals");
            }
         }
      if (_endInterval[i])
         {
         TR_ASSERT_FATAL(_startInterval[i], "If endInterval exist, then startInterval must exist as well");
         size_t index = getInterval(_endInterval[i]->size());
         TR_ASSERT_FATAL(i == index, "endInterval block is not in the correct interval i=%zu index=%zu start->size()=%zu end->size()=%zu\n", i, index, _startInterval[i]->size(), _endInterval[i]->size());
         ExtendedBlock *nextBlock = _endInterval[i]->next();
         if (nextBlock)
            {
            size_t index = getInterval(nextBlock->size());
            TR_ASSERT_FATAL(nextBlock == _startInterval[index], "Error with interval %zu end->size()=%zu index=%zu next->size()=%zu\n", i, _endInterval[i]->size(), index, nextBlock->size());
            }
         }
      if (!_startInterval[i])
         {
         TR_ASSERT_FATAL(!_endInterval[i], "If startInterval is NULL, then endInterval must be NULL as well");
         }
      if (!_endInterval[i])
         {
         TR_ASSERT_FATAL(!_startInterval[i], "If endInterval is NULL, then startInterval must be NULL as well");
         }
      }
#endif // ifdef CHECK_PERSISTENT_ALLOCATOR_INTEGRITY
   }
#endif /* defined(J9VM_OPT_JITSERVER) */

void
PersistentAllocator::freeBlock(Block *block)
   {
   TR_ASSERT(block->size() > 0, "Block size is non-positive");

   // Adjust the used persistent memory here and not in freePersistentmemory(block, size)
   // because that call is also used to free memory that wasn't actually committed
   if (TR::AllocatedMemoryMeter::_enabled & persistentAlloc)
      {
      j9thread_monitor_enter(_smallBlockMonitor);
      TR::AllocatedMemoryMeter::update_freed(block->size(), persistentAlloc);
      j9thread_monitor_exit(_smallBlockMonitor);
      }

   // If this is a small block, add it to the appropriate fixed-size-block
   // chain. Otherwise add it to the variable-size-block chain which is in
   // ascending size order.
   //
   size_t const index = freeBlocksIndex(block->size());
   if (index > LARGE_BLOCK_LIST_INDEX)
      {
      j9thread_monitor_enter(_smallBlockMonitor);
      freeFixedSizeBlock(block);
      j9thread_monitor_exit(_smallBlockMonitor);
      }
   else
      {
      j9thread_monitor_enter(_largeBlockMonitor);
#if defined(J9VM_OPT_JITSERVER)
      if (_isJITServer)
         freeBlockToIndexedList(block);
      else
#endif
         freeVariableSizeBlock(block);
      j9thread_monitor_exit(_largeBlockMonitor);
      }
   }

void
PersistentAllocator::deallocate(void * mem, size_t) throw()
   {
   Block * block = static_cast<Block *>(mem) - 1;
#if defined(J9VM_OPT_JITSERVER)
   if (_isJITServer)
      {
      TR_ASSERT_FATAL(block->next() == reinterpret_cast<Block *>(this), "Freeing a block that was created by another allocator or is already on the free list. mem=%p block=%p next=%p this=%p", mem, block, block->next(), reinterpret_cast<Block *>(this));
      block->setNext(NULL);
      }
   else
      {
      TR_ASSERT_FATAL(block->next() == NULL, "Freeing a block that is already on the free list. block=%p next=%p", block, block->next());
      }
#else
   TR_ASSERT_FATAL(block->next() == NULL, "Freeing a block that is already on the free list. block=%p next=%p", block, block->next());
#endif
   freeBlock(block);
   }

// Note: this method has high overhead. Use it only for debugging
extern "C" {
#ifdef LINUX
   int countPresentPagesInSegment(const J9MemorySegment *segment, int &swappedCount, int &filePageCount);
#endif
}

//#define DEBUG_DISCLAIM
//----------------------------- disclaimAllSegments -----------------------------
// Disclaim memory for all the segments belonging to this persistent allocator.
// Return the number of segments successfully disclaimed
// Side effect: may disable disclaiming if the kernel does not support it
//---------------------------------------------------------------------------
int
PersistentAllocator::disclaimAllSegments()
   {
   if (!isDisclaimEnabled())
      return 0;
   int numSegDisclaimed = 0;
#ifdef LINUX
   bool verbose = TR::Options::getCmdLineOptions()->getVerboseOption(TR_VerbosePerformance);
   TR::CompilationInfo *compInfo = TR::CompilationInfo::get(_javaVM.jitConfig);
   bool canDisclaimOnSwap = TR::Options::getCmdLineOptions()->getOption(TR_DisclaimMemoryOnSwap) && !compInfo->isSwapMemoryDisabled();
   j9thread_monitor_enter(_segmentMonitor);
   for (auto segmentIterator = _segments.begin(); segmentIterator != _segments.end(); ++segmentIterator)
      {
      J9MemorySegment &segment = *segmentIterator;
      if (segment.vmemIdentifier.allocator == OMRPORT_VMEM_RESERVE_USED_MMAP_SHM || // Can disclaim to file
        ((segment.vmemIdentifier.mode & J9PORT_VMEM_MEMORY_MODE_VIRTUAL) && canDisclaimOnSwap)) // Can disclaim to swap
         {
         size_t segLength = segment.heapTop - segment.heapBase;
#ifdef DEBUG_DISCLAIM
         int swappedCount = 0;
         int filePageCount = 0;
         int numPresentPages = countPresentPagesInSegment(&segment, swappedCount, filePageCount);
         fprintf(stderr, "Will disclaim persistent memory segment %p of size %zu. Present pages =%d swapped=%d fileMapped=%d\n",
                 &segment, segLength, numPresentPages,  swappedCount, filePageCount);
#endif
         int ret = madvise(segment.heapBase, segLength, MADV_PAGEOUT);
         if (ret != 0)
            {
            // madvise() could fail with EINVAL if MADV_PAGEOUT is not supported by the kernel
            if (verbose)
               TR_VerboseLog::writeLine(TR_Vlog_PERF, "WARNING: Failed to use madvise to disclaim memory for persistent memory");
            if (ret == EINVAL)
               {
               _disclaimEnabled = false; // Don't try to disclaim again, since support seems to be missing
               if (verbose)
                  TR_VerboseLog::writeLine(TR_Vlog_PERF, "WARNING: Disabling persistent memory disclaiming for this allocator from now on");
               }
            }
         else
            {
#ifdef DEBUG_DISCLAIM
            swappedCount = 0;
            filePageCount = 0;
            numPresentPages = countPresentPagesInSegment(&segment, swappedCount, filePageCount);
            fprintf(stderr, "Disclaimed persistent memory segment %p of size %zu. Present pages =%d swapped=%d fileMapped=%d\n",
                    &segment, segLength, numPresentPages, swappedCount, filePageCount);
#endif
            numSegDisclaimed++;
            }
         }
      else
         {
         // I am not allowed to use disclaim because the memory is not backed by a file
         if (verbose)
            TR_VerboseLog::writeLine(TR_Vlog_PERF, "WARNING: Persistent memory segment %p is not backed by a file", &segment);
         }
      }
   j9thread_monitor_exit(_segmentMonitor);
#endif // LINUX
   return numSegDisclaimed;
   }

void
TR::PersistentAllocator::adviseDontNeedSegments()
   {
#ifdef LINUX
   j9thread_monitor_enter(_segmentMonitor);
   for (auto segmentIterator = _segments.begin(); segmentIterator != _segments.end(); ++segmentIterator)
      {
      J9MemorySegment &segment = *segmentIterator;
      size_t segLength = segment.heapTop - segment.heapBase;
      madvise(segment.heapBase, segLength, MADV_DONTNEED);
      }
   j9thread_monitor_exit(_segmentMonitor);
#endif
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
