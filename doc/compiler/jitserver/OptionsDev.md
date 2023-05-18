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

# JITServer Options Handling

JITServer introduces new command line options. Most of them configure JITServer parameters, such as network timeout, or server's IP address. Such options begin with `-XX:JITServer*` and are processed in `JITServerParseCommonOptions()`. The values of most options are stored in the `PersistentInfo` after they are parsed.

Another important function is `setupJITServerOptions()`. Executed during VM startup, it disables unsupported optimizations and modifies some heuristics.

## Client-side options

A client must send its command line options to the server. `-Xjit` options are used to enable/disable optimizations/tracing options, while other options specify compilation-relevant VM parameters. For instance, `-Xgc` specifies a Garbage Collection policy. Despite the server not performing any GC, not knowing the right policy can affect functional correctness of the compiled code.

Two functions - `J9::Options::packOptions` and `J9::Options::unpackOptions` are used for serializing and deserializing client options, respectively. All relevant options must be sent to the server, but if some option doesn't seem to be working you may want to try adding it to the server as well. If it starts working, then the above functions must be modified to handle the missing option. There are also a few options which are not sent because they are uncommon and difficult to serialize. Such options are set to `NULL` within `packOptions`.

Serialized options are sent to the server with each compilation request.
This is done for every compilation, because option subsets may enable or disable options for a subset of methods.
