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

# Global Optimizations

## Global [Value Propagation](https://github.com/eclipse-omr/omr/blob/master/doc/compiler/optimizer/ValuePropagation.md)
Global Value Propagation (GVP) performs propagation of types, constants, ranges, nullness
etc. for value numbers. It requires global value numbering to have been done.
GVP performs de-virtualization (followed by inlining in some cases), check removal, branch
folding, and replacement by constants (integer and strings).

## Partial Redundancy Elimination (PRE)
Partial Redundancy Elimination performs global commoning and loop invariant code motion
for expressions unaffected by exception semantics (e.g. an iadd operation does not affect
exception semantics in any way) and exception checks. It is based on Knoop, Ruthing and Steffen’s
lazy code motion algorithm that involves two local analyses and five bit vector
analyses.

In our enhancement to the original algorithm to enable optimization of
checks, we perform lazy code motion optimistically first (ignoring exception ordering
constraints imposed by Java). We then perform a post-pass data flow analysis that attempts
to move the expressions to their optimal placement points as computed by the traditional
Least Common Multiple (LCM) algorithm. Note that it may not be possible to move an
expression to the appropriate placement point as deemed by the optimistic analyses.

Two enhancements in the OpenJ9 implementation affect Knoop, Ruthing and Steffen’s lazy code
motion algorithm substantially.

