We use [Protocol Buffers](https://github.com/google/protobuf) for serialization. Data is serialized into generic structures which are type checked at runtime. We do this mostly for simplicity and flexibility. By dynamically serializing data, we can send and receive arbitrary tuples of data in a single line of code, with little runtime overhead. There is no need to edit protocol files or write serializers for custom data types. The implementation relies heavily on C++ template metaprogramming to provide this convenience and requires a fully-featured C++11 compiler.

Relevant files:
```
rpc/ProtobufTypeConvert.hpp
rpc/protos/compile.proto
rpc/raw/J9Server.*
rpc/raw/J9Client.*
rpc/grpc/J9Server.*
rpc/grpc/J9Client.*
```

To add a new message, edit `rpc/protos/compile.proto`.

On the server, messages can be sent and received from anywhere via the `J9ServerStream`, using the methods `write` and `read`.  The stream is accessible on any compilation thread via `TR::CompilationInfo::getStream()`, but be aware that a thread local read is performed, so try to avoid calling it in particularly hot code. Certain classes may cache a pointer to the stream.

On the client, receiving messages is handled automatically within a loop in `JITaaSCompilationThread.cpp`. A function called `handleServerMessage` is called for each received message. A switch statement checks the type of message and sends an appropriate response. Instead of `read`, the client side uses a method called `getRecvData` to retrieve the read data. This is done because there is some common functionality that must be handled for each message read. `read` should not be called directly on the client, unless you know what you are doing.

If there is a mismatch between the sent and received types, an assertion failure will occur in debug builds. In prod, this error will just fail the compilation. It's important that development is done with debug builds to catch these errors early.

To determine whether a type `T` can be serialized with the current infrastructure, consider the following (psuedocode):
```
is_serializable<T> =
      T == uint32_t
   || T == uint64_t
   || T == int32_t
   || T == int64_t
   || T == bool
   || std::is_trivially_copyable<T>
   || T == std::string
   || T == std::vector<U> && is_serializable<U>
   || T == std::tuple<U...> && is_serializable<U>...
   || std::is_base_of<google::protobuf::Message, T>
```

If you are not familiar with some advanced C++ features, this may look quite confusing. Trivially copyable objects are those that can be safely `memcpy`'d. `std::is_base_of<google::protobuf::Message, T>` means that `T` inherits from `google::protobuf::Message`.

**Warning:** Pointers are trivially copyable and will be copied directly. The value they point to will not be copied. If you need to send a string, make sure it is wrapped in an `std::string`, or else you will end up sending the location of the string and none of the contents.

Note that `std::string`, `std::vector` and `std::tuple` are not trivially copyable, so structs containing them cannot be serialized. Structs containing only primitive types are fine. If you need to send a struct like object containing non trivially copyable types, use `std::tuple`. Structs containing pointers **are** trivially copyable, but the values pointed to will not be copied for you. These details arise because it is impossible to inspect the types of fields in a struct at compile time in C++ (unless you consider The Great Type Loophole, but let's not go there).

If you need to send an empty message (forcing the other process to block until it is received), you can send the special type JITaaS::Void. Sending/receiving no values is not supported so an empty type is used as a workaround.
