
#
# Copyright (c) 2001, 2018 IBM Corp. and others
#
# This program and the accompanying materials are made available under
# the terms of the Eclipse Public License 2.0 which accompanies this
# distribution and is available at https://www.eclipse.org/legal/epl-2.0/
# or the Apache License, Version 2.0 which accompanies this distribution and
# is available at https://www.apache.org/licenses/LICENSE-2.0.
#
# This Source Code may also be made available under the following
# Secondary Licenses when the conditions for such availability set
# forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
# General Public License, version 2 with the GNU Classpath
# Exception [1] and GNU General Public License, version 2 with the
# OpenJDK Assembly Exception [2].
#
# [1] https://www.gnu.org/software/classpath/license.html
# [2] http://openjdk.java.net/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
#

These tests check that missing Jars on the classpath do not cause unexpected errors

NonExistJarTests.BuildInitial1
	Build A.jar, B.jar and C.jar

NonExistJarTests.StoreWithB
	Run A_Main and store classes from A.jar B.jar and C.jar in the cache

NonExistJarTests.CheckForError1
	Run A_Main with B.jar missing from the classpath. Even though it is cached, it should throw a NoClassDefFoundError

NonExistJarTests.BuildInitial2
	Build A.jar and BC.jar with the normal classes and build B_.jar with a Rogue B class.

NonExistJarTests.StoreWithRogueB
	The test runs with the classpath A.jar;B.jar;BC.jar. Initially, B.jar does not exist.
	The test loads C as the first class, which means that all the jars will be opened on the classpath.
	After loading C, it renames B_.jar to B.jar and then tries to load B. It should load the B from BC.jar and not B.jar
	The error condition is if "ROGUE" is printed by the class in B.jar