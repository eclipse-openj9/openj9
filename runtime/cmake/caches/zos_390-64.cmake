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

set(J9VM_ARCH_S390 ON CACHE BOOL "")
set(J9VM_ENV_CALL_VIA_TABLE ON CACHE BOOL "")
set(J9VM_ENV_DATA64 ON CACHE BOOL "")
set(J9VM_ENV_LITTLE_ENDIAN OFF CACHE BOOL "")
set(J9VM_GC_ENABLE_SPARSE_HEAP_ALLOCATION ON CACHE BOOL "")

# We need to modify the c/c++ compile rules to tweak the order of options for a2e.
# Namely we need to make sure that the nosearch flag appears before our include paths.
set(CMAKE_C_COMPILE_OBJECT "<CMAKE_C_COMPILER> <DEFINES> <FLAGS> <INCLUDES> -o <OBJECT> -c <SOURCE>" CACHE STRING "")
set(CMAKE_CXX_COMPILE_OBJECT "<CMAKE_CXX_COMPILER> <DEFINES> <FLAGS> <INCLUDES> -o <OBJECT> -c <SOURCE>" CACHE STRING "")

if(JAVA_SPEC_VERSION EQUAL 8)
	set(OMR_ZOS_COMPILE_TARGET "zOSV1R13" CACHE STRING "")
	set(OMR_ZOS_COMPILE_ARCHITECTURE 7 CACHE STRING "")
	set(OMR_ZOS_COMPILE_TUNE 10 CACHE STRING "")
else()
	set(OMR_ZOS_COMPILE_TARGET "zOSV2R3" CACHE STRING "")
	set(OMR_ZOS_COMPILE_ARCHITECTURE 10 CACHE STRING "")
	set(OMR_ZOS_COMPILE_TUNE 12 CACHE STRING "")
endif()
set(OMR_ZOS_LINK_COMPAT ${OMR_ZOS_COMPILE_TARGET} CACHE STRING "")

set(OMR_GC_CONCURRENT_SCAVENGER ON CACHE BOOL "")
set(OMR_GC_IDLE_HEAP_MANAGER ON CACHE BOOL "")
set(OMR_THR_YIELD_ALG OFF CACHE BOOL "")
set(OMR_THR_SPIN_WAKE_CONTROL OFF CACHE BOOL "")
set(OMR_USE_NATIVE_ENCODING OFF CACHE BOOL "")

set(J9VM_GC_ENABLE_DOUBLE_MAP OFF CACHE BOOL "")
set(J9VM_GC_SUBPOOLS_ALIAS ON CACHE BOOL "")
set(J9VM_GC_TLH_PREFETCH_FTA OFF CACHE BOOL "")
set(J9VM_INTERP_ATOMIC_FREE_JNI ON CACHE BOOL "")
set(J9VM_JIT_FREE_SYSTEM_STACK_POINTER ON CACHE BOOL "")
set(J9VM_JIT_RUNTIME_INSTRUMENTATION ON CACHE BOOL "")
set(J9VM_JIT_TRANSACTION_DIAGNOSTIC_THREAD_BLOCK ON CACHE BOOL "")
set(J9VM_PORT_RUNTIME_INSTRUMENTATION ON CACHE BOOL "")
set(J9VM_THR_ASYNC_NAME_UPDATE OFF CACHE BOOL "")

set(J9VM_ZOS_3164_INTEROPERABILITY ON CACHE BOOL "")
set(J9VM_OPT_SHR_MSYNC_SUPPORT ON CACHE BOOL "")

include("${CMAKE_CURRENT_LIST_DIR}/common.cmake")
