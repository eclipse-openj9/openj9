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

# JITServer Overview

This document provides a high-level overview of JITServer architecture.
If you are looking for a step-by-step guide on how to use JITServer, read ["Getting started with JITServer"](Usage.md) instead.

As mentioned in the above guide, JITServer introduces two additional personas to JVM: **client** and **server**, with clients outsourcing most of their compilation workload to a server.

## Server (`jitserver` binary)

Server is a JVM where only the JIT compiler is active. It compiles methods for client JVMs.

By default the server can use 64 compilation threads, compared to regular JVM's 7 threads. This value can be increased up to 999 with `-XcompilationThreadsNNN` command line option. One server can serve multiple clients at the same time.

JITServer can perform compilations of all types and hotness levels, e.g. AOT, thunks, hot compilations. It also supports an almost identical set of optimizations as a regular JIT, with a few minor exceptions.
If all compilations are done remotely, the performance gap compared to non-JITServer is usually within a few percentage points (~2%), meaning the quality of the remotely compiled code almost matches the locally compiled methods.

## Client (`-XX:+UseJITServer` option)

Client is a JVM that runs the target Java application. The only major difference from non-JITServer is that JIT compilation requests are sent to a server to be done remotely.

If a client detects that the server is down/unavailable, it will automatically switch to local compilations, performing identical to a non-JITServer JVM. Client will be able to reconnect to a server if it comes back up.

## Compilation lifecycle

Here is a step-by-step description of how client and server interact during a remote compilation:

1. Compilation request is generated on the client and added to the compilation queue
2. Request is popped from the queue and assigned a compilation thread
3. Compilation thread serializes compilation request and sends it to the server
4. Server listener thread receives request and adds it to the server compilation queue
5. Server pops compilation from the queue and assigns to a compilation thread
6. Queries are sent to the client to inquire about the VM or compilation options
7. A final message is sent which includes the compiled code
8. Client relocates and installs the code
