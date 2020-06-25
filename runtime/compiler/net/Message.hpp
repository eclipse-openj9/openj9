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
#include "OMR/Bytes.hpp" // for alignNoCheck

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

      It is assumed that the MetaData immediately follows the messages size
      which is encoded as a uint32_t
   */
   struct MetaData
      {
      MetaData() :
         _version(0), _config(0), _type(MessageType_MAXTYPE), _numDataPoints(0)
         {}
      uint32_t _version;
      uint32_t _config; // includes JITServerCompatibilityFlags which must match
      MessageType _type;
      uint16_t _numDataPoints;

      void init()
         {
         _version = 0;
         _config = 0;
         _type = MessageType_MAXTYPE;
         _numDataPoints = 0;
         }
      };

   /**
   @brief Utility function that builds the "full version" of client/server as 
   a composition of the version number and compatibility flags.
   */
   static uint64_t buildFullVersion(uint32_t version, uint32_t config) 
      { 
      return (((uint64_t)config) << 32) | version;
      }

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
         OBJECT, // only trivially-copyable; pointers fall into this category
         ENUM,
         VECTOR,
         SIMPLE_VECTOR, // vector whose elements are trivially-copyable and less than 256 bytes
         EMPTY_VECTOR,
         TUPLE,
         LAST_TYPE
         };
      static const char* const _descriptorNames[];
      /**
         @brief Constructor

         @param type The type of the data described by the descriptor
         @param payloadSize Size of the real data (excluding any required padding)

      */
      DataDescriptor(DataType type, uint32_t payloadSize) : _type(type), _dataOffset(0), _vectorElementSize(0)
         {
         // Round the _size up to a 4-byte multiple to ensure that the 
         // descriptor coming after this data point is 4-byte aligned
         _size = OMR::alignNoCheck(payloadSize, sizeof(uint32_t));
         _paddingSize = static_cast<uint8_t>(_size - payloadSize);    
         }
      DataType getDataType() const { return _type; }
      uint32_t getPayloadSize() const { return _size - _paddingSize - _dataOffset; }
      uint32_t getTotalSize() const { return _size; }
      uint8_t getPaddingSize() const { return _paddingSize; }
      uint8_t getDataOffset() const { return _dataOffset; }
      uint8_t getVectorElementSize() const { return _vectorElementSize; }
      void setVectorElementSize(uint8_t sz) { _vectorElementSize = sz; }

      /**
         @brief Initialize descriptor with values give as parameters.

         @param type   The type of data described by descriptor
         @param totalSize   Total size of data following the descriptor
         @param paddingSize   Size of padding added after the real payload (included in totalSize)
         @param dataOffset   Distance from end of descriptor to start of real payload (included in totalSize)
      */
      void init(DataType type, uint32_t totalSize, uint8_t paddingSize, uint8_t dataOffset)
         {
         _type = type;
         _paddingSize = paddingSize;
         _dataOffset = dataOffset;
         _vectorElementSize = 0;
         _size = totalSize;
         }

      /**
         @brief Adjust an existing descriptor by adding some offset to real payload

         @param initialPadding   Distance from end of descriptor to start of real payload (included in totalSize)
      */
      void addInitialPadding(uint8_t initialPadding)
         {
         TR_ASSERT(_dataOffset == 0, "Initial padding added twice");
         TR_ASSERT(initialPadding == 4, "Current implementation assumes only 4 bytes of padding"); // because we add 4 bytes to align on a 8-byte boundary
         _dataOffset = initialPadding;
         _size += initialPadding; // Total size increases as well
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

      /**
          @brief Get a pointer to the beginning of the next descriptor in the MessageBuffer.

          @return Returns a pointer to the following descriptor in the MessageBuffer
      */
      DataDescriptor* getNextDescriptor()
         {
         return static_cast<DataDescriptor*>(static_cast<void*>(static_cast<char*>(static_cast<void*>(this + 1)) + _size));
         }

      uint32_t print(uint32_t nestingLevel);
      private:
      DataType _type; // Message type on 8 bits
      uint8_t  _paddingSize; // How many bytes are actually used for padding (0-3)
      uint8_t  _dataOffset; // Offset from DataDescriptor to actual data
      uint8_t  _vectorElementSize; // Size of an element for SIMPLE_VECTORs
      uint32_t _size; // Size of the data segment, which can include nested data
      }; // struct DataDescriptor

   Message()
      {
      // Reserve space for encoding the size and MetaData.
      // These will be populated at a later time
      _buffer.reserveValue<uint32_t>(); // Reserve space for encoding the size
      _buffer.reserveValue<Message::MetaData>();

      getMetaData()->init();
      }

   MetaData *getMetaData() const
      {
      return _buffer.getValueAtOffset<MetaData>(sizeof(uint32_t)); // sizeof(uint32_t) represents the space for message size
      }

   /**
      @brief Add a new data point to the message.

      Writes the descriptor and attached data to the MessageBuffer
      and updates the message structure. If the attached data is not
      aligned on a 32-bit boundary, some padding will be written as well.

      @param desc Descriptor for the new data
      @param dataStart Pointer to the new data
      @param needs64BitAlignment Whether data following the descriptor needs to be 64-bit aligned

      @return The total amount of data written (including padding, but not including the descriptor)
   */
   uint32_t addData(const DataDescriptor &desc, const void *dataStart, bool needs64BitAlignment = false);

   /**
      @brief Allocate space for a descriptor in the MessageBuffer
      and update the message structure without writing any actual data.

      This method is needed for some specific cases, mainly for writing
      non-primitive data points.

      @return The offset to the descriptor reserved
   */
   uint32_t reserveDescriptor()
      {
      uint32_t descOffset = _buffer.reserveValue<DataDescriptor>();
      _descriptorOffsets.push_back(descOffset);
      return descOffset;
      }

   /**
      @brief Get a pointer to the descriptor at given index

      @param idx Index of the descriptor

      @return A pointer to the descriptor
   */
   DataDescriptor *getDescriptor(size_t idx) const
      {
      TR_ASSERT(idx < _descriptorOffsets.size(), "Out-of-bounds access for _descriptorOffsets");
      uint32_t offset = _descriptorOffsets[idx];
      return _buffer.getValueAtOffset<DataDescriptor>(offset);
      }

   /**
      @brief Get a pointer to the last descriptor added to the message.
   */
   DataDescriptor *getLastDescriptor() const
      {
      return _buffer.getValueAtOffset<DataDescriptor>(_descriptorOffsets.back());
      }

   /**
      @brief Get the total number of descriptors in the message.
   */
   uint32_t getNumDescriptors() const { return _descriptorOffsets.size(); }

   /**
      @brief Set message type

      @param type The new message type
   */
   void setType(MessageType type) { getMetaData()->_type = type; }

   /**
      @brief Get message type
   */
   MessageType type() const { return getMetaData()->_type; }

   /**
      @brief Serialize the message
      
      Write total message size to the beginning of message buffer and return pointer to it.

      @return A pointer to the beginning of the serialized message
   */
   char *serialize()
      {
      *_buffer.getValueAtOffset<uint32_t>(0) = _buffer.size();
      return _buffer.getBufferStart();
      }

   /**
      @brief Return the size of the serialized message.
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
   void setSerializedSize(uint32_t serializedSize)
      {
      _buffer.writeValue(serializedSize);
      }

   /**
      @brief Expand the internal buffer when requiredSize is greater than the capacity

      If expansion occurred, this method copies the number of bytes from the beginning
      of the buffer to _curPtr from the old buffer to the new buffer.
   */
   void expandBufferIfNeeded(uint32_t requiredSize)
      {
      _buffer.expandIfNeeded(requiredSize);
      }

   /**
      @brief Expand the internal buffer to requiredSize and copy numBytesToCopy
      from the old buffer into the new one
   */
   void expandBuffer(uint32_t requiredSize, uint32_t numBytesToCopy)
      {
      _buffer.expand(requiredSize, numBytesToCopy);
      }

   uint32_t getBufferCapacity() const { return _buffer.getCapacity(); }

   /**
      @brief Get the pointer to the start of the buffer.
      
      This method should be used only to read an incoming message.
      
      @return A pointer to the beginning of the MessageBuffer
   */
   char *getBufferStartForRead() { return _buffer.getBufferStart(); }

   void clearForRead()
      {
      _descriptorOffsets.clear();
      _buffer.clear();
      }

   void clearForWrite()
      {
      _descriptorOffsets.clear();
      _buffer.clear();
      _buffer.reserveValue<uint32_t>(); // For writing the size
      _buffer.reserveValue<MetaData>(); // For writing the metadata
      }

   void print();
protected:
   std::vector<uint32_t> _descriptorOffsets;
   MessageBuffer _buffer; // Buffer used for send/receive operations
   };


class ServerMessage : public Message
   {
   };

class ClientMessage : public Message
   {
public:
   uint64_t fullVersion() 
      {
      const MetaData* metaData = getMetaData();
      return buildFullVersion(metaData->_version, metaData->_config);
      }
   void setFullVersion(uint32_t version, uint32_t config) 
      {
      MetaData *metaData = getMetaData();
      metaData->_version = version;
      metaData->_config = config;
      }
   void clearFullVersion() 
      { 
      MetaData *metaData = getMetaData();
      metaData->_version = 0; 
      metaData->_config = 0;
      }
   };
};
#endif
