<!--
Copyright (c) 2017, 2018 IBM Corp. and others

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

# Value JProfiling

JProfiling provides block frequency and value info profiling with low overhead.
This documentation focuses on the value info profiling implementation.

Value JProfiling relies on the insight that profiling information is only
significant when a small set of highly frequent values exist for a variable,
as these values enable optimization. Therefore, the implementation can avoid
the overhead of full fidelity value profiling, whilst providing the same benefits,
by only recording the most frequency values and detecting the case where
there are none.

## Implementation

Each profiled variable receives a constant sized hash table and an *other*
counter. The hash table maps a value to the frequency with which it
is seen. When attempting to increment the frequency for a value, a hash
table lookup is performed. If the value is found, its corresponding frequency
is incremented. Otherwise, the value is either added to the table or
the *other* counter is incremented, depending on if the table has
reached its capacity.

This approach limits the overhead introduced by profiling, as variables
with a small set of highly frequent values will mostly consist of cheap
hash table lookups and increments. Adding values to the table may be
expensive, due to the computation and synchronization required, however
this is limited by the tables capacity, with the remaining values
being cheaply attributed to the *other* counter.

The process of incrementing the frequency for an existing value or the
*other* counter has been implemented in the IL, whilst the addition
of values to the table is managed by a helper call. This avoids locality
issues due to frequent calls and allows for other optimizations to further
reduce the instrumentation's disruption to the mainline.

There are various implementations of the hash function, all based around
the X86 instruction `pext`. *N* bits are identified amongst the *N + 1* values
held in the table, such that they can distinguish between then. These bits are
extracted from the value and gathered into the low bits of an index, capable
of addressing an array of length *2^N*. This index is used to extract values
and frequencies from the hash table. The approach trades runtime performance
for memory overhead, as the hash table size doubles for every additional value it
is to support. These additional slots are mostly left unused.

When adding a value to the table, the prior *N* selected bits are thrown
out and a new set of *N + 1* bits are chosen, based on the table's existing
contents and the new value. This may require rearrangement of the existing
values if the bits change sufficiently. To ensure this rearrangement process
won't race with other threads attempting to add to the table or increments
from the jitted code, the table's is locked and its keys are manipulated
to avoid any matches. This is achieved by backing up the values, setting
the first slot to -1 and setting the rest to 0, relying on that fact
that a value of 0 will always be placed in the first slot due to the
hash function. Values are then rearranged in their backed up data structure
and then committed back once the process has finished. Frequencies can
be rearranged in place, as no increments should occur.

During this process, other threads will increment the *other* counter so
that the total frequency remains accurate. This process is complicated by 
placing in the *other* counter in an unused slot. This is desirable, as a
table with a capacity of 3 or greater will have at least one counter slot
that is not in use. The frequency slot being used as the *other* counter
cannot move during rearragnment, as it may race with other moves. To
get around this, the final destination of the *other* counter is stashed
and then performed as the final rearrangement.

Once the values and frequencies have been rearranged, the new hash
configuration can be committed and the addition is finished.
If the table reached its capacity, the full flag is set and any new
values seen in jitted code are attributed to the *other* counter.
An additional optimization is made here, to cut down on memory accesses.
The table is considered full when the index for the *other* counter
is positive, so the tested value can be used immediately to increment
the counter. In the event that its negative, the helper should be
called to add a new value to the table.

As the table populates based on the first distinct values that are
seen, its possible to have ignored highly frequent values, for example due to
an early phase change. To account for this, a thread inspects the hash
tables and clears any with high *other* counters relative to that of the
profiled values. If it decides that highly frequency values have been
missed, it will reset the table and allow it to repopulate.

Each table has a lock bit, which is locked when accessing data from a
compilation thread, rearranging in the helper or clearing in the
thread. Increments from the mainline do not check this lock, to avoid
the overhead caused by synchronization. Additionally, these increments
are not atomic, to avoid this same overhead, which may result in races.
In testing, this did not affect results sufficiently to negatively impact
optimizations. It should be noted that, unlike the global monitor used by
other profiling implementations, the lock is not reentrant.

In addition to the `pext` hash function, others are supported. One approach
uses an array of bytes specifying shifts to apply to the original value.
This is implemented using existing opcodes and should be supported on all
platforms. A more efficient implementation exists using the new `bitpermute`
opcode, configured with an array of bytes specifying bit indices, rather
than shifts. If supported by the codegen, the `bitpermute` approach will
be used, as indicated by `getSupportsBitPermute()`.

## Configuration

