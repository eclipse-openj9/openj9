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

add_tracegen(dg.tdf)
add_tracegen(j9trc.tdf)
add_tracegen(j9trc_aux.tdf)
add_tracegen(mt.tdf)

add_library(j9trc SHARED
	method_trace.c
	method_trigger.c
	trccomponent.c
	trcengine.c
	trcformatter.c
	trclog.c
	trcmain.c
	trcmisc.c
	trcoptions.c
	trcqueue.c
	trctrigger.c
	trcwrappers.c

	${CMAKE_CURRENT_BINARY_DIR}/ut_dg.c
	${CMAKE_CURRENT_BINARY_DIR}/ut_j9trc.c
	${CMAKE_CURRENT_BINARY_DIR}/ut_j9trc_aux.c
	${CMAKE_CURRENT_BINARY_DIR}/ut_mt.c
)

target_include_directories(j9trc
	PRIVATE
		../omr/include_core
		../nls
		${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(j9trc
	PRIVATE
		j9vm_interface
		j9util
		j9utilcore
		j9avl
		j9hashtable
		j9pool
		j9thr
		j9stackmap
		omrcore
		#TODO: why isn't this in the module.xml?
		j9hookable
)

omr_add_exports(j9trc
	JVM_OnUnload
	JVM_OnLoad
	J9VMDllMain
)

target_enable_ddr(j9trc j9ddr)

install(
	TARGETS j9trc
	LIBRARY DESTINATION ${j9vm_SOURCE_DIR}
	RUNTIME DESTINATION ${j9vm_SOURCE_DIR}
)

