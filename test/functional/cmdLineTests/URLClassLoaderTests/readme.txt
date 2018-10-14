
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

These tests are designed to check the implementation of class sharing in URLClassLoader.
A readme.txt is added to each directory to describe in more detail what each test does.

Test groups:

Sanity
	Basic tests to ensure that the support is actually present and working. These tests are a prereq to running the other tests.
FindStore
	Tests which exercise storing and finding of classes in the cache using various different classpath configurations.
JarExt
	Tests which ensure that the Jar Extensions mechanism and Jar Indexing work correctly
SignedSealed
	Tests which ensure that signed jars and sealed packages work correct
NonExistJar
	Tests which ensure that Jars missing from the classpath or which appear on the classpath do not break the support
JVMTI
	Tests the JVMTI functions which can add classpath entries to the bootstrap and system classloaders (Java6 only)

Prereqs:

The tests have a few prereqs before they can all run successfully:

1) A reference JDK containing javac, jar and jarsigner in the /bin directory
2) A Test JDK which must contain a jre/lib/ext directory
3) For the FindStore tests on Windows, a mapped network drive must be provided in the format //machine/share/directory which is writable.

Running the tests:

Example batch files have been provided which run the tests. The locations of the testsuite and JDKs are set in these batch files:

set BASEDIR=C:\ben\URLClassLoaderTests		/* The location of the test-suite */

set NETUSEDIR=\\OTT6F\IRIS\BLUEJ\team\ben	/* The location of a mapped network drive */

set TESTJREBASE=C:\ben\j9vmwi3223\sdk		/* The root of the test JRE */
set REFJREBASE=C:\ben\j9vmwi3223\sr3		/* The root of the reference JDK */

set REFJREBIN=%REFJREBASE%\jre\bin		/* The location of reference JRE jre/bin */
set REFBIN=%REFJREBASE%\bin			/* The location of reference JRE bin */
set TESTEXE=%TESTJREBASE%\jre\bin\java		/* The location of the test executable */
set TESTEXTDIR=%TESTJREBASE%\jre\lib\ext	/* The location of the lib/ext directory in the test JRE */

set EXCLUDEFILE=excludeJava5.xml		/* The exclude file to use */

