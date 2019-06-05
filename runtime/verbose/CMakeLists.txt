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

add_tracegen(j9vrb.tdf)

add_library(j9vrb SHARED
	errormessage_internal.c
	errormessagebuffer.c
	errormessageframeworkcfr.c
	errormessageframeworkrtv.c
	errormessagehelper.c
	verbose.c

	${j9vm_SOURCE_DIR}/vm/swalk.c
	${j9vm_SOURCE_DIR}/vm/linearswalk.c
	
	${j9vm_SOURCE_DIR}/codert_vm/jswalk.c #IF J9VM_INTERP_NATIVE_SUPPORT
	${j9vm_SOURCE_DIR}/jit_vm/ctsupport.c #IF J9VM_INTERP_USE_SPLIT_SIDE_TABLES
	${j9vm_SOURCE_DIR}/compiler/runtime/MethodMetaData.c
	
	${CMAKE_CURRENT_BINARY_DIR}/ut_j9vrb.c
)

#TODO this is platform specific
set_property(SOURCE ../compiler/runtime/MethodMetaData.c APPEND_STRING PROPERTY COMPILE_FLAGS  " -std=gnu89")
target_include_directories(j9vrb
	PRIVATE
		${CMAKE_CURRENT_BINARY_DIR}
		${j9vm_SOURCE_DIR}/compiler/runtime
		${j9vm_SOURCE_DIR}/compiler/codegen
		${omr_SOURCE_DIR}/compiler/codegen
		${omr_SOURCE_DIR}/compiler
		${j9vm_SOURCE_DIR}/compiler
		${j9vm_SOURCE_DIR}/shared_common/include
)

target_link_libraries(j9vrb
	PRIVATE
		j9vm_interface
		j9vm_gc_includes
		j9vm_compiler_defines

		omrgc
		j9shr
		j9trc
		j9stackmap
		j9util
		j9utilcore
		j9verutil
		j9avl
		j9hashtable
		j9pool
		j9thr
		j9gcbase
		j9gcstructs
		j9gcstats
		j9gcapi
		j9modronstartup
		j9gcvrbjava
		j9gcvrbold
		j9gcvrbevents
		j9modronstandard
		j9realtime
		j9gcvlhgc
		j9gcvrbhdlrstandardjava
		j9gcvrbhdlrrealtime
		j9gcvrbhdlrvlhgc
		j9hookable
		j9zip

		j9utilcore
		j9vm
		j9codert_vm
		j9modronstandard
		j9gc
)



target_compile_definitions(j9vrb
	PRIVATE
		-DJ9VM_INTERP_STACKWALK_TRACING
		-DJ9VM_INTERP_LINEAR_STACKWALK_TRACING
)

omr_add_exports(j9vrb
	JVM_OnLoad
	JVM_OnUnload
	J9VMDllMain
)

install(
	TARGETS j9vrb
	LIBRARY DESTINATION ${j9vm_SOURCE_DIR}
	RUNTIME DESTINATION ${j9vm_SOURCE_DIR}
)
