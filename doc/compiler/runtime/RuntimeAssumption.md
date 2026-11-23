<!--
Copyright IBM Corp. and others 2000

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
4. When a runtime event (e.g. class load/unload/redefine, etc.) has rendered an
assumption invalid.

In cases where runtime assumption reclaiming is done during a gc cycle, it's very
important that we minimize the processing time so that we don't unduly extend
the GC pause times.

### Data structures

The Runtime Assumption Table (RAT) is made of one hashtable per assumption kind
(see `TR_RuntimeAssumptionKind`). Every assumption lives in up to two linked
structures at the same time:

1. **A RAT bucket chain.** The chain is *doubly linked* in the style of the Linux
`hlist`: the bucket head is a single pointer, and every assumption has a `_next`
pointer and a `_pprev` pointer. `_pprev` points to the pointer that points to the
assumption, which is either the bucket head or the `_next` field of the previous
assumption. Because of this, an assumption can be unlinked from its bucket in O(1)
without knowing its predecessor or walking the bucket. The same assumption key can
appear several times in a bucket. `OMR::RuntimeAssumption::linkIntoBucket()` and
`unlinkFromBucket()` implement the operations.
2. **The circular list of the assumptions of a compiled body.** The list starts at
a sentinel assumption that is referenced from the body's metadata
(`runtimeAssumptionList`). It is used to find all the assumptions that must be
removed when the body is reclaimed.

In addition, the table has a **zombie list**: a doubly linked list (head in
`_zombieHead`) of assumptions that were invalidated and must be freed, but that
have not been freed yet. A zombie assumption has the *marked for detach* bit set
(the low bit of `_nextAssumptionForSameJittedBody`) and is **no longer in any RAT
bucket**. While it is a zombie, its `_next`/`_pprev` fields are used for the
zombie list instead of for a bucket chain. A zombie can still be in the circular
list of its compiled body until it is freed.

Since zombies are not in the buckets, searches of the RAT (for example
`getBucketPtr()` and `getNext()`) never see 'dead' assumptions, and no code that
walks a bucket needs to skip marked entries.

### Removing assumptions

An assumption is removed from the RAT in one of two ways, depending on why it is
removed:

1. **Invalidation by a runtime event** (class extended, method overridden, class
pre-initialized, class unloaded, class redefined, static final field modified,
mutable call site changed, method breakpointed, etc.). The RAT code finds the
bucket for the key, and for every matching assumption it calls `compensate()` and
then `markForDetachFromRAT()`. That function unlinks the assumption from its bucket,
sets the mark bit and pushes the assumption on the zombie list. The assumption is
not freed at this point: it is still in the circular list of its body, and freeing
it is deferred to GC time to keep the invalidation event fast. Because marking modifies
the `_next` pointer of the assumption (it now refers to the zombie list), any loop
that walks a bucket must read the next element **before** it marks the current one.
2. **Reclamation of a compiled body** (recompilation, class unloading, or a failed
compilation): `markAssumptionsAndDetach()` and `reclaimAssumptions()` walk the
circular list of the body.
    - An assumption that is already a zombie is left alone. It was detached from
    the RAT when it was invalidated and it will be freed through the zombie list.
    - Any other assumption that belongs to the body being reclaimed is deleted
    right away: it is unlinked from the circular list and from its RAT bucket (both
    O(1)), then `reclaim()` is called and the memory is freed. `compensate()` is not
    called because the code it would patch is going away.
    - Assumptions that do not belong to the body (for example class redefinition
    assumptions that refer to the persistent method info, which must outlive the
    body) are left in place.
    - If nothing is left in the circular list, the sentinel is detached from the
    metadata. It is freed immediately if no zombie remains in the list; otherwise it
    is marked, and it is freed when the last zombie is dequeued from the list.

When a class is redefined, the assumptions of kind `ClassRedefinitionPIC` have their
key changed to the new class, so they might need to move to a different bucket. This
is done with `unlinkFromBucket()` followed by `linkIntoBucket()`.

### Garbage collection

At the start of every GC cycle, the JIT calls `reclaimMarkedAssumptionsFromRAT()`
with a count, which lets the RAT free a fixed number of zombies (by default 100,
configurable with `TR_numRAToClean`; -1 means all of them). The routine simply pops
entries from the zombie list. It does not scan the hashtables, so the cost does not
depend on the size of the RAT and it does not touch buckets that have no dead
assumptions (which used to cause cache misses and page faults on large tables). For
each zombie, if it is still in the circular list of its body, a walk of that list
removes it (and any other marked assumption that is met on the way). Then `reclaim()`
is called and the memory is freed. An eager cleanup of the entire zombie list is also
done when the JIT enters the idle state.

This staged approach allows us to amortize the clean-up of the assumptions over
time and minimizes the amount of clean-up done during any single period where
execution is stopped. Prior to the conversion to lazy removal you could have
long pauses when the assumption table had to eagerly clean up all of the dead
assumptions.

### Debugging aids

When `DEBUG` or `PROD_WITH_ASSUMES` is defined, the RAT code verifies after every
change that the bucket chains are consistent (`*n->_pprev == n` for every node) and
contain no zombies, and that the zombie list is consistent (every node is marked,
`_pprev` is correct and the number of nodes is equal to the `_marked` counter).

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
