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

# Guidelines for adding `-Xtrace` tracepoints to OpenJ9

## Table of Contents:
- Rules for adding tracepoint definitions to .tdf files
- Adding tracepoints to a module that already has some
- Tracepoint Format Specifiers
- J9TraceFormat.dat
- Adding the first tracepoint to a dynamic module
- Adding the first tracepoint to a static module
- Advanced use #1 - testing at runtime whether your tracepoint is enabled
- Advanced use #2 - adding assert type tracepoints
- Parameters to Assertions/Tracepoints and Side-effects (A warning)
 

## Rules for adding tracepoint definitions to .tdf files

1. Always **add new tracepoints at the end of a .tdf file**.
2. If a tracepoint's signature changes, **obsolete it and create a new one**. By signature we mean the types, order and total number of format specifiers in the Template. Changes to the Tracepoint Type, Overhead, Level and `NoEnv` parameters, and cosmetic changes to the text in the Template can be made without adding a new tracepoint.

These rules are in place to enforce the service requirements that a) any given tracepoint will keep its number between releases, and that b) the formatter from a newer release can be used to format a trace file from an older release. Following the rules is vital also for the Health Center tool (which captures and formats tracepoint data).

This requires all newer .tdf files to be supersets of old .tdf files. Ordering in .tdf files governs numbering, starting with the first tracepoint in the file being 0 and incrementing down the file. This is why tracepoints must be added at the end - otherwise each subsequent tracepoint's number will be incremented, and the formatter will attempt to format the tracepoint using the format string of another tracepoint. 

Setting the `Obsolete` keyword guards against usage of the old tracepoint in the current code stream, but keeps it available for the formatter to use when processing trace from older releases.


## Adding tracepoints to a module that already has some

Adding a tracepoint to a module that is already enabled for trace requires four steps:

1. Determine the trace module name
2. Add your tracepoint to .tdf file
3. Add your tracepoint to your source code
4. Test the tracepoint

1) First determine the module you are about to add tracepoints to.

The name that OpenJ9 trace uses to identify a module/component can be found in the module's trace definition (.tdf) file.  You should find a .tdf file in your module's root directory.  For example, the .tdf file for the j9vm component is runtime/vm/j9vm.tdf. The first line in that file: `Executable=j9vm` specifies the name that OpenJ9 trace (and -Xtrace options) use to identify tracepoints in that component. The .tdf file contains the specifications for all tracepoints in the j9vm component, including their format strings. Files `ut_j9vm.h` and `ut_j9vm.c`, are generated at build time from the contents of the .tdf.

2) Step two is to add the tracepoint to your .tdf file. All new tracepoints must be added to the end of the .tdf file. This ensures that tracepoints preserve their tracepoint numbers between releases, see rules above.

For example:

`TraceEvent=Trc_VM_getMethodOrFieldID_dereferencedClass Overhead=1 Level=3 Template="class=%p (%.*s)"`

This breaks down as follows:

`TraceEvent=Trc_VM_getMethodOrFieldID_dereferencedClass`  This specifies the type and the name of the tracepoint. `Trc_VM_getMethodOrFieldID_dereferencedClass` is the name of the generated macro to use in your code. The `TraceEvent=` denotes this is an event trace point. Other tracepoint types are:  `TraceEntry` `TraceExit` `TraceException` `TraceExit-Exception` `TraceAssert` `TraceDebug`.

`Overhead=1`  Each tracepoint has an overhead to determine whether it is included in the generated object code at build time. This is useful for very hot tracepoints, where even the cost of the simple 'if enabled' test at runtime is too great. The threshold in the tracepoint pre-processor is currently set to 1, so if you set Overhead to a value greater than 1, the tracepoint will expand to an empty statement at build time. It can still be useful to have the tracepoint defined in the source code, but of course requires a custom re-build of the relevant library as and when actually needed for diagnostics.

`Level=3`  Each tracepoint has a level, used to control the volume of trace production at runtime. Level 1 and 2 tracepoints are switched on by default, and are written to the cyclic memory buffers for use in the flight recorder GC history and snap trace dumps. Level 2 trace points are enabled after the VM exits the startup phase (or before that if any other trace settings are enabled). Level 2 should be used for trace points that should be on by default once an application is running but are too frequently hit during the startup phase of an application to be in level 1. For example class loading trace points. If a tracepoint in the default set causes or is likely to cause a performance problem even outside of startup, it is raised to level 3 or higher. Levels can be 0-9, and of those levels, 0 (on by default) for extraordinary events and errors, 1 (on by default), 2 (on by default, after startup), 3, 4, 5 and 6 are commonly used to reflect increasing detail.

