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

# Note: The structure of this file is a bit weird.
# In order to make the transition to cmake easier OMR has a function called omr_find_files
# which mirrors the behaviour of the vpath mechanism in make. See omr/cmake/modules/OmrFindFiles.cmake.

# The logic here is largely a 1:1 translation of the module xml. We build up a list of sources
# (the OBJECTS variable), make a platform specific list of search paths (VPATH variable).
# The final step is where the differences are. While vpaths under make handle everything automatically,
# we have to actually resolve the full paths to the source files (via omr_find_files and output in resolvedPaths)

set(OMR_ENHANCED_WARNINGS OFF)

add_tracegen(common/j9prt.tdf)
j9vm_add_library(j9prt SHARED
	OUTPUT_NAME j9prt${J9VM_VERSION_SUFFIX}
	${CMAKE_CURRENT_BINARY_DIR}/ut_j9prt.c

	# Pull in the objects compiled as part of the OMR port library
	$<TARGET_OBJECTS:omrport_obj>
)

set(OBJECTS "")
list(APPEND OBJECTS
	j9gp.c
	j9gs.c
	j9hypervisor.c
	j9hypervisor_common.c
	j9ipcmutex.c
	j9mem_basic.c
	j9port.c
	j9portcontrol.c
	j9process.c
	j9shmem.c
	j9shsem.c
	j9shsem_deprecated.c
	j9sock.c
	j9sockptb.c
	j9sysinfo.c
)
if(OMR_ARCH_X86)
	list(APPEND OBJECTS j9hypervisor_i386.c)
elseif(OMR_ARCH_POWER)
	list(APPEND OBJECTS j9hypervisor_systemp.c)
elseif(OMR_ARCH_S390)
	list(APPEND OBJECTS
		j9gs_s390.c
		j9hypervisor_systemz.c
	)
endif()

if(OMR_ARCH_X86 OR OMR_ARCH_S390)
	list(APPEND OBJECTS j9sysinfo_helpers.c)
endif()

if(OMR_OS_ZOS)
	list(APPEND OBJECTS
		j9csrsi.c
		j9csrsi_wrp.s
		j9gs_get_supported.s
		j9gs_load_gscb.s
		j9gs_store_gscb.s
		j9mprotect.s
		j9munprotect.s
		protect_helpers.c
	)
	omr_compile_metalc(
		zos390/j9guardedstorage.mc
		zos390/j9guardedstorage.o
	)
	target_sources(j9prt PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/zos390/j9guardedstorage.o)
endif()

if(NOT OMR_OS_WINDOWS)
	list(APPEND OBJECTS
		j9sharedhelper.c
		j9SysvIPCWrappers.c
	)
endif()

if(J9VM_PORT_RUNTIME_INSTRUMENTATION)
	list(APPEND OBJECTS j9ri.c)
endif()

set(resolvedPaths "")
set(VPATH "")

if(OMR_OS_LINUX)
	if(OMR_ARCH_X86 AND OMR_ENV_DATA64)
		list(APPEND VPATH linuxamd64)
	elseif(OMR_ARCH_POWER)
		list(APPEND VPATH linuxppc)
	elseif(OMR_ARCH_S390)
		list(APPEND VPATH linuxs390)
		target_include_directories(j9prt PRIVATE linuxs390)
	endif()
	list(APPEND VPATH linux)
elseif(OMR_OS_WINDOWS)
	list(APPEND VPATH win32)
elseif(OMR_OS_AIX)
	list(APPEND VPATH aix)
elseif(OMR_OS_ZOS)
	list(APPEND VPATH zos390)
endif()

if(NOT OMR_OS_WINDOWS)
	list(APPEND VPATH
		sysvipc
		unix
	)
endif()

list(APPEND VPATH common)

omr_find_files(resolvedPaths
	PATHS
	${VPATH}
	FILES
	${OBJECTS}
)


target_sources(j9prt
	PRIVATE
		${resolvedPaths}
)


target_link_libraries(j9prt 
	PUBLIC
		j9vm_interface

	PRIVATE
		omrsig
		j9utilcore
		j9pool
		j9hashtable
		j9thr
		${CMAKE_DL_LIBS}
)
if(OMR_OS_WINDOWS)
	target_link_libraries(j9prt
		PRIVATE
			Ws2_32.lib
			shell32.lib
			Iphlpapi.lib
			psapi.lib
			pdh.lib
	)
endif()

if(OMR_OS_LINUX)
	target_link_libraries(j9prt PRIVATE rt)
endif()

if(OMR_OS_OSX OR OMR_OS_AIX)
	target_link_libraries(j9prt PRIVATE iconv)
endif()

if(OMR_OS_ZOS)
	target_link_libraries(j9prt PRIVATE omrport_metalc)
endif()

target_compile_definitions(j9prt
	PRIVATE
		-DJ9PORT_LIBRARY_DEFINE
)

if(OMR_OS_WINDOWS)
	target_include_directories(j9prt PRIVATE win32_include)
else()
	if(OMR_OS_ZOS)
		target_include_directories(j9prt PRIVATE zos390)
	endif()
	target_include_directories(j9prt PRIVATE
		unix
		unix_include
	)
endif()

target_include_directories(j9prt
	PUBLIC
		include
		common
	PRIVATE
		$<TARGET_PROPERTY:omrport_obj,INCLUDE_DIRECTORIES>
		${CMAKE_CURRENT_BINARY_DIR}
		${CMAKE_CURRENT_SOURCE_DIR}
)

if(OMR_OS_LINUX)
	if((OMR_ARCH_RISCV) OR (OMR_ARCH_X86 AND OMR_ENV_DATA64))
		set_source_files_properties(unix/j9process.c
			COMPILE_FLAGS
			-Wno-unused-result
		)
	endif()
endif()

omr_add_exports(j9prt
	j9port_allocate_library
	j9port_create_library
	j9port_getSize
	j9port_getVersion
	j9port_init_library
	j9port_isCompatible
	j9port_startup_library
)

# Note: we need to pass EARLY_SOURCE_EVAL because the ddr logic chokes on
# $<TARGET_OBJECTS:omrport_obj> on older versions of cmake
target_enable_ddr(j9prt EARLY_SOURCE_EVAL GLOB_HEADERS)
ddr_set_add_targets(j9ddr j9prt)

install(
	TARGETS j9prt
	LIBRARY DESTINATION ${j9vm_SOURCE_DIR}
	RUNTIME DESTINATION ${j9vm_SOURCE_DIR}
)
