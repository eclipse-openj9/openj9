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

Options parsing happens mostly in `control/J9Options.cpp`. Parsing of the -XX:JITServer options happens mostly in `fePostProcess` and a few helper functions. If you want to add a suboption that should probably go in `JITServerParseCommonOptions`. The values of most options are stored in the `PersistentInfo` after they are parsed.

Two functions `packOptions` and `unpackOptions` are used for sending options specified at the client to the server, so that it is not necessary to specify the same options on both the client and server. However, at the moment there is still some code that checks for command line options directly instead of going through the options for the current compilation. So for those options you must specify them at both the client and the server. You usually don't need to worry about this, but if some option doesn't seem to be working you may want to try adding it to the server as well. There are also a few options which are not sent because they are uncommon and difficult to serialize. Such options are set to `NULL` within `packOptions`.

In `J9::Options::fePostProcessJIT` there is some code that sets other options depending on whether we are in client or server mode. Most of these are workarounds or heuristics.
