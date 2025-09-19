/*******************************************************************************
 * Copyright IBM Corp. and others 2018
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

#include <assert.h>
#include <jni.h>

#include "bcverify_api.h"
#include "j9.h"
#include "j9cfg.h"
#include "jvminit.h"
#include "rommeth.h"
#include "ut_j9scar.h"
#include "util_api.h"

#if JAVA_SPEC_VERSION >= 19
#include "VMHelpers.hpp"
#include "ContinuationHelpers.hpp"
#endif /* JAVA_SPEC_VERSION >= 19 */
#if JAVA_SPEC_VERSION >= 24
#include "j9protos.h"
#endif /* JAVA_SPEC_VERSION >= 24 */
#if JAVA_SPEC_VERSION >= 25
#include "jvmtiInternal.h"
#include "objhelp.h"
#endif /* JAVA_SPEC_VERSION >= 25 */

extern "C" {

#if JAVA_SPEC_VERSION >= 19
extern J9JavaVM *BFUjavaVM;

extern IDATA (*f_threadSleep)(I_64 millis);
#endif /* JAVA_SPEC_VERSION >= 19 */

/* Define for debug
#define DEBUG_BCV
*/

#if JAVA_SPEC_VERSION >= 16
JNIEXPORT void JNICALL
JVM_DefineArchivedModules(JNIEnv *env, jobject obj1, jobject obj2)
{
	Assert_SC_true(!"JVM_DefineArchivedModules unimplemented");
}

JNIEXPORT void JNICALL
JVM_LogLambdaFormInvoker(JNIEnv *env, jstring str)
{
	Assert_SC_true(!"JVM_LogLambdaFormInvoker unimplemented");
}
#endif /* JAVA_SPEC_VERSION >= 16 */

#if JAVA_SPEC_VERSION >= 11
JNIEXPORT void JNICALL
JVM_InitializeFromArchive(JNIEnv *env, jclass clz)
{
	/* A no-op implementation is ok. */
}
#endif /* JAVA_SPEC_VERSION >= 11 */

#if JAVA_SPEC_VERSION >= 14
typedef struct GetStackTraceElementUserData {
	J9ROMClass *romClass;
	J9ROMMethod *romMethod;
	UDATA bytecodeOffset;
} GetStackTraceElementUserData;

static UDATA
getStackTraceElementIterator(J9VMThread *vmThread, void *voidUserData, UDATA bytecodeOffset, J9ROMClass *romClass, J9ROMMethod *romMethod, J9UTF8 *fileName, UDATA lineNumber, J9ClassLoader *classLoader, J9Class* ramClass, UDATA frameType)
{
	UDATA result = J9_STACKWALK_STOP_ITERATING;

	if ((NULL != romMethod)
		&& J9_ARE_ALL_BITS_SET(romMethod->modifiers, J9AccMethodFrameIteratorSkip)
	) {
		/* Skip methods with java.lang.invoke.FrameIteratorSkip / jdk.internal.vm.annotation.Hidden / java.lang.invoke.LambdaForm$Hidden annotation */
		result = J9_STACKWALK_KEEP_ITERATING;
	} else {
		GetStackTraceElementUserData *userData = (GetStackTraceElementUserData *)voidUserData;

		/* We are done, first non-hidden stack frame is found. */
		userData->romClass = romClass;
		userData->romMethod = romMethod;
		userData->bytecodeOffset = bytecodeOffset;
	}
	return result;
}

#if defined(DEBUG_BCV)
static void cfdumpBytecodePrintFunction(void *userData, char *format, ...)
{
	PORT_ACCESS_FROM_PORT((J9PortLibrary*)userData);
	va_list args;
	char outputBuffer[512] = {0};

	va_start(args, format);
	j9str_vprintf(outputBuffer, 512, format, args);
	va_end(args);
	j9tty_printf(PORTLIB, "%s", outputBuffer);
}
#endif /* defined(DEBUG_BCV) */

JNIEXPORT jstring JNICALL
JVM_GetExtendedNPEMessage(JNIEnv *env, jthrowable throwableObj)
{
	J9VMThread *vmThread = (J9VMThread *)env;
	J9JavaVM *vm = vmThread->javaVM;
	jobject msgObjectRef = NULL;

	Trc_SC_GetExtendedNPEMessage_Entry(vmThread, throwableObj);
	if (J9_ARE_ANY_BITS_SET(vm->extendedRuntimeFlags2, J9_EXTENDED_RUNTIME2_SHOW_EXTENDED_NPEMSG)) {
		J9InternalVMFunctions const * const vmFuncs = vm->internalVMFunctions;
		char *npeMsg = NULL;
		GetStackTraceElementUserData userData = {0};
		/* If -XX:+ShowHiddenFrames option has not been set, skip hidden method frames */
		UDATA skipHiddenFrames = J9_ARE_NO_BITS_SET(vm->runtimeFlags, J9_RUNTIME_SHOW_HIDDEN_FRAMES);

		Trc_SC_GetExtendedNPEMessage_Entry2(vmThread, throwableObj);
		vmFuncs->internalEnterVMFromJNI(vmThread);
		userData.bytecodeOffset = UDATA_MAX;
		vmFuncs->iterateStackTrace(vmThread, (j9object_t*)throwableObj, getStackTraceElementIterator, &userData, TRUE, skipHiddenFrames);
		if ((NULL != userData.romClass)
			&& (NULL != userData.romMethod)
			&& (UDATA_MAX != userData.bytecodeOffset)
		) {
			PORT_ACCESS_FROM_VMC(vmThread);
			J9NPEMessageData npeMsgData = {0};
#if defined(DEBUG_BCV)
			{
				U_8 *bytecodes = J9_BYTECODE_START_FROM_ROM_METHOD(userData.romMethod);
				U_32 flags = 0;

#if defined(J9VM_ENV_LITTLE_ENDIAN)
				flags |= BCT_LittleEndianOutput;
#else /* defined(J9VM_ENV_LITTLE_ENDIAN) */
				flags |= BCT_BigEndianOutput;
#endif /* defined(J9VM_ENV_LITTLE_ENDIAN) */
				j9bcutil_dumpBytecodes(PORTLIB, userData.romClass, bytecodes, 0, userData.bytecodeOffset, flags, (void *)cfdumpBytecodePrintFunction, PORTLIB, 0);
			}
#endif /* defined(DEBUG_BCV) */
			npeMsgData.npePC = userData.bytecodeOffset;
			npeMsgData.vmThread = vmThread;
			npeMsgData.romClass = userData.romClass;
			npeMsgData.romMethod = userData.romMethod;
			npeMsg = vmFuncs->getNPEMessage(&npeMsgData);
			if (NULL != npeMsg) {
				j9object_t msgObject = vm->memoryManagerFunctions->j9gc_createJavaLangString(vmThread, (U_8 *)npeMsg, strlen(npeMsg), 0);
				if (NULL != msgObject) {
					msgObjectRef = vmFuncs->j9jni_createLocalRef(env, msgObject);
				}
				j9mem_free_memory(npeMsg);
			}
			j9mem_free_memory(npeMsgData.liveStack);
			j9mem_free_memory(npeMsgData.bytecodeOffset);
			j9mem_free_memory(npeMsgData.bytecodeMap);
			j9mem_free_memory(npeMsgData.stackMaps);
			j9mem_free_memory(npeMsgData.unwalkedQueue);
		} else {
			Trc_SC_GetExtendedNPEMessage_Null_NPE_MSG(vmThread, userData.romClass, userData.romMethod, userData.bytecodeOffset);
		}
		vmFuncs->internalExitVMToJNI(vmThread);
	}
	Trc_SC_GetExtendedNPEMessage_Exit(vmThread, msgObjectRef);

	return (jstring)msgObjectRef;
}
#endif /* JAVA_SPEC_VERSION >= 14 */

#if JAVA_SPEC_VERSION >= 17
JNIEXPORT void JNICALL
JVM_DumpClassListToFile(JNIEnv *env, jstring str)
{
	Assert_SC_true(!"JVM_DumpClassListToFile unimplemented");
}

JNIEXPORT void JNICALL
JVM_DumpDynamicArchive(JNIEnv *env, jstring str)
{
	Assert_SC_true(!"JVM_DumpDynamicArchive unimplemented");
}
#endif /* JAVA_SPEC_VERSION >= 17 */

#if JAVA_SPEC_VERSION >= 18
JNIEXPORT jboolean JNICALL
JVM_IsFinalizationEnabled(JNIEnv *env)
{
	jboolean isFinalizationEnabled = JNI_TRUE;
	J9VMThread *currentThread = (J9VMThread*)env;
	if (J9_ARE_ANY_BITS_SET(currentThread->javaVM->extendedRuntimeFlags2, J9_EXTENDED_RUNTIME2_DISABLE_FINALIZATION)) {
		isFinalizationEnabled = JNI_FALSE;
	}
	return isFinalizationEnabled;
}

JNIEXPORT void JNICALL
JVM_ReportFinalizationComplete(JNIEnv *env, jobject obj)
{
	Assert_SC_true(!"JVM_ReportFinalizationComplete unimplemented");
}
#endif /* JAVA_SPEC_VERSION >= 18 */

#if JAVA_SPEC_VERSION >= 19
JNIEXPORT void * JNICALL
JVM_LoadZipLibrary(void)
{
	void *zipHandle = NULL;
	J9JavaVM *vm = BFUjavaVM;

	if (NULL != vm) {
		PORT_ACCESS_FROM_JAVAVM(vm);
		uintptr_t handle = 0;

		if (J9PORT_SL_FOUND == j9sl_open_shared_library(
				(char *)"zip",
				&handle,
				OMRPORT_SLOPEN_DECORATE | OMRPORT_SLOPEN_LAZY)
		) {
			zipHandle = (void *)handle;
		}
	}

	/* We may as well assert here: we won't make much progress without the library. */
	Assert_SC_notNull(zipHandle);

	return zipHandle;
}

JNIEXPORT void JNICALL
JVM_RegisterContinuationMethods(JNIEnv *env, jclass clz)
{
	Assert_SC_true(!"JVM_RegisterContinuationMethods unimplemented");
}

JNIEXPORT jboolean JNICALL
JVM_IsContinuationsSupported(void)
{
	return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
JVM_IsPreviewEnabled(void)
{
	jboolean isPreviewEnabled = JNI_FALSE;
	J9VMThread *currentThread = BFUjavaVM->internalVMFunctions->currentVMThread(BFUjavaVM);
	if (J9_ARE_ANY_BITS_SET(currentThread->javaVM->extendedRuntimeFlags2, J9_EXTENDED_RUNTIME2_ENABLE_PREVIEW)) {
		isPreviewEnabled = JNI_TRUE;
	}
	return isPreviewEnabled;
}

static void
setContinuationStateToLastUnmount(J9VMThread *currentThread, jobject thread)
{
	J9InternalVMFunctions const * const vmFuncs = currentThread->javaVM->internalVMFunctions;
	vmFuncs->enterVThreadTransitionCritical(currentThread, thread);

	/* Re-fetch reference as enterVThreadTransitionCritical may release VMAccess. */
	j9object_t threadObj = J9_JNI_UNWRAP_REFERENCE(thread);
	j9object_t continuationObj = J9VMJAVALANGVIRTUALTHREAD_CONT(currentThread, threadObj);
	ContinuationState volatile *continuationStatePtr = VM_ContinuationHelpers::getContinuationStateAddress(currentThread, continuationObj);
	/* Used in JVMTI to not suspend the virtual thread once it enters the last unmount phase. */
	VM_ContinuationHelpers::setLastUnmount(continuationStatePtr);
	vmFuncs->exitVThreadTransitionCritical(currentThread, thread);
}

/* Caller must have VMAccess. */
static void
virtualThreadMountBegin(JNIEnv *env, jobject thread)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9InternalVMFunctions const * const vmFuncs = currentThread->javaVM->internalVMFunctions;

	j9object_t threadObj = J9_JNI_UNWRAP_REFERENCE(thread);
	Assert_SC_true(IS_JAVA_LANG_VIRTUALTHREAD(currentThread, threadObj));

	if (TrcEnabled_Trc_SC_VirtualThread_Info) {
		J9JavaVM *vm = currentThread->javaVM;
		j9object_t continuationObj = J9VMJAVALANGVIRTUALTHREAD_CONT(currentThread, threadObj);
		J9VMContinuation *continuation = J9VMJDKINTERNALVMCONTINUATION_VMREF(currentThread, continuationObj);
		Trc_SC_VirtualThread_Info(
				currentThread,
				threadObj,
				J9VMJAVALANGVIRTUALTHREAD_STATE(currentThread, threadObj),
				J9OBJECT_I64_LOAD(currentThread, threadObj, vm->virtualThreadInspectorCountOffset),
				J9VMJAVALANGVIRTUALTHREAD_CARRIERTHREAD(currentThread, threadObj),
				continuationObj,
				continuation);
	}

	vmFuncs->enterVThreadTransitionCritical(currentThread, thread);

	VM_VMHelpers::virtualThreadHideFrames(currentThread, JNI_TRUE);
}

/* Caller must have VMAccess. */
static void
virtualThreadMountEnd(JNIEnv *env, jobject thread)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions const * const vmFuncs = vm->internalVMFunctions;
	j9object_t threadObj = J9_JNI_UNWRAP_REFERENCE(thread);

	Assert_SC_true(IS_JAVA_LANG_VIRTUALTHREAD(currentThread, threadObj));

	if (TrcEnabled_Trc_SC_VirtualThread_Info) {
		j9object_t continuationObj = J9VMJAVALANGVIRTUALTHREAD_CONT(currentThread, threadObj);
		Trc_SC_VirtualThread_Info(
				currentThread,
				threadObj,
				J9VMJAVALANGVIRTUALTHREAD_STATE(currentThread, threadObj),
				J9OBJECT_I64_LOAD(currentThread, threadObj, vm->virtualThreadInspectorCountOffset),
				J9VMJAVALANGVIRTUALTHREAD_CARRIERTHREAD(currentThread, threadObj),
				continuationObj,
				J9VMJDKINTERNALVMCONTINUATION_VMREF(currentThread, continuationObj));
	}

	VM_VMHelpers::virtualThreadHideFrames(currentThread, JNI_FALSE);

	/* Allow thread to be inspected again. */
	vmFuncs->exitVThreadTransitionCritical(currentThread, thread);

	TRIGGER_J9HOOK_VM_VIRTUAL_THREAD_MOUNT(vm->hookInterface, currentThread);
}

