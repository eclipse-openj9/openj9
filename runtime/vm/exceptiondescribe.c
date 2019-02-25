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

#include "j9.h"
#include "j9protos.h"
#include "j9consts.h"
#include "j2sever.h"
#include "rommeth.h"
#include "j9cp.h"
#include "j9vmnls.h"
#include "vmaccess.h"
#include "objhelp.h"
#include "vm_internal.h"
#include "jvminit.h"

typedef UDATA (*callback_func_t) (J9VMThread * vmThread, void * userData, J9ROMClass * romClass, J9ROMMethod * romMethod, J9UTF8 * fileName, UDATA lineNumber, J9ClassLoader* classLoader);

static void printExceptionInThread (J9VMThread* vmThread);
static UDATA isSubclassOfThreadDeath (J9VMThread *vmThread, j9object_t exception);
static void printExceptionMessage (J9VMThread* vmThread, j9object_t exception);


/* assumes VM access */
static void 
printExceptionInThread(J9VMThread* vmThread) 
{
	char* name;
	const char* format;

	PORT_ACCESS_FROM_VMC(vmThread);
	
	format = j9nls_lookup_message(
		J9NLS_INFO | J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, 
		J9NLS_VM_STACK_TRACE_EXCEPTION_IN, 
		"Exception in thread \"%.*s\" ");

	name = getOMRVMThreadName(vmThread->omrVMThread);

	j9tty_err_printf(PORTLIB, (char*)format, strlen(name), name);
	
	releaseOMRVMThreadName(vmThread->omrVMThread);
}


/* assumes VM access */
static void
printExceptionMessage(J9VMThread* vmThread, j9object_t exception) {
	char stackBuffer[256];
	char* buf = stackBuffer;
	UDATA length = 0;
	const char* separator = "";

	PORT_ACCESS_FROM_VMC(vmThread);

	J9UTF8* exceptionClassName = J9ROMCLASS_CLASSNAME(J9OBJECT_CLAZZ(vmThread, exception)->romClass);
	j9object_t detailMessage = J9VMJAVALANGTHROWABLE_DETAILMESSAGE(vmThread, exception);

	if (NULL != detailMessage) {
		buf = copyStringToUTF8WithMemAlloc(vmThread, detailMessage, J9_STR_NULL_TERMINATE_RESULT, "", 0, stackBuffer, 256, &length);

		if (NULL != buf) {
			separator = ": ";
		}
	}

	j9tty_err_printf(PORTLIB, "%.*s%s%.*s\n",
		(UDATA)J9UTF8_LENGTH(exceptionClassName),
		J9UTF8_DATA(exceptionClassName),
		separator,
		length, 
		buf);

	if (buf != stackBuffer) {
		j9mem_free_memory(buf);
	}
}


