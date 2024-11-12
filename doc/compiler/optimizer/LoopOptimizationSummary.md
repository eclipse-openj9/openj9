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

# [Loop Optimizations](https://github.com/eclipse-omr/omr/blob/master/doc/compiler/optimizer/IntroLoopOptimizations.md)

## Loop Canonicalization
Loop Canonicalization transforms `while` loops to if-guarded `do-while` loops
with a loop invariant (pre-header) block. It places the loop test at the end
of the trees for the loop, so that the loop back-edge is almost always a
backwards branch.

This optimization also adds a loop pre-header block for a loop that is considered
a `do-while` loop (based on the induction variable being used in the backwards branch).
The addition of the loop pre-header is crucial as an enabler for many other loop
optimizations that need a place to add trees that they want executed if and only if
the loop structure is about to be entered, e.g. certain kinds of variable initializations
that they may want to do outside the loop in order to ensure that their transformations
inside the loop work as expected.

## Loop Versioning and Loop Specialization
During Loop Versioning, loop invariant null checks, bound checks, div checks,
checkcasts and inline guards (where the `this` does not change in the loop)
are eliminated from loops by placing equivalent tests outside the loop. It
also eliminates bound checks where the range of values of the index inside
the loop is discovered (if it is a compile time constant) and checks are
inserted outside the loop so that no bound checks fail inside the loop.

This analysis uses induction variable information found by [Value Propagation](https://github.com/eclipse-omr/omr/blob/master/doc/compiler/optimizer/ValuePropagation.md).
Another versioning test that is emitted ensures the loop will not run for a
very long period of time (in which case the async check inside the loop can
be removed). This special versioning test depends on the number of trees
(code) inside the loop.

Loop Specialization replaces loop-invariant expressions that are profiled
and found to be constants, with the constant value after inserting a test
outside the loop that compares the value to the constant. Note that this
cannot be done in the the absence of value profiling infrastructure.

## Loop Unrolling
A majority of loops can be unrolled with or without the loop driving
tests done after each iteration (loop) body. Async checks (yield points)
are eliminated from u-1 (u is the unroll factor) unrolled bodies and only
one remains. Unroll factors and code growth thresholds are arrived at based
on profiling information when available.
This analysis uses induction variable information found by [Value Propagation](https://github.com/eclipse-omr/omr/blob/master/doc/compiler/optimizer/ValuePropagation.md).

## Loop Invariant Code Motion
Done as part of [Partial Redundancy Elimination (PRE)](optimizer/GlobalOptimizationsSummary.md)

## Loop Inversion
Loop Inversion converts a loop where the induction variable is counting up
from zero into one where it counts down to zero. Note that this can only be
done if the inversion of the loop does not affect program semantics inside
the loop (order of exceptions thrown etc). The benefit of doing this is the
fact that there are, in general, instructions that can perform compare/branch
against zero in an efficient manner and special counter machine registers
(for e.g. On PowerPC) can be used.

## Loop Striding
Loop Striding creates derived induction variables (e.g. the address calculation
for array accesses `&a[i]`) and increment/decrement the derived induction
variables by the appropriate stride through every iteration of the loop.
This can eliminate some complex array address calculations inside the loop,
replaced instead by a load (reg or memory). Note that we also create derived
induction variables for non-internal pointers (like `4*i` or `4*i+16`) in
cases when it is not possible to create internal pointers (e.g. `a+4*i+16`).

## Field Privatization
If a field is loaded as well as stored within a loop, then it is (when
possible) beneficial to load the value of the field into a newly created
temp and convert the loads/stores of the field to loads/stores of the newly
created temp and remove the loads/stores of the field from inside the loop.

The value of the temp at all exit points out of the loop is stored back into
the field thus maintaining program semantics subsequent to the loop. Note
that PRE would do Field Privatization, but would not be able to eliminate
the stores to the field from the loop (unless a separate PRE pass is done
on the reversed CFG, which is expensive in compile time). Note that this
optimization can only be done if there are no calls/unresolved accesses in
the loop (as the field value could be read otherwise) and that makes it
rare.

## Async Check (yield point) Placement/Removal
Async checks are added by the VM at backward branches in the bytecodes on the
assumption that one cannot have a loop without a backwards branch. The async check
is used for many purposes, all of which are some variant of the need to
periodically have all application threads yield in order for some sort of action
to be taken, with the prime examples being a) GC and b) walk the stack to sample
where the thread is executing for driving compilation control.

The algorithm for minimizing the async checks in a given loop attempts to
achieve its goal by adopting two distinct methods:

- Check if the loop is known to be short running (either based on induction
variable information obtained from [Value Propagation](https://github.com/eclipse-omr/omr/blob/master/doc/compiler/optimizer/ValuePropagation.md))
or if the special versioning test for short running loops (mentioned earlier)
is found. In either case, since the loop is probably not long running, it does
not require an async check at all.
- If async checks cannot be skipped completely for the loop, attempt to place
the async checks at points within the loop such that there is minimal redundancy
(two async checks along a control flow path are avoided as far as possible).
Note that non-INL (Internal Native Library) calls are implicit async checks
and the placement algorithm takes them into account as well.
