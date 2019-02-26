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

#include "j9.h"
#include "jni.h"
#include "j9comp.h"
#include "vmaccess.h"
#include "j9cfg.h"
#include "j9consts.h"
#include "j9protos.h"
#include "rommeth.h"
#include "j9cp.h"
#include "j9vmls.h"
#include "stackwalk.h"
#include "vmhook.h"
#include "jnichknls.h"
#include "jvminit.h"

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "j9port.h"
#include "jnicheck.h"
#include "jnichk_internal.h"
#include "ut_j9jni.h"
#include "vendor_version.h"

static void jniCallIn (J9VMThread * vmThread);
static void methodExitHook (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void printJnichkHelp (J9PortLibrary* portLib);
static UDATA jniNextSigChar (U_8 ** utfData);
static BOOLEAN jnichk_isObjectArray (JNIEnv* env, jobject obj);
static BOOLEAN jnichk_isIndexable (JNIEnv* env, jobject obj);
static void jniCheckValidClass (JNIEnv* env, const char* function, UDATA argNum, jobject obj);
static IDATA jniDecodeValue (J9VMThread * vmThread, UDATA sigChar, void * valuePtr, char ** outputBuffer, UDATA * outputBufferLength);
jint JNICALL JVM_OnLoad( JavaVM *jvm, char* options, void *reserved );
static void jniCallInReturn (J9VMThread * vmThread, void * vaptr, void *stackResult, UDATA returnType);
static void methodEnterHook (J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData);
static void jniTraceMethodID (JNIEnv* env, jmethodID mid);
static void jniCheckPrintJNIOnLoad (JNIEnv* env, U_32 level);
static UDATA jniIsWeakGlobalRef (JNIEnv* env, jobject reference);
static UDATA jniIsLocalRef (JNIEnv * currentEnv, JNIEnv* env, jobject reference);
static UDATA jniIsLocalRefFrameWalkFunction (J9VMThread* aThread, J9StackWalkState* walkState);
static void fillInLocalRefTracking (JNIEnv* env, J9JniCheckLocalRefState* refTracking);
static const char* getRefType (JNIEnv* env, jobject reference);
static void jniCheckScalarArgA (const char* function, JNIEnv* env, jvalue* arg, char code, UDATA argNum, UDATA trace);
static void jniCheckScalarArg (const char* function, JNIEnv* env, va_list* va, char code, UDATA argNum, UDATA trace);
static void jniCheckPrintMethod (JNIEnv* env, U_32 level);
static void jniTraceObject (JNIEnv* env, jobject aJobject);
static UDATA jniIsGlobalRef (JNIEnv* env, jobject reference);
static J9Class* jnichk_getObjectClazz (JNIEnv* env, jobject objRef);
static void jniCheckCall (const char* function, JNIEnv* env, jobject receiver, UDATA methodType, UDATA returnType, jmethodID method);
static int isLoadLibraryWithPath (J9UTF8* className, J9UTF8* methodName);
static void jniCheckPushCount (JNIEnv* env, const char* function);
static void jniCheckObjectRange (JNIEnv* env, const char* function, jsize actualLen, jint start, jsize len);
static char* jniCheckObjectArg (const char* function, JNIEnv* env, jobject aJobject, char* sigArgs, UDATA argNum, UDATA trace);
static void jniTraceFieldID (JNIEnv* env, jfieldID fid);
static void jniIsLocalRefOSlotWalkFunction (J9VMThread* aThread, J9StackWalkState* walkState, j9object_t*slot, const void * stackLocation);
static IDATA jniCheckParseOptions(J9JavaVM* javaVM, char* options);
static IDATA jniCheckProcessCommandLine(J9JavaVM* javaVM, J9VMDllLoadInfo* loadInfo);
static UDATA globrefHashTableHashFn(void *entry, void *userData);
static UDATA globrefHashTableEqualFn(void *leftEntry, void *rightEntry, void *userData);

static UDATA keyInitCount = 0;
omrthread_tls_key_t jniEntryCountKey;

static omrthread_tls_key_t potentialPendingExceptionKey;

#define LOAD_LIB_WITH_PATH_CLASS "java/lang/ClassLoader"
#define LOAD_LIB_WITH_PATH_METHOD "loadLibraryWithPath"

#define BAD_VA_LIST	0xBAADDEED

IDATA
J9VMDllMain(J9JavaVM* vm, IDATA stage, void* reserved)
{
	extern const struct JNINativeInterface_ JNICheckTable;
	J9HookInterface** hook;
	J9VMDllLoadInfo* loadInfo;
	IDATA rc;

	PORT_ACCESS_FROM_JAVAVM(vm);

	switch(stage) {
		case ALL_VM_ARGS_CONSUMED:
			hook = vm->internalVMFunctions->getVMHookInterface(vm);
			loadInfo = FIND_DLL_TABLE_ENTRY( J9_CHECK_JNI_DLL_NAME );

			rc = jniCheckProcessCommandLine(vm, loadInfo);
			if (rc != J9VMDLLMAIN_OK) {
				return rc;
			}

			vm->jniFunctionTable = GLOBAL_TABLE(JNICheckTable);
			if (omrthread_tls_alloc(&jniEntryCountKey)) {
				return J9VMDLLMAIN_FAILED;
			}
			if (omrthread_tls_alloc(&potentialPendingExceptionKey)) {
				return J9VMDLLMAIN_FAILED;
			}

			if (jniCheckMemoryInit(vm)) {
				return J9VMDLLMAIN_FAILED;
			}

			if ((*hook)->J9HookRegisterWithCallSite(hook, J9HOOK_VM_NATIVE_METHOD_ENTER, methodEnterHook, OMR_GET_CALLSITE(), NULL)) {
				j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_JNICHK_UNABLE_TO_HOOK);
				return J9VMDLLMAIN_FAILED;
			}

			if ((*hook)->J9HookRegisterWithCallSite(hook, J9HOOK_VM_NATIVE_METHOD_RETURN, methodExitHook, OMR_GET_CALLSITE(), NULL)) {
				j9tty_err_printf(PORTLIB, "<JNI check utility: unable to hook event>\n");
				return J9VMDLLMAIN_FAILED;
			}

			vm->checkJNIData.jniGlobalRefHashTab = hashTableNew(OMRPORT_FROM_J9PORT(PORTLIB),
					J9_GET_CALLSITE(),
					0, /* default table size */
					sizeof(JNICHK_GREF_HASHENTRY), /* entry size */
					0, /* default entry alignment */
					0, /* flags */
					OMRMEM_CATEGORY_VM,
					(J9HashTableHashFn) globrefHashTableHashFn,
					(J9HashTableEqualFn) globrefHashTableEqualFn,
					NULL,
					NULL );

			if (NULL == vm->checkJNIData.jniGlobalRefHashTab) {
				return J9VMDLLMAIN_FAILED;
			}

			j9nls_printf(PORTLIB, J9NLS_INFO, J9NLS_JNICHK_INSTALLED);
			break;
		
		case JIT_INITIALIZED :
			/* Register this module with trace */
			UT_MODULE_LOADED(J9_UTINTERFACE_FROM_VM(vm));
			Trc_JNI_VMInitStages_Event1(NULL);
			break;
		
		case GC_SHUTDOWN_COMPLETE:
			if (NULL != vm->checkJNIData.jniGlobalRefHashTab) {
				/* free the hash table */
				hashTableFree(vm->checkJNIData.jniGlobalRefHashTab);
				vm->checkJNIData.jniGlobalRefHashTab = NULL;
			}
			break;
	}

	return J9VMDLLMAIN_OK;
}

static IDATA
jniCheckProcessCommandLine(J9JavaVM* vm, J9VMDllLoadInfo* loadInfo)
{
	char* options = "";
	char* globalOptions = "";
	IDATA rc;
	IDATA xcheckJNIIndex, levelIndex;
	PORT_ACCESS_FROM_JAVAVM(vm);

	/* Default to warnings and advice disabled */

	vm->checkJNIData.options |= (JNICHK_NOWARN | JNICHK_NOADVICE);

#if defined(J9ZOS390)
	/* for ZOS only, set the default to novalist, on all other platforms it's valist */
	vm->checkJNIData.options |= JNICHK_NOVALIST;
#endif /* defined(J9ZOS390) */

	/*
	 * -Xcheck:nabounds is a RI-compatibility option. It is equivalent to -Xcheck:jni.
	 * No sub-options are permitted for -Xcheck:nabounds
	 */
	FIND_AND_CONSUME_ARG( EXACT_MATCH, "-Xcheck:nabounds", NULL );

	xcheckJNIIndex = FIND_AND_CONSUME_ARG( OPTIONAL_LIST_MATCH, "-Xcheck:jni", NULL );

	levelIndex = FIND_AND_CONSUME_ARG( STARTSWITH_MATCH, "-Xcheck:level=", NULL );

	if (xcheckJNIIndex >= 0) {
		GET_OPTION_VALUE(xcheckJNIIndex, ':', &options);
		options = strchr(options, ':');
		if (options == NULL) {
			options = "";
		} else {
			options++;
		}
	}

	if (levelIndex >= 0) {
		GET_OPTION_VALUE(levelIndex, ':', &globalOptions);
	}

	/* always parse the global options first */
	rc = jniCheckParseOptions(vm, globalOptions);
	if (rc != J9VMDLLMAIN_OK) {
		return rc;
	}

	/* now parse the specific options only if they appear after the global option */
	if (xcheckJNIIndex > levelIndex) {
		rc = jniCheckParseOptions(vm, options);
		if (rc != J9VMDLLMAIN_OK) {
			return rc;
		}
	}

	/* parse whichever option appears last */
	rc = jniCheckParseOptions(vm, xcheckJNIIndex < levelIndex ? globalOptions : options);
	if (rc != J9VMDLLMAIN_OK) {
		return rc;
	}

	return 	J9VMDLLMAIN_OK;
}

static IDATA
jniCheckParseOptions(J9JavaVM* vm, char* options)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	char* scan_start = options;
	char* scan_limit = options + strlen(options);

	while (scan_start < scan_limit) {
		/* ignore separators */
		try_scan(&scan_start, ",");

		/* scan for verbose */
		if (try_scan(&scan_start, "verbose")) {
			vm->checkJNIData.options |= JNICHK_VERBOSE;
			continue;
		}

		/* scan for nobounds */
		if (try_scan(&scan_start, "nobounds")) {
			vm->checkJNIData.options |= JNICHK_NOBOUNDS;
			continue;
		}

		/* scan for nonfatal */
		if (try_scan(&scan_start, "nonfatal")) {
			vm->checkJNIData.options |= JNICHK_NONFATAL;
			continue;
		}

		/* scan for nowarn */
		if (try_scan(&scan_start, "nowarn")) {
			vm->checkJNIData.options |= JNICHK_NOWARN;
			continue;
		}

		/* scan for noadvice */
		if (try_scan(&scan_start, "noadvice")) {
			vm->checkJNIData.options |= JNICHK_NOADVICE;
			continue;
		}

		/* scan for warn */
		if (try_scan(&scan_start, "warn")) {
			vm->checkJNIData.options &= ~JNICHK_NOWARN;
			continue;
		}

		/* scan for advice */
		if (try_scan(&scan_start, "advice")) {
			vm->checkJNIData.options &= ~JNICHK_NOADVICE;
			continue;
		}

		/* scan for pedantic */
		if (try_scan(&scan_start, "pedantic")) {
			vm->checkJNIData.options |= JNICHK_PEDANTIC;
			continue;
		}

		/* scan for trace */
		if (try_scan(&scan_start, "trace")) {
			vm->checkJNIData.options |= JNICHK_TRACE;
			continue;
		}

		/* scan for novalist */
		if (try_scan(&scan_start, "novalist")) {
			vm->checkJNIData.options |= JNICHK_NOVALIST;
			continue;
		}

		/* scan for valist */
		if (try_scan(&scan_start, "valist")) {
			vm->checkJNIData.options &= ~JNICHK_NOVALIST;
			continue;
		}

		/* scan for all */
		if (try_scan(&scan_start, "all")) {
			vm->checkJNIData.options |= JNICHK_INCLUDEBOOT;
			continue;
		}
		
		if (try_scan(&scan_start, "alwayscopy")) {
			vm->checkJNIData.options |= JNICHK_ALWAYSCOPY;
			continue;
		}

		/* scan for levels */
		if (try_scan(&scan_start, "level=low")) {
			vm->checkJNIData.options = (JNICHK_NONFATAL | JNICHK_NOWARN | JNICHK_NOADVICE);
			continue;
		} else if (try_scan(&scan_start, "level=medium")) {
			vm->checkJNIData.options = (JNICHK_NONFATAL | JNICHK_NOWARN);
			continue;
		} else if (try_scan(&scan_start, "level=high")) {
			vm->checkJNIData.options = 0;
			continue;
		} else if (try_scan(&scan_start, "level=maximum")) {
			vm->checkJNIData.options = (JNICHK_INCLUDEBOOT | JNICHK_PEDANTIC);
			continue;
		}

		/* scan for help */
		if (try_scan(&scan_start, "help")) {
			printJnichkHelp(PORTLIB);
			return J9VMDLLMAIN_SILENT_EXIT_VM;
		}

		/* no match */
		j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_JNICHK_UNRECOGNIZED_OPTION, scan_start);
		printJnichkHelp(PORTLIB);
		return J9VMDLLMAIN_FAILED;
	}

	return J9VMDLLMAIN_OK;
}