/* Caller must have VMAccess. */
static void
virtualThreadUnmountBegin(JNIEnv *env, jobject thread)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions const * const vmFuncs = vm->internalVMFunctions;

	j9object_t threadObj = J9_JNI_UNWRAP_REFERENCE(thread);

	Assert_SC_true(IS_JAVA_LANG_VIRTUALTHREAD(currentThread, threadObj));

	if (TrcEnabled_Trc_SC_VirtualThread_Info) {
		j9object_t continuationObj = J9VMJAVALANGVIRTUALTHREAD_CONT(currentThread, threadObj);
		Trc_SC_VirtualThread_Info(
				currentThread,
				threadObj,
				J9VMJAVALANGVIRTUALTHREAD_STATE(currentThread, threadObj),
				J9OBJECT_I64_LOAD(currentThread, threadObj, vm->virtualThreadInspectorCountOffset),
				J9VMJAVALANGVIRTUALTHREAD_CARRIERTHREAD(currentThread, threadObj),
				continuationObj,
				J9VMJDKINTERNALVMCONTINUATION_VMREF(currentThread, continuationObj));
	}

	TRIGGER_J9HOOK_VM_VIRTUAL_THREAD_UNMOUNT(vm->hookInterface, currentThread);

	vmFuncs->enterVThreadTransitionCritical(currentThread, thread);

	j9object_t carrierThreadObject = currentThread->carrierThreadObject;
	/* Virtual thread is being umounted. If its carrier thread is suspended, spin until
	 * the carrier thread is resumed. The carrier thread should not be mounted until it
	 * is resumed.
	 */
	while (VM_VMHelpers::isThreadSuspended(currentThread, carrierThreadObject)) {
		vmFuncs->exitVThreadTransitionCritical(currentThread, thread);
		vmFuncs->internalReleaseVMAccess(currentThread);
		/* Spin is used instead of the halt flag; otherwise, the virtual thread will
		 * show as suspended.
		 *
		 * TODO: Dynamically increase the sleep time to a bounded maximum.
		 */
		f_threadSleep(10);
		vmFuncs->internalAcquireVMAccess(currentThread);
		vmFuncs->enterVThreadTransitionCritical(currentThread, thread);
		carrierThreadObject = currentThread->carrierThreadObject;
	}

	VM_VMHelpers::virtualThreadHideFrames(currentThread, JNI_TRUE);
}

