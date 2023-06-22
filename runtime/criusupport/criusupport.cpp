/*******************************************************************************
 * Copyright IBM Corp. and others 2021
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
#if defined(LINUX)
#include <fcntl.h>
#include <errno.h>
#include <dlfcn.h>
#include <malloc.h>
#endif /* defined(LINUX) */

extern "C" {

#include "criusupport.hpp"
#include "VMHelpers.hpp"

#include "j9.h"
#include "j9jclnls.h"
#include "jni.h"
#include "jvminit.h"
#include "jvmtinls.h"
#include "omrlinkedlist.h"
#include "omrthread.h"
#include "omrutil.h"
#include "ut_j9criu.h"
#include "util_api.h"
#include "vmargs_api.h"

#define STRING_BUFFER_SIZE 256
#define ENV_FILE_BUFFER 1024

#define RESTORE_ARGS_RETURN_OK 0
#define RESTORE_ARGS_RETURN_OOM 1
#define RESTORE_ARGS_RETURN_OPTIONS_FILE_FAILED 2
#define RESTORE_ARGS_RETURN_ENV_VAR_FILE_FAILED 3

#define J9VM_DELAYCHECKPOINT_NOTCHECKPOINTSAFE 0x1
#define J9VM_DELAYCHECKPOINT_CLINIT 0x2

#define OPENJ9_RESTORE_OPTS_VAR "OPENJ9_RESTORE_JAVA_OPTIONS="

static bool
setupJNIFieldIDsAndCRIUAPI(JNIEnv *env, jclass *currentExceptionClass, IDATA *systemReturnCode, const char **nlsMsgFormat)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	jclass criuJVMCheckpointExceptionClass = NULL;
	jclass criuSystemCheckpointExceptionClass = NULL;
	jclass criuJVMRestoreExceptionClass = NULL;
	jclass criuSystemRestoreExceptionClass = NULL;
	bool returnCode = true;
	PORT_ACCESS_FROM_VMC(currentThread);
	IDATA libCRIUReturnCode = 0;

	UT_MODULE_LOADED(J9_UTINTERFACE_FROM_VM(vm));

	criuJVMCheckpointExceptionClass = env->FindClass("org/eclipse/openj9/criu/JVMCheckpointException");
	Assert_CRIU_notNull(criuJVMCheckpointExceptionClass);
	vm->checkpointState.criuJVMCheckpointExceptionClass = (jclass) env->NewGlobalRef(criuJVMCheckpointExceptionClass);

	vm->checkpointState.criuJVMCheckpointExceptionInit = env->GetMethodID(criuJVMCheckpointExceptionClass, "<init>", "(Ljava/lang/String;I)V");

	criuSystemCheckpointExceptionClass = env->FindClass("org/eclipse/openj9/criu/SystemCheckpointException");
	Assert_CRIU_notNull(criuSystemCheckpointExceptionClass);
	vm->checkpointState.criuSystemCheckpointExceptionClass = (jclass) env->NewGlobalRef(criuSystemCheckpointExceptionClass);

	vm->checkpointState.criuSystemCheckpointExceptionInit = env->GetMethodID(criuSystemCheckpointExceptionClass, "<init>", "(Ljava/lang/String;I)V");

	criuJVMRestoreExceptionClass = env->FindClass("org/eclipse/openj9/criu/JVMRestoreException");
	Assert_CRIU_notNull(criuJVMRestoreExceptionClass);
	vm->checkpointState.criuJVMRestoreExceptionClass = (jclass) env->NewGlobalRef(criuJVMRestoreExceptionClass);

	vm->checkpointState.criuJVMRestoreExceptionInit = env->GetMethodID(criuJVMRestoreExceptionClass, "<init>", "(Ljava/lang/String;I)V");

	criuSystemRestoreExceptionClass = env->FindClass("org/eclipse/openj9/criu/SystemRestoreException");
	Assert_CRIU_notNull(criuSystemRestoreExceptionClass);
	vm->checkpointState.criuSystemRestoreExceptionClass = (jclass) env->NewGlobalRef(criuSystemRestoreExceptionClass);

	vm->checkpointState.criuSystemRestoreExceptionInit = env->GetMethodID(criuSystemRestoreExceptionClass, "<init>", "(Ljava/lang/String;I)V");

	if ((NULL == vm->checkpointState.criuSystemRestoreExceptionInit)
		|| (NULL == vm->checkpointState.criuJVMRestoreExceptionInit)
		|| (NULL == vm->checkpointState.criuSystemCheckpointExceptionInit)
		|| (NULL == vm->checkpointState.criuJVMCheckpointExceptionInit)
	) {
		/* pending exception already set */
		returnCode = false;
		goto done;
	}

	if ((NULL == vm->checkpointState.criuJVMCheckpointExceptionClass)
		|| (NULL == vm->checkpointState.criuSystemCheckpointExceptionClass)
		|| (NULL == vm->checkpointState.criuJVMRestoreExceptionClass)
		|| (NULL == vm->checkpointState.criuSystemRestoreExceptionClass)
	) {
		vmFuncs->internalEnterVMFromJNI(currentThread);
		vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
		vmFuncs->internalExitVMToJNI(currentThread);
		returnCode = false;
		goto done;
	}

	libCRIUReturnCode = j9sl_open_shared_library((char*)"criu", &vm->checkpointState.libCRIUHandle, OMRPORT_SLOPEN_DECORATE | OMRPORT_SLOPEN_LAZY);

	if (libCRIUReturnCode != OMRPORT_SL_FOUND) {
		*currentExceptionClass = criuSystemCheckpointExceptionClass;
		*systemReturnCode = libCRIUReturnCode;
		*nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_JCL_CRIU_LOADING_LIBCRIU_FAILED, NULL);
		returnCode = false;
		goto done;
	}

	/* the older criu libraries do not contain the criu_set_unprivileged function so we can do a NULL check before calling it */
	j9sl_lookup_name(vm->checkpointState.libCRIUHandle, (char*)"criu_set_unprivileged", (UDATA *)&vm->checkpointState.criuSetUnprivilegedFunctionPointerType, "Z");

	if (j9sl_lookup_name(vm->checkpointState.libCRIUHandle, (char*)"criu_set_images_dir_fd", (UDATA *)&vm->checkpointState.criuSetImagesDirFdFunctionPointerType, "P")
		|| j9sl_lookup_name(vm->checkpointState.libCRIUHandle, (char*)"criu_set_shell_job", (UDATA *)&vm->checkpointState.criuSetShellJobFunctionPointerType, "Z")
		|| j9sl_lookup_name(vm->checkpointState.libCRIUHandle, (char*)"criu_set_log_level", (UDATA *)&vm->checkpointState.criuSetLogLevelFunctionPointerType, "I")
		|| j9sl_lookup_name(vm->checkpointState.libCRIUHandle, (char*)"criu_set_log_file", (UDATA *)&vm->checkpointState.criuSetLogFileFunctionPointerType, "P")
		|| j9sl_lookup_name(vm->checkpointState.libCRIUHandle, (char*)"criu_set_leave_running", (UDATA *)&vm->checkpointState.criuSetLeaveRunningFunctionPointerType, "Z")
		|| j9sl_lookup_name(vm->checkpointState.libCRIUHandle, (char*)"criu_set_ext_unix_sk", (UDATA *)&vm->checkpointState.criuSetExtUnixSkFunctionPointerType, "Z")
		|| j9sl_lookup_name(vm->checkpointState.libCRIUHandle, (char*)"criu_set_file_locks", (UDATA *)&vm->checkpointState.criuSetFileLocksFunctionPointerType, "Z")
		|| j9sl_lookup_name(vm->checkpointState.libCRIUHandle, (char*)"criu_set_tcp_established", (UDATA *)&vm->checkpointState.criuSetTcpEstablishedFunctionPointerType, "Z")
		|| j9sl_lookup_name(vm->checkpointState.libCRIUHandle, (char*)"criu_set_auto_dedup", (UDATA *)&vm->checkpointState.criuSetAutoDedupFunctionPointerType, "Z")
		|| j9sl_lookup_name(vm->checkpointState.libCRIUHandle, (char*)"criu_set_track_mem", (UDATA *)&vm->checkpointState.criuSetTrackMemFunctionPointerType, "Z")
		|| j9sl_lookup_name(vm->checkpointState.libCRIUHandle, (char*)"criu_set_work_dir_fd", (UDATA *)&vm->checkpointState.criuSetWorkDirFdFunctionPointerType, "P")
		|| j9sl_lookup_name(vm->checkpointState.libCRIUHandle, (char*)"criu_init_opts", (UDATA *)&vm->checkpointState.criuInitOptsFunctionPointerType, "V")
		|| j9sl_lookup_name(vm->checkpointState.libCRIUHandle, (char*)"criu_dump", (UDATA *)&vm->checkpointState.criuDumpFunctionPointerType, "V")
	) {
		*currentExceptionClass = criuSystemCheckpointExceptionClass;
		*systemReturnCode = 1;
		*nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_JCL_CRIU_LOADING_LIBCRIU_FUNCTIONS_FAILED, NULL);
		returnCode = false;
		goto done;
	}

	if ((FIND_ARG_IN_VMARGS(STARTSWITH_MATCH, MAPOPT_AGENTLIB_JDWP_EQUALS, NULL) >= 0)
		|| (FIND_ARG_IN_VMARGS(STARTSWITH_MATCH, MAPOPT_XRUNJDWP, NULL) >= 0)
	) {
		vm->checkpointState.flags |= J9VM_CRIU_IS_JDWP_ENABLED;
	}