void jniCheckSubclass(JNIEnv* env, const char* function, IDATA argNum, jobject aJobject, const char* type) {
	J9JavaVM* j9vm = ((J9VMThread*)env)->javaVM;

	jclass superclazz = j9vm->EsJNIFunctions->FindClass(env, type);
	if (superclazz == NULL) {
		jniCheckFatalErrorNLS(env, J9NLS_JNICHK_ARGUMENT_CLASS_NOT_FOUND, function, argNum, type);
	}

	if (!j9vm->EsJNIFunctions->IsInstanceOf(env, aJobject, superclazz)) {
		jniCheckFatalErrorNLS(env, J9NLS_JNICHK_ARGUMENT_IS_NOT_SUBCLASS, function, argNum, type);
	}
}


void
jniCheckRange(JNIEnv* env,  const char* function, const char* type, IDATA arg, IDATA argNum, IDATA min, IDATA max)
{
	if (arg < min) {
		jniCheckFatalErrorNLS(env, J9NLS_JNICHK_ARGUMENT_IS_TOO_LOW, function, argNum, type, arg, min);
	} else if (arg > max) {
		jniCheckFatalErrorNLS(env, J9NLS_JNICHK_ARGUMENT_IS_TOO_HIGH, function, argNum, type, arg, max);
	}
}


void jniCheckNull(JNIEnv* env, const char* function, IDATA argNum, jobject obj) {
	J9JavaVM* j9vm = ((J9VMThread*)env)->javaVM;

	if ( j9vm->EsJNIFunctions->IsSameObject(env, NULL, obj) ) {
		jniCheckFatalErrorNLS(env, J9NLS_JNICHK_ARGUMENT_IS_NULL, function, argNum);
	} else if ( jniIsWeakGlobalRef(env, obj) ) {
		jniCheckWarningNLS(env, J9NLS_JNICHK_WEAK_GLOBAL_REF_COULD_BE_NULL, function, argNum, function);
	}
}


void jniCheckClass(JNIEnv* env, const char* function, IDATA argNum, jobject aJobject, J9Class* expectedClass, const char* expectedType) {

	if ( jnichk_getObjectClazz(env, aJobject) != expectedClass) {
		jniCheckFatalErrorNLS(env, J9NLS_JNICHK_ARGUMENT_IS_NOT_X, function, argNum, expectedType);
	}
}


void
jniCheckCallV(const char* function, JNIEnv* env, jobject receiver, UDATA methodType, UDATA returnType, jmethodID method, va_list originalArgs)
{
	J9JavaVM* vm = ((J9VMThread*)env)->javaVM;
	PORT_ACCESS_FROM_JAVAVM(vm);
	J9Method *ramMethod = ((J9JNIMethodID*)method)->method;
	J9ROMMethod* romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(ramMethod);
	J9UTF8* sig = J9ROMMETHOD_GET_SIGNATURE(UNTAGGED_METHOD_CP(ramMethod)->ramClass->romClass, romMethod);
	char* sigArgs;
	va_list args;
	UDATA argNum;
	UDATA novalist = vm->checkJNIData.options & JNICHK_NOVALIST;
	UDATA trace = vm->checkJNIData.options & JNICHK_TRACE;

	jniCheckCall(function, env, receiver, methodType, returnType, method);

	if (!novalist) {
		if (*((U_32 *) VA_PTR(originalArgs)) == BAD_VA_LIST) {
			jniCheckFatalErrorNLS(env, J9NLS_JNICHK_VA_LIST_REUSE, function);
		}
	}

	if (trace) {
		UDATA indent = (UDATA) omrthread_tls_get(((J9VMThread *) env)->osThread, jniEntryCountKey);

		j9tty_printf(PORTLIB, "%p %*sArguments: ", env, indent * 2, "");
	}

	/* check args */
#if 1
	memcpy(VA_PTR(args), VA_PTR(originalArgs), sizeof(args));
#else
	va_copy(args, originalArgs);
#endif
	argNum = 3; /* skip the env and receiver */
	sigArgs = (char*)J9UTF8_DATA(sig);
	while (*++sigArgs != ')') {
		if (trace) {
			if (argNum !=3) j9tty_printf(PORTLIB, ", ");
		}
		switch (*sigArgs) {
		case 'L':
		case '[':
			sigArgs = jniCheckObjectArg(function, env, va_arg(args, jobject), sigArgs, argNum, trace);
			break;
		default:
			jniCheckScalarArg(function, env, VA_PTR(args), *sigArgs, argNum, trace);
			break;
		}
		++argNum;
	}

	if (trace) {
		if (argNum == 3) {
			j9tty_printf(PORTLIB, "void");
		}
		j9tty_printf(PORTLIB, "\n");
	}
}


void
jniCheckCallA(const char* function, JNIEnv* env, jobject receiver, UDATA methodType, UDATA returnType, jmethodID method, jvalue* args)
{
	J9JavaVM* vm = ((J9VMThread*)env)->javaVM;
	PORT_ACCESS_FROM_JAVAVM(vm);
	J9Method *ramMethod = ((J9JNIMethodID*)method)->method;
	J9ROMMethod* romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(ramMethod);
	J9UTF8* sig = J9ROMMETHOD_GET_SIGNATURE(UNTAGGED_METHOD_CP(ramMethod)->ramClass->romClass, romMethod);
	char* sigArgs;
	UDATA argNum;
	UDATA trace = vm->checkJNIData.options & JNICHK_TRACE;

	jniCheckCall(function, env, receiver, methodType, returnType, method);

	if (trace) {
		UDATA indent = (UDATA) omrthread_tls_get(((J9VMThread *) env)->osThread, jniEntryCountKey);

		j9tty_printf(PORTLIB, "%p %*sArguments: ", env, indent * 2, "");
	}

	/* check args */
	argNum = 3; /* skip the env and receiver */
	sigArgs = (char*)J9UTF8_DATA(sig);
	while (*++sigArgs != ')') {
		if (trace) {
			if (argNum !=3) j9tty_printf(PORTLIB, ", ");
		}
		switch (*sigArgs) {
		case 'L':
		case '[':
			sigArgs = jniCheckObjectArg(function, env, args++->l, sigArgs, argNum, trace);
			break;
		default:
			jniCheckScalarArgA(function, env, args++, *sigArgs, argNum, trace);
			break;
		}
		++argNum;
	}

	if (trace) {
		if (argNum == 3) {
			j9tty_printf(PORTLIB, "void");
		}
		j9tty_printf(PORTLIB, "\n");
	}
}


