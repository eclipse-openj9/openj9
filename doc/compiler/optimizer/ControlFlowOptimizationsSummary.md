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

# Control Flow Optimizations

## Basic Block Ordering
Basic Block Ordering moves blocks around based on making the 'hottest' paths (based on
profiling and/or static criteria like loop nesting depth, expected code synergy)
the fall through.
- 'Hot' path: The code path that is executed very often.
- 'Cold' path: The code path that is executed less often.

## Block Hoisting
Block Hoisting duplicates the code in a block and places the copied code into
any predecessors of the block that have the block as their only successor.
This is done if it would enable more commoning or other optimizations to occur.

One example of when this optimization may be effective is with respect to local
dead store elimination in case the hoisted block ends in a return, since it might
allow the removal of trailing stores to local variables.

## Block Splitting
The basic idea of block splitter and virtual guard tail splitter are the same: namely,
do the transformation referred to as "tail duplication" in compiler literature,
that essentially involves duplicating code after control flow merge points to
avoid the merge in control flow. This transformation should result in more
opportunities to specialize and optimize the duplicated code for the hottest paths
leading up to the control flow merge.

The difference between block splitter and virtual guard tail splitter is that
block splitting uses profiled block hotness instead of the static hotness
(virtual call path is 'cold'; inlined path is 'hot') used by virtual guard tail splitter.

## Redundant Goto Elimination
A conditional branch that branches to a block containing only a goto can be
changed to branch directly to the destination of the goto.

## Catch Block Removal
If a block does not have any code that can cause exceptions, then an exception
edge in the CFG from the block to any catch blocks can be removed.

Catch Block Removal considers the type of the exception and the type being handled
in the catch block before deciding to remove the exception edge.

In the special case that all the exception edges going to a particular catch block
are removed, the catch block itself is unreachable and therefore removed as well.

## Exception Directed Optimization (EDO)
EDO profiles catch blocks within a method and if many exceptions are caught
by a catch block, then it attempts to convert the corresponding throws (if/once
they are present in the same method) into gotos to the catch block, thereby
eliminating the allocation of the thrown exception object completely (if it
can be proven that the throw definitely reaches the catch block only and the
thrown object is never used in code reachable from the catch block).
See [this document](EdoOptimization.md) for an explanation of the mechanics involved.

Note that [Value Propagation](https://github.com/eclipse-omr/omr/blob/master/doc/compiler/optimizer/ValuePropagation.md)
(which propagates type information) is required
to prove that the type of the exception being thrown will be caught by the
catch block. Another important pre-requisite for doing EDO successfully is
that aggressive inlining often needs to be done in order to ensure the code
for the throws that reach a particular catch are in the same method as the
catch. Since aggressive inlining is not ideal in general whenever catch blocks
are present, catch blocks are profiled (to determine whether exceptions are
indeed being caught) in order to select the catch block(s) that will yield
the best trade-off.
