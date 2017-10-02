/*******************************************************************************
 * Copyright (c) 2012, 2017 IBM Corp. and others
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

#include "j9accessbarrier.h"
#include "j9protos.h"
#include "j9vmnls.h"
#include "ut_j9vm.h"
#include "util_api.h"
#include "vm_api.h"
#include "vm_internal.h"
#include "j9cp.h"
#include "jni.h"
#include "stackwalk.h"
#include "rommeth.h"
#include "j2sever.h"
#include "objhelp.h"

#include "VMHelpers.hpp"
#include "VMAccess.hpp"

extern "C" {

static bool isAccessibleToAllModulesViaReflection(J9VMThread *currentThread, J9Class *clazz, bool javaBaseLoaded);


UDATA
pushReflectArguments(J9VMThread *currentThread, j9object_t parameterTypes, j9object_t arguments)
{
	UDATA rc = 1; /* invalid argument count */
	U_32 typeCount = J9INDEXABLEOBJECT_SIZE(currentThread, parameterTypes);
	if (NULL == arguments) {
		if (0 != typeCount) {
			goto done;
		}
	} else if (J9INDEXABLEOBJECT_SIZE(currentThread, arguments) != typeCount) {
		goto done;
	}
	if (0 != typeCount) {
		J9JavaVM *vm = currentThread->javaVM;
		J9Class * const doubleClass = J9VMJAVALANGDOUBLE_OR_NULL(vm);
		J9Class * const longClass = J9VMJAVALANGLONG_OR_NULL(vm);
		J9Class * const floatClass = J9VMJAVALANGFLOAT_OR_NULL(vm);
		J9Class * const integerClass = J9VMJAVALANGINTEGER_OR_NULL(vm);
		J9Class * const byteClass = J9VMJAVALANGBYTE_OR_NULL(vm);
		J9Class * const characterClass = J9VMJAVALANGCHARACTER_OR_NULL(vm);
		J9Class * const shortClass = J9VMJAVALANGSHORT_OR_NULL(vm);
		J9Class * const booleanClass = J9VMJAVALANGBOOLEAN_OR_NULL(vm);
		J9Class * const booleanReflectClass = vm->booleanReflectClass;
		J9Class * const charReflectClass = vm->charReflectClass;
		J9Class * const floatReflectClass = vm->floatReflectClass;
		J9Class * const doubleReflectClass = vm->doubleReflectClass;
		J9Class * const byteReflectClass = vm->byteReflectClass;
		J9Class * const shortReflectClass = vm->shortReflectClass;
		J9Class * const intReflectClass = vm->intReflectClass;
		J9Class * const longReflectClass = vm->longReflectClass;
		UDATA *sp = currentThread->sp;
		rc = 2; /* illegal argument */
		for (U_32 i = 0; i < typeCount; ++i) {
			j9object_t argObject = J9JAVAARRAYOFOBJECT_LOAD(currentThread, arguments, i);
			j9object_t typeObject = J9JAVAARRAYOFOBJECT_LOAD(currentThread, parameterTypes, i);
			J9Class *type = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, typeObject);
			if (J9ROMCLASS_IS_PRIMITIVE_TYPE(type->romClass)) {
				if (NULL != argObject) {
					I_32 singleValue = 0;
					I_64 doubleValue = 0;
					J9Class *argType = J9OBJECT_CLAZZ(currentThread, argObject);
					if (argType == doubleClass) {
						doubleValue = J9VMJAVALANGDOUBLE_VALUE(currentThread, argObject);
					} else if (argType == longClass) {
						doubleValue = J9VMJAVALANGLONG_VALUE(currentThread, argObject);
					} else if (argType == floatClass) {
						singleValue = J9VMJAVALANGFLOAT_VALUE(currentThread, argObject);
					} else if (argType == integerClass) {
						singleValue = J9VMJAVALANGINTEGER_VALUE(currentThread, argObject);
					} else if (argType == byteClass) {
						singleValue = J9VMJAVALANGBYTE_VALUE(currentThread, argObject);
					} else if (argType == characterClass) {
						singleValue = J9VMJAVALANGCHARACTER_VALUE(currentThread, argObject);
					} else if (argType == shortClass) {
						singleValue = J9VMJAVALANGSHORT_VALUE(currentThread, argObject);
					} else if (argType == booleanClass) {
						singleValue = J9VMJAVALANGBOOLEAN_VALUE(currentThread, argObject);
					} else {
						goto done;
					}

					if (type == booleanReflectClass) {
						if (argType == booleanClass) {
pushSingle:
							sp -= 1;
							*(I_32*)sp = singleValue;
							continue;
						}
						goto done;
					}
					if (type == charReflectClass) {
						if (argType == characterClass) {
							goto pushSingle;
						}
						goto done;
					}
					if (type == byteReflectClass) {
						if (argType == byteClass) {
							goto pushSingle;
						}
						goto done;
					}
					if (type == shortReflectClass) {
						if ((argType == shortClass) || (argType == byteClass)) {
							goto pushSingle;
						}
						goto done;
					}
					if (type == intReflectClass) {
						if ((argType == integerClass) || (argType == characterClass) || (argType == shortClass) || (argType == byteClass)) {
							goto pushSingle;
						}
						goto done;
					}
					if (type == longReflectClass) {
						if (argType == longClass) {
pushDouble:
							sp -= 2;
							*(I_64*)sp = doubleValue;
							continue;
						}
						if ((argType == integerClass) || (argType == characterClass) || (argType == shortClass) || (argType == byteClass)) {
							doubleValue = (I_64)singleValue;
							goto pushDouble;
						}
						goto done;
					}
					if (type == floatReflectClass) {
						if (argType == floatClass) {
							goto pushSingle;
						}
						if ((argType == integerClass) || (argType == characterClass) || (argType == shortClass) || (argType == byteClass)) {
							sp -= 1;
							*(jfloat*)sp = (jfloat)singleValue;
							continue;
						}
						if (argType == longClass) {
							sp -= 1;
							*(jfloat*)sp = (jfloat)doubleValue;
							continue;
						}
						goto done;
					}
					if (type == doubleReflectClass) {
						if (argType == doubleClass) {
							goto pushDouble;
						}
						if ((argType == integerClass) || (argType == characterClass) || (argType == shortClass) || (argType == byteClass)) {
							sp -= 2;
							*(jdouble*)sp = (jdouble)singleValue;
							continue;
						}
						if (argType == longClass) {
							sp -= 2;
							*(jdouble*)sp = (jdouble)doubleValue;
							continue;
						}
						if (argType == floatClass) {
							sp -= 2;
							*(jdouble*)sp = (jdouble)*(jfloat*)&singleValue;
							continue;
						}
						goto done;
					}
				}
				goto done;
			} else {
				if (NULL != argObject) {
					if (0 == VM_VMHelpers::inlineCheckCast(J9OBJECT_CLAZZ(currentThread, argObject), type)) {
						goto done;
					}
				}
				sp -= 1;
				*(j9object_t*)sp = argObject;
			}
		}
		currentThread->sp = sp;
	}
	rc = 0; /* success */
done:
	return rc;
}

extern void c_cInterpreter(J9VMThread *currentThread);
extern J9NameAndSignature const * const exceptionConstructors[];
extern J9NameAndSignature const clinitNameAndSig;
extern J9NameAndSignature const throwableNameAndSig;
extern J9NameAndSignature const printStackTraceNameAndSig;
extern J9NameAndSignature const threadRunNameAndSig;
extern J9NameAndSignature const initNameAndSig;
extern J9NameAndSignature const initCauseNameAndSig;

#if defined(WIN32) && !defined(J9VM_ENV_DATA64)
extern void __cdecl win32ExceptionHandler(void);
extern UDATA getFS0(void);
extern void setFS0(UDATA);
#endif /* WIN32 && !J9VM_ENV_DATA64 */

#if defined(J9VM_PORT_ZOS_CEEHDLRSUPPORT)
#include "leconditionhandler.h"
extern _ENTRY globalLeConditionHandlerENTRY;
#endif /* J9VM_PORT_ZOS_CEEHDLRSUPPORT */

