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

Currently there are two methods for removing runtime assumptions. The first is to
remove assumptions one at a time which can be costly if many assumptions are being 
removed in sequence during a GC cycle. The second method is to mark a set of 
assumptions that are going to be removed and then do a single scan of the runtime 
assumption table to remove all the marked assumptions in one pass.

**Implementation details** for the marking runtime assumption reclaiming method:

An array of bools is created, one element in the array for each assumption "kind"
in the *runtime assumption table* (RAT). The bool is set to "true" when at least 
one assumption of that "kind" has been marked. In the Hashtable of assumptions 
(there is one hashtable for each assumption "kind") an array of 32bit integers is 
created that matches the number of buckets in the hashtable. This array of 
integers is used to count the number of marked assumptions in each hashtable 
bucket.

When an assumption is marked *markForDetachFromRAT()* the bool array and a 
hashtable integer array is updated. Marking is implemented as a low-order bit 
being set on the "next" pointer *_nextAssumptionForSameJittedBody* and therefore 
it's important that the assumption is detached from the JIT body linked-list when 
marking happens. The routine *markAssumptionsAndDetach()* is responsible for 
detaching JIT body assumptions from the body linked-list and marking assumptions.

After all assumptions have been marked that are in need of being reclaimed, we 
can initiate a single pass over the RAT using *reclaimMarkedAssumptionsFromRAT()*. 
For each assumption "kind" hashtable which has been marked as having at least 
one marked assumption and for each hashtable bucket that has a marked count that 
is greater then zero, we walk the buckets linked-list removing any marked 
assumption.

This mark and sweep process allows us to efficiently remove assumptions from the 
RAT without having to have a "previous" pointer in the runtime assumption 
structure. Since the number of runtime assumptions can at times number in the 
hundreds of thousands, it's important to keep the runtime assumption structure as 
compact as possible. The mark and sweep reclaiming process is now used for all
cases of assumption reclaiming during a GC cycle, but it can be disabled by
setting the *TR_useOldRAReclaim* environment variable to "1" which will cause
the "one at a time" reclaiming process to be used during GC cycles.

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
