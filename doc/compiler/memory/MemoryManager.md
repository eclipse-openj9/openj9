<!--
Copyright (c) 2017, 2018 IBM Corp. and others

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

# OpenJ9 Compiler Memory Manager

The JIT Compiler in OpenJ9 extends the OMR Compiler. Therefore, it
also extendes the OMR Compiler's Memory Manager, consequently 
managing its own memory for the same reasons as the OMR Compiler. 
However, in order to ensure that all memory in the JVM is accounted 
for, as well as to minimize the likelihood of memory leaks,
the OpenJ9 Compiler Memory Manager uses JVM APIs instead 
of OS APIs directly.

Some parts of the memory manager override the OMR implementations, while 
others either extend or directly use them. For information on any terms 
not documented here, refer to the documentation found in 
[eclipse/omr](https://github.com/eclipse/omr/blob/master/doc/compiler/memory/MemoryManager.md).

## Compiler Memory Manager Hierarchy

### Low Level Allocators
Low Level Allocators cannot be used by STL containers. They are 
generally used by High Level Allocators as the underlying 
provider of memory.

```
                          +---------------------+
                          |                     |
                          | TR::SegmentProvider |
                          |                     |
                          +-----+-----------+---+
                                ^           ^
                                |           |
                                |           |
                                |           |
          +---------------------+--+     +--+-----------------------+
          |                        |     |                          |
          |  TR::SegmentAllocator  |     | J9::DebugSegmentProvider |
          |                        |     |                          |
          +--------+---------------+     +--------------------------+
                   ^
                   |
                   |
+------------------+---------+
|                            |
| J9::SystemSegmentProvider  |
|                            |
+----------------------------+


--------------------------------------------------------------------


                +-----------------------+
                |                       |
                | J9::J9SegmentProvider |
                |                       |
                +-----+-----------+-----+
                      ^           ^
                      |           |
                      |           |
                      |           |
+---------------------+--+     +--+---------------+
|                        |     |                  |
|  J9::SegmentAllocator  |     | J9::SegmentCache |
|                        |     |                  |
+------------------------+     +------------------+

```

#### J9::SystemSegmentProvider
`J9::SystemSegmentProvider` implements `TR::SegmentAllocator`. 
It uses `J9::J9SegmentProvider` (see below) in order to allocate 
`TR::MemorySegment`s and uses `TR::RawAllocator` for all its 
internal memory management requirements. As part of its initialization, it
preallocates a segment of memory. This is the default Low 
Level Allocator used by the Compiler. However, the compiler will
use `OMR::DebugSegmentProvider` instead of `J9::SystemSegmentProvider` 
when the `TR_EnableScratchMemoryDebugging` option is enabled.

#### J9::DebugSegmentProvider
`J9::DebugSegmentProvider` implements `TR::SegmentProvider`. 
This object exists specifically for the Debugger Extensions. It
uses `dbgMalloc`/`dbgFree` to allocate and free memory.

#### J9::J9SegmentProvider
`J9::J9SegmentProvider` is a pure virtual class which provides
APIs to
* Request Memory
* Release Memory
* Inform about the preferred segment size

It is related to `TR::SegmentProvider` not by inheritence, but rather
 is chained to `J9::SystemSegmentProvider`. In OpenJ9, the Port
Library provides APIs to allocate/deallocate segments of memory. These
segments show up in the `Internal Memory` section of a javacore. They
allow the Compiler to minimize the number of times it needs to request
memory from the Port Library (an expensive operation for the allocation
patterns exhibited by the Compiler). Therefore, in some sense, 
`J9::J9SegmentProvider` is the truest allocator of segments as defined by
the Port Library. It should only be used as the backing provider of
other Allocators.

#### J9::SegmentAllocator
`J9::SegmentAllocator` implements `J9::J9SegmentProvider`. It uses the
`allocateMemorySegment` and `freeMemorySegment` APIs accessed via
`javaVM->internalVMFunctions`.

#### J9::SegmentCache
`J9::SegmentCache` implements `J9::J9SegmentProvider`. It requires a
`J9::SegmentAllocator` instantiated to use as its backing provider. As part
of its initialization, it preallocates a segment. This class exists
as an optimization to minimize the churn of memory allocation and
deallocation between compilations.

### High level Allocators
High Level Allocators can be used by STL containers by wrapping them
in a `TR::typed_allocator` (see below). They generally use a Low 
Level Allocator as their underlying provider of memory.

#### TR::PersistentAllocator
`TR::PersistentAllocator` is used to allocate memory that persists 
for the lifetime of the JVM. This class overrides `OMR::PersistentAllocator`. 
It uses `J9::SegmentAllocator` to allocate memory and `TR::RawAllocator` 
for all its internal memory management requirements. It receives these 
allocators via the `TR::PersistentAllocatorKit`. All allocations/deallocations 
are thread safe. Therefore, there is extra overhead due to the need to 
acquire a lock before each allocation/dellocation. It also contains an automatic 
conversion (which wraps it in a `TR::typed_allocator`) for ease of use with 
STL containers.

#### TR::RawAllocator
`TR::RawAllocator` uses `j9mem_allocate_memory`/`j9mem_free_memory` to allocate 
and free memory. This class overrides `TR::RawAllocator` defined in OMR. It also 
contains an automatic conversion (which wraps it in a 
`TR::typed_allocator`) for ease of use with STL containers.


## How the Compiler Allocates Memory

The Compiler deals with two categories of allocations:
1. Allocations that are only useful during a compilation
2. Allocations that need to persist throughout the lifetime of the JVM

### Allocations only useful during a compilation
Before a Compilation Thread begins going through the list of methods to
be compiled, it initializes a `J9::SegmentCache` local object. As stated
above, this will preallocate a segment. Then, it goes through the list. 
At the start of a compilation, it initializes a `J9::SystemSegmentProvider`, 
passing in the `J9::SegmentCache` as the backing provider (technically it is 
passed a reference to a `J9::J9SegmentProvider`, the reason for which is described
below). As stated above, `J9::SystemSegmentProvider` will allocate a
segment as part of its initialization. However, the size of the segment it
allocates is the same as the size of the segment preallocated by
`J9::SegmentCache`. Therefore, this operation becomes very inexpensive. If 
`TR_EnableScratchMemoryDebugging` is enabled then
`OMR::DebugSegmentProvider` is used instead. The rest of the process is similar 
to that of OMR; it initializes a local `TR::Region` object, and a local 
`TR_Memory` object which uses the `TR::Region` for general allocations 
(and sub Regions), as well as for the first `TR::StackMemoryRegion`. 

At the end of the compilation, the `TR::Region` and `J9::SystemSegmentProvider` 
(or `OMR::DebugSegmentProvider`) go out of scope, invoking their 
destructors and freeing all the memory. However, when
`J9::SystemSegmentProvider` releases its memory via `J9::SegmentCache`, the
latter will free all memory **EXCEPT** the initial segment it preallocated.
Thus, when the Compilation Thread goes to compile the next method, it immediately
has memory available for compilation thanks to a significant amount of memory
already preallocated. After a period of time when no compilations have
occured, the Compilation Thread exits the scope `J9::SegmentCache` was
instantiated in, causing it to be destroyed and finally releasing all of its
memory.

The reason why `J9::SystemSegmentProvider` is passed in a reference to
`J9::J9SegmentProvider` and not `J9::SegmentCache` is because the code that
instantiates `J9::SystemSegmentProvider` is common for both compilations on 
Compilation Threads AND compilations on Application Threads. When a compilation 
occurs on an Application Thread, `J9::SegmentAllocator` is instantiated instead.

There are a lot of places (thanks to `TR_ALLOC` and related macros) 
where memory is explicity allocated. However, `TR::Region` 
should be the allocator used for all new code as much as possible.

### Allocations that persist for the lifetime of the JVM
The Compiler initializes a `TR::PeristentAllocator` object when 
it is first initialized (close to bootstrap time). For the most 
part it allocates persistent memory either directly using the global 
`jitPersistentAlloc`/`jitPersistentFree` or via the object methods 
added through `TR_ALLOC` (and related macros). `TR::PersistentAllocator` 
should be the allocator used for all new code as much as possible.


## Subtleties and Miscellaneous Information

:rotating_light:`TR::Region` allocations are untyped raw memory. In order to have a region
destroy an object allocated within it, the object would need to be created
using `TR::Region::create`. This requires passing in a reference to an
existing object to copy, which requires that the object be
copy-constructable. The objects die in LIFO order when the Region is destroyed.:rotating_light:

`TR::PersistentAllocator` is not a `TR::Region`. That said, if one wanted
to create a `TR::Region` that used `TR::PersistentAllocator` as its backing
provider, they would simply need to extend `TR::SegmentProvider`, perhaps
calling it `TR::PersistentSegmentProvider`, and have it use
`TR::PersistentAllocator`. The `TR::Region` would then be provided this 
`TR::PersistentSegmentProvider` object.

