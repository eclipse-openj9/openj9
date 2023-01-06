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

MicroJIT uses a stack-based virtual ISA. This means values are pushed onto
the computation stack, popped off the computation stack, saved and
loaded from a local array, and that operands for a given instruction
are either in the instruction stream, or on the computation stack.

MicroJIT is implemented on register-based architectures, and makes use
of those registers as pointers onto the stack and temporary holding
cells for values either being used by an operation, or being saved
and loaded to and from the local array. Below are the mappings for
the supported architectures.

# AMD64
| Register | Description                                                                                      |
|:--------:|:------------------------------------------------------------------------------------------------:|
| rsp      | The stack extent of the computation stack pointer                                                |
| r10      | The computation stack pointer                                                                    |
| r11      | Storing the accumulator or pointer to an object                                                  |
| r12      | Storing values that act on the accumulator, are to be written to, or have been read from a field |
| r13      | Holds addresses for absolute addressing                                                          |
| r14      | The pointer to the start of the local array                                                      |
| r15      | Values loaded from memory for storing on the stack or in the local array                         |