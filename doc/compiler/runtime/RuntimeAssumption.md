<!--
Copyright (c) 2000, 2019 IBM Corp. and others

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

**Runtime assumption framework** is a key component of the Testarossa JIT 
compiler technology. It allows the optimizer to speculate what the mostly 
likely cases will be and generate faster code accordingly at compile time, 
while still maintaining functional correctness if any assumption is 
violated during run time.

The **lifetime of a runtime assumption** can be viewed in *three* stages:

1. The JIT optimizer makes assumptions on the state of a language runtime 
during method compilation and manages a set of internal data structures to 
keep track of the assumed runtime states.
2. If the assumptions remain valid at the end of method compilation, they 
are then saved into a *runtime assumption table* which persists beyond method 
compilation and into method execution. Some of the information in the data 
structures used in the previous stage also need to be saved into persistent 
memory so they can be accessed beyond method compilation.
3. If/When the assumed language runtime states change and assumptions are 
violated during method execution, the language runtime informs the runtime 
assumption table to notify the affected assumptions so they can perform 
necessary work to ensure correct program execution onward.

Please note that when a method compilation ends and the compiled method body 
is about to be committed to use, the language runtime will need to do final 
checks on its set of runtime assumptions. If some are violated during 
compilation, the language runtime can choose to perform work for those 
assumptions to maintain program correctness or fail the compilation. This is 
critical since no notifications about invalidation events will be given 
during method compilation -- only once a method is committed to the runtime 
and hooked up into its infrastructure.

The work every runtime assumption must perform when the assumption it holds 
is no longer true is called **compensation**. Compensation in Testarossa 
can be grouped into two main categories:

1. *Location redirection*: Patching at a particular location to unconditionally 
jump to a destination;
2. *Value modification*: Patching at a particular location to change its value 
to another value.

How a language runtime manages its assumed states during method compilation 
and method execution is up to the language runtime developers.

Removing runtime assumptions from the *runtime assumption table* is required in 
the following cases:

1. When a JIT body is being reclaimed because its class is being unloaded during 
a GC cycle.
2. When a JIT body is being partially reclaimed during a GC cycle because a 
recompile has occurred which replaces a previous JIT body for a method.
3. When a JIT compile has been aborted after registering runtime assumptions.

In cases where runtime assumption reclaiming is done during a gc cycle, it's very
important that we minimize the processing time so that we don't unduly extend
the GC pause times.

The Runtime Assumption Table (RAT) employs a mark-sweep style of clean-up. When an
assumption needs to be removed from the table because it is no longer being used
or is no longer valid, a bit is set on the assumption. This bit means that all
searches of the RAT via the public APIs will never return a pointer to the 
'dead' assumption. Internally, the assumption remains in the table's linked lists.

At the start of every GC cycle, the JIT will allow the Runtime Assumption Table 
to clean-up a fixed number of entries. This is done by walking the buckets in 
the table and checking if the bucket has been recorded as containing dead entries.
Once a bucket with dead entries is identified we walk the linked list looking for
dead assumptions. When one is found we check if the assumption is currently in a
JIT'd body circular linked list. If it is, a walk of that linked list is done
removing all dead entries from that list. Once completed the dead assumption is
removed from the bucket linked list.

This staged approach allows us to amortize the clean-up of the assumptions over
time and minimizes the amount of clean-up done during any single period where
execution is stopped. Prior to the conversion to lazy removal you could have 
long pauses when the assumption table had to eagerly clean up all of the dead
assumptions.

Currently there are two methods for removing runtime assumptions. The first is to
remove assumptions one at a time which can be costly if many assumptions are being 
removed in sequence during a GC cycle. The second method is to mark a set of 
assumptions that are going to be removed and then do a single scan of the runtime 
assumption table to remove all the marked assumptions in one pass.

---

For J9, there is an important distinction between true runtime assumptions and
stop-the-world runtime assumptions. A true runtime assumption must be able to 
be patched while the application threads are executing (possibly even 
executing the instructions around the patch point). This means that the 
patching operations (whether it is patching to a location or modifying a 
value) must be atomic in the context of an executing instruction which 
generally imposes very strict patchability requirements. The stop-the-world 
assumptions are much less strict. The assumption in the JIT will be that 
any these guards can only be tripped at treetops that return true to 
canGCAndReturn or canGCAndExecpt, and that while at one of these trees and 
execution is halted guards may be patched. This is a much looser patching 
requirement (you can overwrite multiple instructions for example on x86). 
Currently HCR assumptions and OSR assumptions are considered 
stop-the-world for patching purposes and all others are considered to be 
true runtime assumptions.
