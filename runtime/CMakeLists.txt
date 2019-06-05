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

cmake_minimum_required(VERSION 3.4 FATAL_ERROR)

###
### Basic housekeeping stuff
### i.e. things which need to be set up before including J9vmPlatform
###

include(cmake/version.cmake)

project(j9vm VERSION ${J9VM_VERSION} LANGUAGES CXX C)

set(J9VM_OMR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/omr" CACHE PATH "Path to the OMR directory")
set(CMAKE_MODULE_PATH "${j9vm_SOURCE_DIR}/cmake/modules" "${J9VM_OMR_DIR}/cmake/modules" ${CMAKE_MODULE_PATH})

###
### End basic housekeeping
###

# Get platform specific configuration stuff
include(cmake/platform.cmake)

# If we are building on windows, we want ASM_MASM not ASM
if(OMR_OS_WINDOWS)
	enable_language(ASM_MASM)
else()
	enable_language(ASM)
endif()

# Pull in definitions for omr_add_hookgen and omr_add_tracegen
# these will be in omr/cmake/modules
include(OmrHookgen)
if(J9VM_IS_NON_STAGING)
	set(OMR_TRACE_ROOT "${CMAKE_CURRENT_BINARY_DIR}")
else()
	set(OMR_TRACE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
endif()
include(OmrTracegen)
include(OmrDDRSupport)
include(cmake/build_tags.cmake)
include(cmake/compiler_config.cmake)
include(cmake/J9vmGenAsm.cmake)

###
### Various configuration
### all configuration that does not need to proceed J9vmPlatform
###

# Build everything with -fPIC (or equivalent).
# We do this to ensure that our static libraries get built with -fPIC
# (cmake automatically builds shared libraries with -fPIC)
# TODO this has the side effect that we also build executables with -fPIE which we don't want.
# Need to investigate if it's possible to get one without the other. If it's not we need
# to explicitly tag individual targets which seems tedious.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(OMR_HOST_OS STREQUAL "osx")
	set(CMAKE_INSTALL_RPATH "@loader_path")
else()
	set(CMAKE_INSTALL_RPATH "\$ORIGIN")
endif()

set(OMR_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}" CACHE STRING "")
set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}" CACHE STRING "" FORCE)

set(OMR_INSTALL_BIN_DIR "${j9vm_SOURCE_DIR}" CACHE PATH "")
set(OMR_INSTALL_LIB_DIR "${j9vm_SOURCE_DIR}" CACHE PATH "")
set(OMR_INSTALL_ARCHIVE_DIR "${j9vm_SOURCE_DIR}/lib" CACHE PATH "")

# if this is a non-staging build, we want to put binaries in current source dir
if(J9VM_IS_NON_STAGING)
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
	# we also want to set the RPATH properly
	set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
endif()

# Generic configuration for omr
include(cmake/omr_config.cmake)

if(OPENJ9_BUILD)
	add_definitions(-DOPENJ9_BUILD)
endif()

# clean up the variables we used
set(versionStr)
set(checkCondition)
set(conditionalVersionString)

###
### Handle Generated files
###

configure_file(include/j9cfg.h.in j9cfg.h)
configure_file(include/j9lib.h.in j9lib.h)
configure_file(include/j9version.h.in j9version.h)

# Definition of add_hookgen pulled in from omr/cmake/modules/OmrHookgen.cmake
# Here we handle all the .hdf files which don't have a more logical home.
# i.e. they live in directories where we don't need to build anything
# and/or its hard to track down exactly who depends on them

omr_add_hookgen(INPUT oti/j9jit.hdf)
omr_add_hookgen(INPUT oti/j9vm.hdf)
omr_add_hookgen(INPUT oti/zipCachePool.hdf)
omr_add_hookgen(INPUT gc_include/j9mm.hdf)

# Wrap up the hookgen'd files in a named target
# This is to work around the fact that CMake does not do proper dependency tracking
# for generated files across different directories.
add_custom_target(j9vm_hookgen
	DEPENDS
		${CMAKE_CURRENT_BINARY_DIR}/jithook.h
		${CMAKE_CURRENT_BINARY_DIR}/vmhook.h
		${CMAKE_CURRENT_BINARY_DIR}/vmzipcachehook.h
		${CMAKE_CURRENT_BINARY_DIR}/mmhook.h
)

# Generate NLS headers
if(J9VM_IS_NON_STAGING)
	add_custom_command(
		OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/java.properties"
		DEPENDS j9nls
		COMMAND "${Java_JAVA_EXECUTABLE}" -cp "$<TARGET_PROPERTY:j9nls,JAR_FILE>" com.ibm.oti.NLSTool.J9NLS -source "${CMAKE_CURRENT_SOURCE_DIR}"
		VERBATIM
	)
	add_custom_target(j9vm_nlsgen
		DEPENDS
			"${CMAKE_CURRENT_BINARY_DIR}/java.properties"
	)
else()
	# if we aren't generating nls headers, just create a dummy target
	add_custom_target(j9vm_nlsgen)
endif()

if(J9VM_IS_NON_STAGING)
	set(J9VM_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/include")
	file(MAKE_DIRECTORY "${J9VM_INCLUDE_DIR}")
else()
	set(J9VM_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
endif()

# On z/OS, some generated files must be converted to EBCDIC for consistency.
# This macro is intended to be used as a replacement for add_custom_command
# for those files.
if(OMR_OS_ZOS)
	macro(add_custom_ascii_command)
		set(options)
		set(singleValueArgs OUTPUT WORKING_DIRECTORY)
		set(multiValueArgs COMMAND DEPENDS)
		cmake_parse_arguments(opt "${options}" "${singleValueArgs}" "${multiValueArgs}" ${ARGN})

		add_custom_command(OUTPUT "${opt_OUTPUT}"
			DEPENDS ${opt_DEPENDS}
			COMMAND ${opt_COMMAND}
			COMMAND iconv -f ISO8859-1 -t IBM-1047 < "${opt_OUTPUT}" > "${opt_OUTPUT}.tmp"
			COMMAND mv -f "${opt_OUTPUT}.tmp" "${opt_OUTPUT}"
			COMMAND chtag -t -c IBM-1047 "${opt_OUTPUT}"
			VERBATIM
			WORKING_DIRECTORY "${opt_WORKING_DIRECTORY}"
		)
	endmacro(add_custom_ascii_command)
else()
	macro(add_custom_ascii_command)
		set(options)
		set(singleValueArgs OUTPUT WORKING_DIRECTORY)
		set(multiValueArgs COMMAND DEPENDS)
		cmake_parse_arguments(opt "${options}" "${singleValueArgs}" "${multiValueArgs}" ${ARGN})

		add_custom_command(OUTPUT "${opt_OUTPUT}"
			DEPENDS ${opt_DEPENDS}
			COMMAND ${opt_COMMAND}
			VERBATIM
			WORKING_DIRECTORY "${opt_WORKING_DIRECTORY}"
		)
	endmacro(add_custom_ascii_command)
endif()

# generate headers

add_custom_ascii_command(OUTPUT "${J9VM_INCLUDE_DIR}/jni.h"
	DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/jni.h.m4"
	COMMAND m4 -D "JAVA_SPEC_VERSION=${JAVA_SPEC_VERSION}" "${CMAKE_CURRENT_SOURCE_DIR}/include/jni.h.m4" ${ASCII_TO_NATIVE} > "jni.h"
	VERBATIM
	WORKING_DIRECTORY "${J9VM_INCLUDE_DIR}"
)
add_custom_ascii_command(OUTPUT "${J9VM_INCLUDE_DIR}/jvmti.h"
	DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/jvmti.h.m4"
	COMMAND m4 -D "JAVA_SPEC_VERSION=${JAVA_SPEC_VERSION}" "${CMAKE_CURRENT_SOURCE_DIR}/include/jvmti.h.m4" ${ASCII_TO_NATIVE} > "jvmti.h"
	VERBATIM
	WORKING_DIRECTORY "${J9VM_INCLUDE_DIR}"
)

# Note we do this here rather than in the redirector directory to work around
# issues in cmake. If we did it there each target which consumed generated.c would need
# its own rule to create it, which leads race conditions when building in parallel.

add_custom_ascii_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/redirector/generated.c"
	DEPENDS oti/helpers.m4 redirector/generated.c.m4
	COMMAND m4 -I "${CMAKE_CURRENT_SOURCE_DIR}/oti" -I "${CMAKE_CURRENT_SOURCE_DIR}/redirector" "${CMAKE_CURRENT_SOURCE_DIR}/redirector/generated.c.m4" > generated.c
	VERBATIM
	WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/redirector"
)

add_custom_ascii_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/j9vm/generated.h"
	DEPENDS oti/helpers.m4 redirector/forwarders.m4 j9vm/generated.h.m4
	COMMAND m4 -I "${CMAKE_CURRENT_SOURCE_DIR}/oti" -I "${CMAKE_CURRENT_SOURCE_DIR}/redirector" "${CMAKE_CURRENT_SOURCE_DIR}/j9vm/generated.h.m4" > generated.h
	VERBATIM
	WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/j9vm"
)

add_custom_target(j9vm_m4gen
	DEPENDS
		"${CMAKE_CURRENT_BINARY_DIR}/redirector/generated.c"
		"${CMAKE_CURRENT_BINARY_DIR}/j9vm/generated.h"
		"${J9VM_INCLUDE_DIR}/jni.h"
		"${J9VM_INCLUDE_DIR}/jvmti.h"
)

if(J9VM_IS_NON_STAGING)
	add_custom_target(copy_default_options ALL
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/options.default" "${CMAKE_CURRENT_BINARY_DIR}/options.default"
	)
endif()

###
### Helper interface libraries
###

# j9vm_interface is used to hold properties we want to apply to everyone.
# e.g. adding oti/ to the include path etc.
add_library(j9vm_interface INTERFACE)
target_include_directories(j9vm_interface
	INTERFACE
		${CMAKE_CURRENT_SOURCE_DIR}/oti
		${CMAKE_CURRENT_SOURCE_DIR}/include
		${J9VM_OMR_DIR}/include_core
		${CMAKE_CURRENT_BINARY_DIR}/include
		${CMAKE_CURRENT_BINARY_DIR}/omr
		${CMAKE_CURRENT_BINARY_DIR}
		${CMAKE_BINARY_DIR}
)

if(J9VM_IS_NON_STAGING)
	target_include_directories(j9vm_interface INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/nls)
else()
	target_include_directories(j9vm_interface INTERFACE nls)
endif()

add_dependencies(j9vm_interface
	j9vm_hookgen
	j9vm_m4gen
	j9vm_nlsgen
)

# j9vm_gc_includes is used to track the include directories that are consumed by the various gc components
add_library(j9vm_gc_includes INTERFACE)
target_include_directories(j9vm_gc_includes
	INTERFACE
		# Pull in the public include paths exposed by the omrgc library
		$<TARGET_PROPERTY:omrgc,INTERFACE_INCLUDE_DIRECTORIES>
		# And then the include paths for the code we add ontop of the base omrgc
		${j9vm_SOURCE_DIR}/gc_include
		${j9vm_SOURCE_DIR}/gc_glue_java
		${j9vm_SOURCE_DIR}/gc_base
		${j9vm_SOURCE_DIR}/gc_stats
		${j9vm_SOURCE_DIR}/gc_structs
)

# Stub library to simplify programs which use makelib/cmain.c to provide a signal protected main
add_library(j9vm_main_wrapper INTERFACE)
target_sources(j9vm_main_wrapper INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/makelib/cmain.c)
target_link_libraries(j9vm_main_wrapper
	INTERFACE
		j9thr
		j9prt
)

###
### OMR configuration stuff
###

make_ddr_set(j9ddr)

# set up glue library target names
# See https://github.com/eclipse/omr/blob/master/doc/BuildingWithCMake.md
set(OMR_GC_GLUE_TARGET "j9vm_gc_glue" CACHE INTERNAL "")
set(OMR_RAS_GLUE_TARGET "j9vm_ras_glue" CACHE INTERNAL "")
set(OMR_CORE_GLUE_TARGET "j9vm_core_glue" CACHE INTERNAL "")
set(OMR_UTIL_GLUE_TARGET "j9vm_util_glue" CACHE INTERNAL "")

# setup OMR library overrides
set(OMR_GC_LIB "j9gc" CACHE INTERNAL "")
set(OMR_HOOK_LIB "j9hookable" CACHE INTERNAL "")
set(OMR_PORT_LIB "j9prt" CACHE INTERNAL "")
set(OMR_THREAD_LIB "j9thr" CACHE INTERNAL "")
set(OMR_TRACE_LIB "j9trc" CACHE INTERNAL "")

set(OMR_DDR_SET "j9ddr" CACHE INTERNAL "")

add_subdirectory(gc_glue_java)
# hack to add our binary dir to omr include path
# needed to grab the headers created by trace/hookgen
include_directories(${CMAKE_CURRENT_BINARY_DIR})
add_subdirectory("${J9VM_OMR_DIR}" omr)

# Due to CMake technical limitations we can't properly propagate dependencies
# to omrutil via the glue.
add_dependencies(omrutil_obj j9vm_nlsgen run_cptool)

###
### Share library version suffixing
###
# Note: this has to happen after we add_subdirectory omr

# The string we append to the name of shared libs
# set(versionStr "${J9VM_VERSION_MAJOR}${J9VM_VERSION_MINOR}")
set(versionStr "29")

# Save the old shared library suffix val (eg. '.so' or '.dll', so we can use it later if needed)
set(J9VM_OLD_SHARED_SUFFIX ${CMAKE_SHARED_LIBRARY_SUFFIX})
set(J9VM_SHARED_LIBRARY_SUFFIX ${versionStr}${CMAKE_SHARED_LIBRARY_SUFFIX})
# Prepend the version string so eg. '29.so' gets tacked onto the end of library names if they are built into shared libs
set(CMAKE_SHARED_LIBRARY_SUFFIX "${J9VM_SHARED_LIBRARY_SUFFIX}")

###
### Begin the definitions of our modules
###
# TODO: a few subdirectories still need to be implemented (currently commented out)
# TODO: most libraries still need to be gated with if() statements

if(J9VM_MODULE_BCUTIL)
	add_subdirectory(bcutil)
endif()
if(J9VM_MODULE_BCVERIFY)
	add_subdirectory(bcverify)
endif()
if(J9VM_MODULE_CASSUME)
	add_subdirectory(cassume)
endif()
if(J9VM_MODULE_CFDUMPER)
	add_subdirectory(cfdumper)
endif()
add_subdirectory(clear_vmi)
if(J9VM_MODULE_CODERT_VM)
	add_subdirectory(codert_vm)
endif()
add_subdirectory(cuda)
if(J9VM_MODULE_DBGEXT)
	add_subdirectory(dbgext)
endif()
if(OMR_DDR)
	add_subdirectory(ddr)
endif()
if(J9VM_MODULE_DDREXT)
	add_subdirectory(ddrext)
endif()
if(OMR_HOST_OS STREQUAL "win")
	# TODO: need to add windows support
	# add_subdirectory(dfix)
endif()
if(J9VM_MODULE_EXELIB)
	add_subdirectory(exelib)
endif()
if(J9VM_MODULE_FLSANITY)
	add_subdirectory(flsanity)
endif()
if(J9VM_MODULE_GC)
	add_subdirectory(gc)
endif()
if(J9VM_MODULE_GC_API)
	add_subdirectory(gc_api)
endif()
if(J9VM_MODULE_GC_BASE)
	add_subdirectory(gc_base)
endif()
if(J9VM_MODULE_GC_CHECK)
	add_subdirectory(gc_check)
endif()
if(J9VM_MODULE_DDR)
	# TODO: Does this need to be implemented? Or are we only going to bother to support
	# new style ddrgen with cmake?
	# add_subdirectory(gc_ddr)
endif()
if(J9VM_MODULE_GC_MODRON_STANDARD)
	add_subdirectory(gc_modron_standard)
endif()
if(J9VM_MODULE_GC_MODRON_STARTUP)
	add_subdirectory(gc_modron_startup)
endif()
if(J9VM_MODULE_GC_REALTIME)
	add_subdirectory(gc_realtime)
endif()
if(J9VM_MODULE_GC_STATS)
	add_subdirectory(gc_stats)
endif()
if(J9VM_MODULE_GC_STRUCTS)
	add_subdirectory(gc_structs)
endif()
# NOTE this is not conditional in the UMA module.xml
add_subdirectory(gc_tests)
if(J9VM_MODULE_GC_TRACE)
	add_subdirectory(gc_trace)
	if(J9VM_MODULE_GC_MODRON_STANDARD)
		add_subdirectory(gc_trace_standard)
	endif()
	if(J9VM_GC_VLHGC)
		add_subdirectory(gc_trace_vlhgc)
	endif()
endif()
if(J9VM_GC_REALTIME)
	add_subdirectory(gc_verbose_handler_realtime)
endif()
if(J9VM_GC_MODRON_STANDARD)
	add_subdirectory(gc_verbose_handler_standard_java)
endif()
if(J9VM_GC_VLHGC)
	add_subdirectory(gc_verbose_handler_vlhgc)
endif()
# NOTE this is not conditional in the UMA module.xml
add_subdirectory(gc_verbose_java)
add_subdirectory(gc_verbose_old)
add_subdirectory(gc_verbose_old_events)
if(J9VM_GC_VLHGC)
	add_subdirectory(gc_vlhgc)
endif()
if(J9VM_MODULE_GCCHK)
	add_subdirectory(gcchk)
endif()
if(J9VM_MODULE_GDB_PLUGIN)
	add_subdirectory(gdb_plugin)
endif()
if(J9VM_OPT_HARMONY)
	add_subdirectory(harmony)
	add_subdirectory(hyvm)
endif()
if(J9VM_MODULE_HOOKABLE)
	add_subdirectory(hookable)
endif()
if(J9VM_MODULE_J9VM)
	add_subdirectory(j9vm)
endif()
# NOTE this is not conditional in the UMA module.xml
add_subdirectory(jcl)
if(J9VM_MODULE_JEXTRACTNATIVES)
	add_subdirectory(jextractnatives)
endif()
# NOTE this is not conditional in the UMA module.xml
add_subdirectory(jilgen)
if(J9VM_MODULE_J9JIT_VM)
	add_subdirectory(jit_vm)
endif()
if(J9VM_MODULE_JNICHK)
	add_subdirectory(jnichk)
endif()
if(J9VM_MODULE_JNIINV)
	add_subdirectory(jniinv)
endif()
add_subdirectory(jsigWrapper)
if(J9VM_MODULE_JVMTI)
	add_subdirectory(jvmti)
endif()
# NOTE this is not conditional in the UMA module.xml
add_subdirectory(libffi)
add_subdirectory(mgmt)
if(J9VM_MODULE_PORT)
	add_subdirectory(port)
endif()
if(J9VM_MODULE_RASDUMP)
	add_subdirectory(rasdump)
endif()
if(J9VM_MODULE_RASTRACE)
	add_subdirectory(rastrace)
endif()
if(J9VM_MODULE_J9VM)
	add_subdirectory(redirector)
endif()
add_subdirectory(runtimetools)
if(J9VM_MODULE_SHARED)
	add_subdirectory(shared)
endif()
if(J9VM_MODULE_SHARED_COMMON)
	add_subdirectory(shared_common)
endif()
if(J9VM_MODULE_SHARED_UTIL)
	add_subdirectory(shared_util)
endif()
if(J9VM_MODULE_SIMPLEPOOL)
	add_subdirectory(simplepool)
endif()
if(J9VM_MODULE_STACKMAP)
	add_subdirectory(stackmap)
endif()
if(J9VM_OPT_SIDECAR OR J9VM_OPT_HARMONY)
	add_subdirectory(sunvmi)
endif()
if(J9VM_MODULE_THREAD)
	add_subdirectory(thread)
endif()
if(J9VM_MODULE_THRTRACE)
	add_subdirectory(thrtrace)
endif()
if(J9VM_INTERP_NATIVE_SUPPORT)
	add_subdirectory(compiler)
endif()
if(J9VM_MODULE_UTIL)
	add_subdirectory(util)
endif()
# NOTE this is not conditional in the UMA module.xml
add_subdirectory(util_core)
if(J9VM_MODULE_VERBOSE)
	add_subdirectory(verbose)
endif()
if(J9VM_MODULE_VERUTIL)
	add_subdirectory(verutil)
endif()
if(J9VM_MODULE_VM)
	add_subdirectory(vm)
endif()
add_subdirectory(vmchk)
# TODO: need to add windows support
if(J9VM_MODULE_WINDBG)
	if(OMR_ENV_DATA32)
		# add_subdirectory(win)
	else()
		# add_subdirectory(win_64)
	endif()
	# add_subdirectory(windbg)
endif()
if(J9VM_MODULE_ZIP)
	add_subdirectory(zip)
endif()
if(J9VM_MODULE_ZLIB)
	add_subdirectory(zlib)
endif()

# NOTE this is not conditional in the UMA module.xml
add_subdirectory(tests)
