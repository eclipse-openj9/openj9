<!--
Copyright IBM Corp. and others 2023

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

# Background

The `J9JITExceptionTable` (also `typedef`'d to `TR_MethodMetaData`)
is generally referred to as "the metadata". It contains most of the
metadata associated with a compiled body[^1]. Most of this metadata
is stored in the variable length section of the `J9JITExceptionTable`
structure. This document outlines this variable length section. It does
not describe fields that are described by a data structure (e.g., the
`J9JITExceptionTable`, `TR_InlinedCallSite`, etc.).

# Structure

## High Level Overview
The following diagram provides the high level overview. The
`J9JITExceptionTable` is allocated and populated in
[`createMethodMetaData`](https://github.com/eclipse-openj9/openj9/blob/v0.41.0-release/runtime/compiler/runtime/MetaData.cpp#L1315).

```
    +-----------------------------------------------------+
    | struct J9JITExceptionTable                          |
    |-----------------------------------------------------|
(1) | Exception Info                                      |
    |-----------------------------------------------------|
(2) | Inline Info                                         |
    |-----------------------------------------------------|
(3) | Stack Atlas                                         |
    |-----------------------------------------------------|
(4) | Stack Alloc Map                                     | // If Local Objects
    |-----------------------------------------------------|
(5) | Internal Pointer Map in J9 Format                   |
    |-----------------------------------------------------|
(6) | OSR Info                                            | // If OSR
    |-----------------------------------------------------|
(7) | GPU Info                                            | // If GPU
    |-----------------------------------------------------|
(8) | Bytecode PC to Instruction Address Map              | // If Hardware Profiling
    +-----------------------------------------------------+
```

## (1) Exception Info
The Exception Info section starts immediately after the
`J9JITExceptionTable` struct, and so accessing it involves adding
`sizeof(J9JITExceptionTable)` bytes to a pointer pointing to the start
of the metadata. This section consists of the following set of data
for exceptions in the method body. For some entries, the size is 4 bytes
if wide offsets are required, and 2 bytes otherwise; wide offsets are
required when an offset cannot be represented in just 2 bytes. The
decision to use wide offsets or not is global (with respect to this
`J9JITExceptionTable`); i.e., if a single offset cannot be represented
in 2 bytes, then all offsets are represented in 4 bytes.
```
    +-----------------------------------------------------+
    | Instruction Start PC Offset                         |
    | Size: 2 or 4 bytes                                  |
    |-----------------------------------------------------|
    | Instruction End PC Offset                           |
    | Size: 2 or 4 bytes                                  |
    |-----------------------------------------------------|
    | Instruction Handler PC Offset                       |
    | Size: 2 or 4 bytes                                  |
    |-----------------------------------------------------|
    | Catch Type                                          |
    | Size: 2 or 4 bytes                                  |
    |-----------------------------------------------------|
    | BC Caller Index                                     | // If wide offsets, and AOT or Remote Compilation
    | Size: pointer                                       |
    |-----------------------------------------------------|
    | J9Method                                            | // If wide offsets, and local JIT Compilation
    | Size: pointer                                       |
    |-----------------------------------------------------|
    | BC Index                                            | // If FSD
    | Size: 4 bytes                                       |
    +-----------------------------------------------------+
```

## (2) Inline Info
The Inline Info section can be accessed via the `inlinedCalls` field of
the `J9JITExceptionTable` struct. This section consists of the following
set of data for each inlined site
```
    +-----------------------------------------------------+
    | struct TR_InlinedCallSite                           |
    | Size: sizeof(TR_InlinedCallSite)                    |
    |-----------------------------------------------------|
    | Monitor Autos Map                                   |
    | Size: Bytes needed to represent                     |
    |       cg->getStackAtlas()->getNumberOfSlotsMapped() |
    |       + 1 (for the live monitor map)                |
    +-----------------------------------------------------+
```

## (3) Stack Atlas
The Stack Atlas section can be accessed via the `gcStackAtlas` field
of the `J9JITExceptionTable struct`. This section consists of the
following set of data
```
    +-----------------------------------------------------+
    | struct J9JITStackAtlas                              |
    | Size: sizeof(J9JITStackAtlas)                       |
    |-----------------------------------------------------|
    | Monitor Autos Map                                   |
    | Size: Bytes needed to represent                     |
    |       cg->getStackAtlas()->getNumberOfSlotsMapped() |
    |       + 1 (for the live monitor map)                |
    |-----------------------------------------------------|
    | Stack Maps                                          |
    +-----------------------------------------------------+
```

The `Stack Map`s are written in the reverse order in which they exist
in the list in the Compilation. It consists of the following set of
data for each Stack Map.
```
    +-----------------------------------------------------+
    | Lowest code offset                                  |
    | Size: 2 or 4 bytes                                  | // 4 bytes if wide offsets, 2 bytes otherwise.
    |-----------------------------------------------------|
    | Alignment                                           | // If comp->isAlignStackMaps() && !fourByteOffsets
    | Size: 2 bytes                                       |
    |-----------------------------------------------------|
    | TR_BytecodeInfo                                     |
    | Size: 4 bytes                                       |
    |-----------------------------------------------------|
    | Register Save Description                           | // If the current map is different from the previous
    | Size: 4 bytes                                       |
    |-----------------------------------------------------|
    | Register Map                                        | // If the current map is different from the previous
    | Size: 4 bytes                                       |
    |-----------------------------------------------------|
    | Internal Pointer Map                                | // If it exists and if the current map is different from the previous
    |-----------------------------------------------------|
    | Map Bits                                            |
    | Size: Bytes needed to represent                     |
    |       map->getNumberOfSlotsMapped()                 |
    |-----------------------------------------------------|
    | Live Monitor Bits                                   | // If Live Monitor Bits
    | Size: Bytes needed to represent                     |
    |       map->getNumberOfSlotsMapped()                 |
    +-----------------------------------------------------+
```

where the `Internal Pointer Map` is
```
    +-----------------------------------------------------+
    | Map Size                                            |
    | Size: 1 byte                                        |
    |-----------------------------------------------------|
    | Number of Distinct Pinning Arrays                   |
    | Size: 1 byte                                        |
    |-----------------------------------------------------|
    | Distinct Pinning Array Info                         |
    +-----------------------------------------------------+
```

where `Distinct Pinning Array Info` contains the following set of data
for each Distinct Pinning Array
```
    +-----------------------------------------------------+
    | Index for this Pinning Array Temp                   |
    | Size: 1 byte                                        |
    |-----------------------------------------------------|
    | Number of Derived Register Pointers                 |
    | Size: 1 byte                                        |
    |-----------------------------------------------------|
    | Internal Pointer Register Number                    |
    | Size: 1 byte                                        |
    |-----------------------------------------------------|
    | Derived Register Number                             | // One entry for each Derived Register
    | Size: 1 byte                                        |
    +-----------------------------------------------------+
```

## (4) Stack Alloc Map
The Stack Alloc Map section can be accessed via the `stackAllocMap`
field of the `J9JITStackAtlas` struct that is embedded in the
[(3) Stack Atlas](#3-stack-atlas) section. It consists of the
following set of data
```
    +-----------------------------------------------------+
    | Pointer to first stack map                          |
    | Size: pointer                                       |
    |-----------------------------------------------------|
    | Stack Map Alloc Map Bits                            |
    | Size: Bytes needed to represent                     |
    |       cg->getStackAtlas() ->getStackAllocMap()      |
    +-----------------------------------------------------+
```

## (5) Internal Pointer Map in J9 Format
The  Internal Pointer Map in J9 Format section can be accessed via
the `internalPointerMap` field of the `J9JITStackAtlas` struct that
is embedded in the [(3) Stack Atlas](#3-stack-atlas) section. It
consists of the following set of data
```
    +-----------------------------------------------------+
    | Pointer to first stack map                          |
    | Size: pointer                                       |
    |-----------------------------------------------------|
    | Map Size                                            |
    | Size: 1 byte                                        |
    |-----------------------------------------------------|
    | Alignment                                           | // If comp->isAlignStackMaps()
    | Size: 1 byte                                        |
    |-----------------------------------------------------|
    | Index of First Internal Pointer                     |
    | Size: 2 bytes                                       |
    |-----------------------------------------------------|
    | Offset of First Internal Pointer                    |
    | Size: 2 bytes                                       |
    |-----------------------------------------------------|
    | Number of Distinct Pinning Array Temps              |
    |           + Pinning Arrays for Internal Pointer     |
    |             Registers                               |
    | Size: 1 byte                                        |
    |-----------------------------------------------------|
    | Distinct Pinning Array Temp Info                    |
    |-----------------------------------------------------|
    | Pinning Array for Internal Pointer Registers Info   |
    +-----------------------------------------------------+
```

Where `Distinct Pinning Array Temp Info` contains the following
set of data for each Distinct Pinning Array Temp
```
    +-----------------------------------------------------+
    | Index of Base Array Temp                            |
    | Size: 1 byte                                        |
    |-----------------------------------------------------|
    | Number of Derived Internal Pointers                 |
    | Size: 1 byte                                        |
    |-----------------------------------------------------|
    | Index of this Internal Pointer Temp                 |
    | Size: 1 byte                                        |
    |-----------------------------------------------------|
    | Index of Remaining Internal Pointer Temp            | // One entry for each Remaining Internal Pointer Temp
    | Size: 1 byte                                        | // derived from this Base Pinning Array Temp
    +-----------------------------------------------------+
```

and `Pinning Arrays for Internal Pointer Registers Info` contains
the following set of data for each Pinning Array for Internal Pointer
Registers
```
    +-----------------------------------------------------+
    | Index                                               |
    | Size: 1 byte                                        |
    |-----------------------------------------------------|
    | 0                                                   |
    | Size: 1 byte                                        |
    +-----------------------------------------------------+
```

## (6) OSR Info
The OSR Info section can be accessed via the `osrInfo` field
of the `J9JITExceptionTable` struct. This section consists of the
following set of data
```
    +-----------------------------------------------------+
    | Section 0:                                          |
    | Mapping from instruction PC offsets to a list       |
    | of shared slots that are live at that offset        |
    |-----------------------------------------------------|
    | Section 1:                                          |
    | Mapping from the call site index to OSR catch       |
    | block                                               |
    +-----------------------------------------------------+
```

### Section 0
```
    +-----------------------------------------------------+
    | Section Size                                        |
    | Size: 4 bytes                                       |
    |-----------------------------------------------------|
    | Max Scratch Buffer Size                             |
    | Size: 4 bytes                                       |
    |-----------------------------------------------------|
    | Number of Mappings                                  |
    | Size: 4 bytes                                       |
    |-----------------------------------------------------|
    | Instruction to Shared Slot Map                      |
    +-----------------------------------------------------+
```
where `Instruction to Shared Slot Map` contains the following
set of data for each mapping
```
    +-----------------------------------------------------+
    | Instruction PC offset                               |
    | Size: 4 bytes                                       |
    |-----------------------------------------------------|
    | Number of Scratch Buffer Infos                      |
    | Size: 4 bytes                                       |
    |-----------------------------------------------------|
    | Scratch Buffer Info                                 |
    +-----------------------------------------------------+
```
where `Scratch Buffer Info` contains the following set of
data for each Scatch Buffer Info
```
    +-----------------------------------------------------+
    | Inlined Site Index                                  |
    | Size: 4 bytes                                       |
    |-----------------------------------------------------|
    | OSR Buffer Offset                                   |
    | Size: 4 bytes                                       |
    |-----------------------------------------------------|
    | Scratch Buffer Offset                               |
    | Size: 4 bytes                                       |
    |-----------------------------------------------------|
    | Sym Size                                            |
    | Size: 4 Bytes                                       |
    +-----------------------------------------------------+
```

### Section 1
```
    +-----------------------------------------------------+
    | Section Size                                        |
    | Size: 4 bytes                                       |
    |-----------------------------------------------------|
    | Number of Mappings                                  |
    | Size: 4 bytes                                       |
    |-----------------------------------------------------|
    | Start PC Offset or NULL                             | // For each mapping
    | Size: 4 bytes                                       |
    +-----------------------------------------------------+
```

## (7) GPU Info
The GPU Info section can be accessed via the `gpuCode` field
of the `J9JITExceptionTable` struct. This section consists of the
following set of data
```
    +-----------------------------------------------------+
    | struct GpuMetaData                                  |
    | Size: sizeof(GpuMetaData)                           |
    |-----------------------------------------------------|
    | Method Signature Offset                             |
    | Size: strlen(comp->signature())+1                   |
    |-----------------------------------------------------|
    | Line Number Array                                   |
    |-----------------------------------------------------|
    | Ptx Array                                           |
    |-----------------------------------------------------|
    | Ptx Array Entry                                     |
    |-----------------------------------------------------|
    | CU Module Array; initialized to 0                   |
    | Size: sizeof(CUmodule)                              |
    |       * comp->getGPUPtxCount()                      |
    |       * maxNumCachedDevices                         |
    +-----------------------------------------------------+
```
`Line Number Array Offset` contains the following
set of data for each GPU Ptx
```
    +-----------------------------------------------------+
    | Line Number                                         |
    | Size: sizeof(int)                                   |
    +-----------------------------------------------------+
```
`Ptx Array` contains the following set of data
for each GPU Ptx
```
    +-----------------------------------------------------+
    | GPU Ptx Array Entry Location                        |
    | (Pointer into the                                   |
    | `Ptx Array Entry` section)                          |
    | Size: pointer                                       |
    +-----------------------------------------------------+
```
`Ptx Array Entry` contains the following set
of data for each GPU Ptx
```
    +-----------------------------------------------------+
    | Ptx Array Entry                                     |
    | (pointed to by the associated                       |
    | entry in the `Ptx Array` section)                   |
    | Size: strlen of the string at this location         |
    |       + 1                                           |
    +-----------------------------------------------------+
```

## (8) Bytecode PC to Instruction Address Map
The Bytecode PC to Instruction Address Map section can be accessed
via the `riData` field of the `J9JITExceptionTable` struct. This
section consists of the following set of data
```
    +-----------------------------------------------------+
    | METADATA_MAPPING_EYECATCHER                         |
    | Size: pointer                                       |
    |-----------------------------------------------------|
    | Map Size                                            |
    | Size: pointer                                       |
    |-----------------------------------------------------|
    | Bytecode PC to IA Map                               |
    +-----------------------------------------------------+
```
where `Bytecode PC to IA Map` is the following
set of data for each mapping
```
    +-----------------------------------------------------+
    | Bytecode PC                                         |
    | Size: pointer                                       |
    |-----------------------------------------------------|
    | Instruction Address                                 |
    | Size: pointer                                       |
    +-----------------------------------------------------+
```

[^1]: The rest of the metadata is stored in the
`TR_PersistentJittedBodyInfo` structure, which can be accessed
via the `bodyInfo` field of the `J9JITExceptionTable` struct. This
structure contains data such as flags used to describe the body,
as well as data used to implement features such as
Guarded Counting Recompilation (GCR).