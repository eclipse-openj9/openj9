/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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

#include "j9protos.h"
#include "jvminit.h"
#include "omrthread.h"
#include "j9consts.h"
#include "ut_j9vm.h"
#include "vm_api.h"
#include "vmhook_internal.h"

static UDATA vmRuntimeStateListenerProc(J9PortLibrary* portLib, void *userData);
static int J9THREAD_PROC vmRuntimeStateListenerProcWrapper(void *entryarg);

/**
 * Main function executed by VM runtime state listener thread
 *
 * @param portLib
 * @param userData pointer to J9VMRuntimeStateListener
 *
 * @return 0
 */
static UDATA
vmRuntimeStateListenerProc(J9PortLibrary* portLib, void *userData)
{
	J9JavaVM *vm = (J9JavaVM *)userData;
	J9VMRuntimeStateListener *listener = &vm->vmRuntimeStateListener;
	J9VMThread *vmThread = listener->runtimeStateListenerVMThread;
	IDATA rc = 0;
	U_32 currentState = getVMRuntimeState(vm);

	omrthread_monitor_enter(listener->runtimeStateListenerMutex);
	listener->runtimeStateListenerState = J9VM_RUNTIME_STATE_LISTENER_STARTED;
	/* notify parent thread that we have started successfully */
	omrthread_monitor_notify(listener->runtimeStateListenerMutex);

	while (J9VM_RUNTIME_STATE_LISTENER_STOP != listener->runtimeStateListenerState) {
		do {
			rc = omrthread_monitor_wait(listener->runtimeStateListenerMutex);
		} while ((0 != rc) || ((J9VM_RUNTIME_STATE_LISTENER_STOP != listener->runtimeStateListenerState) && (currentState == getVMRuntimeState(vm))));

		if (J9VM_RUNTIME_STATE_LISTENER_STOP != listener->runtimeStateListenerState) {
			currentState = getVMRuntimeState(vm);
			omrthread_monitor_exit(listener->runtimeStateListenerMutex);
			TRIGGER_J9HOOK_VM_RUNTIME_STATE_CHANGED(vm->hookInterface, vmThread, currentState);
			omrthread_monitor_enter(listener->runtimeStateListenerMutex);
		}
	}

	DetachCurrentThread((JavaVM *)vm);
	listener->runtimeStateListenerState = J9VM_RUNTIME_STATE_LISTENER_TERMINATED;
	omrthread_monitor_notify(listener->runtimeStateListenerMutex);
	omrthread_exit(listener->runtimeStateListenerMutex);

	/* NO GUARANTEED EXECUTION BEYOND THIS POINT */

	return 0;
}

/**
 * Wrapper function around vmRuntimeStateListenerProc() to protect from synchronous signals
 *
 * @param entryarg pointer to J9VMRuntimeStateListener
 *
 * @return JNI_OK on success, JNI_ERR on failure
 */
static int J9THREAD_PROC
vmRuntimeStateListenerProcWrapper(void *entryarg)
{
	J9JavaVM *vm = (J9JavaVM *)entryarg;
	J9VMRuntimeStateListener *listener = &vm->vmRuntimeStateListener;
	J9JavaVMAttachArgs attachArgs = { 0 };
	int result = JNI_ERR;
	PORT_ACCESS_FROM_JAVAVM(vm);

	attachArgs.version = JNI_VERSION_1_8;
	attachArgs.name = "VM Runtime State Listener";
	attachArgs.group = vm->systemThreadGroupRef;
	result = (int)internalAttachCurrentThread(vm, &listener->runtimeStateListenerVMThread, &attachArgs,
                                  J9_PRIVATE_FLAGS_DAEMON_THREAD | J9_PRIVATE_FLAGS_SYSTEM_THREAD | J9_PRIVATE_FLAGS_ATTACHED_THREAD,
                                  omrthread_self());

	if (JNI_OK == result) {
		UDATA rc = 0;
		j9sig_protect(vmRuntimeStateListenerProc,
			vm,
			structuredSignalHandler,
			listener->runtimeStateListenerVMThread,
			J9PORT_SIG_FLAG_SIGALLSYNC | J9PORT_SIG_FLAG_MAY_CONTINUE_EXECUTION,
			&rc);
	} else {
		omrthread_monitor_enter(listener->runtimeStateListenerMutex);
		listener->runtimeStateListenerState = J9VM_RUNTIME_STATE_LISTENER_ABORT;
		omrthread_monitor_notify(listener->runtimeStateListenerMutex);
		omrthread_monitor_exit(listener->runtimeStateListenerMutex);
	}
	return result;
}

