################################################################################
# Copyright (c) 2017, 2019 IBM Corp. and others
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

add_executable(constgen
    jilconsts.c
)

target_link_libraries(constgen 
	PRIVATE
		j9vm_interface

		omrport
		j9thr
)

# Add target to generate jilconsts.inc/jilvalues.m4 from the constgen program
add_custom_command(
	OUTPUT ${j9vm_BINARY_DIR}/oti/jilconsts.inc ${j9vm_BINARY_DIR}/oti/jilvalues.m4
	COMMAND ${CMAKE_COMMAND} -E make_directory ${j9vm_BINARY_DIR}/oti
	COMMAND $<TARGET_FILE:constgen>
	VERBATIM
	WORKING_DIRECTORY ${j9vm_BINARY_DIR}
)
add_custom_target(run_constgen
	DEPENDS ${j9vm_BINARY_DIR}/oti/jilconsts.inc ${j9vm_BINARY_DIR}/oti/jilvalues.m4
)
