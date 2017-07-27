#ifndef RPC_PROTOBUF_TYPE_CONVERT_H
#define RPC_PROTOBUF_TYPE_CONVERT_H

#include <cstdint>
#include <utility>
#include <type_traits>
#include "types.h"

namespace JAAS
   {

   extern uint64_t typeCount;

   struct TypeID
      {
      uint64_t id;
      TypeID()
         {
         id = typeCount++;
         }
      };

   template <typename T, typename = void>
   struct ProtobufTypeConvert
      {
      using ProtoType = T;
      static T onRecv(ProtoType in) { return in; }
      static ProtoType onSend(T in) { return in; }
      };

   template <typename Primitive, typename Proto>
   struct PrimitiveTypeConvert
      {
      static TypeID type;
      using ProtoType = Proto;
      static Primitive onRecv(ProtoType in)
         {
         if (type.id != in.type())
            throw StreamTypeMismatch("Primitive type mismatch: " + std::to_string(type.id) + " != "  + std::to_string(in.type()));
         static_assert(sizeof(decltype(in.val())) == sizeof(Primitive), "Size of primitive types must be the same");
         return (Primitive)in.val();
         }
      static ProtoType onSend(Primitive in)
         {
         ProtoType val;
         static_assert(sizeof(decltype(val.val())) == sizeof(Primitive), "Size of primitive types must be the same");
         val.set_val((decltype(val.val()))in);
         val.set_type(type.id);
         return val;
         }
      };
   template <typename Primitive, typename Proto> TypeID PrimitiveTypeConvert<Primitive, Proto>::type;

   template <> struct ProtobufTypeConvert<bool> : PrimitiveTypeConvert<bool, Bool> { };
   template <> struct ProtobufTypeConvert<uint64_t> : PrimitiveTypeConvert<uint64_t, UInt64> { };
   template <> struct ProtobufTypeConvert<uint32_t> : PrimitiveTypeConvert<uint32_t, UInt32> { };
   template <> struct ProtobufTypeConvert<int32_t> : PrimitiveTypeConvert<int32_t, Int32> { };
   template <> struct ProtobufTypeConvert<std::string> : PrimitiveTypeConvert<std::string, Bytes> { };

   // Implement conversion for all enums
   template <typename T> struct ProtobufTypeConvert<T, typename std::enable_if<std::is_enum<T>::value>::type> : PrimitiveTypeConvert<T, Int32> { };

   template <typename T>
   struct ProtobufTypeConvert<T*> : PrimitiveTypeConvert<T*, UInt64>
      {
      static T* onRecv(UInt64 in)
         {
         UInt64 flipped(in);
         flipped.set_val(~flipped.val());
         return PrimitiveTypeConvert<T*, UInt64>::onRecv(flipped);
         }
      };

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
