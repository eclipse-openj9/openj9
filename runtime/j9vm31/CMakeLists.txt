################################################################################
# Copyright IBM Corp. and others 2021
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

# Open XL incorrectly handles static init structs containing function pointers used in
# jnicsup.cpp and jniinv.cpp, resulting in an 0C4 ABEND when invoked via a CEL4RO31 call.
# Temporarily force libjvm31.so to be compiled with XL C/C++ toolchain as a workaround.
if(OMR_TOOLCONFIG STREQUAL "openxl")
	enable_language(J9VM31_CC)
	enable_language(J9VM31_CXX)
	set(USE_J9VM31_LANGUAGES TRUE)
	message(STATUS "j9vm31: Using custom J9VM31_CC and J9VM31_CXX languages with XL C/C++ toolchain.")
else()
	set(USE_J9VM31_LANGUAGES FALSE)
endif()

# libjvm31 is a standalone 31-bit shim library in what is otherwise a 64-bit build.
add_library(jvm31 SHARED
	j9cel4ro64.c
	j9vm31floatstubs.s
	jnicgen.cpp
	jnicsup.cpp
	jnifield.cpp
	jniinv.cpp
	jnimisc.cpp
	jnireflect.cpp
)

if(USE_J9VM31_LANGUAGES)
	set_source_files_properties(
		j9cel4ro64.c
		PROPERTIES LANGUAGE J9VM31_CC)

	set_source_files_properties(
		jnicgen.cpp
		jnicsup.cpp
		jnifield.cpp
		jniinv.cpp
		jnimisc.cpp
		jnireflect.cpp
		PROPERTIES LANGUAGE J9VM31_CXX
	)
	set_target_properties(jvm31 PROPERTIES LINKER_LANGUAGE J9VM31_CXX)

	# XL C/C++ generates the sidedeck file in the working directory. Copy it
	# to the target directory where Open XL would have generated the file.
	add_custom_command(TARGET jvm31 POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different
					"${CMAKE_CURRENT_BINARY_DIR}/libjvm31.x"
					"${CMAKE_CURRENT_BINARY_DIR}/../libjvm31.x"
			VERBATIM
	)
endif()

# Note: These include directories need to be declared ahead of the removals of compile flags
# below, otherwise, the directories will not be added.
target_include_directories(jvm31
	PRIVATE
		# Use the include31 with the generated header artifacts.
		${CMAKE_CURRENT_BINARY_DIR}/../include31
		# Explicitly include OMR's include directories, instead of linking to INTERFACE, to ensure
		# we don't pick up other 64-bit linking dependencies.
		${omr_SOURCE_DIR}/include_core
		${CMAKE_CURRENT_BINARY_DIR}/../omr
)
add_dependencies(jvm31 j9vm31_m4gen)

include(exports.cmake)

# Need to remove XPLINK and 64-bit compile and link options, as libjvm31 needs to be a 31-bit Standard Linkage library.

# Flags to be removed when compiling.
set(compile_flags_to_remove
	"\"-Wc,xplink\""
	"-Wc,lp64"
	"\"-Wa,SYSPARM(BIT64)\""
	"-DJ9ZOS39064"
	"-qxplink=noback"
	"-m64"
)

if(USE_J9VM31_LANGUAGES)
	# Set the default XL C/C++ compilation / linking flags.
	omr_stringify(CMAKE_J9VM31_CC_FLAGS
		-O3
		-qdebug=nohook
		-qnogonumber
		-qstackprotect
		"\"-Wc,ARCH(10)\""
		-Wc,DLL,EXPORTALL
		"\"-Wc,inline(auto,noreport,600,5000)\""
		"\"-Wc,langlvl(extc99)\""
		"\"-Wc,list(),offset,gonumber\""
		"\"-Wc,TUNE(12)\""
	)

	omr_stringify(CMAKE_J9VM31_CXX_FLAGS
		-O3
		-qdebug=nohook
		-qnoeh
		-qnogonumber
		-qstackprotect
		-Wc,rostring
		"\"-Wc,enum(4)\""
		"\"-Wc,FLOAT(IEEE,FOLD,AFP)\""
		-Wa,goff
		"\"-Wc,inline(auto,noreport,600,5000)\""
		"\"-Wc,list(),offset,gonumber\""
		-Wc,NOANSIALIAS
		"\"-Wc,TARGET(zOSV2R3)\""
		-+
		"\"-Wc,ARCH(10)\""
		-Wc,DLL,EXPORTALL
		"\"-Wc,TUNE(12)\""
		-qasm
		-qlanglvl=extended0x
		-qnortti
	)

	omr_stringify(CMAKE_SHARED_LIBRARY_J9VM31_CXX_FLAGS
		-Wl,compat=zOSV2R3
		-Wl,dll
	)
else()
	if(OMR_TOOLCONFIG STREQUAL "openxl")
		set(compile_flags_to_append
			"-m32n"
			"-mzos-float-kind=ieee"
			"-Wno-c++11-narrowing"
		)
	endif()

	foreach(flag IN LISTS compile_flags_to_remove)
		omr_remove_flags(CMAKE_C_FLAGS "${flag}")
		omr_remove_flags(CMAKE_CXX_FLAGS "${flag}")
		omr_remove_flags(CMAKE_ASM_FLAGS "${flag}")
	endforeach()

	foreach(flag IN LISTS compile_flags_to_append)
		omr_append_flags(CMAKE_C_FLAGS "${flag}")
		omr_append_flags(CMAKE_CXX_FLAGS "${flag}")
		omr_append_flags(CMAKE_ASM_FLAGS "${flag}")
	endforeach()
endif()

# Flags to be removed when linking.
set(link_flags_to_remove
	"-Wl,xplink"
	"-Wl,lp64"
)

foreach(flag IN LISTS link_flags_to_remove)
	omr_remove_flags(CMAKE_SHARED_LINKER_FLAGS "${flag}")
endforeach()

install(
	TARGETS jvm31
	LIBRARY DESTINATION ${j9vm_SOURCE_DIR}
	RUNTIME DESTINATION ${j9vm_SOURCE_DIR}
)
