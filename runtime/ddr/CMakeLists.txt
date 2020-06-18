################################################################################
# Copyright (c) 2019, 2020 IBM Corp. and others
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

set(OMR_ENHANCED_WARNINGS OFF)

j9vm_add_library(j9ddr_misc SHARED
	OUTPUT_NAME j9ddr_misc${J9VM_VERSION_SUFFIX}
	algorithm_versions.c
	gcddr.cpp
	jitflagsddr.c
	omrddr.cpp
	types.cpp
	vmddr.cpp
)

target_link_libraries(j9ddr_misc
	PRIVATE
		j9vm_interface
		j9vm_gc_includes
)
target_include_directories(j9ddr_misc
	PRIVATE
		${j9vm_SOURCE_DIR}/
		${j9vm_SOURCE_DIR}/bcutil
		${j9vm_BINARY_DIR}/bcutil
		${j9vm_SOURCE_DIR}/shared_common/include
		${omr_SOURCE_DIR}/compiler
)

# remove all optimization options so needed debugging information is not suppressed
omr_remove_flags(CMAKE_CXX_FLAGS -O -O1 -O2 -O3 /Ox)
omr_remove_flags(CMAKE_C_FLAGS -O -O1 -O2 -O3 /Ox)

if(OMR_TOOLCONFIG STREQUAL "gnu")
	# defining _FORTIFY_SOURCE is invalid when optimizations are disabled
	omr_remove_flags(CMAKE_C_FLAGS ${OMR_STRNCPY_FORTIFY_OPTIONS})
	omr_remove_flags(CMAKE_CXX_FLAGS ${OMR_STRNCPY_FORTIFY_OPTIONS})
	if(NOT CMAKE_CXX_COMPILER_ID MATCHES Clang)
		target_compile_options(j9ddr_misc PRIVATE -femit-class-debug-always)
	endif()
endif()


if(OMR_OS_AIX)
	target_link_libraries(j9ddr_misc
		PRIVATE
			-Wl,-bexpall
	)
endif()
target_enable_ddr(j9ddr_misc)
ddr_set_add_targets(j9ddr j9ddr_misc)

# At least one symbol must be exported, but because this shared library
# is only used by ddrgen it doesn't matter which name is exported.
omr_add_exports(j9ddr_misc getAlgorithmVersionStructTable)
