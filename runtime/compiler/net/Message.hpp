
#ifndef MESSAGE_H
#define MESSAGE_H

#include <stdlib.h>
#include "net/MessageBuffer.hpp"
#include "net/gen/compile.pb.h"

namespace JITServer
{

class Message
   {
public:
   enum DataType
      {
      INT32,
      INT64,
      UINT32,
      UINT64,
      BOOL,
      STRING,
      OBJECT, // only trivially-copyable,
      ENUM,
      VECTOR,
      TUPLE
      };

   // Struct containing message metadata,
   // i.e. number of data points, type and message type.
   struct MetaData
      {
      MetaData() :
         numDataPoints(0)
         {}

      uint16_t numDataPoints; // number of data points in a message
      MessageType type;
      uint64_t version;
      };

   // Struct describing a single data point in a message.
   struct DataDescriptor
      {
      DataType type;
      uint32_t size; // size of the data segment, which can include nested data

      DataDescriptor(DataType type, uint32_t size) :
         type(type),
         size(size)
         {}

      uint32_t checkIntegrity(uint32_t serializedSize);

      bool isContiguous() const { return type != VECTOR && type != TUPLE; }

      void *getDataStart() { return static_cast<void *>(&size + 1); }

      void print();
      };

   Message();

   void checkIntegrity();
   MetaData *getMetaData() const;

   void addData(const DataDescriptor &desc, const void *dataStart);
   uint32_t reserveDescriptor();
   DataDescriptor *getDescriptor(size_t idx) const;
   DataDescriptor *getLastDescriptor() const;
   void reconstruct();

   void setType(MessageType type) { getMetaData()->type = type; }
   MessageType type() const { return getMetaData()->type; }

   MessageBuffer *getBuffer() { return &_buffer; }

   char *serialize();
   uint32_t serializedSize() { return _buffer.size(); }

   void clearForRead();
   void clearForWrite();
protected:
   uint32_t _metaDataOffset;
   std::vector<uint32_t> _descriptorOffsets;
   uint32_t _serializedSizeOffset;
   MessageBuffer _buffer;
   };


class ServerMessage : public Message
   {
   };

class ClientMessage : public Message
   {
public:
   uint64_t version() { return getMetaData()->version; }
   void setVersion(uint64_t version) { getMetaData()->version = version; }
   void clearVersion() { getMetaData()->version = 0; }
   };
};
#endif
