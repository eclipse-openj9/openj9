<!--
Copyright (c) 2016, 2019 IBM Corp. and others

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

# Prerequisites:

  * ant 1.7.1 or above with ant-contrib.jar
  * make 3.81 or above (recommend make 4.1 or above on windows)
  * perl 5.10.1 or above with JSON, Text::CSV and XML::Parser module installed
  * curl 7.20.0 or above (needs -J/--remote-header-name support)
  
  * docker (needed if you wish to run docker-based application tests)

Note for Windows testing, cygwin is also required.  
