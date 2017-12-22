/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
#if !defined(HY_VMLS_H)
#define HY_VMLS_H

#if defined(__cplusplus)
extern "C"
{
#endif

#include "j9comp.h"
#include "jni.h"

#define HY_VMLS_MAX_KEYS 256

typedef struct HyVMLSFunctionTable
{
	UDATA (JNICALL * HYVMLSAllocKeys) (JNIEnv * env, UDATA * pInitCount, ...);
	void (JNICALL * HYVMLSFreeKeys) (JNIEnv * env, UDATA * pInitCount, ...);
	void *(JNICALL * HyVMLSGet) (JNIEnv * env, void *key);
	void *(JNICALL * HyVMLSSet) (JNIEnv * env, void **pKey, void *value);
} HyVMLSFunctionTable;


#if defined(__cplusplus)
}
#endif

#endif /* HY_VMLS_H */
