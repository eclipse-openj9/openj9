/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

#include <string.h>
#if defined (LINUXPPC64) || (defined (AIXPPC) && defined (PPC64)) || defined (J9ZOS39064) || defined(J9ZTPF)
#include <stdlib.h> /* for getenv */
#endif

#include "j9user.h"
#include "j9.h"
#include "jni.h"
#include "j9protos.h"
#include "vmaccess.h"
#include "j9consts.h"	
#include "omrthread.h"
#include "j9dump.h"
#include "jvminit.h"
#include "j9vmnls.h"
#include "j2sever.h"
#include "vmhook_internal.h"
#include "vm_internal.h"
#include "zip_api.h"

#include "ut_j9vm.h"
#include "jvmri.h"

static J9JavaVM * vmList = NULL;

#ifdef J9VM_OPT_VM_LOCAL_STORAGE
#include "j9vmls.h"

#define VM_FROM_ENV(env) (((J9VMThread *) (env))->javaVM)

static J9VMLSTable VMLSTable;
#endif

static UDATA terminateRemainingThreads (J9VMThread* vmThread);
static jint attachCurrentThread (JavaVM * vm, void ** p_env, void * thr_args, UDATA threadType);
jint JNICALL JNI_GetDefaultJavaVMInitArgs (void * vm_args);
static jint verifyCurrentThreadAttached (J9JavaVM * vm, J9VMThread ** pvmThread);
#if (defined(J9VM_OPT_SIDECAR)) 
void sidecarShutdown (J9VMThread* shutdownThread);
void sidecarExit(J9VMThread* shutdownThread);
#endif /* J9VM_OPT_SIDECAR */

static UDATA protectedDestroyJavaVM(J9PortLibrary* portLibrary, void * userData);
static UDATA protectedDetachCurrentThread(J9PortLibrary* portLibrary, void * userData);
static UDATA protectedInternalAttachCurrentThread(J9PortLibrary* portLibrary, void * userData);
static IDATA launchAttachApi(J9VMThread *currentThread);

J9_DECLARE_CONSTANT_UTF8(j9_init, "<init>");
J9_DECLARE_CONSTANT_UTF8(j9_clinit, "<clinit>");
J9_DECLARE_CONSTANT_UTF8(j9_printStackTrace, "printStackTrace");
J9_DECLARE_CONSTANT_UTF8(j9_shutdown, "shutdown");
J9_DECLARE_CONSTANT_UTF8(j9_exit, "exit");
J9_DECLARE_CONSTANT_UTF8(j9_run, "run");
J9_DECLARE_CONSTANT_UTF8(j9_initCause, "initCause");
J9_DECLARE_CONSTANT_UTF8(j9_completeInitialization, "completeInitialization");
J9_DECLARE_CONSTANT_UTF8(j9_void_void, "()V");
J9_DECLARE_CONSTANT_UTF8(j9_class_void, "(Ljava/lang/Class;)V");
J9_DECLARE_CONSTANT_UTF8(j9_class_class_void, "(Ljava/lang/Class;Ljava/lang/Class;)V");
J9_DECLARE_CONSTANT_UTF8(j9_string_void, "(Ljava/lang/String;)V");
J9_DECLARE_CONSTANT_UTF8(j9_throwable_void, "(Ljava/lang/Throwable;)V");
J9_DECLARE_CONSTANT_UTF8(j9_throwable_throwable, "(Ljava/lang/Throwable;)Ljava/lang/Throwable;");
J9_DECLARE_CONSTANT_UTF8(j9_int_void, "(I)V");

J9_DECLARE_CONSTANT_NAS(initCauseNameAndSig, j9_initCause, j9_throwable_throwable);
J9_DECLARE_CONSTANT_NAS(completeInitializationNameAndSig, j9_completeInitialization, j9_void_void);
J9_DECLARE_CONSTANT_NAS(threadRunNameAndSig, j9_run, j9_void_void);
J9_DECLARE_CONSTANT_NAS(initNameAndSig, j9_init, j9_void_void);
J9_DECLARE_CONSTANT_NAS(printStackTraceNameAndSig, j9_printStackTrace, j9_void_void);
J9_DECLARE_CONSTANT_NAS(clinitNameAndSig, j9_clinit, j9_void_void);
J9_DECLARE_CONSTANT_NAS(throwableNameAndSig , j9_init, j9_string_void);
J9_DECLARE_CONSTANT_NAS(arrayIndexNameAndSig, j9_init, j9_int_void);
J9_DECLARE_CONSTANT_NAS(throwableClassNameAndSig, j9_init, j9_class_void);
J9_DECLARE_CONSTANT_NAS(throwableClassClassNameAndSig, j9_init, j9_class_class_void);
J9_DECLARE_CONSTANT_NAS(throwableCauseNameAndSig, j9_init, j9_throwable_void);

J9NameAndSignature const * const exceptionConstructors[] = {
		&throwableNameAndSig,				/* J9ExceptionConstructorString */
		&arrayIndexNameAndSig,				/* J9ExceptionConstructorInt */
		&throwableClassNameAndSig,			/* J9ExceptionConstructorClass */
		&throwableClassClassNameAndSig,		/* J9ExceptionConstructorClassClass */
		&throwableCauseNameAndSig,			/* J9ExceptionConstructorThrowable */
};


typedef struct {
	J9JavaVM * vm;
	J9VMThread ** p_env;
	J9JavaVMAttachArgs * thr_args;
	UDATA threadType;
	void * osThread;
} J9InternalAttachCurrentThreadArgs;

