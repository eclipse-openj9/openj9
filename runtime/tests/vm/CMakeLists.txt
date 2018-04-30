################################################################################
# Copyright (c) 2017, 2018 IBM Corp. and others
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
add_executable(vmtest
	resolvefield_tests.c
	testHelpers.c
	vmstubs.c
	vmtest.c

	#TODO this should maybe be refactored?
	${j9vm_SOURCE_DIR}/vm/description.c
	${j9vm_SOURCE_DIR}/vm/KeyHashTable.c
	${j9vm_SOURCE_DIR}/vm/ModularityHashTables.c
	${j9vm_SOURCE_DIR}/vm/resolvefield.cpp
	${j9vm_SOURCE_DIR}/vm/ObjectFieldInfo.cpp
	${j9vm_SOURCE_DIR}/vm/segment.c
	${j9vm_SOURCE_DIR}/vm/visible.c
	${j9vm_SOURCE_DIR}/vm/vmthinit.c
	${j9vm_SOURCE_DIR}/vm/stringhelpers.cpp
	${j9vm_BINARY_DIR}/vm/ut_j9vm.c
)
add_dependencies(vmtest trc_j9vm)

target_compile_definitions(vmtest
	PRIVATE
		J9VM_TEST
)
target_include_directories(vmtest PRIVATE ${j9vm_BINARY_DIR}/vm)

target_link_libraries(vmtest
	PRIVATE
		j9vm_gc_includes
		j9vm_main_wrapper

		j9exelib
		j9prt
		j9verutil
		j9util
		j9utilcore
		j9avl
		j9hashtable
		j9thr
		j9stackmap
		j9pool
		j9simplepool
		j9bcv
		j9dyn
		j9hookable
		j9vm
)

install(
	TARGETS vmtest
	RUNTIME DESTINATION ${j9vm_SOURCE_DIR}
)
