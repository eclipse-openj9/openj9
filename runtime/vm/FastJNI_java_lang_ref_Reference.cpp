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

#include "fastJNI.h"
#include "j9protos.h"
#include "j9consts.h"
#include "omrgcconsts.h"
#include "omr.h"

extern "C" {

/* java.lang.ref.Reference: private native T getImpl() */
j9object_t JNICALL
Fast_java_lang_ref_Reference_getImpl(J9VMThread *currentThread, j9object_t receiverObject)
{
	/* Only called from java for metronome GC policy */
	return currentThread->javaVM->memoryManagerFunctions->j9gc_objaccess_referenceGet(currentThread, receiverObject);
}

/* java.lang.ref.Reference: private native void reprocess() */
void JNICALL
Fast_java_lang_ref_Reference_reprocess(J9VMThread *currentThread, j9object_t receiverObject)
{
	J9JavaVM* javaVM = currentThread->javaVM;
	J9MemoryManagerFunctions* mmFuncs = javaVM->memoryManagerFunctions;
	if (J9_GC_POLICY_METRONOME == ((OMR_VM *)javaVM->omrVM)->gcPolicy) {
		/* Under metronome call getReferent, which will mark the referent if a GC is in progress. */
		mmFuncs->j9gc_objaccess_referenceGet(currentThread, receiverObject);
	} else {
		/* Reprocess this object if a concurrent GC is in progress */
		mmFuncs->J9WriteBarrierBatchStore(currentThread, receiverObject);
	}
}

J9_FAST_JNI_METHOD_TABLE(java_lang_ref_Reference)
	J9_FAST_JNI_METHOD("getImpl", "()Ljava/lang/Object;", Fast_java_lang_ref_Reference_getImpl,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_NOT_GC_POINT | J9_FAST_JNI_NO_NATIVE_METHOD_FRAME | J9_FAST_JNI_NO_EXCEPTION_THROW |
		J9_FAST_JNI_NO_SPECIAL_TEAR_DOWN | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS)
	J9_FAST_JNI_METHOD("reprocess", "()V", Fast_java_lang_ref_Reference_reprocess,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_NOT_GC_POINT | J9_FAST_JNI_NO_NATIVE_METHOD_FRAME | J9_FAST_JNI_NO_EXCEPTION_THROW |
		J9_FAST_JNI_NO_SPECIAL_TEAR_DOWN | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS)
J9_FAST_JNI_METHOD_TABLE_END
}
