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

add_executable(pltest
	j9cudaTest.c
	j9dumpTest.c
	j9errorTest.c
	j9fileTest.c
	j9heapTest.c
	j9hypervisorTest.c
	j9memTest.c
	j9mmapTest.c
	j9nlsTest.c
	j9portTest.c
	j9processTest.c
	j9riTest.c
	j9shsem_deprecatedTest.c
	j9shsemTest.c
	j9slTest.c
	j9sockTest.c
	j9strTest.c
	j9timeTest.c
	j9ttyExtendedTest.c
	j9ttyTest.c
	j9vmemTest.c
	main.c
	shmem.c
	si_numcpusTest.c
	si.c
	socket.c
	testHelpers.c
	testProcessHelpers.c
)

target_link_libraries(pltest
	PRIVATE
		j9vm_main_wrapper
		j9prt
		j9pool
		j9utilcore
		j9avl
		j9hashtable
		j9thr
)

install(
	TARGETS pltest
	RUNTIME DESTINATION ${j9vm_SOURCE_DIR}
)