static VMINLINE J9Method*
mapVirtualMethod(J9VMThread *currentThread, J9Method *method, UDATA vTableIndex, J9Class *receiverClass)
{
	if (J9_ARE_ANY_BITS_SET(vTableIndex, J9_JNI_MID_INTERFACE)) {
		UDATA iTableIndex = vTableIndex & ~(UDATA)J9_JNI_MID_INTERFACE;
		J9Class *interfaceClass = J9_CLASS_FROM_METHOD(method);
		// TODO: should this code be handling Object methods?
		// refactor VMHelpers convertITableIndexToVTableIndex to take NAS?
		vTableIndex = 0;
		J9ITable * iTable = receiverClass->lastITable;
		if (interfaceClass == iTable->interfaceClass) {
			goto foundITable;
		}
		iTable = (J9ITable*)receiverClass->iTable;
		while (NULL != iTable) {
			if (interfaceClass == iTable->interfaceClass) {
				receiverClass->lastITable = iTable;
foundITable:
				vTableIndex = ((UDATA*)(iTable + 1))[iTableIndex];
				break;
			}
			iTable = iTable->next;
		}
	}
	if (0 != vTableIndex) {
		method = *(J9Method**)(((UDATA)receiverClass) + vTableIndex);
	}
	return method;
}

static VMINLINE void
javaOffloadSwitchOn(J9VMThread *currentThread)
{
#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
	J9JavaVM *vm = currentThread->javaVM;
	if (NULL != vm->javaOffloadSwitchOnWithReasonFunc) {
		if (0 == currentThread->javaOffloadState) {
			vm->javaOffloadSwitchOnWithReasonFunc(currentThread, J9_JNI_OFFLOAD_SWITCH_INTERPRETER);
		}
		currentThread->javaOffloadState += 1;
	}
#endif /* J9VM_OPT_JAVA_OFFLOAD_SUPPORT */
}

static VMINLINE void
javaOffloadSwitchOff(J9VMThread *currentThread)
{
#if defined(J9VM_OPT_JAVA_OFFLOAD_SUPPORT)
	J9JavaVM *vm = currentThread->javaVM;
	if (NULL != vm->javaOffloadSwitchOffWithReasonFunc) {
		currentThread->javaOffloadState -= 1;
		if (0 == currentThread->javaOffloadState) {
			vm->javaOffloadSwitchOffWithReasonFunc(currentThread, J9_JNI_OFFLOAD_SWITCH_INTERPRETER);
		}
	}
#endif /* J9VM_OPT_JAVA_OFFLOAD_SUPPORT */
}

static VMINLINE bool
buildCallInStackFrame(J9VMThread *currentThread, J9VMEntryLocalStorage *newELS, bool returnsObject = false, bool releaseVMAccess = false)
{
	Assert_VM_mustHaveVMAccess(currentThread);
	bool success = true;
	J9VMEntryLocalStorage *oldELS = currentThread->entryLocalStorage;
	UDATA flags = 0;
	J9SFJNICallInFrame *frame = ((J9SFJNICallInFrame*)currentThread->sp) - 1;
	javaOffloadSwitchOn(currentThread);
	if (NULL != oldELS) {
		/* Assuming oldELS > newELS, bytes used is (oldELS - newELS) */
		UDATA freeBytes = currentThread->currentOSStackFree;
		UDATA usedBytes = ((UDATA)oldELS - (UDATA)newELS);
		freeBytes -= usedBytes;
		currentThread->currentOSStackFree = freeBytes;
		if ((IDATA)freeBytes < 0) {
			if (J9_ARE_NO_BITS_SET(currentThread->privateFlags, J9_PRIVATE_FLAGS_CONSTRUCTING_EXCEPTION)) {
				setCurrentExceptionNLS(currentThread, J9VMCONSTANTPOOL_JAVALANGSTACKOVERFLOWERROR, J9NLS_VM_OS_STACK_OVERFLOW);
				currentThread->currentOSStackFree += usedBytes;
				success = false;
				javaOffloadSwitchOff(currentThread);
				goto done;
			}
		}
	}
	if (returnsObject) {
		flags |= J9_SSF_RETURNS_OBJECT;
	}
	if (releaseVMAccess) {
		flags |= J9_SSF_REL_VM_ACC;
	}
	frame->exitAddress = NULL;
	frame->specialFrameFlags = flags;
	frame->savedCP = currentThread->literals;
	frame->savedPC = currentThread->pc;
	frame->savedA0 = (UDATA*)((UDATA)currentThread->arg0EA | J9SF_A0_INVISIBLE_TAG);
	currentThread->sp = (UDATA*)frame;
	currentThread->pc = currentThread->javaVM->callInReturnPC;
	currentThread->literals = 0;
	currentThread->arg0EA = (UDATA*)&frame->savedA0;
	newELS->oldEntryLocalStorage = oldELS;
	currentThread->entryLocalStorage = newELS;
#if defined(WIN32) && !defined(J9VM_ENV_DATA64)
	if (J9_ARE_NO_BITS_SET(currentThread->javaVM->sigFlags, J9_SIG_XRS_SYNC)) {
		newELS->gpLink = (UDATA*)getFS0();
		newELS->gpHandler = (UDATA*)win32ExceptionHandler;
		newELS->currentVMThread = currentThread;
		setFS0((UDATA)&newELS->gpLink);
	}
#endif /* WIN32 && !J9VM_ENV_DATA64 */
#if defined(J9VM_PORT_ZOS_CEEHDLRSUPPORT)
	if (J9_ARE_ANY_BITS_SET(currentThread->javaVM->sigFlags, J9_SIG_ZOS_CEEHDLR)) {
		newELS->entryVMState = setVMState(currentThread, J9VMSTATE_INTERPRETER);
		if (J9_ARE_ANY_BITS_SET(currentThread->privateFlags, J9_PRIVATE_FLAGS_STACKS_OUT_OF_SYNC)) {
			javaAndCStacksMustBeInSync(currentThread, FALSE);
		}
		if (J9_ARE_NO_BITS_SET(currentThread->javaVM->sigFlags, J9_SIG_XRS_SYNC)) {
			if (J9_ARE_ANY_BITS_SET(currentThread->privateFlags, J9_PRIVATE_FLAGS_SKIP_THREAD_SIGNAL_PROTECTION)) {
				/* Do not call CEEHDLR, but clear the flag so that subsequent callins do make the call */
				currentThread->privateFlags &= ~J9_PRIVATE_FLAGS_SKIP_THREAD_SIGNAL_PROTECTION;
			} else {
				newELS->currentVMThread = currentThread;
				CEEHDLR(&globalLeConditionHandlerENTRY, (long*)&newELS->currentVMThread, NULL);
			}
		}
	}
#endif /* J9VM_PORT_ZOS_CEEHDLRSUPPORT */
done:
	return success;
}