`Template="...."`  This is the formatting string which is associated with this tracepoint. The formatting string can include up to 16 parameters, using the format specifiers listed in the table below. This format string should remain the same (wrt the type, order and total number of format specifiers) between releases. If the type, order or total number of format specifiers needs to change, you must create a new tracepoint. Again this allows us to meet our requirement of support in the trace formatter for previous releases, and is also required for Health Center.

Some tracepoints have the `NoEnv` parameter specified, for example:

`TraceEvent=Trc_VM_openNativeLibrary NoEnv Overhead=1 Level=1 Template="Attempting to open %s from classpath %s"`

`NoEnv`  Normally, tracepoint macros have the current `J9VMThread` passed in as the first parameter. This is used by the trace engine to locate the per-thread trace buffer into which the tracepoint is written. The `NoEnv` keyword denotes that a current `J9VMThread` is not readily available in the calling code and will not be passed in. Tracepoints which use `NoEnv` take extra time. See the `UT_THREAD_FROM_ENV` macro in `runtime/rastrace/tracewrappers.c`. The trace engine internally calls `j9thread_self()` and `j9thread_tls_get()` to find the trace buffer for the thread (if any).

Other parameters:

`Obsolete` Guards against usage of the old tracepoint in the current code stream, but keeps it available for the formatter to use when processing trace from older releases.

`Test` See the Advanced use #1 section.


### Tracepoint Format Specifiers

| Specifier | Description |
| --------- | ----------- |
| %d | 32-bit integer, formatted as signed decimal |
| %u | 32-bit integer, formatted as unsigned decimal |
| %lld | 64-bit integer, formatted as signed decimal |
| %llu | 64-bit integer, formatted as unsigned decimal |
| %x | 32-bit integer, formatted as hexadecimal |
| %llx | 64-bit integer, formatted as hexadecimal |
| %zu | platform-dependent size_t (equivalent to J9 UDATA), formatted as unsigned decimal |
| %zd | platform-dependent size_t (equivalent to J9 IDATA), formatted as signed decimal |
| %zx | platform-dependent size_t (equivalent to J9 UDATA), formatted as hexadecimal |
| %f | float or double, formatted in fixed-point notation |
| %p | 32-bit pointer in 32-bit builds, 64-bit pointer on 64-bit builds, formatted as hexadecimal |
| %s | pointer to null-terminated character array, formatted as character string |
| %.*s | 32-bit unsigned integer followed by pointer to character array, formatted as character string of specified length |


The `ut_xxx.h` and `ut_xxx.c` files are refreshed by the tracegen step of the build. `ut_xxx.h` contains the macro you need.

The .tdf entry above generates the following macro in `ut_j9vm.h`:

```
#if UT_TRACE_OVERHEAD >= 1
#define Trc_VM_getMethodOrFieldID_dereferencedClass(thr, P1, P2, P3) do { /* tracepoint name: j9vm.123 */ \
	if ((unsigned char) j9vm_UtActive[123] != 0){ \
		j9vm_UtModuleInfo.intf->Trace(UT_THREAD(thr), &j9vm_UtModuleInfo, ((123u << 8) | j9vm_UtActive[123]), "\6\12\377", P1, P2, P3);} \
	} while(0)
#else
#define Trc_VM_getMethodOrFieldID_dereferencedClass(thr, P1, P2, P3)   /* tracepoint name: j9vm.123 */
#endif
```

Note the number of the new tracepoint is detailed in the comment - this example is `j9vm.123`.

Tracegen also updates `lib/J9TraceFormat.dat`, which contains the tracepoint information used by the trace formatter (the Template string from your new tracepoint goes in there).

3) Nearly there - add the tracepoint macro to your source code:

```
Trc_VM_getMethodOrFieldID_dereferencedClass(vmThread, clazz,
        (U_32)J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(clazz->romClass)),
        J9UTF8_DATA(J9ROMCLASS_CLASSNAME(clazz->romClass)));
```

