################################################################################
# Copyright IBM Corp. and others 1991
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

#TODO Platform hacks
set(J9VM_ARCH_X86 ON CACHE BOOL "")
set(J9VM_ENV_DATA64 ON CACHE BOOL "")
set(J9VM_ENV_HAS_FPU ON CACHE BOOL "")
set(J9VM_ENV_LITTLE_ENDIAN ON CACHE BOOL "")
set(J9VM_GC_ENABLE_SPARSE_HEAP_ALLOCATION ON CACHE BOOL "")

set(J9VM_INTERP_ATOMIC_FREE_JNI ON CACHE BOOL "")
set(J9VM_INTERP_ATOMIC_FREE_JNI_USES_FLUSH ON CACHE BOOL "")
set(J9VM_INTERP_TWO_PASS_EXCLUSIVE ON CACHE BOOL "")
set(J9VM_MODULE_CODEGEN_IA32 ON CACHE BOOL "")
set(J9VM_MODULE_CODERT_IA32 ON CACHE BOOL "")
set(J9VM_MODULE_JIT_IA32 ON CACHE BOOL "")
set(J9VM_MODULE_JITRT_IA32 ON CACHE BOOL "")
set(J9VM_MODULE_MASM2GAS ON CACHE BOOL "")
set(J9VM_OPT_SWITCH_STACKS_FOR_SIGNAL_HANDLER ON CACHE BOOL "")

set(OMR_GC_CONCURRENT_SCAVENGER ON CACHE BOOL "")
set(OMR_GC_IDLE_HEAP_MANAGER ON CACHE BOOL "")
set(OMR_GC_VLHGC_CONCURRENT_COPY_FORWARD ON CACHE BOOL "")
set(OMR_PORT_NUMA_SUPPORT ON CACHE BOOL "")

include("${CMAKE_CURRENT_LIST_DIR}/linux.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/common.cmake")
