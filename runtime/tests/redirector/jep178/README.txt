
#
# Copyright (c) 2014, 2017 IBM Corp. and others
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

		Test statically linking JNI Libraries [JEP 178]

1.0 Brief
Directory contains a couple of tests that test out the JEP 178. Oracle haven't 
provided any JTREG (regression tests) or JCK tests specifically targeting this 
feature.
The test builds into 2 executables:
	1.1 testjep178_static{.exe} - this executable is build as if it were linked
		against a two static libraries "testlibA" and "testlibB" (in a platform
		dependent manner). Thus, symbols in those libraries are effectively
		built into the executable's image. When this executable launches a JVM
		instance, it also becomes a provider of definitions required for those
		JNI methods that correspond to, and have been built and exposed by the
		executable. testjep178_static tests this scenario; the JNI routines do
		not do much other than printing messages indicating that these have 
		been reached via static linking. The command line tester then parses 
		the output messages to check for this (looks for the occurrence of the
		tag [statically]), amongst other things.
		
	1.2 testjep178_dynamic{.exe} - this executable is built in the traditional
		JVM launcher model; that is, it merely launches a JVM instance, 
		expecting externally available shared libraries in -Djava.library.path
		to be linked dynamically by the JVM, in the absence of such libraries 
		being built into the executable. This test merely ensures that the 
		traditional dynamic linking model of JNI method resolution still holds.

Note: Ideally, the two executables should have been built out of the same "C"
source, with a guard against the JNI method definitions, such that two distinct
objects built out of this source, with and without the guard, would then be
linked appropriately into _static and _dynamic executables. However, it turned
out that UMA supports compilation of a source <file.c> into objects with the
same name, that is, <file.o>. Therefore, the sources have been split into two 
files: 
	testjep178_static.c
	testjep178_dynamic.c
Hence, if the JVM launcher code needs fixes and/or changes, they must be 
affected into both these sources.

Additionally, the test also builds 2 shared libraries (or DLL's):
	1.3 testlibA.{so,dll} - that provides a definition for JNI method fooImpl() 
		printing messages to indicate that the routine has been reached via
		dynamic linking (specifically, the tag [dynamically]).
	1.4 testlibB.{so,dll} - that provides a definition for JNI method barImpl() 
		printing similar a message as 1.3.

Also, the test packages the Java class com.ibm.j9.tests.jeptests.StaticLinking,
which merely loads the two "libraries" testlibA and testlibB, itself being
oblivious about how or where these would be loaded from.

2.0 Testing static linking
When the Java program loads a library, such as,
		System.loadLibrary("testlibA");
the jvm tries to resolve the initializer routine in the executable.  Since it 
is found exported in testjep178_static, it invokes it and goes ahead to resolve
definitions for JNI routines, such as fooImpl(), barImpl(), etc.

3.0 Ensuring dynamic linking
When the same Java program is launched by the launcher in testjep178_dynamic,
the JVM fails to locate routine JNI_OnLoad_testlibA (not defined or not export-
ed); the JVM falls back on the traditional dynamic linking model to resolve JNI
methods.  In the current setup, the shared library "libtestlibA.so" shall be 
linked, at above loadLibrary() invocation.

4.0 Building the tests	
In addition to the current executable, the two shared libraries, namely, 
testlibA and testlibB should be around, even when testing testjep178_static.
This ensures that the JVM is not opting to link dynamically, as long as the
executable provides definitions for those JNI's.

	4.1 Building on z/OS - Compile using -Wc,DLL,EXPORTALL to ensure the 
		executable is built as a DLL and also, that all symbols are exported; 
		to selectively do this, use #pragma export against those functions that
		require to be exported.
		- Link the object thus formed using the linker option -Wl,xplink,dll 
		(again, to ensure the object(s) are linked as a DLL.

	4.2 Building on Linux - Ensure that the functions are /not/ static as on 
		Linux, no special options are required to expose symbols.
		- Add the compiler option -rdynamic; where as symbols are all exported,
		the limitation is that shared libraries cannot resolve symbols back 
		from the calling executable.  Thus, an executable exporting symbols 
		may show "T", when queried using "nm", if this is not built using 
		-rdynamic, symbols it defines are still not visible to shared 
		libraries.

	4.3 Building on AIX - Add -qpic [Position Independent Code (PIC)].
		- Add -brtl -bexpall to ensure run-time linking and export symbols 
		defined in the executable (against using symbol specific visibility 
		control).

	4.4 Building on Windows - No special mechanisms required here, except that
		the routines to be used as JNI definitions should not be declared 
		static (use JNIEXPORT).

5.0 Miscellaneous
At present, the JNI version provided to the jvm itself is /not/ required to be
"1.8" for static linking to proceed.  J9 skips this check on purpose, so as to
be compatible with Oracle JDK, which seems to accept lower JNI versions for 
static linking.
In future, J9 may need to be fixed w.r.t. this, should Oracle disallow versions
lesser than "1.8", going forward.

If the cleanup/finalization routine named JNI_OnUnload_<library> is defined and
exported, this would preempt the definition of a regular JNI_OnUnload.

Further, the OnUnload_<library> routine should preempt OnLoad, should both be 
present. With Oracle JDK, neither of these were seen invoked, since this 
life-cycle routine ought to be invoked when the class loader containing the 
native library gets garbage collected, and this is not a definitive thing in a
typical Java program.  J9, however, always invokes this at VM exit to ensure
clean up, as we run more than one instances inside the same address space.
