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

# This CMakeLists is included by the VM CMake lists, and works after composition
# has occurred.
#
# Relies on a few pieces of OMR, and a few pieces of the VM CMakeLists.
#
# Longer term, this will of course have to collapse into the VM builds.

if(OMR_ARCH_X86)
	# On windows, the proper binary format is not auto detected
	if(OMR_OS_WINDOWS)
		if(OMR_ENV_DATA64)
			set(CMAKE_ASM_NASM_OBJECT_FORMAT win64)
		else()
			set(CMAKE_ASM_NASM_OBJECT_FORMAT win32)
		endif()
	endif()
	enable_language(ASM_NASM)
	# We have to manually append "/" to the paths as NASM versions older than v2.14 requires trailing / in the directory paths
	set(asm_inc_dirs
		"-I${j9vm_SOURCE_DIR}/oti/"
		"-I${j9vm_BINARY_DIR}/oti/"
		"-I${CMAKE_CURRENT_SOURCE_DIR}/"
		"-I${CMAKE_CURRENT_SOURCE_DIR}/x/runtime/"
		"-I${CMAKE_CURRENT_SOURCE_DIR}/x/amd64/runtime/"
		"-I${CMAKE_CURRENT_SOURCE_DIR}/x/i386/runtime/"
	)
	omr_append_flags(CMAKE_ASM_NASM_FLAGS ${asm_inc_dirs})
endif()

# On windows exceptions are controlled via a the preprocessor define _HAS_EXCEPTIONS
# We need to enable it again, after omr platform config removed it
get_directory_property(compile_defs COMPILE_DEFINITIONS)
list(REMOVE_ITEM compile_defs "_HAS_EXCEPTIONS=0")
set_property(DIRECTORY PROPERTY COMPILE_DEFINITIONS ${compile_defs})

omr_add_tracegen(env/j9jit.tdf)

# this is a workaround because the jit code is inconsistent about how it includes the tracegen files
# It only really works if we put it in the source tree
add_custom_command(
	OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/env/ut_j9jit.h
	DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/ut_j9jit.h
	COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_BINARY_DIR}/ut_j9jit.h" "${CMAKE_CURRENT_SOURCE_DIR}/env/ut_j9jit.h"
	VERBATIM
)
add_custom_target(j9jit_tracegen DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/env/ut_j9jit.h)

#TODO We should get rid of this, but its still required by the compiler_support module in omr
set(J9SRC  ${j9vm_SOURCE_DIR})


#include compiler support module from OMR
include(OmrCompilerSupport)

# Override masm2gas with J9JIT version until we've
# resolved the divergence.
set(masm2gas_path  build/scripts/masm2gas.pl )
get_filename_component(masm2gas_path ${masm2gas_path} ABSOLUTE)
set(MASM2GAS_PATH  ${masm2gas_path} CACHE INTERNAL "MASM2GAS PATH")

# The list of files that are added to the compiler in addition
# To the defaults provided by create_omr_compiler_library
set(J9JIT_FILES "" CACHE INTERNAL "The computed list of j9jit files")

# Used inside added subdirectories to help
# fill in the object list
macro(j9jit_files)
	set(J9JIT_FILES ${J9JIT_FILES} ${ARGN} CACHE INTERNAL "The computed list of j9jit files")
endmacro(j9jit_files)

j9jit_files(${CMAKE_CURRENT_BINARY_DIR}/ut_j9jit.c)
# Add our subdirs.
add_subdirectory(codegen)
add_subdirectory(compile)
add_subdirectory(control)
add_subdirectory(env)
add_subdirectory(il)
add_subdirectory(ilgen)
add_subdirectory(infra)
add_subdirectory(optimizer)
add_subdirectory(ras)
add_subdirectory(runtime)

# Should this be conditional?
if(TR_HOST_ARCH STREQUAL "x" OR TR_TARGET_ARCH STREQUAL "x")
	add_subdirectory(x)
endif()
if(TR_HOST_ARCH STREQUAL "p" OR TR_TARGET_ARCH STREQUAL "p")
	add_subdirectory(p)