void
jniCheckArgs(const char *function, int exceptionSafe, int criticalSafe, J9JniCheckLocalRefState *refTracking, const U_32 *descriptor, JNIEnv *env, ...)
{
	J9JavaVM *vm = ((J9VMThread*)env)->javaVM;
	PORT_ACCESS_FROM_JAVAVM(vm);
	va_list va;
	const U_32 *code;
	int argNum = 2;
	UDATA trace = vm->checkJNIData.options & JNICHK_TRACE;
	UDATA warn = (0 == (vm->checkJNIData.options & JNICHK_NOWARN));
	J9VMThread * vmThread = (J9VMThread*)env;

	if (vm->reserved1_identifier != (void*)J9VM_IDENTIFIER) {
		jniCheckFatalErrorNLS(env, J9NLS_JNICHK_INVALID_ENV, function);
	}

	if (vm->internalVMFunctions->currentVMThread(vm) != vmThread) {
		jniCheckFatalErrorNLS(env, J9NLS_JNICHK_WRONG_ENV, function);
	}

	if (exceptionSafe == 0) {
		const char* exceptionSetter;

		if ( vmThread->currentException ) {
			jniCheckFatalErrorNLS(env, J9NLS_JNICHK_PENDING_EXCEPTION, function);
		}

		exceptionSetter = jniCheckGetPotentialPendingException();
		if (exceptionSetter != NULL) {
			jniCheckWarningNLS(env,
				J9NLS_JNICHK_MISSING_EXCEPTION_CHECK,
				function,
				exceptionSetter,
				function);
		}
	}

	if (criticalSafe != CRITICAL_SAFE) {
		if ((vmThread->jniCriticalCopyCount != 0) || (vmThread->jniCriticalDirectCount != 0)) {
			if (criticalSafe == CRITICAL_WARN) {
				if (warn) {
					j9nls_printf(PORTLIB, J9NLS_WARNING, J9NLS_JNICHK_CRITICAL_UNSAFE_WARN, function);
/*					jniCheckPrintMethod(env); */
				}
			} else {
				jniCheckFatalErrorNLS(env, J9NLS_JNICHK_CRITICAL_UNSAFE_ERROR, function);
			}
		}
	}

	fillInLocalRefTracking(env, refTracking);

	if (trace) {
		UDATA indent = (UDATA) omrthread_tls_get(((J9VMThread *) env)->osThread, jniEntryCountKey);

		j9tty_printf(PORTLIB, "%p %*s%s(", env, indent * 2, "", function);
	}

	va_start(va, env);
	code = descriptor;
	while (*code) {
		jmethodID aJmethodID;
		jfieldID aJfieldID;
		jsize aJsize;
		jobject aJobject;
		void* aPointer;
		U_32 asciiCode = *code & JNIC_MASK;

		switch (asciiCode) {
		case JNIC_JBYTE:
		case JNIC_JBOOLEAN:
		case JNIC_JCHAR:
		case JNIC_JSHORT:
		case JNIC_JINT:
		case JNIC_JLONG:
		case JNIC_JFLOAT:
		case JNIC_JDOUBLE:
			jniCheckScalarArg(function, env, VA_PTR(va), *code, argNum, trace);
			break;

		case JNIC_JMETHODID:
			aJmethodID = va_arg(va, jmethodID);
			if (trace) {
				jniTraceMethodID(env, aJmethodID);
			}
			break;
		case JNIC_JFIELDID:
			aJfieldID = va_arg(va, jfieldID);
			if (trace) {
				jniTraceFieldID(env, aJfieldID);
			}
			break;
		case JNIC_VALIST:
			va_arg(va, va_list*);
			if (trace) {
				j9tty_printf(PORTLIB, "(va_list)%p", va);
			}
			break;
		case JNIC_JSIZE:
			aJsize = va_arg(va, jsize);
			jniCheckRange(env, function, "jsize", aJsize, argNum, 0, 0x7FFFFFFF);
			if (trace) {
				j9tty_printf(PORTLIB, "(jsize)%d", (jint)aJsize);
			}
			break;

		case JNIC_POINTER:
			aPointer = va_arg(va, void*);
			if (trace) {
				j9tty_printf(PORTLIB, "(void*)%p", aPointer);
			}
			break;

		case JNIC_STRING:
			aPointer = va_arg(va, char*);
			if (trace) {
				if (aPointer == NULL) {
					j9tty_printf(PORTLIB, "(const char*)%p", aPointer);
				} else {
					j9tty_printf(PORTLIB, "\"%s\"", aPointer);
				}
			}
			break;

		case JNIC_CLASSNAME:
			aPointer = va_arg(va, char*);
			if (NULL == aPointer) {
				jniCheckFatalErrorNLS(env, J9NLS_JNICHK_NULL_ARGUMENT, function, argNum);
			} else if (warn) {
				if (!verifyClassnameUtf8(aPointer, strlen(aPointer))) {
					jniCheckWarningNLS(env, J9NLS_JNICHK_MALFORMED_IDENTIFIER, function, argNum, aPointer);
				}
			}
			if (trace) {
				j9tty_printf(PORTLIB, "\"%s\"", aPointer);
			}
			break;

		case JNIC_MEMBERNAME:
			aPointer = va_arg(va, char*);
			if (NULL == aPointer) {
				jniCheckFatalErrorNLS(env, J9NLS_JNICHK_NULL_ARGUMENT, function, argNum);
			} else if (warn) {
				if (!verifyIdentifierUtf8(aPointer, strlen(aPointer))) {
					jniCheckWarningNLS(env, J9NLS_JNICHK_MALFORMED_IDENTIFIER, function, argNum, aPointer);
				}
			}
			if (trace) {
				j9tty_printf(PORTLIB, "\"%s\"", aPointer);
			}
			break;

		case JNIC_METHODSIGNATURE:
			aPointer = va_arg(va, char*);
			if (aPointer == NULL) {
				jniCheckFatalErrorNLS(env, J9NLS_JNICHK_NULL_ARGUMENT, function, argNum);
			} else if (warn) {
				if (verifyMethodSignatureUtf8(aPointer, strlen(aPointer)) < 0) {
					jniCheckWarningNLS(env, J9NLS_JNICHK_MALFORMED_METHOD_SIGNATURE, function, argNum, aPointer);
				}
			}
			if (trace) {
				j9tty_printf(PORTLIB, "\"%s\"", aPointer);
			}
			break;

		case JNIC_FIELDSIGNATURE:
			aPointer = va_arg(va, char*);
			if (aPointer == NULL) {
				jniCheckFatalErrorNLS(env, J9NLS_JNICHK_NULL_ARGUMENT, function, argNum);
			} else if (warn) {
				if (verifyFieldSignatureUtf8(aPointer, strlen(aPointer), 0) < 0) {
					jniCheckWarningNLS(env, J9NLS_JNICHK_MALFORMED_FIELD_SIGNATURE, function, argNum, aPointer);
				}
			}
			if (trace) {
				j9tty_printf(PORTLIB, "\"%s\"", aPointer);
			}
			break;

		case JNIC_JOBJECTARRAY:
			aJobject = va_arg(va, jobject);
			jniCheckNull(env, function, argNum, aJobject);
			jniCheckRef(env, function, argNum, aJobject);
			if (!jnichk_isObjectArray(env, aJobject)) {
				jniCheckFatalErrorNLS(env, J9NLS_JNICHK_ARGUMENT_IS_NOT_JOBJECTARRAY, function, argNum);
			}
			if (trace) jniTraceObject(env, aJobject);
			break;

		case JNIC_JARRAY:
			aJobject = va_arg(va, jobject);
			jniCheckNull(env, function, argNum, aJobject);
			jniCheckRef(env, function, argNum, aJobject);
			if (!jnichk_isIndexable(env, aJobject)) {
				jniCheckFatalErrorNLS(env, J9NLS_JNICHK_ARGUMENT_IS_NOT_JARRAY, function, argNum);
			}
			if (trace) jniTraceObject(env, aJobject);
			break;

		case JNIC_JINTARRAY:
			aJobject = va_arg(va, jobject);
			jniCheckNull(env, function, argNum, aJobject);
			jniCheckRef(env, function, argNum, aJobject);
			jniCheckClass(env, function, argNum, aJobject, vm->intArrayClass, "jintArray");
			if (trace) jniTraceObject(env, aJobject);
			break;

		case JNIC_JDOUBLEARRAY:
			aJobject = va_arg(va, jobject);
			jniCheckNull(env, function, argNum, aJobject);
			jniCheckRef(env, function, argNum, aJobject);
			jniCheckClass(env, function, argNum, aJobject, vm->doubleArrayClass, "jdoubleArray");
			if (trace) jniTraceObject(env, aJobject);
			break;

		case JNIC_JCHARARRAY:
			aJobject = va_arg(va, jobject);
			jniCheckNull(env, function, argNum, aJobject);
			jniCheckRef(env, function, argNum, aJobject);
			jniCheckClass(env, function, argNum, aJobject, vm->charArrayClass, "jcharArray");
			if (trace) jniTraceObject(env, aJobject);
			break;

		case JNIC_JSHORTARRAY:
			aJobject = va_arg(va, jobject);
			jniCheckNull(env, function, argNum, aJobject);
			jniCheckRef(env, function, argNum, aJobject);
			jniCheckClass(env, function, argNum, aJobject, vm->shortArrayClass, "jshortArray");
			if (trace) jniTraceObject(env, aJobject);
			break;

		case JNIC_JBYTEARRAY:
			aJobject = va_arg(va, jobject);
			jniCheckNull(env, function, argNum, aJobject);
			jniCheckRef(env, function, argNum, aJobject);
			jniCheckClass(env, function, argNum, aJobject, vm->byteArrayClass, "jbyteArray");
			if (trace) jniTraceObject(env, aJobject);
			break;

		case JNIC_JBOOLEANARRAY:
			aJobject = va_arg(va, jobject);
			jniCheckNull(env, function, argNum, aJobject);
			jniCheckRef(env, function, argNum, aJobject);
			jniCheckClass(env, function, argNum, aJobject, vm->booleanArrayClass, "jbooleanArray");
			if (trace) jniTraceObject(env, aJobject);
			break;

		case JNIC_JFLOATARRAY:
			aJobject = va_arg(va, jobject);
			jniCheckNull(env, function, argNum, aJobject);
			jniCheckRef(env, function, argNum, aJobject);
			jniCheckClass(env, function, argNum, aJobject, vm->floatArrayClass, "jfloatArray");
			if (trace) jniTraceObject(env, aJobject);
			break;

		case JNIC_JLONGARRAY:
			aJobject = va_arg(va, jobject);
			jniCheckNull(env, function, argNum, aJobject);
			jniCheckRef(env, function, argNum, aJobject);
			jniCheckClass(env, function, argNum, aJobject, vm->longArrayClass, "jlongArray");
			if (trace) jniTraceObject(env, aJobject);
			break;

		case JNIC_JCLASS:
			aJobject = va_arg(va, jobject);
			jniCheckNull(env, function, argNum, aJobject);
			jniCheckRef(env, function, argNum, aJobject);
			jniCheckClass(env, function, argNum, aJobject, J9VMJAVALANGCLASS_OR_NULL(vm), "jclass");
			jniCheckValidClass(env, function, argNum, aJobject);
			if (trace) jniTraceObject(env, aJobject);
			break;

		case JNIC_JSTRING:
			aJobject = va_arg(va, jobject);
			jniCheckNull(env, function, argNum, aJobject);
			jniCheckRef(env, function, argNum, aJobject);
			jniCheckClass(env, function, argNum, aJobject, J9VMJAVALANGSTRING_OR_NULL(vm), "jstring");
			if (trace) jniTraceObject(env, aJobject);
			break;

		case JNIC_JTHROWABLE:
			aJobject = va_arg(va, jobject);
			jniCheckNull(env, function, argNum, aJobject);
			jniCheckRef(env, function, argNum, aJobject);
			jniCheckSubclass(env, function, argNum, aJobject, "java/lang/Throwable");
			if (trace) jniTraceObject(env, aJobject);
			break;

		case JNIC_DIRECTBUFFER:
			aJobject = va_arg(va, jobject);
			jniCheckNull(env, function, argNum, aJobject);
			jniCheckRef(env, function, argNum, aJobject);
			jniCheckDirectBuffer(env, function, argNum, aJobject);
			if (trace) jniTraceObject(env, aJobject);
			break;

		case JNIC_REFLECTMETHOD:
			aJobject = va_arg(va, jobject);
			jniCheckNull(env, function, argNum, aJobject);
			jniCheckRef(env, function, argNum, aJobject);
			/* jniCheckSubclass(env, function, argNum, aJobject, "java/lang/reflect/Constructor" or "java/lang/reflect/Method"); */
			if (trace) jniTraceObject(env, aJobject);
			break;

		case JNIC_REFLECTFIELD:
			aJobject = va_arg(va, jobject);
			jniCheckNull(env, function, argNum, aJobject);
			jniCheckRef(env, function, argNum, aJobject);
			jniCheckSubclass(env, function, argNum, aJobject, "java/lang/reflect/Field");
			if (trace) jniTraceObject(env, aJobject);
			break;

		case JNIC_NONNULLOBJECT:
			aJobject = va_arg(va, jobject);
			jniCheckNull(env, function, argNum, aJobject);
			jniCheckRef(env, function, argNum, aJobject);
			if (trace) jniTraceObject(env, aJobject);
			break;
		case JNIC_JOBJECT:
			aJobject = va_arg(va, jobject);
			if (aJobject != NULL) {
				jniCheckRef(env, function, argNum, aJobject);
			}
			if (trace) jniTraceObject(env, aJobject);
			break;

		case JNIC_WEAKREF:
			aJobject = va_arg(va, jobject);
			if (aJobject != NULL) jniCheckWeakGlobalRef(env, function, argNum, aJobject);
			if (trace) jniTraceObject(env, aJobject);
			break;

		case JNIC_GLOBALREF:
			aJobject = va_arg(va, jobject);
			if (aJobject != NULL) jniCheckGlobalRef(env, function, argNum, aJobject);
			if (trace) jniTraceObject(env, aJobject);
			break;

		case JNIC_LOCALREF:
			aJobject = va_arg(va, jobject);
			if (aJobject != NULL) jniCheckLocalRef(env, function, argNum, aJobject);
			if (trace) jniTraceObject(env, aJobject);
			break;

		default:
			jniCheckFatalErrorNLS(env, J9NLS_JNICHK_BAD_DESCRIPTOR, function, (UDATA)*code);
		}

		code += 1;
		argNum += 1;

		if (trace && *code) {
			j9tty_printf(PORTLIB, ", ");
		}

	}

	if (trace) {
		if (!strncmp("Call", function, 4)) {
			j9tty_printf(PORTLIB, ") {\n");
		} else {
			j9tty_printf(PORTLIB, ")\n");
		}
	}


}


static J9Class*
jnichk_getObjectClazz(JNIEnv* env, jobject objRef)
{
	J9VMThread* vmThread = (J9VMThread*)env;
	J9Class* clazz = NULL;

	enterVM(vmThread);

	if (objRef != NULL) {
		j9object_t obj = *(j9object_t*)objRef;

		if (obj != NULL) {
			clazz = J9OBJECT_CLAZZ(vmThread, obj);
		}
	}

	exitVM(vmThread);

	return clazz;
}

static BOOLEAN jnichk_isObjectArray(JNIEnv* env, jobject obj) {
	J9VMThread* vmThread = (J9VMThread*) env;
	J9JavaVM* vm = vmThread->javaVM;
	J9Class* clazz = NULL;
	BOOLEAN result = FALSE;

	enterVM(vmThread);

	clazz = J9OBJECT_CLAZZ(vmThread, *(j9object_t*)obj);

	if (J9CLASS_IS_ARRAY(clazz)) {
		result = (OBJECT_HEADER_SHAPE_POINTERS == J9CLASS_SHAPE(clazz));

	}

	exitVM(vmThread);

	return result;
}

static BOOLEAN jnichk_isIndexable(JNIEnv* env, jobject obj) {
	J9VMThread* vmThread = (J9VMThread*) env;
	J9JavaVM* vm = vmThread->javaVM;
	J9Class* clazz = NULL;
	BOOLEAN result;

	enterVM(vmThread);

	clazz = J9OBJECT_CLAZZ(vmThread, *(j9object_t*)obj);
	result = J9CLASS_IS_ARRAY(clazz);

	exitVM(vmThread);

	return result;
}

