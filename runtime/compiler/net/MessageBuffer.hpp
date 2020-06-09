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

#include "env/jittypes.h"
#include "env/TRMemory.hpp"
#include "OMR/Bytes.hpp" // for alignNoCheck

namespace JITServer
{
/**
   @class MessageBuffer
   @brief A wrapper around a contiguous, persistent memory allocated buffer
   for storing a JITServer message.

   The buffer is extensible, i.e. when the current capacity is reached, a new,
   larger buffer can be allocated and data copied there.
   Since reallocation causes addresses of values inside the buffer to change, read/write operations
   return an offset into the buffer to indicate the location of data, instead of pointers.

   Method getValueAtOffset returns a pointer to data at a given offset, but be mindful that
   the pointer might become invalid if more data is added to the buffer.

   Variable _curPtr defines the boundary of the current data. Reading/writing to/from buffer
   will always advance the pointer.
 */
class MessageBuffer
   {
public:
   MessageBuffer();

   ~MessageBuffer()
      {
      TR_Memory::jitPersistentFree(_storage);
      }


   /**
      @brief Get the current active size of the buffer.

      Note: this returns the number of bytes written to the buffer so far,
      NOT the overall capacity of the buffer. Capacity is the number of
      bytes allocated, but not necessarily used.

      @return the size of the buffer
   */
   uint32_t size() const { return _curPtr - _storage; }

   char *getBufferStart() const { return _storage; }

   /**
      @brief Return a pointer to the value at given offset inside the buffer.

      Given a type and offset, returns the pointer of that type.
      Behavior is only defined if offset does not exceed populated buffer size.

      @return ponter of the specified type at given offset
   */
   template <typename T>
   T *getValueAtOffset(uint32_t offset) const
      {
      TR_ASSERT_FATAL(offset < size(), "Offset is outside of buffer bounds");
      return reinterpret_cast<T *>(_storage + offset);
      }

   /**
      @brief Write value of type T to the buffer.

      Copies the value into buffer, expanding it if needed,
      and advances _curPtr by sizeof(T) bytes plus some padding
      so that the data inside the buffer is always 32-bit aligned
      Behavior is undefined if T is not trivially copyable (i.e. not contiguous in memory).

      @param val value to be written

      @return offset to the beginning of written value inside the buffer
   */
   template <typename T>
   uint32_t writeValue(const T &val)
      {
      static_assert(std::is_trivially_copyable<T>::value == true, "T must be trivially copyable.");
      uint8_t paddingSize = static_cast<uint8_t>(OMR::alignNoCheck(sizeof(T), sizeof(uint32_t)) - sizeof(T));
      return writeData(&val, sizeof(T), paddingSize);
      }

   /**
      @brief Write a given number of bytes into the buffer.

      Copies dataSize bytes from dataStart into the buffer, expanding it if needed,
      and advances _curPtr by (dataSize + paddingSize) bytes.

      @param dataStart pointer to the beginning of the data to be written
      @param dataSize number of bytes of real data to be written
      @param paddingSize number of bytes of padding

      @return offset to the beginning of written data inside the buffer
   */
   uint32_t writeData(const void *dataStart, uint32_t dataSize, uint8_t paddingSize);

   /**
      @brief Reserve memory for a value of type T.

      Advances _curPtr by sizeof(T) bytes, expanding the
      buffer if needed.
      Behavior is undefined if T is not trivially copyable (i.e. not contiguous in memory).

      @return offset to the beginning of the reserved memory block
   */
   template <typename T>
   uint32_t reserveValue()
      {
      expandIfNeeded(size() + sizeof(T));
      char *valStart = _curPtr;
      _curPtr += sizeof(T);
      return offset(valStart);
      }

   /**
      @brief Read next value of type T from the buffer.

      Assumes that the next unread value in the buffer is of type T.
      Advances _curPtr by sizeof(T) bytes.

      @return offset to the beginning of value
   */
   template <typename T>
   uint32_t readValue()
      {
      return readData(sizeof(T));
      }

   /**
      @brief "Read" next dataSize bytes from the buffer.

      Assumes that the buffer contains at least dataSize unread bytes.
      Advances _curPtr by dataSize bytes ( this is considered a "read")

      @return offset to the beginning of data
   */
   uint32_t readData(uint32_t dataSize)
      {
      char* data = _curPtr;
      _curPtr += dataSize; // Advance cursor
      return offset(data); // Return offset before the advance
      }

   void clear() { _curPtr = _storage; }

   /**
      @brief Check to see if the current pointer in the MessageBuffer is 64-bit aligned.
   */
   bool is64BitAligned() { return ((uintptr_t)_curPtr & ((uintptr_t)0x7)) == 0; }

   /**
      @brief Moves the current pointer in the MessageBuffer to achieve 64-bit alignment

      @return returns the number of padding bytes required for alignment (0-7)
   */
   uint8_t alignCurrentPositionOn64Bit();

   /**
      @brief Expand the underlying buffer if more than allocated memory is needed.

      If requiredSize is greater than _capacity, allocates a new buffer of size
      requiredSize * 2 (to prevent frequent reallocations),
      copies all existing data based on _curPtr location to the new buffer,
      and frees the old buffer.

      @param requiredSize the number of bytes the buffer needs to fit.
   */
   void expandIfNeeded(uint32_t requiredSize);

   /**
      @brief Expand the underlying buffer to requiredSize and copy numBytesToCopy
      from the old buffer to the new buffer when requiredSize is greater than
      the capacity, and free the old buffer.
   */
   void expand(uint32_t requiredSize, uint32_t numBytesToCopy);

   uint32_t getCapacity() const { return _capacity; }

private:
   static const size_t INITIAL_BUFFER_SIZE = 18000;
   uint32_t offset(char *addr) const { return addr - _storage; }

   uint32_t _capacity;
   char *_storage;
   char *_curPtr;
   };
};
#endif
