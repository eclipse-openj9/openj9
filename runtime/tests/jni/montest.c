/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

#include "jnitest_internal.h"
#include "j9port.h"
#include "j9.h"

static jint lastReturnCode;

jint JNICALL
Java_j9vm_test_monitor_Helpers_getLastReturnCode(JNIEnv * env, jclass clazz)
{
	return lastReturnCode;
}

jint JNICALL
Java_j9vm_test_monitor_Helpers_monitorEnter(JNIEnv * env, jclass clazz, jobject obj)
{
	lastReturnCode = (*env)->MonitorEnter(env, obj);
	return lastReturnCode;
}

jint JNICALL 
Java_j9vm_test_monitor_Helpers_monitorExit(JNIEnv * env, jclass clazz, jobject obj)
{
	lastReturnCode = (*env)->MonitorExit(env, obj);
	return lastReturnCode;
}

jint JNICALL 
Java_j9vm_test_monitor_Helpers_monitorExitWithException(JNIEnv * env, jclass clazz, jobject obj, jobject throwable)
{
	(*env)->Throw(env, throwable);

	lastReturnCode = (*env)->MonitorExit(env, obj);

	return lastReturnCode;
}

void JNICALL 
Java_j9vm_test_monitor_Helpers_monitorReserve(JNIEnv * env, jclass clazz, jobject objRef)
{
#ifdef J9VM_THR_LOCK_RESERVATION

	J9VMThread* vmThread = (J9VMThread*)env;
	j9object_t obj;
	j9objectmonitor_t lock;
	j9objectmonitor_t* lockEA;
	jclass errorClazz;

	vmThread->javaVM->internalVMFunctions->internalEnterVMFromJNI(vmThread);

	obj = *(j9object_t*)objRef;
	lockEA = J9OBJECT_MONITOR_EA(vmThread, obj);

	if (lockEA == NULL) {
		vmThread->javaVM->internalVMFunctions->internalExitVMToJNI(vmThread);		
		errorClazz = (*env)->FindClass(env, "java/lang/Error");
		if (errorClazz != NULL) {
			(*env)->ThrowNew(env, errorClazz, "Object has no lock word");
		}
		return;
	}

	lock = *lockEA;

	if (lock != 0) {
		vmThread->javaVM->internalVMFunctions->internalExitVMToJNI(vmThread);		
		errorClazz = (*env)->FindClass(env, "java/lang/Error");
		if (errorClazz != NULL) {
			(*env)->ThrowNew(env, errorClazz, "Object's lock word is not 0");
		}
		return;
	}

	*lockEA = (j9objectmonitor_t)(UDATA)vmThread | OBJECT_HEADER_LOCK_RESERVED;

	vmThread->javaVM->internalVMFunctions->internalExitVMToJNI(vmThread);

#endif
}



