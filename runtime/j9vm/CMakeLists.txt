################################################################################
# Copyright IBM Corp. and others 2017
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
# [2] https://openjdk.org/legal/assembly-exception.html
#
# SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
################################################################################

set(OMR_ENHANCED_WARNINGS OFF)

add_tracegen(j9scar.tdf)

# Note we add all the actual sources to this interface library.
# This is so that we can reuse them without having to explicitly re-list all the files.
j9vm_add_library(jvm_common INTERFACE)

target_sources(jvm_common INTERFACE
	${CMAKE_CURRENT_SOURCE_DIR}/j7verify.c
	${CMAKE_CURRENT_SOURCE_DIR}/j7vmi.c
	${CMAKE_CURRENT_SOURCE_DIR}/j8vmi.c
	${CMAKE_CURRENT_SOURCE_DIR}/j9memcategories.c
	${CMAKE_CURRENT_SOURCE_DIR}/java11vmi.c
	${CMAKE_CURRENT_SOURCE_DIR}/jvm.c
	${CMAKE_CURRENT_SOURCE_DIR}/vmi.c
	${CMAKE_CURRENT_SOURCE_DIR}/javanextvmi.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/valhallavmi.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/asgct.cpp
)

target_link_libraries(jvm_common
	INTERFACE
		j9vm_interface
		j9vm_gc_includes

		j9exelib
		j9utilcore
		j9avl
		j9hashtable
		j9pool

		${CMAKE_DL_LIBS}
		${OMR_PLATFORM_THREAD_LIBRARY}
)

target_include_directories(jvm_common
	INTERFACE
		${CMAKE_CURRENT_BINARY_DIR}
		# TODO we have to repeat ../include because of duplication of vmi.h
		../include
		../jcl/
)

j9vm_add_library(jvm SHARED ${CMAKE_CURRENT_BINARY_DIR}/ut_j9scar.c)
target_link_libraries(jvm
	PRIVATE
		jvm_common
		j9util
)

if(OMR_OS_WINDOWS)
	target_link_libraries(jvm PRIVATE ws2_32)
endif()
add_dependencies(jvm omrgc_hookgen)
include(exports.cmake)

install(
	TARGETS jvm
	LIBRARY DESTINATION ${j9vm_SOURCE_DIR}
	RUNTIME DESTINATION ${j9vm_SOURCE_DIR}
)