void
jniVerboseGetID(const char *function, JNIEnv *env, jclass classRef, const char *name, const char *sig)
{
	J9VMThread *vmThread = (J9VMThread *)env;
	
	if ( vmThread->javaVM->checkJNIData.options & JNICHK_VERBOSE) {
		J9UTF8 *className;
		PORT_ACCESS_FROM_VMC(vmThread);

		enterVM(vmThread);

		className = J9ROMCLASS_CLASSNAME(J9VM_J9CLASS_FROM_JCLASS(vmThread, classRef)->romClass);

		exitVM(vmThread);

		Trc_JNI_GetID(env, function, J9UTF8_DATA(className), name, sig);
		j9tty_printf(PORTLIB, "<JNI %s: %.*s.%s %s>\n",
			function,
			J9UTF8_LENGTH(className),
			J9UTF8_DATA(className),
			name,
			sig);
	}
}


static void printJnichkHelp(J9PortLibrary* portLib) {
	PORT_ACCESS_FROM_PORT(portLib);

	j9file_printf(PORTLIB, J9PORT_TTY_OUT, j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG, J9NLS_JNICHK_HELP_1, NULL), J9JVM_VERSION_STRING);
	j9file_printf(PORTLIB, J9PORT_TTY_OUT, J9_COPYRIGHT_STRING "\n\n");
	j9file_printf(PORTLIB, J9PORT_TTY_OUT, j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG, J9NLS_JNICHK_HELP_2, NULL));
	j9file_printf(PORTLIB, J9PORT_TTY_OUT, "\n");
	j9file_printf(PORTLIB, J9PORT_TTY_OUT, j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG, J9NLS_JNICHK_HELP_3, NULL));
	j9file_printf(PORTLIB, J9PORT_TTY_OUT, j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG, J9NLS_JNICHK_HELP_4, NULL));
	j9file_printf(PORTLIB, J9PORT_TTY_OUT, j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG, J9NLS_JNICHK_HELP_5, NULL));
	j9file_printf(PORTLIB, J9PORT_TTY_OUT, j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG, J9NLS_JNICHK_HELP_6, NULL));
	j9file_printf(PORTLIB, J9PORT_TTY_OUT, j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG, J9NLS_JNICHK_HELP_7, NULL));
	j9file_printf(PORTLIB, J9PORT_TTY_OUT, j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG, J9NLS_JNICHK_HELP_8, NULL));
	j9file_printf(PORTLIB, J9PORT_TTY_OUT, j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG, J9NLS_JNICHK_HELP_14, NULL));
	j9file_printf(PORTLIB, J9PORT_TTY_OUT, j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG, J9NLS_JNICHK_HELP_9, NULL));
	j9file_printf(PORTLIB, J9PORT_TTY_OUT, j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG, J9NLS_JNICHK_HELP_15, NULL));
	j9file_printf(PORTLIB, J9PORT_TTY_OUT, j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG, J9NLS_JNICHK_HELP_10, NULL));
	j9file_printf(PORTLIB, J9PORT_TTY_OUT, j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG, J9NLS_JNICHK_HELP_13, NULL));
	j9file_printf(PORTLIB, J9PORT_TTY_OUT, j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG, J9NLS_JNICHK_HELP_11, NULL));
	j9file_printf(PORTLIB, J9PORT_TTY_OUT, j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG, J9NLS_JNICHK_HELP_12, NULL));
	j9file_printf(PORTLIB, J9PORT_TTY_OUT, "\n");
}



void jniVerboseFindClass(const char* function, JNIEnv* env, const char* name) {
	J9VMThread* vmThread = (J9VMThread*)env;
	if ( vmThread->javaVM->checkJNIData.options & JNICHK_VERBOSE) {
		PORT_ACCESS_FROM_VMC( vmThread );
		Trc_JNI_FindClass(env, function, name);
		j9tty_printf(PORTLIB, "<JNI %s: %s>\n", function, name);
	}
}



static void
jniCheckPrintMethod(JNIEnv* env, U_32 level)
{
	J9VMThread * vmThread = (J9VMThread*)env;
	PORT_ACCESS_FROM_VMC(vmThread);
	J9SFMethodFrame * stackFrame = (J9SFMethodFrame*)((U_8*)vmThread->sp + (UDATA)vmThread->literals);
	J9Method* method = stackFrame->method;

	if (method != NULL) {
		J9UTF8 * className = J9ROMCLASS_CLASSNAME(UNTAGGED_METHOD_CP(method)->ramClass->romClass);
		J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
		J9UTF8 * name = J9ROMMETHOD_GET_NAME(UNTAGGED_METHOD_CP(method)->ramClass->romClass, romMethod);
		J9UTF8 * sig = J9ROMMETHOD_GET_SIGNATURE(UNTAGGED_METHOD_CP(method)->ramClass->romClass, romMethod);

		/* special case for JNI_OnLoad */
		if (isLoadLibraryWithPath(className, name)) {
			jniCheckPrintJNIOnLoad(env, level);
		} else {
			switch (level) {
			case J9NLS_ERROR:
			default:
				j9nls_printf(PORTLIB, level, J9NLS_JNICHK_ERROR_IN_METHOD, J9UTF8_LENGTH(className), J9UTF8_DATA(className), J9UTF8_LENGTH(name), J9UTF8_DATA(name), J9UTF8_LENGTH(sig), J9UTF8_DATA(sig));
				break;
			case J9NLS_WARNING:
				j9nls_printf(PORTLIB, level, J9NLS_JNICHK_WARNING_IN_METHOD, J9UTF8_LENGTH(className), J9UTF8_DATA(className), J9UTF8_LENGTH(name), J9UTF8_DATA(name), J9UTF8_LENGTH(sig), J9UTF8_DATA(sig));
				break;
			case J9NLS_INFO:
				j9nls_printf(PORTLIB, level, J9NLS_JNICHK_ADVICE_IN_METHOD, J9UTF8_LENGTH(className), J9UTF8_DATA(className), J9UTF8_LENGTH(name), J9UTF8_DATA(name), J9UTF8_LENGTH(sig), J9UTF8_DATA(sig));
				break;
			}
		}
	} else if (stackFrame->savedPC == J9SF_FRAME_TYPE_END_OF_STACK) {
			switch (level) {
			case J9NLS_ERROR:
			default:
				j9nls_printf(PORTLIB, level, J9NLS_JNICHK_ERROR_IN_OUTER_FRAME);
				break;
			case J9NLS_WARNING:
				j9nls_printf(PORTLIB, level, J9NLS_JNICHK_WARNING_IN_OUTER_FRAME);
				break;
			case J9NLS_INFO:
				j9nls_printf(PORTLIB, level, J9NLS_JNICHK_ADVICE_IN_OUTER_FRAME);
				break;
			}
	} else {
			switch (level) {
			case J9NLS_ERROR:
			default:
				j9nls_printf(PORTLIB, level, J9NLS_JNICHK_ERROR_IN_EVENT_FRAME);
				break;
			case J9NLS_WARNING:
				j9nls_printf(PORTLIB, level, J9NLS_JNICHK_WARNING_IN_EVENT_FRAME);
				break;
			case J9NLS_INFO:
				j9nls_printf(PORTLIB, level, J9NLS_JNICHK_ADVICE_IN_EVENT_FRAME);
				break;
			}
	}
}


static UDATA jniIsLocalRef(JNIEnv * currentEnv, JNIEnv* env, jobject reference)
{
	J9VMThread *vmThread = (J9VMThread *) env;
	J9VMThread *currentThread = (J9VMThread*)currentEnv;

	if (vmThread->javaVM->checkJNIData.options  & JNICHK_PEDANTIC) {
		/* fast case - is the local ref an immediate in the top frame? */
		U_8 *bp = (U_8 *) vmThread->sp + (UDATA) vmThread->literals;
		if (
			((U_8 *) reference >= (U_8 *) vmThread->sp && (U_8 *) reference <= bp) ||
			((U_8 *) reference >= bp + sizeof(J9SFMethodFrame) && (U_8 *) reference <= (U_8 *) vmThread->arg0EA)
		) {
			/* just make sure that the ref is aligned and hasn't been NULL'ed */
			return ((UDATA) reference & (sizeof(UDATA) - 1)) == 0 && *(j9object_t*) reference != NULL;
		} else {
			J9StackWalkState walkState;

			enterVM(vmThread);

			walkState.userData1 = reference;
			walkState.userData2 = vmThread->jniLocalReferences;
			walkState.userData3 = NULL;
			walkState.flags = J9_STACKWALK_ITERATE_FRAMES | J9_STACKWALK_ITERATE_O_SLOTS | J9_STACKWALK_SKIP_INLINES;
			walkState.skipCount = 0;
			walkState.frameWalkFunction = jniIsLocalRefFrameWalkFunction;
			walkState.objectSlotWalkFunction = jniIsLocalRefOSlotWalkFunction;
			walkState.walkThread = vmThread;

			vmThread->javaVM->walkStackFrames(vmThread, &walkState);

			exitVM(vmThread);

			return walkState.userData3 == reference;
		}
	} else {
		J9JNIReferenceFrame* frame;
		J9JavaStack* stack = vmThread->stackObject;
		UDATA rc = 0;

		/* quick check - just check if it's a reference to a stack */
		while (stack != NULL) {
			if ((U_8*)reference < (U_8*)stack->end && (U_8*)reference >= (U_8*)(stack+1)) {
				return ((UDATA) reference & (sizeof(UDATA) - 1)) == 0 && *(j9object_t*) reference != NULL;
			}
			stack = stack->previous;
		}

		/* is it in the local references? */
		frame = (J9JNIReferenceFrame*)vmThread->jniLocalReferences;
		if (frame != NULL) {
			enterVM(vmThread);
			while (frame != NULL) {
				/* walk the pool */
				if (pool_includesElement(frame->references, reference)) {
					rc = 1;
					break;
				}
				frame = frame->previous;
			}
			exitVM(vmThread);
		}

		return rc;
	}
}



static UDATA
jniIsGlobalRef(JNIEnv* env, jobject reference)
{
	J9VMThread* vmThread = (J9VMThread*)env;
	J9JavaVM* vm = vmThread->javaVM;
	UDATA rc;
	JNICHK_GREF_HASHENTRY entry;
	JNICHK_GREF_HASHENTRY* actualResult;

	enterVM(vmThread);

#ifdef J9VM_THR_PREEMPTIVE
	omrthread_monitor_enter(vm->jniFrameMutex);
#endif
	/* walk the JNIGlobalReferences pool */
	rc = pool_includesElement(vm->jniGlobalReferences, reference);

	if (!rc) {
		j9object_t heapclass;

		/* The address of the classObject slot of a J9Class can be used as a global reference. */
		heapclass = *(j9object_t*)reference;

		/* search for the reference in the hashtable */
		entry.reference = (UDATA)reference;
		actualResult = hashTableFind(vm->checkJNIData.jniGlobalRefHashTab, &entry);
		if (NULL == actualResult || (NULL != actualResult && actualResult->alive) ) {
			/* verify that the object is a j.l.Class */
			if (J9VM_IS_INITIALIZED_HEAPCLASS(vmThread, heapclass)) {
				rc = (reference == (jobject)&(J9VM_J9CLASS_FROM_HEAPCLASS(vmThread, heapclass)->classObject));
			}
		}
	}
#ifdef J9VM_THR_PREEMPTIVE
	omrthread_monitor_exit(vm->jniFrameMutex);
#endif

	exitVM(vmThread);

	return rc;
}




static void jniIsLocalRefOSlotWalkFunction(J9VMThread* aThread, J9StackWalkState* walkState, j9object_t*slot, const void * stackLocation) {

	if (slot == walkState->userData1) {
		walkState->userData3 = (void *)stackLocation;
#if 0
{
	J9Method* method = walkState->method;
	if (method != NULL) {
		J9UTF8 * className = J9ROMCLASS_CLASSNAME(UNTAGGED_METHOD_CP(method)->ramClass->romClass);
		J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
		J9UTF8 * name = J9ROMMETHOD_GET_NAME(UNTAGGED_METHOD_CP(method)->ramClass->romClass, romMethod);
		J9UTF8 * sig = J9ROMMETHOD_GET_SIGNATURE(UNTAGGED_METHOD_CP(method)->ramClass->romClass, romMethod);
		printf("Found in frame %d in %.*s.%.*s%.*s\n", walkState->framesWalked + 1, J9UTF8_LENGTH(className), J9UTF8_DATA(className), J9UTF8_LENGTH(name), J9UTF8_DATA(name), J9UTF8_LENGTH(sig), J9UTF8_DATA(sig));
	}
}
#endif
	}
}


