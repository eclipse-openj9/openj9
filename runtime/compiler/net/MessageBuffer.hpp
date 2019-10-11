#ifndef MESSAGE_BUFFER_H
#define MESSAGE_BUFFER_H

#include <stdlib.h>
#include "env/jittypes.h"
#include <cstring>

namespace JITServer
{
class MessageBuffer
   {
public:
   MessageBuffer();

   ~MessageBuffer()
      {
      free(_storage);
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
