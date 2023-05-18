/*******************************************************************************
 * Copyright IBM Corp. and others 2019
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
#if !defined(CRIUHELPERS_HPP_)
#define CRIUHELPERS_HPP_

#include "j9.h"
#include "omrlinkedlist.h"

class VM_CRIUHelpers {
	/*
	 * Data members
	 */
private:

protected:

public:

	/*
	 * Function members
	 */
private:

protected:

public:
	/*
	 * Queries whether the JVM is running in single thread mode
	 *
	 * @param[in] vm Java VM token
	 * @return True if JVM is in single thread mode, false otherwise
	 */
	static VMINLINE bool
	isJVMInSingleThreadMode(J9JavaVM *vm)
	{
		return J9_IS_SINGLE_THREAD_MODE(vm);
	}


	static VMINLINE bool
	delayedLockingOperation(J9VMThread *currentThread, j9object_t instance, UDATA operation)
	{
		bool rc = true;
		J9JavaVM *vm = currentThread->javaVM;
		J9InternalVMFunctions* vmFuncs = vm->internalVMFunctions;

		/* This is only contended when the GC is shutting down threads. */
		omrthread_monitor_enter(vm->delayedLockingOperationsMutex);

		jobject globalRef = vmFuncs->j9jni_createGlobalRef((JNIEnv*) currentThread, instance, JNI_FALSE);
		if (NULL == globalRef) {
			goto throwOOM;
		}

		{
			J9DelayedLockingOpertionsRecord *newRecord = static_cast<J9DelayedLockingOpertionsRecord*>(pool_newElement(vm->checkpointState.delayedLockingOperationsRecords));
			if (NULL == newRecord) {
				goto throwOOM;
			}

			newRecord->globalObjectRef = globalRef;
			newRecord->operation = operation;

			if (J9_LINKED_LIST_IS_EMPTY(vm->checkpointState.delayedLockingOperationsRoot)) {
				J9_LINKED_LIST_ADD_FIRST(vm->checkpointState.delayedLockingOperationsRoot, newRecord);
			} else {
				J9_LINKED_LIST_ADD_LAST(vm->checkpointState.delayedLockingOperationsRoot, newRecord);
			}
		}

		Trc_VM_criu_delayedLockingOperation_delayOperation(currentThread, operation, instance);
done:
		omrthread_monitor_exit(vm->delayedLockingOperationsMutex);
		return rc;

throwOOM:
		rc = false;
		vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
		goto done;
	}

	static VMINLINE void
	addInternalJVMClassIterationRestoreHook(J9VMThread *currentThread, classIterationRestoreHookFunc hookFunc)
	{
		J9JavaVM *vm = currentThread->javaVM;
		J9InternalClassIterationRestoreHookRecord *newHook = (J9InternalClassIterationRestoreHookRecord*)pool_newElement(vm->checkpointState.classIterationRestoreHookRecords);
		if (NULL == newHook) {
			setNativeOutOfMemoryError(currentThread, 0, 0);
		} else {
			newHook->hookFunc = hookFunc;
		}
	}
};

#endif /* CRIUHELPERS_HPP_ */