endif()
if(TR_HOST_ARCH STREQUAL "z" OR TR_TARGET_ARCH STREQUAL "z")
	add_subdirectory(z)
endif()
if(TR_HOST_ARCH STREQUAL "arm" OR TR_TARGET_ARCH STREQUAL "arm")
	add_subdirectory(arm)
endif()

# Some files in OMR provide duplicate or extra functionality not needed
# in J9, so we need to remove them
set(REMOVED_OMR_FILES
	${omr_SOURCE_DIR}/compiler/control/CompileMethod.cpp
	${omr_SOURCE_DIR}/compiler/control/CompilationController.cpp
	${omr_SOURCE_DIR}/compiler/env/FEBase.cpp
	${omr_SOURCE_DIR}/compiler/optimizer/FEInliner.cpp
	${omr_SOURCE_DIR}/compiler/env/JitConfig.cpp
	${omr_SOURCE_DIR}/compiler/env/PersistentAllocator.cpp
	${omr_SOURCE_DIR}/compiler/env/SystemSegmentProvider.cpp
	${omr_SOURCE_DIR}/compiler/infra/OMRMonitor.cpp
	${omr_SOURCE_DIR}/compiler/runtime/Trampoline.cpp
	${omr_SOURCE_DIR}/compiler/runtime/Runtime.cpp
	${omr_SOURCE_DIR}/compiler/ilgen/IlInjector.cpp
	${omr_SOURCE_DIR}/compiler/ilgen/OMRBytecodeBuilder.cpp
	${omr_SOURCE_DIR}/compiler/ilgen/OMRIlBuilder.cpp
	${omr_SOURCE_DIR}/compiler/ilgen/OMRIlValue.cpp
	${omr_SOURCE_DIR}/compiler/ilgen/OMRJitBuilderRecorder.cpp
	${omr_SOURCE_DIR}/compiler/ilgen/OMRJitBuilderRecorderBinaryBuffer.cpp
	${omr_SOURCE_DIR}/compiler/ilgen/OMRJitBuilderRecorderBinaryFile.cpp
	${omr_SOURCE_DIR}/compiler/ilgen/OMRJitBuilderRecorderTextFile.cpp
	${omr_SOURCE_DIR}/compiler/ilgen/OMRMethodBuilder.cpp
	${omr_SOURCE_DIR}/compiler/ilgen/OMRThunkBuilder.cpp
	${omr_SOURCE_DIR}/compiler/ilgen/OMRTypeDictionary.cpp
	${omr_SOURCE_DIR}/compiler/ilgen/OMRVirtualMachineOperandArray.cpp
	${omr_SOURCE_DIR}/compiler/ilgen/OMRVirtualMachineOperandStack.cpp
	${omr_SOURCE_DIR}/compiler/ilgen/OMRVirtualMachineRegister.cpp
	${omr_SOURCE_DIR}/compiler/ilgen/OMRVirtualMachineRegisterInStruct.cpp
	${omr_SOURCE_DIR}/compiler/ilgen/OMRVirtualMachineState.cpp
)


get_target_property(compiler_defines j9vm_compiler_defines INTERFACE_COMPILE_DEFINITIONS)
# Extra defines not provided by the create_omr_compiler_library call
set(TARGET_DEFINES
	J9_PROJECT_SPECIFIC
	_LONG_LONG
	${compiler_defines}
)

# When VM is built with CMake, we should just use the
# INTERFACE specification of target_link_libraries,
# though that would also involve teaching MASM2GAS and
# PASM2ASM about target includes.
#
# In the mean time, like the makefiles, this is is using
# the includes from the SDK
set(J9_INCLUDES
	# From jitinclude.mk
	${omr_SOURCE_DIR}/thread
	${omr_SOURCE_DIR}/gc/include
	# endjitinclude.mk
	../ # Frustratingly there are some #include "frob/baz.hpp" refs in the compiler which require this.
	${j9vm_SOURCE_DIR}/codert_vm
	${j9vm_SOURCE_DIR}/gc_include
	${j9vm_SOURCE_DIR}/gc_glue_java
	${j9vm_SOURCE_DIR}/jit_vm
	${j9vm_SOURCE_DIR}/nls
	${j9vm_SOURCE_DIR}/oti
	${j9vm_SOURCE_DIR}/include
	${j9vm_SOURCE_DIR}/util
	${j9vm_BINARY_DIR}
	${omr_BINARY_DIR}
	${CMAKE_CURRENT_BINARY_DIR}
)