static void
restoreCallInFrame(J9VMThread *currentThread)
{
	Assert_VM_mustHaveVMAccess(currentThread);
	J9SFJNICallInFrame *frame = ((J9SFJNICallInFrame*)(currentThread->arg0EA + 1)) - 1;
	UDATA const flags = frame->specialFrameFlags;
	bool releaseVMAccess = J9_ARE_ANY_BITS_SET(flags, J9_SSF_REL_VM_ACC);
	bool returnsObject = J9_ARE_ANY_BITS_SET(flags, J9_SSF_RETURNS_OBJECT);
	UDATA slot0 = currentThread->sp[0];
	UDATA slot1 = currentThread->sp[1];
	currentThread->literals = frame->savedCP;
	currentThread->pc = frame->savedPC;
	currentThread->arg0EA = (UDATA*)((UDATA)frame->savedA0 & ~(UDATA)J9SF_A0_INVISIBLE_TAG);
	currentThread->sp = (UDATA*)(frame + 1);
	if (VM_VMHelpers::exceptionPending(currentThread) || VM_VMHelpers::immediateAsyncPending(currentThread)) {
		currentThread->returnValue = 0;
		currentThread->returnValue2 = 0;
	} else {
		if (releaseVMAccess && returnsObject) {
			currentThread->returnValue = (UDATA)VM_VMHelpers::createLocalRef((JNIEnv*)currentThread, (j9object_t)slot0);
		} else {
			currentThread->returnValue = slot0;
			currentThread->returnValue2 = slot1;
		}
	}
	J9VMEntryLocalStorage *newELS = currentThread->entryLocalStorage;
	J9VMEntryLocalStorage *oldELS = newELS->oldEntryLocalStorage;
	if (NULL != oldELS) {
		UDATA usedBytes = ((UDATA)oldELS - (UDATA)newELS);
		currentThread->currentOSStackFree += usedBytes;
	}
#if defined(WIN32) && !defined(J9VM_ENV_DATA64)
	if (J9_ARE_NO_BITS_SET(currentThread->javaVM->sigFlags, J9_SIG_XRS_SYNC)) {
		setFS0((UDATA)newELS->gpLink);
	}
#endif /* WIN32 && !J9VM_ENV_DATA64 */
#if defined(J9VM_PORT_ZOS_CEEHDLRSUPPORT)
	if (J9_ARE_ANY_BITS_SET(currentThread->javaVM->sigFlags, J9_SIG_ZOS_CEEHDLR)) {
		setVMState(currentThread, newELS->entryVMState);
	}
#endif /* J9VM_PORT_ZOS_CEEHDLRSUPPORT */
	currentThread->entryLocalStorage = oldELS;
	javaOffloadSwitchOff(currentThread);
}

void JNICALL
sendClinit(J9VMThread *currentThread, J9Class *clazz, UDATA reserved1, UDATA reserved2, UDATA reserved3)
{
	Trc_VM_sendClinit_Entry(currentThread);
	J9VMEntryLocalStorage newELS;
	if (buildCallInStackFrame(currentThread, &newELS, false, false)) {
		/* Lookup the method */
		J9Method *method = (J9Method*)javaLookupMethod(currentThread, clazz, (J9ROMNameAndSignature*)&clinitNameAndSig, NULL, J9_LOOK_STATIC | J9_LOOK_NO_CLIMB | J9_LOOK_NO_THROW | J9_LOOK_DIRECT_NAS);
		/* If the method was found, run it */
		if (NULL != method) {
			Trc_VM_sendClinit_forClass(
					currentThread,
					J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(clazz->romClass)),
					J9UTF8_DATA(J9ROMCLASS_CLASSNAME(clazz->romClass)));
			currentThread->returnValue = J9_BCLOOP_RUN_METHOD;
			currentThread->returnValue2 = (UDATA)method;
			c_cInterpreter(currentThread);
		}
		restoreCallInFrame(currentThread);
	}
	Trc_VM_sendClinit_Exit(currentThread);
}

void JNICALL
sendLoadClass(J9VMThread *currentThread, j9object_t classLoaderObject, j9object_t classNameObject, UDATA reserved1, UDATA reserved2)
{
	Trc_VM_sendLoadClass_Entry(currentThread);
	J9VMEntryLocalStorage newELS;
	if (buildCallInStackFrame(currentThread, &newELS, true, false)) {
		/* Run the method from the vTable */
		UDATA vTableIndex = J9VMJAVALANGCLASSLOADER_LOADCLASS_REF(currentThread->javaVM)->methodIndexAndArgCount >> 8;
		J9Class *classLoaderClass = J9OBJECT_CLAZZ(currentThread, classLoaderObject);
		J9Method *method = *(J9Method**)(((UDATA)classLoaderClass) + vTableIndex);
		*--currentThread->sp = (UDATA)classLoaderObject;
		*--currentThread->sp = (UDATA)classNameObject;
		currentThread->returnValue = J9_BCLOOP_RUN_METHOD;
		currentThread->returnValue2 = (UDATA)method;
		c_cInterpreter(currentThread);
		restoreCallInFrame(currentThread);
	}
	Trc_VM_sendLoadClass_Exit(currentThread);
}

void JNICALL
cleanUpAttachedThread(J9VMThread *currentThread, UDATA reserved1, UDATA reserved2, UDATA reserved3, UDATA reserved4)
{
	Trc_VM_cleanUpAttachedThread_Entry(currentThread);
	J9VMEntryLocalStorage newELS;
	if (buildCallInStackFrame(currentThread, &newELS, false, true)) {
		j9object_t threadObject = currentThread->threadObject;
		if (NULL != threadObject) {
			/* Make sure no exception is pending when cleanup is called */
			VM_VMHelpers::clearException(currentThread);
			/* Run the method */
			*--currentThread->sp = (UDATA)threadObject;
			currentThread->returnValue = J9_BCLOOP_RUN_METHOD;
			currentThread->returnValue2 = (UDATA)J9VMJAVALANGJ9VMINTERNALS_THREADCLEANUP_METHOD(currentThread->javaVM);
			c_cInterpreter(currentThread);
		}
		restoreCallInFrame(currentThread);
	}
	Trc_VM_cleanUpAttachedThread_Exit(currentThread);
}

void JNICALL
handleUncaughtException(J9VMThread *currentThread, UDATA reserved1, UDATA reserved2, UDATA reserved3, UDATA reserved4)
{
	Trc_VM_handleUncaughtException_Entry(currentThread);
	J9VMEntryLocalStorage newELS;
	/* Called only if an exception is pending */
	j9object_t exception = currentThread->currentException;
	Assert_VM_notNull(exception);
	currentThread->currentException = NULL;
	/* Report uncaught exception (must be done before new frame) */
	TRIGGER_J9HOOK_VM_EXCEPTION_DESCRIBE(currentThread->javaVM->hookInterface, currentThread, exception);
	if (buildCallInStackFrame(currentThread, &newELS, false, true)) {
		j9object_t threadObject = currentThread->threadObject;
		if (NULL != threadObject) {
			/* Run the uncaughtException method on the current Thread */
			*--currentThread->sp = (UDATA)threadObject;
			*--currentThread->sp = (UDATA)exception;
			currentThread->returnValue = J9_BCLOOP_RUN_METHOD;
			currentThread->returnValue2 = (UDATA)J9VMJAVALANGTHREAD_UNCAUGHTEXCEPTION_METHOD(currentThread->javaVM);
			c_cInterpreter(currentThread);
		}
		restoreCallInFrame(currentThread);
	}
	Trc_VM_handleUncaughtException_Exit(currentThread);
}

