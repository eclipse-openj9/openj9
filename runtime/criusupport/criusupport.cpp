/*******************************************************************************
 * Copyright (c) 2021, 2022 IBM Corp. and others
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
#if defined(LINUX)
#include <criu/criu.h>
#include <fcntl.h>
#include <errno.h>
#include <dlfcn.h>
#include <malloc.h>
#endif /* defined(LINUX) */

#include "criusupport.hpp"
#include "VMHelpers.hpp"

#include "j9.h"
#include "j9jclnls.h"
#include "jni.h"
#include "jvmtinls.h"
#include "omrlinkedlist.h"
#include "omrthread.h"
#include "ut_j9criu.h"

extern "C" {

typedef void (*criuSetUnprivilegedFunctionPointerType)(bool unprivileged);

#define STRING_BUFFER_SIZE 256

static void
setupJNIFieldIDs(JNIEnv *env)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	jclass criuJVMCheckpointExceptionClass = NULL;
	jclass criuSystemCheckpointExceptionClass = NULL;
	jclass criuRestoreExceptionClass = NULL;

	UT_MODULE_LOADED(J9_UTINTERFACE_FROM_VM(vm));

	criuJVMCheckpointExceptionClass = env->FindClass("org/eclipse/openj9/criu/JVMCheckpointException");
	Assert_CRIU_notNull(criuJVMCheckpointExceptionClass);
	vm->criuJVMCheckpointExceptionClass = (jclass) env->NewGlobalRef(criuJVMCheckpointExceptionClass);

	vm->criuJVMCheckpointExceptionInit = env->GetMethodID(criuJVMCheckpointExceptionClass, "<init>", "(Ljava/lang/String;I)V");
	Assert_CRIU_notNull(vm->criuJVMCheckpointExceptionInit);

	criuSystemCheckpointExceptionClass = env->FindClass("org/eclipse/openj9/criu/SystemCheckpointException");
	Assert_CRIU_notNull(criuSystemCheckpointExceptionClass);
	vm->criuSystemCheckpointExceptionClass = (jclass) env->NewGlobalRef(criuSystemCheckpointExceptionClass);

	vm->criuSystemCheckpointExceptionInit = env->GetMethodID(criuSystemCheckpointExceptionClass, "<init>", "(Ljava/lang/String;I)V");
	Assert_CRIU_notNull(vm->criuSystemCheckpointExceptionInit);

	criuRestoreExceptionClass = env->FindClass("org/eclipse/openj9/criu/RestoreException");
	Assert_CRIU_notNull(criuRestoreExceptionClass);
	vm->criuRestoreExceptionClass = (jclass) env->NewGlobalRef(criuRestoreExceptionClass);

	vm->criuRestoreExceptionInit = env->GetMethodID(criuRestoreExceptionClass, "<init>", "(Ljava/lang/String;I)V");
	Assert_CRIU_notNull(vm->criuRestoreExceptionInit);

	if (NULL == vm->criuJVMCheckpointExceptionClass
		|| NULL == vm->criuSystemCheckpointExceptionClass
		|| NULL == vm->criuRestoreExceptionClass
	) {
		vmFuncs->internalEnterVMFromJNI(currentThread);
		vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
		vmFuncs->internalExitVMToJNI(currentThread);
	}
}

#define J9_NATIVE_STRING_NO_ERROR 0
#define J9_NATIVE_STRING_OUT_OF_MEMORY (-1)
#define J9_NATIVE_STRING_FAIL_TO_CONVERT (-2)

#define J9_CRIU_UNPRIVILEGED_NO_ERROR 0
#define J9_CRIU_UNPRIVILEGED_DLSYM_ERROR (-1)
#define J9_CRIU_UNPRIVILEGED_DLSYM_NULL_SYMBOL (-2)

/**
 * Converts the given java string to native representation. The nativeString parameter should point
 * to a buffer with its size specified by nativeStringBufSize. If the java string length exceeds the buffer
 * size, nativeString will be set to allocated memory.
 *
 * @note If successful, the caller is responsible for freeing any memory allocated and stored in nativeString.
 *
 * @param[in] currentThread current thread
 * @param[in] javaString java string object
 * @param[out] nativeString the native representation of the java string
 * @param[in] nativeStringBufSize size of the nativeString buffer
 *
 * @return return code indicating success, allocation failure, or string conversion failure
 */