/* assumes VM access */
static UDATA
printStackTraceEntry(J9VMThread * vmThread, void * voidUserData, J9ROMClass *romClass, J9ROMMethod * romMethod, J9UTF8 * sourceFile, UDATA lineNumber, J9ClassLoader* classLoader) {
	const char* format;
	J9JavaVM *vm = vmThread->javaVM;
	J9InternalVMFunctions * vmFuncs = vm->internalVMFunctions;
	PORT_ACCESS_FROM_JAVAVM(vm);

	if (romMethod == NULL) {
		format = j9nls_lookup_message(
			J9NLS_INFO | J9NLS_DO_NOT_PRINT_MESSAGE_TAG, 
			J9NLS_VM_STACK_TRACE_UNKNOWN, 
			NULL);
		j9tty_err_printf(PORTLIB, (char*)format);
	} else {
		J9UTF8* className = J9ROMCLASS_CLASSNAME(romClass);
		J9UTF8* methodName = J9ROMMETHOD_GET_NAME(romClass, romMethod);
		char *sourceFileName = NULL;
		UDATA sourceFileNameLen = 0;
		char *moduleNameUTF = NULL;
		char *moduleVersionUTF = NULL;
		char nameBuf[J9VM_PACKAGE_NAME_BUFFER_LENGTH];
		char versionBuf[J9VM_PACKAGE_NAME_BUFFER_LENGTH];
		BOOLEAN freeModuleName = FALSE;
		BOOLEAN freeModuleVersion = FALSE;
		UDATA j2seVersion = J2SE_VERSION(vm) & J2SE_VERSION_MASK;

		if (j2seVersion >= J2SE_V11) {
			moduleNameUTF = JAVA_BASE_MODULE;
			moduleVersionUTF = JAVA_MODULE_VERSION;

			if (J9_ARE_ALL_BITS_SET(vm->runtimeFlags, J9_RUNTIME_JAVA_BASE_MODULE_CREATED)) {
				J9Module *module = NULL;
				U_8 *classNameUTF = J9UTF8_DATA(J9ROMCLASS_CLASSNAME(romClass));
				UDATA length = packageNameLength(romClass);

				omrthread_monitor_enter(vm->classLoaderModuleAndLocationMutex);
				module = vmFuncs->findModuleForPackage(vmThread, classLoader, classNameUTF, (U_32) length);
				omrthread_monitor_exit(vm->classLoaderModuleAndLocationMutex);

				if ((NULL != module) && (module != vm->javaBaseModule)) {
					moduleNameUTF = copyStringToUTF8WithMemAlloc(
						vmThread, module->moduleName, J9_STR_NULL_TERMINATE_RESULT, "", 0, nameBuf, J9VM_PACKAGE_NAME_BUFFER_LENGTH, NULL);
					if (nameBuf != moduleNameUTF) {
						freeModuleName = TRUE;
					}
					if (NULL != moduleNameUTF) {
						moduleVersionUTF = copyStringToUTF8WithMemAlloc(
							vmThread, module->version, J9_STR_NULL_TERMINATE_RESULT, "", 0, versionBuf, J9VM_PACKAGE_NAME_BUFFER_LENGTH, NULL);
						if (versionBuf != moduleVersionUTF) {
							freeModuleVersion = TRUE;
						}
					}
				}
			}
		}
		if (romMethod->modifiers & J9AccNative) {
			sourceFileName = "NativeMethod";
			sourceFileNameLen = LITERAL_STRLEN("NativeMethod");
		} else if (sourceFile) {
			sourceFileName = (char *)J9UTF8_DATA(sourceFile);
			sourceFileNameLen = (UDATA)J9UTF8_LENGTH(sourceFile);
		} else {
			sourceFileName = "Unknown Source";
			sourceFileNameLen = LITERAL_STRLEN("Unknown Source");
		}
		if (NULL != moduleNameUTF) {
			if (NULL != moduleVersionUTF) {
				if (lineNumber) {
					format = j9nls_lookup_message(
						J9NLS_DO_NOT_PRINT_MESSAGE_TAG,
						J9NLS_VM_STACK_TRACE_WITH_MODULE_VERSION_LINE,
						"\tat %.*s.%.*s (%s@%s/%.*s:%u)\n");
				} else {
					format = j9nls_lookup_message(
						J9NLS_DO_NOT_PRINT_MESSAGE_TAG,
						J9NLS_VM_STACK_TRACE_WITH_MODULE_VERSION,
						"\tat %.*s.%.*s (%s@%s/%.*s)\n");
				}
				j9tty_err_printf(PORTLIB, (char*)format,
					(UDATA)J9UTF8_LENGTH(className), J9UTF8_DATA(className),
					(UDATA)J9UTF8_LENGTH(methodName), J9UTF8_DATA(methodName),
					moduleNameUTF, moduleVersionUTF,
					sourceFileNameLen, sourceFileName,
					lineNumber); /* line number will be ignored in if it's not used in the format string */
				if (TRUE == freeModuleVersion) {
					j9mem_free_memory(moduleVersionUTF);
				}
			} else {
				if (lineNumber) {
					format = j9nls_lookup_message(
						J9NLS_DO_NOT_PRINT_MESSAGE_TAG,
						J9NLS_VM_STACK_TRACE_WITH_MODULE_LINE,
						"\tat %.*s.%.*s (%s/%.*s:%u)\n");
				} else {
					format = j9nls_lookup_message(
						J9NLS_DO_NOT_PRINT_MESSAGE_TAG,
						J9NLS_VM_STACK_TRACE_WITH_MODULE,
						"\tat %.*s.%.*s (%s/%.*s)\n");
				}
				j9tty_err_printf(PORTLIB, (char*)format,
					(UDATA)J9UTF8_LENGTH(className), J9UTF8_DATA(className),
					(UDATA)J9UTF8_LENGTH(methodName), J9UTF8_DATA(methodName),
					moduleNameUTF,
					sourceFileNameLen, sourceFileName,
					lineNumber); /* line number will be ignored in if it's not used in the format string */
			}
			if (TRUE == freeModuleName) {
				j9mem_free_memory(moduleNameUTF);
			}
		} else {
			if (lineNumber) {
				format = j9nls_lookup_message(
					J9NLS_DO_NOT_PRINT_MESSAGE_TAG,
					J9NLS_VM_STACK_TRACE_WITH_LINE,
					"\tat %.*s.%.*s (%.*s:%u)\n");
			} else {
				format = j9nls_lookup_message(
					J9NLS_DO_NOT_PRINT_MESSAGE_TAG,
					J9NLS_VM_STACK_TRACE,
					"\tat %.*s.%.*s (%.*s)\n");
			}
			j9tty_err_printf(PORTLIB, (char*)format,
				(UDATA)J9UTF8_LENGTH(className), J9UTF8_DATA(className),
				(UDATA)J9UTF8_LENGTH(methodName), J9UTF8_DATA(methodName),
				sourceFileNameLen, sourceFileName,
				lineNumber); /* line number will be ignored in if it's not used in the format string */
		}
	}

	return TRUE;
}