void JNICALL
initializeAttachedThreadImpl(J9VMThread *currentThread, const char *name, j9object_t *group, UDATA daemon, J9VMThread *initializee)
{
	Trc_VM_initializeAttachedThread_Entry(currentThread);
	J9VMEntryLocalStorage newELS;
	if (buildCallInStackFrame(currentThread, &newELS, false, true)) {
		J9JavaVM *vm = currentThread->javaVM;
		/* Create cached OutOfMemoryError */
		j9object_t cachedOOM = createCachedOutOfMemoryError(currentThread, NULL);
		if (NULL != cachedOOM) {
			initializee->outOfMemoryError = cachedOOM;
			J9MemoryManagerFunctions const * const mmFuncs = vm->memoryManagerFunctions;
			/* See if a name was given for the thread (if so, this implies that it will be a daemon thread) */
			j9object_t threadName = NULL;
			if (NULL != name) {
				/* Allocate a String for the thread name */
				threadName = mmFuncs->j9gc_createJavaLangString(currentThread, (U_8*)name, strlen(name), 0);
				if (NULL == threadName) {
oom:
					setHeapOutOfMemoryError(currentThread);
					goto done;
				}
			}
			{
				/* Allocate the thread object */
				PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, threadName);
				j9object_t threadObject = mmFuncs->J9AllocateObject(currentThread, J9VMJAVALANGTHREAD_OR_NULL(vm), J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
				threadName = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);
				if (NULL == threadObject) {
					goto oom;
				}
				/* Link the thread and the object */
				initializee->threadObject = threadObject;
				J9VMJAVALANGTHREAD_SET_THREADREF(currentThread, threadObject, initializee);
				/* Run the Thread constructor */
				I_32 priority = J9THREAD_PRIORITY_NORMAL;
				if (J9_ARE_NO_BITS_SET(vm->runtimeFlags, J9_RUNTIME_NO_PRIORITIES)) {
					priority = (I_32)vm->j9Thread2JavaPriorityMap[omrthread_get_priority(initializee->osThread)];
					if (priority < J9THREAD_PRIORITY_USER_MIN) {
						priority = J9THREAD_PRIORITY_USER_MIN;
					} else if (priority > J9THREAD_PRIORITY_USER_MAX) {
						priority = J9THREAD_PRIORITY_USER_MAX;
					}
				}
				j9object_t threadGroup = (NULL == group) ? NULL : *group;
				*--currentThread->sp = (UDATA)threadObject;
				if (J2SE_SHAPE(vm) == J2SE_SHAPE_RAW) {
					/* Oracle constructor takes thread group, thread name */
					J9VMJAVALANGTHREAD_SET_PRIORITY(currentThread, threadObject, priority);
					J9VMJAVALANGTHREAD_SET_ISDAEMON(currentThread, threadObject, (I_32)daemon);
					*--currentThread->sp = (UDATA)threadGroup;
					*--currentThread->sp = (UDATA)threadName;
				} else {
					/* J9 constructor takes thread name, thread group, priority and isDaemon */
					J9VMJAVALANGTHREAD_SET_STARTED(currentThread, threadObject, JNI_TRUE);
					*--currentThread->sp = (UDATA)threadName;
					*--currentThread->sp = (UDATA)threadGroup;
					*(I_32*)--currentThread->sp = priority;
					*(I_32*)--currentThread->sp = (I_32)daemon;
				}
				currentThread->returnValue = J9_BCLOOP_RUN_METHOD;
				currentThread->returnValue2 = (UDATA)J9VMJAVALANGTHREAD_INIT_METHOD(vm);
				c_cInterpreter(currentThread);
			}
		}
done:
		restoreCallInFrame(currentThread);
	}
	Trc_VM_initializeAttachedThread_Exit(currentThread);
}

void JNICALL
internalSendExceptionConstructor(J9VMThread *currentThread, J9Class *exceptionClass, j9object_t exception, j9object_t detailMessage, UDATA constructorIndex)
{
	Trc_VM_internalSendExceptionConstructor_Entry(currentThread);
	J9VMEntryLocalStorage newELS;
	if (buildCallInStackFrame(currentThread, &newELS, false, false)) {
		/* Lookup constructor for exception class */
		constructorIndex &= J9_EX_CTOR_TYPE_MASK;
		J9NameAndSignature const *nas = exceptionConstructors[constructorIndex >> J9_EXCEPTION_INDEX_SHIFT];
		UDATA const lookupFlags = J9_LOOK_VIRTUAL | J9_LOOK_NO_CLIMB | J9_LOOK_NO_THROW | J9_LOOK_DIRECT_NAS;
		J9Method *method = (J9Method*)javaLookupMethod(currentThread, exceptionClass, (J9ROMNameAndSignature*)nas, NULL, lookupFlags);
		/* If the requested constructor is not found, just use <init>(String)V with no detail message */
		if (NULL == method) {
			method = (J9Method*)javaLookupMethod(currentThread, exceptionClass, (J9ROMNameAndSignature*)&throwableNameAndSig, NULL, lookupFlags);
			detailMessage = NULL;
			constructorIndex = J9_EX_CTOR_STRING;
		}
		/* If a constructor was found, push the appropriate parameters and run it */
		if (NULL != method) {
			*--currentThread->sp = (UDATA)exception;
			switch(constructorIndex) {
			case J9_EX_CTOR_CLASS_CLASS: {
				J9ClassCastParms *parms = (J9ClassCastParms*)detailMessage;
				*--currentThread->sp = (UDATA)J9VM_J9CLASS_TO_HEAPCLASS(parms->instanceClass);
				*--currentThread->sp = (UDATA)J9VM_J9CLASS_TO_HEAPCLASS(parms->castClass);
				break;
			}
#if defined(J9VM_ENV_DATA64)
			case J9_EX_CTOR_INT:
				*(I_32*)--currentThread->sp = (I_32)(UDATA)detailMessage;
				break;
#endif /* J9VM_ENV_DATA64 */
			default:
				*--currentThread->sp = (UDATA)detailMessage;
				break;
			}
			currentThread->returnValue = J9_BCLOOP_RUN_METHOD;
			currentThread->returnValue2 = (UDATA)method;
			c_cInterpreter(currentThread);
		}
		restoreCallInFrame(currentThread);
	}
	Trc_VM_internalSendExceptionConstructor_Exit(currentThread);
}

void JNICALL
printStackTrace(J9VMThread *currentThread, j9object_t exception, UDATA reserved1, UDATA reserved2, UDATA reserved3)
{
	Trc_VM_printStackTrace_Entry(currentThread);
	J9VMEntryLocalStorage newELS;
	if (buildCallInStackFrame(currentThread, &newELS, false, false)) {
		/* Look up the printStackTrace method on the exception instance */
		J9Class *exceptionClass = J9OBJECT_CLAZZ(currentThread, exception);
		J9Method *method = (J9Method*)javaLookupMethod(currentThread, exceptionClass, (J9ROMNameAndSignature*)&printStackTraceNameAndSig, NULL, J9_LOOK_VIRTUAL | J9_LOOK_DIRECT_NAS);
		/* If the method was found, run it (exception is already set if the method cannot be found) */
		if (NULL != method) {
			*--currentThread->sp = (UDATA)exception;
			currentThread->returnValue = J9_BCLOOP_RUN_METHOD;
			currentThread->returnValue2 = (UDATA)method;
			c_cInterpreter(currentThread);
		}
		restoreCallInFrame(currentThread);
	}
	Trc_VM_printStackTrace_Exit(currentThread);
}

void JNICALL
runJavaThread(J9VMThread *currentThread, UDATA reserved1, UDATA reserved2, UDATA reserved3, UDATA reserved4)
{
	Trc_VM_runJavaThread_Entry(currentThread);
	J9VMEntryLocalStorage newELS;
	if (buildCallInStackFrame(currentThread, &newELS, false, false)) {
		/* Lookup the run()V method on the Thread instance */
		j9object_t threadObject = currentThread->threadObject;
		J9Class *clazz = J9OBJECT_CLAZZ(currentThread, threadObject);
		J9Method *method = (J9Method*)javaLookupMethod(currentThread, clazz, (J9ROMNameAndSignature*)&threadRunNameAndSig, NULL, J9_LOOK_VIRTUAL | J9_LOOK_DIRECT_NAS);
		/* If the method was found, run it (exception is already set if the method cannot be found) */
		if (NULL != method) {
			*--currentThread->sp = (UDATA)threadObject;
			currentThread->returnValue = J9_BCLOOP_RUN_METHOD;
			currentThread->returnValue2 = (UDATA)method;
			c_cInterpreter(currentThread);
		}
		restoreCallInFrame(currentThread);
	}
	Trc_VM_runJavaThread_Exit(currentThread);
}

void JNICALL
runStaticMethod(J9VMThread *currentThread, U_8* className, J9NameAndSignature* selector, UDATA argCount, UDATA* arguments)
{
	/* Assumes that the called method returns void and that className is a canonical UTF.
	 * Also, the arguments must be in stack shape (ints in the low-memory half of the stack slot on 64-bit)
	 * and contain no object pointers, as there are GC points in here before the arguments are copied to
	 * the stack.
	 */
	Trc_VM_runStaticMethod_Entry(currentThread);
	J9VMEntryLocalStorage newELS;
	if (buildCallInStackFrame(currentThread, &newELS, false, true)) {
		J9Class *foundClass = internalFindClassUTF8(currentThread, className, strlen((char*)className), currentThread->javaVM->systemClassLoader, J9_FINDCLASS_FLAG_THROW_ON_FAIL);
		if (NULL != foundClass) {
			/* Initialize the class */
			initializeClass(currentThread, foundClass);
			if (!VM_VMHelpers::exceptionPending(currentThread)) {
				/* Look up the method */
				J9Method *method = (J9Method*)javaLookupMethod(currentThread, foundClass, (J9ROMNameAndSignature*)selector, NULL, J9_LOOK_STATIC | J9_LOOK_DIRECT_NAS);
				/* If the method was found, run it (exception is already set if the method cannot be found) */
				if (NULL != method) {
					for (UDATA i = 0; i < argCount; ++i) {
						*--currentThread->sp = arguments[i];
					}
					currentThread->returnValue = J9_BCLOOP_RUN_METHOD;
					currentThread->returnValue2 = (UDATA)method;
					c_cInterpreter(currentThread);
				}
			}
		}
		restoreCallInFrame(currentThread);
	}
	Trc_VM_runStaticMethod_Exit(currentThread);
}