static IDATA
getNativeString(J9VMThread *currentThread, j9object_t javaString, char **nativeString, IDATA nativeStringBufSize)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	char mutf8StringBuf[STRING_BUFFER_SIZE];
	char *mutf8String = NULL;
	UDATA mutf8StringSize = 0;
	IDATA requiredConvertedStringSize = 0;
	char *localNativeString = *nativeString;
	IDATA res = J9_NATIVE_STRING_NO_ERROR;
	PORT_ACCESS_FROM_VMC(currentThread);
	OMRPORT_ACCESS_FROM_J9PORT(PORTLIB);

	mutf8String = vmFuncs->copyStringToUTF8WithMemAlloc(currentThread, javaString, J9_STR_NULL_TERMINATE_RESULT, "", 0, mutf8StringBuf, STRING_BUFFER_SIZE, &mutf8StringSize);
	if (NULL == mutf8String) {
		res = J9_NATIVE_STRING_OUT_OF_MEMORY;
		goto free;
	}

	/* get the required size */
	requiredConvertedStringSize = omrstr_convert(J9STR_CODE_MUTF8, J9STR_CODE_PLATFORM_RAW,
					mutf8String,
					mutf8StringSize,
					localNativeString,
					0);

	if (requiredConvertedStringSize < 0) {
		Trc_CRIU_getNativeString_getStringSizeFail(currentThread, mutf8String, mutf8StringSize);
		res = J9_NATIVE_STRING_FAIL_TO_CONVERT;
		goto free;
	}

	/* Add 1 for NUL terminator */
	requiredConvertedStringSize += 1;

	if (requiredConvertedStringSize > nativeStringBufSize) {
		localNativeString = (char*) j9mem_allocate_memory(requiredConvertedStringSize, OMRMEM_CATEGORY_VM);
		if (NULL == localNativeString) {
			res = J9_NATIVE_STRING_OUT_OF_MEMORY;
			goto free;
		}
	}

	localNativeString[requiredConvertedStringSize - 1] = '\0';

	/* convert the string */
	requiredConvertedStringSize = omrstr_convert(J9STR_CODE_MUTF8, J9STR_CODE_PLATFORM_RAW,
					mutf8String,
					mutf8StringSize,
					localNativeString,
					requiredConvertedStringSize);

	if (requiredConvertedStringSize < 0) {
		Trc_CRIU_getNativeString_convertFail(currentThread, mutf8String, mutf8StringSize, requiredConvertedStringSize);
		res = J9_NATIVE_STRING_FAIL_TO_CONVERT;
		goto free;
	}

free:
	if (mutf8String != mutf8StringBuf) {
		j9mem_free_memory(mutf8String);
	}
	if (localNativeString != *nativeString) {
		if (J9_NATIVE_STRING_NO_ERROR == res) {
			*nativeString = localNativeString;
		} else {
			j9mem_free_memory(localNativeString);
			localNativeString = NULL;
		}
	}

	return res;
}

/**
 * Caller must first acquire exclusive safepoint or exclusive VMAccess
 */
static void
toggleSuspendOnJavaThreads(J9VMThread *currentThread, BOOLEAN suspend)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;

	Assert_CRIU_true((J9_XACCESS_EXCLUSIVE == vm->exclusiveAccessState) || (J9_XACCESS_EXCLUSIVE == vm->safePointState));

	J9VMThread *walkThread = J9_LINKED_LIST_START_DO(vm->mainThread);
	while (NULL != walkThread) {
		if (VM_VMHelpers::threadCanRunJavaCode(walkThread)
		&& (currentThread != walkThread)
		) {
			if (suspend) {
				vmFuncs->setHaltFlag(walkThread, J9_PUBLIC_FLAGS_HALT_THREAD_FOR_CHECKPOINT);
			} else {
				vmFuncs->clearHaltFlag(walkThread, J9_PUBLIC_FLAGS_HALT_THREAD_FOR_CHECKPOINT);
			}
		}
		walkThread = J9_LINKED_LIST_NEXT_DO(vm->mainThread, walkThread);
	}
}

