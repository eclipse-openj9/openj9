<!--
Copyright IBM Corp. and others 2024

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

# Background

This documention outlines how the compiler prepares for checkpoint and restore.
This is a very dynamic process that requires the coordination of several
components, as well as taking into account changes in the environment. This
coordination is important for a few reasons:

* The options can change on restore, and so the compiler threads cannot be in
flight while the new options are being processed and applied.
* The CPU restrictions can get lifted on restore, which can result in low level
CPU queries returning different answers within the same compilation.
* The Shared Classes Cache (SCC) may no longer exist on restore, or conversely,
an SCC that did not exist pre-checkpoint now does exist.

For all these reasons, and more, the compiler needs to ensure coordination to
maintain coherency across a checkpoint/restore.

# High Level Overview

At bootstrap, the compiler registers callbacks for the
`J9HOOK_VM_PREPARING_FOR_CHECKPOINT` and `J9HOOK_VM_PREPARING_FOR_RESTORE`
hooks, namely `jitHookPrepareCheckpoint` and `jitHookPrepareRestore`
respectively.

When the application signals to the VM that a checkpoint should occur, the VM
calls `jitHookPrepareCheckpoint` as part of the checkpoint process. This calls
the top level method in the compiler that performs the necessary coordination:
`TR::CRRuntime::prepareForCheckpoint`.
Once the coordination is complete, the executing thread returns to the VM, which
eventually invokes CRIU to checkpoint itself.

On restore, the VM calls `jitHookPrepareRestore` as part of the restore process.
This first checks if further checkpointing is not allowed in order to remove the
portability restrictions on the target CPU. It then calls the top level method
in the compiler that performs the necessary coordination:
`TR::CRRuntime::prepareForRestore`. Once the coordination is complete,
the executing thread returns to the VM, which eventually resumes the
application.

# Preparing for Checkpoint

As mentioned above, `TR::CRRuntime::prepareForCheckpoint` handles all the
coordination needed to prepare the compiler for checkpoint. This is a very
complex set of operations as it involves coordinating multiple threads, as well
as being aware of events such as shutdown.

First, this method releases VM Access and acquires the Compilation Monitor. It
then checks if the checkpoint should be interrupted. It uses the
`TR::CRRuntime::_checkpointStatus` member variable to determine the state
of the checkpoint. The status transitions as follows:

1. `TR_CheckpointStatus::NO_CHECKPOINT_IN_PROGRESS`
2. `TR_CheckpointStatus::COMPILE_METHODS_FOR_CHECKPOINT`
3. `TR_CheckpointStatus::SUSPEND_THREADS_FOR_CHECKPOINT`
4. `TR_CheckpointStatus::READY_FOR_CHECKPOINT_RESTORE`

If an event (such as shutdown) needs to interrupt the checkpoint process, the
state is set to `TR_CheckpointStatus::INTERRUPT_CHECKPOINT`. The Compilation
Monitor is used to guard accesses to the checkpoint status; as such, prior to
proceeding with the checkpoint process, the method needs to check if something
like shutdown is currently occurring. Suspending threads during shutdown is
pointless as the shutdown process will wake them up anyway to terminate them.
If the checkpoint should be interrupted, the thread returns immediately;
otherwise, it proceeds to checkpoint[^1].

Next it triggers compilation of any methods that should be compiled proactively
and waits for them to finish.

Then, it calls
`TR::CRRuntime::suspendJITThreadsForCheckpoint` which:

1. Suspends Compilation Threads
2. Suspends the Sampler Thread
3. Suspends the IProfiler Thread

It then frees any SSL certificates. Finally it sets the Checkpoint Status to
`TR_CheckpointStatus::READY_FOR_CHECKPOINT_RESTORE`, which indicates that the
compiler is now ready for checkpoint. Before returning it ensures that it
releases the Compilation Monitor and reacquires VM Access.

As will become evident shortly, all of the subtlety and complexity come from
the combination of the facts that  **i.** shutdown, jitdump, or both can occur
at any point between the start and end of `prepareForCheckpoint`,  **ii.**
coordinating the checkpointing thread and the Compiler Thread(s) to be suspended
involves using a monitor different from the one used to coordinate said Compiler
Thread(s).

## Monitors

