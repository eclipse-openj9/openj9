#ifndef RPC_PROTOBUF_TYPE_CONVERT_H
#define RPC_PROTOBUF_TYPE_CONVERT_H

#include <cstdint>
#include <utility>
#include <type_traits>
#include "types.h"

namespace JAAS
   {

   // This struct and global is used as a hacky replacement for run-time type information (rtti), which is not enabled in our build.
   // Every instance of this struct gets a unique identifier assigned to it (assuming constructors run in serial)
   // It is inteneded to be used by holding a static instance of it within a templated class, so that each template instantiation
   // gets a different unique ID.
   extern uint64_t typeCount;
   struct TypeID
      {
      uint64_t id;
      TypeID()
         {
         id = typeCount++;
         }
      };

   // The ProtobufTypeConvert struct is intended to allow custom conversions to happen automatically before protobuf serialization
   // and after protobuf deserialization. We use specialization to implement it for specific types.
   //
   // The base implementation here simply does nothing to any types that have not been specialized.
   // It will be used if you pass a protobuf type through directly such as JAAS::Void.
   template <typename T, typename = void>
   struct ProtobufTypeConvert
      {
      static_assert(std::is_base_of<google::protobuf::Message, T>::value, "Fell back to base impl of ProtobufTypeConvert but not given protobuf type: did you forget to implement ProtobufTypeConvert for the type you are sending?");
      using ProtoType = T;
      static T onRecv(ProtoType in) { return in; }
      static ProtoType onSend(T in) { return in; }
      };

   // This struct is used as a base for conversions that can be done by a simple cast between data types of the same size.
   // It uses a type id mechanism to verify that the types match at runtime upon deserialization.
   template <typename Primitive, typename Proto>
   struct PrimitiveTypeConvert
      {
      static TypeID type;
      using ProtoType = Proto;
      static_assert(sizeof(decltype(std::declval<ProtoType>().val())) == sizeof(Primitive), "Size of primitive types must be the same");
      static Primitive onRecv(ProtoType in)
         {
         if (type.id != in.type())
            throw StreamTypeMismatch("Primitive type mismatch: " + std::to_string(type.id) + " != "  + std::to_string(in.type()));
         return (Primitive)in.val();
         }
      static ProtoType onSend(Primitive in)
         {
         ProtoType val;
         val.set_val((decltype(val.val()))in);
         val.set_type(type.id);
         return val;
         }
      };
   template <typename Primitive, typename Proto> TypeID PrimitiveTypeConvert<Primitive, Proto>::type;

   // Specialize ProtobufTypeConvert for various primitive types by inheriting from a specifically instantiated PrimitiveTypeConvert
   template <> struct ProtobufTypeConvert<bool> : PrimitiveTypeConvert<bool, Bool> { };
   template <> struct ProtobufTypeConvert<uint64_t> : PrimitiveTypeConvert<uint64_t, UInt64> { };
   template <> struct ProtobufTypeConvert<uint32_t> : PrimitiveTypeConvert<uint32_t, UInt32> { };
   template <> struct ProtobufTypeConvert<int32_t> : PrimitiveTypeConvert<int32_t, Int32> { };
   template <> struct ProtobufTypeConvert<std::string> : PrimitiveTypeConvert<std::string, Bytes> { };

   // Specialize conversion for all enums
   template <typename T> struct ProtobufTypeConvert<T, typename std::enable_if<std::is_enum<T>::value>::type> : PrimitiveTypeConvert<T, Int32> { };

   // Specialize conversion for pointer types
   // When recieving a pointer, we flip all of the bits as a signal to indicate that it should not be dereferenced.
   // This is intended to make violations of this rule easy to spot in a debugger.
   // The exception is if the value is null, in which case we do not flip the bits to prevent breaking null checks.
   template <typename T>
   struct ProtobufTypeConvert<T*> : PrimitiveTypeConvert<T*, UInt64>
      {
      static T* onRecv(UInt64 in)
         {
         UInt64 proto(in);
         if (proto.val())
            proto.set_val(~proto.val());
         return PrimitiveTypeConvert<T*, UInt64>::onRecv(proto);
         }
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
         message->add_data()->PackFrom(ProtobufTypeConvert<Arg1>::onSend(arg1));
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
      static std::tuple<Arg1, Args...> getArgs(AnyData *message, size_t n)
         {
         return std::tuple_cat(GetArgs<Arg1>::getArgs(message, n), GetArgs<Args...>::getArgs(message, n + 1));
         }
      };
   template <typename Arg>
   struct GetArgs<Arg>
      {
      static std::tuple<Arg> getArgs(AnyData *message, size_t n)
         {
         auto &data = message->data(n);
         using T = typename ProtobufTypeConvert<Arg>::ProtoType;
         if (data.Is<T>())
            {
            T msg;
            data.UnpackTo(static_cast<google::protobuf::Message*>(&msg));
            return std::make_tuple(ProtobufTypeConvert<Arg>::onRecv(msg));
            }
         else
            {
            throw StreamTypeMismatch("Expected " + data.type_url() + " for argument " + std::to_string(n));
            }
         }
      };
   template <typename... Args>
   std::tuple<Args...> getArgs(AnyData *message)
      {
      if (sizeof...(Args) != message->data_size())
         throw StreamArityMismatch("Expected " + std::to_string(message->data_size()) + " args to unpack but got " + std::to_string(sizeof...(Args)) + "-tuple");
      return GetArgs<Args...>::getArgs(message, 0);
      }

   }

#endif
