################################################################################
# Copyright (c) 2018, 2018 IBM Corp. and others
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

# Include once
if(COMMAND j9vm_gen_asm)
	return()
endif()

# Helper function for generating assembly from m4
# usage: j9vm_gen_asm(some_file.m4 ....)
# converts 'some_file.m4' into 'some_file.s'.
# Note: path is interpreted relative to CMAKE_CURRENT_SOURCE_DIR
# .s file is put in CMAKE_CURRENT_BINARY_DIR 
# Directory names from the input are ignored. IE. foo/bar.m4 > bar.s 
function(j9vm_gen_asm)
	set(m4_defines "")
	if(OMR_HOST_OS STREQUAL "linux")
		set(m4_defines "-DLINUX")
	elseif(OMR_HOST_OS STREQUAL "osx")
		set(m4_defines "-DOSX")
	endif()
	#TODO need to add other platforms here

	foreach(m4_file IN LISTS ARGN)
		get_filename_component(base_name "${m4_file}" NAME_WE)
		
		add_custom_command(
			OUTPUT ${base_name}.s
			DEPENDS ${m4_file} run_constgen
			COMMAND m4 -I "${j9vm_SOURCE_DIR}/oti" ${m4_defines} ${CMAKE_CURRENT_SOURCE_DIR}/${m4_file} > ${base_name}.s
		)
	endforeach()


endfunction(j9vm_gen_asm)