static UDATA
notCheckpointSafeFrameWalkFunction(J9VMThread *vmThread, J9StackWalkState *walkState)
{
	J9Method *method = walkState->method;
	if (NULL != method) {
		J9ClassLoader *methodLoader = J9_CLASS_FROM_METHOD(method)->classLoader;
		/* we only enforce this in methods loaded by the bootloader */
		if (methodLoader == vmThread->javaVM->systemClassLoader) {
			J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
			if (J9ROMMETHOD_HAS_EXTENDED_MODIFIERS(romMethod)) {
				U_32 extraModifiers = getExtendedModifiersDataFromROMMethod(romMethod);
				if (J9ROMMETHOD_HAS_NOT_CHECKPOINT_SAFE_ANNOTATION(extraModifiers)) {
					*(bool *)walkState->userData1 = false;
					walkState->userData2 = (void *)vmThread;
					walkState->userData3 = (void *)method;
					return J9_STACKWALK_STOP_ITERATING;
				}
			}
		}
	}

	return J9_STACKWALK_KEEP_ITERATING;
}

static bool
checkIfSafeToCheckpoint(J9VMThread *currentThread)
{
	bool isSafe = true;
	J9JavaVM *vm = currentThread->javaVM;

	Assert_CRIU_true((J9_XACCESS_EXCLUSIVE == vm->exclusiveAccessState) || (J9_XACCESS_EXCLUSIVE == vm->safePointState));

	J9VMThread *walkThread = J9_LINKED_LIST_START_DO(vm->mainThread);
	while (NULL != walkThread) {
		if (VM_VMHelpers::threadCanRunJavaCode(walkThread)
		&& (currentThread != walkThread)
		) {
			J9StackWalkState walkState;
			walkState.walkThread = walkThread;
			walkState.flags = J9_STACKWALK_ITERATE_FRAMES | J9_STACKWALK_INCLUDE_NATIVES;
			walkState.skipCount = 0;
			walkState.userData1 = (void *)&isSafe;
			walkState.frameWalkFunction = notCheckpointSafeFrameWalkFunction;

			vm->walkStackFrames(walkThread, &walkState);
			if (!isSafe) {
				Trc_CRIU_checkpointJVMImpl_checkIfSafeToCheckpointBlocked(currentThread, walkState.userData2, walkState.userData3);
				break;
			}
		}
		walkThread = J9_LINKED_LIST_NEXT_DO(vm->mainThread, walkThread);
	}

	return isSafe;
}

static VMINLINE void
acquireSafeOrExcusiveVMAccess(J9VMThread *currentThread, J9InternalVMFunctions *vmFuncs, bool isSafe)
{
	if (isSafe) {
		vmFuncs->acquireSafePointVMAccess(currentThread);
	} else {
		vmFuncs->acquireExclusiveVMAccess(currentThread);
	}
}

static VMINLINE void
releaseSafeOrExcusiveVMAccess(J9VMThread *currentThread, J9InternalVMFunctions *vmFuncs, bool isSafe)
{
	if (isSafe) {
		vmFuncs->releaseSafePointVMAccess(currentThread);
	} else {
		vmFuncs->releaseExclusiveVMAccess(currentThread);
	}
}

