<!--
Copyright (c) 2021, 2021 IBM Corp. and others

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

# JIT Write Barriers

Storing object pointers into other objects and classes must be communicated to
the garbage collector (GC) so that these newly created reference chains can be
tracked by the various GC policies. This communication occurs at runtime through
a mechanism known as a *write barrier*.

A good explanation of the mechanics and write barrier requirements of the
various GC policies in OpenJ9 can be found in this
[video](https://www.youtube.com/watch?v=Z2RUw8Clrec).

A write barrier is required **after** the following situations:

* The store of a reference to a field in an object (either static or an instance
field)
* The store of a reference into an array element via an `TR::ArrayStoreCHK` node
* An arraycopy of references via a 5-child `TR::arraycopy` node

The JIT is required to insert a write barrier in the code it generates in these
circumstances for functional correctness. The simplest means of performing a
write barrier is to simply call out to the GC to perform the appropriate write
barrier action depending on the requirements of the GC policy in place.

Runtime performance can be greatly improved if the JIT inlines part of the
operation of the write barrier. This document describes some of the checks that
can be inlined at runtime to improve performance.

## Write Barrier Kinds

The OpenJ9 VM supports a number of different write barriers depending on which
GC policy has been selected. All possible write barriers required by the GC are
defined in
[gc_include/j9modron.h](https://github.com/eclipse/openj9/blob/f0a4235ef0e2e6e1a8d476c4cf8dbf1ea1bc1cdf/runtime/gc_include/j9modron.h#L42-L51).

In summary:

| Write Barrier Kind | GC Policy and Description |
|:----|:----|
| j9gc_modron_wrtbar_none | `-Xgcpolicy:optthruput` : No write barrier is required |
| j9gc_modron_wrtbar_always | A write barrier is always required.  The JIT always calls the GC write barrier helper for this case. |
| j9gc_modron_wrtbar_cardmark | `-Xgcpolicy:optavgpause` : For non-generational GC policies, a concurrent mark check is required. |
| j9gc_modron_wrtbar_cardmark_incremental | `-Xgcpolicy:balanced` : For region-based GCs, an inter-region barrier is required. |
| j9gc_modron_wrtbar_cardmark_and_oldcheck | `-Xgcpolicy:gencon` : For generational GCs with concurrent mark, a generational check and a concurrent mark check is required. |
| j9gc_modron_wrtbar_oldcheck | `-Xgcpolicy:gencon -Xconcurrentlevel0` : For generational GC policies, a generational check is required.  Concurrent mark is not being used. |

There are two additional barrier kinds required for realtime GC (i.e.,
`-Xgcpolicy:metronome`) that are based on a "snapshot-at-the-beginning" or
(SATB) barrier that must precede the store. Metronome is only supported on x86
Linux and Power AIX and the operation of those barriers is not described in this
document.

## Write Barrier IL Nodes

For the purposes of garbage collection, only stores of references into other
objects need to be tracked. These are represented with two IL opcodes:
`awrtbari` and `awrtbar`.

`awrtbar` is for a direct store into another object (e.g., a store into a static
field) and has two children. The first child is the object being stored (or the
*source* object) and the second child is the object being stored into (or the
*destination* object).

`awrtbari` is for an indirect store into another object (e.g., a store into an
instance field or an array) and has three children. The first child is the
address being written to, the second child is the object being stored (or the
*source* object) and the third child is the object being stored into (or the
*destination* object).

## Inline Write Barrier Checks

Most of the time, the operation performed by a write barrier to keep the GC
state consistent does not actually need to occur. Relatively inexpensive runtime
checks are required to determine that. Because the checks are performed inline
the performance of the write barrier is greatly improved over calling into the
GC each time to determine the correct course of action. The checks required
depend on the GC policy and other features of the GC that are enabled.

### Nullness Check

Consider a reference store `A.field = B`. A write barrier is not required if `B`
is provably null. This can be determined both at compile time via the
`isNonNull()` property on the source object node or at runtime with an explicit
nullness check. 

**Note:** the cost of performing a nullness check at runtime needs to be
considered per architecture as the overhead may be too high given the relative
infrequency of null stores.

### Generational Check

Generational collectors divide the heap into different spaces depending on the
age of the objects they contain: a nursery (or *new space*) where new objects
are allocated and live until they mature past a certain age, and a tenured space
(or *old space*) where long-lived objects reside.

Based on the mechanics of the generational collector, if a new space object is
stored into an old space object then the old space object needs to be
"remembered" so that it can be scanned by the GC.

In pseudocode:
```
   if (A is tenured) {
      if (B is not tenured) {
         remember(A)
      }
   }
```

There are bits in an object's header that can be queried to determine whether
the object resides in new space or old space.

Typically, only the generational test is inlined and not the update to the
remembered set. This is acceptable in practice because the remembered set update
occurs relatively infrequently and can be done out of line by calling the
appropriate JIT write barrier helper.

### Concurrent Mark Check

Part of the marking phase for global garbage collects can be performed
concurrently with the application. In OpenJ9 this is accomplished with a special
concurrent mark thread that scans and marks live objects. However, if an object
`B` is stored into an object `A` that has already been scanned by the concurrent
mark thread then object `A` needs to be rescanned to ensure functional
correctness.

To do this efficiently, the heap is partitioned into a number of *cards* of a
fixed size (e.g., 512 bytes) that are represented by a single byte in a *card
table* structure. The mapping of an object address to a particular card is done
with simple arithmetic and shifting on the object address. Rather than
remembering individual objects to rescan, the card representing the memory where
a destination object lives is "dirtied" at runtime (in practice, writing a `1`
into the card table). All objects that reside in dirtied cards are rescanned at
the end of the current concurrent mark cycle.

This card dirtying only needs to occur if the concurrent mark thread is
currently active and marking objects. This can be tested efficiently at runtime
via flags from the `J9VMThread` structure.

In the case of a generational collector, the check only applies if the
destination object `A` is not a nursery object.

### Heap Object Check

Unlike when interpreting a method, when a method is JIT compiled there is a
possibility that the escape analysis optimization will localize some object
allocations on the method's stack frame. Furthermore, there may also be some
situations where the object being stored to could be either a heap object or a
stack-allocated object that cannot be determined at compile-time. In these
circumstances, a runtime check for whether the object is on the heap must be
inserted because the write barrier checks do not apply to local objects.

Checking whether an object is on the heap is done with an address comparison on
the heap base and size. In pseudocode:
```
   UDATA base = (UDATA)vmThread->omrVMThread->heapBaseForBarrierRange0;
   UDATA objectDelta = (UDATA)object - base;
   UDATA size = vmThread->omrVMThread->heapSizeForBarrierRange0;
   if (objectDelta < size) {
      // Heap object!
   }
```

In practice, the heap base address and size may be constant at compile time.
Exploiting this could improve the performance of this check at runtime. These
properties can be queried on the `Options` class:
```
   isVariableHeapBaseForBarrierRange0()
   isVariableHeapSizeForBarrierRange0()
```
and if constant the values can be obtained via the `Options` queries:
```
   getHeapBaseForBarrierRange0()
   getHeapSizeForBarrierRange0()
```

There are some additional properties that might be set on a write barrier node
when it can be definitively proven that an object is either a heap object or a
local object:

* `isNonHeapObjectWrtBar()` : if `true` then provably a local object; `false`
  implies that it cannot be proven to be a local object at compile time even
  though it may be.
* `isHeapObjectWrtBar()` : if `true` then provably an object on the heap;
  `false` implies that it cannot be proven to be a heap object at compile time
  even though it may be.

These properties should be consulted to determine at compile time if the heap
object checks can be eliminated or if the write barrier can be skipped entirely.

## Write Barrier Pseudocode

The ultimate reference for the operation of each write barrier kind is the
garbage collector code itself. These implementations can be found in
[gc_include/ObjectAccessBarrierAPI.hpp](https://github.com/eclipse/openj9/blob/master/runtime/gc_include/ObjectAccessBarrierAPI.hpp).

The following pseudocode is derived largely from those implementations.

### j9gc_modron_wrtbar_cardmark_and_oldcheck (gencon)
```
   // Check to see if object is old.  If object is not old neither barrier is required
   // if ((object - vmThread->omrVMThread->heapBaseForBarrierRange0) < vmThread->omrVMThread->heapSizeForBarrierRange0) then old

   UDATA base = (UDATA)vmThread->omrVMThread->heapBaseForBarrierRange0;
   UDATA objectDelta = (UDATA)object - base;
   UDATA size = vmThread->omrVMThread->heapSizeForBarrierRange0;
   if (objectDelta < size) {
      // object is old so check for concurrent barrier
      if (0 != (vmThread->privateFlags & J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE)) {
         // concurrent barrier is enabled so dirty the card for object

         /* calculate the delta with in the card table */
         UDATA shiftedDelta = objectDelta >> CARD_SIZE_SHIFT;

         /* find the card and dirty it */
         U_8* card = (U_8*)vmThread->activeCardTableBase + shiftedDelta;
         *card = (U_8)CARD_DIRTY;
      }

      // generational barrier is required if object is old
      //
      rememberObject(vmThread, object);
   }
```

### j9gc_modron_wrtbar_oldcheck (gencon -Xconcurrentlevel0)
```
   /* if value is NULL neither barrier is required */
   if (NULL != value) {
      /* Check to see if object is old.
       * if ((object - vmThread->omrVMThread->heapBaseForBarrierRange0) < vmThread->omrVMThread->heapSizeForBarrierRange0) then old
       *
       * Since card dirtying also requires this calculation remember these values
       */
      UDATA base = (UDATA)vmThread->omrVMThread->heapBaseForBarrierRange0;
      UDATA objectDelta = (UDATA)object - base;
      UDATA size = vmThread->omrVMThread->heapSizeForBarrierRange0;
      if (objectDelta < size) {
         /* generational barrier is required if object is old and value is new */
         /* Check to see if value is new using the same optimization as above */
         UDATA valueDelta = (UDATA)value - base;
         if (valueDelta >= size) {
            /* value is in new space so do generational barrier */
            rememberObject(vmThread, object);
         }
      }
  }
```

### j9gc_modron_wrtbar_cardmark_incremental (balanced)
```
   /* if value is NULL neither barrier is required */
   if (NULL != value) {
      /* Check to see if object is within the barrier range.
       * if ((object - vmThread->omrVMThread->heapBaseForBarrierRange0) < vmThread->omrVMThread->heapSizeForBarrierRange0)
       *
       * Since card dirtying also requires this calculation remember these values
       */
      UDATA base = (UDATA)vmThread->omrVMThread->heapBaseForBarrierRange0;
      UDATA objectDelta = (UDATA)object - base;
      UDATA size = vmThread->omrVMThread->heapSizeForBarrierRange0;
      if (objectDelta < size) {
         /* Object is within the barrier range.  Dirty the card */

         /* calculate the delta with in the card table */
         UDATA shiftedDelta = objectDelta >> CARD_SIZE_SHIFT;

         /* find the card and dirty it */
         U_8* card = (U_8*)vmThread->activeCardTableBase + shiftedDelta;
         *card = (U_8)CARD_DIRTY;
      }
   }
```

### j9gc_modron_wrtbar_cardmark (optavgpause)
```
   /* if value is NULL neither barrier is required */
   if (NULL != value) {
      /* Check to see if object is old.
       * if ((object - vmThread->omrVMThread->heapBaseForBarrierRange0) < vmThread->omrVMThread->heapSizeForBarrierRange0) then old
       *
       * Since card dirtying also requires this calculation remember these values
       */
      UDATA base = (UDATA)vmThread->omrVMThread->heapBaseForBarrierRange0;
      UDATA objectDelta = (UDATA)object - base;
      UDATA size = vmThread->omrVMThread->heapSizeForBarrierRange0;
      if (objectDelta < size) {
         /* object is old so check for concurrent barrier */
         if (0 != (vmThread->privateFlags & J9_PRIVATE_FLAGS_CONCURRENT_MARK_ACTIVE)) {
            /* concurrent barrier is enabled so dirty the card for object */

            /* calculate the delta with in the card table */
            UDATA shiftedDelta = objectDelta >> CARD_SIZE_SHIFT;

            /* find the card and dirty it */
            U_8* card = (U_8*)vmThread->activeCardTableBase + shiftedDelta;
            *card = (U_8)CARD_DIRTY;
         }
      }
   }
```

## Additional Notes

* Write barrier nodes may have the `skipWrtBar()` flag set which indicates that
  the write barrier can be skipped.
  
* Reference arraycopies use a batch write barrier at the end of the copy rather
  than a barrier after each element copied. While there are separate "object
  store" and "batch store" JIT write barrier helpers, in practice the inline
  checks to perform are the same.

* From an implementation perspective, there is obvious overlap between the
  operations of the various write barrier kinds and hence sharing code between
  kinds when it leads to a simpler, more maintainable design should be a goal to
  strive for.

* The `wrtbar` IL opcode usually exists as a tree top. However, an exception to
  that are `ArrayStoreCHK` nodes where the `wrtbar` appears as a child node
  representing the destination object store. This was done to improve the
  synergy and code quality of the required checks for an array element store,
  the element store itself, and any necessary write barrier.
