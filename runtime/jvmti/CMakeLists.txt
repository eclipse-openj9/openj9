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

omr_add_tracegen(j9jvmti.tdf)

add_library(j9jvmti SHARED
	jvmtiBreakpoint.c
	jvmtiCapability.c
	jvmtiClass.c
	jvmtiClassLoaderSearch.c
	jvmtiEventManagement.c
	jvmtiExtensionMechanism.c
	jvmtiField.c
	jvmtiForceEarlyReturn.c
	jvmtiFunctionTable.c
	jvmtiGeneral.c
	jvmtiHeap.c
	jvmtiHeap10.c
	jvmtiHelpers.c
	jvmtiHook.c
	jvmtiJNIFunctionInterception.c
	jvmtiLocalVariable.c
	jvmtiMemory.c
	jvmtiMethod.c
	jvmtiModules.c
	jvmtiObject.c
	jvmtiRawMonitor.c
	jvmtiStackFrame.c
	jvmtiStartup.c
	jvmtiSystemProperties.c
	jvmtiThread.c
	jvmtiThreadGroup.c
	jvmtiTimers.c
	jvmtiWatchedField.c
	suspendhelper.cpp

	${CMAKE_CURRENT_BINARY_DIR}/ut_j9jvmti.c
)
target_include_directories(j9jvmti PRIVATE  ${CMAKE_CURRENT_BINARY_DIR})

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

target_enable_ddr(j9jvmti j9ddr)

install(
	TARGETS j9jvmti
	LIBRARY DESTINATION ${j9vm_SOURCE_DIR}
	RUNTIME DESTINATION ${j9vm_SOURCE_DIR}
)
