/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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
#include "VMHelpers.hpp"
#include "j9vmnls.h"
#include "AtomicSupport.hpp"

extern "C" {

/* java.lang.Thread: public static native void sleep(long millis, int nanos) throws InterruptedException; */
void JNICALL
Fast_java_lang_Thread_sleep(J9VMThread *currentThread, jlong millis, jint nanos)
{
	if (0 == threadSleepImpl(currentThread, (I_64)millis, (I_32)nanos)) {
		/* No need to check return value here - pop frames cannot occur as this method is native */
		VM_VMHelpers::checkAsyncMessages(currentThread);
	}
}

/* java.lang.Thread: public static native Thread currentThread(); */
j9object_t JNICALL
Fast_java_lang_Thread_currentThread(J9VMThread *currentThread)
{
	return currentThread->threadObject;
}


/* java.lang.Thread: public static native boolean interrupted(); */
jboolean JNICALL
Fast_java_lang_Thread_interrupted(J9VMThread *currentThread)
{
	jboolean rc = JNI_FALSE;
#if defined(WIN32)
	/* Synchronize on the thread lock around interrupted() on windows */
	j9object_t threadLock = J9VMJAVALANGTHREAD_LOCK(currentThread, currentThread->threadObject);
	threadLock = (j9object_t)objectMonitorEnter(currentThread, threadLock);
	if (NULL == threadLock) {
		setNativeOutOfMemoryError(currentThread, J9NLS_VM_FAILED_TO_ALLOCATE_MONITOR);
		goto done;
	}
	/* This field is only initialized on windows */
	J9JavaVM * const vm = currentThread->javaVM;
	if (NULL != vm->sidecarClearInterruptFunction) {
		vm->sidecarClearInterruptFunction(currentThread);
	}
#endif /* WIN32 */
	if (0 != omrthread_clear_interrupted()) {
		rc = JNI_TRUE;
	}
#if defined(WIN32)
	objectMonitorExit(currentThread, threadLock);
done:
#endif /* WIN32 */
	return rc;
}

/* java.lang.Thread: public native boolean isInterruptedImpl(); */
jboolean JNICALL
Fast_java_lang_Thread_isInterruptedImpl(J9VMThread *currentThread, j9object_t receiverObject)
{
	return VM_VMHelpers::threadIsInterruptedImpl(currentThread, receiverObject) ? JNI_TRUE : JNI_FALSE;
}

/* java.lang.Thread: public static native void onSpinWait(); */
void JNICALL
Fast_java_lang_Thread_onSpinWait(J9VMThread *currentThread)
{
	VM_AtomicSupport::yieldCPU();
}

#if defined(WIN32)
/* Exception may be thrown from monitor enter on windows */
#define INTERRUPTED_FLAGS J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER
#else /* WIN32 */
#define INTERRUPTED_FLAGS J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER | \
							J9_FAST_JNI_NOT_GC_POINT | J9_FAST_JNI_NO_NATIVE_METHOD_FRAME | J9_FAST_JNI_NO_EXCEPTION_THROW | J9_FAST_JNI_NO_SPECIAL_TEAR_DOWN
#endif /* WIN32 */

J9_FAST_JNI_METHOD_TABLE(java_lang_Thread)
	J9_FAST_JNI_METHOD("sleep", "(JI)V", Fast_java_lang_Thread_sleep,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
	J9_FAST_JNI_METHOD("currentThread", "()Ljava/lang/Thread;", Fast_java_lang_Thread_currentThread,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_NOT_GC_POINT | J9_FAST_JNI_NO_NATIVE_METHOD_FRAME | J9_FAST_JNI_NO_EXCEPTION_THROW |
		J9_FAST_JNI_NO_SPECIAL_TEAR_DOWN | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
	J9_FAST_JNI_METHOD("interrupted", "()Z", Fast_java_lang_Thread_interrupted,
		INTERRUPTED_FLAGS)
	J9_FAST_JNI_METHOD("isInterruptedImpl", "()Z", Fast_java_lang_Thread_isInterruptedImpl,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_NOT_GC_POINT | J9_FAST_JNI_NO_NATIVE_METHOD_FRAME | J9_FAST_JNI_NO_EXCEPTION_THROW |
		J9_FAST_JNI_NO_SPECIAL_TEAR_DOWN | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS)
	J9_FAST_JNI_METHOD("onSpinWait", "()V", Fast_java_lang_Thread_onSpinWait,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_NOT_GC_POINT | J9_FAST_JNI_NO_NATIVE_METHOD_FRAME | J9_FAST_JNI_NO_EXCEPTION_THROW |
		J9_FAST_JNI_NO_SPECIAL_TEAR_DOWN | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
J9_FAST_JNI_METHOD_TABLE_END

}