**Compilation Monitor:** Referred to below as "Comp Monitor"; the monitor used
by the compilation infrastructure to coordinate compilation threads, compilation
entries, etc. (not to be confused with the Compilation Thread Monitor, see
[Compilation Threads](./CompilationControl.md#compilation-threads) and
 [Compilation Monitor](./CompilationControl.md#compilation-monitor) for more
details).

**Compilation Thread Monitor:** Referred to below as "CompThread Monitor"; the
monitor used by a compilation thread to suspend itself for a checkpoint, or to
wait for work.

**Checkpoint/Restore (CR) Monitor:** The monitor used by the Checkpointing
Thread to wait until the Compiler Thread(s) have finished suspending themselves.

**Sampler Monitor:** The monitor used to coordinate the Sampler Thread.

**IProfiler Monitor:** The monitor used to coordinate the IProfiler Thread.

It is worth noting that the rest of the document uses the terms `wait`,
`notify`, and `notifyAll` to refer to
[conditional variables](https://en.wikipedia.org/wiki/Monitor_(synchronization)#Condition_variables_2)
and `block` to refer to when a thread cannot continue execution until it
acquires the monitor it is blocked on.

## Suspending Compilation Threads

To maintain coherency, though not every monitor needs to always be acquired,
the monitors must always be acquired in the following order:
1. Comp Monitor
2. CompThread Monitor
3. CR Monitor

### Normal Execution (steps 5 & 6 can be interchanged)
The Checkpointing Thread at this point has the Comp Monitor in hand.
1. The Checkpointing Thread signals the Comp Threads to suspend
2. The Checkpointing Thread acquires the CR Monitor, releases the Comp Monitor,
   and waits on the CR Monitor
3. The Comp Threads acquire the Comp Monitor and the CompThread Monitor, and
   change their state to `COMPTHREAD_SUSPENDED`
4. The Comp Threads acquire the CR Monitor, call `notifyAll`, and release the CR
   Monitor
5. The Checkpointing Thread wakes up with the CR Monitor in hand, releases the
   CR Monitor, and blocks on the Comp Monitor
6. The Comp Threads release the Comp Monitor and wait on the CompThread Monitor
7. The Checkpointing Thread acquires the Comp Monitor, ensures the state has
   changed to `COMPTHREAD_SUSPENDED` for the current `compInfoPT`
8. The Checkpointing Thread checks the next `compInfoPT`, waiting on the CR
   Monitor if needed (it will release the Comp Monitor prior to waiting)
9. The Checkpointing Thread proceeds with the remaining coordination tasks

In steps 7 and 8, in addition to checking the Compilation Thread State, the
Checkpointing Thread also checks if the checkpoint should be interrupted, and
returns from `TR::CRRuntime::suspendCompThreadsForCheckpoint` if so. This
is because when the Checkpointing Thread releases the Comp Monitor to wait on
the CR Monitor, the Shutdown Thread can acquire the Comp Monitor to begin the
shutdown process.

### JIT Dump

#### Crash on compilation thread
* The crashing Comp Thread will
[return to the VM and terminate the process](https://github.com/eclipse-openj9/openj9/blob/500e0a26e2c5be6ead0f838495ae8d8cc34821e1/runtime/compiler/control/CompilationThread.cpp#L3442-L3447).
* Possible scenarios for the Checkpointing Thread:
    1. The Checkpointing Thread will try to acquire the Comp Monitor to signal
       the Comp Threads to suspend themselves but will end up blocking until the
       JVM terminates.
    2. The Checkpointing Thread will manage to signal Comp Threads to suspend,
       but will remain waiting on the CR Monitor for the crashed Comp Thread to
       suspend until the JVM terminates.

#### Crash on application thread
This scenario should not be possible, as in normal checkpoint, all application
threads will have already been suspended by this point. However, for the sake of
completeness, this scenario does not differ from the process described in the
[Normal Execution](#normal-execution-steps-5--6-can-be-interchanged) section.

### Normal Shutdown

#### Scenario 1
1. The Checkpointing Thread waits on CR Monitor
2. The Comp Thread goes to suspend and already has Comp Monitor in hand
3. The Shutdown Thread blocks on the Comp Monitor
4. The Comp Thread updates its state to `COMPTHREAD_SUSPENDED`, acquires the CR
   Monitor, and calls `notifyAll`
5. The Comp Thread releases the CR Monitor, releases the Comp Monitor, and waits
   on CompThread Monitor
6. The Checkpointing Thread wakes up with the CR Monitor in hand, releases the
   CR Monitor, acquires the Comp Monitor and continues on to next thread
7. The Checkpointing Thread checks the state of the Comp Thread and moves onto
   the next threads possibly finding them all suspended.
8. The Checkpointing Thread releases the Comp Monitor and returns from the hook
9. The Shutdown Thread goes through the sequence of stopping all threads

#### Scenario 2
Repeat steps 1-5 from Scenario 1

6. The Shutdown Thread acquires the Comp Monitor
7. The Checkpointing Thread wakes up with the CR Monitor in hand, releases the
   CR Monitor and blocks on the Comp Monitor
8. The Shutdown Thread sets `TR::CRRuntime::_checkpointStatus` to
   `TR_CheckpointStatus::INTERRUPT_CHECKPOINT`. Additionally (though irrelevant
   in this scenario) it also acquires the CR Monitor, calls `notify`, and
   releases the CR Monitor
9. The Shutdown Thread finishes stopping all threads and releases the Comp
   Monitor
10. The Checkpointing Thread acquires the Comp Monitor, sees that the checkpoint
    should be interrupted
11. The Checkpointing Thread breaks out of the loop, releases the Comp Monitor,
    and returns from the hook

#### Scenario 3 (steps 5 & 6 can be interchanged)
1. The Checkpointing Thread waits on the CR Monitor
2. The Shutdown Thread acquires the Comp Monitor
3. The Comp Thread blocks on the Comp Monitor in order to (eventually) suspend
   itself
4. The Shutdown Thread sets `TR::CRRuntime::_checkpointStatus` to
   `TR_CheckpointStatus::INTERRUPT_CHECKPOINT`, acquires the CR Monitor, calls
   `notify`, and releases the CR Monitor
5. The Shutdown Thread goes about the task of stopping the Comp Threads
6. The Checkpointing Thread wakes up with the CR Monitor in hand, releases the
   CR Monitor and blocks on the Comp Monitor
7. The Shutdown Thread finishes stopping all threads, and releases the Comp
   Monitor
8. The Checkpointing Thread acquires the Comp Monitor, sees that the checkpoint
   should be interrupted
9. The Checkpointing Thread breaks out of the loop, releases the Comp Monitor,
   and returns from the hook

It should be noted that when the Checkpointing Thread runs
`prepareForCheckpoint`, either it will succeed in signalling the Comp Threads to
suspend or it will wait indefinitely on the Comp Monitor (in the case of a JIT
Dump) or it will abort after acquring the Comp Monitor if the Shutdown Thread
already finished its task. Once the Checkpointing Thread reaches the point where
it waits on the CR Monitor, the scenarioes above can occur.

### Shutdown during JIT Dump

#### Crash on compilation thread
In all of the scenarios described [above in normal shutdown](#normal-shutdown):
- The Checkpointing Thread remains waiting on the CR Monitor
- The Shutdown Thread blocks on the Comp Monitor
- The Comp Thread returns to VM and terminates the process

#### Crash on application thread
This scenario should not be possible, as in normal checkpoint, all application
threads will have already been suspended by this point. However, for the sake of
completeness, this scenario does not differ from the process described in the
[Normal Shutdown](#normal-shutdown) section.


## Suspending the Sampler Thread

Coordinating the Sampler Thread involves three monitors:
1. The Comp Monitor
2. The Sampler Monitor
3. The CR Monitor

### Normal Execution
The Checkpointing Thread at this point has the Comp Monitor in hand. Note, steps
4-8 can happen before steps 1-3; this is because the Checkpoint Status will
already have been set to `TR_CheckpointStatus::SUSPEND_THREADS_FOR_CHECKPOINT`.
1. The Checkpointing Thread acquires the Sampler Monitor
2. The Checkpointing Thread calls `j9thread_interrupt` to interrupt the Sampler
   Thread (in case it is in the middle of a timed sleep)
3. The Checkpointing Thread checks if the Sampler Thread Lifetime State is
   `TR::CompilationInfo::SAMPLE_THR_SUSPENDED`; if it's not then it releases
   the Sampler Monitor, acquires the CR Monitor, releases the Comp Monitor, and
   waits on the CR Monitor
4. The Sampler Thread checks if the `TR::CRRuntime::_checkpointStatus` is
   set to `TR_CheckpointStatus::SUSPEND_THREADS_FOR_CHECKPOINT`; if so it
   acquires the Comp Monitor[^2]
5. The Sampler Thread acquires the Sampler Monitor
6. The Sampler Thread sets the Sampler Thread Lifetime State to
   `TR::CompilationInfo::SAMPLE_THR_SUSPENDED`
7. The Sampler Thread acquires the CR Monitor, calls `notifyAll`, and releases
   the CR Monitor
8. The Sampler Thread releases the Comp Monitor and waits on the Sampler Monitor
9. The Checkpointing Thread resumes with the CR Monitor in hand; it releases
   the CR Monitor, and acquires the Comp Monitor

In order to maintain coherency, the Comp Monitor must always be acquired before
either of the other two. However, a subtlety does exist with respect to the
Sampler Monitor and the CR Monitor. The Checkpointing Thread releases the
Sampler Monitor before acquring the CR Monitor. However, the Sampler Thread does
**not** release the Sampler Monitor before acquiring the CR Monitor. This is to
ensure that the Sampler Thread Lifetime State does not change because of an
event such as Shutdown before the Sampler Thread has had a chance to suspend
itself for Checkpoint. This inconsistency can only create a deadlock if the
Checkpointing Thread tries to acquire the Sampler Monitor with the CR Monitor in
hand (which it never does).

### Shutdown

The Shutdown process can be initiated at any point above where the Comp Monitor
or the Sampler Monitor is not held by either the Checkpointing or the Sampler
Thread. Therefore, in Step 3, the Checkpointing Thread also checks if the
checkpoint should be interrupted, and breaks out of the loop if so. On the side
of the Sampler Thread, shutdown cannot occur once the Sampler Thread acquires
the Comp Monitor and verifies that the Checkpoint Status is
`TR_CheckpointStatus::SUSPEND_THREADS_FOR_CHECKPOINT`. If Shutdown occurs prior
to that, the Sampler Thread will not execute the main body of
`suspendSamplerThreadForCheckpoint` and terminate normally; if Shutdown occurs
after the Sampler Thread has suspended itself, the Shutdown Thread will notify
the Sampler Thread to initiate shutdown via the Sampler Thread Lifetime State.

## Suspending the IProfiler Thread

Coordinate the IProfiler Thread involves three monitors:
1. The Comp Monitor
2. The IProfiler Monitor
3. The CR Monitor

### Normal Execution
The Checkpointing Thread at this point has the Comp Monitor in hand.
1. The Checkpointing Thread acquires the IProfier Monitor
2. the Checkpointing Thread sets the IProfiler Thread Lifetime State to
   `TR_IProfiler::IPROF_THR_SUSPENDING`[^3]
3. The Checkpointing Thread calls `notifyAll`; this is because during shutdown
   both the IProfiler Thread and the Shutdown Thread could be waiting on the
   IProfiler Monitor
4. The Checkpointing Thread checks if the IProfiler Thread Lifetime State is
   `TR_IProfiler::IPROF_THR_SUSPENDED`; if it's not, then it releases the
   IProfiler Monitor, acquires the CR Monitor, releases the Comp Monitor, and
   waits on the CR Monitor.
5. The IProfiler Thread can either be waiting for work and therefore resumed due
   to the notify sent by the Checkpointing Thread, or is actively processing
   buffers. In the case of the latter, it will continue to do so until the
   working queue is empty.
6. The IProfiler Thread checks if the IProfiler Thread State is
   `TR_IProfiler::IPROF_THR_SUSPENDING`; if so it acquires the Comp Monitor[^2]
7. The IProfiler Thread acquires the IProfiler Monitor
6. The IProfiler Thread sets the IProfiler Thread Lifetime State to
   `TR_IProfiler::IPROF_THR_SUSPENDED`
8. The IProfiler Thread acquires the CR Monitor, calls `notifyAll`, and releases
   the CR Monitor
9. The IProfiler Thread releases the Comp Monitor and waits on the IProfiler
   Monitor
10. The Checkpointing Thread resumes with the CR Monitor in hand; it releases
    the CR Monitor, and acquires the Comp Monitor

In order to maintain coherency, the Comp Monitor must always be acquired before
either of the other two. However, a subtlety does exist with respect to the
IProfiler Monitor and the CR Monitor. The Checkpointing Thread releases the
IProfiler Monitor before acquring the CR Monitor. However, the IProfiler Thread
does **not** release the IProfiler Monitor before acquiring the CR Monitor. This
is to ensure that the IProfiler Thread Lifetime State does not change because of
an event such as Shutdown before the IProfiler thread has had a chance to
suspend itself for Checkpoint. This inconsistency can only create a deadlock if
the Checkpointing Thread tries to acquire the IProfiler Monitor with the CR
Monitor in hand (which it never does).

The coordination of the IProfiler Thread is a little different from the Sampler
Thread because unlike the Sampler Thread which only performs a timed sleep, the
IProfiler Thread waits until it needs to process a buffer that is added to the
working queue. Additionally, the IProfiler will continue to process buffers
before it suspends itself so that it does not have any outstanding buffers post
restore.

### Shutdown

The Shutdown process can be initiated at any point during the process of
suspending the IProfiler Thread where the Comp Monitor or the IProfiler Monitor
is not held by either the Checkpointing or the IProfiler Thread. Therefore, in
Step 4, the Checkpointing Thread also checks if the checkpoint should be
interrupted, and breaks returns if so. On the side of the IProfiler Thread,
shutdown cannot occur once the IProfiler Thread acquires the Comp Monitor and
verifies that the Checkpoint Status is
`TR_CheckpointStatus::SUSPEND_THREADS_FOR_CHECKPOINT`. If Shutdown occurs prior
to that, the IProfiler Thread will not execute the main body of
`suspendIProfilerThreadForCheckpoint` and terminate normally; if Shutdown occurs
after the IProfiler Thread has suspended itself, the Shutdown Thread will notify
the IProfiler Thread to initiate shutdown via the IProfiler Thread Lifetime
State.

# Preparing for Restore

As mentioned above, `TR::CRRuntime::prepareForRestore` handles all the
coordination needed to prepare the compiler for restore.

First this method calls `J9::OptionsPostRestore::processOptionsPostRestore` to
process any options set post-restore. Once this task is done, it acquires the
Comp Monitor. It then resets the Checkpoint Status to
`TR_CheckpointStatus::NO_CHECKPOINT_IN_PROGRESS`.

Next it resets the Start time, and proceeds to invoke
`resumeJITThreadsForRestore`, which
1. Resumes the IProfiler Thread
2. Resumes the Sampler Thread
3. Resumes the Compilation Threads

It then ensures that Swap Memory is availble before returning back to the VM.

## Resetting the Start Time
While the Checkpoint phase is conceptually part of building the application, in
order to ensure consistency with parts of the compiler that memoize elapsed
time, the start time is reset to pretend like the JVM started
`persistentInfo->getElapsedTime()` milliseconds prior to time at restore.

## Resuming the IProfiler Thread
The Checkpointing Thread at this point has the Comp Monitor in hand.
1. The Checkpointing Thread acquires the IProfiler Monitor
2. The Checkpointing Thread sets the IProfiler Thread Lifetime State to
   `TR_IProfiler::IPROF_THR_RESUMING`
3. The Checkpointing Thread calls `notifyAll`, and releases the IProfiler
   Monitor
4. The IProfiler Thread resumes with the IProfiler Monitor in hand; it releases
   the IProfiler Monitor, acquires the Comp Monitor, and then reacquires the
   IProfiler Monitor. This is to maintain the
   [coherency described above](#normal-execution-1).
5. The IProfiler Thread sets the IProfiler Thread Lifetime State to
   `TR_IProfiler::IPROF_THR_INITIALIZED`
6. The Iprofiler releases the IProfiler Monitor and the Comp Monitor, and
   returns to execute `processWorkingQueue`.

## Resuming the Sampler Thread
The Checkpointing Thread at this point has the Comp Monitor in hand.
1. The Checkpointing Thread acquires the Sampler Monitor
2. The Checkpointing Thread sets the Sampler Thread Lifetime State to
   `TR_IProfiler::IPROF_THR_RESUMING`
3. The Checkpointing Thread calls `notifyAll`, and releases the Sampler
   Monitor
4. The Sampler Thread resumes with the Sampler Monitor in hand; it releases
   the Sampler Monitor, acquires the Comp Monitor, and then reacquires the
   Sampler Monitor. This is to maintain the
   [coherency described above](#normal-execution)
5. The Sampler Thread sets the Sampler Thread Lifetime State to
   `TR::CompilationInfo::SAMPLE_THR_INITIALIZED`
5. The Sampler releases the Sampler Monitor and the Comp Monitor, and
   returns to execute `samplerThreadProc`

## Resuming Compilation Threads
The Checkpointing Thread at this point has the Comp Monitor in hand. It simply
invokes the existing `TR::CompilationInfo::resumeCompilationThread()` method,
which ensures the appropriate amount of compilation threads are resumed (which
could potentially be different from the number of threads pre-checkpoint because
of the post-restore options).


[^1]: As seen later on, even if a shutdown event occurs after this point, the
checkpoint will still be interrupted.
[^2]: Technically it first checks without the Comp Monitor in hand, and then
acquires the Comp Monitor and checks again.
[^3]: Actually, it only does so if the state is not
`TR_IProfiler::IPROF_THR_STOPPING` to ensure that Shutdown is respected.