/* 
 * Walks the backtrace of an exception instance, invoking a user-supplied callback function for
 * each frame on the call stack.
 *
 * @param vmThread
 * @param exception The exception object that contains the backtrace.
 * @param callback The callback function to be invoked for each stack frame.
 * @param userData Opaque data pointer passed to the callback function.
 * @param pruneConstructors Non-zero if constructors should be pruned from the stack trace.
 * @return The number of times the callback function was invoked.
 *
 * @note Assumes VM access
 **/
UDATA
iterateStackTrace(J9VMThread * vmThread, j9object_t* exception, callback_func_t callback, void * userData, UDATA pruneConstructors)
{
	J9JavaVM * vm = vmThread->javaVM;
	UDATA totalEntries = 0;
	j9object_t walkback = J9VMJAVALANGTHROWABLE_WALKBACK(vmThread, (*exception));

	/* Note that exceptionAddr might be a pointer into the current thread's stack, so no java code is allowed to run
	   (nothing which could cause the stack to grow).
	*/

	if (walkback) {
		U_32 arraySize = J9INDEXABLEOBJECT_SIZE(vmThread, walkback);
		U_32 currentElement = 0;
		UDATA callbackResult = TRUE;

#ifndef J9VM_INTERP_NATIVE_SUPPORT
		pruneConstructors = FALSE;
#endif

		/* A zero terminates the stack trace - search backwards through the array to determine the correct size */

		while ((arraySize != 0) && (J9JAVAARRAYOFUDATA_LOAD(vmThread, walkback, arraySize-1)) == 0) {
			--arraySize;
		}

		/* Loop over the stack trace */

		while (currentElement != arraySize) {
			/* write as for or move currentElement++ to very end */ 
			UDATA methodPC = J9JAVAARRAYOFUDATA_LOAD(vmThread, J9VMJAVALANGTHROWABLE_WALKBACK(vmThread, (*exception)), currentElement);
			J9ROMMethod * romMethod = NULL;
			J9ROMClass *romClass = NULL;
			UDATA lineNumber = 0;
			J9UTF8 * fileName = NULL;
			J9ClassLoader *classLoader = NULL;
#ifdef J9VM_INTERP_NATIVE_SUPPORT
			J9JITExceptionTable * metaData = NULL;
			UDATA inlineDepth = 0;
			void * inlinedCallSite = NULL;
			void * inlineMap = NULL;
			J9JITConfig * jitConfig = vm->jitConfig;

			if (jitConfig) {
				metaData = jitConfig->jitGetExceptionTableFromPC(vmThread, methodPC);
				if (metaData) {
					inlineMap = jitConfig->jitGetInlinerMapFromPC(vm, metaData, methodPC);
					if (inlineMap) {
						inlinedCallSite = jitConfig->getFirstInlinedCallSite(metaData, inlineMap);
						if (inlinedCallSite) {
							inlineDepth = jitConfig->getJitInlineDepthFromCallSite(metaData, inlinedCallSite);
							totalEntries += inlineDepth;
						}
					}
				}
			}
#endif

			++currentElement; /* can't increment in J9JAVAARRAYOFUDATA_LOAD macro, so must increment here. */
			++totalEntries;
			if ((callback != NULL) || pruneConstructors) {
#ifdef J9VM_INTERP_NATIVE_SUPPORT
				if (metaData) {
					J9Method *ramMethod;
					J9Class *ramClass;
					UDATA isSameReceiver;
inlinedEntry:
					/* Check for metadata unload */
					if (NULL == metaData->ramMethod) {
						totalEntries = 0;
						goto done;
					}
					if (inlineDepth == 0) {
						if (inlineMap == NULL) {
							methodPC = -1;
							isSameReceiver = FALSE;
						} else {
							methodPC = jitConfig->getCurrentByteCodeIndexAndIsSameReceiver(metaData, inlineMap, NULL, &isSameReceiver);
						}
						ramMethod = metaData->ramMethod;
					} else {
						methodPC = jitConfig->getCurrentByteCodeIndexAndIsSameReceiver(metaData, inlineMap , inlinedCallSite, &isSameReceiver);
						ramMethod = jitConfig->getInlinedMethod(inlinedCallSite);
					}
					if (pruneConstructors) {
						if (isSameReceiver) {
							--totalEntries;
							goto nextInline;
						}
						pruneConstructors = FALSE;
					}
					romMethod = getOriginalROMMethodUnchecked(ramMethod);
					ramClass = J9_CLASS_FROM_CP(J9_CP_FROM_METHOD(ramMethod));
					romClass = ramClass->romClass;
					classLoader = ramClass->classLoader;
				} else {
					pruneConstructors = FALSE;
#endif
					romClass = findROMClassFromPC(vmThread, methodPC, &classLoader);
					if(romClass) {
						romMethod = findROMMethodInROMClass(vmThread, romClass, methodPC);
						if (romMethod != NULL) {
							methodPC -= (UDATA) J9_BYTECODE_START_FROM_ROM_METHOD(romMethod);
						}
					}
#ifdef J9VM_INTERP_NATIVE_SUPPORT
				}
#endif

#ifdef J9VM_OPT_DEBUG_INFO_SERVER
				if (romMethod != NULL) {
					lineNumber = getLineNumberForROMClassFromROMMethod(vm, romMethod, romClass, classLoader, methodPC);
					fileName = getSourceFileNameForROMClass(vm, classLoader, romClass);
				}
#endif

				/* Call the callback with the information */

				if (callback != NULL) {
					callbackResult = callback(vmThread, userData, romClass, romMethod, fileName, lineNumber, classLoader);
				}

#ifdef J9VM_OPT_DEBUG_INFO_SERVER
				if (romMethod != NULL) {
					releaseOptInfoBuffer(vm, romClass);
				}
#endif

				/* Abort the walk if the callback said to do so */

				if (!callbackResult) {
					break;
				}

#ifdef J9VM_INTERP_NATIVE_SUPPORT
nextInline:
				if (inlineDepth != 0) {
					/* Check for metadata unload */
					if (NULL == metaData->ramMethod) {
						totalEntries = 0;
						goto done;
					}
					--inlineDepth;
					inlinedCallSite = jitConfig->getNextInlinedCallSite(metaData, inlinedCallSite);
					goto inlinedEntry;
				}
#endif
			}
		}
	}
done:
	return totalEntries;
}

