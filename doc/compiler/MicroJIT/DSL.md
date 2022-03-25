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

MicroJIT's DSL helps new contributors to avoid making common mistakes
when writing new, or editing existing, templates. It helps developers keep
sizes of data types and number of slots separate from each other conceptually
while also keeping their own footprint small.

# Current Macros

The current existing DSL macros are:
- pop_single_slot
  - Add the size of 1 stack slot to the computation stack pointer
- pop_dual_slot
  - Add the size of 2 stack slots to the computation stack pointer
- push_single_slot
  - Subtract the size of 1 stack slot from the computation stack pointer
- push_dual_slot
  - Subtract the size of 2 stack slots from the computation stack pointer
- _32bit_local_to_rXX_PATCH
  - Move a 32-bit value to an r11d-r15d register from a local array slot
- _64bit_local_to_rXX_PATCH
  - Move a 64-bit value to a 64-bit register from a local array slot
- _32bit_local_from_rXX_PATCH
  - Move a 32-bit value from an r11d-r15d register to a local array slot
- _64bit_local_from_rXX_PATCH
  - Move a 64-bit value from a 64-bit register to a local array slot
- _32bit_slot_stack_to_rXX
  - Move a 64-bit value to an r11-r15 register from the computation stack
- _32bit_slot_stack_to_eXX
  - Move a 32-bit value to an eax-edx register from the computation stack
- _64bit_slot_stack_to_rXX
  - Move a 64-bit value to a 64-bit register from the computation stack
- _32bit_slot_stack_from_rXX
  - Move a 64-bit value from an r11d-r15d register to the computation stack
- _32bit_slot_stack_from_eXX
  - Move a 32-bit value from an eax-edx register to the computation stack
- _64bit_slot_stack_from_rXX
  - Move a 64-bit value from a 64-bit register to the computation stack