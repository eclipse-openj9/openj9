<!--
Copyright (c) 2018, 2019 IBM Corp. and others

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

ResolvedMethod is a wrapper around a Java method which exists for the duration of a single compilation. It collects and manages information about a method and related classes. We need to extend it 

On the server, we extend `TR_ResolvedJ9JITServerMethod` from `TR_ResolvedJ9Method`. Upon instantiation, a mirror instance is created on the client. Instantiation happens either directly via the `createResolvedMethod` family, or indirectly using one of multiple `getResolvedXXXMethod` methods, which perform operations to locate a method of interest and then create the corresponding `ResolvedMethod`.

Many method calls which require VM access are relayed to the client-side mirror. However, some values are cached at the server to avoid sending remote messages. Since all resolved methods are destroyed at the end of the compilation, this is a good choice as a cache for data which may be invalidated by class unloading/redefinition.