jint JNICALL J9_CreateJavaVM(JavaVM ** p_vm, void ** p_env, J9CreateJavaVMParams *createParams)
{
	J9JavaVM *vm;
	J9VMThread *env = NULL;
	IDATA result;
	omrthread_t osThread = NULL;
	J9JavaVM **pvmList = GLOBAL_DATA(vmList);
	UDATA version = (UDATA)((JDK1_1InitArgs *) createParams->vm_args->actualVMArgs)->version;

#ifdef J9VM_THR_PREEMPTIVE
	omrthread_monitor_t globalMonitor;
#endif

	if (!jniVersionIsValid(version) || version == JNI_VERSION_1_1)
		return JNI_EVERSION;			/* unknown arg style, exit. */

#ifndef J9VM_OPT_MULTI_VM
	if (*pvmList)
		return JNI_ERR;
#endif

	if (omrthread_attach_ex(&osThread, J9THREAD_ATTR_DEFAULT))
		return JNI_ERR;

#ifdef J9VM_THR_PREEMPTIVE
	/* we must wait until after we attach to get the global monitor, since it may be * undefined before attach on some
	   platform (e.g. Unix) */
	globalMonitor = omrthread_global_monitor();
#endif

	/* Create the JavaVM structure */
	if ((result = initializeJavaVM(osThread, (J9JavaVM**)p_vm, createParams)) != JNI_OK)
	{
		goto error0;
	}

	vm = *(J9JavaVM**)p_vm;

	/* This has been set during initializeJavaVM */
	env = vm->mainThread;

	/* Success */
	vm->runtimeFlags |= J9_RUNTIME_INITIALIZED;
	*p_env = (void*) env;

	/* Link the VM into the list */

#ifdef J9VM_THR_PREEMPTIVE
	omrthread_monitor_enter(globalMonitor);
#endif

#ifdef J9VM_OPT_MULTI_VM
	if (*pvmList) {
		vm->linkNext = *pvmList;
		vm->linkPrevious = (*pvmList)->linkPrevious;
		(*pvmList)->linkPrevious = vm;
		vm->linkPrevious->linkNext = vm;
	} else {
		*pvmList = vm->linkNext = vm->linkPrevious = vm;
	}
#else

	/* On single threaded VMs, the check at the beginning of this function is sufficient */

#ifdef J9VM_THR_PREEMPTIVE
	if (*pvmList) {
		omrthread_monitor_exit(globalMonitor);
		result = JNI_ERR;
		goto error;
	}
#endif
	*pvmList = vm;
#endif

#ifdef J9VM_THR_PREEMPTIVE
	omrthread_monitor_exit(globalMonitor);
#endif

#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
	enterVMFromJNI(env);
	releaseVMAccess(env);
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */

	TRIGGER_J9HOOK_VM_INITIALIZED(vm->hookInterface, env);

	TRIGGER_J9HOOK_VM_THREAD_STARTED(vm->hookInterface, env, env);

	if (0 != launchAttachApi(env)) {
		result = JNI_ERR;
		goto error;
	}

    enterVMFromJNI(env);
	jniResetStackReferences((JNIEnv *) env);
    releaseVMAccess(env);

#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
	if (NULL != vm->javaOffloadSwitchOffWithReasonFunc) {
		env->javaOffloadState = 0;
		vm->javaOffloadSwitchOffWithReasonFunc(env, J9_JNI_OFFLOAD_SWITCH_CREATE_JAVA_VM);
	}
#endif

	return JNI_OK;

error:
	if (env) exceptionDescribe((JNIEnv *) env);

#ifdef J9VM_INTERP_SIG_QUIT_THREAD
	vm->J9SigQuitShutdown(vm);
#endif

#ifdef J9VM_PROF_EVENT_REPORTING
	if (env) {
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
		enterVMFromJNI(env);
		releaseVMAccess(env);
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */
		TRIGGER_J9HOOK_VM_SHUTTING_DOWN(vm->hookInterface, env, result);
	}
#endif /* J9VM_PROF_EVENT_REPORTING */
	freeJavaVM(vm);
  error0:
	omrthread_detach(osThread);
	*p_vm = NULL;
	*p_env = NULL;
	return (jint) result;
}

jint JNICALL J9_GetCreatedJavaVMs(JavaVM ** vm_buf, jsize bufLen, jsize * nVMs)
{
	jint result = JNI_ERR;
	J9JavaVM**pvmList = GLOBAL_DATA(vmList);
	jsize count = 0;
#ifdef J9VM_THR_PREEMPTIVE
	omrthread_monitor_t globalMonitor;

	if (omrthread_attach_ex(NULL, J9THREAD_ATTR_DEFAULT)) return -1;
	globalMonitor = omrthread_global_monitor();
	omrthread_monitor_enter(globalMonitor);
#endif

	if (bufLen == 0) goto done;

	if (*pvmList) {
#ifdef J9VM_OPT_MULTI_VM
		J9JavaVM * vm = *pvmList;

		while (count < bufLen) {
			++count;
			*(vm_buf++) = (JavaVM *) vm;
			vm = vm->linkNext;
			if (vm == *pvmList) break;
		}
#else
		count = 1;
		*vm_buf = (JavaVM *) *pvmList;
#endif
	} 

#if defined (LINUXPPC64) || (defined (AIXPPC) && defined (PPC64)) || defined (J9ZOS39064)
	/* there was a bug in older VMs on these platforms where jsize was defined to
	 * be 64-bits, rather than the 32-bits required by the JNI spec. Provide backwards
	 * compatibility if the JAVA_JSIZE_COMPAT environment variable is set
	 */
	if (getenv("JAVA_JSIZE_COMPAT")) {
		*(jlong*)nVMs = (jlong)count;
	} else {
		*nVMs = count;
	}
#else
	*nVMs = count;
#endif

	result = JNI_OK;

done:
#ifdef J9VM_THR_PREEMPTIVE
	omrthread_monitor_exit(globalMonitor);
	omrthread_detach(NULL);
#endif
	return result;
}


