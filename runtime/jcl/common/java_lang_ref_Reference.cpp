/*******************************************************************************
 * Copyright (c) 1998, 2018 IBM Corp. and others
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
#include "jcl.h"
#include "jclglob.h"
#include "jclprots.h"
#include "jcl_internal.h"

extern "C" {

/* java.lang.ref.Reference: private native void reprocess(); */
void JNICALL
Java_java_lang_ref_Reference_reprocess(JNIEnv *env, jobject recv)
{
	J9VMThread* currentThread = (J9VMThread*)env;
	J9JavaVM* vm = currentThread->javaVM;
	J9InternalVMFunctions* vmFuncs = vm->internalVMFunctions;
	J9MemoryManagerFunctions* mmFuncs = vm->memoryManagerFunctions;
	vmFuncs->internalEnterVMFromJNI(currentThread);
	j9object_t receiverObject = J9_JNI_UNWRAP_REFERENCE(recv);
	if (J9_GC_POLICY_METRONOME == vm->gcPolicy) {
		/* Under metronome call getReferent, which will mark the referent if a GC is in progress. */
		mmFuncs->j9gc_objaccess_referenceGet(currentThread, receiverObject);
	} else {
		/* Reprocess this object if a concurrent GC is in progress */
		mmFuncs->J9WriteBarrierBatchStore(currentThread, receiverObject);
	}
	vmFuncs->internalExitVMToJNI(currentThread);
}

/* java.lang.ref.Reference: static private native boolean waitForReferenceProcessingImpl(); */
jboolean JNICALL
Java_java_lang_ref_Reference_waitForReferenceProcessingImpl(JNIEnv *env, jclass recv)
{
	jboolean result = JNI_FALSE;
#if defined(J9VM_GC_FINALIZATION)
	J9JavaVM *vm = ((J9VMThread*)env)->javaVM;
	J9MemoryManagerFunctions *mmFuncs = vm->memoryManagerFunctions;
	if (0 != mmFuncs->j9gc_wait_for_reference_processing(vm)) {
		result = JNI_TRUE;
	}
#endif
	return result;
}

}
