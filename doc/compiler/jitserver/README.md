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

# JITServer Documentation

JITServer technology decouples the JIT compiler from the VM and lets the JIT compiler run remotely in its own process. This mechanism prevents your Java application from suffering possible negative effects due to CPU and memory consumption caused by JIT compilation.

This technology can improve quality of service, robustness, and even performance of Java applications. We recommend trying this technology if the following criteria are met:

- Your Java application is required to compile many methods using JIT in a relatively short time.
- The application is running in an environment with limited CPU or memory, which can worsen interference from the JIT compiler.
- The network latency between JITServer and client VM is relatively low.

## Quick Start

To get started with JITServer read [this guide](Usage.md) first.

## Setup/Testing

Building and testing JVM with JITServer is almost identical to building and testing a regular JVM.
Below you can find links to documents that outline the important differences and provide step-by-step instructions.

- [Build](Build.md)
- [Testing](Testing.md)

## Development

The below sections are useful for readers who want to contribute to JITServer
or those who want to learn in more detail how the technology actually works.

- [System Overview](Overview.md)
- [Client Sessions](ClientSession.md)
- [Networking](Networking.md)
- [Messaging](Messaging.md)
- [Memory Model](Memory.md)
- [Caching](Caching.md)
- [Resolved Methods](ResolvedMethod.md)
- [Frontend](Frontend.md)
- [CHTable](CHTable.md)
- [IProfiler](IProfiler.md)
- [Handling Options](OptionsDev.md)
- [Problem Determination](Problem.md)