static UDATA
protectedDestroyJavaVM(J9PortLibrary* portLibrary, void * userData)
{
	J9VMThread * vmThread = userData;
	J9JavaVM * vm = vmThread->javaVM;
	J9JavaVM**pvmList = GLOBAL_DATA(vmList);

	/* Note: we don't have a matching exit call because by the time we leave this function the trace engine
	 * has been shutdown
	 */
	Trc_JNIinv_protectedDestroyJavaVM_Entry();

	/* Note: No need to ever unset this, since the vmThread is unuseable after DestroyJavaVM returns */
	vmThread->gpProtected = TRUE;

	Trc_JNIinv_protectedDestroyJavaVM_StartingThreadWait();

	/* Wait until the current thread is the only non-daemon thread still alive */
	omrthread_monitor_enter(vm->vmThreadListMutex);
	while (vm->totalThreadCount != (vm->daemonThreadCount + 1)) {
		omrthread_monitor_wait(vm->vmThreadListMutex);
	}
	omrthread_monitor_exit(vm->vmThreadListMutex);

	Trc_JNIinv_protectedDestroyJavaVM_FinishedThreadWait();

	/* run exit hooks */
	sidecarShutdown(vmThread);

	Trc_JNIinv_protectedDestroyJavaVM_vmCleanupDone();

	/* mark the current thread as no-longer alive */
	setEventFlag(vmThread, J9_PUBLIC_FLAGS_STOPPED);

	/* We are dead at this point. Clear the suspend bit prior to triggering the thread end hook */
    clearHaltFlag(vmThread, J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND);

	TRIGGER_J9HOOK_VM_THREAD_END(vm->hookInterface, vmThread, 1);

	/* Do the java cleanup */
	enterVMFromJNI(vmThread);
	cleanUpAttachedThread(vmThread);
	releaseVMAccess(vmThread);

	TRIGGER_J9HOOK_VM_SHUTTING_DOWN(vm->hookInterface, vmThread, 0);

	Trc_JNIinv_protectedDestroyJavaVM_vmShutdownHookFired();

#ifdef J9VM_OPT_SIDECAR
	if (vm->sidecarExitHook)
		(*(vm->sidecarExitHook))(vm);
#endif

	/* 
	 * shutdown the finalizer BEFORE we shutdown daemon threads, as 
	 * finalize methods could spawn more threads
	 */
	vm->memoryManagerFunctions->gcShutdownHeapManagement(vm);

	Trc_JNIinv_protectedDestroyJavaVM_HeapManagementShutdown();

#ifdef J9VM_INTERP_SIG_QUIT_THREAD
	if (vm->J9SigQuitShutdown) {
		vm->J9SigQuitShutdown(vm);
	}
#endif

	Trc_JNIinv_protectedDestroyJavaVM_vmShutdownDone();

	if (terminateRemainingThreads(vmThread)) {
		Trc_JNIinv_protectedDestroyJavaVM_terminateRemainingThreadsFailed();

		/* Prevents daemon threads from exiting */
		if (vm->runtimeFlagsMutex != NULL) {
			omrthread_monitor_enter(vm->runtimeFlagsMutex);
		}

		if (vm->runtimeFlags & J9_RUNTIME_EXIT_STARTED) {
			if (vm->runtimeFlagsMutex != NULL) {
				omrthread_monitor_exit(vm->runtimeFlagsMutex);
			}
			/* Do not acquire exclusive here as it may cause deadlocks with
			 * the other thread shutting down.
			 */
			return JNI_ERR;
		}

		vm->runtimeFlags |= J9_RUNTIME_EXIT_STARTED;
		if (vm->runtimeFlagsMutex != NULL) {
			omrthread_monitor_exit(vm->runtimeFlagsMutex);
		}

		/* If we cannot shut down, at least run exit code in loaded modules */
		runExitStages(vm, vmThread);

		/* bring all threads to a safepoint before we exit.  Can't kill the remaining
		 * threads attached to the vm (daemons) so bring them to a safe point and let
		 * exit proceed.  This will prevent intermittent crashes when GC occurs due to
		 * running daemon threads after native stacks have been unloaded from the process
		 */
		internalAcquireVMAccess(vmThread);
		acquireExclusiveVMAccess(vmThread);

#if defined(COUNT_BYTECODE_PAIRS)
		printBytecodePairs(vm);
#endif /* COUNT_BYTECODE_PAIRS */

		Trc_JNIinv_protectedDestroyJavaVM_CallingExitHookSecondary();
		
		if (vm->exitHook) {
			vm->exitHook(0);
		}

		return JNI_ERR;
	}

#ifdef J9VM_OPT_MULTI_VM
#ifdef J9VM_THR_PREEMPTIVE
{
	omrthread_monitor_t globalMonitor = omrthread_global_monitor();
	omrthread_monitor_enter(globalMonitor);
#endif
	vm->linkPrevious->linkNext = vm->linkNext;
	vm->linkNext->linkPrevious = vm->linkPrevious;
	if (vm == *pvmList)
		*pvmList = (vm->linkNext == vm) ? NULL : vm->linkNext;
#ifdef J9VM_THR_PREEMPTIVE
	omrthread_monitor_exit(globalMonitor);
}
#endif
#else
	*pvmList = NULL;
#endif

	Trc_JNIinv_protectedDestroyJavaVM_CallingExitHook();

	if (vm->exitHook) {
		vm->exitHook(0);
	}

	freeJavaVM(vm);

	return JNI_OK;
}

