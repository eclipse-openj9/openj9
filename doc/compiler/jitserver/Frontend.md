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

# JITServer Front-end

When a server is compiling methods, it needs to know lots of information about the client VM, in order to compile compatible code.
Fortunately, the JIT already has a number of classes that provide an interface for interacting with the VM. JITServer extends these classes or modifies their methods on the server.
The overridden/modified methods perform remote calls, fetching the correct VM information from a client. In some cases cached data is accessed instead of performing a remote call.

> **Note:** for more information on caching, refer to ["Caching in JITServer"](Caching.md).

In the following sections you can find descriptions of front-end classes extended/modified for JITServer. When a new method is added to any of these classes, it is likely that a corresponding JITServer version should be added as well.

## Front-end - `TR_J9ServerVM`, `TR_J9SharedCacheServerVM`

In a non-JITServer JVM, the front-end (instantiated as `TR_J9VM` or `TR_J9SharedCacheVM` for AOT) is a class that contains all types of queries for JIT to communicate with the rest of the VM. It is a legacy class, and, if possible, new queries should be added to one of the other front-end classes, to have a cleaner separation by functionality.

`TR_J9VM` is specialized on the server as `TR_J9ServerVM` in the file `runtime/compiler/env/VMJ9Server.hpp`.

`TR_J9SharedCacheServerVM` specializes `TR_J9ServerVM` for AOT compilations.

> **Note:** `TR_J9SharedCacheServerVM` extends the server VM, not `TR_J9SharedCacheVM`.

## Class environment - `J9::ClassEnv`

This class contains queries that return information or perform actions related to Java classes. Methods that require client-side information are modified to fetch it.

## VM environment - `J9::VMEnv`

This class queries the overall VM parameters, such as maximum heap size, interpreter vtable offset or whether the VM is in a startup phase. Methods that require client-side information are modified in the same manner as in `J9::ClassEnv`.

## Object model - `J9::ObjectModel`

This class accesses information related to the structure of Java classes, e.g. whether compressed refs are used, or if value types are enabled. These parameters are mostly defined at JVM startup and will not change during its lifetime. Methods are modified for JITServer in the same manner as in `J9::ClassEnv`.
