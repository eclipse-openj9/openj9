/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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
#include "vmaccess.h"
#include "j2sever.h"
#include "jvminit.h"
#include "omrthread.h"
#include "j9consts.h"
#include "ut_j9vm.h"
#include "vm_api.h"
#include "vmhook_internal.h"
#include "omrutilbase.h"

/* On zOS, NSIG macro is not defined.
 * A value of 65 is chosen if NSIG is undefined.
 */
#if defined(NSIG)
#define J9_SIG_COUNT (NSIG + 1)
#else /* defined(NSIG) */
#define J9_SIG_COUNT 65
#endif /* defined(NSIG) */

/* Different thread states for the VM signal dispatch thread */
#define J9VM_SIGNAL_DISPATCH_THREAD_UNINITIALIZED 0
#define J9VM_SIGNAL_DISPATCH_THREAD_STARTED 1
#define J9VM_SIGNAL_DISPATCH_THREAD_STOP 2
#define J9VM_SIGNAL_DISPATCH_THREAD_ABORT 3
#define J9VM_SIGNAL_DISPATCH_THREAD_TERMINATED 4

J9_DECLARE_CONSTANT_UTF8(j9_int_void, "(I)V");
J9_DECLARE_CONSTANT_UTF8(j9_dispatch, "dispatch");

static UDATA signalDispatchThreadProc(J9PortLibrary* portLib, void *userData);
static int J9THREAD_PROC signalDispatchThreadProcWrapper(void *entryarg);
static void signalDispatch(J9VMThread *vmThread, I_32 sigNum);

/**
 * Invoke jdk.internal.misc.Signal.dispatch(int number) in Java 9 and onwards.
 * Invoke sun.misc.Signal.dispatch(int number) in Java 8.
 *
 * @param vmThread pointer to a J9VMThread
 * @param sigNum integer value of the signal
 */
static void
signalDispatch(J9VMThread *vmThread, I_32 sigNum) {
	J9JavaVM *vm = vmThread->javaVM;
	J9NameAndSignature nas = {0};
	I_32 args[] = {sigNum};

	nas.name = (J9UTF8*)&j9_dispatch;
	nas.signature = (J9UTF8*)&j9_int_void;

	enterVMFromJNI(vmThread);

	if (J2SE_VERSION(vm) >= J2SE_19) {
		runStaticMethod(vmThread, (U_8*)"jdk/internal/misc/Signal", &nas, 1, (UDATA *)args);
	} else {
		runStaticMethod(vmThread, (U_8*)"sun/misc/Signal", &nas, 1, (UDATA *)args);
	}

	internalExceptionDescribe(vmThread);
	releaseVMAccess(vmThread);
}

/**
 * Main function executed by the VM Signal Dispatch Thread.
 *
 * @param portLib pointer to J9PortLibrary
 * @param userData pointer to J9VMSignalDispatchInfo
 *
 * @return 0
 */
static UDATA
signalDispatchThreadProc(J9PortLibrary* portLib, void *userData)
{
	J9JavaVM *vm = (J9JavaVM *)userData;
	J9VMSignalDispatchInfo *signalDispatchInfo = vm->signalDispatchInfo;
	J9VMThread *vmThread = signalDispatchInfo->signalDispatchVMThread;

	omrthread_monitor_enter(signalDispatchInfo->monitor);
	signalDispatchInfo->signalDispatchState = J9VM_SIGNAL_DISPATCH_THREAD_STARTED;

	/* Notify the parent thread that the VM signal dispatch thread has started successfully. */
	omrthread_monitor_notify(signalDispatchInfo->monitor);
	omrthread_monitor_exit(signalDispatchInfo->monitor);

	while (J9VM_SIGNAL_DISPATCH_THREAD_STOP != signalDispatchInfo->signalDispatchState) {
		UDATA i = 1;
		/* Find an unhandled signal. */
		for (; i < J9_SIG_COUNT; i++) {
			UDATA signalCount = signalDispatchInfo->unhandledSignals[i];
			if (signalCount > 0) {
				/* If an unhandled signal is found, then invoke the handler registered
				 * with the handler.
				 */
				signalDispatch(vmThread, (I_32)i);
				subtractAtomic(&signalDispatchInfo->unhandledSignals[i], 1);
				break;
			}
		}
		j9sem_wait(signalDispatchInfo->osSemaphore);
	}

	DetachCurrentThread((JavaVM *)vm);

	omrthread_monitor_enter(signalDispatchInfo->monitor);
	signalDispatchInfo->signalDispatchState = J9VM_SIGNAL_DISPATCH_THREAD_TERMINATED;

	omrthread_monitor_notify(signalDispatchInfo->monitor);
	omrthread_exit(signalDispatchInfo->monitor);

	/* NO GUARANTEED EXECUTION BEYOND THIS POINT */
	return 0;
}

