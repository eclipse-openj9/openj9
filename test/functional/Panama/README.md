<!--
Copyright (c) 2017, 2018 IBM Corp. and others

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

# Panama Test
- Contains [tests](src/org/openj9/test/panama/) for native method handles
- Build with the "Build git tests - panama" job
- Run with linux_x86-64_cmprssptrs_panama.spec on the rawbuild-panama_xa64-b136 sdk
- [HigherLevelNativeMethodHandleTest](src/org/openj9/test/panama/HigherLevelNativeMethodHandleTest.java) uses [panamatest.jar](lib/panamatest.jar), created from [panamatest.c](panamatest.c) with the jextract groveller


    * To create a libpanamatest.so, a gcc is needed:

            gcc -shared -o {$common.workspace.gittest$}/Panama/libpanamatest.so -fPIC {$common.workspace.gittest$}/Panama/panamatest.c

    * To create a file like panamatest.jar, on a machine with os:linux:ubuntu14 capability (such as ub1404x64vm1), run:

            export LD_LIBRARY_PATH=<path_to_clang_lib>/clang/clang+llvm-3.8.0-x86_64-linux-gnu-ubuntu-14.04/lib
            <OpenJDK_root>/bin/jextract -J-Djava.library.path=<path_to_clang_lib>/clang -t panama.test -o panamatest.jar <path to the test repo>/Panama/panamatest.c