static UDATA jniIsLocalRefFrameWalkFunction(J9VMThread* vmThread, J9StackWalkState* walkState) {
	UDATA rc = J9_STACKWALK_KEEP_ITERATING;
	switch ((UDATA)walkState->pc) {
		case J9SF_FRAME_TYPE_JIT_JNI_CALLOUT:
		case J9SF_FRAME_TYPE_JNI_NATIVE_METHOD:
			if (walkState->frameFlags & J9_SSF_CALL_OUT_FRAME_ALLOC) {
				J9JNIReferenceFrame* frame = walkState->userData2;
				UDATA frameType = 0;

				enterVM(vmThread);

				/* Walk the pools - each native frame with J9_SSF_CALL_OUT_FRAME_ALLOC set pushes
				 * and internal reference frame first, so stop once that one has been inspected.
				 */
				do {
					frameType = frame->type;
					if (J9_STACKWALK_KEEP_ITERATING == rc) {
						if (pool_includesElement(frame->references, walkState->userData1)) {
							walkState->userData3 = walkState->userData1;
							rc = J9_STACKWALK_STOP_ITERATING;
						}
					}
					frame = frame->previous;
				} while (frameType != JNIFRAME_TYPE_INTERNAL);
				walkState->userData2 = frame;

				exitVM(vmThread);
			}
	}

	return rc;
}


void
jniCheckRef(JNIEnv* env,  const char* function, IDATA argNum, jobject reference)
{
	J9VMThread* vmThread = (J9VMThread*)env;
	J9JavaVM* vm = vmThread->javaVM;
	UDATA options = vm->checkJNIData.options;

	if (JNICHK_NONFATAL == (options & JNICHK_NONFATAL)) {
		/* if nonfatal is specified, simply return if reference is NULL */
		if (vm->EsJNIFunctions->IsSameObject(env, NULL, reference)) {
			return;
		}
	}

	if (
		jniIsLocalRef(env, env, reference)
		|| jniIsGlobalRef(env, reference)
		|| jniIsWeakGlobalRef(env, reference)
	) {
		return;
	} else {
		/* -1 means it's the return value rather than a parameter */
		if (argNum == -1) {
			jniCheckFatalErrorNLS(env, J9NLS_JNICHK_RETURN_IS_NOT_REF, reference, getRefType(env, reference));
		} else {
			jniCheckFatalErrorNLS(env, J9NLS_JNICHK_ARGUMENT_IS_NOT_REF, function, argNum, reference, getRefType(env, reference));
		}
	}
}



static UDATA jniIsWeakGlobalRef(JNIEnv* env, jobject reference) {
	J9VMThread* vmThread = (J9VMThread*)env;
	J9JavaVM* vm = vmThread->javaVM;
	UDATA rc;

	enterVM(vmThread);

#ifdef J9VM_THR_PREEMPTIVE
	omrthread_monitor_enter(vm->jniFrameMutex);
#endif
	/* walk the JNIWeakGlobalReferences pool */
	rc = pool_includesElement(vm->jniWeakGlobalReferences, reference);
#ifdef J9VM_THR_PREEMPTIVE
	omrthread_monitor_exit(vm->jniFrameMutex);
#endif

	exitVM(vmThread);

	return rc;
}


static const char*
getRefType(JNIEnv* env, jobject reference)
{
	JNIEnv* thread;
	J9VMThread *vmThread = (J9VMThread *) env;
	PORT_ACCESS_FROM_ENV(env);

	if (reference == NULL) {
		return j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG, J9NLS_JNICHK_NULL_REF, NULL);
	}
	if (jniIsLocalRef(env, env, reference)) {
		return j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG, J9NLS_JNICHK_LOCAL_REF, NULL);
	}
	if (jniIsGlobalRef(env, reference)) {
		return j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG, J9NLS_JNICHK_GLOBAL_REF, NULL);
	}
	if (jniIsWeakGlobalRef(env, reference)) {
		return j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG, J9NLS_JNICHK_WEAK_GLOBAL_REF, NULL);
	}

	{
		enterVM(vmThread);
		omrthread_monitor_enter( vmThread->javaVM->vmThreadListMutex );
		thread = (JNIEnv*)((J9VMThread*)env)->linkNext;
		while (thread != env) {
			if (jniIsLocalRef(env, thread, reference)) {
				omrthread_monitor_exit( vmThread->javaVM->vmThreadListMutex );
				exitVM(vmThread);
				return j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG, J9NLS_JNICHK_BAD_LOCAL_REF, NULL);
			}
			thread = (JNIEnv*)((J9VMThread*)thread)->linkNext;
		}
		omrthread_monitor_exit( vmThread->javaVM->vmThreadListMutex );
		exitVM(vmThread);
	}

	if (*(j9object_t*)reference == NULL) {
		return j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG, J9NLS_JNICHK_FREED_LOCAL_REF, NULL);
	}

	return "unknown";
}


void jniCheckLocalRef(JNIEnv* env,  const char* function, IDATA argNum, jobject reference) {

	if (jniIsLocalRef(env, env, reference)) {
		return;
	} else {
		jniCheckFatalErrorNLS(env, J9NLS_JNICHK_ARGUMENT_IS_NOT_LOCAL_REF, function, argNum, reference, getRefType(env, reference));
	}
}


void jniCheckGlobalRef(JNIEnv* env,  const char* function, IDATA argNum, jobject reference) {

	if (jniIsGlobalRef(env, reference)) {
		return;
	} else {
		jniCheckFatalErrorNLS(env, J9NLS_JNICHK_ARGUMENT_IS_NOT_GLOBAL_REF, function, argNum, reference, getRefType(env, reference));
	}
}


void jniCheckWeakGlobalRef(JNIEnv* env,  const char* function, IDATA argNum, jobject reference) {

	if (jniIsWeakGlobalRef(env, reference)) {
		return;
	} else {
		jniCheckFatalErrorNLS(env, J9NLS_JNICHK_ARGUMENT_IS_NOT_WEAK_GLOBAL_REF, function, argNum, reference, getRefType(env, reference));
	}
}


static void
jniTraceObject(JNIEnv* env, jobject aJobject)
{
	J9VMThread *vmThread = (J9VMThread*)env;
	J9JavaVM * vm = vmThread->javaVM;
	J9Class* jlClass, *clazz;
	PORT_ACCESS_FROM_VMC(vmThread);

	jlClass = J9VMJAVALANGCLASS_OR_NULL(vm);

	clazz = jnichk_getObjectClazz(env, aJobject);

	if (clazz == NULL) {
		j9tty_printf(PORTLIB, "(jobject)NULL");
	} else if (clazz == jlClass) {
		J9UTF8* utf8;

		enterVM(vmThread);

		utf8 = J9ROMCLASS_CLASSNAME(J9VM_J9CLASS_FROM_JCLASS(vmThread, aJobject)->romClass);

		exitVM(vmThread);

		j9tty_printf(PORTLIB, "%.*s", J9UTF8_LENGTH(utf8), J9UTF8_DATA(utf8));
	} else {
		J9UTF8* utf8 = J9ROMCLASS_CLASSNAME( clazz->romClass );
		j9tty_printf(PORTLIB, "%.*s@%p", J9UTF8_LENGTH(utf8), J9UTF8_DATA(utf8), aJobject);
	}
}


static void jniTraceMethodID(JNIEnv* env, jmethodID mid) {
	J9VMThread *vmThread = (J9VMThread*)env;
	J9JavaVM * vm = vmThread->javaVM;
	PORT_ACCESS_FROM_VMC(vmThread);

	J9Method* method = ((J9JNIMethodID*)mid)->method;
	J9ROMMethod* romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
	J9UTF8* name = J9ROMMETHOD_GET_NAME(UNTAGGED_METHOD_CP(method)->ramClass->romClass, romMethod);
	J9UTF8* sig = J9ROMMETHOD_GET_SIGNATURE(UNTAGGED_METHOD_CP(method)->ramClass->romClass, romMethod);

	j9tty_printf(PORTLIB, "%.*s%.*s", J9UTF8_LENGTH(name), J9UTF8_DATA(name), J9UTF8_LENGTH(sig), J9UTF8_DATA(sig));

}


static void jniTraceFieldID(JNIEnv* env, jfieldID fid) {
	J9VMThread *vmThread = (J9VMThread*)env;
	J9JavaVM * vm = vmThread->javaVM;
	PORT_ACCESS_FROM_VMC(vmThread);

	J9ROMFieldShape* field = ((J9JNIFieldID*)fid)->field;
	J9UTF8* name = J9ROMFIELDSHAPE_NAME(field);

	j9tty_printf(PORTLIB, "%.*s", J9UTF8_LENGTH(name), J9UTF8_DATA(name));

}


static void
jniCheckObjectRange(JNIEnv* env, const char* function, jsize actualLen, jint start, jsize len)
{
	J9VMThread* vmThread = (J9VMThread*)env;

	/* caller should have already checked this, but check again to be safe */
	if ( vmThread->javaVM->checkJNIData.options & (JNICHK_NOWARN | JNICHK_NOBOUNDS)) {
		return;
	}

	if (len > 0) {
		if (start < 0) {
			jniCheckWarningNLS(env, J9NLS_JNICHK_NEGATIVE_INDEX, function, start);
		}
		if (start >= actualLen) {
			jniCheckWarningNLS(env, J9NLS_JNICHK_INDEX_OUT_OF_RANGE, function, start, actualLen);
		}
		if (start + len > actualLen) {
			jniCheckWarningNLS(env, J9NLS_JNICHK_END_OUT_OF_RANGE, function, start, len, actualLen);
		}
	} else if (len < 0) {
		jniCheckWarningNLS(env, J9NLS_JNICHK_NEGATIVE_LENGTH, function, len);
	}
}


void
jniCheckArrayRange(JNIEnv* env, const char* function, jarray array, jint start, jsize len)
{
	J9JavaVM* j9vm = ((J9VMThread*)env)->javaVM;

	if ( 0 == (j9vm->checkJNIData.options & (JNICHK_NOWARN | JNICHK_NOBOUNDS))) {
		jsize arrayLen = j9vm->EsJNIFunctions->GetArrayLength(env, array);
		jniCheckObjectRange(env, function, arrayLen, start, len);
	}
}


void
jniCheckStringRange(JNIEnv* env, const char* function, jstring string, jint start, jsize len)
{
	J9JavaVM* j9vm = ((J9VMThread*)env)->javaVM;
	if ( 0 == (j9vm->checkJNIData.options & (JNICHK_NOWARN | JNICHK_NOBOUNDS))) {
		jsize strLen = j9vm->EsJNIFunctions->GetStringLength(env, string);
		jniCheckObjectRange(env, function, strLen, start, len);
	}
}


void
jniCheckStringUTFRange(JNIEnv* env, const char* function, jstring string, jint start, jsize len)
{
	J9JavaVM* j9vm = ((J9VMThread*)env)->javaVM;
	if ( 0 == (j9vm->checkJNIData.options & (JNICHK_NOWARN | JNICHK_NOBOUNDS))) {
		jsize strLen = j9vm->EsJNIFunctions->GetStringUTFLength(env, string);
		jniCheckObjectRange(env, function, strLen, start, len);
	}
}


