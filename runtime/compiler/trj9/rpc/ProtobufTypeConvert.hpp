#ifndef RPC_PROTOBUF_TYPE_CONVERT_H
#define RPC_PROTOBUF_TYPE_CONVERT_H

#include <cstdint>
#include <utility>
#include "types.h"

namespace JAAS
   {

   template <typename T>
   struct ProtobufTypeConvert
      {
      using ProtoType = T;
      static T onRecv(ProtoType in) { return in; }
      static ProtoType onSend(T in) { return in; }
      };

   template <typename Primitive, typename Proto>
   struct PrimitiveTypeConvert
      {
      using ProtoType = Proto;
      static Primitive onRecv(ProtoType in) { return in.val(); }
      static ProtoType onSend(Primitive in)
         {
         ProtoType val;
         val.set_val(in);
         return val;
         }
      };
   template <> struct ProtobufTypeConvert<bool> : PrimitiveTypeConvert<bool, Bool> { };
   template <> struct ProtobufTypeConvert<uint64_t> : PrimitiveTypeConvert<uint64_t, UInt64> { };
   template <> struct ProtobufTypeConvert<uint32_t> : PrimitiveTypeConvert<uint32_t, UInt32> { };
   template <> struct ProtobufTypeConvert<std::string> : PrimitiveTypeConvert<std::string, Bytes> { };

   template <typename T>
   struct ProtobufTypeConvert<T*> 
      {
      using ProtoType = Pointer;
      static T* onRecv(ProtoType in) { return (T*) ~in.val(); }
      static ProtoType onSend(T* in)
         {
         ProtoType val;
         val.set_val((uint64_t) in);
         return val;
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