# Platform specific list of flags, derived directly from the
# Makefiles
set(J9_SHAREDFLAGS
	${OMR_PLATFORM_COMPILE_OPTIONS}
	${OMR_PLATFORM_C_COMPILE_OPTIONS}
)
set(J9_CXXFLAGS
	${OMR_PLATFORM_COMPILE_OPTIONS}
	${OMR_PLATFORM_CXX_COMPILE_OPTIONS}
)
if(OMR_OS_LINUX)
	set(J9_SHAREDFLAGS
		${J9_SHAREDFLAGS}
		-fno-strict-aliasing
		-Wno-deprecated
		-Wno-enum-compare
		-Wno-write-strings
		-fomit-frame-pointer
		-fasynchronous-unwind-tables
		-Wreturn-type
		-pthread
	)

	# Platform specific CXX flags, also derived from the
	# Makefiles
	set(J9_CXXFLAGS
		${J9_CXXFLAGS}
		-fno-rtti
		-fno-threadsafe-statics
		-Wno-invalid-offsetof
	)
	set(J9_CFLAGS
		${J9_CFLAGS}
		-std=gnu89
	)
elseif(OMR_OS_WINDOWS)
	set(J9_SHAREDFLAGS
		-D_WIN32
		-DWIN32
		-DSUPPORTS_THREAD_LOCAL
		-DWINDOWS
	)
else()
	message(SEND_ERROR "unsupported platform")
endif()

omr_stringify(J9_CFLAGS_STR ${J9_CFLAGS} ${J9_SHAREDFLAGS})
omr_stringify(J9_CXXFLAGS_STR ${J9_CXXFLAGS} ${J9_SHAREDFLAGS})

# Note: This is explicitly overriding what's provided by
#       the VM CMakeLists, as they pass -fno-exceptions
#       right now, and the JIT needs exceptions
set(CMAKE_CXX_FLAGS "${J9_CXXFLAGS_STR}")
set(CMAKE_C_FLAGS "${J9_CFLAGS_STR}")

create_omr_compiler_library(NAME j9jit
	SHARED
	OBJECTS ${J9_FILES} ${J9JIT_FILES} ${NOT_EXFRRED}
	DEFINES J9_PROJECT_SPECIFIC ${TARGET_DEFINES}
	INCLUDES ${J9_INCLUDES}
	FILTER ${REMOVED_OMR_FILES}
)

add_dependencies(j9jit
	omrgc_hookgen
	j9jit_tracegen
)

target_link_libraries(j9jit
	PRIVATE
		j9vm_interface
		j9avl
		j9codert_vm
		j9hashtable
		j9jit_vm
		j9pool
		j9stackmap
		j9util
		j9utilcore
		j9hookable
		j9thr
		${CMAKE_DL_LIBS}
)

if(OMR_OS_LINUX)
	set_property(TARGET j9jit APPEND_STRING PROPERTY
		LINK_FLAGS "  -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/build/scripts/j9jit.linux.exp")
	target_link_libraries(j9jit PRIVATE m)
elseif(OMR_OS_WINDOWS)
	target_sources(j9jit PRIVATE build/scripts/j9jit.def)
endif()
set_property(TARGET j9jit PROPERTY LINKER_LANGUAGE CXX)

install(
	TARGETS j9jit
	LIBRARY DESTINATION ${j9vm_SOURCE_DIR}
	RUNTIME DESTINATION ${j9vm_SOURCE_DIR}
)