jint JNICALL DestroyJavaVM(JavaVM * javaVM)
{
	J9JavaVM * vm = ((J9InvocationJavaVM *)javaVM)->j9vm;
	J9VMThread * vmThread;
	UDATA result;
	J9PortLibrary * selfHandle;
	PORT_ACCESS_FROM_PORT(vm->portLibrary);

	/* There isn't an exit tracepoint on every path because the trace engine will be shut down
	 * in protectedDestroyJavaVM
	 */
	Trc_JNIinv_DestroyJavaVM_Entry(javaVM);

	/* There is a cyclic dependency when using HealthCenter which causes JVM to hang during shutdown.
	 * To break this cycle, thread doing the shutdown needs to be detached to notify Healthcenter that
	 * it has terminated, and then re-attached as "DestroyJavaVM helper thread".
	 * As part of this fix, Healthcenter also needs to be modified to not do the join on "DestroyJavaVM helper thread". 
	 * Therefore, if ever this thread name is modified, HealthCenter should be informed to adapt accordingly.
	 */
	result = DetachCurrentThread(javaVM);
	if ((JNI_OK == result) || (JNI_EDETACHED == result)) {
	 	J9JavaVMAttachArgs thr_args;
		thr_args.version = JNI_VERSION_1_2;
		thr_args.name = "DestroyJavaVM helper thread";
		thr_args.group = vm->systemThreadGroupRef;
		if (AttachCurrentThread((JavaVM *) vm, (void **) &vmThread, &thr_args) != JNI_OK) {
			Trc_JNIinv_DestroyJavaVM_failedAttach_Exit();
			return JNI_ERR;
		}
	} else {
		Trc_JNIinv_DestroyJavaVM_DetachCurrentThread_Exit(result);
		return (jint) result;
	}

	if(vm->runtimeFlagsMutex != NULL) {
		omrthread_monitor_enter(vm->runtimeFlagsMutex);
	}

	/* we only allow shutdown code to run once */
	
	if(vm->runtimeFlags & J9_RUNTIME_SHUTDOWN_STARTED) {
		if(vm->runtimeFlagsMutex != NULL) {
			omrthread_monitor_exit(vm->runtimeFlagsMutex);
		}
		Trc_JNIinv_DestroyJavaVM_alreadyShutdown_Exit();
		return JNI_ERR;
	}

	vm->runtimeFlags |= J9_RUNTIME_SHUTDOWN_STARTED;

	if(vm->runtimeFlagsMutex != NULL) {
		omrthread_monitor_exit(vm->runtimeFlagsMutex);
	}

	/* Make sure freeJavaVM does not free the port library inside the signal protection */
	selfHandle = PORTLIB->self_handle;
	PORTLIB->self_handle = NULL;

	if (j9sig_protect(
		protectedDestroyJavaVM, vmThread, 
		structuredSignalHandler, vmThread,
		J9PORT_SIG_FLAG_SIGALLSYNC | J9PORT_SIG_FLAG_MAY_CONTINUE_EXECUTION, 
		&result)
	) {
		result = JNI_ERR;
	}

	if (selfHandle != NULL) {
		PORTLIB->self_handle = selfHandle;
		if (result == JNI_OK) {
			j9port_shutdown_library();
		}
	}

	if (result == JNI_OK) {
		/* Detach the OS thread from the VM (this will discard the thread on the last detach) */
		omrthread_detach(NULL);
	}

	return (jint) result;
}


jint JNICALL AttachCurrentThread(JavaVM * vm, void ** p_env, void * thr_args)
{
	return attachCurrentThread(vm, p_env, thr_args, 0);
}


static UDATA
protectedDetachCurrentThread(J9PortLibrary* portLibrary, void * userData)
{
	J9VMThread * vmThread = userData;

	/* Note: No need to ever unset this, since the vmThread will be discarded */
	vmThread->gpProtected = TRUE;

	threadCleanup(vmThread, FALSE);

	return JNI_OK;
}

jint JNICALL DetachCurrentThread(JavaVM * javaVM)
{
	J9JavaVM * vm = ((J9InvocationJavaVM *)javaVM)->j9vm;
	J9VMThread * vmThread = NULL;
	UDATA result = 0;
	PORT_ACCESS_FROM_PORT(vm->portLibrary);

	/* we should return here to avoid the detaching operations after the destroy call to avoid
	 * the unexpected hang caused by the checking of exclusive VM access in deallocateVMThread()
	 */
	if (J9_ARE_ALL_BITS_SET(vm->runtimeFlags, J9_RUNTIME_EXIT_STARTED)) {
		return JNI_OK;
	}

	/* Verify that the current thread is allowed to detach from the VM */

	if ((result = (UDATA) verifyCurrentThreadAttached(vm, &vmThread)) == JNI_OK) {

		Trc_JNIinv_DetachCurrentThread(vmThread);

		/* Perform thread cleanup */

		if (j9sig_protect(
			protectedDetachCurrentThread, vmThread, 
			structuredSignalHandler, vmThread,
			J9PORT_SIG_FLAG_SIGALLSYNC | J9PORT_SIG_FLAG_MAY_CONTINUE_EXECUTION, 
			&result)
		) {
			result = JNI_ERR;
		}

		if (result == JNI_OK) {
			/* Detach the OS thread from the VM (this will discard the thread on the last detach) */
			omrthread_detach(NULL);
		}
	}

	return (jint) result;
}


static UDATA
protectedInternalAttachCurrentThread(J9PortLibrary* portLibrary, void * userData)
{
	J9InternalAttachCurrentThreadArgs * args = userData;
	J9JavaVM * vm = args->vm;
	J9VMThread ** p_env = args->p_env;
	J9JavaVMAttachArgs * thr_args = args->thr_args;
	UDATA threadType = args->threadType;
	const char *threadName = NULL;
	char *modifiedThreadName = NULL; /* needed if the original thread name is bad UTF8 */
	j9object_t *threadGroup = NULL;
	J9VMThread *env;
	UDATA osStackFree;
	void *memorySpace = vm->defaultMemorySpace;
#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
	BOOLEAN ifaEnabled = FALSE;
#endif /* defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT) */

	PORT_ACCESS_FROM_PORT(portLibrary);

	threadType |= J9_PRIVATE_FLAGS_ATTACHED_THREAD;

	if(thr_args != NULL) {
		U_32 jniVersion = (U_32)thr_args->version;

#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
		/* Threads which are already IFA enabled can set the IFA_ENABLED_JNI_VERSION flag
		 * in the JNI version to indicate this to the VM. Otherwise the JVM will remove
		 * the thread from the IFA when it's not running Java code.
		 */
		if (IFA_ENABLED_JNI_VERSION == (jniVersion & IFA_ENABLED_JNI_VERSION_MASK)) {
			jniVersion &= ~IFA_ENABLED_JNI_VERSION_MASK;
			ifaEnabled = TRUE;
		}
#endif /* defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT) */

		if(!jniVersionIsValid((UDATA)jniVersion)) {
			return JNI_EVERSION;
		}
		threadGroup = thr_args->group;
		threadName = thr_args->name;
		if (NULL != threadName) {
			size_t threadNameLength = strlen(threadName);

			if (!isValidUtf8((U_8*)threadName, threadNameLength)) {
					/* this code is very rarely executed, so not optimized */
				const char *errorMessage = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
						J9NLS_VM_JNI_INVALID_UTF8, "Invalid UTF8");
				size_t errorMessageLength = strlen(errorMessage);

				modifiedThreadName = j9mem_allocate_memory(errorMessageLength+threadNameLength+3, OMRMEM_CATEGORY_VM);
				/* add 2 bytes for the ": " and one byte for the null */

				if (NULL != modifiedThreadName) {
					strcpy(modifiedThreadName, errorMessage);
					strcat(modifiedThreadName, ": ");
					fixBadUtf8((U_8*)threadName, (U_8*)modifiedThreadName+strlen(modifiedThreadName), threadNameLength); /* concatenate the corrected name with the error string */
					threadName = modifiedThreadName;
				} else {
					return JNI_ENOMEM;
				}
			}
		}
	}


	if ((env = allocateVMThread(vm, args->osThread, threadType, memorySpace, NULL)) == NULL) {
		return JNI_ENOMEM;
	}
	env->gpProtected = TRUE;

