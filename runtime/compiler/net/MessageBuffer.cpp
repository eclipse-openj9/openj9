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
   _capacity(10000)
   {
   _storage = static_cast<char *>(TR_Memory::jitPersistentAlloc(_capacity));
   if (!_storage)
      throw std::bad_alloc();
   _curPtr = _storage;
   }

uint32_t
MessageBuffer::size() const
   {
   return _curPtr - _storage;
   }

void
MessageBuffer::expandIfNeeded(uint32_t requiredSize)
   {
   // if the data size becomes larger than buffer capacity,
   // expand the storage
   if (requiredSize > _capacity)
      {
      char *oldPtr = _curPtr;
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

uint32_t
MessageBuffer::writeData(const void *dataStart, uint32_t dataSize)
   {
   // write dataSize bytes starting from dataStart address
   // into the buffer. Might require buffer expansion.
   // Return the offset into the buffer for the beginning of data
   uint32_t requiredSize = size() + dataSize;
   expandIfNeeded(requiredSize);
   char *data = _curPtr;
   memcpy(_curPtr, dataStart, dataSize);
   _curPtr += dataSize;
   return offset(data);
   }

uint32_t
MessageBuffer::readData(uint32_t dataSize)
   {
   // "read" next dataSize bytes by returning
   // the offset into the buffer and advancing pointer
   // by dataSize bytes
   char *data = _curPtr;
   _curPtr += dataSize;
   return offset(data);
   }
};