done:
	return returnCode;
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
 * Checks if a thread should be suspended or resumed for checkpoint/restore. If JDWP is not
 * enabled, then all threads will return true. Otherwise, if toggleDebugThreads is true, then all
 * JDWP debug threads will be toggled. If toggleDebugThreads is false, then all non-debug threads
 * will be toggled.
 *
 * @param[in] currentThread the thread being checked
 * @param[in] toggleDebugThreads if only JWDP debug threads should be toggled
 *
 * @return true if the thread should be suspended or resumed, false if not
 */
static bool
shouldToggleJavaThread(J9VMThread *currentThread, BOOLEAN toggleDebugThreads)
{
	J9JavaVM *vm = currentThread->javaVM;
	bool result = true;
	if (J9_ARE_ALL_BITS_SET(vm->checkpointState.flags, J9VM_CRIU_IS_JDWP_ENABLED)) {
		char *threadName = getOMRVMThreadName(currentThread->omrVMThread);
		releaseOMRVMThreadName(currentThread->omrVMThread);
		/* all threads started by JDWP begin with "JDWP" in their name */
		bool isJdwpThread = 0 == strncmp("JDWP", threadName, 4);
		result = (toggleDebugThreads) ? isJdwpThread : !isJdwpThread;
	}
	return result;
}

/**
 * Suspends or resumes Java threads for checkpoint and restore.
 * Caller must first acquire exclusive safepoint or exclusive VMAccess.
 *
 * @param[in] suspend set to true to suspend threads, and false to resume them
 * @param[in] toggleDebugThreads only useful if JDWP is enabled, determines whether to toggle
 * the debug threads or all other threads
 */
