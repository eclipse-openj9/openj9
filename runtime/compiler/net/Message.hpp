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

#ifndef MESSAGE_H
#define MESSAGE_H

#include <vector>
#include <stdlib.h>
#include "net/MessageBuffer.hpp"
#include "net/MessageTypes.hpp"

namespace JITServer
{

class Message
   {
public:
   struct MetaData
      {
      MetaData() :
         numDataPoints(0)
         {}

      uint16_t numDataPoints; // number of data points in a message
      MessageType type;
      uint64_t version;
      };

   // Descriptor for a single data point in a message
   struct DataDescriptor
      {
      // Data types that can be sent in a message
      // Adding new types is possible, as long as
      // serialization/deserialization functions
      // are added to RawTypeConvert
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
         TUPLE,
         LAST_TYPE
         };

      DataDescriptor(DataType type, uint32_t size) :
         type(type),
         size(size)
         {}

      bool isContiguous() const { return type != VECTOR && type != TUPLE; }

      void *getDataStart()
         {
         // the data must always be located immediately after
         // its descriptor.
         return static_cast<void *>(&size + 1);
         }

      void print();

      DataType type;
      uint32_t size; // size of the data segment, which can include nested data
      };

   Message();

   MetaData *getMetaData() const;

   void addData(const DataDescriptor &desc, const void *dataStart);
   uint32_t reserveDescriptor();
   DataDescriptor *getDescriptor(size_t idx) const;
   DataDescriptor *getLastDescriptor() const;
   uint32_t getNumDescriptors() const { return _descriptorOffsets.size(); }

   void setType(MessageType type) { getMetaData()->type = type; }
   MessageType type() const { return getMetaData()->type; }

   char *serialize();
   uint32_t serializedSize() { return _buffer.size(); }
   void deserialize();
   void setSerializedSize(uint32_t serializedSize);
   char *getBufferStartForRead() { return _buffer.getBufferStart(); }

   void clearForRead();
   void clearForWrite();

   void print();
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
