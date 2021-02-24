################################################################################
# Copyright (c) 2017, 2021 IBM Corp. and others
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
################################################################################

if(OMR_OS_AIX)
	add_subdirectory(aixerrmsg)
endif()
add_subdirectory(algorithm)
if(OMR_OS_WINDOWS AND NOT OMR_ENV_DATA64)
	add_subdirectory(bcprof)
endif()
add_subdirectory(bcverify)
add_subdirectory(cutest)
add_subdirectory(gp)
add_subdirectory(j9vm)
add_subdirectory(jni)
add_subdirectory(jniarg)
add_subdirectory(jsig)
add_subdirectory(jvmtitests)
add_subdirectory(lazyclassload)
if(OMR_OS_OSX)
	add_subdirectory(loadLibraryTest)
endif()
add_subdirectory(port)
add_subdirectory(props)
add_subdirectory(redirector)
add_subdirectory(shared)
if(OMR_OS_WINDOWS AND NOT OMR_ENV_DATA64)
	add_subdirectory(shared_service)
endif()
add_subdirectory(thread)
if(OMR_OS_LINUX)
	add_subdirectory(unresolved)
endif()
add_subdirectory(vm)
add_subdirectory(vm_lifecycle)
if(OMR_OS_ZOS)
	add_subdirectory(zos)
endif()
