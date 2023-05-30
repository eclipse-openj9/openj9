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

# Building JITServer

The build procedure for JITServer is nearly identical to a regular OpenJ9 build.

## Building the whole VM

Build instructions for different Java versions:

- [Java 8](../../build-instructions/Build_Instructions_V8.md)
- [Java 11](../../build-instructions/Build_Instructions_V11.md)
- [Java 16](../../build-instructions/Build_Instructions_V16.md)

To build with JITServer, pass `--enable-jitserver` option to the `configure` script.

## Building compiler only

Same instructions as [here](../../build-instructions/Build_Compiler_Only.md),
but to build with JITServer set the environment variable `J9VM_OPT_JITSERVER` to `1`.
