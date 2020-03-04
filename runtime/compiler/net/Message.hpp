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
      @brief Metadata containing the type and size of a single value in a message

      A DataDescriptor needs to be aligned on a 32-bit boundary. Because of this
      requirement, the data following the descriptor needs to be padded to a 
      32-bit boundary
   */
   struct DataDescriptor
      {
      // Data types that can be sent in a message
      // Adding new types is possible, as long as
      // serialization/deserialization functions
      // are added to RawTypeConvert
      enum DataType : uint8_t
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

      /**
         @brief Constructor

         @param type The type of the data described by the descriptor
         @param payloadSize Size of the real data (excluding any required padding)

      */
      DataDescriptor(DataType type, uint32_t payloadSize) : _type(type), _dataOffset(0), _reserved(0xff)
         {
         // align on 4 byte boundary
         _size = (payloadSize + 3) & 0xfffffffc;
         _paddingSize = static_cast<uint8_t>(_size - payloadSize);    
         }
      DataType getDataType() const { return _type; }
      uint32_t getPayloadSize() const { return _size - _paddingSize; }
      uint32_t getTotalSize() const { return _size; }
      uint8_t getPaddingSize() const { return _paddingSize; }
      uint8_t getDataOffset() const { return _dataOffset; }
      void init(DataType type, uint32_t totalSize, uint8_t paddingSize, uint8_t dataOffset)
         {
         _type = type;
         _paddingSize = paddingSize;
         _dataOffset = dataOffset;
         _reserved = 0xff;
         _size = totalSize;
         }

      /**
         @brief Tells if data attached to this descriptor is a type that
         can be directly copied from its address into the contiguous buffer.

         Currently, vectors and tuples are the only non-primitive supported types.
         Note: while vectors of primitive types could be copied directly if you copy
         from &v[0], it will not work if it's a vector of vectors, or a vector of tuples.

         @return Whether the data descriptor is primitive
      */
      bool isPrimitive() const { return _type != VECTOR && _type != TUPLE; }

      /**
         @brief Get the pointer to the beginning of data attached to this descriptor.

         This assumes that the data is located after the descriptor at some
         some offset, no more than 255 bytes away (typically such offset is 0).
         This allows us to align data on a natural boundary.

         @return A pointer to the beginning of the data
      */
      void *getDataStart()
         {
         return static_cast<void *>(static_cast<char*>(static_cast<void*>(this + 1)) + _dataOffset);
         }

      void print();
      private:
      DataType _type; // Message type on 8 bits
      uint8_t  _paddingSize; // How many bytes are actually used for padding (0-3)
      uint8_t  _dataOffset; // Offset from DataDescriptor to actual data. Always 0 for now.
      uint8_t  _reserved; // For future use
      uint32_t _size; // Size of the data segment, which can include nested data
      }; // struct DataDescriptor

   Message();

   MetaData *getMetaData() const;

   /**
      @brief Add a new data point to the message.

      Writes the descriptor and attached data to the MessageBuffer
      and updates the message structure. If the attached data is not
      aligned on a 32-bit boundary, some padding will be written as well.

      @param desc Descriptor for the new data
      @param dataStart Pointer to the new data

      @return The total amount of data written (including padding, but not including the descriptor)
   */
   uint32_t addData(const DataDescriptor &desc, const void *dataStart);

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
