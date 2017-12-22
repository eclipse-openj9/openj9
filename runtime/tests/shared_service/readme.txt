#
# Copyright (c) 2001, 2017 IBM Corp. and others
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

rem This is a sample bat file that can be used to install and start the Windows
rem service that will run a JVM as the system account.  It waits to give the service
rem time to get started and then starts a normal user account JVM, which should
rem run normally.  The service is then stopped and uninstalled.
rem
rem This test requires a JSE build as the point is to run with Shared Classes which
rem are only included in a JSE build.  The servicetest.exe must be copied to the 
rem jre\bin directory.

rem To be run with servicetest.exe in inst.images\x86_nt_4\sdk\jre\bin\

servicetest.exe -install SCServiceTest -exepath C:\userlvl\inst.images\x86_nt_4\sdk\jre\bin\servicetest.exe -srvdesc "Shared Classes Service Test"

servicetest.exe -start SCServiceTest -logpath c:\MyService\log.txt -jvmpath c:\userlvl\inst.images\x86_nt_4\sdk\jre\bin\j9vm\jvm.dll -classpath c:\MyService -classname ServiceLooping -stopmethod stopLooping -cachename MyService

rem Wait for service to get started
sleep 5

java HelloWorld

servicetest.exe -stop SCServiceTest

servicetest.exe -uninstall SCServiceTest


