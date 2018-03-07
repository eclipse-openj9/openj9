<!--
Copyright (c) 2016, 2018 IBM Corp. and others

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

# list of dependent libs

  * asm-all-6.0_BETA.jar
  * asmtools-6.0.jar
  * commons-cli-1.2.jar
  * commons-exec-1.1.jar
  * javassist-3.20.0-GA.jar
  * jcommander-1.48.jar
  * junit-4.10.jar
  * testng-6.10.jar

These libs will be downloaded automatically as part of `make compile` 
process.

Valhalla tests currently make use of the nestmates capabilities which 
are available starting in asm version 7. In order to run the Valhalla 
nestmates on a local machine, you must manually get the asm-7.0.jar and
put it in TestConfig/lib/. Avoid using asm 7 in other tests as this is 
a temporary solution.

