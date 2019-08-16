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

Usually typical OpenJ9 debugging procedures can be followed for JITServer. But there are a few JITServer specific tips and tricks.

If the client crashes in `handleServerMessage` after receiving some bad data from the server, you will need to find out what the server was doing when it sent the message. The easiest way to do this is by running both the server and client in gdb, then sending the server a Ctrl-C after the client crashes. From the gdb prompt on the server you can type `thread apply all backtrace` and find the appropriate compilation thread to determine where the server was.

Please add to this file!!
