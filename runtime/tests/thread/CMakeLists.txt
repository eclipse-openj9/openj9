################################################################################
# Copyright (c) 2017, 2020 IBM Corp. and others
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

set(OMR_ENHANCED_WARNINGS OFF)

j9vm_add_library(thread_cutest_harness STATIC
	cuharness/CuTest.c
	cuharness/parser.c
)

target_include_directories(thread_cutest_harness
	PUBLIC
		cuharness
)

target_link_libraries(thread_cutest_harness
	PRIVATE
		j9vm_interface

		j9prt
		j9util
		j9utilcore
		j9thr
)


j9vm_add_library(j9thrtestnatives SHARED
	OUTPUT_NAME j9thrtestnatives${J9VM_VERSION_SUFFIX}
	testnatives/testnatives.c
)

target_include_directories(j9thrtestnatives
	PRIVATE
	testnatives
)

target_link_libraries(j9thrtestnatives
	PRIVATE
	j9vm_interface

	j9util
	j9utilcore
	j9thr
)

omr_add_exports(j9thrtestnatives
	Java_com_ibm_thread_TestNatives_setSpinCounts
	Java_com_ibm_tests_perftests_jlm_JLMInfoHelper_jvmtiJlmSet
	Java_com_ibm_tests_perftests_jlm_JLMInfoHelper_jvmtiJlmDump
	Agent_OnLoad
	Agent_OnAttach
	Agent_OnUnload
	Java_com_ibm_j9_monitor_tests_TestNatives_getLockWordValue
	Java_com_ibm_j9_monitor_tests_TestNatives_getLockWordOffset
	Java_com_ibm_j9_monitor_tests_TestNatives_getHeaderSize
	Java_com_ibm_j9_monitor_tests_TestNatives_getLockwordSize
	Java_j9vm_test_thread_TestNatives_getSchedulingPolicy
	Java_j9vm_test_thread_TestNatives_getSchedulingPriority
	Java_j9vm_test_thread_TestNatives_getGCAlarmSchedulingPolicy
	Java_j9vm_test_thread_TestNatives_getGCAlarmSchedulingPriority
	Java_j9vm_test_softmx_TestNatives_setAggressiveGCPolicy
)

j9vm_add_library(j9thrnumanatives SHARED
	OUTPUT_NAME j9thrnumanatives${J9VM_VERSION_SUFFIX}
	thrnumanatives/thrnumanatives.c
)

target_link_libraries(j9thrnumanatives
	PRIVATE
	j9vm_interface
)

omr_add_exports(j9thrnumanatives
	Java_com_ibm_j9_numa_NumaTest_getUsableNodeCount
)

j9vm_add_executable(thrstatetest
	thrstate/runtest.c
	thrstate/testcases1.c
	thrstate/testcases2.c
	thrstate/testcases3.c
	thrstate/testcases4.c
	thrstate/testcases5.c
	thrstate/testcases6.c
	thrstate/testsetup.c
)

target_link_libraries(thrstatetest
	PRIVATE
	j9vm_main_wrapper

	j9prt
	j9exelib
	j9util
	j9utilcore
	j9hashtable
	j9avl
	j9thr
	j9pool
)

install(
	TARGETS j9thrtestnatives j9thrnumanatives thrstatetest
	RUNTIME DESTINATION ${j9vm_SOURCE_DIR}
	LIBRARY DESTINATION ${j9vm_SOURCE_DIR}
)