static void
toggleSuspendOnJavaThreads(J9VMThread *currentThread, BOOLEAN suspend, BOOLEAN toggleDebugThreads)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;

	Assert_CRIU_true((J9_XACCESS_EXCLUSIVE == vm->exclusiveAccessState) || (J9_XACCESS_EXCLUSIVE == vm->safePointState));

	J9VMThread *walkThread = J9_LINKED_LIST_START_DO(vm->mainThread);
	while (NULL != walkThread) {
		if (VM_VMHelpers::threadCanRunJavaCode(walkThread)
		&& (currentThread != walkThread)
		) {
			if (shouldToggleJavaThread(walkThread, toggleDebugThreads)) {
				if (suspend) {
					vmFuncs->setHaltFlag(walkThread, J9_PUBLIC_FLAGS_HALT_THREAD_FOR_CHECKPOINT);
				} else {
					vmFuncs->clearHaltFlag(walkThread, J9_PUBLIC_FLAGS_HALT_THREAD_FOR_CHECKPOINT);
				}
			}
		}
		walkThread = J9_LINKED_LIST_NEXT_DO(vm->mainThread, walkThread);
	}
}

static UDATA
notCheckpointSafeOrClinitFrameWalkFunction(J9VMThread *vmThread, J9StackWalkState *walkState)
{
	J9Method *method = walkState->method;
	UDATA returnCode = J9_STACKWALK_KEEP_ITERATING;

	if (NULL != method) {
		J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
		J9ClassLoader *methodLoader = J9_CLASS_FROM_METHOD(method)->classLoader;
		J9UTF8 *romMethodName = J9ROMMETHOD_NAME(romMethod);
		/* only method names that start with '<' are <init>, <vnew> and <clinit>  */
		if (0 == strncmp((char*)J9UTF8_DATA(romMethodName), "<c", 2)) {
			*(UDATA*)walkState->userData1 = J9VM_DELAYCHECKPOINT_CLINIT;
			goto fail;
		}

		/* we only enforce this in methods loaded by the bootloader */
		if (methodLoader == vmThread->javaVM->systemClassLoader) {
			if (J9ROMMETHOD_HAS_EXTENDED_MODIFIERS(romMethod)) {
				U_32 extraModifiers = getExtendedModifiersDataFromROMMethod(romMethod);
				if (J9ROMMETHOD_HAS_NOT_CHECKPOINT_SAFE_ANNOTATION(extraModifiers)) {
					*(UDATA*)walkState->userData1 = J9VM_DELAYCHECKPOINT_NOTCHECKPOINTSAFE;
					goto fail;
				}
			}
		}
	}

done:
	return returnCode;

fail:
	walkState->userData2 = (void *)vmThread;
	walkState->userData3 = (void *)method;
	returnCode = J9_STACKWALK_STOP_ITERATING;
	goto done;
}

static bool
checkIfSafeToCheckpoint(J9VMThread *currentThread)
{
	UDATA notSafeToCheckpoint = 0;
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
			walkState.userData1 = (void *)&notSafeToCheckpoint;
			walkState.frameWalkFunction = notCheckpointSafeOrClinitFrameWalkFunction;

			vm->walkStackFrames(walkThread, &walkState);
			if (0 != notSafeToCheckpoint) {
				Trc_CRIU_checkpointJVMImpl_checkIfSafeToCheckpointBlockedVer2(currentThread, walkState.userData2, walkState.userData3, *(UDATA*)walkState.userData1);
				break;
			}
		}
		walkThread = J9_LINKED_LIST_NEXT_DO(vm->mainThread, walkThread);
	}

	return notSafeToCheckpoint;
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

/**
 * Read buffer pointed to by cursor and add replace the characters sequences "\r\n"
 * and "\n" with a null terminator "\0". The input buffer must be NULL terminated.
 *
 * @param cursor pointer to buffer
 *
 * @return cursor position at the end of the line
 */
static char*
endLine(char *cursor)
{
	while(true) {
		switch (*cursor) {
		case '\r':
			cursor[0] = '\0';
			if (cursor[1] == '\n') {
				cursor++;
				cursor[0] = '\0';
			}
			goto done;
		case '\n':
			cursor[0] = '\0';
			goto done;
		case '\0':
			goto done;
		default:
			break;
		}
		cursor++;
	}
done:
	return cursor;
}

static UDATA
parseEnvVarsForOptions(J9VMThread *currentThread, J9JavaVMArgInfoList *vmArgumentsList, char *envFileChars, I_64 fileLength)
{
	UDATA returnCode = RESTORE_ARGS_RETURN_OK;
	char *cursor = envFileChars;
	char *optString = NULL;
	bool keepSearching = false;
	IDATA result = 0;

	do {
		keepSearching = false;

		optString = strstr(cursor, OPENJ9_RESTORE_OPTS_VAR);

		if (NULL != optString) {
			/* Check that variable name is not a substring of another variable */
			if (envFileChars != optString) {
				char preceedingChar = *(optString - 1);
				if ('\n' != preceedingChar) {
					keepSearching = true;
				}
			}
			optString += LITERAL_STRLEN(OPENJ9_RESTORE_OPTS_VAR);
			cursor = optString;
		}
	} while (keepSearching);

	if (NULL != optString) {
		PORT_ACCESS_FROM_VMC(currentThread);
		UDATA len = endLine(optString) - optString;
		/* +1 for the \0 */
		len++;
		char *restoreArgsChars = (char*) j9mem_allocate_memory(len, OMRMEM_CATEGORY_VM);

		if (NULL == restoreArgsChars) {
			returnCode = RESTORE_ARGS_RETURN_OOM;
			goto done;
		}

		memcpy(restoreArgsChars, optString, len);
		currentThread->javaVM->checkpointState.restoreArgsChars = restoreArgsChars;

		/* parseOptionsBuffer() will free buffer if no options were added, otherwise free at shutdown */
		result = parseOptionsBuffer(currentThread->javaVM->portLibrary, restoreArgsChars, vmArgumentsList, 0, FALSE);
		if (0 == result) {
			 /* no options found, buffer free'd */
			currentThread->javaVM->checkpointState.restoreArgsChars = NULL;
		} else if (result < 0) {
			j9mem_free_memory(restoreArgsChars);
			currentThread->javaVM->checkpointState.restoreArgsChars = NULL;
			returnCode = RESTORE_ARGS_RETURN_ENV_VAR_FILE_FAILED;
		}
	}
done:
	return returnCode;
}