void JNICALL
internalRunStaticMethod(J9VMThread *currentThread, J9Method *method, BOOLEAN returnsObject, UDATA argCount, UDATA* arguments)
{
	Trc_VM_internalRunStaticMethod_Entry(currentThread);
	J9VMEntryLocalStorage newELS;

	if (buildCallInStackFrame(currentThread, &newELS, returnsObject != 0, false)) {
		for (UDATA i = 0; i < argCount; ++i) {
			*--currentThread->sp = arguments[i];
		}
		currentThread->returnValue = J9_BCLOOP_RUN_METHOD;
		currentThread->returnValue2 = (UDATA)method;
		c_cInterpreter(currentThread);

		restoreCallInFrame(currentThread);
	}
	Trc_VM_internalRunStaticMethod_Exit(currentThread);
}

void JNICALL
sendCheckPackageAccess(J9VMThread *currentThread, J9Class *clazz, j9object_t protectionDomain, UDATA reserved3, UDATA reserved4)
{
	Trc_VM_sendCheckPackageAccess_Entry(currentThread);
	J9VMEntryLocalStorage newELS;
	if (buildCallInStackFrame(currentThread, &newELS, false, false)) {
		/* Run the method */
		*--currentThread->sp = (UDATA)J9VM_J9CLASS_TO_HEAPCLASS(clazz);
		*--currentThread->sp = (UDATA)protectionDomain;
		currentThread->returnValue = J9_BCLOOP_RUN_METHOD;
		currentThread->returnValue2 = (UDATA)J9VMJAVALANGJ9VMINTERNALS_CHECKPACKAGEACCESS_METHOD(currentThread->javaVM);
		c_cInterpreter(currentThread);
		restoreCallInFrame(currentThread);
	}
	Trc_VM_sendCheckPackageAccess_Exit(currentThread);
}

void JNICALL
sendCompleteInitialization(J9VMThread *currentThread, UDATA reserved1, UDATA reserved2, UDATA reserved3, UDATA reserved4)
{
	Trc_VM_sendCompleteInitialization_Entry(currentThread);
	J9VMEntryLocalStorage newELS;
	if (buildCallInStackFrame(currentThread, &newELS, false, true)) {
		/* Run the method */
		currentThread->returnValue = J9_BCLOOP_RUN_METHOD;
		currentThread->returnValue2 = (UDATA)J9VMJAVALANGJ9VMINTERNALS_COMPLETEINITIALIZATION_METHOD(currentThread->javaVM);
		c_cInterpreter(currentThread);
		restoreCallInFrame(currentThread);
	}
	Trc_VM_sendCompleteInitialization_Exit(currentThread);
}

static bool
isAccessibleToAllModulesViaReflection(J9VMThread *currentThread, J9Class *clazz, bool javaBaseLoaded) {
	J9JavaVM *vm = currentThread->javaVM;
	bool isAccessible = true;
	J9Module *module = clazz->module;

	if (!J9_IS_J9MODULE_UNNAMED(vm, module) && !J9_IS_J9MODULE_OPEN(module)) {
		if (javaBaseLoaded) {
			const U_8* packageName = NULL;
			UDATA packageNameLength = 0;
			J9PackageIDTableEntry entry = {0};
			J9Package *package = NULL;
			UDATA err = 0;

			entry.taggedROMClass = clazz->packageID;
			packageName = getPackageName(&entry, &packageNameLength);

			omrthread_monitor_enter(vm->classLoaderModuleAndLocationMutex);
			package = getPackageDefinitionWithName(currentThread, module, (U_8*) packageName, (U_16) packageNameLength, &err);
			omrthread_monitor_exit(vm->classLoaderModuleAndLocationMutex);

			if ((ERRCODE_SUCCESS != err) || !package->exportToAll) {
				isAccessible = false;
			}
		} else {
			isAccessible = false;
		}

	}

	return isAccessible;
}

void JNICALL
sendInit(J9VMThread *currentThread, j9object_t object, J9Class *senderClass, UDATA lookupOptions, UDATA reserved4)
{
	Trc_VM_sendInit_Entry(currentThread);
	J9VMEntryLocalStorage newELS;
	if (buildCallInStackFrame(currentThread, &newELS, false, false)) {
		J9Class *clazz = J9OBJECT_CLAZZ(currentThread, object);
		J9Method *method = clazz->initializerCache;
		if (NULL == method) {
			/* Lookup the <init> method for the object */
			method = (J9Method*)javaLookupMethod(currentThread, clazz, (J9ROMNameAndSignature*)&initNameAndSig, senderClass, J9_LOOK_VIRTUAL | J9_LOOK_DIRECT_NAS | lookupOptions);
			if ((NULL != method) && (J9_ARE_NO_BITS_SET(clazz->classFlags, J9ClassDoNotAttemptToSetInitCache))) {
				J9JavaVM *vm = currentThread->javaVM;
				bool javaBaseLoaded = J9_ARE_ALL_BITS_SET(vm->runtimeFlags, J9_RUNTIME_JAVA_BASE_MODULE_CREATED);
				if (J9_ARE_ANY_BITS_SET(clazz->romClass->modifiers, J9AccPublic)) {
					if (J9_ARE_ANY_BITS_SET(J9_ROM_METHOD_FROM_RAM_METHOD(method)->modifiers, J9AccPublic)) {
						if (!J9_ARE_MODULES_ENABLED(vm) || isAccessibleToAllModulesViaReflection(currentThread, clazz, javaBaseLoaded)) {
							clazz->initializerCache = method;
						} else if (javaBaseLoaded) {
							/* remember that this class is not accessible to all. We can only set this
							 * after the module system is ready as there is no way of knowing before hand.*/
							clazz->classFlags |= J9ClassDoNotAttemptToSetInitCache;
						}
					}
				}
			}
		}
		if (NULL != method) {
			/* Run the method */
			*--currentThread->sp = (UDATA)object;
			currentThread->returnValue = J9_BCLOOP_RUN_METHOD;
			currentThread->returnValue2 = (UDATA)method;
			c_cInterpreter(currentThread);
		}
		restoreCallInFrame(currentThread);
	}
	Trc_VM_sendInit_Exit(currentThread);
}

void JNICALL
sendInitCause(J9VMThread *currentThread, j9object_t receiver, j9object_t cause, UDATA reserved3, UDATA reserved4)
{
	Trc_VM_sendInitCause_Entry(currentThread);
	J9VMEntryLocalStorage newELS;
	if (buildCallInStackFrame(currentThread, &newELS, true, false)) {
		/* Look up initCause */
		J9Method *method = (J9Method*)javaLookupMethod(currentThread, J9OBJECT_CLAZZ(currentThread, receiver), (J9ROMNameAndSignature*)&initCauseNameAndSig, NULL, J9_LOOK_VIRTUAL | J9_LOOK_DIRECT_NAS | J9_LOOK_NO_THROW);
		*--currentThread->sp = (UDATA)receiver;
		/* If the method is not found, leave the receiver on stack as the return value (initCause returns this) */
		if (NULL != method) {
			/* Run the method */
			*--currentThread->sp = (UDATA)cause;
			currentThread->returnValue = J9_BCLOOP_RUN_METHOD;
			currentThread->returnValue2 = (UDATA)method;
			c_cInterpreter(currentThread);
		}
		restoreCallInFrame(currentThread);
	}
	Trc_VM_sendInitCause_Exit(currentThread);
}

