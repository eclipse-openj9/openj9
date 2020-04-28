/*******************************************************************************
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
   const char* const Message::DataDescriptor::_descriptorNames[] = {
      "INT32",
      "INT64",
      "UINT32",
      "UINT64",
      "BOOL",
      "STRING",
      "OBJECT",
      "ENUM",
      "VECTOR",
      "SIMPLE_VECTOR",
      "EMPTY_VECTOR",
      "TUPLE",
      "INVALID"
   };

uint32_t
Message::addData(const DataDescriptor &desc, const void *dataStart, bool needs64BitAlignment)
   {
   // Write the descriptor itself
   uint32_t descOffset = _buffer.writeValue(desc);

   // If the data following the descriptor needs to be 64-bit aligned,
   // add some initial padding in the outgoing buffer and write the
   // offset to the real payload into the descriptor
   uint8_t initialPadding = 0;
   if (needs64BitAlignment && !_buffer.is64BitAligned())
      {
      initialPadding = _buffer.alignCurrentPositionOn64Bit();
      TR_ASSERT(initialPadding != 0, "Initial padding must be non zero because we checked alignment");
      DataDescriptor *serializedDescriptor = _buffer.getValueAtOffset<DataDescriptor>(descOffset);
      serializedDescriptor->addInitialPadding(initialPadding);
      }

   // Write the real data and possibly some padding at the end
   _buffer.writeData(dataStart, desc.getPayloadSize(), desc.getPaddingSize()); 
   _descriptorOffsets.push_back(descOffset);
   return desc.getTotalSize() + initialPadding;
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

uint32_t
Message::DataDescriptor::print(uint32_t nestingLevel)
   {
   uint32_t numDescriptorsPrinted = 1;
   TR_VerboseLog::write(TR_Vlog_JITServer, "");
   for (uint32_t i = 0; i < nestingLevel; ++i)
      TR_VerboseLog::write("\t");
   TR_VerboseLog::writeLine("DataDescriptor[%p]: type=%d(%6s) payload_size=%u dataOffset=%u, padding=%u", 
                        this, getDataType(), _descriptorNames[getDataType()], getPayloadSize(), getDataOffset(), getPaddingSize());
   if (!isPrimitive())
      {
      TR_VerboseLog::writeLine(TR_Vlog_JITServer, "DataDescriptor[%p]: nested data begin", this);
      DataDescriptor *curDesc = static_cast<DataDescriptor *>(getDataStart());
      while ((char *) curDesc->getDataStart() + curDesc->getTotalSize() - (char *) getDataStart() <= getTotalSize())
         {
         numDescriptorsPrinted += curDesc->print(nestingLevel + 1);
         curDesc = curDesc->getNextDescriptor();
         }
      TR_VerboseLog::writeLine(TR_Vlog_JITServer, "DataDescriptor[%p] nested data end", this);
      }

   return numDescriptorsPrinted;
   }

void
Message::print()
   {
   const MetaData *metaData = getMetaData();
   TR_VerboseLog::vlogAcquire();
   TR_VerboseLog::writeLine(TR_Vlog_JITServer, "Message: type=%d numDataPoints=%u version=%lu numDescriptors=%lu\n",
                            metaData->_type, metaData->_numDataPoints, metaData->_version, _descriptorOffsets.size());
   uint32_t numDescriptorsPrinted = 0;
   for (uint32_t i = 0; i < _descriptorOffsets.size(); i += numDescriptorsPrinted)
      {
      DataDescriptor* desc = getDescriptor(i);
      numDescriptorsPrinted = desc->print(0);
      }
   TR_VerboseLog::vlogRelease();
   }
};
