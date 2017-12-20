<!--
Copyright (c) 2017, 2017 IBM Corp. and others

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

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
-->

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
persistent profile information will evaluate whether the most recent has superceeded the best.
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

## JProfiling Thread

The JProfiling thread maintains a linked list of all persistent profile info, which it traverses
at runtime to detect profiling information that should be reset. This is done for value profile
info, as it will select the first N distinct values it sees for profiling. Due to this, these values
may not be the most frequenct, as indicated by the relative frequencies of the selected values and
the other counter. If this is determined to be the case, the thread will clear the selected values
and allow them to repopulate. As it is only logical to carry out this step when repopulation is still
possible, the thread will only inspect information that is being activey updated, as indicated by the active
flag on the persistent profile info. This flag is set when the corresponding jitted body is
about to be committed and cleared when it becomes a stub. This reduces the thread's overhead.

To simplify the process of maintaining the linked list of profile info along with any reference counts,
the deallocation of persistent profile information is deferred to this thread. When it traverses the list,
it will check the reference count and deallocate if it finds one that has reached zero. With this design,
compilations add new persistent profile information to the linked list as soon as they create it and the
thread removes them when possible. These constraits simplify the synchronization required on the linked
list significantly, as it only has to be singly linked for O(1) removal and only the single thread can remove.
Due to this, the linked list updates are lockless.

There should only be one of these threads for entire execution, similar to the IProfiler and Sampler threads.
It can be disabled using `TR_DisableJProfilerThread` and details about its execution can be inspected using
the verbose option `profiling`.