/* Caller must have VMAccess. */
static void
virtualThreadUnmountEnd(JNIEnv *env, jobject thread)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions const * const vmFuncs = vm->internalVMFunctions;

	j9object_t threadObj = J9_JNI_UNWRAP_REFERENCE(thread);
	j9object_t continuationObj = J9VMJAVALANGVIRTUALTHREAD_CONT(currentThread, threadObj);
	ContinuationState continuationState = *VM_ContinuationHelpers::getContinuationStateAddress(currentThread, continuationObj);

	Assert_SC_true(IS_JAVA_LANG_VIRTUALTHREAD(currentThread, threadObj));

	if (TrcEnabled_Trc_SC_VirtualThread_Info) {
		Trc_SC_VirtualThread_Info(
				currentThread,
				threadObj,
				J9VMJAVALANGVIRTUALTHREAD_STATE(currentThread, threadObj),
				J9OBJECT_I64_LOAD(currentThread, threadObj, vm->virtualThreadInspectorCountOffset),
				J9VMJAVALANGVIRTUALTHREAD_CARRIERTHREAD(currentThread, threadObj),
				continuationObj,
				J9VMJDKINTERNALVMCONTINUATION_VMREF(currentThread, continuationObj));
	}

	if (VM_ContinuationHelpers::isFinished(continuationState)) {
		vmFuncs->freeTLS(currentThread, threadObj);
	}

	VM_VMHelpers::virtualThreadHideFrames(currentThread, JNI_FALSE);

	/* Allow thread to be inspected again. */
	vmFuncs->exitVThreadTransitionCritical(currentThread, thread);
}
#endif /* JAVA_SPEC_VERSION >= 19 */

