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

#ifndef J9GENERATED_H
#define J9GENERATED_H

#ifdef __cplusplus
extern "C" {
#endif

#include "j9port.h" /* for definition of struct J9RIParameters */
#include "jithook_internal.h" /* for definition of struct J9JITHookInterface */

#define J9_LOWEST_STACK_SLOT(vmThread) ((UDATA *) ((vmThread)->stackObject + 1))

#define J9_JNI_UNWRAP_REFERENCE(o)  (*(j9object_t*)(o))

typedef struct J9SidecarExitFunction {
	struct J9SidecarExitFunction * next;
	void (*func)(void);
} J9SidecarExitFunction;

struct J9JavaVM;
typedef void (*J9SidecarExitHook)(struct J9JavaVM *);

struct J9VMThread;
typedef void (*J9AsyncEventHandler)(struct J9VMThread * currentThread, IDATA handlerKey, void * userData);

#define J9STACKSLOT UDATA

#define J9_FSD_ENABLED(vm) ((vm)->jitConfig && (vm)->jitConfig->fsdEnabled)

#if defined(J9VM_ARCH_X86)
#define J9VM_NEEDS_JNI_REDIRECTION
#define J9JNIREDIRECT_REQUIRED_ALIGNMENT 2
#endif /* J9VM_ARCH_X86 */

#if defined(J9ZOS390) && !defined(J9VM_ENV_DATA64)
#define J9_COMPATIBLE_FUNCTION_POINTER(fp)  helperCompatibleFunctionPointer( (void *)(fp) )
#else
#define J9_COMPATIBLE_FUNCTION_POINTER(fp)  fp
#endif

#if defined(J9VM_INTERP_NEW_HEADER_SHAPE)
#define OBJECT_HEADER_INDEXABLE OBJECT_HEADER_INDEXABLE_NHS
#else
#define OBJECT_HEADER_INDEXABLE OBJECT_HEADER_INDEXABLE_STANDARD
#endif

#include "j9nonbuilder.h"

#ifdef __cplusplus
}
#endif

#endif /* J9GENERATED_H */
