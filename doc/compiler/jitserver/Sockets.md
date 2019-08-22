<!--
Copyright (c) 2000, 2019 IBM Corp. and others

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

# Sockets

Client server communication via TCP sockets is the default communication backend. The implementation mostly resides within the `rpc` directory. There is a base class `J9Stream`, which is specialized for both the client and server in `J9ClientStream` and `J9ServerStream`.

Encryption via TLS (OpenSSL) is optionally supported. See [Usage](Usage.md) for encryption setup instructions.

If a network error occurs, `JITServer::StreamFailure` is thrown.

### `J9Stream`
Implements basic functionality such as stream initialization (with/without TLS), reading/writing objects to streams, and stream cleanup.

### `J9ClientStream`

One instance per compilation thread. Typically, this is only interacted with through the function `handleServerMessage` in `JITServerCompilationThread.cpp`. An instance is created for a new compilation inside `remoteCompile`, and the `buildCompileRequest` method is then called on it to begin the compilation.

### `J9ServerStream`

There is one instance per compilation thread. Instances are created by `ServerStream::serveRemoteCompilationRequests`, a method which loops forever waiting for compilation requests and adding them to the compilation queue using the method `J9CompileDispatcher::compile`.

Accessible on a compilation thread via `TR::CompilationInfo::getStream()`, but beware that a thread local read is performed, so try to avoid calling it in particularly hot code.

Also stores the client ID, accessible via `J9ServerStream::getClientId`.
