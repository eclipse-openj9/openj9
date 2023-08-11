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

The compiler component options initialization occurs in several stages;
this doc outlines the order in which the initialization and processing
occurs, and describes the various stages.

# High Level Outline

1. Frontend preprocess
2. JIT preprocess
3. Process options (excluding option sets)
4. JIT postprocess
5. Frontend postprocess
6. Frontend late postprocess
7. JIT late postprocess
8. Process option sets
9. JIT late postprocess on option sets
10. Frontend late postprocess on option sets

# Details

Since the compiler processes both `-Xjit` and `-Xaot`, both of which
are represented by `TR::Options`, the initialization/processing
occurs twice in two initialization phases:

1. `onLoadInternal`
    * Called when the JVM is in the `JIT_INITIALIZED` init phase.
    * This is where the global AOT and JIT `TR::Options` objects are allocated, initialized, and processed.
2. `aboutToBootstrap`
    * Called when the JVM is in the `ABOUT_TO_BOOTSTRAP` init phase.
    * This is where late post processing occurs, as well as the processing of option sets.

It is worth noting that methods below with the `fe` prefix perform
initialization that is specific to the OpenJ9 runtime whereas
methods with the `jit` prefix perform initialization that is
common across all runtimes that consume OMR.

`onLoadInternal` calls `processOptionsAOT` and `processOptionsJIT` to
process the `-Xaot` and `-Xjit` options respectively. Each of these
call `fePreProcess`, `jitPreProcess`, and `processOptions`. The
preprocessing is used to initialize options with default values prior
to parsing and processing options passed in via the command-line.

`processOptions` calls `processOptionSet`, `jitPostProcess`, and
either `fePostProcessAOT` or `fePostProcessJIT` (depending on
whether the `-Xaot` or `-Xjit` options respectively are currently
being processed). At this point in the options processing procedure,
`processOptionSet` parses and processes (via `processOption`) only
global options (i.e., options that are not specified to only apply
to specific methods); when an option set is found, it stores it for
processing later. The postprocessing is used for initialization taking
into account the fact that options may have been set via the
command-line.

`aboutToBootstrap` calls `latePostProcessAOT` and `latePostProcessJIT`
to set options that can only be set once the JVM has done most of the
necessary initialization and is ready to bootstrap. Each of these
calls `latePostProcess`, which first calls `feLatePostProcess` and
`jitLatePostProcess`.

Next, for each option set that was collected as part of the call to
`processOptionSet` above, `lastPostProcess` calls `processOptionSet`
to now parse and process the option set, followed by calls to
`jitLatePostProcess` and `feLatePostProcess`.

## Call Hierarchy

```
onLoadInternal
   OMR::Options::processOptionsAOT(char *aotOptions, void *feBase, TR_FrontEnd *fe)
      J9::Options::fePreProcess(void * base)
      OMR::Options::jitPreProcess()
      OMR::Options::processOptions(char *options, char *envOptions, void *feBase, TR_FrontEnd *fe, TR::Options *cmdLineOptions)
         OMR::Options::processOptions(char * options, char * envOptions, TR::Options *cmdLineOptions)
            OMR::Options::processOptionSet(char *options, char *envOptions, TR::Options *jitBase, bool isAOT)
               OMR::Options::processOption(char *startOption, TR::OptionTable *table, void *base, int32_t numEntries, TR::OptionSet *optionSet)
            OMR::Options::jitPostProcess()
            J9::Options::fePostProcessAOT(void * base)
   OMR::Options::processOptionsJIT(char *jitOptions, void *feBase, TR_FrontEnd *fe)
      J9::Options::fePreProcess(void * base)
      OMR::Options::jitPreProcess()
      OMR::Options::processOptions(char *options, char *envOptions, void *feBase, TR_FrontEnd *fe, TR::Options *cmdLineOptions)
         OMR::Options::processOptions(char * options, char * envOptions, TR::Options *cmdLineOptions)
            OMR::Options::processOptionSet(char *options, char *envOptions, TR::Options *jitBase, bool isAOT)
               OMR::Options::processOption(char *startOption, TR::OptionTable *table, void *base, int32_t numEntries, TR::OptionSet *optionSet)
            OMR::Options::jitPostProcess()
            J9::Options::fePostProcessJIT(void * base)
aboutToBootstrap
   OMR::Options::latePostProcessAOT(void *jitConfig)
      OMR::Options::latePostProcess(TR::Options *options, void *jitConfig, bool isAOT)
         J9::Options::feLatePostProcess(void * base, TR::OptionSet * optionSet)
         OMR::Options::jitLatePostProcess(TR::OptionSet *optionSet, void * jitConfig)
         OMR::Options::processOptionSet(char *options, char *envOptions, TR::Options *jitBase, bool isAOT)
         OMR::Options::jitLatePostProcess(TR::OptionSet *optionSet, void * jitConfig)
         J9::Options::feLatePostProcess(void * base, TR::OptionSet * optionSet)
   OMR::Options::latePostProcessJIT(void *jitConfig)
      OMR::Options::latePostProcess(TR::Options *options, void *jitConfig, bool isAOT)
         J9::Options::feLatePostProcess(void * base, TR::OptionSet * optionSet)
         OMR::Options::jitLatePostProcess(TR::OptionSet *optionSet, void * jitConfig)
         OMR::Options::processOptionSet(char *options, char *envOptions, TR::Options *jitBase, bool isAOT)
         OMR::Options::jitLatePostProcess(TR::OptionSet *optionSet, void * jitConfig)
         J9::Options::feLatePostProcess(void * base, TR::OptionSet * optionSet)
```
