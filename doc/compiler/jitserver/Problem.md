<!--
Copyright IBM Corp. and others 2018

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

# Debugging JITServer

Typical OpenJ9 debugging procedures can be followed for JITServer, but there are a few JITServer specific tips and tricks.
Unlike regular OpenJ9 where everything runs in one process, JITServer technology moves the JIT compiler activity into a separate process while constantly
working with addresses/objects that are only valid on the client. This makes it very easy to cause a bug on JITServer and may make it hard to catch.
In this document we will explore some common types of bugs and crashes that are encountered in JITServer development and how to deal with them.
This document should be expanded upon whenever possible.

1. [Server-side crashes](#server-side-crashes)
2. [Client-side crashes](#client-side-crashes)
3. [Crashes in compiled code](#crashes-in-compiled-code)

## Server-side crashes

### Common crash causes (server)

The most common type of a server crash is a segmentation fault caused by the compiler trying to dereference an address that is only valid on the client.
For example, dereferencing `J9Class` pointer will lead to immediate crash in the best case, or some corrupt data being written in the worst case.
If you suddenly discover a crash on the server that you haven't seen before, it's most likely caused by a change to mainline JIT code that did not account
for JITServer. If that's the case, the crash should be in the newly added code. You should find the commit that introduced it, and rewrite it in such a way
that accounts for JITServer, i.e. do not dereference client pointers directly but fetch the result from the client first.

### `-XX:+RequireJITServer`

If you want the client to terminate immediately once the server crashes, pass it `-XX:+RequireJITServer` option. This will cause the client
to exit if a remote compilation fails with `compilationStreamFailure` error code, which indicates that the server is missing/crashed or a network error has occured.
This option can be useful when you are running a suite of tests, e.g. `_sanity.functional` and want to detect any server crashes. Once the crash happens,
the current and all subsequent tests will fail, and you will be able to determine which test caused the server to crash.

### JitDump

JitDump can be a useful tool to debug compile-time crashes, as it is supported for JITServer compilations.
When a compilation thread crashes on the server, JitDump thread will trace the IL of the crashed method and attempt a remote compilation of the same method on a diagnostic thread, with tracing enabled. The resulting jitdump can then be used to determine the exact step in the compilation the crash occurs.

## Client-side crashes

### Common crash causes (client)

If the client crashes in `handleServerMessage` after receiving some bad data from the server, you will need to find out what the server was doing when it sent the message. The easiest way to do this is by running both the server and client in gdb, then sending the server a Ctrl-C after the client crashes. From the gdb prompt on the server you can type `thread apply all backtrace` and find the appropriate compilation thread to determine where the server was.

## Crashes in compiled code

Another way a client can crash is when it runs a remotely compiled method but something is wrong with it. These can be some of the tougher bugs to fix
because you often need to find the failing method and look at its trace log to find the problem. Sometimes, the failure is not caused by one compilation
but by a series of them, which makes debugging even harder.

Tracing method compilation is the same as for non-JITServer - use `-Xjit:{<method_name_regex>}(traceFull,log=<log_name>)` in the client options and both client
and server will produce trace logs. Passing options for limit files and other tracing options also works in the same way. In some cases (when the method is very large
and compiled at a high optimization level) having the server generate these logs will slow down method compilation enough that timing-sensitive bugs will happen less frequently.
If that happens, consider setting the environment variable `TR_JITServerShouldIgnoreLineNumbers=1` at the server. This has the downside of suppressing the line numbers in
all the generated logs, but the resulting reduction in network overhead can improve the compilation times of traced methods enough to be worth it.

To find if a problem is happening at the client side due to JITServer-specific issues in compiled code, remote compilation of the identified methods can be excluded by specifying `-Xjit:remoteCompileExclude={<method_name_regex>}`. Passing this option at the client side ensures that only local compilations will be performed on methods matching the specified regex pattern.

Another useful technique to find JITServer-specific issues in compiled code is to compare trace log for a remotely compiled method
with a trace log for the same method but compiled locally.
Differences in logs may reveal bugs when JITServer takes incorrect optimizer/codegen paths.
Passing `-Xjit:enableJITServerFollowRemoteCompileWithLocalCompile` to the client makes every compilation take place both on the server
and locally, making it easier to generate trace files for comparison. It is recommended to only enable the above option for the compilations you want to trace, not for all compilations, as it makes everything much slower.

There are many possible reasons why JITServer might be producing incorrect compiled method body.
Here are some of them:

- **Missing relocation**: although missing relocations are pretty rare in AOT code, it is possible that JITServer needs a relocation
that AOT does not. This is because JITServer supports more optimizations/better code paths than AOT since it does not have to make code
executable on any VM, just on the client one, and it can fetch client addresses during compilation. The fix is to either create a new
relocation or just embed the correct address in the compiled body right away. If it's something that needs to happen infrequently, and does not need to be supported for AOT,
we usually go for the latter solution, as it is easier to implement.
- **Missing VM information**: client needs to inform the server about some VM options that can affect compilation, e.g. GC mode.
If some information about the VM setup is missing, server might compile something that will not run correctly. Usually, such information
should go into the `VMInfo` object on the server.


For debugging JITServer AOT Cache related issues, there are two options that can be useful to determine whether a problem with a given method is caused by an AOT compiler issue or by a JITServer AOT cache method serialization/deserialization issue:

- `-Xaot:jitserverLoadExclude={<method_name_regex>}` option prevents the specified method(s) from being loaded from the JITServer AOT cache.
- `-Xaot:jitserverStoreExclude={<method_name_regex>}` option prevents the specified method(s) from being stored to the JITServer AOT cache.

Note that these options should be specified on the client side.
