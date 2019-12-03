/*******************************************************************************
 * Copyright (c) 1991, 2020 IBM Corp. and others
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
#include "objhelp.h"
#include "j9bcvnls.h"
#include "j9vmnls.h"
#include "j9consts.h"
#include "ut_j9vm.h"
#include "vm_internal.h"
#include "j9accessbarrier.h"
#include "omrthread.h"
#include "vmhook_internal.h"
#include "VMHelpers.hpp"
#include "AtomicSupport.hpp"
#include "ObjectMonitor.hpp"
#include "cfreader.h"

extern "C" {

/* Do not change the order/values of these constants */
typedef enum {
	J9_CLASS_INIT_VERIFIED = 0,
	J9_CLASS_INIT_PREPARED,
	J9_CLASS_INIT_INITIALIZED,
} J9ClassInitState;

static char const *desiredStateNames[] = {
		"VERIFIED",
		"PREPARED",
		"INITIALIZED",
};

static char const *statusNames[] = {
		"UNINITIALIZED",		/* J9ClassInitNotInitialized */
		"INITIALIZED",			/* J9ClassInitSucceeded */
		"FAILED",				/* J9ClassInitFailed */
		"UNVERIFIED",			/* J9ClassInitUnverified */
		"UNPREPARED",			/* J9ClassInitUnprepared */
};

#define STATE_NAME(state) (J9_ARE_ANY_BITS_SET(state, ~(UDATA)J9ClassInitStatusMask) ? "IN_PROGRESS" : statusNames[state])

static j9object_t setInitStatus(J9VMThread *currentThread, J9Class *clazz, UDATA status, j9object_t initializationLock);
static void classInitStateMachine(J9VMThread *currentThread, J9Class *clazz, J9ClassInitState desiredState);

void
initializeImpl(J9VMThread *currentThread, J9Class *clazz)
{
	Trc_VM_initializeImpl_Entry(
			currentThread,
			J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(clazz->romClass)),
			J9UTF8_DATA(J9ROMCLASS_CLASSNAME(clazz->romClass)),
			clazz
	);
	J9JavaVM *vm = currentThread->javaVM;
	UDATA failed = FALSE;
	Trc_VM_initializeImpl_preInit(currentThread);
	TRIGGER_J9HOOK_VM_CLASS_PREINITIALIZE(vm->hookInterface, currentThread, clazz, failed);
	clazz = VM_VMHelpers::currentClass(clazz);
	if (failed) {
		Trc_VM_initializeImpl_preInitFailedRetry(currentThread);
		vm->memoryManagerFunctions->j9gc_modron_global_collect_with_overrides(currentThread, J9MMCONSTANT_EXPLICIT_GC_NATIVE_OUT_OF_MEMORY);
		clazz = VM_VMHelpers::currentClass(clazz);
		TRIGGER_J9HOOK_VM_CLASS_PREINITIALIZE(vm->hookInterface, currentThread, clazz, failed);
		clazz = VM_VMHelpers::currentClass(clazz);
		if (failed) {
			Trc_VM_initializeImpl_preInitFailed(currentThread);
			setNativeOutOfMemoryError(currentThread, 0, 0);
			goto done;
		}
	}

	if (J9ROMCLASS_HAS_CLINIT(clazz->romClass)) {
		sendClinit(currentThread, clazz);
		clazz = VM_VMHelpers::currentClass(clazz);
		if (VM_VMHelpers::exceptionPending(currentThread)) {
			TRIGGER_J9HOOK_VM_CLASS_INITIALIZE_FAILED(vm->hookInterface, currentThread, clazz);
			goto done;
		}
	} else {
		Trc_VM_initializeImpl_noClinit(currentThread);
	}

	VM_AtomicSupport::writeBarrier();
	TRIGGER_J9HOOK_VM_CLASS_INITIALIZE(vm->hookInterface, currentThread, clazz);
done:
	Trc_VM_initializeImpl_Exit(currentThread);
	return;
}

/**
 * Run the bytecode verifier on a J9Class.
 *
 * @param[in] *currentThread current thread
 * @param[in] *clazz the J9Class to verify
 */
