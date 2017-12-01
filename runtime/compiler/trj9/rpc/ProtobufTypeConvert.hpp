#ifndef RPC_PROTOBUF_TYPE_CONVERT_H
#define RPC_PROTOBUF_TYPE_CONVERT_H

#include <cstdint>
#include <utility>
#include <type_traits>
#include "types.h"

namespace JAAS
   {

   // Sending void over gRPC is awkward, so we just wrap a bool...
   struct Void {
      Void() {}
      Void(bool) {}
      operator bool() { return false; }
   };

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

   template <typename T> inline T readPrimitiveFromAny(Any* msg);
   template <> inline uint32_t readPrimitiveFromAny(Any *msg) { return msg->uint32_v(); }
   template <> inline uint64_t readPrimitiveFromAny(Any *msg) { return msg->uint64_v(); }
   template <> inline int32_t readPrimitiveFromAny(Any *msg) { return msg->int32_v(); }
   template <> inline int64_t readPrimitiveFromAny(Any *msg) { return msg->int64_v(); }
   template <> inline bool readPrimitiveFromAny(Any *msg) { return msg->bool_v(); }
   template <> inline std::string readPrimitiveFromAny(Any *msg) { return msg->bytes_v(); }

   inline void writePrimitiveToAny(Any* msg, uint64_t val) { msg->set_uint64_v(val); }
   inline void writePrimitiveToAny(Any* msg, uint32_t val) { msg->set_uint32_v(val); }
   inline void writePrimitiveToAny(Any* msg, int64_t val) { msg->set_int64_v(val); }
   inline void writePrimitiveToAny(Any* msg, int32_t val) { msg->set_int32_v(val); }
   inline void writePrimitiveToAny(Any* msg, std::string val) { msg->set_bytes_v(val); }
   inline void writePrimitiveToAny(Any* msg, bool val) { msg->set_bool_v(val); }

   template <typename T> inline Any::TypeCase primitiveTypeCase();
   template <> inline Any::TypeCase primitiveTypeCase<uint64_t>() { return Any::kUint64V; }
   template <> inline Any::TypeCase primitiveTypeCase<uint32_t>() { return Any::kUint32V; }
   template <> inline Any::TypeCase primitiveTypeCase<int64_t>() { return Any::kInt64V; }
   template <> inline Any::TypeCase primitiveTypeCase<int32_t>() { return Any::kInt32V; }
   template <> inline Any::TypeCase primitiveTypeCase<bool>() { return Any::kBoolV; }
   template <> inline Any::TypeCase primitiveTypeCase<std::string>() { return Any::kBytesV; }

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
   template <typename Primitive, typename Proto=Primitive>
   struct PrimitiveTypeConvert
      {
      static TypeID type;
      using ProtoType = Proto;
      static Primitive onRecv(Any *in)
         {
         if (type.id != in->extendedtypetag())
            throw StreamTypeMismatch("Primitive type mismatch: " + std::to_string(type.id) + " != "  + std::to_string(in->extendedtypetag()));
         return (Primitive) readPrimitiveFromAny<Proto>(in);
         }
      static void onSend(Any *out, Primitive in)
         {
         out->set_extendedtypetag(type.id);
         writePrimitiveToAny(out, (Proto) in);
         }
      };
   template <typename Primitive, typename Proto> TypeID PrimitiveTypeConvert<Primitive, Proto>::type;

   // Specialize ProtobufTypeConvert for various primitive types by inheriting from a specifically instantiated PrimitiveTypeConvert
   template <> struct ProtobufTypeConvert<bool> : PrimitiveTypeConvert<bool> { };
   template <> struct ProtobufTypeConvert<uint64_t> : PrimitiveTypeConvert<uint64_t> { };
   template <> struct ProtobufTypeConvert<int64_t> : PrimitiveTypeConvert<int64_t> { };
   template <> struct ProtobufTypeConvert<uint32_t> : PrimitiveTypeConvert<uint32_t> { };
   template <> struct ProtobufTypeConvert<int32_t> : PrimitiveTypeConvert<int32_t> { };
   template <> struct ProtobufTypeConvert<std::string> : PrimitiveTypeConvert<std::string> { };
   template <> struct ProtobufTypeConvert<Void> : PrimitiveTypeConvert<Void, bool> { };

   // Specialize conversion for all enums by widening to 64 bits
   template <typename T> struct ProtobufTypeConvert<T, typename std::enable_if<std::is_enum<T>::value>::type>
      {
      static TypeID type;
      using ProtoType = uint64_t;
      static_assert(sizeof(T) <= 8, "Enum is larger than 8 bytes!");
      static T onRecv(Any *in)
         {
         if (type.id != in->extendedtypetag())
            throw StreamTypeMismatch("Enum type mismatch: " + std::to_string(type.id) + " != "  + std::to_string(in->extendedtypetag()));
         return (T) readPrimitiveFromAny<uint64_t>(in);
         }
      static void onSend(Any *out, T in)
         {
         out->set_extendedtypetag(type.id);
         writePrimitiveToAny(out, (uint64_t) in);
         }
      };
   template <typename T> TypeID ProtobufTypeConvert<T, typename std::enable_if<std::is_enum<T>::value>::type>::type;

   // Specialize conversion for pointer types
   template <typename T> struct ProtobufTypeConvert<T*> : PrimitiveTypeConvert<T*, uint64_t> { };

   // Specialize conversion for std::vector
   template <typename T> struct ProtobufTypeConvert<std::vector<T>>
      {
      static TypeID type;
      using ProtoType = std::string;

      static_assert(std::is_arithmetic<T>::value, "ProtobufTypeConvert for vector of non-fundemental type");
      static_assert(!std::is_same<T, bool>::value, "ProtobufTypeConvert for vector of bools (non-contiguous in standard)");

      static std::vector<T> onRecv(Any *in)
         {
         if (type.id != in->extendedtypetag())
            throw StreamTypeMismatch("Vector type mismatch: " + std::to_string(type.id) + " != "  + std::to_string(in->extendedtypetag()));
         std::string str = readPrimitiveFromAny<std::string>(in);
         size_t size = str.size() / sizeof(T);
         T* start = (T*) &str[0];
         std::vector<T> vec(start, start+size);
         return vec;
         }
      static void onSend(Any *out, std::vector<T> in)
         {
         out->set_extendedtypetag(type.id);
         writePrimitiveToAny(out, std::string((char *) &in[0], in.size() * sizeof(T)));
         }
      };
   template <typename T> TypeID ProtobufTypeConvert<std::vector<T>>::type;

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
         auto data = message->data(n);
         if (data.type_case() != primitiveTypeCase<typename ProtobufTypeConvert<Arg>::ProtoType>())
            throw StreamTypeMismatch("Expected type " + std::to_string(data.type_case()) + " but got type " + std::to_string(primitiveTypeCase<typename ProtobufTypeConvert<Arg>::ProtoType>()));
         return std::make_tuple(ProtobufTypeConvert<Arg>::onRecv(&data));
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
