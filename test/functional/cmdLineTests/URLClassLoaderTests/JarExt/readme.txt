
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

These tests check that the Jar Extension mechanisms and Jar indexing work with shared classes

Note here that "noTimestampChecks" stops the shared classes support from recognising that jar files have been updated

JarExtensionsTests.BuildInitial1
	Builds A.jar, B.jar, C.jar and D.jar where:
		B.jar specifies C.jar as a "Class-Path:" in its manifest
	Builds E.jar, E1.jar, F.jar, F1.jar, G.jar, G1.jar, H.jar and H1.jar where:
		E.jar specifies E1.jar as a "Class-Path:" in its manifest
		F.jar specifies F1.jar, G.jar and G1.jar as a "Class-Path:" in its manifest
		H.jar specifies H1.jar as a "Class-Path:" in its manifest
	These Jars do not use indexing
	All of the data classes will print "CACHED" when they are loaded.

JarExtensionsTests.SingleJarInManifest
	Run A_Main which loads classes from A.jar, B.jar, C.jar and D.jar, but C.jar is not on the classpath.

JarExtensionsTests.MultipleJarsInManifests
	Run E_Main which loads classes from E.jar, E1.jar, F.jar, F1.jar, G.jar, G1.jar, H.jar and H1.jar,
	but E1.jar, F1.jar, G.jar, G1.jar and H1.jar are not on the classpath

JarExtensionsTests.BuildVerify1
	Rebuilds all of the jar files with similar class files which output "LOCAL" instead of "CACHED" when they are loaded.
	The classes are in the VerifyClasses subdirectory

JarExtensionsTests.SingleJarInManifest.Verify1
	Runs A_Main with "noTimestampChecks" which means that if the original "CACHED" classes are properly cached,
	they will be loaded and run. If the classes from the new jar files are loaded, "LOCAL" is printed, which is an error.

JarExtensionsTests.SingleJarInManifest.Verify2
	Runs A_Main without "noTimestampChecks". Expects that the JVM will spot that the jar files have changed and then
	even though the required classes are in the cache, it should load the new ones from disk instead.

JarExtensionsTests.MultipleJarsInManifests.Verify1
	Runs E_Main with "noTimestampChecks" which means that if the original "CACHED" classes are properly cached,
	they will be loaded and run. If the classes from the new jar files are loaded, "LOCAL" is printed, which is an error.

JarExtensionsTests.MultipleJarsInManifests.Verify2
	Runs E_Main without "noTimestampChecks". Expects that the JVM will spot that the jar files have changed and then
	even though the required classes are in the cache, it should load the new ones from disk instead.

JarExtensionsTests.BuildInitial2
	Exactly the same as JarExtensionsTests.BuildInitial1 - rebuild the jars with "CACHED" classes

JarExtensionsTests.CreateJarIndexes
	Add indexes to jar files B.jar E.jar F.jar and H.jar, which are the jars containing the "Class-Path:" tag

JarExtensionsTests.SingleJarInManifestWithIndex
	As JarExtensionsTests.SingleJarInManifest only with the additional jar indexes

JarExtensionsTests.MultipleJarsInManifestsWithIndex
	As JarExtensionsTests.MultipleJarsInManifests only with the additional jar indexes

JarExtensionsTests.BuildVerify2
	Exactly the same as JarExtensionsTests.BuildVerify1 - rebuild the jars with "LOCAL" classes

JarExtensionsTests.CreateJarIndexes
	Add indexes to jar files B.jar E.jar F.jar and H.jar, which are the jars containing the "Class-Path:" tag

JarExtensionsTests.SingleJarInManifestWithIndex.Verify1
	As JarExtensionsTests.SingleJarInManifest.Verify1 only with the additional jar indexes

JarExtensionsTests.SingleJarInManifestWithIndex.Verify2
	As JarExtensionsTests.SingleJarInManifest.Verify2 only with the additional jar indexes

JarExtensionsTests.MultipleJarsInManifestsWithIndex.Verify1
	As JarExtensionsTests.MultipleJarsInManifests.Verify1 only with the additional jar indexes

JarExtensionsTests.MultipleJarsInManifestsWithIndex.Verify2
	As JarExtensionsTests.MultipleJarsInManifests.Verify2 only with the additional jar indexes