/**
 * Wrapper function around signalDispatchThreadProc() to protect from synchronous signals.
 *
 * @param entryarg pointer to J9VMSignalDispatchInfo
 *
 * @return JNI_OK on success, JNI_ERR on failure
 */
static int J9THREAD_PROC
signalDispatchThreadProcWrapper(void *entryarg)
{
	J9JavaVM *vm = (J9JavaVM *)entryarg;
	J9VMSignalDispatchInfo *signalDispatchInfo = vm->signalDispatchInfo;
	J9JavaVMAttachArgs attachArgs = { 0 };
	int result = JNI_ERR;
	PORT_ACCESS_FROM_JAVAVM(vm);

	attachArgs.version = JNI_VERSION_1_8;
	attachArgs.name = "VM Signal Dispatch Thread";
	attachArgs.group = vm->systemThreadGroupRef;
	result = (int)internalAttachCurrentThread(vm, &signalDispatchInfo->signalDispatchVMThread, &attachArgs,
                                  J9_PRIVATE_FLAGS_DAEMON_THREAD | J9_PRIVATE_FLAGS_SYSTEM_THREAD | J9_PRIVATE_FLAGS_ATTACHED_THREAD,
                                  omrthread_self());

	if (JNI_OK == result) {
		UDATA rc = 0;
		j9sig_protect(signalDispatchThreadProc,
			vm,
			structuredSignalHandler,
			signalDispatchInfo->signalDispatchVMThread,
			J9PORT_SIG_FLAG_SIGALLSYNC | J9PORT_SIG_FLAG_MAY_CONTINUE_EXECUTION,
			&rc);
	} else {
		omrthread_monitor_enter(signalDispatchInfo->monitor);

		signalDispatchInfo->signalDispatchState = J9VM_SIGNAL_DISPATCH_THREAD_ABORT;
		omrthread_monitor_notify(signalDispatchInfo->monitor);

		omrthread_monitor_exit(signalDispatchInfo->monitor);
	}
	return result;
}

I_32
startSignalDispatchThread(J9JavaVM* vm)
{
	I_32 rc = -1;
	J9VMSignalDispatchInfo *signalDispatchInfo = vm->signalDispatchInfo;

	omrthread_monitor_enter(signalDispatchInfo->monitor);

	/* Create this thread as an APPLICATION_THREAD. */
	rc = (I_32) createThreadWithCategory(
				NULL,
				vm->defaultOSStackSize,
				J9THREAD_PRIORITY_NORMAL,
				0,
				signalDispatchThreadProcWrapper,
				vm,
				J9THREAD_CATEGORY_APPLICATION_THREAD);

	if (J9THREAD_SUCCESS == rc) {
		while (J9VM_SIGNAL_DISPATCH_THREAD_UNINITIALIZED == signalDispatchInfo->signalDispatchState) {
			omrthread_monitor_wait(signalDispatchInfo->monitor);
		}

		if (J9VM_SIGNAL_DISPATCH_THREAD_STARTED == signalDispatchInfo->signalDispatchState) {
			rc = 0;
		}
	} else {
		/* Error occurred during thread startup. */
		rc = -1;
	}

	omrthread_monitor_exit(signalDispatchInfo->monitor);
	return rc;
}

void
stopSignalDispatchThread(J9JavaVM *vm)
{
	J9VMSignalDispatchInfo *signalDispatchInfo = vm->signalDispatchInfo;
	if ((NULL != signalDispatchInfo) && (J9VM_SIGNAL_DISPATCH_THREAD_STARTED == signalDispatchInfo->signalDispatchState)) {
		/* Stop the VM signal dispatch thread by changing signalDispatchState to
		 * J9VM_SIGNAL_DISPATCH_THREAD_STOP.
		 */
		omrthread_monitor_enter(signalDispatchInfo->monitor);

		signalDispatchInfo->signalDispatchState = J9VM_SIGNAL_DISPATCH_THREAD_STOP;

		j9sem_post(signalDispatchInfo->osSemaphore);

		/* Wait to receive notification from the signalDispatchThreadProc function that
		 * the VM signal dispatch thread has been terminated.
		 */
		while (J9VM_SIGNAL_DISPATCH_THREAD_TERMINATED != signalDispatchInfo->signalDispatchState) {
			omrthread_monitor_wait(signalDispatchInfo->monitor);
		}

		omrthread_monitor_exit(signalDispatchInfo->monitor);
	}
}

