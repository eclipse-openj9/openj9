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

j9vm_add_library(jvmti_test_agent STATIC
	agent.c
	args.c
	error.c
	help.c
	message.c
	strings.c
	tests.c
	thread.c
	util.c
	version.c
)

target_link_libraries(jvmti_test_agent
	PRIVATE
		j9vm_interface
		j9vm_jvmtitest_includes
		jvmti_test_src
)

if(OMR_OS_ZOS)
	if(OMR_TOOLCONFIG STREQUAL "openxl")
		target_compile_options(jvmti_test_agent PRIVATE -fvisibility=default)
	else()
		target_compile_options(jvmti_test_agent PRIVATE -Wc,DLL,EXPORTALL)
	endif()
endif()