#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
	if (ifaEnabled) {
		env->javaOffloadState = 1;
	}
#endif /* defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT) */

	/* Determine the thread's remaining OS stack */

	osStackFree = omrthread_current_stack_free();
	if (osStackFree == 0) {
		osStackFree = vm->defaultOSStackSize;
	}
	env->currentOSStackFree = osStackFree - (osStackFree / J9VMTHREAD_RESERVED_C_STACK_FRACTION);

	threadAboutToStart(env);

	/* if this is the main thread, the VM hasn't been bootstrapped yet, so we can't do this yet */
	if ( (threadType & J9_PRIVATE_FLAGS_NO_OBJECT) == 0) {
#if defined(J9VM_INTERP_ATOMIC_FREE_JNI)
		internalEnterVMFromJNI(env);
		internalReleaseVMAccess(env);
#endif /* J9VM_INTERP_ATOMIC_FREE_JNI */
		initializeAttachedThread(
			env, 
			threadName,
			threadGroup,
			(threadType & J9_PRIVATE_FLAGS_DAEMON_THREAD) != 0, 
			env);
		j9mem_free_memory(modifiedThreadName);
		if ((env->currentException != NULL) || (env->threadObject == NULL)) {
			/* We did not run threadCleanup, so the zombie counter was not incremented */
			deallocateVMThread(env, FALSE, TRUE);
			return JNI_ERR;
		}

		TRIGGER_J9HOOK_VM_THREAD_STARTED(vm->hookInterface, env, env);

	} else {
		j9mem_free_memory(modifiedThreadName);
#ifdef J9VM_OPT_JAVA_OFFLOAD_SUPPORT
		env->javaOffloadState = 1;
#endif
	}

	/* Success */

	env->gpProtected = FALSE;
	*p_env = env;
	return JNI_OK;
}


IDATA 
internalAttachCurrentThread(J9JavaVM * vm, J9VMThread ** p_env, J9JavaVMAttachArgs * thr_args, UDATA threadType, void * osThread)
{
	PORT_ACCESS_FROM_PORT(vm->portLibrary);
	J9InternalAttachCurrentThreadArgs args;
	UDATA result;

	args.vm = vm;
	args.p_env = p_env;
	args.thr_args = thr_args;
	args.threadType = threadType;
	args.osThread = osThread;

	if (j9sig_protect(
		protectedInternalAttachCurrentThread, &args, 
		structuredSignalHandlerVM, vm,
		J9PORT_SIG_FLAG_SIGALLSYNC | J9PORT_SIG_FLAG_MAY_CONTINUE_EXECUTION, 
		&result)
	) {
		result = JNI_ERR;
	}

	return (IDATA) result;
}

static J9ThreadEnv threadEnv = {
		omrthread_get_priority,
		omrthread_set_priority,
		omrthread_self,
		omrthread_global,
		omrthread_attach,
		omrthread_sleep,
		omrthread_create,
		omrthread_exit,
		omrthread_abort,
		omrthread_priority_interrupt,
		omrthread_clear_interrupted,
		omrthread_monitor_enter,
		omrthread_monitor_exit,
		omrthread_monitor_init_with_name,
		omrthread_monitor_destroy,
		omrthread_monitor_wait,
		omrthread_monitor_notify_all,
		omrthread_tls_get,
		omrthread_tls_set,
		omrthread_tls_alloc,
		omrthread_tls_free
};


/**
 * JVMTI_VERSION_1_0 		0x30010000
 * IFA_ENABLED_JNI_VERSION 0x79xxxxxx	- combine with the JNI_VERSION
 * J9THREAD_VERSION_1_1 	0x7C010001
 * SUNVMI_VERSION_1_1 		0x7D010001
 * UTE_VERSION_1_1          0x7E000101
 * JVMEXT_VERSION_1_1       0x7E010001
 * JVMRAS_VERSION_1_1       0x7F000001
 * JVMRAS_VERSION_1_3       0x7F000003
 * JVMRAS_VERSION_1_5       0x7F000005
 * HARMONY_VMI_VERSION_2_0 	0xC01D0020
 */

jint JNICALL GetEnv(JavaVM *jvm, void **penv, jint version)
{
	J9JavaVM * vm = ((J9InvocationJavaVM *)jvm)->j9vm;
	J9VMThread *vmThread = NULL;
	jint rc = JNI_EVERSION;

	/* all failure cases require that *penv be set to NULL */
	*penv = NULL;

#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
	/* The IFA_ENABLED_JNI_VERSION flag can be used as the version to determine
	 * if the JVM contains this support when the thread is not attached.
	 */
	if (IFA_ENABLED_JNI_VERSION == version) {
		return JNI_OK;
	}
#endif /* defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT) */

	if (version == J9THREAD_VERSION_1_1) {
		*penv = (void*)&threadEnv;
		return JNI_OK;
	}

	/* GetEnv is only allowed on attached threads */
	vmThread = currentVMThread(vm);
	if (vmThread == NULL) {
		return JNI_EDETACHED;
	}

	/* Call the hook - if rc changes from JNI_EVERSION, return it.
	 * Note: the JavaVM parameter must be used in the call instead of the J9JavaVM
	 * because the JavaVM parameter should be a J9InvocationJavaVM* when GetEnv()
	 * is called from JVMTI agents.
	 */
	TRIGGER_J9HOOK_VM_GETENV(vm->hookInterface, jvm, penv, version, rc);
	if (rc != JNI_EVERSION) {
		return rc;
	}

#ifdef J9VM_OPT_HARMONY
	/* Allow retrieval of the Harmony VM interface */
	if (HARMONY_VMI_VERSION_2_0 == version) {
		*penv = &(vm->harmonyVMInterface);
		return JNI_OK;
	}
#endif /* J9VM_OPT_HARMONY */

	if (version == UTE_VERSION_1_1) {
		if (vm->j9rasGlobalStorage != NULL) {	
			*penv = ((RasGlobalStorage *)vm->j9rasGlobalStorage)->utIntf;
		} 
		return *penv == NULL ? JNI_EVERSION : JNI_OK;
	}

	if (version == JVMRAS_VERSION_1_1 || version == JVMRAS_VERSION_1_3 || version == JVMRAS_VERSION_1_5 ) {
		PORT_ACCESS_FROM_JAVAVM(vm);
		if (vm->j9rasGlobalStorage == NULL) {	
			j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_VM_JVMRI_REQUESTED_WITHOUT_TRACE);
			return JNI_EINVAL;
		} else {
			*penv = ((RasGlobalStorage *)vm->j9rasGlobalStorage)->jvmriInterface;
		}
		return *penv == NULL ? JNI_EVERSION : JNI_OK;
	}

