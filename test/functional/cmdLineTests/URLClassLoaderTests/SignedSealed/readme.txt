
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

These tests ensure that package sealing and Jar signing work correctly for shared classes

SignedSealed.BuildInitial1
	Builds AB.jar, which contains A classes and B classes from a sealed package and C classes from an unsealed package
	Builds B1.jar, which contains B classes from a sealed package, but which is not sealed and C classes from an unsealed package
	Builds B2.jar, which contains a single B class from the sealed package, but which is not sealed

Sealed.success
	Runs A_Main which loads classes from classpath AB.jar;B1.jar
	This should work because all the sealed classes come from the same jar (AB.jar)

Sealed.failure1
	Runs A_Main with classpath B1.jar;AB.jar which should fail because the classes loaded from B1 are not sealed.
	Expect a SecurityException

Sealed.failure2
	Runs A_Main with classpath B2.jar;AB.jar which should fail because the classes loaded from B1 are not sealed.
	This is different from failure1 because only the second B class is loaded from B2, the first is loaded from AB.jar.	
	Expect a SecurityException

(Note that the above 3 tests each get their own cache)

Sealed.verify1
	Runs A_Main again with the failure1 classpath to ensure that it still throws a SecurityException

Sealed.verify2
	Runs A_Main again with the failure2 classpath to ensure that it still throws a SecurityException

SignedSealed.BuildInitial2
	Builds A.jar, B.jar and C.jar and then signs them using jarsigner to produce sA.jar, sB.jar and sC.jar
	C.jar contains classes from one package. A.jar and B.jar contain classes from a different package.

Signed.success1Store
	Runs A_Main and stores classes using sA.jar;sB.jar;sC.jar in the cache

Signed.success1Find
	Runs the same test as success1Store to ensure that no error is thrown loading classes out of the cache

Signed.success2Store
	Runs A_Main and stores classes using sA.jar;sB.jar;C.jar in the cache
	This should work because the unsigned classes in C are from a different package than A or B.

Signed.success2Find
	Runs the same test as success2Store to ensure that no error is thrown loading classes out of the cache

Signed.success3Store
	Runs A_Main and stores classes using A.jar;B.jar;sC.jar in the cache
	This should work because the signed classes in C are from a different package than A or B.

Signed.success3Find
	Runs the same test as success3Store to ensure that no error is thrown loading classes out of the cache

Signed.failure1Store
	Runs A_Main and stores classes using A.jar;sB.jar;C.jar in the cache
	This should throw a SecurityException because A is offering unsigned classes in sB's signed package

Signed.failure1Find
	Runs the same test as failure1Store to ensure that an exception is still thrown

Signed.failure2Store
	Runs A_Main and stores classes using sA.jar;B.jar;C.jar in the cache
	This should throw a SecurityException because B is offering unsigned classes in sA's signed package

Signed.failure2Find
	Runs the same test as failure1Store to ensure that an exception is still thrown