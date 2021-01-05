################################################################################
# Copyright (c) 2020, 2020 IBM Corp. and others
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

include(OmrTargetSupport)

# Adds copyright metadata on windows. On other platforms its a no-op.
function(j9vm_add_copyright_data target_name)
	omr_assert(FATAL_ERROR TEST TARGET ${target_name} MESSAGE "Expected a target name, got ${target_name}")

	if(NOT OMR_OS_WINDOWS)
		return()
	endif()

	get_target_property(target_type "${target_name}" TYPE)
	if(NOT target_type STREQUAL "SHARED_LIBRARY")
		return()
	endif()

	omr_assert(SEND_ERROR TEST OPENJDK_VERSION_NUMBER_FOUR_POSITIONS MESSAGE "Required variable OPENJDK_VERSION_NUMBER_FOUR_POSITIONS is not set")
	set(VERSION_STRING "${OPENJDK_VERSION_NUMBER_FOUR_POSITIONS}")
	string(REPLACE "." "," VERSION_COMMA "${VERSION_STRING}")

	string(TOUPPER "${CMAKE_BUILD_TYPE}" build_type)
	# Create an ordered list of property names to search.
	set(name_properties
		"RUNTIME_OUTPUT_NAME_${build_type}"
		"RUNTIME_OUTPUT_NAME"
		"OUTPUT_NAME_${build_type}"
		"OUTPUT_NAME"
	)

	# Create a generator expression to search possible output names.
	omr_genex_property_chain(MODULE_NAME "${target_name}" ${name_properties} "${target_name}")
	set(MODULE_FILE_NAME "${MODULE_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}")
	set(resource_output_file "${CMAKE_CURRENT_BINARY_DIR}/${target_name}.rc")
	omr_process_template("${j9vm_SOURCE_DIR}/cmake/shared_lib.rc.in" "${resource_output_file}")
	target_sources("${target_name}" PRIVATE "${resource_output_file}")
endfunction()

# Currently j9vm_add_library and j9vm_add_executable just call
# omr_add_library and omr_add_executable, but have them so that we can add
# functionality in the future, without having to hunt down all the 
# add_library/add_executable calls.

function(j9vm_add_library name)
	omr_add_library("${name}" ${ARGN})
	get_target_property(target_type "${name}" TYPE)
	if(target_type STREQUAL "SHARED_LIBRARY")
		j9vm_add_copyright_data("${name}")
	endif()
endfunction()

function(j9vm_add_executable name)
	omr_add_executable("${name}" ${ARGN})
endfunction()