void JNICALL
Java_org_eclipse_openj9_criu_CRIUSupport_checkpointJVMImpl(JNIEnv *env,
		jclass unused,
		jstring imagesDir,
		jboolean leaveRunning,
		jboolean shellJob,
		jboolean extUnixSupport,
		jint logLevel,
		jstring logFile,
		jboolean fileLocks,
		jstring workDir,
		jboolean tcpEstablished,
		jboolean autoDedup,
		jboolean trackMemory,
		jboolean unprivileged)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	J9PortLibrary *portLibrary = vm->portLibrary;

	jclass currentExceptionClass = NULL;
	char *exceptionMsg = NULL;
	const char *nlsMsgFormat = NULL;
	UDATA msgCharLength = 0;
	IDATA systemReturnCode = 0;
	criuSetUnprivilegedFunctionPointerType criuSetUnprivileged = NULL;
	const char *dlerrorReturnString = NULL;
	PORT_ACCESS_FROM_VMC(currentThread);

	if (NULL == vm->criuJVMCheckpointExceptionClass) {
		setupJNIFieldIDs(env);
	}

	vm->checkpointState.checkpointThread = currentThread;

	Trc_CRIU_checkpointJVMImpl_Entry(currentThread);
	if (vmFuncs->isCheckpointAllowed(currentThread)) {
#if defined(LINUX)
		j9object_t cpDir = NULL;
		j9object_t log = NULL;
		j9object_t wrkDir = NULL;
		IDATA dirFD = 0;
		IDATA workDirFD = 0;
		char directoryBuf[STRING_BUFFER_SIZE];
		char *directoryChars = directoryBuf;
		char logFileBuf[STRING_BUFFER_SIZE];
		char *logFileChars = logFileBuf;
		char workDirBuf[STRING_BUFFER_SIZE];
		char *workDirChars = workDirBuf;
		BOOLEAN isAfterCheckpoint = FALSE;
		I_64 checkpointNanoTimeMonotonic = 0;
		I_64 restoreNanoTimeMonotonic = 0;
		U_64 checkpointNanoUTCTime = 0;
		U_64 restoreNanoUTCTime = 0;
		UDATA success = 0;
		bool safePoint = J9_ARE_ANY_BITS_SET(vm->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_OSR_SAFE_POINT);
		bool retryPermitted = vm->checkpointState.maxRetryForNotCheckpointSafe > 0;
		BOOLEAN syslogFlagNone = TRUE;
		char *syslogOptions = NULL;
		I_32 syslogBufferSize = 0;
		UDATA oldVMState = VM_VMHelpers::setVMState(currentThread, J9VMSTATE_CRIU_SUPPORT_CHECKPOINT_PHASE_START);

		vmFuncs->internalEnterVMFromJNI(currentThread);

		Assert_CRIU_notNull(imagesDir);
		cpDir = J9_JNI_UNWRAP_REFERENCE(imagesDir);
		systemReturnCode = getNativeString(currentThread, cpDir, &directoryChars, STRING_BUFFER_SIZE);
		switch (systemReturnCode) {
		case J9_NATIVE_STRING_NO_ERROR:
			break;
		case J9_NATIVE_STRING_OUT_OF_MEMORY:
			vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
			goto freeDir;
		case J9_NATIVE_STRING_FAIL_TO_CONVERT:
			currentExceptionClass = vm->criuJVMCheckpointExceptionClass;
			nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_JCL_CRIU_FAILED_TO_CONVERT_JAVA_STRING, NULL);
			goto freeDir;
		}

		if (NULL != logFile) {
			log = J9_JNI_UNWRAP_REFERENCE(logFile);
			systemReturnCode = getNativeString(currentThread, log, &logFileChars, STRING_BUFFER_SIZE);
			switch (systemReturnCode) {
			case J9_NATIVE_STRING_NO_ERROR:
				break;
			case J9_NATIVE_STRING_OUT_OF_MEMORY:
				vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
				goto freeLog;
			case J9_NATIVE_STRING_FAIL_TO_CONVERT:
				currentExceptionClass = vm->criuJVMCheckpointExceptionClass;
				nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_JCL_CRIU_FAILED_TO_CONVERT_JAVA_STRING, NULL);
				goto freeLog;
			}
		}

		if (NULL != workDir) {
			wrkDir = J9_JNI_UNWRAP_REFERENCE(workDir);
			systemReturnCode = getNativeString(currentThread, wrkDir, &workDirChars, STRING_BUFFER_SIZE);
			switch (systemReturnCode) {
			case J9_NATIVE_STRING_NO_ERROR:
				break;
			case J9_NATIVE_STRING_OUT_OF_MEMORY:
				vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
				goto freeWorkDir;
			case J9_NATIVE_STRING_FAIL_TO_CONVERT:
				currentExceptionClass = vm->criuJVMCheckpointExceptionClass;
				nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_JCL_CRIU_FAILED_TO_CONVERT_JAVA_STRING, NULL);
				goto freeWorkDir;
			}
		}

		dirFD = open(directoryChars, O_DIRECTORY);
		if (dirFD < 0) {
			systemReturnCode = errno;
			currentExceptionClass = vm->criuJVMCheckpointExceptionClass;
			nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_JCL_CRIU_FAILED_TO_OPEN_DIR, NULL);
			goto freeWorkDir;
		}

		if (NULL != workDir) {
			workDirFD = open(workDirChars, O_DIRECTORY);
			if (workDirFD < 0) {
				systemReturnCode = errno;
				currentExceptionClass = vm->criuJVMCheckpointExceptionClass;
				nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_JCL_CRIU_FAILED_TO_OPEN_WORK_DIR, NULL);
				goto closeDirFD;
			}
		}

		systemReturnCode = criu_init_opts();
		if (0 != systemReturnCode) {
			currentExceptionClass = vm->criuSystemCheckpointExceptionClass;
			nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_JCL_CRIU_INIT_FAILED, NULL);
			goto closeWorkDirFD;
		}

		if (JNI_TRUE == unprivileged) {
			/*
			 * dlsym() can return NULL in case of error or if the specified symbol's value is actually NULL.
			 * In order to distinguish between the two cases we need to check for (and thereby clear) any pending errors by first calling dlerror(),
			 * then calling dlsym(), then calling dlerror() again to check for any new error. We can then call our function
			 * via the pointer returned by dlsym(), assuming it is not NULL.
			 */
			dlerrorReturnString = dlerror();
			if (NULL == dlerrorReturnString) {
				criuSetUnprivileged = (criuSetUnprivilegedFunctionPointerType)dlsym(RTLD_DEFAULT, "criu_set_unprivileged");
				dlerrorReturnString = dlerror();
			}
			if ((NULL == dlerrorReturnString) && (NULL != criuSetUnprivileged)) {
				systemReturnCode = J9_CRIU_UNPRIVILEGED_NO_ERROR;
				criuSetUnprivileged(JNI_FALSE != unprivileged);
			} else {
				currentExceptionClass = vm->criuSystemCheckpointExceptionClass;
				systemReturnCode = (NULL != dlerrorReturnString) ? J9_CRIU_UNPRIVILEGED_DLSYM_ERROR : J9_CRIU_UNPRIVILEGED_DLSYM_NULL_SYMBOL;
				nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_JCL_CRIU_CANNOT_SET_UNPRIVILEGED, NULL);
				goto closeWorkDirFD;
			}
		}

		criu_set_images_dir_fd(dirFD);
		criu_set_shell_job(JNI_FALSE != shellJob);
		if (logLevel > 0) {
			criu_set_log_level((int)logLevel);
		}
		if (NULL != logFile) {
			criu_set_log_file(logFileChars);
		}
		criu_set_leave_running(JNI_FALSE != leaveRunning);
		criu_set_ext_unix_sk(JNI_FALSE != extUnixSupport);
		criu_set_file_locks(JNI_FALSE != fileLocks);
		criu_set_tcp_established(JNI_FALSE != tcpEstablished);
		criu_set_auto_dedup(JNI_FALSE != autoDedup);
		criu_set_track_mem(JNI_FALSE != trackMemory);

		if (NULL != workDir) {
			criu_set_work_dir_fd(workDirFD);
		}

		acquireSafeOrExcusiveVMAccess(currentThread, vmFuncs, safePoint);

		for (UDATA i = 0; !checkIfSafeToCheckpoint(currentThread) && retryPermitted; i++) {
			releaseSafeOrExcusiveVMAccess(currentThread, vmFuncs, safePoint);
			vmFuncs->internalExitVMToJNI(currentThread);
			omrthread_nanosleep(1000);
			vmFuncs->internalEnterVMFromJNI(currentThread);
			if (i == vm->checkpointState.maxRetryForNotCheckpointSafe) {
				currentExceptionClass = vm->criuJVMCheckpointExceptionClass;
				systemReturnCode = vm->checkpointState.maxRetryForNotCheckpointSafe;
				nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_JCL_CRIU_MAX_RETRY_FOR_NOTCHECKPOINTSAFE_REACHED, NULL);
				goto closeWorkDirFD;
			}
			acquireSafeOrExcusiveVMAccess(currentThread, vmFuncs, safePoint);
		}

		toggleSuspendOnJavaThreads(currentThread, TRUE);

		vm->extendedRuntimeFlags2 |= J9_EXTENDED_RUNTIME2_CRIU_SINGLE_THREAD_MODE;

		releaseSafeOrExcusiveVMAccess(currentThread, vmFuncs, safePoint);

		VM_VMHelpers::setVMState(currentThread, J9VMSTATE_CRIU_SUPPORT_CHECKPOINT_PHASE_JAVA_HOOKS);

		if (FALSE == vmFuncs->jvmCheckpointHooks(currentThread)) {
			/* throw the pending exception */
			goto wakeJavaThreads;
		}

		/* trigger a GC to disclaim memory */
		vm->memoryManagerFunctions->j9gc_modron_global_collect_with_overrides(currentThread, J9MMCONSTANT_EXPLICIT_GC_SYSTEM_GC);
		vm->memoryManagerFunctions->j9gc_modron_global_collect_with_overrides(currentThread, J9MMCONSTANT_EXPLICIT_GC_PREPARE_FOR_CHECKPOINT);

		acquireSafeOrExcusiveVMAccess(currentThread, vmFuncs, safePoint);

		VM_VMHelpers::setVMState(currentThread, J9VMSTATE_CRIU_SUPPORT_CHECKPOINT_PHASE_INTERNAL_HOOKS);

		/* Run internal checkpoint hooks, after iterating heap objects */
		if (FALSE == vmFuncs->runInternalJVMCheckpointHooks(currentThread)) {
			currentExceptionClass = vm->criuJVMCheckpointExceptionClass;
			nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
				J9NLS_JCL_CRIU_FAILED_TO_RUN_INTERNAL_CHECKPOINT_HOOKS, NULL);
			goto wakeJavaThreadsWithExclusiveVMAccess;
		}

		vm->checkpointState.checkpointRestoreTimeDelta = 0;
		portLibrary->nanoTimeMonotonicClockDelta = 0;
		checkpointNanoTimeMonotonic = j9time_nano_time();
		checkpointNanoUTCTime = j9time_current_time_nanos(&success);
		if (0 == success) {
			systemReturnCode = errno;
			currentExceptionClass = vm->criuJVMCheckpointExceptionClass;
			nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_JCL_CRIU_J9_CURRENT_TIME_NANOS_FAILURE, NULL);
			goto wakeJavaThreadsWithExclusiveVMAccess;
		}
		Trc_CRIU_before_checkpoint(currentThread, checkpointNanoTimeMonotonic, checkpointNanoUTCTime);

		syslogOptions = (char *)j9mem_allocate_memory(STRING_BUFFER_SIZE, J9MEM_CATEGORY_VM);
		if (NULL == syslogOptions) {
			systemReturnCode = J9_NATIVE_STRING_OUT_OF_MEMORY;
			vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
			goto wakeJavaThreadsWithExclusiveVMAccess;
		}
		systemReturnCode = vmFuncs->queryLogOptions(vm, STRING_BUFFER_SIZE, syslogOptions, &syslogBufferSize);
		if (JVMTI_ERROR_NONE != systemReturnCode) {
			currentExceptionClass = vm->criuJVMCheckpointExceptionClass;
			nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_JVMTI_COM_IBM_LOG_QUERY_OPT_ERROR, NULL);
			j9mem_free_memory(syslogOptions);
			goto wakeJavaThreadsWithExclusiveVMAccess;
		}
		Trc_CRIU_checkpointJVMImpl_syslogOptions(currentThread, syslogOptions);
		if (0 != strcmp(syslogOptions, "none")) {
			/* Not -Xsyslog:none, close the system logger handle before checkpoint. */
			j9port_control(OMRPORT_CTLDATA_SYSLOG_CLOSE, 0);
			syslogFlagNone = FALSE;
		}

		malloc_trim(0);
		VM_VMHelpers::setVMState(currentThread, J9VMSTATE_CRIU_SUPPORT_CHECKPOINT_PHASE_END);
		systemReturnCode = criu_dump();
		VM_VMHelpers::setVMState(currentThread, J9VMSTATE_CRIU_SUPPORT_RESTORE_PHASE_START);
		if (!syslogFlagNone) {
			/* Re-open the system logger, and set options with saved string value. */
			j9port_control(J9PORT_CTLDATA_SYSLOG_OPEN, 0);
			vmFuncs->setLogOptions(vm, syslogOptions);
		}
		j9mem_free_memory(syslogOptions);
		if (systemReturnCode < 0) {
			currentExceptionClass = vm->criuSystemCheckpointExceptionClass;
			nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_JCL_CRIU_DUMP_FAILED, NULL);
			goto wakeJavaThreadsWithExclusiveVMAccess;
		}

		restoreNanoUTCTime = j9time_current_time_nanos(&success);
		if (0 == success) {
			systemReturnCode = errno;
			currentExceptionClass = vm->criuRestoreExceptionClass;
			nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_JCL_CRIU_J9_CURRENT_TIME_NANOS_FAILURE, NULL);
			goto wakeJavaThreadsWithExclusiveVMAccess;
		}
		restoreNanoTimeMonotonic = j9time_nano_time();
		/* JVM downtime between checkpoint and restore is calculated with j9time_current_time_nanos()
		 * which is expected to be accurate in scenarios such as host rebooting, CRIU image moving across timezones.
		 */
		vm->checkpointState.checkpointRestoreTimeDelta = (I_64)(restoreNanoUTCTime - checkpointNanoUTCTime);
		if (vm->checkpointState.checkpointRestoreTimeDelta < 0) {
			/* A negative value was calculated for checkpointRestoreTimeDelta,
			 * Trc_CRIU_before_checkpoint & Trc_CRIU_after_checkpoint can be used for further investigation.
			 * Currently OpenJ9 CRIU only supports 64-bit systems, and IDATA is equivalent to int64_t here.
			 */
			systemReturnCode = (IDATA)vm->checkpointState.checkpointRestoreTimeDelta;
			currentExceptionClass = vm->criuRestoreExceptionClass;
			nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
					J9NLS_JCL_CRIU_NEGATIVE_CHECKPOINT_RESTORE_TIME_DELTA, NULL);
			goto wakeJavaThreadsWithExclusiveVMAccess;
		}
		/* j9time_nano_time() might be based on a different starting point in scenarios
		 * such as host rebooting, CRIU image moving across timezones.
		 * The adjustment calculated below is expected to be same as checkpointRestoreTimeDelta
		 * if there is no change for j9time_nano_time() start point.
		 * This value might be negative.
		 */
		portLibrary->nanoTimeMonotonicClockDelta = restoreNanoTimeMonotonic - checkpointNanoTimeMonotonic;
		Trc_CRIU_after_checkpoint(currentThread, restoreNanoUTCTime, checkpointNanoUTCTime, vm->checkpointState.checkpointRestoreTimeDelta,
			restoreNanoTimeMonotonic, checkpointNanoTimeMonotonic, portLibrary->nanoTimeMonotonicClockDelta);

		/* We can only end up here if the CRIU restore was successful */
		isAfterCheckpoint = TRUE;

		VM_VMHelpers::setVMState(currentThread, J9VMSTATE_CRIU_SUPPORT_RESTORE_PHASE_JAVA_HOOKS);

		/* Run internal restore hooks, and cleanup */
		if (FALSE == vmFuncs->runInternalJVMRestoreHooks(currentThread)) {
			currentExceptionClass = vm->criuRestoreExceptionClass;
			nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
				J9NLS_JCL_CRIU_FAILED_TO_RUN_INTERNAL_RESTORE_HOOKS, NULL);
			goto wakeJavaThreadsWithExclusiveVMAccess;
		}

		releaseSafeOrExcusiveVMAccess(currentThread, vmFuncs, safePoint);

		VM_VMHelpers::setVMState(currentThread, J9VMSTATE_CRIU_SUPPORT_RESTORE_PHASE_INTERNAL_HOOKS);

		if (FALSE == vmFuncs->jvmRestoreHooks(currentThread)) {
			/* throw the pending exception */
			goto wakeJavaThreads;
		}

		if (FALSE == vmFuncs->runDelayedLockRelatedOperations(currentThread)) {
			currentExceptionClass = vm->criuRestoreExceptionClass;
			systemReturnCode = 0;
			nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
					J9NLS_JCL_CRIU_FAILED_DELAY_LOCK_RELATED_OPS, NULL);
		}

