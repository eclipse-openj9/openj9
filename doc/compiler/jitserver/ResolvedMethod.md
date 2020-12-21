<!--
Copyright (c) 2018, 2021 IBM Corp. and others

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

# Resolved Methods
## What are resolved methods?

Resolved methods are wrappers around Java methods which exist for the duration of a single compilation. They collect and manage information about a method and related classes.
Resolved methods are some of the most commonly used entitites by the JIT compiler, thus it is important to handle them correctly and efficiently on JITServer.

For non-JITServer, we mostly care about 2 classes representing a resolved method - `TR_ResolvedJ9Method` and `TR_ResolvedRelocatableJ9Method`.
The former represents resolved methods in a regular compilation, and the latter in an AOT compilation.

## JITServer implementation

### Class hierarchy
On the server, we implement two new classes to represent resolved methods - `TR_ResolvedJ9JITServerMethod` and `TR_ResolvedRelocatableJ9JITServerMethod`.
Same as for non-JITServer, the former is for regular compilations and the latter is for AOT. `TR_ResolvedJ9JITServerMethod` is extended from `TR_ResolvedJ9Method` and 
`TR_ResolvedRelocatableJ9JITServerMethod` is extended from `TR_ResolvedJ9JITServerMethod`.

### Client-side mirrors
Upon instantiation of a server-side resolved method, a resolved method is also created on the client that represents the same Java method. We call that client-side copy "mirror".

Instantiation happens either directly via the `createResolvedMethod` family, or indirectly using one of multiple `getResolvedXXXMethod` methods, which perform operations to locate a method of interest and then create the corresponding `ResolvedMethod`.

### Caching inside resolved methods
Some answers to resolved method queries are cached at the server to avoid sending remote messages.
All of this information is sent during method instantiation, inside `TR_ResolvedJ9JITServerMethodInfo`.
The cached values either won't change during the lifetime of a resolved method (i.e. constant pool pointer, J9 class pointer)
or having incorrect value cached for the duration of one compilation does not have impact on correctness or performance (i.e. whether a method is interpreted).

Resolved methods could also be used as more general per-compilation caches, containing information that does not directly describe the resolved method itself.
Since resolved methods are destroyed at the end of compilation, this would be functionally correct.
However, such usage is discouraged because `TR::CompilationInfoPerThreadRemote` is a better place for such caches. Placing caches there makes them more visible
and it makes more sense to store information not related to resolved methods outside of them.

### Performance considerations

Since resolved methods are so frequently created and used, the number of remote messages associated with them is normally a large portion of total messages. Therefore, minimizing the number of requests for resolved method creation is important for improving CPU consumption.

There are two main ways we optimize resolved method creation:
1. Caching all newly created resolved methods in a per-compilation cache on the server. We use `TR_ResolvedMethodInfoCache` and put it in `TR::CompilationInfoPerThreadRemote`.
Whenever a resolved method needs to be created, server will first check the cache and recreate the resolved method from there, if possible.
2. Prefetching multiple resolved methods before they are first requested. There are parts of the compiler that request many resolved methods in a loop.
For example, ILGen and inliner will walk through method bytecodes and create a resolved method for every method call bytecode. 
We use `TR_ResolvedJ9JITServerMethod::cacheResolvedMethodsCallees` to do a preliminary walk and cache all required methods in one remote call.
Techniques like these do not reduce the total number of resolved methods created or data transferred through the network but it reduces the total number of remote calls,
which still has a positive impact on CPU consumption.

Despite all these optimizations, resolved methods are still responsible for most of the remote messages. This is due to the fact that resolved methods only exist
for the duration of a single compilation so any type of persistent caching is not available to us.