static void jniCheckValidClass(JNIEnv* env, const char* function, UDATA argNum, jobject obj) {
	J9VMThread *vmThread = (J9VMThread*)env;
	J9JavaVM *vm = vmThread->javaVM;
	UDATA classDepthAndFlags;
	UDATA options = vm->checkJNIData.options;
	J9ROMClass * romClass;
	J9Class *clazz;

	if ( JNICHK_NONFATAL == (options & JNICHK_NONFATAL)) {
		/* if nonfatal is specified, simply return if obj is null */
		if (vm->EsJNIFunctions->IsSameObject(env, NULL, obj) ) {
			return;
		}
	}

	{
		enterVM(vmThread);
		clazz = J9VM_J9CLASS_FROM_JCLASS((J9VMThread *)env, obj);
		classDepthAndFlags = J9CLASS_FLAGS(clazz);
		romClass = clazz->romClass;
		exitVM(vmThread);
	}

	if (classDepthAndFlags & J9AccClassHotSwappedOut) {
		J9UTF8* className = J9ROMCLASS_CLASSNAME(romClass);
		jniCheckFatalErrorNLS(env, J9NLS_JNICHK_OBSOLETE_CLASS, function, J9UTF8_LENGTH(className), J9UTF8_DATA(className));
	}
}



static void
jniCheckScalarArg(const char* function, JNIEnv* env, va_list* va, char code, UDATA argNum, UDATA trace) {
	J9JavaVM* vm = ((J9VMThread*)env)->javaVM;
	PORT_ACCESS_FROM_JAVAVM(vm);
	jbyte aJbyte;
	jboolean aJboolean;
	jchar aJchar;
	jint aJint;
	jdouble aJdouble;
	jlong aJlong;
	jshort aJshort;
	jfloat aJfloat;

	switch (code) {
		case 'B':	/* jbyte */
			aJbyte = va_arg(*va, int);
			jniCheckRange(env, function, "jbyte", (IDATA)aJbyte, argNum, -0x80, 0x7F);
			if (trace) {
				j9tty_printf(PORTLIB, "(jbyte)%d", (jint)aJbyte);
			}
			break;
		case 'Z':	/* jboolean */
			aJboolean = va_arg(*va, int);
			jniCheckRange(env, function, "jboolean", (IDATA)aJboolean, argNum, 0, 1);
			if (trace) {
				j9tty_printf(PORTLIB, "%s", aJboolean ? "true" : "false");
			}
			break;
		case 'C':	/* jchar */
			aJchar = va_arg(*va, int);
			jniCheckRange(env, function, "jchar", (IDATA)aJchar, argNum, 0, 0xFFFF);
			if (trace) {
				j9tty_printf(PORTLIB, "(jchar)%d", (jint)aJchar);
			}
			break;
		case 'S':	/* jshort */
			aJshort = va_arg(*va, int);
			jniCheckRange(env, function, "jshort", (IDATA)aJshort, argNum, -0x8000, 0x7FFF);
			if (trace) {
				j9tty_printf(PORTLIB, "(jshort)%d", (jint)aJshort);
			}
			break;
		case 'I':	/* jint */
			aJint = va_arg(*va, jint);
#ifdef J9VM_ENV_DATA64
			jniCheckRange(env, function, "jint", (IDATA)aJint, argNum, -0x80000000LL, 0x7FFFFFFFLL);
#endif
			if (trace) {
				j9tty_printf(PORTLIB, "(jint)%d", (jint)aJint);
			}
			break;
		case 'J':	/* jlong */
			aJlong = va_arg(*va, jlong);
			if (trace) {
				j9tty_printf(PORTLIB, "(jlong)%lld", aJlong);
			}
			break;
		case 'F':	/* jfloat */
			aJfloat = (jfloat)va_arg(*va, jdouble);
			if (trace) {
				j9tty_printf(PORTLIB, "(jfloat)%lf", (jdouble)aJfloat);
			}
			break;
		case 'D':	/* jdouble */
			aJdouble = va_arg(*va, jdouble);
			if (trace) {
				j9tty_printf(PORTLIB, "(jdouble)%lf", (jdouble)aJdouble);
			}
			break;

		default:
			jniCheckFatalErrorNLS(env, J9NLS_JNICHK_BAD_DESCRIPTOR, function, (UDATA)code);
		}

}



void jniCheckLocalRefTracking(JNIEnv* env, const char* function, J9JniCheckLocalRefState* savedState) {
	J9JavaVM* vm = ((J9VMThread*)env)->javaVM;
	J9JniCheckLocalRefState currentState;
	PORT_ACCESS_FROM_JAVAVM(vm);

	jniCheckPushCount(env, function);

	if ( (vm->checkJNIData.options & JNICHK_NOWARN)) return;

	fillInLocalRefTracking(env, &currentState);

	/* check the global pools first */
	if (currentState.globalRefCapacity > savedState->globalRefCapacity) {
		jniCheckWarningNLS(env,
			J9NLS_JNICHK_GREW_GLOBAL_REF_POOL,
			function,
			savedState->globalRefCapacity,
			currentState.globalRefCapacity);
	}
	if (currentState.weakRefCapacity > savedState->weakRefCapacity) {
		jniCheckWarningNLS(env,
			J9NLS_JNICHK_GREW_WEAK_GLOBAL_REF_POOL,
			function,
			savedState->weakRefCapacity,
			currentState.weakRefCapacity);
	}

	if (currentState.framesPushed != savedState->framesPushed) {
		if (currentState.framesPushed > 1) {
			/* the pushed frame was explicit. Don't report it */
			return;
		}
		if (currentState.framesPushed < savedState->framesPushed) {
			/* the number of frames decreased */
			return;
		}
	} else if (currentState.topFrameCapacity == savedState->topFrameCapacity) {
		/* no change of capacity */
		return;
	}

	jniCheckWarningNLS(env,
		J9NLS_JNICHK_GREW_LOCAL_REF_FRAME,
		function,
		savedState->numLocalRefs,
		currentState.topFrameCapacity + J9_SSF_CO_REF_SLOT_CNT,
		currentState.numLocalRefs);
}


static void fillInLocalRefTracking(JNIEnv* env, J9JniCheckLocalRefState* refTracking) {
	J9VMThread *vmThread = (J9VMThread*)env;
	J9SFJNINativeMethodFrame* frame;

	refTracking->framesPushed = 0;

	frame = (J9SFJNINativeMethodFrame*)((U_8*)vmThread->sp + (UDATA)vmThread->literals);
	if (frame->specialFrameFlags & J9_SSF_CALL_OUT_FRAME_ALLOC) {
		J9JNIReferenceFrame *frame = (J9JNIReferenceFrame*)vmThread->jniLocalReferences;
		UDATA frameType;

		refTracking->numLocalRefs = J9_SSF_CO_REF_SLOT_CNT;
		refTracking->topFrameCapacity = pool_capacity(frame->references);

		do {
			frameType = frame->type;
			refTracking->numLocalRefs += pool_numElements(frame->references);
			frame = frame->previous;
			refTracking->framesPushed += 1;
		} while (frame && frameType != JNIFRAME_TYPE_INTERNAL);
	} else {
		refTracking->numLocalRefs = ((UDATA)vmThread->literals) / sizeof(UDATA);
		refTracking->topFrameCapacity = J9_SSF_CO_REF_SLOT_CNT;
	}

	refTracking->globalRefCapacity = pool_capacity(vmThread->javaVM->jniGlobalReferences);
	refTracking->weakRefCapacity = pool_capacity(vmThread->javaVM->jniWeakGlobalReferences);
}




static void
methodEnterHook(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMNativeMethodEnterEvent* event = eventData;
	J9VMThread* vmThread = event->currentThread;
	J9Method* method = event->method;
	void* arg0EA = event->arg0EA;
	UDATA trace = vmThread->javaVM->checkJNIData.options & JNICHK_TRACE;

	if (trace) {
		PORT_ACCESS_FROM_VMC(vmThread);
		J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
		J9UTF8 * nameUTF = J9ROMMETHOD_GET_NAME(UNTAGGED_METHOD_CP(method)->ramClass->romClass, romMethod);
		J9UTF8 * sigUTF = J9ROMMETHOD_GET_SIGNATURE(UNTAGGED_METHOD_CP(method)->ramClass->romClass, romMethod);
		J9UTF8 * classNameUTF = J9ROMCLASS_CLASSNAME(J9_CLASS_FROM_METHOD(method)->romClass);
		char argBuffer[2048 + 3];
		UDATA remainingSize = sizeof(argBuffer) - 3;
		char * current = argBuffer;
		UDATA sigChar;
		U_8 * sigData = J9UTF8_DATA(sigUTF) + 1;
		UDATA * argPtr = arg0EA;
		UDATA written;
		UDATA indent = (UDATA) omrthread_tls_get(vmThread->osThread, jniEntryCountKey);

		j9tty_printf(PORTLIB, "%p %*sCall JNI: %.*s.%.*s%.*s {\n", vmThread, indent * 2, "", (U_32) J9UTF8_LENGTH(classNameUTF), J9UTF8_DATA(classNameUTF), (U_32) J9UTF8_LENGTH(nameUTF), J9UTF8_DATA(nameUTF), (U_32) J9UTF8_LENGTH(sigUTF), J9UTF8_DATA(sigUTF));
		++indent;
		omrthread_tls_set(vmThread->osThread, jniEntryCountKey, (void*) indent);

		argBuffer[0] = '\0';
		if (!(romMethod->modifiers & J9AccStatic)) {
			written = j9str_printf(PORTLIB, current, remainingSize, "receiver ");
			current += written;
			remainingSize -= written;
			jniDecodeValue(vmThread, 'L', argPtr, &current, &remainingSize);
			--argPtr;
		}
		while ((sigChar = jniNextSigChar(&sigData)) != ')') {
			if (argPtr != arg0EA) {
				written = j9str_printf(PORTLIB, current, remainingSize, ", ");
				current += written;
				remainingSize -= written;
			}
			if (sigChar == 'J' || sigChar == 'D') --argPtr;
			if (jniDecodeValue(vmThread, sigChar, argPtr, &current, &remainingSize) < 0) {
				break;
			}
			--argPtr;
		}

		j9tty_printf(PORTLIB, "%p %*sArguments: %s\n", vmThread, indent * 2, "", (argPtr == arg0EA) ? "void" : argBuffer);
	}
}


static void
methodExitHook(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9VMNativeMethodReturnEvent* event = eventData;
	J9VMThread* vmThread = event->currentThread;
	J9Method* method = event->method;
	PORT_ACCESS_FROM_VMC(vmThread);
	UDATA trace = vmThread->javaVM->checkJNIData.options  & JNICHK_TRACE;
	UDATA* returnValues = event->returnValuePtr;
	jobject returnRef = event->poppedByException ? (jobject)NULL : event->returnRef;
	J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
	J9UTF8 * sigUTF = J9ROMMETHOD_GET_SIGNATURE(UNTAGGED_METHOD_CP(method)->ramClass->romClass, romMethod);
	U_8 * sigData = J9UTF8_DATA(sigUTF) + 1;
    UDATA sigChar;

	/* check for unreleased memory (a warning) before checking the critical section count */
	jniCheckForUnreleasedMemory( (JNIEnv*)vmThread );

	if ((vmThread->jniCriticalCopyCount != 0) || (vmThread->jniCriticalDirectCount != 0)) {
		jniCheckFatalErrorNLS((JNIEnv*)vmThread, J9NLS_JNICHK_UNRELEASED_CRITICAL_SECTION);
	}

	/* clear the potential pending exception (Java will now throw it) */
	jniCheckSetPotentialPendingException(NULL);

    /* if the return value is a reference check that it's valid */
	while (*sigData++ != ')');
	sigChar = *sigData;
	if (sigChar == '[') {
		sigChar = 'L';
	}

	if (sigChar == 'L') {
		if (returnRef) {
			jniCheckRef((JNIEnv*) vmThread, "", -1, returnRef);
		}
	}

	if (trace) {
		char argBuffer[1024];
		UDATA remainingSize = sizeof(argBuffer) - 1;
		char * current = argBuffer;
		UDATA indent = (UDATA) omrthread_tls_get(vmThread->osThread, jniEntryCountKey);

		/* Fill in returnValue as it would look on stack */

		if (event->poppedByException) {
			strcpy(argBuffer, "<exception>");
		} else {
			UDATA returnValue[2];

			returnValue[0] = returnValues[0];
#ifdef J9VM_ENV_DATA64
			if (sigChar != 'L' && sigChar != 'D' && sigChar != 'J') {
				*((U_32 *)returnValue) = (U_32) returnValues[0];
			}
#else
			if (sigChar == 'J' || sigChar == 'D') {
				returnValue[0] = returnValues[1];
				returnValue[1] = returnValues[0];
			}
#endif

			jniDecodeValue(vmThread, sigChar, returnValue, &current, &remainingSize);
			argBuffer[sizeof(argBuffer) - 1] = '\0';
		}
		j9tty_printf(PORTLIB, "%p %*sReturn: %s\n", vmThread, indent * 2, "", argBuffer);
		--indent;
		omrthread_tls_set(vmThread->osThread, jniEntryCountKey, (void*) indent);
		j9tty_printf(PORTLIB, "%p %*s}\n", vmThread, indent * 2, "");
	}
}


