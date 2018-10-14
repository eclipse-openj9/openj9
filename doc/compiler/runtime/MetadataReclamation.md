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

# Metadata Reclamation Design {#MetadataReclamation}

When an ordinary java method *aJavaMethod* gets recompiled, this is the sequence
of operations:


1. The first instruction of the old body of *aJavaMethod* gets patched to branch
to a runtime helper routine which:
  1. Patches the caller to call the new body
  2. Tranfers control the new body in such a way that when it returns, it
  returns to the caller and not the helper routine
2. All new invocations of *aJavaMethod* call the new body either directly or 
because it got patched to do so
3. The old body continues to exist until GC, at which point a JIT hook checks
the stacks of all threads to verify that it is no longer in use
4. Once the JIT determines that the old body is safe to reclaim, at any Global
GC it:
  1. Reclaims the majority of the body for code cache reuse leaving behind a
  stub
  2. Updates the *J9JITExceptionTable* to describe just the stub
  

The *J9JITExceptionTable* (also referred to as the metadata) is a struct that
is used to describe the code body. It also has a variable length section that
contains data specific to the body like exception ranges, GC Maps, etc. This
table continues to persist even when all it's describing is the stub. This can
be a big waste of memory since the variable length section can get quite large.
This design aims to reclaim the *J9JITExceptionTable* when a body gets reclaimed
and replace it with a "stub" *J9JITExceptionTable* to describe a code stub.


*J9JITExeptionTable* reclamation occurs when `jitReleaseCodeCollectMetaData` is
called, the JIT tries to allocate a new *J9JITExeptionTable* without any
variable length section
* If it fails, it reuses the existing *J9JITExceptionTable* and does nothing
else.
* If it succeeds, it: 
  1. `memcpy`s the non-variable lenth section from the existing
  *J9JITExceptionTable*
  2. Updates the various `prevMethod` and `nextMethod` pointers
  3. Sets the following fields to `NULL`: 
     * inlinedCalls - This is pointer to the list of inlined calls in the
     variable length section 
     * gcStackAtlas - This is a pointer to the J9JITStackAtlas struct in the
     variable length section
     * gpuCode - This is a pointer to the GPU Data in the variable length
     section
     * riData - This is a pointer to the RI Data in the variable length section
     * osrInfo - This is a pointer to OSR info tn the variable length section
     * runtimeAssumptionList - This is a pointer to the list of Runtime
     Assumptions that gets reclaimed along with the old body
  4. Frees the Fast Walk Cache
  5. Sets the `JIT_METADATA_IS_STUB` flag so anyone with a PC in the stub
  knows as much
  6. Adds the new *J9JITExceptionTable* to the AVL Trees
  7. Frees the old *J9JITExceptionTable*

  
The options to enable/disable this are `enableMetadataReclamation`/
`disableMetadataReclamation`.
