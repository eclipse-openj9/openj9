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
#include "VMHelpers.hpp"
#include "ObjectMonitor.hpp"

extern "C" {

/* java.lang.Object: public final native void wait(long millis, int nanos) throws InterruptedException; */
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
	omrthread_monitor_t monitorPtr = NULL;

	if (VM_ObjectMonitor::getMonitorForNotify(currentThread, receiverObject, &monitorPtr, true)) {
		if (0 != omrthread_monitor_notify_all(monitorPtr)) {
			setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALMONITORSTATEEXCEPTION, NULL);
		}
	} else if (NULL != monitorPtr) {
		setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALMONITORSTATEEXCEPTION, NULL);
	}
}

/* java.lang.Object: public final native void notify(); */
void JNICALL
Fast_java_lang_Object_notify(J9VMThread *currentThread, j9object_t receiverObject)
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

J9_FAST_JNI_METHOD_TABLE(java_lang_Object)
	J9_FAST_JNI_METHOD("wait", "(JI)V", Fast_java_lang_Object_wait,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS)
	J9_FAST_JNI_METHOD("notifyAll", "()V", Fast_java_lang_Object_notifyAll,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS)
	J9_FAST_JNI_METHOD("notify", "()V", Fast_java_lang_Object_notify,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS)
J9_FAST_JNI_METHOD_TABLE_END

}