static void
performVerification(J9VMThread *currentThread, J9Class *clazz)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9ROMClass *romClass = clazz->romClass;
	Trc_VM_performVerification_Entry(
			currentThread,
			J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(romClass)),
			J9UTF8_DATA(J9ROMCLASS_CLASSNAME(romClass)),
			clazz
	);

	/* See if verification is enabled */
	if (vm->runtimeFlags & J9_RUNTIME_VERIFY) {
		/* See if this class should be verified:
		 *
		 * - Do not verify any class created by sun.misc.Unsafe
		 * - Do not verify any class which is marked for exclusion in the optional flags
		 * - Verify every class whose bytecodes have been modified
		 * - Do not verify bootstrap classes if the appropriate runtime flag is set
		 */
		if (!J9CLASS_IS_EXEMPT_FROM_VALIDATION(clazz) && J9_ARE_NO_BITS_SET(romClass->optionalFlags, J9_ROMCLASS_OPTINFO_VERIFY_EXCLUDE)) {
			J9BytecodeVerificationData * bcvd = vm->bytecodeVerificationData;
			if ((J9ROMCLASS_HAS_MODIFIED_BYTECODES(romClass) ||
				(0 == (bcvd->verificationFlags & J9_VERIFY_SKIP_BOOTSTRAP_CLASSES)) ||
				!VM_VMHelpers::classIsBootstrap(vm, clazz))
			) {
				U_8 *verifyErrorStringUTF = NULL;
				Trc_VM_verification_Start(currentThread, J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(clazz->romClass)), J9UTF8_DATA(J9ROMCLASS_CLASSNAME(clazz->romClass)), clazz->classLoader);
				omrthread_monitor_enter(bcvd->verifierMutex);
				bcvd->vmStruct = currentThread;
				bcvd->classLoader = clazz->classLoader;
				IDATA verifyResult = j9bcv_verifyBytecodes(vm->portLibrary, clazz, romClass, bcvd);
				clazz = VM_VMHelpers::currentClass(clazz);
				bcvd->vmStruct = NULL;
				if (0 != verifyResult) {
					/* INL had a check for Object here which is unnecessary in SE */
					if (-2 == verifyResult) {
						omrthread_monitor_exit(bcvd->verifierMutex);
						/* vmStruct is already up to date */
						setNativeOutOfMemoryError(currentThread, J9NLS_BCV_ERR_VERIFY_OUT_OF_MEMORY);
						goto done;
					}
					verifyErrorStringUTF = j9bcv_createVerifyErrorString(vm->portLibrary, bcvd);
				}
				omrthread_monitor_exit(bcvd->verifierMutex);
				if (VM_VMHelpers::exceptionPending(currentThread)) {
					PORT_ACCESS_FROM_JAVAVM(vm);
					j9mem_free_memory(verifyErrorStringUTF);
					goto done;
				}
				if (NULL != verifyErrorStringUTF) {
					PORT_ACCESS_FROM_JAVAVM(vm);
					/* vmStruct is already up to date */
					j9object_t verifyErrorStringObject = vm->memoryManagerFunctions->j9gc_createJavaLangString(currentThread, verifyErrorStringUTF, strlen((char*)verifyErrorStringUTF), 0);
					j9mem_free_memory(verifyErrorStringUTF);
					setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGVERIFYERROR, (UDATA*)verifyErrorStringObject);
					goto done;
				}

				Trc_VM_verification_End(currentThread, J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(clazz->romClass)), J9UTF8_DATA(J9ROMCLASS_CLASSNAME(clazz->romClass)), clazz->classLoader);
			} else {
				Trc_VM_performVerification_unverifiable(currentThread);
			}
		}
	} else {
		Trc_VM_performVerification_noVerify(currentThread);
	}

	if (J9_ARE_ALL_BITS_SET(romClass->extraModifiers, J9AccClassNeedsStaticConstantInit)) {
		/* Prepare the class - the event is sent after the class init status has been updated */
		Trc_VM_performVerification_prepareClass(currentThread);
		romClass = clazz->romClass;
		UDATA *objectStaticAddress = clazz->ramStatics;
		UDATA *singleStaticAddress = objectStaticAddress + romClass->objectStaticCount;
		U_64 *doubleStaticAddress = (U_64*)(singleStaticAddress + romClass->singleScalarStaticCount);

#if !defined(J9VM_ENV_DATA64)
		if (0 != ((UDATA)doubleStaticAddress & (sizeof(U_64) - 1))) {
			/* Increment by a U_32 to ensure 64 bit aligned */
			doubleStaticAddress = (U_64*)(((U_32*)doubleStaticAddress) + 1);
		}
#endif

		/* initialize object slots first */
		J9ROMFieldWalkState fieldWalkState;
		J9ROMFieldShape *field = romFieldsStartDo(romClass, &fieldWalkState);
		while (field != NULL) {
			U_32 modifiers = field->modifiers;
			if (J9_ARE_ALL_BITS_SET(modifiers, J9AccStatic)) {
				const bool hasConstantValue = J9_ARE_ALL_BITS_SET(modifiers, J9FieldFlagConstant);
			
				if (J9_ARE_ALL_BITS_SET(modifiers, J9FieldFlagObject)) {
					if (hasConstantValue) {
						U_32 index = *(U_32*)romFieldInitialValueAddress(field);
						J9ConstantPool *ramConstantPool = J9_CP_FROM_CLASS(clazz);
						J9RAMStringRef *ramCPEntry = ((J9RAMStringRef*)ramConstantPool) + index;
						j9object_t stringObject = ramCPEntry->stringObject;
						if (NULL == stringObject) {
							resolveStringRef(currentThread, ramConstantPool, index, J9_RESOLVE_FLAG_RUNTIME_RESOLVE);
							clazz = VM_VMHelpers::currentClass(clazz);
							if (VM_VMHelpers::exceptionPending(currentThread)) {
								goto done;
							}
							stringObject = ramCPEntry->stringObject;
						}
						J9STATIC_OBJECT_STORE(currentThread, clazz, (j9object_t*)objectStaticAddress, stringObject);
						/* Overwriting NULL with a string that is in immortal, so no exception can occur */
					}
					objectStaticAddress += 1;
				} else if (0 == (modifiers & (J9FieldFlagObject | J9FieldSizeDouble))) {
					if (hasConstantValue) {
						*(U_32*)singleStaticAddress = *(U_32*)romFieldInitialValueAddress(field);
					}
					singleStaticAddress += 1;
				} else if (J9_ARE_ALL_BITS_SET(modifiers, J9FieldSizeDouble)) {
					if (hasConstantValue) {
						*doubleStaticAddress = *(U_64*)romFieldInitialValueAddress(field);
					}
					doubleStaticAddress += 1;
				} else {
					// Can't happen now - maybe in the future with valuetypes?
				}
			}
			field = romFieldsNextDo(&fieldWalkState);
		}
	}
