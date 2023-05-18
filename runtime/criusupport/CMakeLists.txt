################################################################################
# Copyright IBM Corp. and others 2021
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

add_tracegen(j9criu.tdf)

j9vm_add_library(j9criu SHARED
	OUTPUT_NAME j9criu${J9VM_VERSION_SUFFIX}
	criusupport.cpp

	${CMAKE_CURRENT_BINARY_DIR}/ut_j9criu.c
)

target_include_directories(j9criu
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}
		${CMAKE_CURRENT_BINARY_DIR}
)
add_dependencies(j9vm_interface j9vm_m4gen)

target_link_libraries(j9criu
	PRIVATE
		j9vm_interface
		j9vm_gc_includes
		j9util
		j9thr
		dl
		j9utilcore
		j9pool
)

include(exports.cmake)

target_enable_ddr(j9criu)
ddr_set_add_targets(j9ddr j9criu)

install(
	TARGETS j9criu
	LIBRARY DESTINATION ${j9vm_SOURCE_DIR}
	RUNTIME DESTINATION ${j9vm_SOURCE_DIR}
)