I_32
startVMRuntimeStateListener(J9JavaVM* vm)
{
	I_32 rc = -1;
	J9VMRuntimeStateListener *listener = &vm->vmRuntimeStateListener;

	omrthread_monitor_enter(listener->runtimeStateListenerMutex);

	/* Create this thread as a RESOURCE_MONITOR_THREAD */
	rc = (I_32) createThreadWithCategory(
				NULL,
				vm->defaultOSStackSize,
				J9THREAD_PRIORITY_NORMAL,
				0,
				vmRuntimeStateListenerProcWrapper,
				vm,
				J9THREAD_CATEGORY_RESOURCE_MONITOR_THREAD);

	if (J9THREAD_SUCCESS == rc) {
		do {
			omrthread_monitor_wait(listener->runtimeStateListenerMutex);
		} while (J9VM_RUNTIME_STATE_LISTENER_UNINITIALIZED == listener->runtimeStateListenerState);

		if (J9VM_RUNTIME_STATE_LISTENER_STARTED == listener->runtimeStateListenerState) {
			rc = 0;
		}
	} else {
		/* error occurred during thread startup */
		rc = -1;
	}

	omrthread_monitor_exit(listener->runtimeStateListenerMutex);
	return rc;
}

void
stopVMRuntimeStateListener(J9JavaVM *vm)
{
	J9VMRuntimeStateListener *listener = &vm->vmRuntimeStateListener;
	if (J9VM_RUNTIME_STATE_LISTENER_STARTED == listener->runtimeStateListenerState) {
		/* Now stop the listener thread */
		omrthread_monitor_enter(listener->runtimeStateListenerMutex);
		listener->runtimeStateListenerState = J9VM_RUNTIME_STATE_LISTENER_STOP;
		omrthread_monitor_notify_all(listener->runtimeStateListenerMutex);
		while (J9VM_RUNTIME_STATE_LISTENER_TERMINATED != listener->runtimeStateListenerState) {
			omrthread_monitor_wait(listener->runtimeStateListenerMutex);
		}
		omrthread_monitor_exit(listener->runtimeStateListenerMutex);
	}
}

U_32
getVMRuntimeState(J9JavaVM *vm)
{
	return vm->vmRuntimeStateListener.vmRuntimeState;
}

BOOLEAN
updateVMRuntimeState(J9JavaVM *vm, U_32 newState)
{
	J9VMRuntimeStateListener *listener = &vm->vmRuntimeStateListener;
	BOOLEAN rc = FALSE;

	Assert_VM_true((J9VM_RUNTIME_STATE_ACTIVE == newState) || (J9VM_RUNTIME_STATE_IDLE == newState));

	if (J9VM_RUNTIME_STATE_LISTENER_STARTED == listener->runtimeStateListenerState) {
		omrthread_monitor_enter(listener->runtimeStateListenerMutex);
		if (J9VM_RUNTIME_STATE_LISTENER_STARTED == listener->runtimeStateListenerState) {
			Assert_VM_true(vm->vmRuntimeStateListener.vmRuntimeState != newState);
			vm->vmRuntimeStateListener.vmRuntimeState = newState;
		}
		omrthread_monitor_notify(listener->runtimeStateListenerMutex);
		omrthread_monitor_exit(listener->runtimeStateListenerMutex);
		rc = TRUE;
	}

	return rc;
}

U_32
getVMMinIdleWaitTime(J9JavaVM *vm)
{
	return vm->vmRuntimeStateListener.minIdleWaitTime;
}

