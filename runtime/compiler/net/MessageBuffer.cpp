/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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

#include "net/MessageBuffer.hpp"
#include <cstring>

namespace JITServer
{
MessageBuffer::MessageBuffer() :
   _capacity(INITIAL_BUFFER_SIZE)
   {
   _storage = static_cast<char *>(TR_Memory::jitPersistentAlloc(_capacity));
   if (!_storage)
      throw std::bad_alloc();
   _curPtr = _storage;
   }

void
MessageBuffer::expandIfNeeded(uint32_t requiredSize)
   {
   // if the data size becomes larger than buffer capacity,
   // expand the storage
   if (requiredSize > _capacity)
      {
      _capacity = requiredSize * 2;
      char *newStorage = static_cast<char *>(TR_Memory::jitPersistentAlloc(_capacity));
      if (!newStorage)
         throw std::bad_alloc();
      uint32_t curSize = size();
      memcpy(newStorage, _storage, curSize);
      TR_Memory::jitPersistentFree(_storage);
      _storage = newStorage;
      _curPtr = _storage + curSize;
      }
   }

void
MessageBuffer::expand(uint32_t requiredSize, uint32_t numBytesToCopy)
   {
   TR_ASSERT_FATAL((requiredSize > _capacity), "requiredSize %u has to be greater than _capacity %u", requiredSize, _capacity);
   TR_ASSERT_FATAL((numBytesToCopy <= _capacity), "numBytesToCopy %u has to be less than _capacity %u", numBytesToCopy, _capacity);

   _capacity = requiredSize;
   uint32_t curSize = size();

   char *newStorage = static_cast<char *>(TR_Memory::jitPersistentAlloc(_capacity));
   if (!newStorage)
      throw std::bad_alloc();

   memcpy(newStorage, _storage, numBytesToCopy);

   TR_Memory::jitPersistentFree(_storage);
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
};
