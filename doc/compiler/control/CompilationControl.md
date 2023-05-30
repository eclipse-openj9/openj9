<!--
Copyright IBM Corp. and others 2022

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

# Overview

Compilation Control refers to the infrastructure and heuristics
associated with the process of coordinating compilations.

# Compilation lifecycle

1. A method is slated for compilation.
2. A compilation entry is added to the [compilation queue](#compilation-queue).
3. A [Compilation Thread](#compilation-threads) picks up the
   entry and starts the compilation. By default the compilation
   is performed asynchronously; i.e., the Application Thread does
   not wait for the result of the compilation.
4. The compiled body is stored into the [Code Cache](#code-and-data-cache-management).
5. The method is updated to execute the newly compiled body
   for subsequent invocations.

# Components of Compilation Control

## Compilation Triggers

There are two main types of compilation: First Time Compilation and
Recompilation. As the name suggests, First Time Compilations refer
to compilations of methods that have only been executed in the
interpreter. Recompilations refer to methods that are recompiled,
generally at a higher optimization level, to further improve
performance.

### Invocation Counts

The JIT sets an invocation count for every method in every class
that is loaded by the VM. There are three types of invocation
counts:

* `scount`: Number of invocations before the method is loaded from the [SCC](#shared-classes-cache).
* `bcount`: Number of invocations before a method with loops is compiled.
* `count`: Number of invocations before a method without loops is compiled.

If a compiled method body exists in the SCC, the `scount` is used for
the invocation count for the method. Otherwise, `count` is used if the
method has no loops and `bcount` is used if the method does have loops.

This occurs in the [JIT hook](#jit-hooks)
`jitHookInitializeSendTarget`. The count is stored in the `extra`
field of the `J9Method`. The least significant bit (LSB) of this
field is set if the method is interpreted. As such the count is encoded
by left-shifting 1 bit. Conversely, in order to decode the invocation
count, the value in `extra` has to be right-shifted 1 bit. It is worth
noting that JNI methods do not undergo this process. The various
counts can be controlled by setting `count=`, `bcount=`, `scount=`
in the `-Xjit` and/or `-Xaot` options.

When the method is invoked, the VM decrements the count in the
`extra` field. When the count hits zero, the VM notifies the
JIT to schedule a compilation via a callback registered in the
`J9JITConfig` struct. Once the compilation succeeds,
the `extra` field is updated with the address of the Start PC of
the compiled body. Because the `extra` field's LSB is used to test
whether the method is interpreted or compiled, the Start PC of a
compiled method body is always at least 2-byte aligned.

It is also worth noting that
[Async Checkpoint Sampling](#async-checkpoint-sampling)
can also result in artificially decrementing the invocation count
to cause the method to get compiled sooner if it is determined
that the method is important for performance.

### Recompilation

Recompilations are triggered in a multitude of ways; see the
[Recompilation doc](../runtime/Recompilation.md) for more details.

### Dynamic Loop Transfer

Dynamic Loop Transfer (DLT) is a feature in OpenJ9 that allows
a thread executing in the middle of an interpreted method to
switch to the middle of a compiled method body and continue
execution. This feature is used for methods that spend a lot of
time in the interpreter but are not invoked many times; this
happens when executing a long running loop.

The JVM maintains a DLT buffer per Java Thread. Using
[Async Checkpoint Sampling](#async-checkpoint-sampling),
if the JIT determines that a method is a long running
interpreted method that would benefit from DLT, the method is
queued for both a DLT and regular compilation. On a successful
DLT compilation, the VM transitions the thread to execute the
compiled version of the method.

It is worth noting that a DLT compiled method body can only be
transitioned to from a specific Bytecode Offset. It is for this
reason that a regular compilation is also requested along with
the DLT compilation; the former ensures that new invocations
of the method do not have to run interpreted until it reaches
the Bytecode PC that permits the transition to the DLT compiled
method body.


## Compilation Queue

The Compilation Queue is a weighted queue of compilation
entries. The weights represent the priority of a compilation;
in general, the cheaper a compilation is expected to be, the
higher its priority. The queue is synchronized using the
[Compilation Monitor](#compilation-monitor).

### Low Priority Queue

The JIT also maintains a Low Priority Queue (LPQ). This is
used to compile methods that are not immediately needed for
performance, but would benefit the application in the long
run. Entries from the LPQ are only picked up by Compilation
Threads when the main Compilation Queue is empty.


## Compilation Threads

Compilations are performed on dedicated threads known as Compilation
Threads. These threads are created at JVM startup, and are
suspended until there is work to be done. The exact number of
threads created depends on the CPU resources available to the JVM as
well as the [`-Xcompilationthreads`](https://www.eclipse.org/openj9/docs/xcompilationthreads/)
option. The number of active threads depends on the size of the
Compilation Queue.

In general, the Compilation Threads are synchronized using
the [Compilation Monitor](#compilation-monitor). However, each
thread also has a Compilation Thread Monitor. This monitor is
used to synchronize per thread information, such as its State,
as well as to suspend and resume as needed. It is also used for
synchronization between a Compilation Thread and an Application Thread
waiting for a compilation result from said Compilation Thread; this
can occur when the JIT needs to perform a Synchronous Compilation
(either because of a
[Mandatory Recompilation](../runtime/Recompilation.md#mandatory-recompilation)
or because `-Xjit:disableAsyncCompilation` was specified).


## Compilation Monitor

The Compilation Monitor is a global monitor that is used to
synchronize many aspects of the compilation infrastructure.


## Sampler Thread

The Sampler Thread can be thought of as a daemon within the JIT. It
performs several checks periodically. Most notably, it is responsible
for triggering sampling via Async Checkpoint Sampling.

### Async Checkpoint Sampling

Async Checkpoints, also known as Safe Points and Yield Points, are
well defined points during a Java Thread's execution wherein the
thread yields to the VM for the purpose of doing GC or any other
task that requires the thread to be stopped. Async Checkpoint Sampling
piggybacks off this infrastructure to obtain sampling information
that is used to determine which methods should be compiled and
at what optimization level.

This is the general procedure:

1. Every 10ms, the Sampler Thread sets a flag in the J9VMThead of all
   active Java Threads.
2. When an Application Thread reaches an Async Checkpoint, it checks to
   see whether the flag has been set, and if so, calls a JIT hook.
3. The hook collects a bunch of information, including the Java Methods
   on the stack.

Async Checkpoint Sampling is used for [DLT](#dynamic-loop-transfer),
[Recompilation](#recompilation), and printing tracepoints
containing information about the methods currently on the Java
Thread's stack.


## Code And Data Cache Management

There unfortunately exists a bit of name overloading. When speaking
generally about a place to store compiled bodies, the term "Code Cache"
is used. However, the actual chunk of memory is known as the "Code
Repository", and individual subsections of the Code Repository are
called "Code Caches". By default, the JIT allocates a 256 MiB Code
Repository. This is then carved into 2 MiB Code Caches. The Data
Caches are allocated using standard `allocateMemorySegment` and
`freeMemorySegment` APIs accessed via
`javaVM->internalVMFunctions`.

The compiled bodies are stored into the Code Repository; data related
to the compiled body is stored in Data Caches. An example of such data
is the `J9JITExceptionTable` (also `typedef`'d as `TR_MetaData`) that
is used to describe the body (e.g. start and end address, optimization
level, etc.). Space in the Code Repository and Data Caches have to be
allocated on an as-needed basis. Additionally, these allocations have
to be synchronized since multiple Compilation Threads can be active
simultaneously. As such, each Compilation Thread, as part of the
compilation, will reserve a Code Cache at the beginning of the codegen
phase to ensure that no other thread can write to it. If performing an
AOT or Remote compilation, it also reserves a Data Cache at the start
of a compilation to ensure that the memory allocated is contiguous.


### Trampolines

A trampoline is essentially a snippet of code that is used to branch to
a location far from the current Instruction Pointer. Each Code Cache
has a section for storing Trampolines so that methods in a
Code Cache can share Trampolines. The Code section grows towards
increasing addresses, while the Trampoline section grows towards
decreasing addresses.

The Trampoline section is not reserved if the JVM can guarantee that far
jumps will never be needed. The Code Repository is fundamental to
providing this guarantee by ensuring that Code Caches are close together
and obviate the need for far jumps. However, platforms such as POWER
where the maximum distance of the branch target is much shorter than
platforms such as x86 may still need to use Trampolines.

### Code Cache Reclamation

When a method is recompiled, the old body eventually stops being used,
which is a waste of space in the Code Cache. In order to maximize the
usable space in the Code Cache, the JIT reclaims stale bodies; see the
[Code Cache Reclamation doc](../runtime/CodeCacheReclamation.md) for
more details.


# Influencing Factors

There are many factors that influence the decisions the Control
Infrastructure makes. The JIT has to improve application
performance without taking away too many resources from the
application. The following are some of the considerations that have
the biggest impact and influence.

## JIT State

The JIT maintains several states that reflect the various execution
characteristics of an application. The Compilation Control heuristics
depend on the state, or phase, the JIT is in. The JIT transitions
between the following states:

- IDLE_STATE
  - Decisions focus on minimizing Compilation Thread CPU usage.
- STARTUP_STATE
  - Decisions focus on making Startup as fast as possible. Startup
    refers to the period of time between the start of the JVM and
    the point at which the Java Application is ready for load.
- RAMPUP_STATE
  - Decisions focus on making Rampup as fast as possible. Rampup
    refers to the period of time between the end of Startup and the
    point at which the Java Application is running at peak throughput.
- STEADY_STATE
  - Decisions focus on maintaining throughput. When the application
    runs at peak throughput, it is said to be in Steady State.
- DEEPSTEADY_STATE
  - Decisions focus on maintaining throughput while minimizing CPU
    consumed by the JIT (including the Sampler Thread).

## Available Resources

The JIT should minimize its impact on application performance.
Therefore, how aggressively it can compile methods depends on
the resources available to the JVM. Multiple compilation threads may
only be activated if the system is not resource constrained.
To minimize memory usage, only one higher optimization level
(hot, very-hot, scorching) compilation is performed at a time.
Additionally, the maximum amount of memory available for a compilation
depends on the amount of physical memory available.

## JIT Hooks

The JIT registers several hooks with the VM. These hooks are invoked
when the environment changes at well defined points. These changes
include Class Loading / Unloading, Code Redefinition, Garbage
Collection, etc. These events can cause invalidations both in the
generated code and the compilation. The former is addressed using
[Runtime Assumptions](../runtime/RuntimeAssumption.md); the latter
is addressed by aborting the compilation and possibly reattempting
it.

## Shared Classes Cache

The existence of the Shared Classes Cache (SCC) has a significant
influence on the heuristics; this is because the SCC allows for
Ahead of Time (AOT) Compilation. See the [AOT docs](../aot) for
technical details on the AOT feature.

Because AOT compilations enable very fast startup, the JIT tries
to maximize the number of compiled bodies it can load from the SCC
(referred to as AOT Load). As a consequence, if the SCC exists,
the invocation counts set on methods are lower than in the
configuration where the SCC does not exist. However, it is worth
noting that this is a global change; it is not known at the point
when invocation counts are set whether the method can or will be
AOT Compiled. Finally, the invocation count for an AOT Load
is even lower - by default it is 20.

By their very nature, AOT method bodies have to be relocatable,
that is, they cannot be tailored to only run in the current JVM
instance. As a consequence, the quality of AOT compiled code is,
in general, less than that of JIT compiled code. Therefore, the JIT
uses AOT to speed up the early phase of the application, namely
Startup, and then depends on recompilation to achieve peak Steady
State performance. The idea is that the set of methods that need
to be recompiled for peak Steady State performance is smaller than
the total number of methods in the application. Therefore, the JIT
consumes less memory and CPU.

## JIT Server

JIT Server is a feature in OpenJ9 that allows clients to offload
compilations to a server; see the [JIT Server docs](../jitserver)
for more details.

## Special Modes

OpenJ9 has a few special flags that the user can specify to indicate
to the JIT the kind of behaviour desired. This section outlines
these modes.

### -Xtune:virtualized

[-Xtune:virtualized](https://www.eclipse.org/openj9/docs/xtunevirtualized/)
emphasizes fast Startup and Rampup, and low CPU utilization,
at the cost of a small throughput degradation. It is designed for
use in virtualized environments.

### -Xtune:throughput

This mode emphasizes maximizing throughput performance at the expense
of other performance metrics like start-up time, footprint and CPU
consumption.

### -Xquickstart

[-Xquickstart](https://www.eclipse.org/openj9/docs/xquickstart/)
emphasizes quick start-up of applications. It is designed
for Java developers.
