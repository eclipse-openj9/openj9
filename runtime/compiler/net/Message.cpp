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

#include "net/Message.hpp"
#include "infra/Assert.hpp"
#include "env/VerboseLog.hpp"

namespace JITServer
{
Message::Message()
   {
   // when the message is constructed, it's not valid
   _buffer.reserveValue<uint32_t>(); // Reserve space for encoding the size
   _buffer.reserveValue<Message::MetaData>();
   }

Message::MetaData *
Message::getMetaData() const
   {
   return _buffer.getValueAtOffset<MetaData>(sizeof(uint32_t)); // sizeof(uint32_t) represents the space for message size
   }

uint32_t
Message::addData(const DataDescriptor &desc, const void *dataStart)
   {
   // Write the descriptor itself
   uint32_t descOffset = _buffer.writeValue(desc); 
   // Write the real data and possibly some padding
   _buffer.writeData(dataStart, desc.getPayloadSize(), desc.getPaddingSize()); 
   _descriptorOffsets.push_back(descOffset);
   return desc.getTotalSize();
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
Message::deserialize()
   {
   // Assume that buffer is populated with data that defines a valid message
   // Reconstruct the message by setting metadata and pointers to descriptors
   // Note that the size of the entire message buffer has already been stripped
   //
   _buffer.readValue<MetaData>(); // This only advances curPtr in the MessageBuffer

   uint32_t numDataPoints = getMetaData()->_numDataPoints;

   _descriptorOffsets.reserve(numDataPoints);
   // TODO: do I need to clear the vector of _descriptorOffsets just in case?
   for (uint32_t i = 0; i < numDataPoints; ++i)
      {
      uint32_t descOffset = _buffer.readValue<DataDescriptor>(); // Read the descriptor itself
      _descriptorOffsets.push_back(descOffset);
      // skip the data segment, which is processed in getArgs
      _buffer.readData(getLastDescriptor()->getTotalSize());
      }
   }

char *
Message::serialize()
   {
   // write total message size to the beginning of message buffer
   // and return pointer to it
   *_buffer.getValueAtOffset<uint32_t>(0) = _buffer.size();
   return _buffer.getBufferStart();
   }

void
Message::setSerializedSize(uint32_t serializedSize)
   {
   _buffer.expandIfNeeded(serializedSize);
   _buffer.writeValue(serializedSize);
   }

void
Message::clearForRead()
   {
   _descriptorOffsets.clear();
   _buffer.clear();
   }

void
Message::clearForWrite()
   {
   _descriptorOffsets.clear();
   _buffer.clear();
   _buffer.reserveValue<uint32_t>(); // For writing the size
   _buffer.reserveValue<MetaData>(); // For writing the metadata
   }

void
Message::DataDescriptor::print()
   {
   TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "DataDescriptor[%p]: type=%d payload_size=%u padding=%u\n", 
                                  this, getDataType(), getPayloadSize(), getPaddingSize());
   if (!isPrimitive())
      {
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "DataDescriptor[%p]: nested data begin\n", this);
      DataDescriptor *curDesc = static_cast<DataDescriptor *>(getDataStart());
      while ((char *) curDesc->getDataStart() + curDesc->getTotalSize() - (char *) getDataStart() <= getTotalSize())
         {
         curDesc->print();
         curDesc = reinterpret_cast<DataDescriptor *>(reinterpret_cast<char *>(curDesc->getDataStart()) + curDesc->getTotalSize());
         }
      TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "DataDescriptor[%p] nested data end\n", this);
      }
   }

void
Message::print()
   {
   const MetaData *metaData = getMetaData();
   TR_VerboseLog::writeLineLocked(TR_Vlog_JITServer, "Message: type=%d numDataPoints=%u version=%lu\n",
                                  metaData->_type, metaData->_numDataPoints, metaData->_version);
   for (int32_t i = 0; i < _descriptorOffsets.size(); ++i)
      getDescriptor(i)->print();
   }
};
