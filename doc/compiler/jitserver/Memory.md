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

# JITServer Memory Model

## Types of Memory

Client and server use the same memory types as a regular JIT, i.e. scratch memory (heap/stack/region) and persistent memory, where persistent memory is the only type that persists across multiple compilations, while other memory is automatically deallocated at the end of a compilation.

There is one large difference on the server side: it instantiates persistent allocators per client, instead of using one global allocator.

## Persistent per-client memory

On the server side, the vast majority of persistent memory is used for caching (see ["JITServer Client Sessions"](ClientSession.md) and ["Caching in JITServer"](Caching.md)). Since caching is usually done per-client, JITServer can use a different persistent allocator for each client cache. The benefit of this approach is that once a client disconnects, and the corresponding client session is destroyed, its persistent allocator can be destroyed as well, which frees all the allocated free blocks, reducing overall memory consumption. Having multiple allocators also reduces lock contention on the server, improving scalability.

### Allocation Regions

Server begins per-client allocation by calling `TR::CompilationInfoPerThreadBase::enterPerClientAllocationRegion()`,
once it reads a compilation request.

When a compilation is done (or aborted), server invokes `TR::CompilationInfoPerThreadBase::exitPerClientAllocationRegion()` to stop
per-client allocation.

While most allocations can be done on a per-client basis, some persistent allocations
must be done globally, i.e. they must not depend on the lifetime of a client session.

The `enter/exit` methods can be used to handle this but to make things easier, JITServer introduces *allocation region* objects. There are two types of allocation regions: `PerClientAllocationRegion` and `GlobalAllocationRegion`.

They use [RAII](https://en.wikipedia.org/wiki/Resource_acquisition_is_initialization) technique so that all allocations made inside the scope of a region object are either done per-client or globally. The most common use case is to initialize a global allocation region inside a per-client region, when some global allocations need to be done.
