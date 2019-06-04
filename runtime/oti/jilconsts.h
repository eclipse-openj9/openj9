/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef JILCONSTS_H
#define JILCONSTS_H

#include "j9.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "j9.h"

#define J9JIT_GROW_CACHES  0x100000
#define J9JIT_CG_OPT_LEVEL_HIGH  16
#define J9JIT_PATCHING_FENCE_REQUIRED  0x4000000
#define J9JIT_CG_OPT_LEVEL_BEST_AVAILABLE  8
#define J9JIT_CG_OPT_LEVEL_NONE  2
#define J9JIT_CG_REGISTER_MAPS  32
#define J9JIT_JIT_ATTACHED  0x800000
#define J9JIT_AOT_ATTACHED  0x1000000
#define J9JIT_SCAVENGE_ON_RESOLVE  0x4000
#define J9JIT_CG_BREAK_ON_ENTRY  1
#define J9JIT_JVMPI_INLINE_METHOD_OFF  16
#define J9JIT_SCAVENGE_ON_RUNTIME  0x200000
#define J9JIT_JVMPI_GEN_INLINE_ENTRY_EXIT  4
#define J9JIT_JVMPI_DISABLE_DIRECT_TO_JNI  64
#define J9JIT_TESTMODE  0x1000
#define J9JIT_JVMPI_DISABLE_DIRECT_RECLAIM  0x100
#define J9JIT_GC_NOTIFY  0x40000
#define J9JIT_ASSUME_STRICTFP  64
#define J9JIT_JVMPI_GEN_COMPILED_ENTRY_EXIT  1
#define J9JIT_JVMPI_GEN_NATIVE_ENTRY_EXIT  8
#define J9JIT_TOSS_CODE  0x8000
#define J9JIT_DATA_CACHE_FULL  0x20000000
#define J9JIT_DEFER_JIT  0x2000000
#define J9JIT_CG_OPT_LEVEL_LOW  4
#define J9JIT_REPORT_EVENTS  0x20000
#define J9JIT_JVMPI_DISABLE_DIRECT_INLINING_NATIVES  0x80
#define J9JIT_ASSUME_STRICTFPCOMPARES  0x80
#define J9JIT_JVMPI_JIT_DEFAULTS  39
#define J9JIT_JVMPI_GEN_BUILTIN_ENTRY_EXIT  2
#define J9JIT_AOT  0x2000
#define J9JIT_PATCHING_FENCE_TYPE  0x8000000
#define J9JIT_RUNTIME_RESOLVE  0x80000
#define J9JIT_CODE_CACHE_FULL  0x40000000
#define J9JIT_COMPILE_CLINIT  0x400000
#define J9JIT_JVMPI_INLINE_ALLOCATION_OFF  32

#define J9JIT_JIT_VTABLE_OFFSET 0x0
#define J9JIT_INTERP_VTABLE_OFFSET sizeof(J9Class)
#define J9JIT_VMTHREAD_CURRENTEXCEPTION_OFFSET offsetof(J9VMThread, currentException)

#define J9JIT_RESOLVE_FAIL_COMPILE -2
#define J9JIT_NEVER_TRANSLATE_COUNT_VALUE -3

#define J9JIT_REVERT_METHOD_TO_INTERPRETED(javaVM, method) \
	(javaVM)->internalVMFunctions->initializeMethodRunAddressNoHook((javaVM), (method))

#if defined(J9ZOS390) && !defined(J9VM_ENV_DATA64)
/* CAA is r12 - r4-r15 are saved in the C stack frame (must also add the 2048 stack bias) */
#define J9TR_CAA_SAVE_OFFSET ((UDATA)&(((J9CInterpreterStackFrame*)2048)->preservedGPRs[8]))
#endif /* J9ZOS390 && !J9VM_ENV_DATA64 */

#ifdef __cplusplus
}
#endif

#endif /* JILCONSTS_H */