/**
 * This is an helper function to call exceptionDescribe indirectly from gpProtectAndRun function.
 * 
 * @param entryArg	Current VM Thread (JNIEnv * env)
 */
static UDATA
gpProtectedExceptionDescribe(void *entryArg)
{
	JNIEnv * env = (JNIEnv *)entryArg;
	
	exceptionDescribe(env);

	return 0;					/* return value required to conform to port library definition */
}

/**
 * Assumes VM access
 * 
 * Builds the exception	
 *
 * @param env    J9VMThread *
 *
 */
void   
internalExceptionDescribe(J9VMThread * vmThread)
{
	/* If the exception is NULL, do nothing.  Do not fetch the exception value into a local here as we do not have VM access yet. */

	if (vmThread->currentException != NULL) {
		J9JavaVM * vm = vmThread->javaVM;
		j9object_t exception;
		J9Class * eiieClass = NULL;

		/* Fetch and clear the current exception */

		exception = (j9object_t) vmThread->currentException;
		vmThread->currentException = NULL;

		/* In non-tiny VMs, do not print exception traces for ThreadDeath or its subclasses. */

		if (isSubclassOfThreadDeath(vmThread, exception)) {
			goto done;
		}

		/*Report surfaced exception */

		TRIGGER_J9HOOK_VM_EXCEPTION_DESCRIBE(vm->hookInterface, vmThread, exception);

		/* Print the exception thread */

		printExceptionInThread(vmThread);

		/* If the VM has completed initialization, try to print it in Java.  If initialization is not complete of the java call fails, print the exception natively. */

		if (vm->runtimeFlags & J9_RUNTIME_INITIALIZED) {
			PUSH_OBJECT_IN_SPECIAL_FRAME(vmThread, exception);
			printStackTrace(vmThread, exception);
			exception = POP_OBJECT_IN_SPECIAL_FRAME(vmThread);
			if (vmThread->currentException == NULL) {
				goto done;
			}
			vmThread->currentException = NULL;
		}

		do {
			/* Print the exception class name and detail message */

			printExceptionMessage(vmThread, exception);

			/* Print the stack trace entries */

			iterateStackTrace(vmThread, &exception, printStackTraceEntry, NULL, TRUE);

			/* If the exception is an instance of ExceptionInInitializerError, print the wrapped exception */

			if (eiieClass == NULL) {
				eiieClass = internalFindKnownClass(vmThread, J9VMCONSTANTPOOL_JAVALANGEXCEPTIONININITIALIZERERROR, J9_FINDKNOWNCLASS_FLAG_EXISTING_ONLY);
				vmThread->currentException = NULL;
			}
			if (J9OBJECT_CLAZZ(vmThread, exception) == eiieClass) {
#if JAVA_SPEC_VERSION >= 12
				exception = J9VMJAVALANGTHROWABLE_CAUSE(vmThread, exception);
#else
				exception = J9VMJAVALANGEXCEPTIONININITIALIZERERROR_EXCEPTION(vmThread, exception);
#endif /* JAVA_SPEC_VERSION */
			} else {
				break;
			}
		} while (exception != NULL);
	
done: ;
	}
}

