<!--
Copyright (c) 2000, 2018 IBM Corp. and others

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

Hot Code Replace (HCR) is a Java Virtual Machine (JVM) execution mode which
aims to allow JVMTI agents to instrument Java code while still maintaining high
performance execution. Under HCR, agents may redefine the implementation of a
Java method in terms of bytecodes, but may not alter the inheritance hierarchy
or shape of classes (no adding or removing fields or methods for example). The
JIT already supported HCR, but enabling HCR support incurred a ~2-3% throughput
performance overhead as well as a compilation time overhead. This meant that
the JVM only enabled support for HCR if it detected agents requiring method
redefinition at JVM startup. Oracle has long supported dynamic agent attach –
they allow agents to be attached at runtime and so need to always run in HCR
mode.

The traditional means of implementing HCR support in the JIT was to add an HCR
guard to every inlined method. The guard would patch if the method it protected
was redefined – this stops us from executing the inlined method body and
instead calls the VM to run the new method implementation. The control-flow
diamond this creates is the reason from the throughput performance overhead
mentioned previously – the cold call on the taken side of the guard causes most
analyses to become very conservative due to the arbitrary nature of the code
the call may run.

Unlike traditional HCR, nextGenHCR implements HCR using on-stack replacement
(OSR). OSR is a technology introduced into the VM and JIT in Java 7.1 and which
has had some limited usage until now. OSR allows JITed code to, optionally,
transition execution back to the VM at well-defined points in the program.
Achieving this transition requires some very complex mechanics as the VM’s
bytecode stack state must be recreated from the values held in registers and on
the stack by the JITed method implementation.

NextGenHCR will transition back to the VM when a redefinition occurs, rather
than simply calling the redefined method. To achieve this, the traditional HCR
implementation runs during inlining, wrapping calls in HCR guards, however,
never merging them with other virtual guards. The OSRGuardInsertion
optimization is performed after inlining and will determine when a HCR guard
can be replaced with a set of OSR guards, based on fear analysis. If the OSR
transitions are possible, the HCR guard will be removed and OSR guards
inserted. Runtime assumptions are added to patch these OSR guards in the event
of a method redefinition, triggering the transition of execution to the VM via
the OSR mechanism.

# Control Options

As this feature relies on HCR and OSR, both will be enabled when in nextGenHCR.
To prevent these from being enabled and to avoid any additional modifications
to the trees, use `-Xjit:disableNextGenHCR`. In the event that `disableOSR` or the
environment variable `DisableHCR` is set, nextGenHCR will disable itself. It
should be noted that if the VM requests HCR when nextGenHCR is disabled, the
traditional implementation will be used. Setting the environment variable
`TR_DisableHCRGuards` will result in nextGenHCR performing as usual, but it will
not insert any OSR guards as there will be no HCR guards to replace. This is a
debug mode to evaluate OSR overhead and it will not correctly implement
redefinitions. There is a similar `TR_DisableOSRGuards` variable, which will
prevent nextGenHCR from removing HCR guards and inserting OSR guards, but will
still include the other OSR and HCR overheads.

Both HCR implementations will be disabled under FSD, as HCR implements a subset
of its functionality. Moreover, nextGenHCR requires VM support with a feature
described by the flag `OSRSafePoint`. If this is not enabled, nextGenHCR will
disable itself. Finally, there are a number of situations where we will
fallback on the traditional HCR implementation to reduce compile time overhead.
These include cold compiles, profiling compiles and DLT.

# OSR Implementation Details
The current OSR is very complex and supports a number of different execution
models. This section aims to inform you about the different OSR structures you
may see in the code, their meaning, and the different modes of OSR operation.

## OSR Points

The most fundamental construct in the OSR implementation is the OSR Point. An
OSR point is a program execution point where the JIT may wish to transition
execution of the method from the JITed code body back to the VM’s bytecode
interpreter loop.

## Involuntary and Voluntary OSR

OSR can operate in two modes: voluntary or involuntary. Under involuntary OSR,
the VM controls when execution will transition from JITed code to the bytecode
interpreter loop – this mode is used to implement Full Speed Debug. In this
mode, any operation which may yield control to the VM may allow the VM to
trigger an OSR transition – we view this as involuntary OSR because the JITed
code does not control when a transition may occur. Under voluntary OSR the
JITed code controls when to trigger an OSR transition to the VM. This mode is
used in a number of ways by the JIT, but most notably it is the mode used to
implement nextGenHCR. To limit the number of points where OSR transitions may
occur under nextGenHCR, method redefinitions are only permitted at a subset of
yield points. These include calls, asyncchecks and monitor enters. There is
also an implicit OSR point in the method prologue due to the stack overflow. 