done:
	return;
}

j9object_t
enterInitializationLock(J9VMThread *currentThread, j9object_t initializationLock)
{
	initializationLock = (j9object_t)VM_ObjectMonitor::enterObjectMonitor(currentThread, initializationLock);
	if (VM_VMHelpers::immediateAsyncPending(currentThread)) {
		Trc_VM_enterInitializationLock_async(currentThread);
		VM_ObjectMonitor::exitObjectMonitor(currentThread, initializationLock);
		initializationLock = NULL;
	} else if (NULL == initializationLock) {
		Trc_VM_enterInitializationLock_OOM(currentThread);
		setNativeOutOfMemoryError(currentThread, J9NLS_VM_FAILED_TO_ALLOCATE_MONITOR);
	}
	return initializationLock;
}

/**
 * Set the initializationStatus of a J9Class.
 *
 * @param[in] *currentThread current thread
 * @param[in] *clazz the J9Class to modify
 * @param[in] status the new status
 * @param[in] initializationLock the initialization lock object for the Class
 *
 * @return the initialization lock object (possibly relocated)
 */
static j9object_t
setInitStatus(J9VMThread *currentThread, J9Class *clazz, UDATA status, j9object_t initializationLock)
{
	Trc_VM_setInitStatus_newStatus(
			currentThread,
			status,
			STATE_NAME(status)
	);
	initializationLock = (j9object_t)VM_ObjectMonitor::enterObjectMonitor(currentThread, initializationLock);
	Assert_VM_false(NULL == initializationLock);
	clazz = VM_VMHelpers::currentClass(clazz);
	do {
		clazz->initializeStatus = status;
		clazz = clazz->replacedClass;
	} while (NULL != clazz);
	omrthread_monitor_t monitor = NULL;
	if (VM_ObjectMonitor::getMonitorForNotify(currentThread, initializationLock, &monitor, false)) {
		omrthread_monitor_notify_all(monitor);
	}
	VM_ObjectMonitor::exitObjectMonitor(currentThread, initializationLock);
	return initializationLock;
}