wakeJavaThreads:
		acquireSafeOrExcusiveVMAccess(currentThread, vmFuncs, safePoint);

wakeJavaThreadsWithExclusiveVMAccess:

		vm->extendedRuntimeFlags2 &= ~J9_EXTENDED_RUNTIME2_CRIU_SINGLE_THREAD_MODE;

		toggleSuspendOnJavaThreads(currentThread, FALSE);

		releaseSafeOrExcusiveVMAccess(currentThread, vmFuncs, safePoint);

		VM_VMHelpers::setVMState(currentThread, J9VMSTATE_CRIU_SUPPORT_RESTORE_PHASE_END);
closeWorkDirFD:
		if ((0 != close(workDirFD)) && (NULL == currentExceptionClass)) {
			systemReturnCode = errno;
			if (isAfterCheckpoint) {
				currentExceptionClass = vm->criuRestoreExceptionClass;
			} else {
				currentExceptionClass = vm->criuSystemCheckpointExceptionClass;
			}
			nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_JCL_CRIU_FAILED_TO_CLOSE_WORK_DIR, NULL);
		}
closeDirFD:
		if ((0 != close(dirFD)) && (NULL == currentExceptionClass)) {
			systemReturnCode = errno;
			if (isAfterCheckpoint) {
				currentExceptionClass = vm->criuRestoreExceptionClass;
			} else {
				currentExceptionClass = vm->criuSystemCheckpointExceptionClass;
			}
			nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_JCL_CRIU_FAILED_TO_CLOSE_DIR, NULL);
		}
