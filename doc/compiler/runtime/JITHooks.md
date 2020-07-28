<!--
Copyright (c) 2020, 2020 IBM Corp. and others

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

# Background

The VM has a mechansim to register callbacks for specific runtime event.
The JIT makes use of these events to perform tasks either for functional 
correctness, performance, or housekeeping. 

# Hooks
This section outlines various events or Hooks the JIT registers a callback
against. The callbacks associated with these Hooks can be found in
HookedByTheJit.cpp.

|Hook|Description|Comment|Locks Held<sup>1</sup>|
|--|--|--|--|
|`J9HOOK_VM_PROFILING_BYTECODE_BUFFER_FULL`|Notifies when the bytecode profiling buffer (consumed by the IProfiler) for a given thread is full.||VM Access, IProfiler Buffer Arrival Monitor|
|`J9HOOK_VM_LOOKUP_JNI_ID`|Notifies when a method or field ID is looked up via JNI.|The JIT uses this to get notified when `main` in the Java code is about to be run; it then unregisters this hook.|Exclusive VM Access|
|`J9HOOK_VM_INITIALIZE_SEND_TARGET`|Notifies when J9Methods are to have their send target initialized.|Used to set initial invocation counts.||
|`J9HOOK_MM_OMR_LOCAL_GC_START`|Notifies when a local Garbage Collection has started.|Local here applies to policies such as GenCon, where only the nursery region is being GC'd.|Exclusive VM Access|
|`J9HOOK_MM_OMR_LOCAL_GC_END`|Notifies when a local GC has ended|If Real Time GC is enabled, the JIT registers `jitHookLocalGCEnd`; otherwise it registers `jitHookReleaseCodeLocalGCEnd`.|Exclusive VM Access|
|`J9HOOK_MM_OMR_GLOBAL_GC_START`|Notifies when a global GC has started.|Global here implies a GC of the entire heap.|Exclusive VM Access|
|`J9HOOK_MM_OMR_GLOBAL_GC_END`|Notifies when a global GC has ended.|If Real Time GC is enabled, the JIT registers `jitHookGlobalGCEnd`; otherwise it registers `jitHookReleaseCodeGlobalGCEnd`.|Exclusive VM Access|
|`J9HOOK_MM_OMR_GC_CYCLE_END`|Notifies when the GC Cycle has ended.|The JIT only registers against this hook if Real Time GC is enabled.|Exclusive VM Access|
|`J9HOOK_VM_INTERNAL_CLASS_LOAD`|Notifies when a class has been loaded.||Class Table Mutex|
|`J9HOOK_VM_CLASS_PREINITIALIZE`|Notifies the initialization of a class prior to running the `<clinit>`.||Class Table Mutex|
|`J9HOOK_VM_CLASS_INITIALIZE`|Notifies the initialization of a class after running the `<clint>`.|||
|`J9HOOK_VM_CLASS_UNLOAD`|Notifies when a class has been unloaded.||Exclusive VM Access|
|`J9HOOK_VM_CLASSES_UNLOAD`|Notifies with the list of classes that have been unloaded.||Exclusive VM Access, Class Table Mutex, Compilation Monitor|
|`J9HOOK_VM_ANON_CLASSES_UNLOAD`|Notifies with the list of anonymous classes that have been unloaded. ||Exclusive VM Access|
|`J9HOOK_VM_CLASS_LOADER_UNLOAD`|Notifies with the list of classloaders that have been unloaded.|The JIT uses this hook to clean up the DLT buffers.|Exclusive VM Access|
|`J9HOOK_VM_CLASS_LOADERS_UNLOAD`|Notifies for each classloader that is unloaded.||Exclusive VM Access|
|`J9HOOK_MM_CLASS_UNLOADING_END`|Notifies that the GC has finished unloading classes.||Exclusive VM Access|
|`J9HOOK_MM_INTERRUPT_COMPILATION`|Notifies that JIT compilation should be interrupted in order for the GC to unload classes.|||
|`J9HOOK_VM_THREAD_CREATED`|Notifies when a thread has been created.|||
|`J9HOOK_VM_THREAD_DESTROY`|Notifies when a thread has been destroyed.|||
|`J9HOOK_VM_THREAD_STARTED`|Notifies when a thread has started running.|||
|`J9HOOK_VM_THREAD_END`|Notifies when a thread has finished running.|||
|`J9HOOK_VM_THREAD_CRASH`|Notifies when a thread has crashed.|||
|`J9HOOK_VM_JNI_NATIVE_REGISTERED`|Notifies when a native has been registered.||Assumption Table Mutex|

# Chronological Order of Hooks
Certain hooks will be invoked in a specific order because they relate to
a defined set of events that occur in the VM, for example GC. This
section outlines the order in which these hooks get triggered (assuming
no failures or error conditions).

## Garbage Collection
1. `J9HOOK_MM_OMR_GC_CYCLE_START`
2. `J9HOOK_MM_OMR_LOCAL_GC_START` or `J9HOOK_MM_OMR_GLOBAL_GC_START`
3. `J9HOOK_MM_OMR_LOCAL_GC_END` or `J9HOOK_MM_OMR_GLOBAL_GC_END`
4. `J9HOOK_MM_OMR_GC_CYCLE_END`

## Class Loading/Unloading
1. `J9HOOK_VM_INTERNAL_CLASS_LOAD`
2. `J9HOOK_VM_CLASS_PREINITIALIZE`
3. `J9HOOK_VM_CLASS_INITIALIZE`
4. `J9HOOK_VM_CLASS_UNLOAD`
5. `J9HOOK_VM_CLASSES_UNLOAD`
6. `J9HOOK_VM_ANON_CLASSES_UNLOAD`
7. `J9HOOK_VM_CLASS_LOADERS_UNLOAD`
8. `J9HOOK_VM_CLASS_LOADER_UNLOAD`

## Threads
1. `J9HOOK_VM_THREAD_CREATED`
2. `J9HOOK_VM_THREAD_STARTED`
3. `J9HOOK_VM_THREAD_END `
4. `J9HOOK_VM_THREAD_DESTROY`

<hr/>

1. The locks are not necessarily all held simultaneously.
