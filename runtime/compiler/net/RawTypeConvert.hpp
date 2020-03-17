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
#ifndef RAW_TYPE_CONVERT_H
#define RAW_TYPE_CONVERT_H

#include <cstdint>
#include <utility>
#include <type_traits>
#include "StreamExceptions.hpp"
#include "omrcomp.h"
#include "net/Message.hpp"

class J9Class;
class TR_ResolvedJ9Method;

namespace JITServer
{
// Sending void over the network is awkward, so we just
// wrap a bool.
struct Void
   {
   Void() {}
   Void(bool) {}
   operator bool() const { return false; }
   };

// The following are the template functions used to serialize/deserialize data.
// RawTypeConvert<T>::onRecv(Message::DataDescriptor *desc) - given a data descriptor,
// deserializes it into a value of type T and returns it.
//
// RawTypeConvert::onSend(Message &msg, const T&val) - given a value of type T,
// serializes it into an instance of Message::DataDescriptor and adds it to the message.
// Returns the number of data bytes written.
template <typename T, typename = void> struct RawTypeConvert { };
template <> struct RawTypeConvert<uint32_t>
   {
   static inline uint32_t onRecv(Message::DataDescriptor *desc) 
      {
      TR_ASSERT(desc->getDataType() == Message::DataDescriptor::DataType::UINT32, "onRecv type missmatch UINT32");
      TR_ASSERT(desc->getPayloadSize() == sizeof(uint32_t), "onRecv size missmatch");
      return *static_cast<uint32_t *>(desc->getDataStart()); 
      }
   static inline uint32_t onSend(Message &msg, const uint32_t &val)
      {
      return msg.addData(Message::DataDescriptor(Message::DataDescriptor::DataType::UINT32, sizeof(uint32_t)), &val);
      }
   };
template <> struct RawTypeConvert<uint64_t>
   {
   static inline uint64_t onRecv(Message::DataDescriptor *desc) 
      {
      TR_ASSERT(desc->getDataType() == Message::DataDescriptor::DataType::UINT64, "onRecv type missmatch UINT64");
      TR_ASSERT(desc->getPayloadSize() == sizeof(uint64_t), "onRecv size missmatch");
      return *static_cast<uint64_t *>(desc->getDataStart()); 
      }
   static inline uint32_t onSend(Message &msg, const uint64_t &val)
      {
      return msg.addData(Message::DataDescriptor(Message::DataDescriptor::DataType::UINT64, sizeof(uint64_t)), &val, true);
      }
   };
template <> struct RawTypeConvert<int32_t>
   {
   static inline int32_t onRecv(Message::DataDescriptor *desc) 
      {
      TR_ASSERT(desc->getDataType() == Message::DataDescriptor::DataType::INT32, "onRecv type missmatch INT32");
      TR_ASSERT(desc->getPayloadSize() == sizeof(int32_t), "onRecv size missmatch");
      return *static_cast<int32_t *>(desc->getDataStart()); 
      }
   static inline uint32_t onSend(Message &msg, const int32_t &val)
      {
      return msg.addData(Message::DataDescriptor(Message::DataDescriptor::DataType::INT32, sizeof(int32_t)), &val);
      }
   };
template <> struct RawTypeConvert<int64_t>
   {
   static inline int64_t onRecv(Message::DataDescriptor *desc) 
      {
      TR_ASSERT(desc->getDataType() == Message::DataDescriptor::DataType::INT64, "onRecv type missmatch INT64");
      TR_ASSERT(desc->getPayloadSize() == sizeof(int64_t), "onRecv size missmatch");
      return *static_cast<int64_t *>(desc->getDataStart()); 
      }
   static inline uint32_t onSend(Message &msg, const int64_t &val)
      {
      return msg.addData(Message::DataDescriptor(Message::DataDescriptor::DataType::INT64, sizeof(int64_t)), &val, true);
      }
   };
template <> struct RawTypeConvert<bool>
   {
   static inline bool onRecv(Message::DataDescriptor *desc) 
      {
      TR_ASSERT(desc->getDataType() == Message::DataDescriptor::DataType::BOOL, "onRecv type missmatch BOOL");
      TR_ASSERT(desc->getTotalSize() == 4 && desc->getPaddingSize() == 3, "onRecv size missmatch");
      return *static_cast<bool *>(desc->getDataStart()); 
      }
   static inline uint32_t onSend(Message &msg, const bool &val)
      {
      return msg.addData(Message::DataDescriptor(Message::DataDescriptor::DataType::BOOL, sizeof(bool)), &val);
      }
   };
template <> struct RawTypeConvert<const std::string>
   {
   static inline std::string onRecv(Message::DataDescriptor *desc)
      {
      TR_ASSERT(desc->getDataType() == Message::DataDescriptor::DataType::STRING, "onRecv type missmatch STRING");
      return std::string(static_cast<char *>(desc->getDataStart()), desc->getPayloadSize());
      }
   static inline uint32_t onSend(Message &msg, const std::string &value)
      {
      return msg.addData(Message::DataDescriptor(Message::DataDescriptor::DataType::STRING, value.length()), &value[0]);
      }
   };

template <> struct RawTypeConvert<std::string> : RawTypeConvert<const std::string> {};

// For trivially copyable classes
template <typename T> struct RawTypeConvert<T, typename std::enable_if<std::is_trivially_copyable<T>::value>::type>
   {
   static inline T onRecv(Message::DataDescriptor *desc) 
      {
      TR_ASSERT(desc->getDataType() == Message::DataDescriptor::DataType::OBJECT, "onRecv type missmatch OBJECT");
      return *static_cast<T *>(desc->getDataStart()); 
      }
   static inline uint32_t onSend(Message &msg, const T &value)
      {
      return msg.addData(Message::DataDescriptor(Message::DataDescriptor::DataType::OBJECT, sizeof(T)), &value, sizeof(T) > 4);
      }
   };

// For vectors
template <typename T> struct RawTypeConvert<T, typename std::enable_if<std::is_same<T, std::vector<typename T::value_type>>::value>::type>
   {
   static inline T onRecv(Message::DataDescriptor *desc)
      {
      if (desc->getDataType() == Message::DataDescriptor::DataType::EMPTY_VECTOR)
         {
         return std::vector<typename T::value_type>();
         }
      if (desc->getDataType() == Message::DataDescriptor::DataType::SIMPLE_VECTOR)
         {
         TR_ASSERT(desc->getVectorElementSize() == sizeof(typename T::value_type), "Vector element size missmatch");
         typename T::value_type *dataStart = static_cast<typename T::value_type *>(desc->getDataStart());
         typename T::value_type *dataEnd = reinterpret_cast<typename T::value_type *>(reinterpret_cast<uintptr_t>(dataStart) + desc->getPayloadSize());
         return T(dataStart, dataEnd);
         }

      TR_ASSERT(desc->getDataType() == Message::DataDescriptor::DataType::VECTOR, "onRecv type missmatch VECTOR");
      // The first element is another descriptor to represent the number of elements in the vector
      Message::DataDescriptor *sizeDesc = static_cast<Message::DataDescriptor *>(desc->getDataStart());
      uint32_t size = RawTypeConvert<uint32_t>::onRecv(sizeDesc);

      std::vector<typename T::value_type> values;
      values.reserve(size);

      // Find the descriptor of the first element in the array
      auto curDesc = sizeDesc->getNextDescriptor();
      for (uint32_t i = 0; i < size; ++i)
         {
         values.push_back(RawTypeConvert<typename T::value_type>::onRecv(curDesc));
         curDesc = curDesc->getNextDescriptor();
         }
      return values;
      }
   static inline uint32_t onSend(Message &msg, const T &value)
      {
      if (value.size() == 0)
         {
         // Empty vector. Write just a descriptor followed by no data
         return msg.addData(Message::DataDescriptor(Message::DataDescriptor::DataType::EMPTY_VECTOR, 0), 0);
         }
      // If the vector is for trivially copyable elements and
      // the size of such an element is smaller than 256,
      // copy the data as a contiguous region of memory
      if (std::is_trivially_copyable<typename T::value_type>::value)
         {
         size_t elemSize = sizeof(typename T::value_type);
         if (elemSize <= 0xff)
            {
            uint32_t payloadSize = elemSize * value.size();
            Message::DataDescriptor desc(Message::DataDescriptor::DataType::SIMPLE_VECTOR, payloadSize);
            // Need to store the size of an element in the descriptor
            desc.setVectorElementSize(elemSize);
            return msg.addData(desc, value.data(), elemSize > 4);
            }
         }
      // Complex vectors here
      uint32_t descIndex = msg.getNumDescriptors();
      uint32_t descOffset = msg.reserveDescriptor();

      // Write the size of the vector as a data point
      uint32_t totalSize = (value.size() + 1) * sizeof(Message::DataDescriptor);
      totalSize += RawTypeConvert<uint32_t>::onSend(msg, value.size());
      for (int32_t i = 0; i < value.size(); ++i)
         {
         // Serialize each element in a vector as its own datapoint
         totalSize += RawTypeConvert<typename T::value_type>::onSend(msg, value[i]);
         }

      // Only get pointer to the vector descriptor after all of its elements were serialized,
      // because buffer might have been reallocated
      Message::DataDescriptor *desc = msg.getDescriptor(descIndex);
      // If each element is 32-bit aligned, then the entire vector is 32-bit aligned and padding is 0
      desc->init(Message::DataDescriptor::DataType::VECTOR, totalSize, 0, 0);
      return totalSize;
      }
   };
// For tuples
//
// used to unpack a tuple into variadic args
template <size_t... I> struct index_tuple_raw { template <size_t N> using type = index_tuple_raw<I..., N>; };
template <size_t N> struct index_tuple_gen_raw { using type = typename index_tuple_gen_raw<N - 1>::type::template type<N - 1>; };
template <> struct index_tuple_gen_raw<0> { using type = index_tuple_raw<>; };

template <size_t n, typename Arg1, typename... Args>
struct TupleTypeConvert
   {
   static inline std::tuple<Arg1, Args...> onRecvImpl(Message::DataDescriptor *desc)
      {
      return std::tuple_cat(TupleTypeConvert<n, Arg1>::onRecvImpl(desc),
                            TupleTypeConvert<n + 1, Args...>::onRecvImpl(desc->getNextDescriptor()));
      }
   static inline uint32_t onSendImpl(Message &msg, const Arg1 &arg1, const Args&... args)
      {
      uint32_t totalSize = TupleTypeConvert<n, Arg1>::onSendImpl(msg, arg1);
      totalSize += TupleTypeConvert<n + 1, Args...>::onSendImpl(msg, args...);
      return totalSize;
      }
   };

template <size_t n, typename Arg1>
struct TupleTypeConvert<n, Arg1>
   {
   static inline std::tuple<Arg1> onRecvImpl(Message::DataDescriptor *desc)
      {
      return std::make_tuple(RawTypeConvert<Arg1>::onRecv(desc));
      }
   static inline uint32_t onSendImpl(Message &msg, const Arg1 &arg1)
      {
      return RawTypeConvert<Arg1>::onSend(msg, arg1) + sizeof(Message::DataDescriptor);
      }
   };

template <typename... T> struct RawTypeConvert<const std::tuple<T...>>
   {
   static inline std::tuple<T...> onRecv(Message::DataDescriptor *desc)
      {
      TR_ASSERT(desc->getDataType() == Message::DataDescriptor::DataType::TUPLE, "onRecv type missmatch TUPLE");
      return TupleTypeConvert<0, T...>::onRecvImpl(static_cast<Message::DataDescriptor *>(desc->getDataStart()));
      }

   template <typename Tuple, size_t... Idx>
   static inline uint32_t onSendImpl(Message &msg, const Tuple &val, index_tuple_raw<Idx...>)
      {
      uint32_t descIndex = msg.getNumDescriptors();
      uint32_t descOffset = msg.reserveDescriptor();
      uint32_t totalSize = TupleTypeConvert<0, T...>::onSendImpl(msg, std::get<Idx>(val)...);
      // Only get pointer to the tuple descriptor after all of its elements were serialized,
      // because buffer might have been reallocated
      Message::DataDescriptor *desc = msg.getDescriptor(descIndex);
      // If each element is 32-bit aligned, then the entire tuple is 32-bit aligned and padding is 0
      desc->init(Message::DataDescriptor::DataType::TUPLE, totalSize, 0, 0);
      return totalSize;
      }
   
   static inline uint32_t onSend(Message &msg, const std::tuple<T...> &val)
      {
      using Idx = typename index_tuple_gen_raw<sizeof...(T)>::type;
      return onSendImpl(msg, val, Idx());
      }
   };

template <typename... T> struct RawTypeConvert<std::tuple<T...>> : RawTypeConvert<const std::tuple<T...>> { };

// setArgs fills out a message with values from a variadic argument list.
// calls RawTypeConvert::onSend for each argument.
template <typename Arg1, typename... Args>
struct SetArgsRaw
   {
   static void setArgs(Message &message, Arg1 &arg1, Args&... args)
      {
      SetArgsRaw<Arg1>::setArgs(message, arg1);
      SetArgsRaw<Args...>::setArgs(message, args...);
      }
   };
template <typename Arg1>
struct SetArgsRaw<Arg1>
   {
   static void setArgs(Message &message, Arg1 &arg1)
      {
      RawTypeConvert<Arg1>::onSend(message, arg1);
      }
   };
template <typename... Args>
void setArgsRaw(Message &message, Args&... args)
   {
   message.getMetaData()->_numDataPoints = sizeof...(args);
   SetArgsRaw<Args...>::setArgs(message, args...);
   }

template <typename Arg1, typename... Args>
struct GetArgsRaw
   {
   static std::tuple<Arg1, Args...> getArgs(const Message &message, size_t n)
      {
      return std::tuple_cat(GetArgsRaw<Arg1>::getArgs(message, n), GetArgsRaw<Args...>::getArgs(message, n + 1));
      }
   };
template <typename Arg>
struct GetArgsRaw<Arg>
   {
   static std::tuple<Arg> getArgs(const Message &message, size_t n)
      {
      Message::DataDescriptor *desc = message.getDescriptor(n);
      return std::make_tuple(RawTypeConvert<Arg>::onRecv(desc));
      }
   };
template <typename... Args>
std::tuple<Args...> getArgsRaw(const Message &message)
   {
   if (sizeof...(Args) != message.getMetaData()->_numDataPoints)
      throw StreamArityMismatch("Received " + std::to_string(message.getMetaData()->_numDataPoints) + " args to unpack but expect " + std::to_string(sizeof...(Args)) + "-tuple");
   return GetArgsRaw<Args...>::getArgs(message, 0);
   }
};
#endif
