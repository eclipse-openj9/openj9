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

set_source_files_properties(${j9vm_BINARY_DIR}/vm/ut_j9vm.c PROPERTIES GENERATED TRUE)
set_source_files_properties(${j9vm_BINARY_DIR}/bcutil/ut_j9bcu.c PROPERTIES GENERATED TRUE)
add_library(j9dyn_generic_test STATIC

	${j9vm_BINARY_DIR}/vm/ut_j9vm.c
	${j9vm_SOURCE_DIR}/vm/stringhelpers.cpp
	${j9vm_SOURCE_DIR}/vm/KeyHashTable.c
	${j9vm_SOURCE_DIR}/vm/ModularityHashTables.c
	${j9vm_SOURCE_DIR}/vm/dllsup.c
)

# We need to add an explicit dependency since CMake doesn't track dependencies for generated files across directories
add_dependencies(j9dyn_generic_test trc_j9vm)

# And add the vm binary dir to our include path
target_include_directories(j9dyn_generic_test PRIVATE ${j9vm_BINARY_DIR}/vm)

target_link_libraries(j9dyn_generic_test
	PRIVATE
		j9vm_interface

		j9dyn_sources
)
target_compile_definitions(j9dyn_generic_test
	PUBLIC
		J9DYN_TEST
)