A `J9VMThread` pointer is passed in as the first parameter (unless the tracepoint has been defined as `NoEnv`, see above). This must be the VM thread that the current code is executing on. It allows the trace engine to store the tracepoint in the per-thread trace buffer belonging to that thread, without the use of locks.

There is no compile-time or run-time checking that the parameters you supply on a tracepoint match the format specifiers you put in the tracepoint definition. Any mis-matches, eg passing an integer when the tracepoint definition was `%s`, or passing in a 64-bit pointer where the tracepoint definition was `%x`, will typically damage the next parameter if any, or cause a crash in the trace engine.
 

4) All done, now test it:

Build the code, and test the tracepoint using a -Xtrace command line option such as the following (with a testcase that exercises the relevant code path):

`java -Xtrace:print=j9vm.122-123 -version`

```
15:18:14.107 0x2b10500            j9vm.122      > getMethodOrFieldID (name=in signature=Ljava/io/InputStream; jclass=0000000002B6E830 isField=1 isStatic=1)
15:18:14.113 0x2b10500            j9vm.123      - class=0000000002B6E800 (java/lang/System)
15:18:14.134 0x2b10500            j9vm.122      > getMethodOrFieldID (name=getBoolean signature=(Ljava/lang/String;)Z jclass=0000000002BAAA70 isField=0 isStatic=1)
15:18:14.145 0x2b10500            j9vm.123      - class=0000000002B46200 (java/lang/Boolean)
```

This example uses `-Xtrace:print`, which invokes the runtime trace formatter to load your template string from `lib/J9TraceFormat.dat`, format your tracepoint's parameters into the template, and print it on stdout. For higher performance, the trace engine writes the raw/binary tracepoint data to memory buffers, then to disk, and formatting is done off-line. To test this option, use for example:

`java -Xtrace:maximal=j9vm.123,output=trace.out -version`

Then format the trace output using

```
traceformat trace.out
```

Note that in addition to the tracepoints selected on the `-Xtrace` option, you will see the default tracepoints in the output file as well, unless you specify `-Xtrace:none` ahead of your trace option.


## J9TraceFormat.dat

The tracepoint ID, type, overhead, level, name and format specification are included in `lib/J9TraceFormat.dat`, and the file is a useful readable reference for all the tracepoints in the VM.  For example:

`j9bcu.14 4 1 2 N Trc_BCU_internalLoadROMClass_Exit "BCU internalLoadROMClass: result=%d"`

The format of each tracepoint entry in the .dat file is:

`<component>.<n> <t> <o> <l> <f> <symbol> <template>`

`<component>.<n>` is the tracepoint component name and number.

`<t>` is the tracepoint type (0 through 12), these types are used:

```
0 = event
1 = exception
2 = function entry
4 = function exit
5 = function exit with exception
8 = debug
12 = assert
```

`<o>` is the overhead (0 through 10), which determines whether the tracepoint is compiled into the runtime VM code.

`<l>` is the level of the tracepoint (0 through 9). High frequency tracepoints, known as hot tracepoints, are assigned higher level numbers.

`<f>` is an internal flag (Y/N), no longer used.

`<symbol>` is the internal symbolic name of the tracepoint.

`<template>` is the template (in double quotation marks) that is used to format the entry.


## Adding the first tracepoint to a dynamic module

Ok, you want to enable a new module for trace. You need to set up this module as a trace module. Fortunately this isn't too hard, but there are several steps to follow.

1. Determine a unique and representative name for your module
2. Create a .tdf file for your module
3. The build
4. Include the generated .c file in your module's build (this usually involves an update to a module.xml or CMakeLists.txt file)
5. Include the generated header file in the source files that will be issuing the tracepoint
6. Add a call to the startup of your module to register it with trace engine at runtime
7. Optionally add a call to shutdown your module with trace when it unloads

1) Create a module name

You are largely on your own here, but search the source to make sure you choose a new one.

2) Create a .tdf file for your module

Start it with the key value pair `Executable=modName` where modName is what you came up with in 1).

Add a clause that lets the trace engine know which .dat file your tracepoints are to be recorded in. For an OpenJ9 component, this will be:

`DATFileName=J9TraceFormat.dat`

Add any of the optional submodules you may wish your component to use:

`Submodules=j9vmutil,j9util`