static IDATA
jniDecodeValue(J9VMThread * vmThread, UDATA sigChar, void * valuePtr, char ** outputBuffer, UDATA  * outputBufferLength) {
	PORT_ACCESS_FROM_VMC(vmThread);
	IDATA argSize = 1;
	UDATA written;
	double doubleValue;
	I_64 longValue;

	switch (sigChar) {
		case 'B':
			written = j9str_printf(PORTLIB, *outputBuffer, *outputBufferLength, "(jbyte)%d", *((I_32 *) valuePtr));
			break;
		case 'C':
			written = j9str_printf(PORTLIB, *outputBuffer, *outputBufferLength, "(jchar)%d", *((I_32 *) valuePtr));
			break;
		case 'D':
			memcpy(&doubleValue, valuePtr, 8);
			written = j9str_printf(PORTLIB, *outputBuffer, *outputBufferLength, "(jdouble)%lf", doubleValue);
			argSize = 2;
			break;
		case 'F':
			written = j9str_printf(PORTLIB, *outputBuffer, *outputBufferLength, "(jfloat)%lf", *((float *) valuePtr));
			break;
		case 'I':
			written = j9str_printf(PORTLIB, *outputBuffer, *outputBufferLength, "(jint)%d", *((I_32 *) valuePtr));
			break;
		case 'J':
			memcpy(&longValue, valuePtr, 8);
			written = j9str_printf(PORTLIB, *outputBuffer, *outputBufferLength, "(jlong)%lld", longValue);
			argSize = 2;
			break;
		case 'S':
			written = j9str_printf(PORTLIB, *outputBuffer, *outputBufferLength, "(jshort)%d", *((I_32 *) valuePtr));
			break;
		case 'Z':
			written = j9str_printf(PORTLIB, *outputBuffer, *outputBufferLength, "(jboolean)%s", *((I_32 *) valuePtr) ? "true" : "false");
			break;
		case 'L':
			written = j9str_printf(PORTLIB, *outputBuffer, *outputBufferLength, "(jobject)0x%p", *((UDATA *) valuePtr));
			break;
		default:
			written = j9str_printf(PORTLIB, *outputBuffer, *outputBufferLength, "void");
			argSize = 0;
			break;
	}

	if (written > *outputBufferLength)
		return -1;

	*outputBufferLength -= written;
	*outputBuffer += written;

	return argSize;
}



static UDATA jniNextSigChar(U_8 ** utfData)
{
	UDATA utfChar;
	U_8 * data = *utfData;

	utfChar = *data++;

	switch (utfChar) {
		case '[':
			do {
				utfChar = *data++;
			} while (utfChar == '[');
			if (utfChar != 'L') {
				utfChar = 'L';
				break;
			}
			/* Fall through to consume type name, utfChar == 'L' for return value */

		case 'L':
			while (*data++ != ';') ;
	}

	*utfData = data;
	return utfChar;
}


void jniCallInReturn_void(JNIEnv* env, void * vaptr) {
	jniCallInReturn((J9VMThread *) env, vaptr, NULL, 'V');
}


void jniCallInReturn_jbyte(JNIEnv* env, void * vaptr, jbyte result) {
	jniCallInReturn((J9VMThread *) env, vaptr, &result, 'B');
}


void jniCallInReturn_jboolean(JNIEnv* env, void * vaptr, jboolean result) {
	jniCallInReturn((J9VMThread *) env, vaptr, &result, 'Z');
}


void jniCallInReturn_jobject(JNIEnv* env, void * vaptr, jobject result) {
	jniCallInReturn((J9VMThread *) env, vaptr, &result, 'L');
}


void jniCallInReturn_jchar(JNIEnv* env, void * vaptr, jchar result) {
	jniCallInReturn((J9VMThread *) env, vaptr, &result, 'C');
}


void jniCallInReturn_jshort(JNIEnv* env, void * vaptr, jshort result) {
	jniCallInReturn((J9VMThread *) env, vaptr, &result, 'S');
}


void jniCallInReturn_jint(JNIEnv* env, void * vaptr, jint result) {
	jniCallInReturn((J9VMThread *) env, vaptr, &result, 'I');
}


void jniCallInReturn_jlong(JNIEnv* env, void * vaptr, jlong result) {
	jniCallInReturn((J9VMThread *) env, vaptr, &result, 'J');
}


void jniCallInReturn_jfloat(JNIEnv* env, void * vaptr, jfloat result) {
	jniCallInReturn((J9VMThread *) env, vaptr, &result, 'F');
}


void jniCallInReturn_jdouble(JNIEnv* env, void * vaptr, jdouble result) {
	jniCallInReturn((J9VMThread *) env, vaptr, &result, 'D');
}


static void jniCallIn(J9VMThread * vmThread) {
	UDATA trace = vmThread->javaVM->checkJNIData.options  & JNICHK_TRACE;

	jniCheckPushCount((JNIEnv *) vmThread, "about to call in");

	if (trace) {
		omrthread_tls_set(vmThread->osThread, jniEntryCountKey, (void*) (((UDATA) omrthread_tls_get(vmThread->osThread, jniEntryCountKey)) + 1));
	}
}


static void jniCallInReturn(J9VMThread * vmThread, void * vaptr, void *stackResult, UDATA returnType) {
	PORT_ACCESS_FROM_VMC(vmThread);
	char argBuffer[1024];
	UDATA remainingSize = sizeof(argBuffer) - 1;
	char * current = argBuffer;
	UDATA novalist = vmThread->javaVM->checkJNIData.options  & JNICHK_NOVALIST;
	UDATA trace = vmThread->javaVM->checkJNIData.options  & JNICHK_TRACE;
	UDATA indent = (UDATA) omrthread_tls_get(vmThread->osThread, jniEntryCountKey);

	jniCheckPushCount((JNIEnv *) vmThread, "return from call in");

	if (vaptr && !novalist) {
		*((U_32 *) vaptr) = BAD_VA_LIST;
	}

	if (trace) {
		jniDecodeValue(vmThread, returnType, stackResult, &current, &remainingSize);
		argBuffer[sizeof(argBuffer) - 1] = '\0';
		j9tty_printf(PORTLIB, "%p %*sReturn: %s\n", vmThread, indent * 2, "", (vmThread->currentException ? "<exception>" : argBuffer));
		--indent;
		omrthread_tls_set(vmThread->osThread, jniEntryCountKey,  (void*) indent);
		j9tty_printf(PORTLIB, "%p %*s}\n", vmThread, indent * 2, "");
	}
}


static int isLoadLibraryWithPath(J9UTF8* className, J9UTF8* methodName) {
	const size_t classNameLen = sizeof(LOAD_LIB_WITH_PATH_CLASS) - 1;
	const size_t methodNameLen = sizeof(LOAD_LIB_WITH_PATH_METHOD) - 1;

	/* we ignore the signature, since it may be different in CLDC vs. other class libs */
	return
		(J9UTF8_LENGTH(className) == classNameLen)
		&& (J9UTF8_LENGTH(methodName) == methodNameLen)
		&& (0 == memcmp(LOAD_LIB_WITH_PATH_CLASS, J9UTF8_DATA(className), classNameLen))
		&& (0 == memcmp(LOAD_LIB_WITH_PATH_METHOD, J9UTF8_DATA(methodName), methodNameLen));
}


static void
jniCheckPrintJNIOnLoad(JNIEnv *env, U_32 level)
{
	j9array_t *libName;
	UDATA size;
	UDATA alloc = FALSE;
	char *data;
	J9VMThread *vmThread = (J9VMThread*)env;
	PORT_ACCESS_FROM_VMC(vmThread);

	enterVM(vmThread);

	/* loadLibrary is a static method, and its first argument is the name of the library */
	libName = (j9array_t *)vmThread->arg0EA;
	size = J9INDEXABLEOBJECT_SIZE(vmThread, *libName);

#if defined(J9VM_GC_ARRAYLETS)
	{
		UDATA i;

		data = j9mem_allocate_memory(size, OMRMEM_CATEGORY_VM);
		if (data) {
			alloc = TRUE;
			for (i = 0; i < size; i++) {
				data[i] = J9JAVAARRAYOFBYTE_LOAD(vmThread, *libName, i);
			}
		} else {
			data = "";
			size = 0;
		}
	}
#else /* J9VM_GC_ARRAYLETS */
	data = (char *)(*libName + 1);
#endif /* J9VM_GC_ARRAYLETS */

	switch (level) {
	case J9NLS_ERROR:
	default:
		j9nls_printf(PORTLIB, level, J9NLS_JNICHK_ERROR_IN_ONLOAD, size, data);
		break;
	case J9NLS_WARNING:
		j9nls_printf(PORTLIB, level, J9NLS_JNICHK_WARNING_IN_ONLOAD, size, data);
		break;
	case J9NLS_INFO:
		j9nls_printf(PORTLIB, level, J9NLS_JNICHK_ADVICE_IN_ONLOAD, size, data);
		break;
	}
	
	if (alloc == TRUE) {
		j9mem_free_memory(data);
	}

	exitVM(vmThread);
}


static void jniCheckPushCount(JNIEnv* env, const char* function) {
	J9VMThread *vmThread = (J9VMThread *) env;
	UDATA refBytes = (((J9SFMethodFrame*)((U_8*)vmThread->sp + (UDATA)vmThread->literals))->specialFrameFlags & J9_SSF_JNI_PUSHED_REF_COUNT_MASK) * sizeof(UDATA);

	if ((UDATA) vmThread->literals < refBytes) {
		jniCheckFatalErrorNLS(env, J9NLS_JNICHK_INCORRECT_PUSH_COUNT, function, vmThread->literals, refBytes);
	}
}



jboolean
inBootstrapClass(JNIEnv* env)
{
	J9VMThread* vmThread = (J9VMThread*)env;
	J9Method* method = ((J9SFMethodFrame*)((U_8*)vmThread->sp + (UDATA)vmThread->literals))->method;

	if (method) {
		J9JavaVM *vm = vmThread->javaVM;
		J9InternalVMFunctions const * const vmFuncs = vm->internalVMFunctions;
		J9Class* clazz = J9_CLASS_FROM_METHOD(method);
		J9ClassLocation *classLocation = NULL;
		J9ClassPathEntry cpEntry;

		if (clazz->classLoader != vmThread->javaVM->systemClassLoader) {
			return JNI_FALSE;
		}

		omrthread_monitor_enter(vm->classLoaderModuleAndLocationMutex);
		classLocation = vmFuncs->findClassLocationForClass(vmThread, clazz);
		omrthread_monitor_exit(vm->classLoaderModuleAndLocationMutex);
		if (NULL != classLocation) {
			if (getClassPathEntry(vmThread, clazz->classLoader, classLocation->entryIndex, &cpEntry) == 0) {
				if (cpEntry.flags & CPE_FLAG_BOOTSTRAP) {
					J9UTF8 * className = J9ROMCLASS_CLASSNAME(clazz->romClass);
					J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
					J9UTF8 * name = J9ROMMETHOD_GET_NAME(UNTAGGED_METHOD_CP(method)->ramClass->romClass, romMethod);

					/* special case for JNI_OnLoad */
					if (isLoadLibraryWithPath(className, name)) {
						return JNI_FALSE;
					}
					return JNI_TRUE;
				}
			}
		}
	}

	return JNI_FALSE;
}


const char*
jniCheckGetPotentialPendingException(void)
{
	return omrthread_tls_get(omrthread_self(), potentialPendingExceptionKey);
}



