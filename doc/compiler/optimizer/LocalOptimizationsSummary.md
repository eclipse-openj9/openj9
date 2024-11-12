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

# Local Optimizations

Local optimizations operate on a single extended basic block at a time.

## Tree Simplification
Tree Simplification performs simple IL tree transformations like constant
folding, strength reduction, canonicalizing expressions (e.g. an `add` with a
constant child will have the constant as the second child), branch/switch
simplification etc.

## Local Common Subexpression Elimination (CSE)/Copy Propagation
Local CSE/Copy Propagation commons identical expressions within a block and
performs removal of identical checks along with traditional commoning. Copy
Propagation is done simultaneously (The right hand side (RHS) of a store is
evaluated and the resulting register is used at each use of the stored value).
Commoning is done on syntactic equivalence.

## Local Value Propagation
Local [Value Propagation](https://github.com/eclipse-omr/omr/blob/master/doc/compiler/optimizer/ValuePropagation.md)
performs constant, type and relational propagation within a block.
It propagates constants and types based on assignments and performs removal
of subsumed checks and compares. It also performs devirtualization
of calls, checkcast and array store check elimination. It does not do value
numbering, instead it works on the assumption that every expression has a
unique value number; Its effectiveness is improved if expressions having same
value are commoned by a prior local CSE pass.

## Local Dead Store Elimination
Local Dead Store Elimination eliminates stores to locals/fields/statics (if
there are two stores to the same memory without any intervening use in the
same block).

## Local Reordering of Expressions
Local Reordering of Expressions reduces live ranges (register pressure) by
moving evaluations around within a block.

## Compaction of Null Explicit Null Checks
Compaction of Null Explicit Null Checks minimize the cases where an explicit null
check needs to be performed (null checks are implicit, i.e. by default on X86,
the JIT does not emit code to perform a null check, instead we use a hardware trap).
Sometimes (e.g. as a result of dead store removal of a store to a field) an explicit
null check node may need to be created in the IL trees (if the original dead store
was responsible for checking nullness implicitly); this pass attempts to find another
expression in the same block that can be used to perform the null check implicitly again.

## Dead Trees Elimination
Expressions that are evaluated and not used for a few instructions can actually be
evaluated exactly where required (subject to the constraint that their values should
be the same if evaluated later). This optimization attempts to simply delay evaluation
of expressions and has the effect of reducing register pressure by shortening
live ranges. It has the effect of removing some IL trees that are simply anchors
for (already) evaluated expressions thus making the trees more compact.

## Rematerialization
Rematerialization is performed as a late pass before code generation.
The optimization queries the underlying code generator (X86 etc.) for the max number of
registers available for assignment simultaneously, and then tries to reverse
commoning in regions of code within a block where the number of live expressions could
cause spills. Only certain kinds of expressions are considered for rematerialization,
e.g. address calculations (as chips can support more complex addressing modes),
or adds/subs with constants.

## String Peepholes
String Peepholes pattern-matches certain known bytecode sequences that are generated
by Javac typically for string operations like concatenation. It replaces them by a more
efficient IL sequence that is equivalent semantically. In the special case of
appending a `char` to an existing `String` object, a special private constructor is
called which is more efficient than going through a `StringBuffer` object.