static UDATA
loadRestoreArgsFromEnvVariable(J9VMThread *currentThread, J9JavaVMArgInfoList *vmArgumentsList, char *envFile)
{
	PORT_ACCESS_FROM_VMC(currentThread);
	IDATA fileDescriptor = -1;
	UDATA returnCode = RESTORE_ARGS_RETURN_OK;
	char envFileBuf[ENV_FILE_BUFFER];
	char *envFileChars = envFileBuf;
	UDATA allocLength = 0;
	IDATA bytesRead = 0;
	IDATA readResult = 0;

	I_64 fileLength = j9file_length(envFile);
	/* Restrict file size to < 2G */
	if (fileLength > J9CONST64(0x7FFFFFFF)) {
		returnCode = RESTORE_ARGS_RETURN_ENV_VAR_FILE_FAILED;
		goto done;
	}

	if ((fileDescriptor = j9file_open(envFile, EsOpenRead, 0)) == -1) {
		returnCode = RESTORE_ARGS_RETURN_ENV_VAR_FILE_FAILED;
		goto done;
	}

	allocLength = fileLength + 1;
	if (allocLength > ENV_FILE_BUFFER) {
		envFileChars = (char*) j9mem_allocate_memory(allocLength, OMRMEM_CATEGORY_VM);
		if (NULL == envFileChars) {
			returnCode = RESTORE_ARGS_RETURN_OOM;
			goto close;
		}
	}

	/* read all data in a loop */
	while (bytesRead < fileLength) {
		readResult = j9file_read(fileDescriptor, envFileChars + bytesRead, (IDATA)(fileLength - bytesRead));
		if (-1 == readResult) {
			returnCode = RESTORE_ARGS_RETURN_ENV_VAR_FILE_FAILED;
			goto freeEnvFile;
		}
		bytesRead += readResult;
	}
	envFileChars[fileLength] = '\0';

	returnCode = parseEnvVarsForOptions(currentThread, vmArgumentsList, envFileChars, fileLength);

freeEnvFile:
	if (envFileBuf != envFileChars) {
		j9mem_free_memory(envFileChars);
	}
close:
	j9file_close(fileDescriptor);
done:
	return returnCode;
}

static void
traceRestoreArgs(J9VMThread *currentThread, J9VMInitArgs *restoreArgs)
{
	JavaVMInitArgs *args = restoreArgs->actualVMArgs;

	for (IDATA i = 0; i < args->nOptions; i++) {
		Trc_CRIU_restoreArg(currentThread, args->options[i].optionString);
	}
}

