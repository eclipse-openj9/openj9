<!--
Copyright (c) 2000, 2017 IBM Corp. and others

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

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
-->

# Overview

This is a work in progress FAQ to capture questions and answers on TR's IL.
New entries will be added as questions are identified in issues, code reviews,
etc.  Over time this will become a comprehensive overview of TR IL.

## Do `monenter` & `monexit` trees have to be anchored to a 'treetop'?

### Detailed question:
From [issue 475](https://github.com/eclipse/openj9/issues/475):
A log of `java/lang/StringBuffer.length()I` shows:
```
n13n      monent  jitMethodMonitorEntry[#178  helper Method]
n18n        ==>aRegLoad
...
n10n      treetop
n9n         monexit  jitMethodMonitorExit[#33  helper Method]
n18n          ==>aRegLoad
```
Is there a reason for this asymmetry between `monent` and `monexit`? Are there cases where `monexit` is not under a `treetop`?

### Answer:
Both `monenter` & `monexit` can be at the top level.  They are also allowed to be under a `treetop` as well.
They may show up under a `treetop` when they are under a `NULLCHK` that gets replaced by a `treetop`.