This tells your module that it's initialization code needs to call the trace initialization function (`UT_MODULE_LOADED`) for these modules as well. This code is automatically generated. If you do not include the submodules (typically from the statically linked modules that your dynamic module uses) that you are using then the tracepoints and asserts in these modules will be disabled when your module calls them.

3) The build

The tracegen step of a build creates a .c file and a header file for your module, with names like `ut_xxx.c` and `ut_xxx.h` where `xxx` is your module name.

4) Add the `ut_xxx.c` file to your module's makefiles

In a `CMakeLists.txt` file, add `${CMAKE_CURRENT_BINARY_DIR}/ut_xxx.c` to the `j9vm_add_library()` file list.

The .c file contains control information and tables for your trace component, so needs to be built into an object to be linked into your module.

5) Include the generated header file in the source files that will be using the tracepoints

`#include "ut_xxx.h"`

use `extern "C"` if you are writing a C++ module.

6) Add a call to the startup of your module to register it with trace engine at runtime

You need to register the module with trace at runtime. This is normally done during the startup of your module, normally in your J9VMDllMain, after the `TRACE_ENGINE_INITIALIZED` startup stage.

The macro `UT_MODULE_LOADED` is defined in the trace header to perform this for you.

Example:

```
case JIT_INITIALIZED :
        /* Register this module with trace */
        UT_MODULE_LOADED(vm);
        Trc_MM_VMInitStages_Event1(NULL);
        break;
```

Note the tracepoint immediately after the module_loaded message. This is a convention that proves very useful.


7) Optionally add a call to shutdown your module with trace when it unloads

The macro `UT_MODULE_UNLOADED` is defined in the trace header to perform this for you. It's not strictly necessary to call this function. Indeed, currently only two modules do.

 

## Adding the first tracepoint to a static module

Follow steps 1-6 for adding the first tracepoint to a dynamic module above.

Your static module does not have a startup and shutdown, you must therefore add it to the list of submodules in the dynamic modules that use your static module in order for its tracepoints to get registered with the trace engine at runtime. E.g. the shared classes .tdf, j9shr.tdf, uses several static modules which it lists in its header's Submodules list.

`executable=j9shr Submodules=j9vmutil,j9util,pool,avl,simplepool DATFileName=J9TraceFormat.dat`


If you do not include your module in a submodules (list typically from the statically linked modules that a dynamic module uses) then the tracepoints and asserts in this modules will be disabled when another module calls a function containing them. They will only be enabled when called by the dynamic modules that have listed your module as a submodule. Every dynamic module which uses a static module needs to include it in it's submodule list.



## Advanced use #1 - testing at runtime whether your tracepoint is enabled

There is a facility to define a tracepoint with an additional option `Test`. The Tracegen processor will then generate an additional macro, which can be used to test at runtime whether your tracepoint is enabled. This is useful if your runtime code needs to do extra, expensive, work to prepare data for the tracepoint, which would be wasteful if the tracepoint is not enabled.

For example:

`TraceEvent=Trc_dump_reportDumpStart_Event1 NoEnv Overhead=1 Level=1 Test Template="JVM Requesting %s Dump using filename=%s"`

Note the additional `Test` option in this tracepoint definition. The generated macros look like this:
 
```
#if UT_TRACE_OVERHEAD >= 1
#define TrcEnabled_Trc_dump_reportDumpStart_Event1  (j9dmp_UtActive[2] != 0)
#define Trc_dump_reportDumpStart_Event1(P1, P2) do { /* tracepoint name: j9dmp.2 */ \
   if ((unsigned char) j9dmp_UtActive[2] != 0){ \
      j9dmp_UtModuleInfo.intf->Trace(. . . .
   } while(0)
#else
 . . . .
#endif
```

The additional `TrcEnabled_Trc_dump_reportDumpStart_Event1` macro expands to just the test whether the tracepoint is enabled, and can be used in advance of the tracepoint, for example:

```
   if (TrcEnabled_Trc_dump_reportDumpStart_Event1) {
     . . . do expensive extra preparation

   }
   Trc_dump_reportDumpStart_Event1(. . . 
```


## Advanced use #2 - adding assert type tracepoints

There is a facility to define an assert-type tracepoint. This type of tracepoint takes a condition expression as a parameter, which if false triggers the following in the trace engine:

- an assert message to stderr,
- system, java and snap dumps and
- the JVM terminates.