void
jniCheckSetPotentialPendingException(const char* function)
{
	omrthread_tls_set(omrthread_self(), potentialPendingExceptionKey, (void*)function);
}



void
jniCheckPopLocalFrame(JNIEnv* env, const char* function)
{
	J9VMThread* vmThread = (J9VMThread*)env;
	J9SFJNINativeMethodFrame* frame = (J9SFJNINativeMethodFrame*)((U_8*)vmThread->sp + (UDATA)vmThread->literals);
	J9JNIReferenceFrame *referenceFrame = (J9JNIReferenceFrame*)vmThread->jniLocalReferences;

	if ( frame->specialFrameFlags & J9_SSF_CALL_OUT_FRAME_ALLOC ) {
		if ( referenceFrame != NULL ) {
			if ( referenceFrame->type == JNIFRAME_TYPE_USER ) {
				/* this is a valid frame to be popping */
				return;
			}
		}
	}

	jniCheckFatalErrorNLS(env, J9NLS_JNICHK_NO_LOCAL_FRAME_ON_STACK, function);
}


void
jniCheckFatalErrorNLS(JNIEnv* env, U_32 nlsModule, U_32 nlsIndex, ...)
{
	J9JavaVM* vm = ((J9VMThread*)env)->javaVM;
	UDATA options = vm->checkJNIData.options;

	if ( (options & JNICHK_INCLUDEBOOT) || !inBootstrapClass(env)) {
		PORT_ACCESS_FROM_JAVAVM(vm);
		va_list args;

		va_start(args, nlsIndex);
		j9nls_vprintf(J9NLS_ERROR, nlsModule, nlsIndex, args);
		va_end(args);

		jniCheckPrintMethod(env, J9NLS_ERROR);

		if ( options & JNICHK_NONFATAL) {
			j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_JNICHK_NONFATAL_ERROR);
		} else {
			j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_JNICHK_FATAL_ERROR);
			j9nls_printf(PORTLIB, J9NLS_INFO, J9NLS_JNICHK_FATAL_ERROR_ADVICE);
			vm->EsJNIFunctions->FatalError(env, "JNI error");
		}
	}
}


void
jniCheckWarningNLS(JNIEnv* env, U_32 nlsModule, U_32 nlsIndex, ...)
{
	J9JavaVM* vm = ((J9VMThread*)env)->javaVM;
	UDATA options = vm->checkJNIData.options;

	if ( 0 == (options & JNICHK_NOWARN)) {
		if ( (options & JNICHK_INCLUDEBOOT) || !inBootstrapClass(env)) {
			PORT_ACCESS_FROM_JAVAVM(vm);
			va_list args;

			va_start(args, nlsIndex);
			j9nls_vprintf(J9NLS_WARNING, nlsModule, nlsIndex, args);
			va_end(args);

			jniCheckPrintMethod(env, J9NLS_WARNING);
		}
	}
}


void
jniCheckAdviceNLS(JNIEnv* env, U_32 nlsModule, U_32 nlsIndex, ...)
{
	J9JavaVM* vm = ((J9VMThread*)env)->javaVM;
	UDATA options = vm->checkJNIData.options;

	if ( 0 == (options & JNICHK_NOADVICE)) {
		if ( (options & JNICHK_INCLUDEBOOT) || !inBootstrapClass(env)) {
			PORT_ACCESS_FROM_JAVAVM(vm);
			va_list args;

			va_start(args, nlsIndex);
			j9nls_vprintf(J9NLS_INFO, nlsModule, nlsIndex, args);
			va_end(args);

			jniCheckPrintMethod(env, J9NLS_INFO);
		}
	}
}


void
jniCheckDirectBuffer(JNIEnv* env, const char* function, IDATA argNum, jobject aJobject)
{
	J9JavaVM* j9vm = ((J9VMThread*)env)->javaVM;
	jclass superclazz = j9vm->EsJNIFunctions->FindClass(env, "java/nio/Buffer");

	if (superclazz == NULL) {
		(*env)->ExceptionClear(env);
	} else if (j9vm->EsJNIFunctions->IsInstanceOf(env, aJobject, superclazz)) {
		/* everything is OK. Do not issue a diagnostic */
		return;
	}

	/* either we failed to find java/nio/Buffer or aJobject is not a java/nio/Buffer. Either way, it can't be a java/nio/Buffer */
	jniCheckWarningNLS(env, J9NLS_JNICHK_ARGUMENT_IS_NOT_DIRECTBUFFER, function, argNum);
}


void
jniCheckReflectMethod(JNIEnv* env, const char* function, IDATA argNum, jobject aJobject)
{
	J9JavaVM* j9vm = ((J9VMThread*)env)->javaVM;
	jclass superclazz;

	superclazz = j9vm->EsJNIFunctions->FindClass(env, "java/lang/reflect/Method");
	if (superclazz == NULL) {
		(*env)->ExceptionClear(env);
	} else if (j9vm->EsJNIFunctions->IsInstanceOf(env, aJobject, superclazz)) {
		/* everything is OK. Do not issue a diagnostic */
		return;
	}

	superclazz = j9vm->EsJNIFunctions->FindClass(env, "java/lang/reflect/Constructor");
	if (superclazz == NULL) {
		(*env)->ExceptionClear(env);
	} else if (j9vm->EsJNIFunctions->IsInstanceOf(env, aJobject, superclazz)) {
		/* everything is OK. Do not issue a diagnostic */
		return;
	}

	/* either we failed to find java/nio/Buffer or aJobject is not a java/nio/Buffer. Either way, it can't be a java/nio/Buffer */
	jniCheckFatalErrorNLS(env, J9NLS_JNICHK_ARGUMENT_IS_NOT_REFLECTMETHOD, function, argNum);
}


static void
jniCheckCall(const char* function, JNIEnv* env, jobject receiver, UDATA methodType, UDATA returnType, jmethodID method)
{
	J9VMThread *vmThread = (J9VMThread *)env;
	J9JavaVM *vm = vmThread->javaVM;
	PORT_ACCESS_FROM_JAVAVM(vm);
	J9Method *ramMethod = ((J9JNIMethodID*)method)->method;
	J9Class *declaringClass = J9_CLASS_FROM_METHOD(ramMethod);
	jclass declaringClassRef;
	J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(ramMethod);
	J9UTF8 *sig = J9ROMMETHOD_GET_SIGNATURE(UNTAGGED_METHOD_CP(ramMethod)->ramClass->romClass,romMethod);
	char *returnSig;
	UDATA novalist = vm->checkJNIData.options & JNICHK_NOVALIST;

	jniCheckNull(env, function, 0, receiver);

	jniCallIn((J9VMThread *) env);

	if (methodType == METHOD_CONSTRUCTOR) {
		J9UTF8* name = J9ROMMETHOD_GET_NAME(UNTAGGED_METHOD_CP(ramMethod)->ramClass->romClass,romMethod);
		if ( (J9UTF8_DATA(name)[0] != '<') || (J9UTF8_LENGTH(name) != sizeof("<init>") - 1) ) {
			jniCheckFatalErrorNLS(env, J9NLS_JNICHK_METHOD_IS_NOT_CONSTRUCTOR, function);
		}
	}

	if ( ((romMethod->modifiers & J9AccStatic) == J9AccStatic) != (methodType == METHOD_STATIC) ) {
		if (methodType == METHOD_STATIC) {
			jniCheckFatalErrorNLS(env, J9NLS_JNICHK_METHOD_IS_NOT_STATIC, function);
		} else {
			jniCheckFatalErrorNLS(env, J9NLS_JNICHK_METHOD_IS_STATIC, function);
		}
	}

	returnSig = strchr((const char*)J9UTF8_DATA(sig), ')') + 1;
	if ( (UDATA)*returnSig != returnType && ((UDATA)*returnSig != '[' || returnType != 'L') ) {
		jniCheckFatalErrorNLS(env, J9NLS_JNICHK_METHOD_HAS_WRONG_RETURN_TYPE, function, *returnSig);
	}


	declaringClassRef = (jclass)&declaringClass->classObject;

	switch (methodType) {
		case METHOD_STATIC:
			if (!vm->EsJNIFunctions->IsAssignableFrom(env, receiver, declaringClassRef)) {
				jniCheckFatalErrorNLS(env, J9NLS_JNICHK_METHOD_INCORRECT_CLAZZ, function);
			}
			break;
		case METHOD_INSTANCE:
			if (!vm->EsJNIFunctions->IsInstanceOf(env, receiver, declaringClassRef)) {
				jniCheckFatalErrorNLS(env, J9NLS_JNICHK_METHOD_INELIGIBLE_RECEIVER, function);
			}
			break;
		case METHOD_CONSTRUCTOR:
			if (!vm->EsJNIFunctions->IsSameObject(env, receiver, declaringClassRef)) {
				jniCheckFatalErrorNLS(env, J9NLS_JNICHK_METHOD_INCORRECT_CLAZZ, function);
			}
			break;
	}

}


static void
jniCheckScalarArgA(const char* function, JNIEnv* env, jvalue* arg, char code, UDATA argNum, UDATA trace)
{
	J9JavaVM* vm = ((J9VMThread*)env)->javaVM;
	PORT_ACCESS_FROM_JAVAVM(vm);

	switch (code) {
		case 'B':	/* jbyte */
			if (trace) {
				j9tty_printf(PORTLIB, "(jbyte)%d", (jint)arg->b);
			}
			break;
		case 'Z':	/* jboolean */
			jniCheckRange(env, function, "jboolean", (IDATA)arg->z, argNum, 0, 1);
			if (trace) {
				j9tty_printf(PORTLIB, "%s", arg->z ? "true" : "false");
			}
			break;
		case 'C':	/* jchar */
			if (trace) {
				j9tty_printf(PORTLIB, "(jchar)%d", (jint)arg->c);
			}
			break;
		case 'S':	/* jshort */
			if (trace) {
				j9tty_printf(PORTLIB, "(jshort)%d", (jint)arg->s);
			}
			break;
		case 'I':	/* jint */
			if (trace) {
				j9tty_printf(PORTLIB, "(jint)%d", (jint)arg->i);
			}
			break;
		case 'J':	/* jlong */
			if (trace) {
				j9tty_printf(PORTLIB, "(jlong)%lld", arg->j);
			}
			break;
		case 'F':	/* jfloat */
			if (trace) {
				j9tty_printf(PORTLIB, "(jfloat)%lf", (jdouble)arg->f);
			}
			break;
		case 'D':	/* jdouble */
			if (trace) {
				j9tty_printf(PORTLIB, "(jdouble)%lf", (jdouble)arg->d);
			}
			break;

		default:
			jniCheckFatalErrorNLS(env, J9NLS_JNICHK_BAD_DESCRIPTOR, function, (UDATA)code);
		}

}


static char*
jniCheckObjectArg(const char* function, JNIEnv* env, jobject aJobject, char* sigArgs, UDATA argNum, UDATA trace)
{
	J9JavaVM* vm = ((J9VMThread*)env)->javaVM;
	PORT_ACCESS_FROM_JAVAVM(vm);

	switch (*sigArgs) {
	case 'L':
		while (*sigArgs != ';') {
			sigArgs++;
		}
		break;
	case '[':
		while (*sigArgs == '[') {
			sigArgs++;
		}
		if (*sigArgs == 'L') {
			while (*sigArgs != ';') {
				sigArgs++;
			}
		}
	}

	if (aJobject != NULL) {
		jniCheckRef(env, function, argNum, aJobject);
	}
	if (trace) {
		j9tty_printf(PORTLIB, "(jobject)0x%p", aJobject);
	}

	return sigArgs;
}


static UDATA
globrefHashTableHashFn(void *entry, void *userData)
{
	return ((JNICHK_GREF_HASHENTRY*)entry)->reference;
}

static UDATA
globrefHashTableEqualFn(void *leftEntry, void *rightEntry, void *userData)
{
	UDATA l = ((JNICHK_GREF_HASHENTRY*)leftEntry)->reference;
	UDATA r = ((JNICHK_GREF_HASHENTRY*)rightEntry)->reference;
	return l == r;
}