void JNICALL
sendInitializationAlreadyFailed(J9VMThread *currentThread, J9Class *clazz, UDATA reserved2, UDATA reserved3, UDATA reserved4)
{
	Trc_VM_sendInitializationAlreadyFailed_Entry(currentThread);
	J9VMEntryLocalStorage newELS;
	if (buildCallInStackFrame(currentThread, &newELS, true, false)) {
		/* Run the method */
		*--currentThread->sp = (UDATA)J9VM_J9CLASS_TO_HEAPCLASS(clazz);
		currentThread->returnValue = J9_BCLOOP_RUN_METHOD;
		currentThread->returnValue2 = (UDATA)J9VMJAVALANGJ9VMINTERNALS_INITIALIZATIONALREADYFAILED_METHOD(currentThread->javaVM);
		c_cInterpreter(currentThread);
		restoreCallInFrame(currentThread);
	}
	Trc_VM_sendInitializationAlreadyFailed_Exit(currentThread);
}

void JNICALL
sendRecordInitializationFailure(J9VMThread *currentThread, J9Class *clazz, j9object_t throwable, UDATA reserved3, UDATA reserved4)
{
	Trc_VM_sendRecordInitializationFailure_Entry(currentThread);
	J9VMEntryLocalStorage newELS;
	if (buildCallInStackFrame(currentThread, &newELS, true, false)) {
		/* Run the method */
		*--currentThread->sp = (UDATA)J9VM_J9CLASS_TO_HEAPCLASS(clazz);
		*--currentThread->sp = (UDATA)throwable;
		currentThread->returnValue = J9_BCLOOP_RUN_METHOD;
		currentThread->returnValue2 = (UDATA)J9VMJAVALANGJ9VMINTERNALS_RECORDINITIALIZATIONFAILURE_METHOD(currentThread->javaVM);
		c_cInterpreter(currentThread);
		restoreCallInFrame(currentThread);
	}
	Trc_VM_sendRecordInitializationFailure_Exit(currentThread);
}

void JNICALL
sendFromMethodDescriptorString(J9VMThread *currentThread, J9UTF8 *descriptor, J9ClassLoader *classLoader, J9Class *appendArgType, UDATA reserved4)
{
	Trc_VM_sendFromMethodDescriptorString_Entry(currentThread);
	J9VMEntryLocalStorage newELS;
	if (buildCallInStackFrame(currentThread, &newELS, true, false)) {
		/* Convert descriptor to a String */
		J9JavaVM *vm = currentThread->javaVM;
		J9MemoryManagerFunctions const * const mmFuncs = vm->memoryManagerFunctions;
		j9object_t descriptorString = mmFuncs->j9gc_createJavaLangString(currentThread, J9UTF8_DATA(descriptor), J9UTF8_LENGTH(descriptor), 0);
		if (NULL != descriptorString) {
			/* Run the method */
			*--currentThread->sp = (UDATA)descriptorString;
			*--currentThread->sp = (UDATA)classLoader->classLoaderObject;
			currentThread->returnValue = J9_BCLOOP_RUN_METHOD;
			if (NULL == appendArgType) {
				currentThread->returnValue2 = (UDATA)J9VMJAVALANGINVOKEMETHODTYPE_FROMMETHODDESCRIPTORSTRING_METHOD(vm);
			} else {
				/* If an appendArgType has been provided, push it on the stack and call a different MethodType factory method. */
				*--currentThread->sp = (UDATA)J9VM_J9CLASS_TO_HEAPCLASS(appendArgType);
				currentThread->returnValue2 = (UDATA)J9VMJAVALANGINVOKEMETHODTYPE_FROMMETHODDESCRIPTORSTRINGAPPENDARG_METHOD(vm);
			}
			c_cInterpreter(currentThread);
		}
		restoreCallInFrame(currentThread);
	}
	Trc_VM_sendFromMethodDescriptorString_Exit(currentThread);
}

void JNICALL
sendResolveMethodHandle(J9VMThread *currentThread, UDATA cpIndex, J9ConstantPool *ramCP, J9Class *definingClass, J9ROMNameAndSignature* nameAndSig)
{
	Trc_VM_sendResolveMethodHandle_Entry(currentThread);
	J9VMEntryLocalStorage newELS;
	if (buildCallInStackFrame(currentThread, &newELS, true, false)) {
		/* Convert name and signature to String objects */
		J9JavaVM *vm = currentThread->javaVM;
		J9MemoryManagerFunctions const * const mmFuncs = vm->memoryManagerFunctions;
		J9UTF8 *nameUTF = J9ROMNAMEANDSIGNATURE_NAME(nameAndSig);
		j9object_t nameString = mmFuncs->j9gc_createJavaLangString(currentThread, J9UTF8_DATA(nameUTF), J9UTF8_LENGTH(nameUTF), 0);
		if (NULL != nameString) {
			J9UTF8 *sigUTF = J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig);
			PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, nameString);
			j9object_t sigString = mmFuncs->j9gc_createJavaLangString(currentThread, J9UTF8_DATA(sigUTF), J9UTF8_LENGTH(sigUTF), 0);
			nameString = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);
			if (NULL != sigString) {
				/* Run the method */
				J9ROMMethodHandleRef *romCPEntry = (J9ROMMethodHandleRef*)(ramCP->romConstantPool + cpIndex);
				J9Class *clazz = ramCP->ramClass;
				*(I_32*)--currentThread->sp = (I_32)(romCPEntry->handleTypeAndCpType >> J9DescriptionCpTypeShift);
				*--currentThread->sp = (UDATA)J9VM_J9CLASS_TO_HEAPCLASS(clazz);
				*--currentThread->sp = (UDATA)J9VM_J9CLASS_TO_HEAPCLASS(definingClass);
				*--currentThread->sp = (UDATA)nameString;
				*--currentThread->sp = (UDATA)sigString;
				*--currentThread->sp = (UDATA)clazz->classLoader->classLoaderObject;
				currentThread->returnValue = J9_BCLOOP_RUN_METHOD;
				currentThread->returnValue2 = (UDATA)J9VMJAVALANGINVOKEMETHODHANDLE_SENDRESOLVEMETHODHANDLE_METHOD(vm);
				c_cInterpreter(currentThread);
			}
		}
		restoreCallInFrame(currentThread);
	}
	Trc_VM_sendResolveMethodHandle_Exit(currentThread);
}

void JNICALL
sendForGenericInvoke(J9VMThread *currentThread, j9object_t methodHandle, j9object_t methodType, UDATA dropFirstArg, UDATA reserved4)
{
	Trc_VM_sendForGenericInvoke_Entry(currentThread);
	J9VMEntryLocalStorage newELS;
	if (buildCallInStackFrame(currentThread, &newELS, true, false)) {
		/* Run the method */
		*--currentThread->sp = (UDATA)methodHandle;
		*--currentThread->sp = (UDATA)methodType;
		*(I_32*)--currentThread->sp = (I_32)dropFirstArg;
		currentThread->returnValue = J9_BCLOOP_RUN_METHOD;
		currentThread->returnValue2 = (UDATA)J9VMJAVALANGINVOKEMETHODHANDLE_FORGENERICINVOKE_METHOD(currentThread->javaVM);
		c_cInterpreter(currentThread);
		restoreCallInFrame(currentThread);
	}
	Trc_VM_sendForGenericInvoke_Exit(currentThread);
}

void JNICALL
sendForGenericInvokeVarHandle(J9VMThread *currentThread, j9object_t methodHandle, j9object_t methodType, j9object_t varHandle, UDATA reserved4)
{
	Trc_VM_sendForGenericInvokeVarHandle_Entry(currentThread);
	J9VMEntryLocalStorage newELS;
	
	/* Run the method */
	if (buildCallInStackFrame(currentThread, &newELS, true, false)) {
		*--currentThread->sp = (UDATA)methodHandle;
		*--currentThread->sp = (UDATA)methodType;
		*--currentThread->sp = (UDATA)varHandle;
		currentThread->returnValue = J9_BCLOOP_RUN_METHOD;
		currentThread->returnValue2 = (UDATA)J9VMJAVALANGINVOKEMETHODHANDLE_FORGENERICINVOKEVARHANDLE_METHOD(currentThread->javaVM);
		c_cInterpreter(currentThread);
		restoreCallInFrame(currentThread);
	}
	Trc_VM_sendForGenericInvokeVarHandle_Exit(currentThread);
}