#if defined(J9VM_OPT_SIDECAR)
	if (version == JVMEXT_VERSION_1_1) {
		*penv = (void*)&vm->jvmExtensionInterface;
		return JNI_OK;
	}
#endif /* J9VM_OPT_SIDECAR */

#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
	/* The IFA_ENABLED_JNI_VERSION flag can be set in the JNI version to determine
	 * if the JVM contains this support.
	 */
	if (IFA_ENABLED_JNI_VERSION == (version & IFA_ENABLED_JNI_VERSION_MASK)) {
		version &= ~IFA_ENABLED_JNI_VERSION_MASK;
	}
#endif /* defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT) */

   if (!jniVersionIsValid((UDATA)version)) {
		return JNI_EVERSION;
	}

	*penv = (void *)vmThread;
	return JNI_OK;
}


IDATA 
attachSystemDaemonThread(J9JavaVM * vm, J9VMThread ** p_env, const char * threadName)
{
	J9JavaVMAttachArgs attachArgs;
	omrthread_t osThread;
	IDATA rc;

	if (omrthread_attach_ex(&osThread, J9THREAD_ATTR_DEFAULT)) {
		return JNI_EDETACHED;
	}

	attachArgs.version = JNI_VERSION_1_2;
	attachArgs.name = threadName;
	attachArgs.group = vm->systemThreadGroupRef;
	rc = internalAttachCurrentThread(
		vm, p_env, &attachArgs, 
		J9_PRIVATE_FLAGS_SYSTEM_THREAD | J9_PRIVATE_FLAGS_DAEMON_THREAD, 
		osThread);

	if (rc != JNI_OK) {
		omrthread_detach(osThread);
	}

	return rc;
}




#if (defined(J9VM_OPT_VM_LOCAL_STORAGE)) 
void initializeVMLocalStorage(J9JavaVM * vm)
{
	extern J9VMLSFunctionTable J9VMLSFunctions;
	J9VMLSTable* vmls = GLOBAL_DATA(VMLSTable);

	vm->vmLocalStorageFunctions = GLOBAL_TABLE( J9VMLSFunctions );

#ifdef J9VM_OPT_MULTI_VM
	if (!vmls->initialized) {
		UDATA i;
#ifdef J9VM_THR_PREEMPTIVE
		omrthread_monitor_t globalMonitor = omrthread_global_monitor();

		omrthread_monitor_enter(globalMonitor);
		if (!vmls->initialized) {
#endif
			for (i = 1; i < J9VMLS_MAX_KEYS-1; ++i)
				vmls->keys[i] = i + 1;
			vmls->keys[0] = 0;
			vmls->keys[J9VMLS_MAX_KEYS-1] = 0;
			vmls->head = 1;
			vmls->freeKeys = J9VMLS_MAX_KEYS-1;
			vmls->initialized = TRUE;
#ifdef J9VM_THR_PREEMPTIVE
		}
		omrthread_monitor_exit(globalMonitor);
#endif
	}
#endif
}

#endif /* J9VM_OPT_VM_LOCAL_STORAGE */


#if (defined(J9VM_OPT_VM_LOCAL_STORAGE)) 
UDATA JNICALL J9VMLSAllocKeys(JNIEnv * env, UDATA * pInitCount, ...)
{
	va_list args;
	J9VMLSTable *vmls = GLOBAL_DATA(VMLSTable);
	J9JavaVM **pvmList = GLOBAL_DATA(vmList);
#ifdef J9VM_THR_PREEMPTIVE
	omrthread_monitor_t globalMonitor = omrthread_global_monitor();

	omrthread_monitor_enter(globalMonitor);
#endif

	if (++(*pInitCount) == 1) {
		void **pKey;
#ifdef J9VM_OPT_MULTI_VM
		UDATA count = 0;

		/* Count the number of keys being initialized */

		va_start(args, pInitCount);
		while ((va_arg(args, void *)) != NULL) {
			++count;
		}
		va_end(args);

		/* Fail if there are not enough free keys left */

		if (count > vmls->freeKeys) {
#ifdef J9VM_THR_PREEMPTIVE
			omrthread_monitor_exit(globalMonitor);
#endif
			return 1;
		}

		/* Allocate the keys */

		va_start(args, pInitCount);
		while ((pKey = va_arg(args, void *)) != NULL) {
			UDATA key;

			key = vmls->head;
			vmls->head = vmls->keys[key];
			vmls->keys[key] = (UDATA) - 1;
			*pKey = (void *) key;

			/* Initialize the value of the key to NULL this VM and all other known VMs. VMs which are being created
			   (i.e. not in the list yet) will have all VMLS values set to NULL already.  The current VM may or may not 
			   be in the list, which means that the list may be empty.  The global monitor protects the VM list as well 
			   as the key list. */

			VM_FROM_ENV(env)->vmLocalStorage[key - 1] = NULL;
			if (*pvmList) {
				J9JavaVM *vm = *pvmList;

				do {
					vm->vmLocalStorage[key - 1] = NULL;
					vm = vm->linkNext;
				} while (vm != *pvmList);
			}
		}
		va_end(args);
		vmls->freeKeys -= count;
#else
		va_start(args, pInitCount);
		while ((pKey = va_arg(args, void *)) != NULL) {
			*pKey = NULL;
		}
		va_end(args);
#endif
	}
#ifdef J9VM_THR_PREEMPTIVE
	omrthread_monitor_exit(globalMonitor);
#endif

	return 0;
}