static UDATA
loadRestoreArguments(J9VMThread *currentThread, const char *optionsFile, char *envFile)
{
	UDATA result = RESTORE_ARGS_RETURN_OK;
	J9JavaVM *vm = currentThread->javaVM;
	J9JavaVMArgInfoList vmArgumentsList;
	UDATA ignored = 0;
	J9VMInitArgs *previousArgs = vm->checkpointState.restoreArgsList;

	vmArgumentsList.pool = pool_new(sizeof(J9JavaVMArgInfo), 0, 0, 0, J9_GET_CALLSITE(), J9MEM_CATEGORY_CLASSES, POOL_FOR_PORT(vm->portLibrary));
	if (NULL == vmArgumentsList.pool) {
		vm->internalVMFunctions->setNativeOutOfMemoryError(currentThread, 0, 0);
		result = RESTORE_ARGS_RETURN_OOM;
		goto done;
	}
	vmArgumentsList.head = NULL;
	vmArgumentsList.tail = NULL;

	if (NULL != optionsFile) {
		/* addXOptionsFile() adds the options file name to the args list, if its null it adds a null character to the args list */
		if (0 != addXOptionsFile(vm->portLibrary, optionsFile, &vmArgumentsList, 0)) {
			result = RESTORE_ARGS_RETURN_OPTIONS_FILE_FAILED;
			goto done;
		}
	}

	if (NULL != envFile) {
		result = loadRestoreArgsFromEnvVariable(currentThread, &vmArgumentsList, envFile);
		if (RESTORE_ARGS_RETURN_OK != result) {
			goto done;
		}
	}

	vm->checkpointState.restoreArgsList = createJvmInitArgs(vm->portLibrary, vm->vmArgsArray->actualVMArgs, &vmArgumentsList, &ignored);
	vm->checkpointState.restoreArgsList->previousArgs = previousArgs;

	if (TrcEnabled_Trc_CRIU_restoreArg) {
		traceRestoreArgs(currentThread, vm->checkpointState.restoreArgsList);
	}
done:
	pool_kill(vmArgumentsList.pool);
	return result;
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
		jboolean unprivileged,
		jstring optionsFile,
		jstring environmentFile)
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
	const char *dlerrorReturnString = NULL;
	bool setupCRIU = true;
	PORT_ACCESS_FROM_VMC(currentThread);

	if (NULL == vm->checkpointState.criuJVMCheckpointExceptionClass) {
		setupCRIU = setupJNIFieldIDsAndCRIUAPI(env, &currentExceptionClass, &systemReturnCode, &nlsMsgFormat);
	}

	vm->checkpointState.checkpointThread = currentThread;

	Trc_CRIU_checkpointJVMImpl_Entry(currentThread);
	if (vmFuncs->isCheckpointAllowed(currentThread) && setupCRIU) {
#if defined(LINUX)
		j9object_t cpDir = NULL;
		j9object_t log = NULL;
		j9object_t wrkDir = NULL;
		j9object_t optFile = NULL;
		j9object_t envFile = NULL;
		IDATA dirFD = 0;
		IDATA workDirFD = 0;
		char directoryBuf[STRING_BUFFER_SIZE];
		char *directoryChars = directoryBuf;
		char logFileBuf[STRING_BUFFER_SIZE];
		char *logFileChars = logFileBuf;
		char workDirBuf[STRING_BUFFER_SIZE];
		char *workDirChars = workDirBuf;
		char optionsFileBuf[STRING_BUFFER_SIZE];
		char *optionsFileChars = optionsFileBuf;
		char envFileBuf[STRING_BUFFER_SIZE];
		char *envFileChars = envFileBuf;
		BOOLEAN isAfterCheckpoint = FALSE;
		I_64 checkpointNanoTimeMonotonic = 0;
		I_64 restoreNanoTimeMonotonic = 0;
		U_64 checkpointNanoUTCTime = 0;
		U_64 restoreNanoUTCTime = 0;
		UDATA success = 0;
		bool safePoint = J9_ARE_ANY_BITS_SET(vm->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_OSR_SAFE_POINT);
		UDATA maxRetries = vm->checkpointState.maxRetryForNotCheckpointSafe;
		BOOLEAN syslogFlagNone = TRUE;
		char *syslogOptions = NULL;
		I_32 syslogBufferSize = 0;
		UDATA oldVMState = VM_VMHelpers::setVMState(currentThread, J9VMSTATE_CRIU_SUPPORT_CHECKPOINT_PHASE_START);
		UDATA notSafeToCheckpoint = 0;

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
			currentExceptionClass = vm->checkpointState.criuJVMCheckpointExceptionClass;
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
				currentExceptionClass = vm->checkpointState.criuJVMCheckpointExceptionClass;
				nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_JCL_CRIU_FAILED_TO_CONVERT_JAVA_STRING, NULL);
				goto freeLog;
			}
		}

		if (NULL != optionsFile) {
			optFile = J9_JNI_UNWRAP_REFERENCE(optionsFile);
			systemReturnCode = getNativeString(currentThread, optFile, &optionsFileChars, STRING_BUFFER_SIZE);
			switch (systemReturnCode) {
			case J9_NATIVE_STRING_NO_ERROR:
				break;
			case J9_NATIVE_STRING_OUT_OF_MEMORY:
				vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
				goto freeOptionsFile;
			case J9_NATIVE_STRING_FAIL_TO_CONVERT:
				currentExceptionClass = vm->checkpointState.criuJVMCheckpointExceptionClass;
				nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_JCL_CRIU_FAILED_TO_CONVERT_JAVA_STRING, NULL);
				goto freeOptionsFile;
			}
		} else {
			optionsFileChars = NULL;
		}

		if (NULL != environmentFile) {
			envFile = J9_JNI_UNWRAP_REFERENCE(environmentFile);
			systemReturnCode = getNativeString(currentThread, envFile, &envFileChars, STRING_BUFFER_SIZE);
			switch (systemReturnCode) {
			case J9_NATIVE_STRING_NO_ERROR:
				break;
			case J9_NATIVE_STRING_OUT_OF_MEMORY:
				vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
				goto freeEnvFile;
			case J9_NATIVE_STRING_FAIL_TO_CONVERT:
				currentExceptionClass = vm->checkpointState.criuJVMCheckpointExceptionClass;
				nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_JCL_CRIU_FAILED_TO_CONVERT_JAVA_STRING, NULL);
				goto freeEnvFile;
			}
		} else {
			envFileChars = NULL;
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
				currentExceptionClass = vm->checkpointState.criuJVMCheckpointExceptionClass;
				nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_JCL_CRIU_FAILED_TO_CONVERT_JAVA_STRING, NULL);
				goto freeWorkDir;
			}
		}

		dirFD = open(directoryChars, O_DIRECTORY);
		if (dirFD < 0) {
			systemReturnCode = errno;
			currentExceptionClass = vm->checkpointState.criuJVMCheckpointExceptionClass;
			nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_JCL_CRIU_FAILED_TO_OPEN_DIR, NULL);
			goto freeWorkDir;
		}

		if (NULL != workDir) {
			workDirFD = open(workDirChars, O_DIRECTORY);
			if (workDirFD < 0) {
				systemReturnCode = errno;
				currentExceptionClass = vm->checkpointState.criuJVMCheckpointExceptionClass;
				nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_JCL_CRIU_FAILED_TO_OPEN_WORK_DIR, NULL);
				goto closeDirFD;
			}
		}

		systemReturnCode = vm->checkpointState.criuInitOptsFunctionPointerType();
		if (0 != systemReturnCode) {
			currentExceptionClass = vm->checkpointState.criuSystemCheckpointExceptionClass;
			nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_JCL_CRIU_INIT_FAILED, NULL);
			goto closeWorkDirFD;
		}

		if (JNI_TRUE == unprivileged) {
			if (NULL != vm->checkpointState.criuSetUnprivilegedFunctionPointerType) {
				systemReturnCode = J9_CRIU_UNPRIVILEGED_NO_ERROR;
				vm->checkpointState.criuSetUnprivilegedFunctionPointerType(JNI_FALSE != unprivileged);
			} else {
				currentExceptionClass = vm->checkpointState.criuSystemCheckpointExceptionClass;
				systemReturnCode = (NULL != dlerrorReturnString) ? J9_CRIU_UNPRIVILEGED_DLSYM_ERROR : J9_CRIU_UNPRIVILEGED_DLSYM_NULL_SYMBOL;
				nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_JCL_CRIU_CANNOT_SET_UNPRIVILEGED, NULL);
				goto closeWorkDirFD;
			}
		}

		vm->checkpointState.criuSetImagesDirFdFunctionPointerType(dirFD);
		vm->checkpointState.criuSetShellJobFunctionPointerType(JNI_FALSE != shellJob);
		if (logLevel > 0) {
			vm->checkpointState.criuSetLogLevelFunctionPointerType((int)logLevel);
		}
		if (NULL != logFile) {
			vm->checkpointState.criuSetLogFileFunctionPointerType(logFileChars);
		}
		vm->checkpointState.criuSetLeaveRunningFunctionPointerType(JNI_FALSE != leaveRunning);
		vm->checkpointState.criuSetExtUnixSkFunctionPointerType(JNI_FALSE != extUnixSupport);
		vm->checkpointState.criuSetFileLocksFunctionPointerType(JNI_FALSE != fileLocks);
		vm->checkpointState.criuSetTcpEstablishedFunctionPointerType(JNI_FALSE != tcpEstablished);
		vm->checkpointState.criuSetAutoDedupFunctionPointerType(JNI_FALSE != autoDedup);
		vm->checkpointState.criuSetTrackMemFunctionPointerType(JNI_FALSE != trackMemory);

		if (NULL != workDir) {
			vm->checkpointState.criuSetWorkDirFdFunctionPointerType(workDirFD);
		}

		acquireSafeOrExcusiveVMAccess(currentThread, vmFuncs, safePoint);

		notSafeToCheckpoint = checkIfSafeToCheckpoint(currentThread);

		for (UDATA i = 0; (0 != notSafeToCheckpoint) && (i <= maxRetries); i++) {
			releaseSafeOrExcusiveVMAccess(currentThread, vmFuncs, safePoint);
			vmFuncs->internalExitVMToJNI(currentThread);
			omrthread_nanosleep(10000);
			vmFuncs->internalEnterVMFromJNI(currentThread);
			acquireSafeOrExcusiveVMAccess(currentThread, vmFuncs, safePoint);
			notSafeToCheckpoint = checkIfSafeToCheckpoint(currentThread);
		}

		if ((J9VM_DELAYCHECKPOINT_NOTCHECKPOINTSAFE == notSafeToCheckpoint)
			|| ((J9VM_DELAYCHECKPOINT_CLINIT == notSafeToCheckpoint) && J9_ARE_ALL_BITS_SET(vm->checkpointState.flags, J9VM_CRIU_IS_THROW_ON_DELAYED_CHECKPOINT_ENABLED))
		) {
			releaseSafeOrExcusiveVMAccess(currentThread, vmFuncs, safePoint);
			currentExceptionClass = vm->checkpointState.criuJVMCheckpointExceptionClass;
			systemReturnCode = vm->checkpointState.maxRetryForNotCheckpointSafe;
			nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_JCL_CRIU_MAX_RETRY_FOR_NOTCHECKPOINTSAFE_REACHED, NULL);
			goto closeWorkDirFD;
		} else {
			Trc_CRIU_checkpointJVMImpl_checkpointWithActiveCLinit(currentThread);
		}

		toggleSuspendOnJavaThreads(currentThread, TRUE, FALSE);

		vm->extendedRuntimeFlags2 |= J9_EXTENDED_RUNTIME2_CRIU_SINGLE_THREAD_MODE;

		releaseSafeOrExcusiveVMAccess(currentThread, vmFuncs, safePoint);

		VM_VMHelpers::setVMState(currentThread, J9VMSTATE_CRIU_SUPPORT_CHECKPOINT_PHASE_JAVA_HOOKS);

		if (FALSE == vmFuncs->jvmCheckpointHooks(currentThread)) {
			/* throw the pending exception */
			goto wakeJavaThreads;
		}

		/* At this point, Java threads are all finished, and JVM is considered paused before taking the checkpoint. */
		vm->checkpointState.checkpointRestoreTimeDelta = 0;
		portLibrary->nanoTimeMonotonicClockDelta = 0;
		checkpointNanoTimeMonotonic = j9time_nano_time();
		checkpointNanoUTCTime = j9time_current_time_nanos(&success);
		if (0 == success) {
			systemReturnCode = errno;
			currentExceptionClass = vm->checkpointState.criuJVMCheckpointExceptionClass;
			nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_JCL_CRIU_J9_CURRENT_TIME_NANOS_FAILURE, NULL);
			goto wakeJavaThreadsWithExclusiveVMAccess;
		}
		Trc_CRIU_checkpoint_nano_times(currentThread, checkpointNanoTimeMonotonic, checkpointNanoUTCTime);
		TRIGGER_J9HOOK_VM_PREPARING_FOR_CHECKPOINT(vm->hookInterface, currentThread);

		vm->memoryManagerFunctions->j9gc_prepare_for_checkpoint(currentThread);

		acquireSafeOrExcusiveVMAccess(currentThread, vmFuncs, safePoint);

		VM_VMHelpers::setVMState(currentThread, J9VMSTATE_CRIU_SUPPORT_CHECKPOINT_PHASE_INTERNAL_HOOKS);

		/* Run internal checkpoint hooks, after iterating heap objects */
		if (FALSE == vmFuncs->runInternalJVMCheckpointHooks(currentThread, &nlsMsgFormat)) {
			currentExceptionClass = vm->checkpointState.criuJVMCheckpointExceptionClass;
			goto wakeJavaThreadsWithExclusiveVMAccess;
		}

		if (J9_ARE_ALL_BITS_SET(vm->checkpointState.flags, J9VM_CRIU_IS_JDWP_ENABLED)) {
			toggleSuspendOnJavaThreads(currentThread, TRUE, TRUE);
		}

		syslogOptions = (char *)j9mem_allocate_memory(STRING_BUFFER_SIZE, J9MEM_CATEGORY_VM);
		if (NULL == syslogOptions) {
			systemReturnCode = J9_NATIVE_STRING_OUT_OF_MEMORY;
			vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
			goto wakeJavaThreadsWithExclusiveVMAccess;
		}
		systemReturnCode = vmFuncs->queryLogOptions(vm, STRING_BUFFER_SIZE, syslogOptions, &syslogBufferSize);
		if (JVMTI_ERROR_NONE != systemReturnCode) {
			currentExceptionClass = vm->checkpointState.criuJVMCheckpointExceptionClass;
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
		Trc_CRIU_before_checkpoint(currentThread, j9time_nano_time(), j9time_current_time_nanos(&success));
		VM_VMHelpers::setVMState(currentThread, J9VMSTATE_CRIU_SUPPORT_CHECKPOINT_PHASE_END);
		systemReturnCode = vm->checkpointState.criuDumpFunctionPointerType();
		VM_VMHelpers::setVMState(currentThread, J9VMSTATE_CRIU_SUPPORT_RESTORE_PHASE_START);
		restoreNanoTimeMonotonic = j9time_nano_time();
		restoreNanoUTCTime = j9time_current_time_nanos(&success);
		Trc_CRIU_after_checkpoint(currentThread, restoreNanoTimeMonotonic, restoreNanoUTCTime);
		if (!syslogFlagNone) {
			/* Re-open the system logger, and set options with saved string value. */
			j9port_control(J9PORT_CTLDATA_SYSLOG_OPEN, 0);
			vmFuncs->setLogOptions(vm, syslogOptions);
		}
		j9mem_free_memory(syslogOptions);
		if (systemReturnCode < 0) {
			currentExceptionClass = vm->checkpointState.criuSystemCheckpointExceptionClass;
			nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_JCL_CRIU_DUMP_FAILED, NULL);
			goto wakeJavaThreadsWithExclusiveVMAccess;
		}
		if (0 == success) {
			systemReturnCode = errno;
			currentExceptionClass = vm->checkpointState.criuJVMRestoreExceptionClass;
			nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_JCL_CRIU_J9_CURRENT_TIME_NANOS_FAILURE, NULL);
			goto wakeJavaThreadsWithExclusiveVMAccess;
		}
		/* JVM downtime between checkpoint and restore is calculated with j9time_current_time_nanos()
		 * which is expected to be accurate in scenarios such as host rebooting, CRIU image moving across timezones.
		 */
		vm->checkpointState.checkpointRestoreTimeDelta = (I_64)(restoreNanoUTCTime - checkpointNanoUTCTime);
		if (vm->checkpointState.checkpointRestoreTimeDelta < 0) {
			/* A negative value was calculated for checkpointRestoreTimeDelta,
			 * Trc_CRIU_checkpoint_nano_times & Trc_CRIU_restore_nano_times can be used for further investigation.
			 * Currently OpenJ9 CRIU only supports 64-bit systems, and IDATA is equivalent to int64_t here.
			 */
			systemReturnCode = (IDATA)vm->checkpointState.checkpointRestoreTimeDelta;
			currentExceptionClass = vm->checkpointState.criuJVMRestoreExceptionClass;
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
		Trc_CRIU_restore_nano_times(currentThread, restoreNanoUTCTime, checkpointNanoUTCTime, vm->checkpointState.checkpointRestoreTimeDelta,
			restoreNanoTimeMonotonic, checkpointNanoTimeMonotonic, portLibrary->nanoTimeMonotonicClockDelta);

		/* We can only end up here if the CRIU restore was successful */
		isAfterCheckpoint = TRUE;

		switch (loadRestoreArguments(currentThread, optionsFileChars, envFileChars)) {
		case RESTORE_ARGS_RETURN_OPTIONS_FILE_FAILED:
			currentExceptionClass = vm->checkpointState.criuJVMRestoreExceptionClass;
			nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
				J9NLS_JCL_CRIU_LOADING_OPTIONS_FILE_FAILED, NULL);
				/* fallthrough */
		case RESTORE_ARGS_RETURN_OOM:
			/* exception is already set */
			goto wakeJavaThreadsWithExclusiveVMAccess;
		case RESTORE_ARGS_RETURN_OK:
			break;
		}

		VM_VMHelpers::setVMState(currentThread, J9VMSTATE_CRIU_SUPPORT_RESTORE_PHASE_JAVA_HOOKS);

		/* Run internal restore hooks, and cleanup */
		if (FALSE == vmFuncs->runInternalJVMRestoreHooks(currentThread, &nlsMsgFormat)) {
			currentExceptionClass = vm->checkpointState.criuJVMRestoreExceptionClass;
			goto wakeJavaThreadsWithExclusiveVMAccess;
		}

		if (J9_ARE_ALL_BITS_SET(vm->checkpointState.flags, J9VM_CRIU_IS_JDWP_ENABLED)) {
			toggleSuspendOnJavaThreads(currentThread, FALSE, TRUE);
		}

		releaseSafeOrExcusiveVMAccess(currentThread, vmFuncs, safePoint);

		if (FALSE == vm->memoryManagerFunctions->j9gc_reinitialize_for_restore(currentThread, &nlsMsgFormat)) {
			currentExceptionClass = vm->checkpointState.criuJVMRestoreExceptionClass;
			goto wakeJavaThreads;
		}

		VM_VMHelpers::setVMState(currentThread, J9VMSTATE_CRIU_SUPPORT_RESTORE_PHASE_INTERNAL_HOOKS);

		if (FALSE == vmFuncs->jvmRestoreHooks(currentThread)) {
			/* throw the pending exception */
			goto wakeJavaThreads;
		}

		if (FALSE == vmFuncs->runDelayedLockRelatedOperations(currentThread)) {
			currentExceptionClass = vm->checkpointState.criuJVMRestoreExceptionClass;
			systemReturnCode = 0;
			nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
					J9NLS_JCL_CRIU_FAILED_DELAY_LOCK_RELATED_OPS, NULL);
		}

		if (NULL != vm->checkpointState.restoreArgsList) {
			J9VMInitArgs *restoreArgsList = vm->checkpointState.restoreArgsList;
			/* mark -Xoptionsfile= as consumed */
			FIND_AND_CONSUME_ARG(restoreArgsList, STARTSWITH_MATCH, VMOPT_XOPTIONSFILE_EQUALS, NULL);
			IDATA ignoreEnabled = FIND_AND_CONSUME_ARG(restoreArgsList, EXACT_MATCH, VMOPT_XXIGNOREUNRECOGNIZEDRESTOREOPTIONSENABLE, NULL);
			IDATA ignoreDisabled = FIND_AND_CONSUME_ARG(restoreArgsList, EXACT_MATCH, VMOPT_XXIGNOREUNRECOGNIZEDRESTOREOPTIONSDISABLE, NULL);

			bool dontIgnoreUnsupportedRestoreOptions = (ignoreEnabled < 0) || (ignoreEnabled < ignoreDisabled);

			if ((FALSE == vmFuncs->checkArgsConsumed(vm, vm->portLibrary, restoreArgsList)) && dontIgnoreUnsupportedRestoreOptions) {
				currentExceptionClass = vm->checkpointState.criuJVMRestoreExceptionClass;
				systemReturnCode = 0;
				nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
					J9NLS_JCL_CRIU_FAILED_TO_ENABLE_ALL_RESTORE_OPTIONS, NULL);
			}
		}


