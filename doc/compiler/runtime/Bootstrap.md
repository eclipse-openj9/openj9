<!--
Copyright IBM Corp. and others 2025

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

The OpenJ9 VM bootstraps in a staged approach. These stages are enumerated in
the [`INIT_STAGE` enum](https://github.com/eclipse-openj9/openj9/blob/d4f97df2b8d823389467d4e9114c557b53a5d311/runtime/oti/jvminit.h#L196-L217).
Among these stages, there are a few that are relevant to the JIT. This document
describes the initialization that the JIT performs in each stage.

The VM invokes the JIT hook
```
IDATA J9VMDllMain(J9JavaVM* vm, IDATA stage, void * reserved)
```
for each stage, passing the stage as an argument. The JIT then switches on
`stage`; in each case it does work appropriate for that stage.

# Stages

The following stages are in chronological order (in a JVM that starts up
without error).

## `DLL_LOAD_TABLE_FINALIZED`

This stage is used to perform some very early bootstrap initialization via
```
bool initializeJIT(J9JavaVM *vm)
```
as well as to consume external JVM command line options relevant to the JIT.

## `SYSTEM_CLASSLOADER_SET` (`AOT_INIT_STAGE`)

The JIT defines `AOT_INIT_STAGE` to `SYSTEM_CLASSLOADER_SET`, to contrast with
`JIT_INITIALIZED` as well as to make explicit that very early AOT initialization
occurs first. Specifically, this stage is used to set the `J9JIT_AOT_ATTACHED`
runtime flag in the `J9JITConfig` (though this gets reset and set yet again
later on), as well as to set the `aotrtInitialized` boolean variable.

## `JIT_INITIALIZED`

This stage is used to perform the bulk of the main initialization of the
JIT. `-Xjit` and `-Xaot` arguments are preprocessed here, as well as the
initialization of a significant portion of the `J9JITConfig` via
```
J9JITConfig * codert_onload(J9JavaVM * javaVM)
```
Finally, The bulk of the initialization performed in this stage occurs in
```
jint onLoadInternal(J9JavaVM * javaVM, J9JITConfig *jitConfig, char *xjitCommandLine, char *xaotCommandLine, UDATA flagsParm, void *reserved0, I_32 xnojit);
```
which includes initializing
* `J9JITConfig` function pointers
* Verbose Log infrastructure
* Persistent Memory
* CH Table
* Compilation Info
* Options (including JITServer client configuration)
* Runtime Assumption Table
* Code and Data Caches
* Compilation Threads
* IProfiler

## `ABOUT_TO_BOOTSTRAP`

This stage is used to perform the remainder of the main initialization of the
JIT. This stage does some early `TR_J9SharedCache` initialization followed by
the bulk of remaining initialization performed in
```
int32_t aboutToBootstrap(J9JavaVM * javaVM, J9JITConfig * jitConfig);
```
which includes
* Options Post Processing (including FSD)
* Performing SCC Validations
* Setting up Hooks
* Creating the Sampler Thread

## `VM_INITIALIZATION_COMPLETE`

This stage is used to finish initializing the Sampler, IProfiler, and
Compilation Threads.

## `INTERPRETER_SHUTDOWN`

This stage is used to shut down the infrastructure of various components in the
JIT (including the Sampler, IProfiler, and Compilation Threads) via
```
void JitShutdown(J9JITConfig * jitConfig);
```

## `LIBRARIES_ONUNLOAD` and `JVM_EXIT_STAGE`

This stage can also invoke `JitShutdown` if the `INTERPRETER_SHUTDOWN` stage was
skipped. In addition, it frees the `J9JITConfig` as well as various data
structures allocated by the JIT.

`LIBRARIES_ONUNLOAD` is used to do an early unload of the JIT dll if the JIT
detects (in the `DLL_LOAD_TABLE_FINALIZED` stage) that the
`TR_DisableFullSpeedDebug` environment variable is set.