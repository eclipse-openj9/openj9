/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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

#ifndef RPC_PROTOBUF_TYPE_CONVERT_H
#define RPC_PROTOBUF_TYPE_CONVERT_H

#include <cstdint>
#include <utility>
#include <type_traits>
#include "StreamExceptions.hpp"

namespace JITServer
   {
   template <typename... Args>
   std::tuple<Args...> getArgs(const AnyData *message);
   template <typename... Args>
   void setArgs(AnyData *message, Args... args);

   // Sending void over protobuf is awkward, so we just wrap a bool...
   struct Void {
      Void() {}
      Void(bool) {}
      operator bool() const { return false; }
   };

   // This struct and global is used as a replacement for run-time type information (rtti), 
   // which is not supported in our build.
   // Every instance of this struct gets a unique ID assigned to it (assuming constructors run serially)
   // It is intended to be used by holding a static instance of it within a templated class, 
   // so that each template instantiation gets a different unique ID.
   struct TypeID
      {
      const uint64_t id;
      TypeID():id(typeCount++) {}
      private:
         static uint64_t typeCount;
      };

   // used to unpack a tuple into variadic args
   template <size_t... I> struct index_tuple { template <size_t N> using type = index_tuple<I..., N>; };
   template <size_t N> struct index_tuple_gen { using type = typename index_tuple_gen<N - 1>::type::template type<N - 1>; };
   template <> struct index_tuple_gen<0> { using type = index_tuple<>; };

   // handles (de)serialization of "primitive" types, i.e. those that do not require more advanced pre/post processing
   // this includes: int, bool, string, trivially copyable types, vector<T>, and tuple<T...> where T is primitive
   template <typename T, typename = void> struct AnyPrimitive { };
   template <> struct AnyPrimitive<uint32_t>
      {
      static inline uint32_t read(const Any *msg) { return msg->uint32_v(); }
      static inline void write(Any* msg, const uint32_t val) { msg->set_uint32_v(val); }
      static inline Any::TypeCase typeCase() { return Any::kUint32V; }
      };
   template <> struct AnyPrimitive<uint64_t>
      {
      static inline uint64_t read(const Any *msg) { return msg->uint64_v(); }
      static inline void write(Any* msg, const uint64_t val) { msg->set_uint64_v(val); }
      static inline Any::TypeCase typeCase() { return Any::kUint64V; }
      };
   template <> struct AnyPrimitive<int32_t>
      {
      static inline int32_t read(const Any *msg) { return msg->int32_v(); }
      static inline void write(Any* msg, const int32_t val) { msg->set_int32_v(val); }
      static inline Any::TypeCase typeCase() { return Any::kInt32V; }
      };
   template <> struct AnyPrimitive<int64_t>
      {
      static inline int64_t read(const Any *msg) { return msg->int64_v(); }
      static inline void write(Any* msg, const int64_t val) { msg->set_int64_v(val); }
      static inline Any::TypeCase typeCase() { return Any::kInt64V; }
      };
   template <> struct AnyPrimitive<bool>
      {
      static inline bool read(const Any *msg) { return msg->bool_v(); }
      static inline void write(Any* msg, const bool val) { msg->set_bool_v(val); }
      static inline Any::TypeCase typeCase() { return Any::kBoolV; }
      };
   template <> struct AnyPrimitive<const std::string>
      {
      static inline const std::string& read(const Any *msg) { return msg->bytes_v(); }
      static inline void write(Any* msg, const std::string &val) { msg->set_bytes_v(val); }
      static inline Any::TypeCase typeCase() { return Any::kBytesV; }
      };
   // trivially copyable
   template <typename T> struct AnyPrimitive<T, typename std::enable_if<std::is_trivially_copyable<T>::value>::type>
      {
      static inline T read(const Any *msg)
         {
         char data[sizeof(T)];
         const std::string &strVal = msg->bytes_v();
         memcpy(data, strVal.data(), sizeof(T));
         T &val = (T&) data;
         return val;
         }
      static inline void write(Any *msg, const T &val)
         {
         std::string strVal((const char *) &val, sizeof(T));
         msg->set_bytes_v(strVal);
         }
      static inline Any::TypeCase typeCase() { return Any::kBytesV; }
      };
   // vector
   template <typename T> struct AnyPrimitive<T, typename std::enable_if<std::is_same<T, std::vector<typename T::value_type>>::value>::type>
      {
      static inline T read(const Any *msg)
         {
         std::vector<typename T::value_type> out;
         auto &items = msg->vector_v();
         for (size_t i = 0; i < items.data_size(); i++)
            {
            auto item = items.data(i);
            out.push_back(AnyPrimitive<const typename T::value_type>::read(&item));
            }
         return out;
         }
      static inline void write(Any *msg, const T &val)
         {
         auto data = msg->mutable_vector_v();
         for (const typename T::value_type& item : val)
            {
            AnyPrimitive<const typename T::value_type>::write(data->add_data(), item);
            }
         }
      static inline Any::TypeCase typeCase() { return Any::kVectorV; }
      };
   // tuple
   // Using getArgs and setArgs here is perhaps a bit heavyweight, but it does allow non-primitives to be sent,
   // which we can't do with vectors right now.
   // (should be trivial to add support for though, just forward declare and use ProtoBufTypeConvert)
   template <typename... T> struct AnyPrimitive<const std::tuple<T...>>
      {
      static inline std::tuple<T...> read(const Any *msg)
         {
         auto &items = msg->vector_v();
         return getArgs<T...>(&items);
         }
      template <typename Tuple, size_t... Idx>
      static inline void write_impl(Any *msg, const Tuple &val, index_tuple<Idx...>)
         {
         auto data = msg->mutable_vector_v();
         setArgs(data, std::get<Idx>(val)...);
         }
      static inline void write(Any *msg, const std::tuple<T...> &val)
         {
         using Idx = typename index_tuple_gen<sizeof...(T)>::type;
         write_impl(msg, val, Idx());
         }
      static inline Any::TypeCase typeCase() { return Any::kVectorV; }
      };

   // This struct is used as a base for conversions that can be done by a simple cast between data types of the same size.
   // It uses a type id mechanism to verify that the types match at runtime upon deserialization.
   template <typename Primitive, typename Proto=Primitive>
   struct PrimitiveTypeConvert
      {
      static TypeID type;
      using ProtoType = Proto;
      static Primitive onRecv(const Any *in)
         {
         if (type.id != in->extendedtypetag())
            throw StreamTypeMismatch("Primitive type mismatch: " + std::to_string(type.id) + " != "  + std::to_string(in->extendedtypetag()));
         const Proto &out = AnyPrimitive<Proto>::read(in);
         return (Primitive) out;
         }
      static void onSend(Any *out, const Primitive &in)
         {
         out->set_extendedtypetag(type.id);
         AnyPrimitive<Proto>::write(out, (Proto) in);
         }
      };
   template <typename Primitive, typename Proto> TypeID PrimitiveTypeConvert<Primitive, Proto>::type;

   // ProtobufTypeConvert is the midsection between primitives and get/setArgs
   // It decides how to map some non-primitive types down onto primitives.
   template <typename T, typename = void> struct ProtobufTypeConvert : PrimitiveTypeConvert<T>
      {
      static_assert(std::is_trivially_copyable<T>::value, "Fell back to base impl of ProtobufTypeConvert but not given trivially copyable type - your type is not supported");
      };

   // Specialize for directly sending protobuf messages
   template <typename T> struct ProtobufTypeConvert<T, typename std::enable_if<std::is_base_of<google::protobuf::Message, T>::value>::type>
      {
      using ProtoType = T;
      static inline T &onRecv(const ProtoType &in) { return in; }
      static inline ProtoType &onSend(const T &in) { return in; }
      };

   // Specialize ProtobufTypeConvert for various primitive types by inheriting from a specifically instantiated PrimitiveTypeConvert
   template <> struct ProtobufTypeConvert<bool> : PrimitiveTypeConvert<bool> { };
   template <> struct ProtobufTypeConvert<uint64_t> : PrimitiveTypeConvert<uint64_t> { };
   template <> struct ProtobufTypeConvert<int64_t> : PrimitiveTypeConvert<int64_t> { };
   template <> struct ProtobufTypeConvert<uint32_t> : PrimitiveTypeConvert<uint32_t> { };
   template <> struct ProtobufTypeConvert<int32_t> : PrimitiveTypeConvert<int32_t> { };
   template <> struct ProtobufTypeConvert<std::string> : PrimitiveTypeConvert<const std::string> { };
   template <> struct ProtobufTypeConvert<Void> : PrimitiveTypeConvert<Void, bool> { };
   template <typename... T> struct ProtobufTypeConvert<std::tuple<T...>> : PrimitiveTypeConvert<const std::tuple<T...>> { };

   // Specialize conversion for all enums by widening to 64 bits
   template <typename T> struct ProtobufTypeConvert<T, typename std::enable_if<std::is_enum<T>::value>::type> : PrimitiveTypeConvert<T, uint64_t>
      {
      static_assert(sizeof(T) <= 8, "Enum is larger than 8 bytes!");
      };

   // Specialize conversion for pointer types
   template <typename T> struct ProtobufTypeConvert<T, typename std::enable_if<std::is_pointer<T>::value>::type> : PrimitiveTypeConvert<T, uint64_t> { };

   // Specialize conversion for std::vector
   template <typename T> struct ProtobufTypeConvert<std::vector<T>> : PrimitiveTypeConvert<std::vector<T>>
      {
      static_assert(!std::is_same<T, bool>::value, "ProtobufTypeConvert for vector of bools (non-contiguous in standard)");
      };


   // setArgs fills out a protobuf AnyData message with values from a variadic argument list.
   // It calls ProtobufTypeConvert::onSend for each argument.
   template <typename Arg1, typename... Args>
   struct SetArgs
      {
      static void setArgs(AnyData *message, Arg1 arg1, Args... args)
         {
         SetArgs<Arg1>::setArgs(message, arg1);
         SetArgs<Args...>::setArgs(message, args...);
         }
      };
   template <typename Arg1>
   struct SetArgs<Arg1>
      {
      static void setArgs(AnyData *message, Arg1 arg1)
         {
         ProtobufTypeConvert<Arg1>::onSend(message->add_data(), arg1);
         }
      };
   template <typename... Args>
   void setArgs(AnyData *message, Args... args)
      {
      message->clear_data();
      SetArgs<Args...>::setArgs(message, args...);
      }

   // getArgs returns a tuple of values which are extracted from a protobuf AnyData message.
   // It calls ProtobufTypeConvert::onRecv for each value extracted.
   template <typename Arg1, typename... Args>
   struct GetArgs
      {
      static std::tuple<Arg1, Args...> getArgs(const AnyData *message, size_t n)
         {
         return std::tuple_cat(GetArgs<Arg1>::getArgs(message, n), GetArgs<Args...>::getArgs(message, n + 1));
         }
      };
   template <typename Arg>
   struct GetArgs<Arg>
      {
      static std::tuple<Arg> getArgs(const AnyData *message, size_t n)
         {
         auto data = message->data(n);
         if (data.type_case() != AnyPrimitive<typename ProtobufTypeConvert<Arg>::ProtoType>::typeCase())
            throw StreamTypeMismatch("Received type " + std::to_string(data.type_case()) + " but expect type " + std::to_string(AnyPrimitive<typename ProtobufTypeConvert<Arg>::ProtoType>::typeCase()));
         return std::make_tuple(ProtobufTypeConvert<Arg>::onRecv(&data));
         }
      };
   template <typename... Args>
   std::tuple<Args...> getArgs(const AnyData *message)
      {
      if (sizeof...(Args) != message->data_size())
         throw StreamArityMismatch("Received " + std::to_string(message->data_size()) + " args to unpack but expect " + std::to_string(sizeof...(Args)) + "-tuple");
      return GetArgs<Args...>::getArgs(message, 0);
      }

   }

#endif
