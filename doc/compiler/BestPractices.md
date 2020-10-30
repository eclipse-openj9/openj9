<!--
Copyright (c) 2019, 2020 IBM Corp. and others

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

# Compiler Best Practices
This doc is meant to serve as a place to add and refresh Best Practices
that should be kept in mind when developing on the Compiler component
of Eclipse OpenJ9.

## AOT
AOT, i.e. Ahead Of Time, Compilation is basically a regular JIT compilation,
but with the added requirement that it be relocatable. Therefore, regardless
of where in the compiler work is being done, it is important to always keep
in mind how said work would apply to a relocatable compile.

### CPU Feature Validation
1. Use only the processor feature flags the JIT cares about to determine CPU features
2. If the JIT uses some CPU feature that, for whatever reason, isn't available via 
the CPU processor feature flags, it should be a Relo Runtime Feature Flag (so that 
the same query can be performed at load time)

## JITServer
With JITServer tech, the JIT compiler is decoupled from the rest of the 
JVM (the runtime) and has to make remote calls (over the network) to find various
pieces of information about the runtime environment (Java classes/methods, 
VM/GC data structures, JIT runtime data structures like CHTable, runtime 
assumption table, profiling information, etc.).
In order to keep the JITServer healthy going forward, the rule-of-thumb for
JIT developers is to access all runtime information through a "frontend" layer. 
This includes:
1. The `TR_FrontEnd` class and all its derived classes
2. The family of `TR::Method` classes (`TR_J9MethodBase`, `TR_J9Method`,
`TR_ResolvedMethod`, `TR_ResolvedJ9MethodBase`, `TR_ResolvedJ9Method`, 
`TR_ResolvedRelocatableJ9Method`), 
3. Classes like `CompilerEnv`, `ClassEnv`, `ObjectModel`, `VMEnv`, 
`VMMethodEnv` (and their OpenJ9 derived classes)

If adding new methods to the "frontend" classes shown above, we may need to
provide an overridden implementation in the JITServer derived classes.
If changing/amending existing methods in the "frontend" classes shown above, 
we may need to adjust the implementation of the JITServer overridden methods too.
Also, care must be taken when changing CHTable, IProfiler, Runtime Assumptions and
RelocationRuntime routines because some of the methods in these data structures
have been declared virtual and have been overridden to be able to fetch the 
desired data from a remote JVM.
New runtime data structures may need to provide overridden implementations to
handle remote calls.

Moreover, static data structures (or globals) should be avoided as much as
possible because this may prevent JITServer from concurrently serving multiple
clients that are configured differently. For instance, per-compilation Options
are serialized and sent to the server to be used for a particular compilation, 
but JVM global options through static fields cannot be processed because they
would affect other connected clients.

:rotating_light:Any time a Front End query or a relocation type is changed or added,
the `MINOR_NUMBER` **must** be updated to prevent a client from connecting to a
server that may issue a query or generate a relocation that the client does not
know how to handle.:rotating_light:

