################################################################################
# Copyright (c) 1991, 2019 IBM Corp. and others
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

#TODO Platform hacks
set(J9VM_ARCH_X86 ON CACHE BOOL "")
set(J9VM_ENV_DATA64 ON CACHE BOOL "")
set(J9VM_ENV_HAS_FPU ON CACHE BOOL "")
set(J9VM_ENV_LITTLE_ENDIAN ON CACHE BOOL "")

set(OMR_GC_IDLE_HEAP_MANAGER ON CACHE BOOL "")
set(OMR_GC_TLH_PREFETCH_FTA ON CACHE BOOL "")
set(J9VM_PORT_RUNTIME_INSTRUMENTATION OFF CACHE BOOL "")
set(J9VM_MODULE_CODEGEN_IA32 ON CACHE BOOL "")
set(J9VM_MODULE_CODERT_IA32 ON CACHE BOOL "")
set(J9VM_MODULE_JIT_IA32 ON CACHE BOOL "")
set(J9VM_MODULE_JITRT_IA32 ON CACHE BOOL "")
set(J9VM_MODULE_MASM2GAS ON CACHE BOOL "")
set(J9VM_GC_IDLE_HEAP_MANAGER ON CACHE BOOL "")
set(J9VM_OPT_SWITCH_STACKS_FOR_SIGNAL_HANDLER ON CACHE BOOL "")

include("${CMAKE_CURRENT_LIST_DIR}/common.cmake")
