<!--
Copyright IBM Corp. and others 2018

This program and the accompanying materials are made available under
the terms of the Eclipse Public License 2.0 which accompanies this
distribution and is available at https://www.eclipse.org/legal/epl-2.0/
or the Apache License, Version 2.0 which accompanies this distribution and
is available at https://www.apache.org/licenses/LICENSE-2.0.

This Source Code may also be made available under the following
Secondary Licenses when the conditions for such availability set
forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
General Public License, version 2 with the GNU Classpath
Exception [1] and GNU General Public License, version 2 with the
OpenJDK Assembly Exception [2].

[1] https://www.gnu.org/software/classpath/license.html
[2] https://openjdk.org/legal/assembly-exception.html

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
-->

# JITServer Messaging Protocol

## Overview

All data exchanged between server and clients is structured in the form of "JITServer Messsages". Every message sent requires a response, i.e. messages are exchanged in pairs. Both client and server can send messages. Client sends only compilation requests, while server sends all other message types.

Every JITServer message (outgoing and incoming) has the following properties:

- It has a `MessageType`, as defined in `runtime/compiler/net/MessageTypes.hpp`
- It contains one or more serializable datapoints. Serializable datapoints are defined
as either trivially copyable datatypes, or as datatypes with `onRecv()` and `onSend()` functions specialized in `RawTypeConvert` (see [Supported Datatypes](#supported-datatypes)).
- The message type defines the structure of a message, i.e. the type determines the number, order and types of datapoints. For every message, we need to send some specific data, and receive a specific response.

> **Note:** Since at least one datapoint is required, it's not technically possible to send an empty message. To achieve the same result, send `JITServer::Void` and then read it on the receiving end.

Messages are not usually constructed directly. Instead, `Client/ServerStream::write()` and `Client/ServerStream::read()` methods are used to send a message as a variadic argument list, and read it as a tuple (See the [example](#example) below).

For the messaging protocol to work correctly, client and server must run matching JITServer versions. If a new message type gets added, or the structure of an existing message is modified, the protocol version number needs to be incremented.

### **Supported datatypes**

- Integers and integer-derived types, e.g. `int32_t`, `uint64_t`, `enum`, `bool`.
- [Trivially-copyable](https://en.cppreference.com/w/cpp/types/is_trivially_copyable) classes, i.e. classes that can be safely copied with `memcpy`.
- `std::string` - to send a `char *`, convert it to `std::string` first.
- `std::vector` - vector elements must be of a supported datatype (nested vectors/tuples are supported).
- `std::tuple` - tuple elements must be of a supported datatype (nested vectors/tuples are supported).

### **Example**

Here is a sample code of a typical message exchange for the message type `VM_dereferenceStaticAddress`:

Server sends the message to the client:

```c++
TR_StaticFinalData
TR_J9ServerVM::dereferenceStaticFinalAddress(void *staticAddress, TR::DataType addressType)
    {
    ....
    JITServer::ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
    stream->write(JITServer::MessageType::VM_dereferenceStaticAddress, staticAddress, addressType);
    ...
    }
```

Client receives the message in `handleServerMessage`, fetches the required data and writes back a response:

```c++
case MessageType::VM_dereferenceStaticAddress:
    {
    auto recv = client->getRecvData<void *, TR::DataType>();
    void *address = std::get<0>(recv);
    auto addressType = std::get<1>(recv);
    client->write(response, fe->dereferenceStaticFinalAddress(address, addressType));
    }
    break;
```

Server reads the response

```c++
TR_StaticFinalData
TR_J9ServerVM::dereferenceStaticFinalAddress(void *staticAddress, TR::DataType addressType)
    {
    ....
    JITServer::ServerStream *stream = _compInfoPT->getMethodBeingCompiled()->_stream;
    stream->write(JITServer::MessageType::VM_dereferenceStaticAddress, staticAddress, addressType);
    auto data =  std::get<0>(stream->read<TR_StaticFinalData>());
    ...
    }
```

Data is written by calling `write()` function of a stream object,
giving it `MessageType` as a first argument, and supplying datapoints as remaining arguments.

Data is read by calling `read()` function of a stream object; a tuple is returned.

Stream objects will internally invoke methods to construct messages from the supplied arguments and to convert messages to tuples. For more information on streams, read ["Networking"](Networking.md).

The following sections describe implementation details of messaging. That information is only needed if you are modifying the messaging infrastructure itself.
If you just want to add a new message type, proceed to [here](#adding-new-message-types).

## `Message`

The top-level class describing a JITServer message.
Contains two nested classes: `MetaData` and `DataDescriptor`.

`Message::MetaData` - describes the overall structure of the message, such as message type, version number, and number of datapoints.

`Message::DataDescriptor` - describes a single datapoint in a message, such as the type of the datapoint and the size of the payload. All valid datatypes are defined in `Message::DataDescriptor::DataType`.

Each message contains `MetaData` and some number of `DataDescriptor`s.

The `Message` object provides methods to add and retrieve descriptors and datapoints, but it uses `MessageBuffer` for underlying storage.

A message needs to be written to the network stream and read from it. This is done using `serialize()` and `deserialize()` methods:

- `serialize()` - returns a pointer to the beginning of the underlying `MessageBuffer`.
This can be written to a network.
- `deserialize()` - constructs a valid `Message`, assuming that the underlying `MessageBuffer` is populated with valid message data that was received from a network stream.

## `MessageBuffer`

The underlying storage for `Message` contents. It is implemented as a contiguous buffer
that's automatically expanded if the message contents become too big for the current buffer.

`MessageBuffer` provides methods to get a pointer to the beginning of the buffer, as well as methods to extract a pointer to a value at a given offset into the buffer. We use offsets due to direct pointers being invalidated with each buffer expansion.

## `RawTypeConvert`

This is a class that performs serialization/deserialization of supported data types.
It contains two template methods that are specialized for each data type:

- `uint32_t RawTypeConvert<T>::onSend(Message &msg, T val)` - this method should be overriden to serialize value of type `T`, add it to the message and return the number of bytes written.
- `T RawTypeConvert<T>::onRecv(Message::DataDescriptor *desc)` - this method should be overridden to deserialize data of type `T` from the data descriptor and return it.

## `GetArgsRaw`/`SetArgsRaw`

`SetArgsRaw` is used to convert a variadic argument list into a valid message, while
`GetArgsRaw` is used to convert a message into a tuple. They work by using template metaprogramming to recursively invoke `RawTypeConvert` methods on each datapoint.

## Adding new message types

New message types might need to be added when some new JIT code that uses VM-specific information is introduced. For instance if a new method is added to `TR_J9VM`, it probably needs to be extended in `TR_J9ServerVM` from where a new remote message will be sent. Whenever possible, new message types need to be contained to front-end classes, to avoid inserting JITServer-specific code all over the JIT codebase. Read ["JITServer Front-end"](Frontend.md) to learn more. Before adding a new message type, first make sure that the required task cannot be achieved with an existing message.

Here is a list of steps needed to add a new message type:

1. Add a new message type to `runtime/compiler/net/MessageTypes.hpp`. The new type needs to be added to `enum MessageType` and string array `messageNames`. The order matters, so make sure that the new message is in the right order in both structures.

2. Add code to send the message from the server.

3. Add a new message handler to `handleServerMessage()` in `runtime/compiler/control/JITClientCompilationThread.cpp`. The handler should read the incoming message, perform whatever work needs to be done on the client, and write a response back to the server.

4. Add code to receive the client's response on the server.

5. Increment version number, as defined in `CommunicationStream::MINOR_NUMBER`. This will indicate that a compatibility-breaking change has been made to JITServer.

For Steps 2-4, [example](#example) can be used as a reference.

Once the new message is added, it is usually a good idea to do some testing with `TR_PrintJITServerMsgStats` env variable to see if the new message gets exchanged a lot.
If it's being sent very frequently, there will be negative performance implications, so you should consider caching the results. For more information, read ["Caching in JITServer"](Caching.md).
