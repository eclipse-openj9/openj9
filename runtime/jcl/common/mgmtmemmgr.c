/*******************************************************************************
 * Copyright (c) 1998, 2016 IBM Corp. and others
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

#include "jni.h"
#include "j9.h"
#include "mgmtinit.h"

static UDATA getIndexFromManagerID(J9JavaLangManagementData *mgmt, UDATA id);


jboolean JNICALL
Java_com_ibm_java_lang_management_internal_MemoryManagerMXBeanImpl_isManagedPoolImpl(JNIEnv *env, jclass beanInstance, jint id, jint poolID)
{
	J9JavaVM *javaVM = ((J9VMThread *) env)->javaVM;
	J9JavaLangManagementData *mgmt = javaVM->managementData;

	if (0 != (J9VM_MANAGEMENT_GC_HEAP & id)) {
		J9GarbageCollectorData *gc = &mgmt->garbageCollectors[getIndexFromManagerID(mgmt, id)];
		if (javaVM->memoryManagerFunctions->j9gc_is_managedpool_by_collector(javaVM, (UDATA)(gc->id & J9VM_MANAGEMENT_GC_HEAP_ID_MASK), (UDATA)(poolID & J9VM_MANAGEMENT_POOL_HEAP_ID_MASK))) {
			return JNI_TRUE;
		}
	}

	return JNI_FALSE;
}

static UDATA
getIndexFromManagerID(J9JavaLangManagementData *mgmt, UDATA id)
{
	UDATA idx = 0;

	for(; idx < mgmt->supportedCollectors; idx++) {
		if ((mgmt->garbageCollectors[idx].id & J9VM_MANAGEMENT_GC_HEAP_ID_MASK) == (id & J9VM_MANAGEMENT_GC_HEAP_ID_MASK)) {
			break;
		}
	}
	return idx;
}