#endif /* J9VM_OPT_VM_LOCAL_STORAGE */


#if (defined(J9VM_OPT_VM_LOCAL_STORAGE)) 
void JNICALL J9VMLSFreeKeys(JNIEnv * env, UDATA * pInitCount, ...)
{
	va_list args;
	J9VMLSTable* vmls = GLOBAL_DATA(VMLSTable);
#ifdef J9VM_THR_PREEMPTIVE
	omrthread_monitor_t globalMonitor = omrthread_global_monitor();

	omrthread_monitor_enter(globalMonitor);
#endif

	if (--(*pInitCount) == 0) {
		void ** pKey;

#ifdef J9VM_OPT_MULTI_VM
		va_start(args, pInitCount);
		while ((pKey = va_arg(args, void *)) != NULL) {
			UDATA	key;

			key = (UDATA) *pKey;
			*pKey = NULL;
			if (vmls->keys[key] == (UDATA) -1) {
				vmls->keys[key] = vmls->head;
				vmls->head = key;
				++vmls->freeKeys;
			}
		}
		va_end(args);
#else
		va_start(args, pInitCount);
		while ((pKey = va_arg(args, void *)) != NULL) {
			*pKey = NULL;
		}
		va_end(args);
#endif
	}

#ifdef J9VM_THR_PREEMPTIVE
	omrthread_monitor_exit(globalMonitor);
#endif
}
#endif /* J9VM_OPT_VM_LOCAL_STORAGE */


#if (defined(J9VM_OPT_VM_LOCAL_STORAGE)) 
void * JNICALL J9VMLSGet(JNIEnv * env, void * key)
{
#ifdef J9VM_OPT_MULTI_VM
	return VM_FROM_ENV(env)->vmLocalStorage[((UDATA) key) - 1];
#else
	return key;
#endif
}
#endif /* J9VM_OPT_VM_LOCAL_STORAGE */


#if (defined(J9VM_OPT_VM_LOCAL_STORAGE)) 
void * JNICALL J9VMLSSet(JNIEnv * env, void ** pKey, void * value)
{
#ifdef J9VM_OPT_MULTI_VM
	VM_FROM_ENV(env)->vmLocalStorage[((UDATA) *pKey) - 1] = value;
#else
	*pKey = value;
#endif
	return value;
}
#endif /* J9VM_OPT_VM_LOCAL_STORAGE */

jint JNICALL AttachCurrentThreadAsDaemon(JavaVM * vm, void ** p_env, void * thr_args)
{
	return attachCurrentThread(vm, p_env, thr_args, J9_PRIVATE_FLAGS_DAEMON_THREAD);
}


#if (defined(J9VM_OPT_SIDECAR)) 
/* run the shutdown method in java.lang.Shutdown */
void sidecarShutdown(J9VMThread* shutdownThread) {
	J9JavaVM * vm = shutdownThread->javaVM;

	if (!(vm->runtimeFlags & J9_RUNTIME_CLEANUP)) {
		J9NameAndSignature nas;

		nas.name = (J9UTF8*)&j9_shutdown;
		nas.signature = (J9UTF8*)&j9_void_void;

		vm->runtimeFlags |= J9_RUNTIME_CLEANUP;
		enterVMFromJNI(shutdownThread);
		runStaticMethod(shutdownThread, (U_8*)"java/lang/Shutdown", &nas, 0, NULL);
		internalExceptionDescribe(shutdownThread);
		releaseVMAccess(shutdownThread);
	}
}
#endif /* J9VM_OPT_SIDECAR */



static jint 
attachCurrentThread(JavaVM *vm, void **p_env, void *thr_args, UDATA threadType)
{
	omrthread_t osThread = NULL;
	void *env = NULL;
	IDATA j9thrRc = 0;
	jint rc = JNI_OK;
	J9JavaVM *j9vm = ((J9InvocationJavaVM *)vm)->j9vm;

	j9thrRc = attachThreadWithCategory(&osThread, J9THREAD_CATEGORY_APPLICATION_THREAD);
	if (0 != j9thrRc) {
		Trc_JNIinv_AttachCurrentThread_J9ThreadAttachFailed(j9thrRc);
		return JNI_EDETACHED;
	}

	/* If the thread is already attached to the VM, fill in the p_env with the existing env and succeed */

	env = getVMThreadFromOMRThread(j9vm, osThread);
	if (env) {
		omrthread_detach(osThread);
		*p_env = env;
		Trc_JNIinv_AttachCurrentThread_AlreadyAttached(env);
		return JNI_OK;
	}

	/* Not attached */

	rc = (jint) internalAttachCurrentThread(j9vm, (J9VMThread **)p_env, (J9JavaVMAttachArgs *)thr_args, threadType, osThread);
	if (JNI_OK == rc) {
		Trc_JNIinv_AttachCurrentThread_NewAttach(*(J9VMThread **)p_env);
	} else {
		Trc_JNIinv_AttachCurrentThread_VMAttachFailed(rc);
		omrthread_detach(osThread);
	}
	return rc;
}


#if (defined(J9VM_OPT_SIDECAR)) 
jint JNICALL QueryGCStatus(JavaVM *vm, jint *nHeaps, GCStatus *status, jint statusSize)
{
	return ((J9JavaVM *)vm)->memoryManagerFunctions->queryGCStatus(vm, nHeaps, status, statusSize);
}
#endif /* J9VM_OPT_SIDECAR */


/* kill all remaining threads, except:
 *   1) the current thread
 *   2) system threads
 *   3) attached threads
 * Return non-zero if any attached threads were found.
 * Otherwise return zero.
 */
