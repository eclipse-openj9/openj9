/*******************************************************************************
 * Copyright IBM Corp. and others 2001
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include "fastJNI.h"

#include "j9consts.h"
#include "j9protos.h"
#include "ObjectMonitor.hpp"
#include "ut_j9vm.h"
#include "VMHelpers.hpp"
#if defined(J9VM_OPT_CRIU_SUPPORT)
#include "CRIUHelpers.hpp"
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */

extern "C" {

/* java.lang.Object: public final native void waitImpl(long millis, int nanos) throws InterruptedException; */
void JNICALL
Fast_java_lang_Object_wait(J9VMThread *currentThread, j9object_t receiverObject, jlong millis, jint nanos)
{
	if (0 == monitorWaitImpl(currentThread, receiverObject, (I_64)millis, (I_32)nanos, TRUE)) {
		/* No need to check return value here - pop frames cannot occur as this method is native */
		VM_VMHelpers::checkAsyncMessages(currentThread);
	}
}

/* java.lang.Object: public final native void notifyAll(); */
void JNICALL
Fast_java_lang_Object_notifyAll(J9VMThread *currentThread, j9object_t receiverObject)
{
#if defined(J9VM_OPT_CRIU_SUPPORT)
	if (VM_CRIUHelpers::isJVMInSingleThreadMode(currentThread->javaVM)) {
		/* The exception is already set if the operation failed. */
		VM_CRIUHelpers::delayedLockingOperation(currentThread, receiverObject, J9_SINGLE_THREAD_MODE_OP_NOTIFY_ALL);
	} else
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */
	{
		omrthread_monitor_t monitorPtr = NULL;

		if (VM_ObjectMonitor::getMonitorForNotify(currentThread, receiverObject, &monitorPtr, true)) {
			if (0 != omrthread_monitor_notify_all(monitorPtr)) {
				setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALMONITORSTATEEXCEPTION, NULL);
			}
		} else if (NULL != monitorPtr) {
			setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALMONITORSTATEEXCEPTION, NULL);
		}
	}
}

/* java.lang.Object: public final native void notify(); */
void JNICALL
Fast_java_lang_Object_notify(J9VMThread *currentThread, j9object_t receiverObject)
{
#if defined(J9VM_OPT_CRIU_SUPPORT)
	if (VM_CRIUHelpers::isJVMInSingleThreadMode(currentThread->javaVM)) {
		/* The exception is already set if the operation failed. */
		VM_CRIUHelpers::delayedLockingOperation(currentThread, receiverObject, J9_SINGLE_THREAD_MODE_OP_NOTIFY);
	} else
#endif /* defined(J9VM_OPT_CRIU_SUPPORT) */
	{
		omrthread_monitor_t monitorPtr = NULL;

		if (VM_ObjectMonitor::getMonitorForNotify(currentThread, receiverObject, &monitorPtr, true)) {
			if (0 != omrthread_monitor_notify(monitorPtr)) {
				setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALMONITORSTATEEXCEPTION, NULL);
			}
		} else if (NULL != monitorPtr) {
			setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALMONITORSTATEEXCEPTION, NULL);
		}
	}
}

J9_FAST_JNI_METHOD_TABLE(java_lang_Object)
	J9_FAST_JNI_METHOD("waitImpl", "(JI)V", Fast_java_lang_Object_wait,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS)
	J9_FAST_JNI_METHOD("notifyAll", "()V", Fast_java_lang_Object_notifyAll,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS)
	J9_FAST_JNI_METHOD("notify", "()V", Fast_java_lang_Object_notify,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS)
J9_FAST_JNI_METHOD_TABLE_END

}
