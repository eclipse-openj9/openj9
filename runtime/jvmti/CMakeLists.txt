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

omr_add_tracegen(j9jvmti.tdf)

j9vm_add_library(j9jvmti SHARED
	OUTPUT_NAME j9jvmti${J9VM_VERSION_SUFFIX}
	heapify.cpp
	jvmtiBreakpoint.c
	jvmtiCapability.c
	jvmtiClass.c
	jvmtiClassLoaderSearch.c
	jvmtiEventManagement.c
	jvmtiExtensionMechanism.c
	jvmtiField.c
	jvmtiForceEarlyReturn.cpp
	jvmtiFunctionTable.c
	jvmtiGeneral.c
	jvmtiHeap.c
	jvmtiHeap10.c
	jvmtiHelpers.cpp
	jvmtiHook.c
	jvmtiJNIFunctionInterception.c
	jvmtiLocalVariable.cpp
	jvmtiMemory.c
	jvmtiMethod.c
	jvmtiModules.c
	jvmtiObject.c
	jvmtiRawMonitor.c
	jvmtiStackFrame.cpp
	jvmtiStartup.c
	jvmtiSystemProperties.c
	jvmtiThread.cpp
	jvmtiThreadGroup.c
	jvmtiTimers.c
	jvmtiWatchedField.c
	suspendhelper.cpp

	${CMAKE_CURRENT_BINARY_DIR}/ut_j9jvmti.c
)

target_include_directories(j9jvmti
	PRIVATE
		.
		${CMAKE_CURRENT_BINARY_DIR}
		${j9vm_SOURCE_DIR}/gc_include
		${omr_SOURCE_DIR}/gc/include
		${j9vm_SOURCE_DIR}/compiler/runtime/
)

target_link_libraries(j9jvmti
	PRIVATE
		j9vm_interface
		j9vm_gc_includes
		j9vm_compiler_defines

		j9util
		j9utilcore
		j9avl
		j9hashtable
		j9pool
		j9thr
		j9stackmap
		j9hookable
		j9zip
)

omr_add_exports(j9jvmti
	JVM_OnLoad
	J9VMDllMain
)

target_enable_ddr(j9jvmti GLOB_HEADERS)
ddr_set_add_targets(j9ddr j9jvmti)

install(
	TARGETS j9jvmti
	LIBRARY DESTINATION ${j9vm_SOURCE_DIR}
	RUNTIME DESTINATION ${j9vm_SOURCE_DIR}
)