BOOLEAN
recordAndNotifySignalReceived(J9JavaVM *vm, UDATA sigNum)
{
	BOOLEAN rc = FALSE;
	J9VMSignalDispatchInfo *signalDispatchInfo = vm->signalDispatchInfo;

	/* If -Xrs option is specified, then VM signal dispatch thread is not
	 * launched. So, we don't need to notify the VM signal dispatch thread
	 * if -Xrs option is specified.
	 */
	if (J9_ARE_ANY_BITS_SET(vm->sigFlags, J9_SIG_XRS | J9_SIG_XRS_ASYNC)) {
		return rc;
	}

	/* We can't rely upon omrthread_monitor_enter/exit code in this function.
	 * So, we don't acquire vm->runtimeFlagsMutex before checking vm->runtimeFlags.
	 * If shutdown or exit is started, then we shouldn't notify the VM signal dispatch
	 * thread.
	 */
	if (J9_ARE_ANY_BITS_SET(vm->runtimeFlags, J9_RUNTIME_EXIT_STARTED | J9_RUNTIME_SHUTDOWN_STARTED)) {
		return rc;
	}

	/* Atomically increment signal count by 1. */
	addAtomic(&signalDispatchInfo->unhandledSignals[sigNum], 1);

	/* The code below notifies the VM signal dispatch thread. This resumes execution in the
	 * signalDispatchThreadProc function. omrthread_attach and omrthread_detach are not
	 * signal-safe functions. It is unsafe to invoke these functions from the signal handler.
	 * j9sem_t is implemented using omrthread_monitor_t on zOS so we need to attach and detach
	 * to the thread library in order to use j9sem_t on zOS. On other distros, sem_post equivalent
	 * is used by j9sem_t. sem_post is signal-safe.
	 */
	if (J9VM_SIGNAL_DISPATCH_THREAD_STARTED == signalDispatchInfo->signalDispatchState) {
#if defined(J9ZOS390)
		omrthread_attach(NULL);
		if (0 == j9sem_post(signalDispatchInfo->osSemaphore)) {
			rc = TRUE;
		}
		omrthread_detach(NULL);
#else /* defined(J9ZOS390) */
		if (0 == j9sem_post(signalDispatchInfo->osSemaphore)) {
			rc = TRUE;
		}
#endif  /* defined(J9ZOS390) */
	}

	return rc;
}

BOOLEAN
isSignalUsedForShutdown(UDATA sigNum)
{
	return
#if defined(SIGHUP)
		(SIGHUP == sigNum) ||
#endif /* defined(SIGHUP) */
#if defined(SIGINT)
		(SIGINT == sigNum) ||
#endif /* defined(SIGINT) */
#if defined(SIGTERM)
		(SIGTERM == sigNum) ||
#endif /* defined(SIGTERM) */
		FALSE;
}

BOOLEAN
initVMSignalDispatchThreadSynchronizationVariables(J9JavaVM *vm)
{
	PORT_ACCESS_FROM_PORT(vm->portLibrary);

	vm->signalDispatchInfo = (J9VMSignalDispatchInfo *)j9mem_allocate_memory(sizeof(J9VMSignalDispatchInfo), OMRMEM_CATEGORY_VM);
	if (NULL == vm->signalDispatchInfo) {
		goto exit;
	}
	memset(vm->signalDispatchInfo, 0, sizeof(J9VMSignalDispatchInfo));

	vm->signalDispatchInfo->unhandledSignals = (UDATA *)j9mem_allocate_memory(J9_SIG_COUNT*sizeof(UDATA), OMRMEM_CATEGORY_VM);
	if (NULL == vm->signalDispatchInfo->unhandledSignals) {
		goto cleanup1;
	}
	memset(vm->signalDispatchInfo->unhandledSignals, 0, J9_SIG_COUNT*sizeof(UDATA));

	if (omrthread_monitor_init_with_name(&vm->signalDispatchInfo->monitor, 0, "VM Signal Dispatch Thread Monitor")) {
		goto cleanup2;
	}

	if (0 != j9sem_init(&vm->signalDispatchInfo->osSemaphore, 0)) {
		goto cleanup3;
	}

	return TRUE;

cleanup3:
	omrthread_monitor_destroy(vm->signalDispatchInfo->monitor);
cleanup2:
	j9mem_free_memory(vm->signalDispatchInfo->unhandledSignals);
cleanup1:
	j9mem_free_memory(vm->signalDispatchInfo);
exit:
	return FALSE;
}

void
destroyVMSignalDispatchThreadSynchronizationVariables(J9JavaVM *vm)
{
	J9VMSignalDispatchInfo *signalDispatchInfo = vm->signalDispatchInfo;
	PORT_ACCESS_FROM_PORT(vm->portLibrary);

	if (NULL != signalDispatchInfo) {
		omrthread_monitor_destroy(signalDispatchInfo->monitor);

		j9sem_destroy(signalDispatchInfo->osSemaphore);

		j9mem_free_memory(signalDispatchInfo->unhandledSignals);
		j9mem_free_memory(signalDispatchInfo);
	}
}
