<!--
Copyright IBM Corp. and others 2023

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

# Overview

This doc goes over how options are processed post restore in a
Checkpoint/Restore environment. The `TR::OptionsPostRestore` class
encapsulates all the logic that performs this processing. The
semantics of compiler options when they are processed post restore
are described in
[https://github.com/eclipse-openj9/openj9/issues/16714](this)
issue.

`TR::OptionsPostRestore` has two public APIs:
1. The Constructor
2. `processOptionsPostRestore`
which are used to first construct the object, and initiate the process
of parsing and processing compiler options to be applied in the
post-restore environment. Some options are applied retroactively,
whereas others only apply going forward. Unlike options processing at
the JVM startup, post restore options processing uses the
`vm->checkpointState.restoreArgsList` args array.

First `OptionsPostRestore` searches the args array for `-Xjit`,
`-Xnotjit`, `-Xaot`, and `-Xnoaot`. If `-Xnoaot` appears after `-Xaot`
then both compiling and loading AOT code going forward is disabled.
If `-Xnojit` appears before `-Xjit`, compiling JIT code going forward
is disabled; however, additionlly, all previously compiled code in the
code cache is invalidated.

Next, `preProcessInternalCompilerOptions` performs tasks that should
be done prior to parsing and processing `-Xjit:` and `-Xaot:` options.
`processInternalCompilerOptions` is then called for both `-Xjit:` and
`-Xaot:` which parses and processes the options (if they exist).
Following this step, `iterateOverExternalOptions` iterates over and
processes `-X` and `-XX` options. Then, if the JIT is not disabled
(because of `-Xnojit`), JITServer options are processed in
`processJitServerOptions`.

Finally, `postProcessInternalCompilerOptions` performs tasks that need
to be done after the `-Xjit:`, `-Xaot:`, and external options have all
been processed.

# Important Notes

### JIT and AOT compiled code

There is a difference in how compiled code is perceived post restore.
In a normal JVM run, JIT code is code that is compiled in the current
JVM instance; AOT code is code that is loaded from the SCC - it may or
may not have been compiled in the current JVM instance. However,
because at the start of the JVM, there is no code, it makes sense to
distinguish code _that is yet to be placed into a code cache_
based on its provenance.

In a CRIU run, as far as the post-restore options processing is
concerned, all existing code in the code cache is JIT code. The idea
is that once code is installed in the code cache, whether the code
was generated in this JVM instance or loaded from the SCC is
irrelevant except for debugging (at which point, the provenance can
still be determined by looking at the `TR_PersistentJittedBodyInfo`).

### `-Xnojit` and `-Xnoaot`

In a normal JVM run, `-Xnojit` means that the Compiler component will
be initialized but it won't perform any JIT compilation; it will still
perform AOT load and AOT compilation. Similarly, `-Xnoaot` means that
the Compiler won't perform any AOT load or AOT compilation, but it
will till perform JIT compilation.

Post restore, these two options behave in a similar manner however there
is one big difference: `-Xnojit` will invalidate all previously compiled
code in the code cache regardless of whether it was a JIT compile or an
AOT load. As such, in order to disable all compiled code, one needs to
specify `-Xnojit` (to disable JIT compilation and invalidate all previously
compiled code) and `-Xnoaot` to disable AOT load/compilation.

If `-Xnoaot` was specified pre-checkpoint, there is a lot of infrastructure
that needs to be set up, including validating the existing SCC. Therefore,
if `-Xnoaot` was specified pre=chekpoint, `-Xaot` is ignored post-restore.

### `-Xjit:exclude={*}`

`-Xjit:exclude={*}` is very similar to `-Xnojit` in terms of its effect.
All existing JIT code is invalidated. Further JIT compilation ends up
disabled because `exclude={*}` filters out all methods.

### Invalidating method bodies

In order to invalidate code, the method body needs to have a
`TR_PersistentJittedBodyInfo`; method bodies, such as JNI thunks, do
not have a body info, and therefore it is impossible to prevent
invalidate these bodies; they will continue to be executed even with
`-Xnojit`/`-Xjit:exclude={*}`.

### JIT code on the application stack at checkpoint

If an application thread has a compiled method on its stack, it is not
possible without OSR to prevent executing that code when the stack
unwinds, even with `-Xnojit`/`-Xjit:exclude={*}`.

### Asynchronous and Synchronous Compilation

Because synchronous and asynchronous compiled methods have different
preprologues, and because the logic to trigger patching of the Start PC
if the body gets invalidated uses a global flag to determine whether the
method was synchronous or asynchronous, at present the options
processing ignores a request to compile synchronously if it wasn't
already specified pre-checkpoint.

### Vlog/RtLog Semantics

Both the vlog and rtlog have the same semantics. If the vlog/rtlog are
specified pre-checkpoint and not post-restore, they remain unchanged. If
the vlog/rtlog is NOT specified pre-checkpoint but IS specified
post-restore, then the vlog/rtlog is opened post-restore. If they are
specified both pre-checkpoint and post-restore, then the pre-checkpoint
file(s) are closed and the post-restore file(s) are opened. A
consequence of this is that if the same file name is specified then a
new file will be opened. This can have the consequence of overwriting
the previous file if the PID of the restored process is the same.

### Start Time

While the checkpoint phase is conceptually part of building the
application, in order to ensure consistency with parts of the compiler
that memoize elapsed time, the start time is reset to pretend like the
JVM started `persistentInfo->getElapsedTime()` milliseconds ago. This
will impact options such as `-XsamplingExpirationTime`. However, such
an option may not make sense in the context of checkpoint/restore.

### `-Xrs`, `-Xtrace`, `-Xjit:disableTraps`, `-Xjit:noResumableTrapHandler`

Currently, unless specified pre-checkpoint, the compiler generates will
generate code assuming these options are not set. If these options are
then specified post-restore, the options processing code will
invalidate all JIT code in the code cache, as well as prevent further
AOT compilation (since the AOT method header would have already been
validated).

### JITServer

The following table outlines when a client JVM will connect to a
JITServer instance.

||Non-Portable CRIU Pre-Checkpoint|Non-Portable CRIU Post-Restore|Portable CRIU Pre-Checkpoint|Portable CRIU Post-Restore|
|--|--|--|--|--|
|No Options Pre-Checkpoint; No Options Post-Restore|:x:|:x:|:x:|:x:|
|No Options Pre-checkpoint; `-XX:+UseJITServer` Post-Restore|:x:|:white_check_mark:|:x:|:white_check_mark:|
|`-XX:+UseJITServer` Pre-Checkpoint; No Options Post-Restore|:x:|:white_check_mark:|:white_check_mark:|:white_check_mark:|
|`-XX:+UseJITServer` Pre-Checkpoint; `-XX:-UseJITServer` Post-Restore|:x:|:x:|:white_check_mark:|:x:|
|`-XX:-UseJITServer` Pre-Checkpoint; `-XX:+UseJITServer` Post-Restore|:x:|:x:|:x:|:x:|
