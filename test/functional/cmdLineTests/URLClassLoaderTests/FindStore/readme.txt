
#
# Copyright (c) 2001, 2019 IBM Corp. and others
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

These tests check the basic find and store functionality of URLClassLoader with various different classpath configurations

Note here that "noTimestampChecks" stops the shared classes support from recognising that jar files have been updated

FindStore.BuildInitial
	Builds A.jar B.jar C.Jar and D.jar which will be used to test explicit paths
	Builds E.jar F.jar G.jar and H.jar which will be used to test relative paths
	Builds I.jar, J.jar K.jar and L.jar which will be used to test the net use paths
	Builds class files into M_Classes, N_Classes, O_Classes and P_Classes to test directory paths
	All of the data classes will print "CACHED" when they are loaded.

FindStore.StoreExplicit
	Runs A_Main which will cause classes to be loaded from A.jar B.jar C.Jar and D.jar.
	The classpath used specifies explicit paths.
	Nothing.jar is added as the 0th element of the classpath. This is used for testing classpath matching later.

FindStore.StoreRelative
	Runs E_Main which will cause classes to be loaded from E.jar F.jar G.jar and H.jar.
	The classpath used specifies relative paths.
	Nothing.jar is added as the 1st element of the classpath. This is used for testing classpath matching later.

FindStore.StoreNetUse
	Runs I_Main which will cause classes to be loaded from I.jar, J.jar K.jar and L.jar.
	The classpath used specifies net use paths.
	Nothing.jar is added as the 3rd element of the classpath. This is used for testing classpath matching later.

FindStore.StoreClassfile
	Runs M_Main which will cause classes to be loaded from M_Classes, N_Classes, O_Classes and P_Classes.
	The classpath used specifies directories.
	Nothing.jar is added as the 4th element of the classpath. This is used for testing classpath matching later.

FindStore.BuildVerify
	Rebuilds all of the jar files with similar class files which output "LOCAL" instead of "CACHED" when they are loaded.
	The classes are in the VerifyClasses subdirectory

FindStore.FindExplicit
	Runs the JVM with "noTimestampChecks" using the same classpath as StoreExplicit.
	Expects that the "CACHED" classes will be loaded.

FindStore.FindRelative
	Runs the JVM with "noTimestampChecks" using the same classpath as StoreRelative.
	Expects that the "CACHED" classes will be loaded.

FindStore.FindNetUse
	Runs the JVM with "noTimestampChecks" using the same classpath as StoreNetUse.
	Expects that the "CACHED" classes will be loaded.

FindStore.FindClassfile
	Runs the JVM with "noTimestampChecks" using the same classpath as StoreClassFile.
	Expects that the "CACHED" classes will be loaded.

FindStore.FindDiffcpExplicit
	Runs the JVM with "noTimestampChecks" using a different classpath to StoreExplicit (Nothing.jar is removed).
	Expects that the "CACHED" classes will still be loaded, even though the classpaths don't exactly match

FindStore.FindDiffcpRelative
	Runs the JVM with "noTimestampChecks" using a different classpath to StoreRelative (Nothing.jar is removed).
	Expects that the "CACHED" classes will still be loaded, even though the classpaths don't exactly match

FindStore.FindDiffcpNetUse
	Runs the JVM with "noTimestampChecks" using a different classpath to StoreNetUse (Nothing.jar is removed).
	Expects that the "CACHED" classes will still be loaded, even though the classpaths don't exactly match

FindStore.FindDiffcpClassfile
	Runs the JVM with "noTimestampChecks" using a different classpath to StoreClassFile (Nothing.jar is removed).
	Expects that the "CACHED" classes will still be loaded, even though the classpaths don't exactly match