Value JProfiling can be used to replace JitProfiling. In this configuration,
all existing requests for value profiling trees will insert placeholder
JProfiling helper calls. The JProfilingValue optimization pass will run
late in the opt strategy and will lower these placeholders into the partial IL and
helper implementation. This can be enabled with `enableJProfilingInProfilingCompilations`.
Late swaps to profiling are not supported in this mode and will result in
a compilation abort and restart.

Value JProfiling can also run under the existing JProfiling mode, enabled
with `enableJProfiling`. In this mode, the JProfilingValue optimization pass
will insert instrumentation for all `instanceof`, `checkcast` and indirect
calls, profiling the vtable result for the object in question.

The environment option `TR_JProfilingValueDumpInfo` can be set to dump
table contents throughout the helper call to stdout.

# Persistent Profile Info

Persistent profile info holds profiling information between compilations. It holds both
meta data and the actual profiling information. For example, when a compilation decides
to include profiling instrumentation, it will create a `TR_PersistentProfileInfo` and
add call site information as well as the frequency with which profiling should be collected.
Depending on what instrumentation is then added, it may create value profiling and/or
block frequency counters and their respective persistent data structures. These data structures
are used by jitted code to record values/frequencies that are seen.

Later compiles, either of the same method or others with similar inlining, will attempt to
access this information to inform optimizations. Its possible that there could have been
several compiles of the particular method, all with profiling data. To simplify the situation
and ensure a recent compile doesn't reduce the quality of profiling information, the
best and most recent profiling information are tracked on the method info. A request for a method's
persistent profile information will evaluate whether the most recent has superseded the best.
If so, the most recent now becomes best and is returned. Otherwise, the best is returned.
Only these two have to be compared as there will typically only be one compilation of a method
collecting information at a time.

## Reference Counts

As accesses to profiling information can exceed the lifetime of the body for which they were
created and they can incur significant memory overhead, its necessary to deallocate when
they can no longer be accessed. This is achieved through reference counting. There is an implicit
reference count on each persistent profile info, to represent the reference from its body info.
Other sources of references include the best and recent pointers on the method info. Methods wrapping
these two references will manage reference counts as necessary. Each compilation has a `TR_AccessedProfileInfo`
which manages the reference counts for accesses from within a compilation, extending the duration of the
reference until the end of the compile. Through this, most uses of profile information don't have
to be concerned with reference counts.

To reduce synchronization overhead, accesses to profiling information through the body info
will not modify reference counts as its assumed these uses will all be within the lifetime of the body
info, such as during its compilation or commit.

Once the reference count reaches zero, it will be deallocated, either immediately or after a short
duration in a separate cleanup process. It cannot be incremented after reaching zero.

## Value Profile Info

Value profile info collected from jitted code is located in the corresponding persistently allocated
`TR_ValueProfileInfo` for a body. There are other sources of info, such as the interpreter. These
are accessed through heap allocated `TR_ExternalValueProfileInfo`. To provide a consistent interface
for optimizations, a unified or subset of these sources can be accessed through the
`TR_ValueProfileInfoManager`.

Value profile info for a variable consists of a persistently allocated `TR_AbstractProfilerInfo`. 
This contains a metadata, such as the BCI and kind for the variable, and the collected information.
When accessed in compilations, this data is wrapped in a `TR_AbstractInfo` of the appropriate
subclass for the profiled kind, such as `TR_AddressInfo`. These wrappers provide common methods
for a profiled kind.

## JProfiling Thread

The JProfiling thread maintains a linked list of all persistent profile info, which it traverses
at runtime to detect profiling information that should be reset. This is done for value profile
info, as it will select the first N distinct values it sees for profiling. Due to this, these values
may not be the most frequent, as indicated by the relative frequencies of the selected values and
the other counter. If this is determined to be the case, the thread will clear the selected values
and allow them to repopulate. As it is only logical to carry out this step when repopulation is still
possible, the thread will only inspect information that is being actively updated, as indicated by the active
flag on the persistent profile info. This flag is set when the corresponding jitted body is
about to be committed and cleared when it becomes a stub. This reduces the thread's overhead.

To simplify the process of maintaining the linked list of profile info along with any reference counts,
the deallocation of persistent profile information is deferred to this thread. When it traverses the list,
it will check the reference count and deallocate if it finds one that has reached zero. With this design,
compilations add new persistent profile information to the linked list as soon as they create it and the
thread removes them when possible. These constraints simplify the synchronization required on the linked
list significantly, as it only has to be singly linked for O(1) removal and only the single thread can remove.
Due to this, the linked list updates are lockless.

There should only be one of these threads for entire execution, similar to the IProfiler and Sampler threads.
It can be disabled using `TR_DisableJProfilerThread` and details about its execution can be inspected using
the verbose option `profiling`.