freeWorkDir:
		if (workDirBuf != workDirChars) {
			j9mem_free_memory(workDirChars);
		}
freeLog:
		if (logFileBuf != logFileChars) {
			j9mem_free_memory(logFileChars);
		}
freeDir:
		if (directoryBuf != directoryChars) {
			j9mem_free_memory(directoryChars);
		}

		VM_VMHelpers::setVMState(currentThread, oldVMState);
		vmFuncs->internalExitVMToJNI(currentThread);
#endif /* defined(LINUX) */
	}

	/*
	 * Pending exceptions will be set by the JVM hooks, these exception will take precedence.
	 */
	if ((NULL != currentExceptionClass) && (NULL == currentThread->currentException)) {
		msgCharLength = j9str_printf(PORTLIB, NULL, 0, nlsMsgFormat, systemReturnCode);
		exceptionMsg = (char*) j9mem_allocate_memory(msgCharLength, J9MEM_CATEGORY_VM);

		j9str_printf(PORTLIB, exceptionMsg, msgCharLength, nlsMsgFormat, systemReturnCode);

		jmethodID init = NULL;
		if (vm->criuJVMCheckpointExceptionClass == currentExceptionClass) {
			init = vm->criuJVMCheckpointExceptionInit;
		} else if (vm->criuSystemCheckpointExceptionClass == currentExceptionClass) {
			init = vm->criuSystemCheckpointExceptionInit;
		} else {
			init = vm->criuRestoreExceptionInit;
		}
		jstring jExceptionMsg = env->NewStringUTF(exceptionMsg);

		if (JNI_FALSE == env->ExceptionCheck()) {
			jobject exception = env->NewObject(currentExceptionClass, init, jExceptionMsg, (jint)systemReturnCode);
			if (NULL != exception) {
				env->Throw((jthrowable)exception);
			}
		}

		if (NULL != exceptionMsg) {
			j9mem_free_memory(exceptionMsg);
		}
	}

	vm->checkpointState.checkpointThread = NULL;

	Trc_CRIU_checkpointJVMImpl_Exit(currentThread);
}

} /* extern "C" */