void  
prepareClass(J9VMThread *currentThread, J9Class *clazz)
{
	classInitStateMachine(currentThread, clazz, J9_CLASS_INIT_PREPARED);
}


void  
initializeClass(J9VMThread *currentThread, J9Class *clazz)
{
	classInitStateMachine(currentThread, clazz, J9_CLASS_INIT_INITIALIZED);
}

/**
 * Execute the state machine for class initialization, up to a defined endpoint.
 *
 * @param[in] *currentThread current thread
 * @param[in] *clazz the J9Class to verify
 * @param[in] desiredState how far to initialize the class
 */
static void
classInitStateMachine(J9VMThread *currentThread, J9Class *clazz, J9ClassInitState desiredState)
{
	J9JavaVM *vm = currentThread->javaVM;
	Assert_VM_true(clazz == VM_VMHelpers::currentClass(clazz));
	j9object_t classObject = J9VM_J9CLASS_TO_HEAPCLASS(clazz);
	j9object_t initializationLock = J9VMJAVALANGCLASS_INITIALIZATIONLOCK(currentThread, classObject);
	Trc_VM_classInitStateMachine_Entry(
			currentThread,
			J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(clazz->romClass)),
			J9UTF8_DATA(J9ROMCLASS_CLASSNAME(clazz->romClass)),
			clazz,
			clazz->classLoader,
			desiredStateNames[desiredState]
	);
	/* initializationLock == NULL means initialization successfully completed */
	if (NULL == initializationLock) {
		Trc_VM_classInitStateMachine_noLock(currentThread);
	} else {
		while (true) {
			UDATA const unlockedStatus = clazz->initializeStatus;
			Trc_VM_classInitStateMachine_status(
					currentThread,
					unlockedStatus,
					STATE_NAME(unlockedStatus)
			);
			switch(unlockedStatus) {
			case J9ClassInitSucceeded:
				goto done;
			case J9ClassInitUnverified: {
				initializationLock = enterInitializationLock(currentThread, initializationLock);
				clazz = VM_VMHelpers::currentClass(clazz);
				if (NULL == initializationLock) {
					goto done;
				}
				if (J9ClassInitUnverified != clazz->initializeStatus) {
					Trc_VM_classInitStateMachine_stateChanged(currentThread);
					VM_ObjectMonitor::exitObjectMonitor(currentThread, initializationLock);
					break;
				}
				/* Mark this class as verifying in this thread */
				Trc_VM_classInitStateMachine_markVerificationInProgress(currentThread);
				clazz->initializeStatus = (UDATA)currentThread | J9ClassInitUnverified;
				VM_ObjectMonitor::exitObjectMonitor(currentThread, initializationLock);
doVerify:
				/* Verify the superclass */
				J9Class *superclazz = VM_VMHelpers::getSuperclass(clazz);
				if (NULL != superclazz) {
					Trc_VM_classInitStateMachine_verifySuperclass(currentThread);
					PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, initializationLock);
					classInitStateMachine(currentThread, superclazz, J9_CLASS_INIT_VERIFIED);
					initializationLock = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);
					clazz = VM_VMHelpers::currentClass(clazz);
					if (VM_VMHelpers::exceptionPending(currentThread)) {
						initializationLock = setInitStatus(currentThread, clazz, J9ClassInitUnverified, initializationLock);
						clazz = VM_VMHelpers::currentClass(clazz);
						goto done;
					}
				}
				/* Verify this class */
				PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, initializationLock);
				performVerification(currentThread, clazz);
				if (!VM_VMHelpers::exceptionPending(currentThread)) {
					/* Validate class relationships if -XX:+ClassRelationshipVerifier is used and if the classfile major version is at least 51 (Java 7) */
					if (J9_ARE_ANY_BITS_SET(vm->extendedRuntimeFlags2, J9_EXTENDED_RUNTIME2_ENABLE_CLASS_RELATIONSHIP_VERIFIER) && (clazz->romClass->majorVersion >= CFR_MAJOR_VERSION_REQUIRING_STACKMAPS)) {
						U_8 *clazzName = J9UTF8_DATA(J9ROMCLASS_CLASSNAME(clazz->romClass));
						UDATA clazzNameLength = J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(clazz->romClass));
						J9Class *validateResult = j9bcv_validateClassRelationships(currentThread, clazz->classLoader, clazzName, clazzNameLength, clazz);

						if (NULL != validateResult) {
							U_8 *resultName = J9UTF8_DATA(J9ROMCLASS_CLASSNAME(validateResult->romClass));
							UDATA resultNameLength = J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(validateResult->romClass));
							Trc_VM_classInitStateMachine_classRelationshipValidationFailed(currentThread, clazzNameLength, clazzName, resultNameLength, resultName);
							setCurrentExceptionNLSWithArgs(currentThread, J9NLS_VM_CLASS_RELATIONSHIP_INVALID, J9VMCONSTANTPOOL_JAVALANGVERIFYERROR, clazzNameLength, clazzName, resultNameLength, resultName);
						}
					}
				}
				initializationLock = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);
				clazz = VM_VMHelpers::currentClass(clazz);
				if (VM_VMHelpers::exceptionPending(currentThread)) {
					initializationLock = setInitStatus(currentThread, clazz, J9ClassInitUnverified, initializationLock);
					clazz = VM_VMHelpers::currentClass(clazz);
					goto done;
				}
				initializationLock = enterInitializationLock(currentThread, initializationLock);
				clazz = VM_VMHelpers::currentClass(clazz);
				if (NULL == initializationLock) {
					goto done;
				}
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
				if (J9_IS_J9CLASS_VALUETYPE(clazz)) {
					PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, initializationLock);
					/* Preparation is the earliest point where the defaultValue would needed. 
					* I.e pre-filling static fields. Therefore, the defaultValue must be allocated at 
					* the end of verification 
					*/
					j9object_t defaultValue = vm->memoryManagerFunctions->J9AllocateObject(currentThread, clazz, J9_GC_ALLOCATE_OBJECT_TENURED | J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
					initializationLock = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);
					clazz = VM_VMHelpers::currentClass(clazz);
					if (NULL == defaultValue) {
						setHeapOutOfMemoryError(currentThread);
						goto done;
					}
					j9object_t *defaultValueAddress = &(clazz->flattenedClassCache->defaultValue);
					J9STATIC_OBJECT_STORE(currentThread, clazz, defaultValueAddress, defaultValue);
				}