void JNICALL
sendResolveInvokeDynamic(J9VMThread *currentThread, J9ConstantPool *ramCP, UDATA callSiteIndex, J9ROMNameAndSignature *nameAndSig, U_16 *bsmData)
{
	Trc_VM_sendResolveInvokeDynamic_Entry(currentThread);
	J9VMEntryLocalStorage newELS;
	if (buildCallInStackFrame(currentThread, &newELS, true, false)) {
		/* Convert name and signature to String objects */
		J9JavaVM *vm = currentThread->javaVM;
		J9MemoryManagerFunctions const * const mmFuncs = vm->memoryManagerFunctions;
		J9UTF8 *nameUTF = J9ROMNAMEANDSIGNATURE_NAME(nameAndSig);
		j9object_t nameString = mmFuncs->j9gc_createJavaLangString(currentThread, J9UTF8_DATA(nameUTF), J9UTF8_LENGTH(nameUTF), 0);
		if (NULL != nameString) {
			J9UTF8 *sigUTF = J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig);
			PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, nameString);
			j9object_t sigString = mmFuncs->j9gc_createJavaLangString(currentThread, J9UTF8_DATA(sigUTF), J9UTF8_LENGTH(sigUTF), 0);
			nameString = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);
			if (NULL != sigString) {
				/* Run the method */
				/* skip one slot because we are passing a long */
				currentThread->sp -= 2;
				/*
				 * Need to pass the ramClass so that we can get the
				 * correct ramConstantPool. If we pass the classObject
				 * we will always get the latest ramClass, which is not always
				 * the correct one. In cases where we can have an
				 * old method (caused by class redefinition) on the stack,
				 * we will need to search the old ramClass to get the correct
				 * constanPool. It is difficult to do this if we pass the
				 * classObject.
				 */
				*(U_64*)currentThread->sp = (U_64)ramCP->ramClass;
				*--currentThread->sp = (UDATA)nameString;
				*--currentThread->sp = (UDATA)sigString;
				currentThread->sp -= 2;
				*(U_64*)currentThread->sp = (U_64)(UDATA)bsmData;
				currentThread->returnValue = J9_BCLOOP_RUN_METHOD;
				currentThread->returnValue2 = (UDATA)J9VMJAVALANGINVOKEMETHODHANDLE_RESOLVEINVOKEDYNAMIC_METHOD(vm);
				c_cInterpreter(currentThread);
			}
		}
		restoreCallInFrame(currentThread);
	}
	Trc_VM_sendResolveInvokeDynamic_Exit(currentThread);
}

void JNICALL
runCallInMethod(JNIEnv *env, jobject receiver, jclass clazz, jmethodID methodID, void* args)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	Trc_VM_runCallInMethod_Entry(currentThread);
	J9VMEntryLocalStorage newELS;
	if (buildCallInStackFrame(currentThread, &newELS, false, true)) {
		if (NULL != methodID) {
			J9SFJNICallInFrame *frame = (J9SFJNICallInFrame*)currentThread->sp;
			J9JNIMethodID *mid = (J9JNIMethodID*)methodID;
			J9Method *method = mid->method;
			J9Class *j9clazz = NULL;
			j9object_t receiverObject = NULL;
			if (NULL != receiver) {
				receiverObject = J9_JNI_UNWRAP_REFERENCE(receiver);
			}
			VM_VMHelpers::clearException(currentThread);
			if (NULL == clazz) {
				/* virtual send -- lookup real method */
				j9clazz = J9OBJECT_CLAZZ(currentThread, receiverObject);
				method = mapVirtualMethod(currentThread, method, mid->vTableIndex, j9clazz);
			} else {
				j9clazz = J9VM_J9CLASS_FROM_JCLASS(currentThread, clazz);
			}
			if (NULL != receiverObject) {
				*--currentThread->sp = (UDATA)receiverObject;
				// TODO: j9clazz is never NULL, should check clazz
				if (NULL != j9clazz) {
					/* nonvirtual send -- could be a constructor, so invoke the recently-created object barrier */
					currentThread->javaVM->memoryManagerFunctions->j9gc_objaccess_recentlyAllocatedObject(currentThread, receiverObject);
				}
			}
			frame->specialFrameFlags |= pushArguments(currentThread, method, args);
			/* Run the method */
			currentThread->returnValue = J9_BCLOOP_RUN_METHOD;
			currentThread->returnValue2 = (UDATA)method;
			c_cInterpreter(currentThread);
		}
		restoreCallInFrame(currentThread);
	}
	Trc_VM_runCallInMethod_Exit(currentThread);
}

