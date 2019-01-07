################################################################################
# Copyright (c) 2018, 2019 IBM Corp. and others
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

# We don't want to add the version suffix to dll names.
set(CMAKE_SHARED_LIBRARY_SUFFIX ${J9VM_OLD_SHARED_SUFFIX})

set_source_files_properties(
	${j9vm_BINARY_DIR}/j9vm/ut_j9scar.c
	${j9vm_BINARY_DIR}/j9vm/generated.h
	PROPERTIES GENERATED TRUE
)

add_library(jvm_jdk12 SHARED ${j9vm_BINARY_DIR}/j9vm/ut_j9scar.c)
target_include_directories(jvm_jdk12 PRIVATE ${j9vm_SOURCE_DIR}/j9vm)
target_link_libraries(jvm_jdk12
	PRIVATE
		jvm_common
		j9util_b156
)
add_dependencies(jvm_jdk12
	omrgc_hookgen
	trc_j9scar
)

set_target_properties(jvm_jdk12 PROPERTIES LIBRARY_OUTPUT_NAME jvm)

install(
	TARGETS jvm_jdk12
	LIBRARY DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}
)
