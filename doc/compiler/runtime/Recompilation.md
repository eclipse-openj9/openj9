<!--
Copyright (c) 2019, 2020 IBM Corp. and others

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

# Background

A natural extension of a JIT Compiler's ability to compile methods "just 
in time" is to *re*compile methods "just in time". There are several 
circumstances when a method might be recompiled; these are outlined 
in this doc.

# Reasons for recompilation

This section outlines the various reasons for the
recompilation of a method. Recompilation can either be optional or
mandatory. However, since any compilation is technically not
mandatory for functional correctness (it exists for performance
only), both *optional* and *mandatory* in the previous sentence should
be suffixed with "in order to continue executing the existing compiled
version of the method".

## Optional Recompilation

The following trigger a recompilation that is optional in the sense that
it is not functionally incorrect to continue executing the existing JIT'd 
body but there is a performance benefit to executing a more optimized body. 

### Asynchronous Triggers

This section outlines mechanisms that are triggered asynchronously with
respect to the java application threads.

#### Sampling

The JIT has a Sampler Thread that is used to periodically perform tasks. One
of these tasks is to set a flag on all Java Threads to yield at the next 
yield point. Once the threads enter the yield point, they temporarily stop
executing java code, and instead execute a JIT callback (see 
`jitMethodSampleInterrupt` in `HookedByTheJit.cpp`). This callback is used
to build a window of samples, which is used to determine whether or not a 
method should be recompiled.

The JIT also has support for Hardware Profiling. Hardware is used to collect
profiling information and put it in buffers. A separate HWProfiler Thread
periodically processes the buffers and determines whether or not a method
should be recompiled.

### Synchronous Triggers

This section outlines mechanisms that are triggered synchronously with respect
to java application threads; i.e. some application thread has to stop running
java code in order to trigger a recompilation.

#### Counting

This mechanism refers to the notion of triggering a recompilation by counting
the number of times the method is invoked. Guarded Counting Recompilation (GCR)
is an example of this, wherein the compiler will generate IL Trees at the start 
of a method to decrement a counter when the method is invoked, and to trigger
a recompilation when the counter hits 0. A "Counting Body" is another example
which is similar in principle to GCR, but has a different pre-prologue. 

#### Explict

This section refers to mechanisms that trigger recompilation explicitly, i.e.
not based on counting or sampling, but via some other criteria. The features
in OpenJ9 that might explicitly trigger recompilation are:
* Exception Driven Optimization (EDO)
* JProfiling
* On Stack Replacement (OSR)

To use OSR as an example, the OSR mechanism may force execution to transition
to the interpreter because an optimized path in the generated code is no longer
valid to run. As part of this transition, the mechanism might also trigger a
recompilation if this path contributed to a significant portion of the 
performance provided by the method - i.e. it is better to recompile the
method than leave it as is.

## Mandatory Recompilation

The following trigger a recompilation that is mandatory in the sense
that 1. it **is** functionally incorrect to execute the existing JIT'd body,
and 2. in order to (at the very least) maintain the performance provided by 
the previously valid compiled code.

### Invalidation 

Invalidation refers to circumstance when assumptions made about the
environment while compiling a method are no longer valid. Therefore, at the 
very least, the old compiled body has to be patched to prevent any new 
invocations from executing it. However, given that method was invoked enough 
to warrant a compilation, it stands to reason that compiling the new 
semantics will also yield performance benefits.

An example that would cause invalidation is if an attached JVMTI agent 
modifies the bytecodes of a method, completely, changing its semantics.
Another example would be Pre-Existence. Pre-Existence is when a method is
compiled with the assumption that the class of an object that is passed in
has not been extended; when the assumption no longer holds, the method is 
invalid.

# Recompilation process

The general recompilation process is
1. Trigger Recompilation
2. Update code to execute new body

However, the details vary depending on how the recompilation is triggered.

## Optional Recompilation

In the case of an Optional Recompilation, once the compilation has finished,
the Start PC of the current body is patched to jump to a point in the 
pre-prologue, which jumps to a helper. When this method is next invoked, the
jump to the helper is executed, which first patches the call
instruction in the caller method to call the new body,
and then jumps to the new body (preserving the stack, so that
control will transfer back to the caller on a return instruction). 

## Mandatory Recompilation

In the case of a Mandatory Recompilation, when an invalidation event occurs
that requires the recompilation of a method, the Start PC of the current body
is patched to jump to a point in the pre-prologue, which hooks into the 
compiler infrastructure to trigger a synchronous compilation. Thus, all threads
that invoke this method will wait until the compilation is done. Once completed,
the threads are resumed and start executing the new body. Additionally, the same
process in an Optional Recompilation occurs, namely patching the Start PC to 
ensure all call sites are updated to call the new body.


# Sampling Body vs Counting Body

There is the notion of a "Sampling Body" and a "Counting Body" in the OpenJ9 
Compiler. This refers to the shape of the pre-prologue. The names date back to
when the compiler only had two triggers of recompilation, namely 1. via the
Sampling Thread - "Sampling Body" - or 2. to recompile a Profiled Compilation - 
"Counting Body". Currently, however, there are many different triggers for 
recompilation, and hence the names that refer to the shape of the pre-prologue
are now misnomers: a "Counting Body" still refers to the shape of the pre-prologue 
generated in a Profiled Compilation, whereas a "Sampling Body" refers to the
shape of the pre-prologue generated in all other compilations, even if it was 
an Explict or GCR triggered compilation.