## Pre and Post Execution OSR

In the past, OSR has always been implemented as transitioning before the OSR
point. For example, if a monitor enter was identified as an OSR point, we
execute up to that point and reconstruct the state such that the VM's
interpreter can begin execution of the monitor enter. Under post execution OSR,
the transition occurs after the OSR point has been evaluated in JITed code.
This is achieved by offsetting the desired transition bytecode index by the
size of the OSR point’s opcode. NextGenHCR is implemented using post execution
OSR, such that OSR guards are taken after the yield to the VM. A significant
concern with post execution OSR is that the transition may be delayed until
after a series of pending push slots, such as storing a call result, to ensure
the stack is correctly represented.

## Induction and Analysis OSR

There are two types of OSR points: 

* Induction points: OSR points that can transition to the VM
* Analysis points: OSR points that cannot transition to the VM, exist only to hold OSR data

Under pre execution OSR, all OSR points are induction points. The additional
complexity of the two types is due to post execution OSR. OSR points located
after yields are induction points, as a transition is expected there. However,
if the transition is within an inlined call, the state in the outer frames must
be known to successfully transition the full call stack. Therefore, analysis
points are placed before calls to hold the state at those bytecode indices,
without allowing for a transition to occur there.

## OSR Blocks

In nextGenHCR there are three types of OSR blocks: OSR induce blocks, OSR catch
block and OSR code blocks. Every call site that can OSR has one catch block and
code block. The catch block will remain empty, whilst the code block contains a
call to `prepareForOSR`. This call identifies all symrefs that may be required
for the frame to transition. As these two blocks apply for any possible
transition points in a method, the OSR induce block allows bookkeeping to be
specified at an individual OSR point. It contains `irrelevant` stores, which
store default values to symrefs. These ensure only those symrefs that are live
at an OSR point appear so to other optimizations. Moreover, the block
potentially contains remat of live symrefs and monitor enters, to further
reduce the constraints on other optimizations. These blocks end with an OSR
induce, which transitions to the VM, and an exception edge to the OSR catch
block. Once the OSR induce is called, we transition to the VM. The VM will then
create an OSR buffer, based on the size of the stack and the number of autos it
expects at the induce’s bytecode index. It will then execute the
`prepareForOSR`, which will fill the buffer based on the JIT’s stack.
 
## OSR Bookkeeping

To achieve these transitions, OSR bookkeeping is required at all possible OSR
points until OSRGuardInsertion is performed. The most significant difference
here is the addition of stores to pending push temps, in order to inform the
OSR infrastructure of the stack state. This has already confused a number of
optimizations, particularly StringBuilderTransformer and StringPeepholes.
Optimizations during ILGen opts will attempt to eliminate these as much as
possible. Moreover, all blocks with OSR points will have exception edges to
their OSR catch block, which must be maintained to ensure the necessary symrefs
are retained. Once OSRGuardInsertion and OSRExceptionEdgeRemoval are performed,
exception edges to OSR catch blocks will be limited to the OSR induce blocks
and the creation of OSR guards can no longer be performed.
 
## OSR Guards

OSR guards are placed after OSR points and behave similarly to HCR guards. They
are permanently patched during a yield, if a redefinition occurs. An important
distinction is that all OSR guards will be patched in the event of any
redefinition, rather than just the required set to protect a removed HCR guard.
This is due to the high overhead of calculating these sets for individual HCR
guards. The guard will not necessarily be placed immediately after the OSR
point, rather several stores may be located between the two. These will include
stores to pending push slots, to determine the stack at the transition point,
and stores of call or allocation results, as these are used for remat.

The taken edge of an OSR guard will look like:

```
OSRGuard -> OSRInduceBlock - - -> OSRCatchBlock -> OSRCodeBlock -> [OSRCodeBlock for each call above it] -> Exit
```

## Cannot Attempt OSR

If the JIT executes an operation via intermediate states that differ from the
VM, then we cannot transition during that execution since the semantics of what
the JIT has done cannot be expressed in terms of the semantics the VM would
have executed. This is evident in the JIT's implementation of MethodHandles.
These bytecode indices are marked as `cannotAttemptOSR` during ILGen and will not
have OSR infrastructure generated for them or any methods inlined within them.
All other unmarked bytecode indices in ILGen where one of nextGenHCR’s OSR
points may be generated must correctly represent the VM’s stack with stores to
pending push slots.
