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

#ifndef MESSAGE_BUFFER_H
#define MESSAGE_BUFFER_H

#include <stdlib.h>
#include <cstring>
#include "env/jittypes.h"
#include "env/TRMemory.hpp"

namespace JITServer
{
// A wrapper around a contiguous buffer for storing JITServer message.
// The buffer is extensible, i.e. it can be reallocated to store a message
// larger than its current capacity.
class MessageBuffer
   {
public:
   MessageBuffer();

   ~MessageBuffer()
      {
      TR_Memory::jitPersistentFree(_storage);
      }


   uint32_t size();
   char *getBufferStart() { return _storage; }

   uint32_t offset(char *addr) const { return addr - _storage; }

   template <typename T>
   T *getValueAtOffset(uint32_t offset) const
      {
      return reinterpret_cast<T *>(_storage + offset);
      }

   template <typename T>
   uint32_t writeValue(const T &val)
      {
      return writeData(&val, sizeof(T));
      }

   uint32_t writeData(const void *dataStart, uint32_t dataSize);

   template <typename T>
   uint32_t reserveValue()
      {
      expandIfNeeded(size() + sizeof(T));
      char *valStart = _curPtr;
      _curPtr += sizeof(T);
      return offset(valStart);
      }

   template <typename T>
   uint32_t readValue()
      {
      return readData(sizeof(T));
      }
   uint32_t readData(uint32_t dataSize);

   void clear() { _curPtr = _storage; }
   void expandIfNeeded(uint32_t requiredSize);

private:
   uint32_t _capacity;
   char *_storage;
   char *_curPtr;
   };
};
#endif
