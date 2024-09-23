################################################################################
# Copyright IBM Corp. and others 2019
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

#TODO: Env vars should be auto detected by platform
# Prevent CMake from automatically creating export lists for shared libraries
set(CMAKE_XL_CreateExportList "" CACHE INTERNAL "")

set(J9VM_ARCH_POWER ON CACHE BOOL "")
set(J9VM_ENV_DATA64 ON CACHE BOOL "")
set(J9VM_ENV_DLPAR ON CACHE BOOL "")
set(J9VM_ENV_HAS_FPU ON CACHE BOOL "")
set(J9VM_ENV_SHARED_LIBS_CALLEE_GLOBAL_TABLE_SETUP OFF CACHE BOOL "")
set(J9VM_ENV_SHARED_LIBS_USE_GLOBAL_TABLE ON CACHE BOOL "")

set(J9VM_GC_ENABLE_SPARSE_HEAP_ALLOCATION ON CACHE BOOL "")
set(J9VM_GC_SUBPOOLS_ALIAS ON CACHE BOOL "")
set(J9VM_GC_TLH_PREFETCH_FTA OFF CACHE BOOL "")
set(J9VM_INTERP_ATOMIC_FREE_JNI ON CACHE BOOL "")
set(J9VM_INTERP_ATOMIC_FREE_JNI_USES_FLUSH ON CACHE BOOL "")
set(J9VM_INTERP_TWO_PASS_EXCLUSIVE ON CACHE BOOL "")
set(J9VM_JIT_RUNTIME_INSTRUMENTATION ON CACHE BOOL "")
set(J9VM_MODULE_CODEGEN_PPC ON CACHE BOOL "")
set(J9VM_OPT_SWITCH_STACKS_FOR_SIGNAL_HANDLER OFF CACHE BOOL "")
set(J9VM_PORT_RUNTIME_INSTRUMENTATION ON CACHE BOOL "")
set(J9VM_THR_ASYNC_NAME_UPDATE OFF CACHE BOOL "")


set(OMR_GC_CONCURRENT_SCAVENGER ON CACHE BOOL "")
set(OMR_GC_HEAP_CARD_TABLE ON CACHE BOOL "")
set(OMR_THR_SPIN_WAKE_CONTROL OFF CACHE BOOL "")
set(OMR_THR_THREE_TIER_LOCKING OFF CACHE BOOL "")

# Note: In CMake, 'set's on cache variables only apply if the cache variable
# is not already set. Thus any cache varaibles set in this file, override
# anything set in common.cmake

include("${CMAKE_CURRENT_LIST_DIR}/common.cmake")
