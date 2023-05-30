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

# Networking

This document describes the networking component of JITServer that enables client and server to exchange messages.

The implementation resides within the `runtime/compiler/net` directory.
There is a listener thread `TR_Listener` and a base class `CommunicationStream`, which is specialized for both the client and server in `ClientStream` and `ServerStream`.

Encryption via TLS (OpenSSL) is optionally supported. See ["Getting started with JITServer"](Usage.md) for encryption setup instructions.

If a network error occurs, `JITServer::StreamFailure` is thrown.

## `TR_Listener`

Implementation of a server's "listener" thread that waits for network connection requests.
Server creates this thread and then invokes `TR_Listener::serveRemoteCompilationRequests`,
which will open a TCP socket and `poll` it for new connection requests. Once such request is detected, a new `ServerStream` is initialized, establishing a communication stream between client's and server's compilation threads. The stream is then assigned to a compilation request entry that gets added to the compilation queue.

If encryption is required, the listener thread will also setup the necessary context for the server side.

## `CommunicationStream`

The base stream class that implements functionality for reading/writing JITServer messages to/from an open file descriptor. It also configures common stream parameters and cleans up. `CommunicationStream` uses `Message` and `MessageBuffer` classes to read/write messages. To learn more about those, read ["JITServer Messaging Protocol"](Messaging.md).

## `ClientStream`

Extends `CommunicationStream` to implement an interface for clients to communicate with a server. One instance per active client compilation thread.

Typically, this is only interacted with through the function `handleServerMessage` in `JITClientCompilationThread.cpp`. An instance is created for a new compilation inside `remoteCompile`, and the `buildCompileRequest` method is then called on it to begin the compilation.

If encryption is required, client stream will initialize necessary context and open an SSL connection, instead of a regular one.

## `ServerStream`

Extends `CommunicationStream` to implement an interface for server to communicate with a client. There is usually one instance of this object per active server compilation thread, however streams are not permanently attached to any particular thread. Instead, they are assigned to compilation entries that can be retrieved from the compilation queue by any thread.

Accessible on a compilation thread via `TR::CompilationInfo::getStream()`, but beware that a thread local read is performed, so try to avoid calling it in particularly hot code.

Also stores the client ID, accessible via `ServerStream::getClientId()`.
