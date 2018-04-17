
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

These tests are designed to test the most basic functionality of URLClassLoader to ensure that the support exists and works.

Note here that "noTimestampChecks" stops the shared classes support from recognising that jar files have been updated

Sanity.BuildInitial
	Creates an A.jar in the test directory and an A_ext.jar in the test JRE extensions directory
	The data classes in this jar file print "CACHED" when they are run

Sanity.success
	Stores the classes in the cache from A.jar and A_Ext.jar by running A_Main
	A NoClassDefFoundError is likely to be an error with the jar extensions directory

Sanity.BuildVerify
	Builds a new A.jar and A_Ext.jar which print "LOCAL" when they are run
	The classes are in the VerifyClasses subdirectory

Sanity.verifySuccess
	Runs A_Main with "noTimestampChecks" which means that if the original "CACHED" classes are properly cached,
	they will be loaded and run. If the classes from the new jar files are loaded, "LOCAL" is printed, which is an error.

Sanity.verifyStale
	Runs A_Main without "noTimestampChecks". Expects that the JVM will spot that the jar file has changed and then
	even though the required classes are in the cache, it should load the new ones from disk instead.