- The first is the adaptation of the approach to handle exception checks as described in
[Removing redundancy via exception check motion](https://dl.acm.org/doi/10.1145/1356058.1356077)

- The second is the fact that we adapted the notion of "local anticipatability" used in the
algorithm to work in "downward exposed" manner. Downward exposure essentially means that
the solution computed for a given block corresponds to what is locally anticipatable at
the end of the given block, as opposed to upward exposure that means the solution corresponds
to what is locally anticipatable at the start of the given block. A PRE implementation can use
both upward and downward exposed bit vectors for a given block, but we decided to avoid this
to save the compile time cost since the work was done in the context of a JIT compiler. In
choosing between upward and downward exposure, we decided on the latter since it allows us
to do global commoning in a traditional/expected sense in cases when an expression is computed
in a block after some other tree in that same block kills the expression. In such cases,
the expression would not be considered locally anticipatable in the upward exposed sense,
but it would be considered locally anticipatable in the downward exposed sense (as long as
there is no other tree later in the block that kills the expression). This difference would
allow the global commoning to be done using the expression in the block to avoid recomputing
it again in some later block.

## [Escape Analysis](https://youtu.be/JMpqwLVwmk8)
Escape Analysis uses value numbering to find all the uses of a particular allocation.
If all uses of the allocated object are within the method, then the object is allocated
on the stack. Synchronizations on non-escaping objects can also be eliminated. Some
library methods are known to not cause the object to escape; these methods are special
cased in the analysis.

Stack allocations can be done in two forms: contiguous or non-contiguous.
- Non-contiguous allocations represent the best case scenario from a performance viewpoint
as the object and each of its constituent fields are represented by distinct temps.
This enables each of the fields to be considered by other analyses (e.g. Dead Store Elimination etc.)
completely independently of the others. Note that this can only be done if the allocation
is not used as an object (e.g. passed out into a call which dereferences the object).

- Contiguous allocations mean that the object is allocated on the stack but the fields
are all part of the object on the stack. References to the fields still have to use
the underlying object (except that it's on the stack instead of the heap).

Escape Analysis can request inlining of additional methods in order to expose more cases
in which objects can be seen not to escape and more opportunities of non-contiguous allocation.

There is a notion of "heapifying" a stack allocated object in cases even when an escape point
is detected for the object but the escape point is a cold block. In such cases it is worth
keeping the object on stack on the hot path and only if the cold path is taken, then the object
is allocated on the heap and the contents of the object on the stack are copied into the
newly allocated heap object. As part of heapification, all references from autos/parms to
the stack allocated object are updated to point to the heapified object instead.
This means that it is safe to execute any code where the object escapes from that point on.

OpenJ9 Escape Analysis implementation also has a notion of de-memoization/re-memoization.
This pertains to some special classes in the JCL: `Integer`, `Long`, `Short`, `Byte` etc. that
memoize some instances of those classes in a static array that they maintain (only for some
ranges of values). These classes would use these arrays to return an already allocated/memoized
object rather than allocate a new object every time (if possible) when `valueOf` is called.
These arrays therefore also become escape points for objects that are allocated and stored
into them.

De-memoization attempts to get around this by converting `valueOf` calls to new
allocations before Escape Analysis is attempted on the assumption that allocating such objects
on the stack would be even more preferable to memoizing them in the arrays. If stack allocation
was not possible due to some reason, the de-memoized allocations are re-memoized, leaving the
code as it was originally and an attempt is also made to inline the re-memoized `valueOf` since
the inlining of these calls is suppressed in the inliner to give de-memoization a chance later.

Allocation sinking is an optimization that helps Escape Analysis. It basically moves the `new` node
as close as possible to the corresponding `<init>` call. These two trees can be separated by
arbitrary control flow on occasions when the Java code is like `x = new X(foo());` and so `foo()`
is between the `new` and the `<init>` call. If `foo()` gets inlined, then one can even get non
trivial control flow between the `new` and the `<init>` call.

On PowerPC platform, there is a "Flow Sensitive Escape Analysis" pass.
Flow Sensitive Escape Analysis tries to find places in the code where an object
"has not escaped yet" rather than the notion of "does the object escape anywhere" that is
the hallmark of the Flow Insensitive Escape Analysis that runs on non-Power platforms.
The reason for doing this Flow Sensitive Escape Analysis is because we want to optimize away the
`lwsync` instructions that must be emitted after allocations to ensure that other threads
see a properly initialized object. Flow Sensitive Escape Analysis attempts to move such
`lwsyncs` together (i.e. combine) as much as possible. This includes trying to move an `lwsync`
for an object to another `lwsync` (either for the same object (after a constructor if the
object has final fields) or a different object), a `monexit` or volatile store (that do
`lwsyncs` for different reasons) but all only on the basis that the object has not escaped yet.

## Explicit New Initialization
Explicit New Initialization attempts to skip zero initialization of objects where possible.
Basically it scans forward in the basic block from the point of the allocation and
tries to find explicit definitions of fields (e.g. in constructors) that occur before
any use of the field. In these cases the zero initialization of fields (required according
to Java spec) can be skipped.

Note that for reference fields, the presence of a [Garbage Collection (GC) point](https://github.com/eclipse-omr/omr/blob/81b79405da6c7c960e611a8b2b12fd5861543330/compiler/il/OMRNode.cpp#L2298)
before any user-specified definition means that the field must be zero initialized.
The reason why a reference field must be zero initialized before the object is visible
to the GC is to prevent the GC from potentially chasing an uninitialized non-zero value
in a slot corresponding to a reference field, since the GC is interested in following
such fields to find the set of reachable objects.

Peeking inside calls is done to maximize the number of explicit field initializations seen
by the analysis. In addition, for some special array allocations, we can skip zero
initialization completely (e.g. char arrays that are used by `String`, often do not
need zero initialization as they are filled in by `arraycopy` calls immediately).

## Global Copy Propagation, Dead Store Elimination and Dead Structure Removal
These three analyses use use/def info to perform the traditional optimizations. The
analysis is only done for locals (statics/fields are omitted). Dead structure removal
basically tries to find a region of code with no side-effects and having no definition
that is used outside the structure; in such cases the (dead) region of code can be
eliminated.

## Redundant Monitor Elimination
Redundant Monitor Elimination uses value numbering to perform nested monitor removal
(e.g. `monenter` on the same object twice followed by two `monexits`). It uses value
number information and structure to perform lock coarsening (e.g. `monenter/monexit`
on some object followed by another `monenter/monexit` on the same object). In this
case the extent of code across which the lock is held is increased and one `monenter/monexit`
can be removed. Deadlock avoidance is ensured by examining the code where the lock
would be held after coarsening and making sure there are no calls or lock/unlock
operations in that region of code. Sometimes additional `monenters/monexits` may be
added on ‘cold’ paths to avoid some `monenters/monexits` on ‘hot’ paths.

## Global Register Allocator (GRA)
GRA is a solution for achieving the effect of global register assignment, even though
the underlying code generators have local register allocators. The basic idea
is that [global register dependencies](https://github.com/eclipse-omr/omr/blob/master/doc/compiler/il/GlRegDeps.md)
are introduced in the IL trees; these dependencies
(when evaluated by the code generators) ensure that a certain value is in a certain
register on all control flow paths into a basic block. Stores/loads from memory are
changed into stores/loads from registers in the IL (register numbers specify which
physical register the value is placed into). Global register candidates are chosen
by various optimizer passes like PRE, induction variable detection etc. and appropriate
priorities are given to the candidates based on the number of references and live
range (number of blocks).

The advantages of GRA are that most of the code is platform neutral apart from a few
simple evaluators that must be added to a new platform code generator,
thus drastically reducing the development cost of adding a powerful global register allocator.
It is also relatively cheap in compile time in comparison to (the expected cost of) a
graph-coloring allocator.

## Live Variables for GC
Live Variables for GC attempts to minimize the number of reference locals that need to be zero
initialized upon method entry. The basic idea is to perform liveness analysis and find
out if any reference locals are live at any [GC point](https://github.com/eclipse-omr/omr/blob/81b79405da6c7c960e611a8b2b12fd5861543330/compiler/il/OMRNode.cpp#L2298)
that is not dominated by (user defined) stores on all control flow paths. If a reference local
is uninitialized at a GC point, explicit zero initialization must be inserted at the method entry.
Note that this situation is fairly rare. Another function of this analysis is to minimize the
number of object stack slots that are examined at a GC point (reference locals that are not
live at a GC point need not be examined at all).

Local Live Variables for GC does not perform a full blown liveness analysis; instead
it performs a forward walk through the CFG till the first GC point is seen and collects
all the reference locals that have been stored till that point and marks the other reference locals as
requiring zero initialization.

Global Live Variables for GC collects zero initialization information as explained above;
in addition it also populates each basic block with information about which reference locals
are live on entry to the block. This is then used by the code generator to only
(selectively) switch on bits in the GC stack maps for those reference reference locals that
are live (bits in the GC maps are used to communicate to GC which stack slots need
to be examined). Note that Local Live Variables for GC cannot select the bits that
will be on in each GC stack maps (it does not do liveness); it simply switches on
all the bits for slots containing references at every block.

## Compact Locals
Compact Locals tries to minimize the stack size required by the compiled method.
This is done by computing interferences between the live ranges of the locals/temps
in the method before code generation is done; based on these interferences, locals
that are never live simultaneously can be mapped on to the same stack slot.
Liveness information is used by this analysis.