#if JAVA_SPEC_VERSION >= 20
JNIEXPORT jint JNICALL
JVM_GetClassFileVersion(JNIEnv *env, jclass cls)
{
	jint version = 0;
	J9VMThread *currentThread = (J9VMThread *)env;
	J9InternalVMFunctions const * const vmFuncs = currentThread->javaVM->internalVMFunctions;

	vmFuncs->internalEnterVMFromJNI(currentThread);

	if (NULL == cls) {
		vmFuncs->setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		J9Class *clazz = J9VM_J9CLASS_FROM_JCLASS(currentThread, cls);
		version = (jint)getClassFileVersion(currentThread, clazz);
	}

	vmFuncs->internalExitVMToJNI(currentThread);

	return version;
}
#endif /* JAVA_SPEC_VERSION >= 20 */

#if ((20 <= JAVA_SPEC_VERSION) && (JAVA_SPEC_VERSION <= 23)) || defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
JNIEXPORT void JNICALL
JVM_VirtualThreadHideFrames(
		JNIEnv *env,
#if (JAVA_SPEC_VERSION == 23) || defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
		jclass clz,
#else /* (JAVA_SPEC_VERSION == 23) || defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
		jobject vthread,
#endif /* (JAVA_SPEC_VERSION == 23) || defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
		jboolean hide)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9InternalVMFunctions const * const vmFuncs = currentThread->javaVM->internalVMFunctions;

	vmFuncs->internalEnterVMFromJNI(currentThread);

	j9object_t vThreadObj = currentThread->threadObject;
	Assert_SC_true(IS_JAVA_LANG_VIRTUALTHREAD(currentThread, vThreadObj));
	/* Do not allow JVMTI operations because J9VMThread->threadObject is modified
	 * between the first invocation with hide=true and the second invocation with
	 * hide=false. Otherwise, JVMTI functions will see an unstable
	 * J9VMThread->threadObject.
	 */
	bool hiddenFrames = J9_ARE_ALL_BITS_SET(currentThread->privateFlags, J9_PRIVATE_FLAGS_VIRTUAL_THREAD_HIDDEN_FRAMES);
	if (hide) {
		Assert_SC_true(!hiddenFrames);
#if JAVA_SPEC_VERSION < 23
		Assert_SC_true(vThreadObj == J9_JNI_UNWRAP_REFERENCE(vthread));
#endif /* JAVA_SPEC_VERSION < 23 */
		vmFuncs->enterVThreadTransitionCritical(currentThread, (jobject)&currentThread->threadObject);
	}

	VM_VMHelpers::virtualThreadHideFrames(currentThread, hide);

	if (!hide) {
		Assert_SC_true(hiddenFrames);
		vmFuncs->exitVThreadTransitionCritical(currentThread, (jobject)&currentThread->threadObject);
	}

	vmFuncs->internalExitVMToJNI(currentThread);
}
#endif /* ((20 <= JAVA_SPEC_VERSION) && (JAVA_SPEC_VERSION <= 23)) || defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */

