<!--
Copyright (c) 2022, 2022 IBM Corp. and others

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

# Overview

This document contains descriptions of all the side tables used by MicroJIT to
generate code while limiting heap allocations.

# Register Stack

The register stack contains the number of stack slots contained
in each register used by TR's Linkage. This allows for MicroJIT
to quickly calculate stack offsets for interpreter arguments
on the fly.

RegisterStack:
- useRAX
- useRSI
- useRDX
- useRCX
- useXMM0
- useXMM1
- useXMM2
- useXMM3
- useXMM4
- useXMM5
- useXMM6
- useXMM7
- stackSlotsUsed

# ParamTableEntry and LocalTableEntry

This entry is used for both tables, and contains information used
to copy parameters from their linkage locations to their local
array slots, map their locations into GCMaps, and get the size of
their data without storing their data types.

ParamTableEntry:
| Field          | Description                                                       |
|:--------------:|:-----------------------------------------------------------------:|
| offset         | Offset into stack for loading from stack                          |
| regNo          | Register containing parameter when called by interpreter          |
| gcMapOffset    | Offset used for lookup by GCMap                                   |
| slots          | Number of slots used by this variable when storing in local array |
| size           | Number of bytes used by this variable when stored on the stack    |
| isReference    | Is this variable an object reference                              |
| onStack        | Is this variable currently on the stack                           |
| notInitialized | Is this entry uninitialized by MicroJIT                           |
