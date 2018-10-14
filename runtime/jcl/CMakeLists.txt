################################################################################
# Copyright (c) 2017, 2018 IBM Corp. and others
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

###
### Set up shared interface libraries
###
set(J9VM_JCL_BIN_DIR "${CMAKE_CURRENT_BINARY_DIR}")
add_tracegen(j9jcl.tdf)
add_library(j9vm_jcl_trace INTERFACE)
target_sources(j9vm_jcl_trace
	INTERFACE
		${CMAKE_CURRENT_BINARY_DIR}/ut_j9jcl.c
)
target_include_directories(j9vm_jcl_trace
	INTERFACE
		${CMAKE_CURRENT_BINARY_DIR}
)
# Work arround for cmakes broken dependency tracking for generated files
add_dependencies(j9vm_jcl_trace trc_j9jcl)

# Create interface lib to track common include dirs
add_library(j9vm_jcl_includes INTERFACE)
#TODO this is platform specific
target_include_directories(j9vm_jcl_includes
	INTERFACE
		${CMAKE_CURRENT_SOURCE_DIR}/unix
		${j9vm_SOURCE_DIR}/shared_common/include
)


###
### Pull in definitions of components to be added to classlibs
###
include(cmake/attach.cmake)
include(cmake/jithelpers.cmake)
include(cmake/se6_vm-side_natives.cmake)
include(cmake/se6_vm-side_lifecycle.cmake)
include(cmake/se7.cmake)
include(cmake/se8.cmake)
include(cmake/se9.cmake)
include(cmake/sun_misc_unsafe.cmake)


###
### Define our class libraries
###

if(J9VM_BUILD_JAVA7)
	add_subdirectory(cl_se7_basic)
	#TODO why is cl_se9 gated by BUILD_JAVA7?
	add_subdirectory(cl_se9)
	add_subdirectory(cl_se9_before_b165)
endif()
