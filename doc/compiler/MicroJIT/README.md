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

MicroJIT is a lightweight template-based Just-In-Time (JIT) compiler that integrates
seamlessly with TR. MicroJIT is designed to be used either in-place-of TR
(where CPU resources are scarce) or to reduce the time between start-up and
TR's first compile (where CPU resources are not a constraint).

MicroJIT's entry point is at the `wrapped compile` stage of TR's compile process.
When building with a `--enable-microjit` configuration, the VM checks whether
a method should be compiled with MicroJIT or TR before handing off the method to
code generation ([MicroJIT Compile Stages](Stages.md)).

Being template-based, MicroJIT does not build an intermediate language (IL) the
way that TR does. It instead performs a tight loop over the bytecodes and generates
the patchable assembly for each instance of a JVM Bytecode. This means that porting
MicroJIT to a new platform requires rewriting much of the code generator in that
target platform. The current list of available platforms is available [here](Platforms.md).

MicroJIT uses a stack-based [vISA](vISA.md) for its internal model of how to execute bytecodes.
This maps cleanly onto the JVM Bytecode, so MicroJIT uses the number of stack slots
for JVM data types that the JVM specification requires wherever the target architecture allows.

To keep track of the location of parameters and locals, as well as for creating the
required GCStackAtlas, MicroJIT makes use of [side tables](SideTables.md). These
tables are arrays of structures which represent a row in their table. Using these
tables, and some upper bounding, MicroJIT can allocate the required number of rows
in these tables on the stack rather than the heap, helping MicroJIT compile faster.

The templates MicroJIT uses for its code generation are written in a mix of the
target architecture's assembly (for the NASM assembler) and assembler macros used to
abstract some of the vISA operations away into a domain-specific language ([DSL](DSL.md)).

To try MicroJIT, first build your JVM with a configuration using the `--enable-microjit`
option (see the openj9-openjdk-jdk8 project). Then, once built, use the
`-Xjit:mjitEnabled=1,mjitCount=20` options. You can use other values for `mjitCount` than
20, but peer-reviewed research shows that this value is a good estimation of the best
count for similar template-based JITs on JVM benchmarks.

MicroJIT is an experimental feature. It currently does not compile all methods, and will
fail to compile methods with unsupported bytecodes or calling conventions. For details
on supported bytecodes and calling conventions, see our [supported compilations](support.md)
documentation.

# Topics

- [MicroJIT Compile Stages](Stages.md)
- [Supported Platforms](Platforms.md)
- [vISA](vISA.md)
- [Supporting Side Tables](SideTables.md)
- [Domain-Specific Language](DSL.md)
- [Supported Compilations](support.md)
<hr/>
