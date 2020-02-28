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
/**
   @class Message
   @brief Representation of a JITServer remote message.
   
   All of the actual data and metadata is stored inside MessageBuffer,
   this class just provides an interface to access/add it.
   This is done to minimize the amount of copying when sending/receiving
   messages over the network.

   Each message contains an offset to metadata and a vector of offsets to data descriptors,
   where each descriptor describes a single value sent inside the message.
*/
class Message
   {
public:
   /**
      @class MetaData
      @brief Describes general parameters of a message: number of datapoints, message type, and version.
   */
   struct MetaData
      {
      MetaData() :
         _numDataPoints(0)
         {}

      uint32_t _numDataPoints; // number of data points in a message
      MessageType _type;
      uint64_t _version;
      };

   /**
      @class DataDescriptor
      @brief Metadata containing the type and size a single value in a message
   */
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

      /**
         @brief Tells if data attached to this descriptor is a type that
         can be directly copied from its address into the contiguous buffer.

         Currently, vectors and tuples are the only non-primitive supported types.
         Note: while vectors of primitive types could be copied directly if you copy
         from &v[0], it will not work if it's a vector of vectors, or a vector of tuples.

         @return Whether the data descriptor is primitive
      */
      bool isPrimitive() const { return type != VECTOR && type != TUPLE; }

      /**
         @brief Get the pointer to the beginning of data attached to this descriptor.

         This assumes that the data is always located immediately after its descriptor.

         TODO: maybe it's bad design to make this kind of assumption, even though it's valid
         in current implementation. Maybe a better solution would be to store an offset to 
         beginning of the data.

         @return A pointer to the beginning of the data
      */
      void *getDataStart()
         {
         return static_cast<void *>(this + 1);
         }

      void print();

      DataType type;
      uint32_t size; // size of the data segment, which can include nested data
      };

   Message();

   MetaData *getMetaData() const;

   /**
      @brief Add a new data point to the message.

      Writes the descriptor and attached data to the MessageBuffer
      and updates the message structure.

      @param desc Descriptor for the new data
      @param dataStart Pointer to the new data
   */
   void addData(const DataDescriptor &desc, const void *dataStart);

   /**
      @brief Allocate space for a descriptor in the MessageBuffer
      and update the message structure without writing any actual data.

      This method is needed for some specific cases, mainly for writing
      non-primitive data points.
      TODO: can probably get rid of this, if addData is changed to return
      the index of the descriptor added. Then writing a descriptor with size 0
      would achieve the same result.

      @return The offset to the descriptor reserved
   */
   uint32_t reserveDescriptor();

   /**
      @brief Get a pointer to the descriptor at given index

      @param idx Index of the descriptor

      @return A pointer to the descriptor
   */
   DataDescriptor *getDescriptor(size_t idx) const;

   /**
      @brief Get a pointer to the last descriptor added to the message.

      @return A pointer to the last descriptor
   */
   DataDescriptor *getLastDescriptor() const;
   
   /**
      @brief Get a total number of descriptors in the message.

      @return Number of descriptors
   */
   uint32_t getNumDescriptors() const { return _descriptorOffsets.size(); }

   /**
      @brief Set message type

      @param type The new message type
   */
   void setType(MessageType type) { getMetaData()->_type = type; }

   /**
      @brief Get message type

      @return the message type
   */
   MessageType type() const { return getMetaData()->_type; }

   /**
      @brief Serialize the message

      @return A pointer to the beginning of the serialized message
   */
   char *serialize();

   /**
      @brief The size of the serialized message.

      @return the serialized size
   */
   uint32_t serializedSize() { return _buffer.size(); }

   /**
      @brief Rebuild the message from the MessageBuffer

      Assuming that the MessageBuffer contains data for a valid
      message, rebuilds the message structure, i.e. sets offsets
      for metadata and data descriptors.
   */
   void deserialize();

   /**
      @brief Set serialized size of the message
   */
   void setSerializedSize(uint32_t serializedSize);

   /**
      @brief Get the pointer to the start of the buffer.
      
      This method should be used only to read an incoming message.
      TODO: this is probably bad design, as it's the only place where
      Message explicitly exposes MessageBuffer, but I'm not sure how
      to fix this without resorting to creating more Read/Write streams,
      like protobuf does.
      
      @return A pointer to the beginning of the MessageBuffer
   */
   char *getBufferStartForRead() { return _buffer.getBufferStart(); }

   void clearForRead();
   void clearForWrite();

   void print();
protected:
   uint32_t _metaDataOffset;
   std::vector<uint32_t> _descriptorOffsets;
   MessageBuffer _buffer;
   };


class ServerMessage : public Message
   {
   };

class ClientMessage : public Message
   {
public:
   uint64_t version() { return getMetaData()->_version; }
   void setVersion(uint64_t version) { getMetaData()->_version = version; }
   void clearVersion() { getMetaData()->_version = 0; }
   };
};
#endif
