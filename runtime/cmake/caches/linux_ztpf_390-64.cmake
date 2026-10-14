################################################################################
# Copyright IBM Corp. and others 2026
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
set(J9VM_ENV_DATA64 ON CACHE BOOL "")
set(J9VM_ENV_LITTLE_ENDIAN OFF CACHE BOOL "")
set(J9VM_GC_SPARSE_HEAP_ALLOCATION ON CACHE BOOL "")
set(J9VM_JIT_RUNTIME_INSTRUMENTATION ON CACHE BOOL "")
set(J9VM_MODULE_IFA OFF CACHE BOOL "")
set(J9VM_OPT_JAVA_OFFLOAD_SUPPORT OFF CACHE BOOL "")
set(J9VM_PORT_RUNTIME_INSTRUMENTATION ON CACHE BOOL "")

set(OMR_ARCH_S390 ON CACHE BOOL "")
set(OMR_ENV_DATA64 ON CACHE BOOL "")
set(OMR_GC ON CACHE BOOL "")
set(OMR_GC_CONCURRENT_SCAVENGER ON CACHE BOOL "")
set(OMR_GC_FULL_POINTERS ON CACHE BOOL "")
set(OMR_GC_IDLE_HEAP_MANAGER ON CACHE BOOL "")
set(OMR_GC_OBJECT_MAP OFF CACHE BOOL "")
set(OMR_GC_VLHGC ON CACHE BOOL "")
set(OMR_NEED_LIBRT OFF CACHE BOOL "")
set(OMR_NOTIFY_POLICY_CONTROL ON CACHE BOOL "")
set(OMR_OMRSIG ON CACHE BOOL "")
set(OMR_PORT ON CACHE BOOL "")
set(OMR_PORT_ASYNC_HANDLER ON CACHE BOOL "")
set(OMR_THREAD ON CACHE BOOL "")
set(OMR_THR_CUSTOM_SPIN_OPTIONS ON CACHE BOOL "")
set(OMR_THR_JLM_HOLD_TIMES ON CACHE BOOL "")
set(OMR_THR_LOCK_NURSERY ON CACHE BOOL "")
set(OMR_THR_SPIN_WAKE_CONTROL ON CACHE BOOL "")
set(OMR_THR_THREE_TIER_LOCKING ON CACHE BOOL "")
set(OMR_THR_YIELD_ALG ON CACHE BOOL "")
set(OMRPORT_OMRSIG_SUPPORT ON CACHE BOOL "")
set(OMRTHREAD_LIB_UNIX ON CACHE BOOL "")

# z/TPF specific options
set(ZTPF_PROGRAM_LETTER "J" CACHE STRING "z/TPF program letter")
set(ZTPF_REFRESH_VERSION "X.X.X.X" CACHE STRING "z/TPF refresh version")
set(ZTPF_STDCPP_LIB "stdcpp7" CACHE STRING "z/TPF C++ standard library")

# Note: In CMake, 'set's on cache variables only apply if the cache variable
# is not already set. Thus any cache variables set in this file, override
# anything set in common.cmake
include("${CMAKE_CURRENT_LIST_DIR}/common.cmake")