#if JAVA_SPEC_VERSION >= 21
JNIEXPORT jboolean JNICALL
JVM_PrintWarningAtDynamicAgentLoad()
{
	jboolean result = JNI_TRUE;
	J9JavaVM *vm = BFUjavaVM;
	if (J9_ARE_ANY_BITS_SET(vm->runtimeFlags, J9_RUNTIME_ALLOW_DYNAMIC_AGENT)
		&& (0 <= FIND_ARG_IN_VMARGS(EXACT_MATCH, VMOPT_XXENABLEDYNAMICAGENTLOADING, NULL))
	) {
		result = JNI_FALSE;
	}
	return result;
}

JNIEXPORT void JNICALL
JVM_VirtualThreadMount(JNIEnv *env, jobject vthread, jboolean hide)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions const * const vmFuncs = vm->internalVMFunctions;

	Trc_SC_VirtualThreadMount_Entry(currentThread, vthread, hide);

	vmFuncs->internalEnterVMFromJNI(currentThread);

	if (hide) {
		virtualThreadMountBegin(env, vthread);
	} else {
		virtualThreadMountEnd(env, vthread);
	}

	vmFuncs->internalExitVMToJNI(currentThread);

	Trc_SC_VirtualThreadMount_Exit(currentThread, vthread, hide);
}

