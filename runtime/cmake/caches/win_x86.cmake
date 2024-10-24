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

set(J9VM_ARCH_X86 ON CACHE BOOL "")
set(J9VM_ENV_DATA64 OFF CACHE BOOL "")
set(J9VM_ENV_LITTLE_ENDIAN ON CACHE BOOL "")
set(J9VM_ENV_SSE2_SUPPORT_DETECTION ON CACHE BOOL "")
set(J9VM_GC_ENABLE_SPARSE_HEAP_ALLOCATION OFF CACHE BOOL "")

set(J9VM_INTERP_ATOMIC_FREE_JNI ON CACHE BOOL "")
set(J9VM_INTERP_ATOMIC_FREE_JNI_USES_FLUSH ON CACHE BOOL "")
set(J9VM_INTERP_SIG_USR2 OFF CACHE BOOL "")
set(J9VM_INTERP_TWO_PASS_EXCLUSIVE ON CACHE BOOL "")
set(J9VM_INTERP_USE_UNSAFE_HELPER OFF CACHE BOOL "")
set(J9VM_MODULE_CODEGEN_IA32 ON CACHE BOOL "")
set(J9VM_MODULE_CODERT_IA32 ON CACHE BOOL "")
set(J9VM_MODULE_GDB OFF CACHE BOOL "")
set(J9VM_MODULE_GDB_PLUGIN OFF CACHE BOOL "")
set(J9VM_MODULE_JIT_IA32 ON CACHE BOOL "")
set(J9VM_MODULE_JITRT_IA32 ON CACHE BOOL "")
set(J9VM_MODULE_MASM2GAS ON CACHE BOOL "")
set(J9VM_MODULE_WINDBG ON CACHE BOOL "")
# Note: seems to only be used on arm
set(J9VM_GC_ALIGN_OBJECTS ON CACHE BOOL "")
set(J9VM_JIT_REQUIRES_TRAP_HANDLER ON CACHE BOOL "")
set(J9VM_OPT_NATIVE_CHARACTER_CONVERTER ON CACHE BOOL "")
set(J9VM_OPT_SWITCH_STACKS_FOR_SIGNAL_HANDLER ON CACHE BOOL "")
set(J9VM_OPT_ZERO ON CACHE BOOL "")
set(J9VM_THR_ASYNC_NAME_UPDATE OFF CACHE BOOL "")

set(OMR_ENV_DATA32 ON CACHE BOOL "")
set(OMR_ENV_DATA64 OFF CACHE BOOL "")

set(OMR_PORT_ALLOCATE_TOP_DOWN ON CACHE BOOL "")

# CMake forces a CMAKE_BUILD_TYPE when using msvc, which defaults to Debug
set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "")
include("${CMAKE_CURRENT_LIST_DIR}/common.cmake")
