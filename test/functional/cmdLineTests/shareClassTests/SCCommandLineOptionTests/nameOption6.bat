@echo off

rem
rem Copyright (c) 2004, 2018 IBM Corp. and others
rem
rem This program and the accompanying materials are made available under
rem the terms of the Eclipse Public License 2.0 which accompanies this
rem distribution and is available at https://www.eclipse.org/legal/epl-2.0/
rem or the Apache License, Version 2.0 which accompanies this distribution and
rem is available at https://www.apache.org/licenses/LICENSE-2.0.
rem
rem This Source Code may also be made available under the following
rem Secondary Licenses when the conditions for such availability set
rem forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
rem General Public License, version 2 with the GNU Classpath
rem Exception [1] and GNU General Public License, version 2 with the
rem OpenJDK Assembly Exception [2].
rem
rem [1] https://www.gnu.org/software/classpath/license.html
rem [2] http://openjdk.java.net/legal/assembly-exception.html
rem
rem SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
rem

rem @test 1.0
rem @outputpassed
rem @author Alix Pickerill
rem @summary check the illegal length name command line option does not create a cache

setlocal

set TESTSCRIPT=nameOption6

rem token '%%' is required to get char '%'.
set NAME=%%u%%u%%u%%u%%u%%u%%u%%u%%u%%u%%u%%u%%u%%u%%u%%u
%1\java -Xshareclasses:name=%NAME% HelloWorld  2> %TESTSCRIPT%.out

if exist %TESTSCRIPT%.out (
    %1\java SimpleGrep "The cache name is too long when the user name is included." %TESTSCRIPT%.out
) else (
    echo %TESTSCRIPT%: TEST FAILED: No file created
)

endlocal