static UDATA terminateRemainingThreads(J9VMThread* vmThread) {
	UDATA rc = 0;
	J9VMThread * currentThread;
	J9JavaVM * vm = vmThread->javaVM;

	Trc_VM_terminateRemainingThreads_Entry(vmThread);

	/* Wait for any zombie threads (java notified of death, but vmThread not freed and threadProc not exited) to completely terminate */

	omrthread_monitor_enter(vm->vmThreadListMutex);
	while (vm->zombieThreadCount != 0) {
		omrthread_monitor_wait(vm->vmThreadListMutex);
	}
	omrthread_monitor_exit(vm->vmThreadListMutex);

	/* bring all threads to a safepoint */
	internalAcquireVMAccess(vmThread);
	acquireExclusiveVMAccess(vmThread);

	/* Cancel any remaining threads (except for the current thread). */
	/* because we have exclusive, we also own the thread list mutex */
	currentThread = vmThread->linkNext;
	while (currentThread != vmThread) {
		J9VMThread * nextThread = currentThread->linkNext;

		/* omrthread_cancel seems to have several problems. Occasionally it hangs in Linux
		 * and Neutrino. And on Windows and OS/X (for instance), it doesn't clean up the
		 * cancelled thread, so we leak resources like mad.
		 * So for this release we've decided to go for a conservative answer and simply
		 * refuse to shutdown if there are daemon threads still running.
		 */
		if (currentThread->privateFlags & J9_PRIVATE_FLAGS_SYSTEM_THREAD ) {
			/* do nothing. This thread will be cleaned up specially */
			Trc_VM_terminateRemainingThreads_SystemThread(vmThread, currentThread);
		} else {
			Trc_VM_terminateRemainingThreads_Daemon(vmThread, currentThread);
			rc += 1;
			if (vm->verboseLevel & VERBOSE_SHUTDOWN) {
				PORT_ACCESS_FROM_JAVAVM(vm);
				const char * format = j9nls_lookup_message(J9NLS_INFO, J9NLS_VM_THREAD_PREVENTING_SHUTDOWN,
					/* Cannot rely on NLS database existing at this point.  Add a default message string */
					"Thread \"%s\" is still alive after running the shutdown hooks.\n");
				char * threadName = getOMRVMThreadName(currentThread->omrVMThread);
				j9file_printf(PORTLIB, J9PORT_TTY_ERR, format, threadName);
				releaseOMRVMThreadName(currentThread->omrVMThread);
			}
		}
		currentThread = nextThread;
	} 

	/* If the disable shutdown flag is set, pretend to have a remaining thread */

	if (vm->runtimeFlags & J9_RUNTIME_DISABLE_VM_SHUTDOWN) {
		rc += 1;
	}

	releaseExclusiveVMAccess(vmThread);
	internalReleaseVMAccess(vmThread);

	Trc_VM_terminateRemainingThreads_Exit(vmThread, rc);

	return rc;
}



static jint verifyCurrentThreadAttached(J9JavaVM * vm, J9VMThread ** pvmThread)
{
	J9VMThread * vmThread = currentVMThread(vm);
	J9SFJNINativeMethodFrame * stackFrame;

	/* If the thread is not attached to the VM, error */

	if (!vmThread)
		return JNI_EDETACHED;

	/* A thread can only detach if it is attached (as opposed to forked in java) */

	if (!(vmThread->privateFlags & J9_PRIVATE_FLAGS_ATTACHED_THREAD))
		return JNI_ERR;

	/* A thread can only detach if it has not called into the VM (the current stack frame must be the only frame on stack) */

	if(vmThread->pc != (U_8*)J9SF_FRAME_TYPE_JNI_NATIVE_METHOD){
		return JNI_ERR;
	}

	stackFrame = (J9SFJNINativeMethodFrame *) (((U_8 *) vmThread->sp) + ((UDATA) vmThread->literals));
	if (stackFrame->savedPC != J9SF_FRAME_TYPE_END_OF_STACK)
		return JNI_ERR;

	*pvmThread = vmThread;
	return JNI_OK;
}



#if (defined(J9VM_OPT_SIDECAR)) 
jint JNICALL QueryJavaVM(JavaVM *vm,  jint nQueries, JavaVMQuery *queries)
{
	return JNI_ERR;
}

jint JNICALL ResetJavaVM(JavaVM *vm)
{
	return JNI_ERR;
}

/* run the shutdown method in java.lang.Shutdown */
void 
sidecarExit(J9VMThread* shutdownThread) {
	/* static void java.lang.Shutdown.exit(int status)
	 *
	 * The exit code is 130. Store ints in the low-memory half
	 * of the stack slot on 64-bit platforms. Otherwise, input
	 * argument will always be zero on big-endian platforms.
	 */
	I_32 args[] = {130};
	J9NameAndSignature nas = {0};

	nas.name = (J9UTF8 *)&j9_exit;
	nas.signature = (J9UTF8 *)&j9_int_void;

	enterVMFromJNI(shutdownThread);
	runStaticMethod(shutdownThread, (U_8 *)"java/lang/Shutdown", &nas, 1, (UDATA *)args);
	internalExceptionDescribe(shutdownThread);
	releaseVMAccess(shutdownThread);
}

static IDATA
launchAttachApi(J9VMThread *currentThread) {
	jclass clazz;
	jmethodID meth;
	JNIEnv *env = (JNIEnv *) currentThread;

	clazz = (*env)->FindClass(env, "com/ibm/tools/attach/target/AttachHandler");
	if (NULL == clazz) {
		return -1;
	}

	meth = (*env)->GetStaticMethodID(env, clazz, "initializeAttachAPI", "()V");
	if (NULL == meth) {
		return -1;
	}

	(*env)->CallStaticVoidMethod(env, clazz, meth);
	if ((*env)->ExceptionCheck(env)) {
		return -1;
	}

	return 0;
}

void*
J9_GetInterface(J9_INTERFACE_SELECTOR interfaceSelector, J9PortLibrary* portLib, void *userData)
{
	switch (interfaceSelector) {
	case IF_ZIPSUP:
		return getZipFunctions(portLib, userData);
	default: break;
	}
	Assert_VM_unreachable();
	return NULL;
}

#endif /* J9VM_OPT_SIDECAR */

