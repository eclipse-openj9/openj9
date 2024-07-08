/*******************************************************************************
 * Copyright IBM Corp. and others 2020
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

#include "net/MessageBuffer.hpp"
#include "infra/CriticalSection.hpp"
#include "env/VerboseLog.hpp"
#include "control/Options.hpp"
#include <cstring>

namespace JITServer
{

TR::Monitor *MessageBuffer::_totalBuffersMonitor = NULL;
int MessageBuffer::_totalBuffers = 0;
TR::PersistentAllocator *MessageBuffer::_allocator = NULL;

MessageBuffer::MessageBuffer() :
   _capacity(INITIAL_BUFFER_SIZE)
   {
   OMR::CriticalSection cs(getTotalBuffersMonitor());

   if (!_allocator)
      {
      if (J9::PersistentInfo::_remoteCompilationMode == JITServer::CLIENT)
         {
         uint32_t memoryType = MEMORY_TYPE_VIRTUAL; // Force the usage of mmap for allocation
         TR::PersistentAllocatorKit kit(1 << 20/*1 MB*/, *TR::Compiler->javaVM, memoryType);
         _allocator = new (TR::Compiler->rawAllocator) TR::PersistentAllocator(kit);
         }
      else
         {
         _allocator = &TR::Compiler->persistentGlobalAllocator();
         }
      }

   _storage = allocateMemory(_capacity);
   if (!_storage)
      throw std::bad_alloc();
   _curPtr = _storage;
   _totalBuffers++;
   }

MessageBuffer::~MessageBuffer()
   {
   OMR::CriticalSection cs(getTotalBuffersMonitor());

   freeMemory(_storage);
   _totalBuffers--;

   if ((0 == _totalBuffers) && (J9::PersistentInfo::_remoteCompilationMode == JITServer::CLIENT))
      _allocator->adviseDontNeedSegments();
   }

void
MessageBuffer::expandIfNeeded(uint32_t requiredSize)
   {
   // if the data size becomes larger than buffer capacity,
   // expand the storage
   if (requiredSize > _capacity)
      {
      expand(requiredSize, size());
      }
   }

void
MessageBuffer::expand(uint32_t requiredSize, uint32_t numBytesToCopy)
   {
   TR_ASSERT_FATAL((requiredSize > _capacity), "requiredSize %u has to be greater than _capacity %u", requiredSize, _capacity);
   TR_ASSERT_FATAL((numBytesToCopy <= _capacity), "numBytesToCopy %u has to be less than _capacity %u", numBytesToCopy, _capacity);

   _capacity = computeRequiredCapacity(requiredSize);
   uint32_t curSize = size();

   char *newStorage = allocateMemory(_capacity);
   if (!newStorage)
      throw std::bad_alloc();

   memcpy(newStorage, _storage, numBytesToCopy);

   freeMemory(_storage);
   _storage = newStorage;
   _curPtr = _storage + curSize;
   }

uint32_t
MessageBuffer::writeData(const void *dataStart, uint32_t dataSize, uint8_t paddingSize)
   {
   // write dataSize bytes starting from dataStart address
   // into the buffer. Might require buffer expansion.
   // Return the offset into the buffer for the beginning of data
   uint32_t requiredSize = size() + dataSize + paddingSize;
   expandIfNeeded(requiredSize);
   char *data = _curPtr;
   memcpy(_curPtr, dataStart, dataSize);
   _curPtr += dataSize + paddingSize;
   return offset(data);
   }

uint8_t
MessageBuffer::alignCurrentPositionOn64Bit()
   {
   // Compute the amount of padding required to align _curPtr on 64-bit boundary
   uintptr_t alignedPtr = ((uintptr_t)_curPtr + 0x7) & (~((uintptr_t)0x7));
   uint8_t padding = (uint8_t)(alignedPtr - (uintptr_t)_curPtr); // Guaranteed to fit on a byte

   // Expand the buffer if it's too small to contain the padding
   uint32_t requiredSize = size() + padding;
   expandIfNeeded(requiredSize);

   // Advance _curPtr to align it on 64-bit
   _curPtr += padding;

   return padding;
   }

uint32_t
MessageBuffer::computeRequiredCapacity(uint32_t requiredSize)
   {
   // Compute the nearest power of 2 that can fit requiredSize bytes
   if (requiredSize <= _capacity)
      return _capacity;
   uint32_t extendedCapacity = _capacity * 2;
   while (requiredSize > extendedCapacity)
      extendedCapacity *= 2;
   return extendedCapacity;
   }

void
MessageBuffer::tryFreePersistentAllocator()
   {
   if (J9::PersistentInfo::_remoteCompilationMode != JITServer::CLIENT)
      return;

   OMR::CriticalSection cs(getTotalBuffersMonitor());

   if ((_totalBuffers != 0) || (_allocator == NULL))
      return;

   _allocator->~PersistentAllocator();
   TR::Compiler->rawAllocator.deallocate(_allocator);
   _allocator = NULL;
   if (TR::Options::getVerboseOption(TR_VerbosePerformance))
      TR_VerboseLog::writeLineLocked(TR_Vlog_PERF, "Freed message buffer storage allocator");
   }
};
