<!--
Copyright (c) 2018, 2019 IBM Corp. and others

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
[2] http://openjdk.java.net/legal/assembly-exception.html

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
-->

The IProfiler is specialized for JITServer in two classes: `JITServerIProfiler` and `JITClientIProfiler`. They are allocated in the method `onLoadInternal` in `rossa.cpp`.

There are two different kinds of entries that we handle: method entries and bytecode entries.

### Method entries

Holds profiling data relating to a method. This data is currently not cached. When the method `searchForMethodSample` is called on the server, it sends a message to the client. The client serializes the data using `TR_ContiguousIPMethodHashTableEntry::serialize` and the server deserializes it with `deserializeMethodEntry`.

### Bytecode entries

Profiling data for individual bytecodes. This data is retrieved during compilation using the method `profilingSample`. It is cached at the server in the client session data.

To reduce the number of messages sent, the client will sometimes decide to send the data for the entire method even when the server only asks for a single bytecode. This is done because it is likely that the server will continue on to request other bytecodes in the same method as the compilation progresses. Currently, this will happen if the method is already compiled or is currently being compiled, but otherwise not (for example, if it is being inlined early). This decision is made by the function `handler_IProfiler_profilingSample` in `JITServerCompilationThread.cpp`.

In a debug build, extra messages will be sent to verify that the cached data is (mostly) correct. Often, the profiled counters may be slightly wrong and this can produce warnings, but they are safe to ignore as long as the cached value differs only slightly and does not appear to be corrupted. This validation is performed in `JITServerIProfiler::validateCachedIPEntry`.

Serialization is performed by `JITClientIProfiler::serializeAndSendIProfileInfoForMethod` and deserialization from within `profilingSample`. The code appears quite complex because there are special cases for different types of samples, but if you ignore those cases it's fairly straightforward.
