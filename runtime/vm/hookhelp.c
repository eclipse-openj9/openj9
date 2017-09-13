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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "j9.h"
#include "j9protos.h"
#include "j9consts.h"
#include "vmhook_internal.h"
#include "objhelp.h"
#include "ut_j9vm.h"

#include "ute.h"

void  
triggerClassLoadEvent(J9VMThread* currentThread, J9Class* clazz)
{
	ALWAYS_TRIGGER_J9HOOK_VM_CLASS_LOAD(currentThread->javaVM->hookInterface, currentThread, clazz);
}


j9object_t  
triggerExceptionThrowEvent(J9VMThread* currentThread, j9object_t exception)
{
	ALWAYS_TRIGGER_J9HOOK_VM_EXCEPTION_THROW(currentThread->javaVM->hookInterface, currentThread, exception);

	return exception;
}


j9object_t  
triggerExceptionCatchEvent(J9VMThread* currentThread, j9object_t exception, UDATA* popProtectedFrame)
{
	ALWAYS_TRIGGER_J9HOOK_VM_EXCEPTION_CATCH(currentThread->javaVM->hookInterface, currentThread, exception, popProtectedFrame);

	return exception;
}


void  
triggerMonitorContendedEnterEvent(J9VMThread* currentThread, omrthread_monitor_t monitor)
{
	ALWAYS_TRIGGER_J9HOOK_VM_MONITOR_CONTENDED_ENTER(currentThread->javaVM->hookInterface, currentThread, monitor);
}


void  
triggerMonitorContendedEnteredEvent(J9VMThread* currentThread, omrthread_monitor_t monitor)
{
	ALWAYS_TRIGGER_J9HOOK_VM_MONITOR_CONTENDED_ENTERED(currentThread->javaVM->hookInterface, currentThread, monitor);
}


void  
triggerExceptionDescribeEvent(J9VMThread* currentThread)
{
	j9object_t exception = currentThread->currentException;

	currentThread->currentException = NULL;
	ALWAYS_TRIGGER_J9HOOK_VM_EXCEPTION_DESCRIBE(currentThread->javaVM->hookInterface, currentThread, exception);
	currentThread->currentException = exception;
}


void  
triggerMethodEnterEvent(J9VMThread* currentThread, J9Method* method, void* arg0EA, UDATA methodType, UDATA tracing)
{
	if (tracing) {
		UTSI_TRACEMETHODENTER_FROMVM(currentThread->javaVM, currentThread, method, arg0EA, methodType);
	}
	TRIGGER_J9HOOK_VM_METHOD_ENTER(currentThread->javaVM->hookInterface, currentThread, method, arg0EA, methodType);
}


void  
triggerMethodReturnEvent(J9VMThread* currentThread, J9Method* method, UDATA poppedByException, void* returnValuePtr, UDATA methodType, UDATA tracing)
{
	if (tracing) {
		/* All the smalltalk callers of triggerMethodReturnEvent set poppedByException to FALSE so exceptionPtr can be NULL */
		Assert_VM_false(poppedByException);
		UTSI_TRACEMETHODEXIT_FROMVM(currentThread->javaVM, currentThread, method, NULL, returnValuePtr, methodType);
	}
	TRIGGER_J9HOOK_VM_METHOD_RETURN(currentThread->javaVM->hookInterface, currentThread, method, poppedByException, returnValuePtr, methodType);
}


void  
triggerNativeMethodEnterEvent(J9VMThread* currentThread, J9Method* method, void* arg0EA, UDATA tracing)
{
	if (tracing) {
		UTSI_TRACEMETHODENTER_FROMVM(currentThread->javaVM, currentThread, method, arg0EA, 0);
	}
	TRIGGER_J9HOOK_VM_NATIVE_METHOD_ENTER(currentThread->javaVM->hookInterface, currentThread, method, arg0EA);
}


void  
triggerNativeMethodReturnEvent(J9VMThread* currentThread, J9Method* method, UDATA returnType, UDATA tracing)
{
	UDATA returnValue[2];
	void * returnValuePtr = returnValue;
	jobject returnRef = (jobject) currentThread->returnValue2;	/* The JNI send target put the returned ref here in the object case */

	returnValue[0] = currentThread->returnValue;
	returnValue[1] = currentThread->returnValue2;
	if (returnType == J9NtcObject) {
		PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, (j9object_t) returnValue[0]);
		returnValuePtr = currentThread->sp;
	}

	/* Currently not called when an exception is pending, so always use NULL for exceptionPtr */
	if (tracing) {
		UTSI_TRACEMETHODEXIT_FROMVM(currentThread->javaVM, currentThread, method, NULL, returnValuePtr, 0);
	}
	TRIGGER_J9HOOK_VM_NATIVE_METHOD_RETURN(currentThread->javaVM->hookInterface, currentThread, method, FALSE, returnValuePtr, returnRef);

	if (returnType == J9NtcObject) {
		returnValue[0] = (UDATA) POP_OBJECT_IN_SPECIAL_FRAME(currentThread);
	}
	currentThread->returnValue = returnValue[0];
	currentThread->returnValue2 = returnValue[1];
}
