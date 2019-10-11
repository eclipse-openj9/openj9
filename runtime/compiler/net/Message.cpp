#include "net/Message.hpp"
#include "infra/Assert.hpp"

namespace JITServer
{
void
Message::DataDescriptor::print()
   {
   fprintf(stderr, "DataDescriptor[%p]: type=%d size=%lu\n", this, type, size);
   if (!isContiguous())
      {
      DataDescriptor *curDesc = static_cast<DataDescriptor *>(getDataStart());
      while ((char *) curDesc->getDataStart() + curDesc->size - (char *) getDataStart() <= size)
         {
         curDesc->print();
         curDesc = reinterpret_cast<DataDescriptor *>(reinterpret_cast<char *>(curDesc->getDataStart()) + curDesc->size);
         }
      }
   fprintf(stderr, "DataDescriptor[%p] end\n", this);
   }

uint32_t
Message::DataDescriptor::checkIntegrity(uint32_t serializedSize)
   {
   TR_ASSERT(size < serializedSize, "Descriptor corrupted");
   if (!isContiguous())
      {
      uint32_t totalSize = 0;
      DataDescriptor *curDesc = static_cast<DataDescriptor *>(getDataStart());
      uint32_t numNestedPoints = 0;
      while ((char *) curDesc->getDataStart() + curDesc->size - (char *) getDataStart() <= size)
         {
         totalSize += curDesc->checkIntegrity(serializedSize);
         curDesc = reinterpret_cast<DataDescriptor *>(reinterpret_cast<char *>(curDesc->getDataStart()) + curDesc->size);
         numNestedPoints++;
         }
      TR_ASSERT(totalSize + numNestedPoints * sizeof(DataDescriptor) ==  size, "Nested descriptors corrupted");
      return size;
      }
   else
      {
      return size;
      }
   }

void
Message::checkIntegrity()
   {
   uint32_t serializedSize = *_buffer.getValueAtOffset<uint32_t>(0);
   
   uint32_t numDescriptors = 0;
   DataDescriptor *curDesc = _buffer.getValueAtOffset<DataDescriptor>(20);
   while ((char *) curDesc < _buffer.getBufferStart() + serializedSize)
      {
      if (serializedSize > 20000)
         curDesc->print();
      // check integrity of nested descriptors
      curDesc->checkIntegrity(serializedSize);
      curDesc = _buffer.getValueAtOffset<DataDescriptor>(_buffer.offset((char *) curDesc->getDataStart()) + curDesc->size);
      numDescriptors++;
      }
   }

Message::Message()
   {
   // metadata is always in the beginning of the message
   _serializedSizeOffset = _buffer.reserveValue<uint32_t>();
   _metaDataOffset = _buffer.reserveValue<Message::MetaData>();
   }

Message::MetaData *
Message::getMetaData() const
   {
   return _buffer.getValueAtOffset<MetaData>(_metaDataOffset);
   }

void
Message::addData(const DataDescriptor &desc, const void *dataStart)
   {
   uint32_t descOffset = _buffer.writeValue(desc);
   _buffer.writeData(dataStart, desc.size);
   _descriptorOffsets.push_back(descOffset);
   }

uint32_t
Message::reserveDescriptor()
   {
   uint32_t descOffset = _buffer.reserveValue<DataDescriptor>();
   _descriptorOffsets.push_back(descOffset);
   return descOffset;
   }

Message::DataDescriptor *
Message::getDescriptor(size_t idx) const
   {
   uint32_t offset = _descriptorOffsets[idx];
   return _buffer.getValueAtOffset<DataDescriptor>(offset);
   }

Message::DataDescriptor *
Message::getLastDescriptor() const
   {
   return _buffer.getValueAtOffset<DataDescriptor>(_descriptorOffsets.back());
   }

void
Message::reconstruct()
   {
   // Assume that buffer is populated with data that defines a valid message
   // Reconstruct the message by setting correct meta data and pointers to descriptors
   _metaDataOffset = _buffer.readValue<MetaData>();
   for (uint16_t i = 0; i < getMetaData()->numDataPoints; ++i)
      {
      uint32_t descOffset = _buffer.readValue<DataDescriptor>();
      _descriptorOffsets.push_back(descOffset);
      // skip the actual data
      _buffer.readData(getLastDescriptor()->size);
      }
   }

char *
Message::serialize()
   {
   *_buffer.getValueAtOffset<uint32_t>(_serializedSizeOffset) = _buffer.size();
   return _buffer.getBufferStart();
   }

void
Message::clearForRead()
   {
   _descriptorOffsets.clear();
   _buffer.clear();
   _metaDataOffset = 0;
   // _serializedSizeOffset = _buffer.reserveValue<uint32_t>();
   }

void
Message::clearForWrite()
   {
   _descriptorOffsets.clear();
   _buffer.clear();
   _serializedSizeOffset = _buffer.reserveValue<uint32_t>();
   _metaDataOffset = _buffer.reserveValue<MetaData>();
   }
};