/**
 * This function makes sure that call to "internalExceptionDescribe" is gpProtected
 * 
 * @param env	Current VM thread (J9VMThread *)
 */
void JNICALL   
exceptionDescribe(JNIEnv * env)
{
	J9VMThread * vmThread = (J9VMThread *) env;
	if (vmThread->currentException != NULL) {
		if (vmThread->gpProtected) {
			enterVMFromJNI(vmThread);
			internalExceptionDescribe(vmThread);
			exitVMToJNI(vmThread);
		} else {
			gpProtectAndRun(gpProtectedExceptionDescribe, env, (void *)env);
		}
	}
}


static UDATA
isSubclassOfThreadDeath(J9VMThread *vmThread, j9object_t exception)
{
	J9Class * threadDeathClass = J9VMJAVALANGTHREADDEATH_OR_NULL(vmThread->javaVM);
	J9Class * exceptionClass = J9OBJECT_CLAZZ(vmThread, exception);

	if (threadDeathClass != NULL) {
		UDATA tdDepth;

		if (threadDeathClass == exceptionClass) {
			return TRUE;
		}
		tdDepth = J9CLASS_DEPTH(threadDeathClass);
		if (J9CLASS_DEPTH(exceptionClass) > tdDepth) {
			if (exceptionClass->superclasses[tdDepth] == threadDeathClass) {
				return TRUE;
			}
		}
	}

	return FALSE;
}