JNIEXPORT void JNICALL
JVM_VirtualThreadUnmount(JNIEnv *env, jobject vthread, jboolean hide)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions const * const vmFuncs = vm->internalVMFunctions;

	Trc_SC_VirtualThreadUnmount_Entry(currentThread, vthread, hide);

	vmFuncs->internalEnterVMFromJNI(currentThread);

	if (hide) {
		virtualThreadUnmountBegin(env, vthread);
	} else {
		virtualThreadUnmountEnd(env, vthread);
	}

	vmFuncs->internalExitVMToJNI(currentThread);

	Trc_SC_VirtualThreadUnmount_Exit(currentThread, vthread, hide);
}

JNIEXPORT jboolean JNICALL
JVM_IsForeignLinkerSupported()
{
	return JNI_TRUE;
}

JNIEXPORT void JNICALL
JVM_VirtualThreadStart(JNIEnv *env, jobject vthread)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions const * const vmFuncs = vm->internalVMFunctions;

	Trc_SC_VirtualThreadStart_Entry(currentThread, vthread);

	vmFuncs->internalEnterVMFromJNI(currentThread);

	virtualThreadMountEnd(env, vthread);
	TRIGGER_J9HOOK_VM_VIRTUAL_THREAD_STARTED(vm->hookInterface, currentThread);

	vmFuncs->internalExitVMToJNI(currentThread);

	Trc_SC_VirtualThreadStart_Exit(currentThread, vthread);
}