Additionally for level 1 assert-type tracepoints only an assert message is written to stderr if the assertion is hit before the trace system is available. The JVM will not terminate and no dumps will be produced. Once the trace engine is available these asserts will behave as above for the rest of the JVM's life, this behaviour only applies while the trace engine is not available.

For example:

`TraceAssert=Trc_Assert_PRT_memory_corruption_detected NoEnv Overhead=1 Level=1 Assert="(P1)"`

Note the `TraceAssert` tracepoint type and the `Assert` keyword. The generated macro looks like this:

```
#define Trc_Assert_PRT_memory_corruption_detected(P1) do { /* tracepoint name: j9prt.748 */ \
   if ((unsigned char) j9prt_UtActive[748] != 0){ \
      if ((P1)) { /* assertion satisfied */ } else { \
         if (j9prt_UtModuleInfo.intf != NULL) { \
            j9prt_UtModuleInfo.intf->Trace((void *)NULL, &j9prt_UtModuleInfo, (UT_SPECIAL_ASSERTION | (748 << 8) | j9prt_UtActive[748]), "\377\4\377", __FILE__, __LINE__, UT_STR(((P1)))); \
            Trace_Unreachable(); \
         } else { \
            fprintf(stderr, "** ASSERTION FAILED ** j9prt.748 at %s:%d Trc_Assert_PRT_memory_corruption_detected%s\n", __FILE__, __LINE__, UT_STR(((P1)))); \
         } \
      }} \
   } while(0)
```

The "else" clause following the `(j9prt_UtModuleInfo.intf != NULL)` test ensures that asserts are not silent if trace has not been initialized for the module. This can happen in these cases:

- During the early startup phase of the JVM, before trace is initialized. (This is the intended use case.)
- If you have forgotten to call `UT_MODULE_LOADED` for your dynamic module.
- If you are using a static module and have forgotten to include it in the submodules list for one of the dynamic modules that uses it.

Example usage in the port library (in this case the tests are being done in advance of the tracepoint):

```
        /* Check the tags and update only if not corrupted*/
        if ((checkTagSumCheck (headerTag, J9MEMTAG_EYECATCHER_ALLOC_HEADER) == 0)
                && (checkTagSumCheck (footerTag, J9MEMTAG_EYECATCHER_ALLOC_FOOTER) == 0)
                && (checkPadding(headerTag) == 0)){
 
 .....
        } else {
                BOOLEAN memoryCorruptionDetected= FALSE;
                Trc_Assert_PRT_memory_corruption_detected(memoryCorruptionDetected);
```

Example of an assert tracepoint firing, in this case GC assert j9mm.107:

```
09:41:07.450 0x40600700 j9mm.107 ** ASSERTION FAILED ** at ../gc_modron_base/HeapRegionManager.hpp:233:
                 ((false && (heapAddress < (void *)((UDATA)(this)->_highTableEdge))))
JVMDUMP006I Processing dump event "traceassert", detail "" - please wait.
JVMDUMP032I JVM requested System dump using 'core.20110329.054107.4797.0001.dmp' in response to an event
JVMDUMP010I System dump written to core.20110329.054107.4797.0001.dmp
JVMDUMP032I JVM requested Java dump using ...
JVMDUMP010I Java dump written to ...
JVMDUMP032I JVM requested Snap dump using ...
JVMDUMP010I Snap dump written to ...
JVMDUMP013I Processed dump event "traceassert", detail "".
```

## Parameters to Assertions/Tracepoints and Side-effects (A warning)

If you include an assertion or tracepoint in your code then any expressions in the parameters P1 and P2, P3 etc will not be evaluated until after it has been determined the assertion or tracepoint is active.

This means that if the tracepoint is disabled (either individually or globally with `-Xtrace:none`) code inside P1 (or P2, P3 etc.) will not execute. This is desirable from the point of view of having low overhead trace but care must be taken to ensure that parameters do not contain code that must be executed in order for the VM to function correctly. For example:

`Assert_VM_true(0 == some_function(arg1, arg2));`

will result in `some_function` not being called if `Assert_VM_true` is disabled by any means. Using:

```
int rc = some_function(arg1, arg2);
Assert_VM_true(0 == rc);
```

is correct. The first style risks introducing bugs where the correct functioning of code relies on trace being compiled in and not disabled by the user.