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

set(OMR_ENHANCED_WARNINGS OFF)

add_tracegen(j9codertvm.tdf)
j9vm_add_library(j9codert_vm STATIC
	cache.c
	cnathelp.cpp
	CodertVMHelpers.cpp
	decomp.cpp
	dlt.c
	fsdcsup.c
	jitavl.c
	jithash.cpp
	jswalk.c
	thunkcrt.c

	${CMAKE_CURRENT_BINARY_DIR}/ut_j9codertvm.c
)

target_include_directories(j9codert_vm
	PRIVATE
		.
		${CMAKE_CURRENT_BINARY_DIR}
		${j9vm_SOURCE_DIR}/gc_include
		${omr_SOURCE_DIR}/gc/include
		${j9vm_SOURCE_DIR}/compiler/runtime/
)

if(OMR_ARCH_X86)
	j9vm_gen_asm(xnathelp.m4)
	j9vm_gen_asm(xvector.m4)

	target_sources(j9codert_vm PRIVATE
		xnathelp.s
		xvector.s
	)
elseif(OMR_ARCH_POWER)
	j9vm_gen_asm(pnathelp.m4)

	target_sources(j9codert_vm PRIVATE
		pnathelp.s
	)
elseif(OMR_ARCH_S390)
	set(m4_defines )
	if(OMR_OS_ZOS)
		if(OMR_ENV_DATA64)
			list(APPEND m4_defines  TR_64Bit TR_HOST_64BIT)
		endif()
		# NOTE: no flags needed for 31 bit


		if(J9VM_JIT_FREE_SYSTEM_STACK_POINTER)
			list(APPEND m4_defines J9VM_JIT_FREE_SYSTEM_STACK_POINTER)
		endif()
	endif()
	j9vm_gen_asm(znathelp.m4 DEFINES ${m4_defines})

	target_sources(j9codert_vm PRIVATE
		znathelp.s
	)
elseif(OMR_ARCH_ARM)
	j9vm_gen_asm(armnathelp.m4)
	target_sources(j9codert_vm PRIVATE
		armnathelp.s
	)
elseif(OMR_ARCH_AARCH64)
	j9vm_gen_asm(arm64nathelp.m4)
	target_sources(j9codert_vm PRIVATE
		arm64nathelp.s
	)
elseif(OMR_ARCH_RISCV)
	j9vm_gen_asm(riscvnathelp.m4)
	target_sources(j9codert_vm PRIVATE
		riscvnathelp.s
	)	
else()
	message(SEND_ERROR "Unsupported platform")
endif()


if(J9VM_JIT_RUNTIME_INSTRUMENTATION)
	target_sources(j9codert_vm PRIVATE runtimeInstrumentation.c)
endif()

target_link_libraries(j9codert_vm
	PRIVATE
		j9vm_interface
		j9vm_compiler_defines
)