#endif
				if (((UDATA)currentThread | J9ClassInitUnverified) == clazz->initializeStatus) {
					initializationLock = setInitStatus(currentThread, clazz, J9ClassInitUnprepared, initializationLock);
					clazz = VM_VMHelpers::currentClass(clazz);
				}
				VM_ObjectMonitor::exitObjectMonitor(currentThread, initializationLock);
				break;
			}
			case J9ClassInitUnprepared: {
				if (desiredState < J9_CLASS_INIT_PREPARED) {
					Trc_VM_classInitStateMachine_desiredStateReached(currentThread);
					goto done;
				}
				J9Class *superclazz = VM_VMHelpers::getSuperclass(clazz);
				J9ITable *superITable = NULL;
				if (NULL != superclazz) {
					Trc_VM_classInitStateMachine_prepareSuperclass(currentThread);
					PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, initializationLock);
					classInitStateMachine(currentThread, superclazz, J9_CLASS_INIT_PREPARED);
					initializationLock = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);
					clazz = VM_VMHelpers::currentClass(clazz);
					superclazz = VM_VMHelpers::getSuperclass(clazz);
					superITable = (J9ITable*)superclazz->iTable;
					if (VM_VMHelpers::exceptionPending(currentThread)) {
						goto done;
					}
				}
				/* Prepare the interfaces */
				J9ITable *firstITable = (J9ITable*)clazz->iTable;
				J9ITable *iTable = firstITable;
				while (iTable != superITable) {
					J9Class *interfaceClazz = iTable->interfaceClass;
					if (interfaceClazz != clazz) {
						Trc_VM_classInitStateMachine_prepareSuperinterface(currentThread);
						PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, initializationLock);
						classInitStateMachine(currentThread, interfaceClazz, J9_CLASS_INIT_PREPARED);
						initializationLock = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);
						clazz = VM_VMHelpers::currentClass(clazz);
						if (VM_VMHelpers::exceptionPending(currentThread)) {
							goto done;
						}
						/* Ensure that we are still traversing a valid iTable chain */
						if (firstITable != (J9ITable*)clazz->iTable) {
							iTable = (J9ITable*)clazz->iTable;
							if (NULL != superclazz) {
								superclazz = VM_VMHelpers::getSuperclass(clazz);
								superITable = (J9ITable*)superclazz->iTable;
							}
							continue;
						}
					}
					iTable = iTable->next;
				}
				/* Prepare this class */
				initializationLock = enterInitializationLock(currentThread, initializationLock);
				clazz = VM_VMHelpers::currentClass(clazz);
				if (NULL == initializationLock) {
					goto done;
				}
				if (J9ClassInitUnprepared != clazz->initializeStatus) {
					VM_ObjectMonitor::exitObjectMonitor(currentThread, initializationLock);
					break;
				}
				initializationLock = setInitStatus(currentThread, clazz, J9ClassInitNotInitialized, initializationLock);
				clazz = VM_VMHelpers::currentClass(clazz);
				VM_ObjectMonitor::exitObjectMonitor(currentThread, initializationLock);
				PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, initializationLock);
				TRIGGER_J9HOOK_VM_CLASS_PREPARE(vm->hookInterface, currentThread, clazz);
				initializationLock = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);
				clazz = VM_VMHelpers::currentClass(clazz);
				break;
			}
			case J9ClassInitFailed: {
				/* J9ClassInitFailed can only be set when unlockedStatus is J9ClassInitNotInitialized, and desiredState is J9_CLASS_INIT_INITIALIZED.
				 * J9ClassInitFailed can be ignored if desiredState is not J9_CLASS_INIT_INITIALIZED, i.e., J9_CLASS_INIT_VERIFIED or J9_CLASS_INIT_PREPARED.
				 */
				if (desiredState < J9_CLASS_INIT_INITIALIZED) {
					Trc_VM_classInitStateMachine_desiredStateReached(currentThread);
				} else {
					sendInitializationAlreadyFailed(currentThread, clazz);
				}
				goto done;
			}
			case J9ClassInitNotInitialized: {
				if (desiredState < J9_CLASS_INIT_INITIALIZED) {
					Trc_VM_classInitStateMachine_desiredStateReached(currentThread);
					goto done;
				}
				initializationLock = enterInitializationLock(currentThread, initializationLock);
				clazz = VM_VMHelpers::currentClass(clazz);
				if (NULL == initializationLock) {
					goto done;
				}
				if (J9ClassInitNotInitialized != clazz->initializeStatus) {
					Trc_VM_classInitStateMachine_stateChanged(currentThread);
					VM_ObjectMonitor::exitObjectMonitor(currentThread, initializationLock);
					break;
				}
				/* Mark this class as initializing in this thread */
				Trc_VM_classInitStateMachine_markInitializationInProgress(currentThread);
				clazz->initializeStatus = (UDATA)currentThread | J9ClassInitNotInitialized;
				VM_ObjectMonitor::exitObjectMonitor(currentThread, initializationLock);
				/* Initialize the superclass */
				J9Class *superclazz = VM_VMHelpers::getSuperclass(clazz);
				J9ITable *superITable = NULL;
				J9ITable *firstITable = NULL;
				J9ITable *iTable = NULL;
				if (NULL != superclazz) {
					Trc_VM_classInitStateMachine_initSuperclass(currentThread);
					PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, initializationLock);
					classInitStateMachine(currentThread, superclazz, J9_CLASS_INIT_INITIALIZED);
					initializationLock = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);
					clazz = VM_VMHelpers::currentClass(clazz);
					superclazz = VM_VMHelpers::getSuperclass(clazz);
					if (VM_VMHelpers::exceptionPending(currentThread)) {
						goto initFailed;
					}
					superITable = (J9ITable*)superclazz->iTable;
				}
				/* Do not initialize the superinterfaces of interfaces */
				if (!J9_ARE_ANY_BITS_SET(clazz->romClass->modifiers, J9AccInterface)) {
					/* Initialize super-interfaces with non-static, non-abstract methods */
					Trc_VM_classInitStateMachine_initSuperInterfacesWithNonStaticNonAbstractMethods(currentThread, clazz);
					/* Don't traverse all iTables - indirect super-interfaces are initialized with the superclass */
					firstITable = (J9ITable*)clazz->iTable;
					iTable = firstITable;
					while (iTable != superITable) {
						J9Class *interfaceClazz = iTable->interfaceClass;
						if (J9_ARE_ANY_BITS_SET(interfaceClazz->romClass->extraModifiers, J9AccClassHasNonStaticNonAbstractMethods)) {
							PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, initializationLock);
							classInitStateMachine(currentThread, interfaceClazz, J9_CLASS_INIT_INITIALIZED);
							initializationLock = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);
							clazz = VM_VMHelpers::currentClass(clazz);
							if (VM_VMHelpers::exceptionPending(currentThread)) {
								goto initFailed;
							}
							/* Ensure that we are still traversing a valid iTable chain */
							if (firstITable != (J9ITable*)clazz->iTable) {
								iTable = (J9ITable*)clazz->iTable;
								if (NULL != superclazz) {
									superclazz = VM_VMHelpers::getSuperclass(clazz);
									superITable = (J9ITable*)superclazz->iTable;
								}
								continue;
							}
						}
						iTable = iTable->next;
					}
				}
				/* Initialize this class */
				PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, initializationLock);
				initializeImpl(currentThread, clazz);
				initializationLock = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);
				clazz = VM_VMHelpers::currentClass(clazz);
				if (VM_VMHelpers::exceptionPending(currentThread)) {
initFailed:
					Trc_VM_classInitStateMachine_clinitFailed(currentThread);
					j9object_t throwable = currentThread->currentException;
					currentThread->currentException = NULL;
					PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, initializationLock);
					sendRecordInitializationFailure(currentThread, clazz, throwable);
					initializationLock = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);
					clazz = VM_VMHelpers::currentClass(clazz);
					initializationLock = setInitStatus(currentThread, clazz, J9ClassInitFailed, initializationLock);
					clazz = VM_VMHelpers::currentClass(clazz);
					goto done;
				}
				initializationLock = setInitStatus(currentThread, clazz, J9ClassInitSucceeded, initializationLock);
				clazz = VM_VMHelpers::currentClass(clazz);
				classObject = J9VM_J9CLASS_TO_HEAPCLASS(clazz);
				J9VMJAVALANGCLASS_SET_INITIALIZATIONLOCK(currentThread, classObject, NULL);
				goto done;
			}
			default: { // IN PROGRESS (status contains a thread in the high bits)
				initializationLock = enterInitializationLock(currentThread, initializationLock);
				clazz = VM_VMHelpers::currentClass(clazz);
				if (NULL == initializationLock) {
					goto done;
				}
				UDATA const status = clazz->initializeStatus;
				J9VMThread *initializingThread = (J9VMThread*)(status & ~(UDATA)J9ClassInitStatusMask);
				if (NULL == initializingThread) {
					Trc_VM_classInitStateMachine_stateChanged(currentThread);
					VM_ObjectMonitor::exitObjectMonitor(currentThread, initializationLock);
					break;
				}
				/* Only verification and initialization can be in the "in progress" state */
				if (J9ClassInitUnverified == (status & J9ClassInitStatusMask)) {
					Trc_VM_classInitStateMachine_verificationInProgress(currentThread);
					if (initializingThread == currentThread) {
						Trc_VM_classInitStateMachine_verificationInProgress(currentThread);
						VM_ObjectMonitor::exitObjectMonitor(currentThread, initializationLock);
						goto doVerify;
					}
				} else {
					Trc_VM_classInitStateMachine_initializationInProgress(currentThread);
					if (desiredState < J9_CLASS_INIT_INITIALIZED) {
						Trc_VM_classInitStateMachine_desiredStateReached(currentThread);
						VM_ObjectMonitor::exitObjectMonitor(currentThread, initializationLock);
						goto done;
					}
					if (initializingThread == currentThread) {
						/* Being initialized by the current thread */
						Trc_VM_classInitStateMachine_initializingOnCurrentThread(currentThread);
						VM_ObjectMonitor::exitObjectMonitor(currentThread, initializationLock);
						goto done;
					}
				}
				Trc_VM_classInitStateMachine_waitForOtherThread(currentThread);
				PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, initializationLock);
				monitorWaitImpl(currentThread, initializationLock, 0, 0, FALSE);
				initializationLock = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);
				clazz = VM_VMHelpers::currentClass(clazz);
				VM_ObjectMonitor::exitObjectMonitor(currentThread, initializationLock);
				if (VM_VMHelpers::exceptionPending(currentThread) || VM_VMHelpers::immediateAsyncPending(currentThread)) {
					Trc_VM_classInitStateMachine_waitFailed(currentThread);
					goto done;
				}
				Trc_VM_classInitStateMachine_waitComplete(currentThread);
				break;
			}
			} /* switch */
		} /* while(true) */
	}
done:
	Trc_VM_classInitStateMachine_Exit(currentThread);
	return;
}
} /* extern "C" */