wakeJavaThreads:
		acquireSafeOrExcusiveVMAccess(currentThread, vmFuncs, safePoint);

wakeJavaThreadsWithExclusiveVMAccess:

		vm->extendedRuntimeFlags2 &= ~J9_EXTENDED_RUNTIME2_CRIU_SINGLE_THREAD_MODE;

		toggleSuspendOnJavaThreads(currentThread, FALSE, FALSE);

		releaseSafeOrExcusiveVMAccess(currentThread, vmFuncs, safePoint);

		VM_VMHelpers::setVMState(currentThread, J9VMSTATE_CRIU_SUPPORT_RESTORE_PHASE_END);
closeWorkDirFD:
		if ((0 != close(workDirFD)) && (NULL == currentExceptionClass)) {
			systemReturnCode = errno;
			if (isAfterCheckpoint) {
				currentExceptionClass = vm->checkpointState.criuSystemRestoreExceptionClass;
			} else {
				currentExceptionClass = vm->checkpointState.criuSystemCheckpointExceptionClass;
			}
			nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_JCL_CRIU_FAILED_TO_CLOSE_WORK_DIR, NULL);
		}
closeDirFD:
		if ((0 != close(dirFD)) && (NULL == currentExceptionClass)) {
			systemReturnCode = errno;
			if (isAfterCheckpoint) {
				currentExceptionClass = vm->checkpointState.criuSystemRestoreExceptionClass;
			} else {
				currentExceptionClass = vm->checkpointState.criuSystemCheckpointExceptionClass;
			}
			nlsMsgFormat = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_JCL_CRIU_FAILED_TO_CLOSE_DIR, NULL);
		}
