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

add_library(ffi STATIC
	closures.c
	debug.c
	java_raw_api.c
	prep_cif.c
	raw_api.c
	types.c
)

# We need this because ffi_common.h is in 'include' in the top directory.
# once we fix that this can go away
target_include_directories(ffi PRIVATE ${j9vm_SOURCE_DIR}/include)

target_include_directories(ffi PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

#platform specific stuff hacked in
if(OMR_ARCH_X86)
	if(NOT OMR_ENV_DATA64)
		message(SEND_ERROR "Only x86-64 is currently supported")
	endif()

	target_include_directories(ffi PUBLIC x86)
	if(OMR_OS_LINUX)
		target_include_directories(ffi PUBLIC preconf/xa)
	elseif(OMR_OS_OSX)
		target_include_directories(ffi PUBLIC preconf/oa)
	elseif(OMR_OS_WINDOWS)
		target_include_directories(ffi PUBLIC preconf/wa)
	else()
		message(SEND_ERROR "Unsupported OS")
	endif()

	target_sources(ffi
		PRIVATE
		x86/ffi.c
		x86/ffi64.c
	)

	if(OMR_OS_WINDOWS)
		# We are assuming we are using an MSVC toolchain
		add_custom_command(
			OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/win64.asm
			COMMAND ${CMAKE_C_COMPILER} -nologo -EP -P  -I ${CMAKE_CURRENT_SOURCE_DIR}/preconf/wa -I ${CMAKE_CURRENT_SOURCE_DIR}/x86 -Fiwin64.asm  ${CMAKE_CURRENT_SOURCE_DIR}/x86/win64.S
			DEPENDS x86/win64.S
		)
		target_sources(ffi PRIVATE win64.asm)
		set_source_files_properties(win64.asm PROPERTIES LANGUAGE ASM_MASM)
	else()
		target_sources(ffi PRIVATE x86/sysv.S)
	endif()

	if(OMR_HOST_OS STREQUAL "linux")
		target_sources(ffi
			PRIVATE
			x86/unix64.S
		)
	elseif(OMR_HOST_OS STREQUAL "osx")
		target_sources(ffi
			PRIVATE
			x86/darwin64.s
		)
	endif()
elseif(OMR_ARCH_POWER)
	if(CMAKE_CXX_COMPILER_ID STREQUAL "XL")
		set(CMAKE_ASM_FLAGS "-qalias=noansi -qxflag=LTOL:LTOL0 -qxflag=selinux -qarch=ppc64 -q64")
	endif()
	target_sources(ffi
		PRIVATE
			powerpc/ffi.c
			powerpc/ppc_closure.S
			powerpc/sysv.S
			powerpc/linux64.S
			powerpc/linux64_closure.S
			powerpc/ffi_linux64.c
			powerpc/ffi_sysv.c
	)
	set_source_files_properties(
		powerpc/ppc_closure.S
		powerpc/sysv.S
		powerpc/linux64.S
		powerpc/linux64_closure.S
		PROPERTIES
		LANGUAGE ASM
	)
	if(OMR_ENV_LITTLE_ENDIAN)
		target_include_directories(ffi PUBLIC preconf/xl64)
	else()
		target_include_directories(ffi PUBLIC preconf/xp64)
	endif()
	target_include_directories(ffi PUBLIC powerpc)
endif()
