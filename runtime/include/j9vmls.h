/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

/**
 * @file
 * @ingroup VMLS
 * @brief VM Local Storage Header
 */

#ifndef J9VMLS_H
#define J9VMLS_H

/* @ddr_namespace: default */
#ifdef __cplusplus
extern "C" {
#endif

#include "j9comp.h"
#include "j9cfg.h"
#include "jni.h"

#define J9VMLS_MAX_KEYS 256

/**
 * @struct J9VMLSFunctionTable
 * The VM local storage function table.
 */
typedef struct J9VMLSFunctionTable {
    UDATA  (JNICALL *J9VMLSAllocKeys)(JNIEnv * env, UDATA * pInitCount, ...) ;
    void  (JNICALL *J9VMLSFreeKeys)(JNIEnv * env, UDATA * pInitCount, ...) ;
    void*  (JNICALL *J9VMLSGet)(JNIEnv * env, void * key) ;
    void*  (JNICALL *J9VMLSSet)(JNIEnv * env, void ** pKey, void * value) ;
} J9VMLSFunctionTable;

/**
 * @fn J9VMLSFunctionTable::J9VMLSAllocKeys
 * Allocate one or more slots of VM local storage.
 *
 * @code UDATA  JNICALL J9VMLSAllocKeys(JNIEnv * env, UDATA * pInitCount, ...); @endcode
 *
 * @param[in] env  A JNIEnv pointer
 * @param[in] pInitCount  Pointer to the reference count for these slots
 * @param[out] ...  Locations to store the allocated keys
 *
 * @return 0 on success, 1 on failure.
 *
 * @note Newly allocated VMLS slots contain NULL in all VMs.
 */

/**
 * @fn J9VMLSFunctionTable::J9VMLSFreeKeys
 * Destroy one or more slots of VM local storage.
 *
 * @code void  JNICALL J9VMLSFreeKeys(JNIEnv * env, UDATA * pInitCount, ...); @endcode
 *
 * @param[in] env  A JNIEnv pointer
 * @param[in] pInitCount  Pointer to the reference count for these slots
 * @param[out] ...  Pointers to the allocated keys
 */

/**
 * @fn J9VMLSFunctionTable::J9VMLSGet
 * Retrieve the value in a VM local storage slot.
 *
 * @code void*  JNICALL J9VMLSGet(JNIEnv * env, void * key); @endcode
 *
 * @param[in] env  JNIEnv pointer
 * @param[in] key  The VMLS key
 *
 * @return The contents of the VM local storage slot in the VM that contains the specified env
 */

/**
 * @fn J9VMLSFunctionTable::J9VMLSSet
 * Store a value into a VM local storage slot.
 *
 * @code void*  JNICALL J9VMLSSet(JNIEnv * env, void ** pKey, void * value); @endcode
 *
 * @param[in] env  JNIEnv pointer
 * @param[in] pKey  Pointer to the VM local storage key
 * @param[in] value  Value to store
 *
 * @return The value stored
 */

#ifdef USING_VMI
#define J9VMLS_FNTBL(env) (*VMI_GetVMIFromJNIEnv(env))->GetVMLSFunctions(VMI_GetVMIFromJNIEnv(env))
#else
#define J9VMLS_FNTBL(env) ((J9VMLSFunctionTable *) ((((void ***) (env))[offsetof(J9VMThread,javaVM)/sizeof(UDATA)])[offsetof(J9JavaVM,vmLocalStorageFunctions)/sizeof(UDATA)]))
#endif

#ifdef J9VM_OPT_MULTI_VM
#define J9VMLS_GET(env, key) (J9VMLS_FNTBL(env)->J9VMLSGet(env, (key)))
#define J9VMLS_SET(env, key, value) (J9VMLS_FNTBL(env)->J9VMLSSet(env, &(key), (void *) (value)))
#else
#define J9VMLS_GET(env, key) (key)
#define J9VMLS_SET(env, key, value) ((key) = (void *) (value))
#endif

#ifdef __cplusplus
}
#endif

#endif /* J9VMLS_H */