freeWorkDir:
		if (workDirBuf != workDirChars) {
			j9mem_free_memory(workDirChars);
		}
freeEnvFile:
		if (envFileBuf != envFileChars) {
			j9mem_free_memory(envFileChars);
		}
freeOptionsFile:
		if (optionsFileBuf != optionsFileChars) {
			j9mem_free_memory(optionsFileChars);
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
		if (vm->checkpointState.criuJVMCheckpointExceptionClass == currentExceptionClass) {
			init = vm->checkpointState.criuJVMCheckpointExceptionInit;
		} else if (vm->checkpointState.criuSystemCheckpointExceptionClass == currentExceptionClass) {
			init = vm->checkpointState.criuSystemCheckpointExceptionInit;
		} else if (vm->checkpointState.criuSystemRestoreExceptionClass == currentExceptionClass) {
			init = vm->checkpointState.criuSystemRestoreExceptionInit;
		} else {
			init = vm->checkpointState.criuJVMRestoreExceptionInit;
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

jobject JNICALL
Java_org_eclipse_openj9_criu_CRIUSupport_getRestoreSystemProperites(JNIEnv *env, jclass unused)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	jobject systemProperties = NULL;

	if (NULL != vm->checkpointState.restoreArgsList) {
		systemProperties = vm->internalVMFunctions->getRestoreSystemProperites(currentThread);
	}

	return systemProperties;
}

} /* extern "C" */