JNIEXPORT void JNICALL
JVM_VirtualThreadEnd(JNIEnv *env, jobject vthread)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions const * const vmFuncs = vm->internalVMFunctions;

	Trc_SC_VirtualThreadEnd_Entry(currentThread, vthread);

	vmFuncs->internalEnterVMFromJNI(currentThread);

	TRIGGER_J9HOOK_VM_VIRTUAL_THREAD_END(vm->hookInterface, currentThread);
	setContinuationStateToLastUnmount((J9VMThread *)env, vthread);
	virtualThreadUnmountBegin(env, vthread);
	TRIGGER_J9HOOK_VM_VIRTUAL_THREAD_DESTROY(vm->hookInterface, currentThread);

	vmFuncs->internalExitVMToJNI(currentThread);

	Trc_SC_VirtualThreadEnd_Exit(currentThread, vthread);
}
#endif /* JAVA_SPEC_VERSION >= 21 */

#if JAVA_SPEC_VERSION >= 22
JNIEXPORT void JNICALL
JVM_ExpandStackFrameInfo(JNIEnv *env, jobject object)
{
	assert(!"JVM_ExpandStackFrameInfo unimplemented");
}

JNIEXPORT void JNICALL
JVM_VirtualThreadDisableSuspend(
		JNIEnv *env,
#if JAVA_SPEC_VERSION >= 23
		jclass clz,
#else /* JAVA_SPEC_VERSION >= 23 */
		jobject vthread,
#endif /* JAVA_SPEC_VERSION >= 23 */
		jboolean enter)
{
	/* TODO: Add implementation.
	 * See https://github.com/eclipse-openj9/openj9/issues/18671 for more details.
	 */
}
#endif /* JAVA_SPEC_VERSION >= 22 */

#if JAVA_SPEC_VERSION >= 23
JNIEXPORT jint JNICALL
JVM_GetCDSConfigStatus()
{
	/* OpenJ9 does not support CDS, so we return 0 to indicate that there is no CDS config available. */
	return 0;
}
#endif /* JAVA_SPEC_VERSION >= 23 */

#if JAVA_SPEC_VERSION >= 21
/**
 * @brief Determine if the JVM is running inside a container.
 *
 * @return JNI_TRUE if running inside a container; otherwise, JNI_FALSE
 */
JNIEXPORT jboolean JNICALL
JVM_IsContainerized(void)
{
	J9JavaVM *vm = BFUjavaVM;
	jboolean isContainerized = JNI_FALSE;

	Assert_SC_true(NULL != vm);

	OMRPORT_ACCESS_FROM_J9PORT(vm->portLibrary);
	if (omrsysinfo_is_running_in_container()) {
		isContainerized = JNI_TRUE;
	}

	return isContainerized;
}
#endif /* JAVA_SPEC_VERSION >= 21 */

