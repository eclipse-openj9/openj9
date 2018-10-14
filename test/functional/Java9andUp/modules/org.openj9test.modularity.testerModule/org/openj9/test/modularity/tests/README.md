<!--
  Copyright (c) 2018, 2018 IBM Corp. and others
 
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
# Description

These tests exercise the visibility checks for modules.
The test mainline runs as a modular and attempt to access classes and methods in other modules via:

* Class.forName()
* Lookup.findVirtual() and findStatic()
* Lookup.findClass()
* MethodType.fromMethodDescriptorString()
* MethodHandles

We try various types of access:

* exported packages
* transitively exported packages
* opened packages
* unexported packages
* packages from unread modules

The test verifies that the correct type of exception is thrown if the package is not visible.
Note that part of the test requires a class file which cannot be expressed as a java program.