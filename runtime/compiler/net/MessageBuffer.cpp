#include "net/MessageBuffer.hpp"
#include <cstring>

namespace JITServer
{
MessageBuffer::MessageBuffer() :
   _capacity(10000)
   {
   _storage = static_cast<char *>(malloc(_capacity));
   _curPtr = _storage;
   }

uint32_t
MessageBuffer::size()
   {
   return _curPtr - _storage;
   }

void
MessageBuffer::expandIfNeeded(uint32_t requiredSize)
   {
   if (requiredSize > _capacity)
      {
      // deallocate current storage and reallocate it to fit the message,
      // copying the values
      char *oldPtr = _curPtr;
      _capacity = requiredSize * 2;
      char *newStorage = static_cast<char *>(malloc(_capacity));
      uint32_t curSize = size();
      memcpy(newStorage, _storage, curSize);
      free(_storage);
      _storage = newStorage;
      _curPtr = _storage + curSize;
      // all pointers in message will be invalidated by this, need to fix
      // probably the easiest solution is to have _descriptors vector
      // contain offsets from the start of the storage to the beginning
      // of descriptor i, instead of the direct address
      printf("\nExpanded message buffer to %u old curPtr=%p new curPtr=%p\n", _capacity, oldPtr, _curPtr);

      }
   }

uint32_t
MessageBuffer::writeData(const void *dataStart, uint32_t dataSize)
   {
   // write size number of bytes starting from dataStart
   // into the buffer. Expand it if needed
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
   char *data = _curPtr;
   _curPtr += dataSize;
   return offset(data);
   }
};