#if JAVA_SPEC_VERSION >= 24
/**
 * @brief Determine if the JVM is statically linked, always returns JNI_FALSE.
 *
 * @return JNI_FALSE
 */
JNIEXPORT jboolean JNICALL
JVM_IsStaticallyLinked(void)
{
	/* OpenJDK removed static builds using --enable-static-build. */
	return JNI_FALSE;
}

JNIEXPORT void JNICALL
JVM_VirtualThreadPinnedEvent(JNIEnv *env, jclass clazz, jstring op)
{
	// TODO: emit JFR Event
	return;
}

JNIEXPORT jobject JNICALL
JVM_TakeVirtualThreadListToUnblock(JNIEnv *env, jclass ignored)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;

	return vm->internalVMFunctions->takeVirtualThreadListToUnblock(currentThread);
}
#endif /* JAVA_SPEC_VERSION >= 24 */

#if JAVA_SPEC_VERSION >= 25
JNIEXPORT jobject JNICALL
JVM_CreateThreadSnapshot(JNIEnv *env, jobject thread)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions const * const vmfns = vm->internalVMFunctions;

	vmfns->internalEnterVMFromJNI(currentThread);

	J9Class *clazz = J9VMJDKINTERNALVMTHREADSNAPSHOT_OR_NULL(vm);
	j9object_t resultObject = NULL;
	jobject result = NULL;

	if ((NULL == thread) || (NULL == clazz)) {
		vmfns->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
	} else {
		j9object_t threadObject = NULL;
		BOOLEAN isVirtual = FALSE;
		BOOLEAN isAlive = FALSE;
		J9VMThread *targetThread = NULL;
		if (JVMTI_ERROR_NONE != vmfns->getTargetVMThreadHelper(
				currentThread, thread, JVMTI_ERROR_NONE, 0,
				&targetThread, &isVirtual, &isAlive)
		) {
			goto done;
		}

		if (!isAlive) {
			goto done;
		}

		if (NULL != targetThread) {
			vmfns->haltThreadForInspection(currentThread, targetThread);
		}

		resultObject = vm->memoryManagerFunctions->J9AllocateObject(currentThread, clazz, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
		if (NULL == resultObject) {
			vmfns->setHeapOutOfMemoryError(currentThread);
			goto done;
		}

		threadObject = J9_JNI_UNWRAP_REFERENCE(thread);
		J9VMJDKINTERNALVMTHREADSNAPSHOT_SET_NAME(currentThread, resultObject, J9VMJAVALANGTHREAD_NAME(currentThread, threadObject));
		if (isVirtual) {
			J9VMJDKINTERNALVMTHREADSNAPSHOT_SET_CARRIERTHREAD(
					currentThread, resultObject, J9VMJAVALANGVIRTUALTHREAD_CARRIERTHREAD(currentThread, threadObject));

			/* The following need to be filled for a virtual thread:
			 * - blockerObject
			 * - blockerTypeOrdinal to be filled
			 * - status
			 * - stacktrace
			 * - locks
			 */
		} else {
			/* The following need to be filled for a platform thread:
			 * - blockerObject
			 * - blockerTypeOrdinal to be filled
			 * - status
			 * - stacktrace
			 * - locks
			 */
		}

		if (NULL != resultObject) {
			result = vmfns->j9jni_createLocalRef(env, resultObject);
		}

		if (NULL != targetThread) {
			vmfns->resumeThreadForInspection(currentThread, targetThread);
		}

		vmfns->releaseTargetVMThreadHelper(currentThread, targetThread, thread);
	}

done:
	vmfns->internalExitVMToJNI(currentThread);
	return result;
}

JNIEXPORT jboolean JNICALL
JVM_NeedsClassInitBarrierForCDS(JNIEnv *env, jclass cls)
{
	Assert_SC_true(!"JVM_NeedsClassInitBarrierForCDS unimplemented");
	return JNI_FALSE;
}
#endif /* JAVA_SPEC_VERSION >= 25 */

} /* extern "C" */
