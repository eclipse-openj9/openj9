<!--
Copyright (c) 2020, 2020 IBM Corp. and others

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

This document describes the calling conventions for the JIT Private 
Linkage. The System Linkage info can be found in the
[Eclipse OMR Project](https://github.com/eclipse/omr/blob/master/doc/diagnostics/CallingConventions.md).

# x86

## x86-64

|Register|JIT Private Linkage|Caller Preserved<sup>1</sup>|
|--|--|--|
|RAX||:heavy_check_mark:|
|RBX|||
|RCX|4th int argument |:heavy_check_mark:|
|RDX|3rd int argument |:heavy_check_mark:|
|RSP|stack pointer ||
|RBP|current vm thread ||
|RSI|2nd int argument |:heavy_check_mark:|
|RDI||:heavy_check_mark:|
|R8||:heavy_check_mark:|
|R9|||
|R10||:heavy_check_mark:|
|R11||:heavy_check_mark:|
|R12||:heavy_check_mark:|
|R13||:heavy_check_mark:|
|R14||:heavy_check_mark:|
|R15||:heavy_check_mark:|
|XMM0|1st float argument; float return value |:heavy_check_mark:|
|XMM1|2nd float argument |:heavy_check_mark:|
|XMM2|3rd float argument |:heavy_check_mark:|
|XMM3|4th float argument |:heavy_check_mark:|
|XMM4|5th float argument |:heavy_check_mark:|
|XMM5|6th float argument |:heavy_check_mark:|
|XMM6|7th float argument |:heavy_check_mark:|
|XMM7|8th float argument |:heavy_check_mark:|
|XMM8|||
|XMM9|||
|XMM10|||
|XMM11|||
|XMM12|||
|XMM13|||
|XMM14|||
|XMM15|||

<hr/>

1. The remaining registers are Callee Preserved.

## IA32

The IA32 calling convention in the private linkage pushes all arguments on to the 
stack in left-to-right order, i.e. in the case of an indirect call, the receiver 
object is furthest away from the top of the Java stack. The callee is responsible 
for cleaning up the pushed arguments. The return value is placed in EAX, or 
EDX:EAX for a long value. If SSE2 support is not available, the x87 floating-point 
stack is used for passing floating-point arguments and returning floating-point 
results. Otherwise, XMM registers are used.

# POWER

Scratch registers are not preserved across calls, while non-volatile registers are preserved by called functions.

|Register|JIT Private linkage|
|--|--|
|R0|Scratch|
|R1|Untouched except for JNI setup|
|R2|Scratch; Preserved in 32-bit Linux|
|R3|1st argument/return value|
|R4|2nd argument/low-order portion of 64-bit return values in 32-bit mode|
|R5-10|3rd-8th arguments; the 9th and above arguments are passed on the stack|
|R11|Scratch (frequently used as a temp for call target address)|
|R12|Scratch|
|R13|vmThread (32-bit); OS dedicated (64-bit)|
|R14|Java stack pointer|
|R15|Non-volatile (32-bit); vmThread (64-bit)|
|R16|Non-volatile (32-bit); JIT pseudo-TOC (64-bit)|
|R17-31|Non-volatile|
|IAR|Instruction Address Register (a.k.a PC or NIP on Linux)|
|LR|Link Register (used to pass the return address to a caller)|
|CTR|Count Register (used for calling a far/variable target)|
|VSR32-63|Scratch|
|CR0|Condition Register (used by compare, branch and record-form instructions), scratch|
|CR1-7|Condition Register (used by compare and branch instructions), scratch|
|FP0|1st Floating Point argument / Floating point return value|
|FP1-7|2nd-8th Floating Point argument|
|FP8-13|Scratch|
|FP14-31|Scratch|

# Z

## zLinux
|Register|JIT Private Linkage<sup>1</sup>|Callee Preserved|
|--|--|--|
|R0|||
|R1|parameter||
|R2|parameter/return value||
|R3|parameter/return value (long in 31-bit mode)||
|R4|entry point||
|R5|Java stack pointer |:heavy_check_mark:|
|R6|literal pool (unless dynamically optimized away) |:heavy_check_mark:|
|R7|extended code base |:heavy_check_mark:|
|R8-R11||:heavy_check_mark:|
|R12||:heavy_check_mark:|
|R13|vmThread |:heavy_check_mark:|
|R14|return address |:heavy_check_mark:|
|R15|system stack pointer |:heavy_check_mark:|
|F0|parameter/return value||
|F1|||
|F2|parameter||
|F3|||
|F4|parameter||
|F5|||
|F6|parameter||
|F7|||
|F8-F15||:heavy_check_mark:|

## z/OS

|Register|JIT Private Linkage<sup>1</sup>|Callee Preserved|
|--|--|--|
|R0|||
|R1|parameter||
|R2|parameter/return value||
|R3|parameter/return value (long in 31-bit mode)||
|R4|system stack pointer |:heavy_check_mark:|
|R5|Java stack pointer |:heavy_check_mark:|
|R6|literal pool (unless dynamically optimized away) |:heavy_check_mark:|
|R7|extended code base |:heavy_check_mark:|
|R8-R11||:heavy_check_mark:|
|R12||:heavy_check_mark:|
|R13|vmThread |:heavy_check_mark:|
|R14|return address |:heavy_check_mark:|
|R15|entry point||
|F0|parameter/return value||
|F1|||
|F2|parameter||
|F3|||
|F4|parameter||
|F5|||
|F6|parameter||
|F7|||
|F8-F15||:heavy_check_mark:|

<hr/>

1. The JIT linkage uses the following conventions for high-word registers (HPRs) z196 support is enabled:
    * On 31-bit platforms, all HPRs are volatile.
    * On 64-bit platforms, HPR6 through HPR12 are preserved, and all other HPRs are volatile.

# 32-bit Arm

Scratch registers are not preserved across calls, while non-volatile registers are preserved by called functions.
Usage of the VFP registers in this table assumes hard-float ABI.

|Register|JIT Private Linkage|
|--|--|
|R0|1st argument / return value|
|R1|2nd argument / return value|
|R2-3|3rd-4th arguments|
|R4-5|Scratch|
|R6|Java BP|
|R7|Java stack pointer|
|R8|vmThread|
|R9-10|Non-volatile|
|R11|Scratch|
|R12|Untouched|
|R13|Untouched except for JNI setup|
|R14|Link Register (LR)|
|R15|Program Counter (PC)|
|D0-15|Scratch<sup>1</sup>|
|D16-D31|Untouched<sup>2</sup>|

<hr/>

1. Floating point arguments and return value are passed in GPRs in JIT Private Linkage.
2. Unavailable in some VFP versions


# AArch64 (64-bit Arm)

Scratch registers are not preserved across calls, while non-volatile registers are preserved by called functions.

|Register|JIT Private Linkage|
|--|--|
|R0|1st argument / return value|
|R1-7|2nd-8th arguments|
|R8-15|Scratch|
|R16-17|Untouched|
|R18|Scratch|
|R19|vmThread|
|R20|Java stack pointer|
|R21-28|Non-volatile|
|R29|Untouched|
|R30|Link Register (LR)|
|SP|Untouched except for JNI setup|
|PC|Program Counter (PC)|
|V0|1st floating point argument / floating point return value|
|V1-7|2nd-8th floating point arguments|
|V8-31|Scratch|
