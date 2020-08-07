<!--
Copyright (c) 2019, 2020 IBM Corp. and others

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

The Shared Classes Cache (SCC) is a feature of OpenJ9 that enables sharing 
data between JVMs. For more information see [1].

## JIT

The JIT Compiler uses the SCC to store both AOT code as well as data either
necessary for AOT 
(e.g. [Class Chains](https://github.com/eclipse/openj9/blob/master/doc/compiler/aot/ClassChains.md)),
or for performace (e.g. IProfiler data). The JIT interfaces with the SCC via
the `TR_J9SharedCache` class.

### Offsets

When data that is stored into the SCC needs to refer to other data, it is
referred to by using offsets. Traditionally, the offset would always be
relative to the start of the SCC. However, if the SCC is resized, only 
offsets of data in the ROM Class section of the SCC will be invariant. 
Therefore, offsets of all data, which must be in the metadata section,
are calculated relative to the start of the metadata section. Another 
point of clarification is that offsets of data within the ROMClass section 
is calculated using the start of the ROMClass section as no ROM Structure 
pointer should point before the start of the section. The offsets that
are generated and stored as part of relocation information are first
left shifted 1 bit; the low bit is used to determine whether the offset
is relative to the start of the ROM Class section or the start of the
metadata section.

The APIs in in `TR_J9SharedCache` provide an abstraction layer so that
other parts of the compiler do not need to worry about the details. 
However, it is important that one does not use the offsets directly, but
only via the `TR_J9SharedCache` class.

<hr/>

[1] https://www.eclipse.org/openj9/docs/shrc/ 
