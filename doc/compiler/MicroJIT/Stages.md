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

This document contains information on the compilation stages of MicroJIT
from where to diverges from TR.

# MetaData

At the entry point to MicroJIT (the `mjit` method of `TR::CompilationInfoPerThreadBase`),
MicroJIT enters the first stage of compilation, building MetaData. It parses the
method signature to learn the types of its parameters, and bytecodes to learn the
number and type of its locals. It then estimates an upperbound for how much space
the compiled method will need and requests it from the allocator.

During this stage, it also learns the amount of space to save on the stack for calling
child methods, and will eventually also create a control flow graph here to perform
profiling code generation later. The code generator object is constructed and the next
steps begin.

# Pre-prologue

The pre-prologue is generated first. This contains many useful code snippits, jump points,
and small bits of meta-data used throughout the JVM that are relative to an individual
compilation of a method.

# Prologue

The prologue is then generated. Copying paramters into the appropreate places and updating
tables as needed. Once these tables are created, the structures used by the JVM are created,
namely the `GCStackAtlas` and its initial GC Maps. At this point, the required size of the
stack is known, the required size of the local array is known, and the code generator can
generate the code for allocating the stack space needed, moving the parameters into their
local array slots, and setting the Computation Stack and Local Array pointers (See [vISA](vISA.md)).

# Body

The code generator then itterates in a tight loop over the bytecode intruction stream,
generating the required template for each bytecode, one at a time, and patching them as
needed. In this phase, if a bytecode that is unsupported (See [supported](support.md)
is reached, the code generator bubbles the error up to the top level and compilation
is marked as a failure. The method is set to not be attempted with MicroJIT again, and
its compilation threshold is set to the default TR threshold.

# Cold Area

The snippets required by MicroJIT are generated last. These snippets can serve a
variety of purposes, but are usually cold code not expected to be executed on every
execution of the method.

# Clean up

The remaining meta-data structures are created, MicroJIT updates the internal counts
for compilations, and the unused space allocated for compilation in the start is reclaimed.