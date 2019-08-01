<!--
Copyright (c) 2019, 2019 IBM Corp. and others

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

# Compiler Best Practices
This doc is meant to serve as a place to add and refresh Best Practices
that should be kept in mind when developing on the Compiler component
of Eclipse OpenJ9.

## AOT
AOT, i.e. Ahead Of Time, Compilation is basically a regular JIT compilation,
but with the added requirement that it be relocatable. Therefore, regardless
of where in the compiler work is being done, it is important to always keep
in mind how said work would apply to a relocatable compile.

### CPU Feature Validation
1. Use only the processor feature flags the JIT cares about to determine CPU features
2. If the JIT uses some CPU feature that, for whatever reason, isn't available via 
the CPU processor feature flags, it should be a Relo Runtime Feature Flag (so that 
the same query can be performed at load time) 