void JNICALL
sidecarInvokeReflectMethodImpl(J9VMThread *currentThread, jobject methodRef, jobject recevierRef, jobjectArray argsRef, void *unused)
{
	Trc_VM_sidecarInvokeReflectMethod_Entry(currentThread);
	J9VMEntryLocalStorage newELS;
	if (buildCallInStackFrame(currentThread, &newELS, true, true)) {
		j9object_t methodObject = J9_JNI_UNWRAP_REFERENCE(methodRef);
		J9Class *returnType = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, J9VMJAVALANGREFLECTMETHOD_RETURNTYPE(currentThread, methodObject));
		J9JNIMethodID *methodID = currentThread->javaVM->reflectFunctions.idFromMethodObject(currentThread, methodObject);
		J9Method *method = methodID->method;
		J9Class *declaringClass = J9_CLASS_FROM_METHOD(method);
		if (J9_ARE_ANY_BITS_SET(J9_ROM_METHOD_FROM_RAM_METHOD(method)->modifiers, J9AccStatic)) {
			if (VM_VMHelpers::classRequiresInitialization(currentThread, declaringClass)) {
				initializeClass(currentThread, declaringClass);
				if (VM_VMHelpers::exceptionPending(currentThread)) {
					goto done;
				}
				methodObject = J9_JNI_UNWRAP_REFERENCE(methodRef);
			}
		} else {
			j9object_t receiverObject = NULL;
			if (NULL != recevierRef) {
				receiverObject = J9_JNI_UNWRAP_REFERENCE(recevierRef);
			}
			if (NULL == receiverObject) {
				setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
				goto done;
			}
			{
				J9Class *receiverClass = J9OBJECT_CLAZZ(currentThread, receiverObject);
				if (!VM_VMHelpers::inlineCheckCast(receiverClass, declaringClass)) {
					setCurrentExceptionNLS(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, J9NLS_VM_INVALID_RECEIVER_CLASS);
					goto done;
				}
				method = mapVirtualMethod(currentThread, method, methodID->vTableIndex, receiverClass);
				*--currentThread->sp = (UDATA)receiverObject;
			}
		}
		{
			j9object_t argsObject = NULL;
			if (NULL != argsRef) {
				argsObject = J9_JNI_UNWRAP_REFERENCE(argsRef);
			}
			UDATA pushRC = pushReflectArguments(currentThread, J9VMJAVALANGREFLECTMETHOD_PARAMETERTYPES(currentThread, methodObject), argsObject);
			switch(pushRC) {
			case 1:
				dropPendingSendPushes(currentThread);
				setCurrentExceptionNLS(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, J9NLS_VM_WRONG_NUMBER_OF_ARGUMENTS);
				goto done;
			case 2:
				dropPendingSendPushes(currentThread);
				setCurrentExceptionNLS(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, J9NLS_VM_INVALID_ARGUMENT_CLASS);
				goto done;
			}
		}
		{
			/* Run the method */
			currentThread->returnValue = J9_BCLOOP_RUN_METHOD;
			currentThread->returnValue2 = (UDATA)method;
			c_cInterpreter(currentThread);
			j9object_t exception = currentThread->currentException;
			if (NULL == exception) {
				if (J9ROMCLASS_IS_PRIMITIVE_TYPE(returnType->romClass)) {
					J9JavaVM *vm = currentThread->javaVM;
					if (returnType != vm->voidReflectClass) {
						UDATA returnValue[] = { currentThread->sp[0], currentThread->sp[1] };
						UDATA classIndex = 0;
						UDATA fieldIndex = 0;
						bool returnsDouble = false;
						if (returnType == vm->booleanReflectClass) {
							classIndex = J9VMCONSTANTPOOL_JAVALANGBOOLEAN;
							fieldIndex = J9VMCONSTANTPOOL_JAVALANGBOOLEAN_VALUE;
						} else if (returnType == vm->charReflectClass) {
							classIndex = J9VMCONSTANTPOOL_JAVALANGCHARACTER;
							fieldIndex = J9VMCONSTANTPOOL_JAVALANGCHARACTER_VALUE;
						} else if (returnType == vm->floatReflectClass) {
							classIndex = J9VMCONSTANTPOOL_JAVALANGFLOAT;
							fieldIndex = J9VMCONSTANTPOOL_JAVALANGFLOAT_VALUE;
						} else if (returnType == vm->byteReflectClass) {
							classIndex = J9VMCONSTANTPOOL_JAVALANGBYTE;
							fieldIndex = J9VMCONSTANTPOOL_JAVALANGBYTE_VALUE;
						} else if (returnType == vm->shortReflectClass) {
							classIndex = J9VMCONSTANTPOOL_JAVALANGSHORT;
							fieldIndex = J9VMCONSTANTPOOL_JAVALANGSHORT_VALUE;
						} else if (returnType == vm->intReflectClass) {
							classIndex = J9VMCONSTANTPOOL_JAVALANGINTEGER;
							fieldIndex = J9VMCONSTANTPOOL_JAVALANGINTEGER_VALUE;
						} else if (returnType == vm->doubleReflectClass) {
							classIndex = J9VMCONSTANTPOOL_JAVALANGDOUBLE;
							fieldIndex = J9VMCONSTANTPOOL_JAVALANGDOUBLE_VALUE;
							returnsDouble = true;
						} else { // must be long
							classIndex = J9VMCONSTANTPOOL_JAVALANGLONG;
							fieldIndex = J9VMCONSTANTPOOL_JAVALANGLONG_VALUE;
							returnsDouble = true;
						}
						currentThread->sp = (UDATA*)(((J9SFJNICallInFrame*)(currentThread->arg0EA + 1)) - 1);
						J9Class *wrapperClass = J9VMCONSTANTPOOL_CLASSREF_AT(vm, classIndex)->value;
						UDATA fieldOffset = J9VMCONSTANTPOOL_FIELD_OFFSET(vm, fieldIndex);
						j9object_t wrapperObject = vm->memoryManagerFunctions->J9AllocateObject(currentThread, wrapperClass, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
						if (NULL == wrapperObject) {
							/* stack is already clean */
							setHeapOutOfMemoryError(currentThread);
							goto done;
						}
						if (returnsDouble) {
							J9OBJECT_U64_STORE_VM(vm, wrapperObject, fieldOffset, *(U_64*)returnValue);
						} else {
							J9OBJECT_U32_STORE_VM(vm, wrapperObject, fieldOffset, *(U_32*)returnValue);
						}
						*--currentThread->sp = (UDATA)wrapperObject;
					}
				}
			} else {
				VM_VMHelpers::clearException(currentThread);
				/* stack is already clean */
				setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGREFLECTINVOCATIONTARGETEXCEPTION + J9_EX_CTOR_THROWABLE, (UDATA*)exception);
			}
		}
done:
		restoreCallInFrame(currentThread);
	}
	Trc_VM_sidecarInvokeReflectMethod_Exit(currentThread);
}

void JNICALL
sidecarInvokeReflectConstructorImpl(J9VMThread *currentThread, jobject constructorRef, jobject recevierRef, jobjectArray argsRef, void *unused)
{
	Trc_VM_sidecarInvokeReflectConstructor_Entry(currentThread);
	J9VMEntryLocalStorage newELS;
	if (buildCallInStackFrame(currentThread, &newELS, false, true)) {
		j9object_t constructorObject = J9_JNI_UNWRAP_REFERENCE(constructorRef);
		j9object_t receiverObject = NULL;
		if (NULL != recevierRef) {
			receiverObject = J9_JNI_UNWRAP_REFERENCE(recevierRef);
		}
		if (NULL == receiverObject) {
			setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
			goto done;
		}
		*--currentThread->sp = (UDATA)receiverObject;
		{
			j9object_t argsObject = NULL;
			if (NULL != argsRef) {
				argsObject = J9_JNI_UNWRAP_REFERENCE(argsRef);
			}
			UDATA pushRC = pushReflectArguments(currentThread, J9VMJAVALANGREFLECTCONSTRUCTOR_PARAMETERTYPES(currentThread, constructorObject), argsObject);
			switch(pushRC) {
			case 1:
				dropPendingSendPushes(currentThread);
				setCurrentExceptionNLS(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, J9NLS_VM_WRONG_NUMBER_OF_ARGUMENTS);
				goto done;
			case 2:
				dropPendingSendPushes(currentThread);
				setCurrentExceptionNLS(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, J9NLS_VM_INVALID_ARGUMENT_CLASS);
				goto done;
			}
		}
		{
			J9JNIMethodID *methodID = currentThread->javaVM->reflectFunctions.idFromConstructorObject(currentThread, constructorObject);
			/* Run the method */
			currentThread->returnValue = J9_BCLOOP_RUN_METHOD;
			currentThread->returnValue2 = (UDATA)methodID->method;
			c_cInterpreter(currentThread);
			j9object_t exception = currentThread->currentException;
			if (NULL != exception) {
				VM_VMHelpers::clearException(currentThread);
				/* stack is already clean */
				setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGREFLECTINVOCATIONTARGETEXCEPTION + J9_EX_CTOR_THROWABLE, (UDATA*)exception);
			}
		}
done:
		restoreCallInFrame(currentThread);
	}
	Trc_VM_sidecarInvokeReflectConstructor_Exit(currentThread);
}

void JNICALL
jitFillOSRBuffer(struct J9VMThread *currentThread, void *osrBlock, UDATA reserved1, UDATA reserved2, UDATA reserved3)
{
	Trc_VM_jitFillOSRBuffer_Entry(currentThread);
	J9VMEntryLocalStorage newELS;
	if (buildCallInStackFrame(currentThread, &newELS, false, false)) {
		currentThread->returnValue = J9_BCLOOP_FILL_OSR_BUFFER;
		currentThread->returnValue2 = (UDATA)osrBlock;
		c_cInterpreter(currentThread);
		restoreCallInFrame(currentThread);
	}
	Trc_VM_jitFillOSRBuffer_Exit(currentThread);
}

void JNICALL
initializeAttachedThread(J9VMThread *currentThread, const char *name, j9object_t *group, UDATA daemon, J9VMThread *initializee)
{
	VM_VMAccess::inlineEnterVMFromJNI(currentThread);
	initializeAttachedThreadImpl(currentThread, name, group, daemon, initializee);
	VM_VMAccess::inlineReleaseVMAccess(currentThread);
}

void JNICALL
sidecarInvokeReflectMethod(J9VMThread *currentThread, jobject methodRef, jobject recevierRef, jobjectArray argsRef, void *unused)
{
	VM_VMAccess::inlineEnterVMFromJNI(currentThread);
	sidecarInvokeReflectMethodImpl(currentThread, methodRef, recevierRef, argsRef, unused);
	VM_VMAccess::inlineExitVMToJNI(currentThread);
}

void JNICALL
sidecarInvokeReflectConstructor(J9VMThread *currentThread, jobject constructorRef, jobject recevierRef, jobjectArray argsRef, void *unused)
{
	VM_VMAccess::inlineEnterVMFromJNI(currentThread);
	sidecarInvokeReflectConstructorImpl(currentThread, constructorRef, recevierRef, argsRef, unused);
	VM_VMAccess::inlineExitVMToJNI(currentThread);
}

} /* extern "C" */
