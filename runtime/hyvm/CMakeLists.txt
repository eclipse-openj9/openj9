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

# Dont append version suffix
set(CMAKE_SHARED_LIBRARY_SUFFIX ${J9VM_OLD_SHARED_SUFFIX})

add_library(harmony_jvm SHARED
	harmony_vm.c
)
target_include_directories(harmony_jvm
	PRIVATE
		${j9vm_SOURCE_DIR}/harmony/include
)

target_link_libraries(harmony_jvm
	PRIVATE
		j9vm_interface

		omrsig
		hyprt
		hyprtshim
)

# We actually want the library to be named "jvm.{so|dll}"
set_target_properties(harmony_jvm PROPERTIES
	LIBRARY_OUTPUT_NAME "jvm"
	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
)

omr_add_exports(harmony_jvm
	JNI_CreateJavaVM
	JNI_GetCreatedJavaVMs
	JNI_GetDefaultJavaVMInitArgs
)

install(
	TARGETS harmony_jvm
	LIBRARY DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}
	RUNTIME DESTINATION ${j9vm_SOURCE_DIR}
)
