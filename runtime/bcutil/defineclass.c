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

#include <stdlib.h>
#include <string.h>

#include "j9.h"
#include "j9port.h"
#include "j9protos.h"
#include "j9consts.h"
#include "ut_j9bcu.h"
#include "objhelp.h"
#include "j2sever.h"
#include "bcutil_api.h"
#include "bcutil_internal.h"
#include "SCQueryFunctions.h"
#include "j9jclnls.h"

#if (defined(J9VM_OPT_DYNAMIC_LOAD_SUPPORT))  /* File Level Build Flags */

static UDATA classCouldPossiblyBeShared(J9VMThread * vmThread, J9LoadROMClassData * loadData);
static J9ROMClass * createROMClassFromClassFile (J9VMThread *currentThread, J9LoadROMClassData * loadData, J9TranslationLocalBuffer *localBuffer);
static void throwNoClassDefFoundError (J9VMThread* vmThread, J9LoadROMClassData * loadData);
static void reportROMClassLoadEvents (J9VMThread* vmThread, J9ROMClass* romClass, J9ClassLoader* classLoader);
static J9Class* checkForExistingClass (J9VMThread* vmThread, J9LoadROMClassData * loadData);
static UDATA callDynamicLoader(J9VMThread* vmThread, J9LoadROMClassData *loadData, U_8 * intermediateClassData, UDATA intermediateClassDataLength, UDATA translationFlags, UDATA classFileBytesReplacedByRIA, UDATA classFileBytesReplacedByRCA, J9TranslationLocalBuffer *localBuffer);

static BOOLEAN hasSamePackageName(J9ROMClass *anonROMClass, J9ROMClass *hostROMClass);
static char* createErrorMessage(J9VMThread *vmStruct, J9ROMClass *anonROMClass, J9ROMClass *hostROMClass, const char* errorMsg);
static void setIllegalArgumentExceptionHostClassAnonClassHaveDifferentPackages(J9VMThread *vmStruct, J9ROMClass *anonROMClass, J9ROMClass *hostROMClass);
static void freeAnonROMClass(J9JavaVM *vm, J9ROMClass *romClass);

#define GET_CLASS_LOADER_FROM_ID(vm, classLoader) ((classLoader) != NULL ? (classLoader) : (vm)->systemClassLoader)

/*
 * Warning: sender must hold class table mutex before calling.
 */
J9Class*
internalDefineClass(
	J9VMThread* vmThread,
	void* className,
	UDATA classNameLength,
	U_8* classData,
	UDATA classDataLength,
	j9object_t classDataObject,
	J9ClassLoader* classLoader,
	j9object_t protectionDomain,
	UDATA options,
	J9ROMClass *existingROMClass,
	J9Class *hostClass,
	J9TranslationLocalBuffer *localBuffer)
{
	J9JavaVM* vm = vmThread->javaVM;
	J9ROMClass* orphanROMClass = NULL;
	J9ROMClass* romClass = NULL;
	J9Class* result = NULL;
	J9LoadROMClassData loadData = {0};
	BOOLEAN isAnonFlagSet = J9_ARE_ALL_BITS_SET(options, J9_FINDCLASS_FLAG_ANON);

	/* This trace point is obsolete. It is retained only because j9vm test depends on it.
	 * Once j9vm tests are fixed, it would be marked as Obsolete in j9bcu.tdf
	 */
	Trc_BCU_internalDefineClass_Entry(vmThread, className, classNameLength, className);

	Trc_BCU_internalDefineClass_Entry1(vmThread, className, classNameLength, className, existingROMClass);
	Trc_BCU_internalDefineClass_FullData(vmThread, classDataLength, classData, classLoader);

	classLoader = GET_CLASS_LOADER_FROM_ID(vm, classLoader);

	/* remember the current classpath entry so we can record it at the end */
	vmThread->privateFlags &= ~J9_PRIVATE_FLAGS_CLOAD_NO_MEM;

	loadData.classBeingRedefined = NULL;
	loadData.className = className;
	loadData.classNameLength = classNameLength;
	loadData.classData = classData;
	loadData.classDataLength = classDataLength;
	loadData.classDataObject = classDataObject;
	/* use anonClassLoader if this is an anonClass */
	loadData.classLoader = classLoader;
	if (J9_ARE_ALL_BITS_SET(options, J9_FINDCLASS_FLAG_ANON)) {
		loadData.classLoader = vm->anonClassLoader;
	}
	loadData.protectionDomain = protectionDomain;
	loadData.options = options;
	loadData.freeUserData = NULL;
	loadData.freeFunction = NULL;
	loadData.romClass = existingROMClass;

	if (J9_ARE_NO_BITS_SET(options, J9_FINDCLASS_FLAG_NO_CHECK_FOR_EXISTING_CLASS)) {
		/* For non-bootstrap classes, this check is done in jcldefine.c:defineClassCommon(). */
		if (checkForExistingClass(vmThread, &loadData) != NULL) {
			Trc_BCU_internalDefineClass_Exit(vmThread, className, NULL);
			return NULL;
		}
	}

	if (!isAnonFlagSet) {
		/* See if there's already an orphan romClass available - still own classTableMutex at this point */
		if (NULL != classLoader->romClassOrphansHashTable) {
			orphanROMClass = romClassHashTableFind(classLoader->romClassOrphansHashTable, className, classNameLength);
			if (NULL != orphanROMClass) {
				loadData.romClass = orphanROMClass;
				/* If enableBCI is specified, class was loaded from shared cache and previous attempt to create RAMClass failed,
				 * we can use the orphanROMClass created in previous attempt.
				 */
				if (0 != (loadData.options & J9_FINDCLASS_FLAG_SHRC_ROMCLASS_EXISTS)) {
					Trc_BCU_Assert_NotEquals(NULL, vm->sharedClassConfig);
					Trc_BCU_Assert_True(vm->sharedClassConfig->isBCIEnabled(vm));
					romClass = loadData.romClass;
				}

			}
		}
	}

	if (NULL == romClass) {
		/* Attempt to create the romClass.
		 * When romClass exists in the cache, this call gives JVM a chance to trigger ClassFileLoadHook events.
		 * If ClassFileLoadHook event modifies the class file, it creates a new ROMClass but does not store it in shared class cache.
		 */
		romClass = createROMClassFromClassFile(vmThread, &loadData, localBuffer);
	}
	if (romClass) {
		/* Host class can only be set for anonymous classes, which are defined by Unsafe.defineAnonymousClass.
		 * For other cases, host class is set to NULL.
		 */
		if ((NULL != hostClass) && (J2SE_VERSION(vm) >= J2SE_V11)) {
			J9ROMClass *hostROMClass = hostClass->romClass;
			/* This error-check should only be done for anonymous classes. */
			Trc_BCU_Assert_True(isAnonFlagSet);
			/* From Java 9 and onwards, set IllegalArgumentException when host class and anonymous class have different packages. */
			if (!hasSamePackageName(romClass, hostROMClass)) {
				omrthread_monitor_exit(vm->classTableMutex);
				setIllegalArgumentExceptionHostClassAnonClassHaveDifferentPackages(vmThread, romClass, hostROMClass);
				freeAnonROMClass(vm, romClass);
				goto done;
			}
		}

		/* report the ROM load events */
		reportROMClassLoadEvents(vmThread, romClass, classLoader);

		/* localBuffer should not be NULL */
		Trc_BCU_Assert_True(NULL != localBuffer);

		result = J9_VM_FUNCTION(vmThread, internalCreateRAMClassFromROMClass)(vmThread, classLoader, romClass, options,
				NULL, loadData.protectionDomain, NULL, (IDATA)localBuffer->entryIndex, (IDATA)localBuffer->loadLocationType, NULL, hostClass);
		if (NULL == result) {
			/* ramClass creation failed - remember the orphan romClass for next time */
			if (orphanROMClass != romClass) {
				J9HashTable *hashTable = NULL;

				/* All access to the orphan table must be done while holding classTableMutex */
				omrthread_monitor_enter(vm->classTableMutex);
				hashTable = classLoader->romClassOrphansHashTable;
				if (NULL == hashTable) {
					hashTable = romClassHashTableNew(vm, 16);
					classLoader->romClassOrphansHashTable = hashTable;
				}

				/* It is possible to fail to create a hashTable, make sure one exists before trying to use it. */
				if (NULL != hashTable) {
					if (NULL != orphanROMClass) {
						/* replace the previous entry */
						Trc_BCU_romClassOrphansHashTableReplace(vmThread, classNameLength, className, romClass);
						romClassHashTableReplace(hashTable, orphanROMClass, romClass);
					} else {
						Trc_BCU_romClassOrphansHashTableAdd(vmThread, classNameLength, className, romClass);
						romClassHashTableAdd(hashTable, romClass);
					}
				}
				omrthread_monitor_exit(vm->classTableMutex);
			}
		} else if (NULL != orphanROMClass) {
			J9ROMClass *tableEntry = NULL;
			/* ramClass creation succeeded - the orphanROMClass is no longer an orphan */
			Trc_BCU_romClassOrphansHashTableDelete(vmThread, classNameLength, className, orphanROMClass);
			/* All access to the orphan table must be done while holding classTableMutex.
			 * Ensure the entry is still in the table before removing it.
			 */
			omrthread_monitor_enter(vm->classTableMutex);
			tableEntry = romClassHashTableFind(classLoader->romClassOrphansHashTable, className, classNameLength);
			if (tableEntry == orphanROMClass) {
				romClassHashTableDelete(classLoader->romClassOrphansHashTable, orphanROMClass);
			} else {
				Trc_BCU_internalDefineClass_orphanNotFound(vmThread, orphanROMClass, tableEntry);
			}
			omrthread_monitor_exit(vm->classTableMutex);
		}
	}

done:
	Trc_BCU_internalDefineClass_Exit(vmThread, className, result);

	return result;
}

static void
throwNoClassDefFoundError(J9VMThread* vmThread, J9LoadROMClassData * loadData)
{
	J9JavaVM* vm = vmThread->javaVM;
	U_8* errorBuf;
	UDATA bufSize;
	PORT_ACCESS_FROM_JAVAVM(vm);

	Trc_BCU_throwNoClassDefFoundError_Entry(vmThread);

	bufSize = loadData->classNameLength;
	errorBuf = j9mem_allocate_memory(bufSize + 1, J9MEM_CATEGORY_CLASSES);

	if (errorBuf) {
		memcpy(errorBuf, loadData->className, bufSize);
		errorBuf[bufSize] = (U_8) '\0';

		Trc_BCU_throwNoClassDefFoundError_ErrorBuf(vmThread, errorBuf);

		J9_VM_FUNCTION(vmThread, setCurrentExceptionUTF)(vmThread, J9VMCONSTANTPOOL_JAVALANGNOCLASSDEFFOUNDERROR, (char *) errorBuf);

		j9mem_free_memory(errorBuf);
	} else {
		J9_VM_FUNCTION(vmThread, setCurrentException)(vmThread, J9VMCONSTANTPOOL_JAVALANGNOCLASSDEFFOUNDERROR, NULL);
	}

	Trc_BCU_throwNoClassDefFoundError_Exit(vmThread);
}

/*
 * Check to see if a class with this name is already loaded.
 * If so, exit the classTableMutex, throw a NoClassDefFoundError (but only if throw-on-fail is set) and return the existing class.
 * Otherwise return NULL.
 */
static J9Class*
checkForExistingClass(J9VMThread* vmThread, J9LoadROMClassData * loadData)
{
	J9JavaVM* vm = vmThread->javaVM;
	J9Class* existingClass;

	Trc_BCU_checkForExistingClass_Entry(vmThread, loadData->className, loadData->classLoader);

	existingClass = J9_VM_FUNCTION(vmThread, hashClassTableAt)(
		loadData->classLoader,
		loadData->className,
		loadData->classNameLength);

	if (existingClass != NULL) {
		/* error! a class with this name is already loaded in this class loader */
#ifdef J9VM_THR_PREEMPTIVE
		omrthread_monitor_exit(vm->classTableMutex);
#endif

		Trc_BCU_checkForExistingClass_Exists(vmThread);

		if (loadData->options & J9_FINDCLASS_FLAG_THROW_ON_FAIL) {
			throwNoClassDefFoundError(vmThread, loadData);
		}

		Trc_BCU_checkForExistingClass_Exit(vmThread, existingClass);
		return existingClass;
	}

	Trc_BCU_checkForExistingClass_Exit(vmThread, NULL);
	return NULL;
}

static void
reportROMClassLoadEvents(J9VMThread* vmThread, J9ROMClass* romClass, J9ClassLoader* classLoader)
{
	J9JavaVM* vm = vmThread->javaVM;

	/*
	 * J9HOOK_ROM_CLASS_LOAD
	 */
	TRIGGER_J9HOOK_VM_ROM_CLASS_LOAD(vm->hookInterface, vmThread, romClass);
}

/*
 * Warning: sender must hold class table mutex before calling.
 */
UDATA
internalLoadROMClass(J9VMThread * vmThread, J9LoadROMClassData *loadData, J9TranslationLocalBuffer *localBuffer)
{
	J9JavaVM * vm = vmThread->javaVM;
	UDATA result;
	UDATA translationFlags;
	UDATA classFileBytesReplacedByRIA = FALSE;
	UDATA classFileBytesReplacedByRCA = FALSE;
	U_8 * intermediateClassData = loadData->classData;
	UDATA intermediateClassDataLength = loadData->classDataLength;
	void* intermedtiateFreeUserData = NULL;
	classDataFreeFunction intermediateFreeFunction = NULL;
	PORT_ACCESS_FROM_VMC(vmThread);

	Trc_BCU_internalLoadROMClass_Entry(vmThread, loadData, loadData->classDataLength);

#if 0
	/*This block of code is disabled until CMVC 155494 is resolved. Otherwise it will block cbuilds*/
	Trc_Assert_BCU_mustHaveVMAccess(vmThread);
#endif

	/* Call the class load hook to potentially replace the class data */

	if ((J9_ARE_NO_BITS_SET(loadData->options, J9_FINDCLASS_FLAG_ANON))
		&& (J9_EVENT_IS_HOOKED(vm->hookInterface, J9HOOK_VM_CLASS_LOAD_HOOK) || J9_EVENT_IS_HOOKED(vm->hookInterface, J9HOOK_VM_CLASS_LOAD_HOOK2))
	) {
		U_8 * classData = NULL;
		char * className = NULL;

		Trc_BCU_internalLoadROMClass_ClassLoadHookEnter(vmThread);

		/* If shared cache is BCI enabled and the class is found in shared cache while loading then
		 * loadData->classData points to intermediate ROMClass,
		 * which needs to be converted back to classfile bytes before passing to the agents.
		 */
		if (J9_ARE_ALL_BITS_SET(loadData->options, J9_FINDCLASS_FLAG_SHRC_ROMCLASS_EXISTS)) {
			U_8 * classFileBytes = NULL;
			U_32 classFileBytesCount = 0;
			IDATA result = 0;
			J9ROMClass * intermediateROMClass = (J9ROMClass *) loadData->classData;

			result = j9bcutil_transformROMClass(vm, PORTLIB, intermediateROMClass, &classFileBytes, &classFileBytesCount);
			if (BCT_ERR_NO_ERROR != result) {
				Trc_BCU_internalLoadROMClass_ErrorInRecreatingClassfile(vmThread, intermediateROMClass, result);
				goto done;
			} else {
				/* Successfully recreated classfile bytes from intermediate ROMClass. */
				loadData->classData = classFileBytes;
				loadData->classDataLength = classFileBytesCount;
			}
		} else {
			result = BCT_ERR_OUT_OF_MEMORY;
			/* Make a copy of the class data */
			classData = j9mem_allocate_memory(loadData->classDataLength, J9MEM_CATEGORY_CLASSES);
			if (classData == NULL) {
				goto done;
			}
			/* If arraylets are in use, the classData will never be an object, it will have been copied before calling internalDefineClass */
			memcpy(classData, loadData->classData, loadData->classDataLength);
			if (loadData->freeFunction) {
				loadData->freeFunction(loadData->freeUserData, loadData->classData);
			}
			loadData->classData = classData;
		}
		loadData->freeUserData = PORTLIB;
		loadData->freeFunction = (classDataFreeFunction) OMRPORT_FROM_J9PORT(PORTLIB)->mem_free_memory;
		loadData->classDataObject = NULL;

		/* Make a copy of the class name */

		if (loadData->classNameLength == 0) {
			className = NULL;
		} else {
			UDATA classNameLength = loadData->classNameLength;
			if (J9_ARE_ALL_BITS_SET(loadData->options, J9_FINDCLASS_FLAG_ANON)) {
				classNameLength -= (1 + ROM_ADDRESS_LENGTH);
			}
			className = j9mem_allocate_memory(classNameLength + 1, J9MEM_CATEGORY_CLASSES);
			if (className == NULL) {
				/* free class data before exiting */
				if (NULL != classData) {
					j9mem_free_memory(classData);
					classData = NULL;
				}
				goto done;
			}
			memcpy(className, loadData->className, classNameLength);
			className[classNameLength] = '\0';
		}

#ifdef J9VM_THR_PREEMPTIVE
		omrthread_monitor_exit(vm->classTableMutex);
#endif

		if ((loadData->options & J9_FINDCLASS_FLAG_RETRANSFORMING) == 0) {
			ALWAYS_TRIGGER_J9HOOK_VM_CLASS_LOAD_HOOK(vm->hookInterface,
				vmThread,
				loadData->classLoader,
				loadData->protectionDomain,
				loadData->classBeingRedefined,
				className,
				loadData->classData,
				loadData->classDataLength,
				loadData->freeUserData,
				loadData->freeFunction,
				classFileBytesReplacedByRIA);
		}

		/* Record the bytes from the above hook call */

		intermediateClassData = loadData->classData;
		intermediateClassDataLength = loadData->classDataLength;
		intermedtiateFreeUserData = loadData->freeUserData;
		intermediateFreeFunction = loadData->freeFunction;
		loadData->freeFunction = NULL;

#if defined(J9VM_OPT_JVMTI)

		/* Run the second transformation pass */

		ALWAYS_TRIGGER_J9HOOK_VM_CLASS_LOAD_HOOK2(vm->hookInterface,
			vmThread,
			loadData->classLoader,
			loadData->protectionDomain,
			loadData->classBeingRedefined,
			className,
			loadData->classData,
			loadData->classDataLength,
			loadData->freeUserData,
			loadData->freeFunction,
			classFileBytesReplacedByRCA);
#endif

#ifdef J9VM_THR_PREEMPTIVE
		omrthread_monitor_enter(vm->classTableMutex);
#endif

		if (className != NULL) {
			j9mem_free_memory(className);
			className = NULL;
		}

		Trc_BCU_internalLoadROMClass_ClassLoadHookDone(vmThread);
	}

	if (J9_ARE_ALL_BITS_SET(loadData->options, J9_FINDCLASS_FLAG_RETRANSFORMING)
		&& (FALSE == classFileBytesReplacedByRCA)
	) {
		Trc_BCU_Assert_True(FALSE == classFileBytesReplacedByRIA);

		/* Class file is not modified during retransformation.
		 * If active ROMClass->intermediateClassData is a ROMClass then re-use it.
		 */
		if (!J9ROMCLASS_IS_INTERMEDIATE_DATA_A_CLASSFILE(loadData->classBeingRedefined->romClass)) {
			loadData->romClass = (J9ROMClass *) J9ROMCLASS_INTERMEDIATECLASSDATA(loadData->classBeingRedefined->romClass);
			result = BCT_ERR_NO_ERROR;
			goto doneFreeMem;
		}
	}

	if (0 != (loadData->options & J9_FINDCLASS_FLAG_SHRC_ROMCLASS_EXISTS)) {
		/* We are running with -Xshareclasses:enableBCI and have already found ROMClass in shared class cache. */
		if ((FALSE == classFileBytesReplacedByRIA)
			&& (FALSE == classFileBytesReplacedByRCA)
		) {
			/* Either we are using BCI agent which didn't modify the class file data or there is no BCI agent. */
			Trc_BCU_Assert_NotEquals(NULL, loadData->romClass);
			result = BCT_ERR_NO_ERROR;
			goto doneFreeMem;
		}
	}

	/* Set up translation flags */

#ifdef J9VM_ENV_LITTLE_ENDIAN
	translationFlags = BCT_LittleEndianOutput;
#else
	translationFlags = BCT_BigEndianOutput;
#endif

	/*
	 * RECORD_ALL is set by shared classes when it wishes to keep all debug information in the cache
	 * classCouldPossiblyBeShared() returns true when the classloader is a shared classes enabled loader AND the cache is NOT full
	 *
	 * do NOT attempt to strip debug information when RECORD_ALL is set AND the class could end up the cache.
	 *
	 */
	if ((J9VM_DEBUG_ATTRIBUTE_RECORD_ALL == (vm->requiredDebugAttributes & J9VM_DEBUG_ATTRIBUTE_RECORD_ALL))
			&& classCouldPossiblyBeShared(vmThread, loadData)) {
		/* Shared Classes has requested that all debug information be kept and the class will be shared. */
	} else {
		/* either the class is not going to be shared  -or- shared classes does not require the debug information to be maintained */
		UDATA stripFlags = 0;

		if (0 == (vm->requiredDebugAttributes & J9VM_DEBUG_ATTRIBUTE_LOCAL_VARIABLE_TABLE)) {
			stripFlags |= BCT_StripDebugVars;
		}
		if (0 == (vm->requiredDebugAttributes & J9VM_DEBUG_ATTRIBUTE_LINE_NUMBER_TABLE)) {
			stripFlags |= BCT_StripDebugLines;
		}
		if (0 == (vm->requiredDebugAttributes & J9VM_DEBUG_ATTRIBUTE_SOURCE_FILE)) {
			stripFlags |= BCT_StripDebugSource;
		}
		if (0 == (vm->requiredDebugAttributes & J9VM_DEBUG_ATTRIBUTE_SOURCE_DEBUG_EXTENSION)) {
			stripFlags |= BCT_StripSourceDebugExtension;
		}

		if (stripFlags == (BCT_StripDebugVars | BCT_StripDebugLines | BCT_StripDebugSource | BCT_StripSourceDebugExtension)) {
			stripFlags = BCT_StripDebugAttributes;
		}
		translationFlags |= stripFlags;
	}

	if ((0 == (loadData->options & J9_FINDCLASS_FLAG_UNSAFE)) &&
		(0 != (vm->runtimeFlags & J9_RUNTIME_VERIFY))) {
		translationFlags |= BCT_StaticVerification;
	}

	if (0 != (vm->runtimeFlags & J9_RUNTIME_XFUTURE)) {
		translationFlags |= BCT_Xfuture;
	} else {
		/* Disable static verification for the bootstrap loader if Xfuture not present */
		if ((vm->systemClassLoader == loadData->classLoader)
		&& ((NULL == vm->bytecodeVerificationData) || (0 == (vm->bytecodeVerificationData->verificationFlags & J9_VERIFY_BOOTCLASSPATH_STATIC)))
		&& (NULL == vm->sharedClassConfig)
		) {
			translationFlags &= ~BCT_StaticVerification;
		}
	}

	if (J9_ARE_ANY_BITS_SET(vm->runtimeFlags, J9_RUNTIME_ALWAYS_SPLIT_BYTECODES)) {
		translationFlags |= BCT_AlwaysSplitBytecodes;
	}
	if (J9_ARE_ANY_BITS_SET(vm->extendedRuntimeFlags2, J9_EXTENDED_RUNTIME2_ENABLE_PREVIEW)) {
		translationFlags |= BCT_EnablePreview;
	}

	if (J9_ARE_ALL_BITS_SET(vm->extendedRuntimeFlags2, J9_EXTENDED_RUNTIME2_ENABLE_VALHALLA)) {
		translationFlags |= BCT_ValueTypesEnabled;
	}

	/* Determine allowed class file version */
#ifdef J9VM_OPT_SIDECAR
	if (J2SE_VERSION(vm) >= J2SE_V13) {
		translationFlags |= BCT_Java13MajorVersionShifted;
	} else if (J2SE_VERSION(vm) >= J2SE_V12) {
		translationFlags |= BCT_Java12MajorVersionShifted;
	} else if (J2SE_VERSION(vm) >= J2SE_V11) {
		translationFlags |= BCT_Java11MajorVersionShifted;
	} else if (J2SE_VERSION(vm) >= J2SE_18) {
		translationFlags |= BCT_Java8MajorVersionShifted;
	}
#endif

	/* TODO toss tracepoint?? Trc_BCU_internalLoadROMClass_AttemptExisting(vmThread, segment, romAvailable, bytesRequired); */
	/* Attempt dynamic load */
	result = callDynamicLoader(vmThread, loadData, intermediateClassData, intermediateClassDataLength, translationFlags, classFileBytesReplacedByRIA, classFileBytesReplacedByRCA, localBuffer);

	/* Free the class file bytes if necessary */
doneFreeMem:
	if (NULL != intermediateFreeFunction) {
		if (intermediateClassData == loadData->classData) {
			loadData->freeFunction = NULL;
			loadData->classData = NULL;
		}
		intermediateFreeFunction(intermedtiateFreeUserData, intermediateClassData);
		intermediateClassData = NULL;
	}

	if (loadData->freeFunction) {
		loadData->freeFunction(loadData->freeUserData, loadData->classData);
		loadData->freeFunction = NULL;
		loadData->classData = NULL;
		Trc_BCU_internalLoadROMClass_DoFree(vmThread);
	}

	/* If the ROM class was created successfully, adjust the heapAlloc of the containing segment */

done:
	if (result == BCT_ERR_NO_ERROR) {
		U_8 *intermediateData = J9ROMCLASS_INTERMEDIATECLASSDATA(loadData->romClass);
		U_32 intermediateDataSize = loadData->romClass->intermediateClassDataLength;
		BOOLEAN isSharedClassesEnabled = (NULL != vm->sharedClassConfig);
		BOOLEAN isSharedClassesBCIEnabled = (TRUE == isSharedClassesEnabled) && (TRUE == vm->sharedClassConfig->isBCIEnabled(vm));

		Trc_BCU_internalLoadROMClass_NoError(vmThread, loadData->romClass);

		/* if romClass is in shared cache, ensure its intermediateClassData also points within shared cache */
		if ((NULL != intermediateData)
			&& (TRUE == j9shr_Query_IsAddressInCache(vm, loadData->romClass, loadData->romClass->romSize))
		) {
			Trc_BCU_Assert_True(TRUE == j9shr_Query_IsAddressInCache(vm, intermediateData, intermediateDataSize));
		}

		/* ROMClass should always have intermediate class data. */
		Trc_BCU_Assert_True(NULL != intermediateData);
		Trc_BCU_Assert_True(0 != intermediateDataSize);

		/* If retransformation is not allowed and shared cache is not BCI enabled,
		 * or the class file is not modified by RCA then
		 * romClass->intermediateClassData should point to its own ROMClass.
		 */
		if ((J9_ARE_NO_BITS_SET(vm->requiredDebugAttributes, J9VM_DEBUG_ATTRIBUTE_ALLOW_RETRANSFORM)
			&& ((FALSE == isSharedClassesEnabled) || (FALSE == isSharedClassesBCIEnabled)))
			|| (FALSE == classFileBytesReplacedByRCA)
		) {
			Trc_BCU_Assert_True(intermediateData == (U_8 *)loadData->romClass);
			Trc_BCU_Assert_True(intermediateDataSize == loadData->romClass->romSize);
		}

		/* If shared cache is BCI enabled and class file is modified then the ROMClass should not be in shared cache. */
		if ((TRUE == isSharedClassesBCIEnabled)
			&& ((TRUE == classFileBytesReplacedByRCA)
			|| (TRUE == classFileBytesReplacedByRIA))
		) {
			Trc_BCU_Assert_True(FALSE == j9shr_Query_IsAddressInCache(vm, loadData->romClass, loadData->romClass->romSize));

			/* If class file is modified during load, then intermediate data of the class should not be in shared cache.
			 * But if the class is being re-transformed, then intermediate data may or may not be in shared cache.
			 */
			if (J9_ARE_NO_BITS_SET(loadData->options, J9_FINDCLASS_FLAG_RETRANSFORMING)
				&& (NULL != intermediateData)
			) {
				Trc_BCU_Assert_True(FALSE == j9shr_Query_IsAddressInCache(vm, intermediateData, intermediateDataSize));
			}
		}
	} else {
		loadData->romClass = NULL;
		loadData->romClassSegment = NULL;
	}

#if defined(J9VM_OPT_INVARIANT_INTERNING) && defined(J9VM_OPT_SHARED_CLASSES)
	if (vm->sharedInvariantInternTable != NULL) {
		/* This null check is added because a seg fault occurs otherwise. With the /g (code optimization) option enabled,
		 * the following if-statement never gets executed at run-time as it has an empty body. However, since the /g option
		 * is deprecated at Visual Studio 2010 compiler, the following if-statement is now executed at run-time and causes
		 * a seg fault. This is only a temporary workaround, and if necessary, it will be fixed as part of Fatih's work.
		 */
		if ((vm->sharedInvariantInternTable->flags & J9AVLTREE_DO_VERIFY_TREE_STRUCT_AND_ACCESS) == J9AVLTREE_DO_VERIFY_TREE_STRUCT_AND_ACCESS) {
			/* If taking the string table lock fails in SCStoreTransaction, and SCStringTransaction
			 * the local string intern tree may still have been modified. In this case the call to
			 * avl_intern_verify() from the lock enter/exit functions will not have been run, so
			 * the local tree is checked here when the JVM is run with: -Xshareclasses:verifyInternTree
			 *
			 * For example when "-Xshareclasses:readonly" is used entering the string table lock will fail.
			 */
			/*TODO:FATIH:verify the table
			avl_intern_verify(vm->dynamicLoadBuffers->invariantInternTree,
					vm->dynamicLoadBuffers->invariantInternSharedPool,
					NULL, FALSE);
					*/
		}
	}
#endif

	Trc_BCU_internalLoadROMClass_Exit(vmThread, result);

	return result;
}

static UDATA
callDynamicLoader(J9VMThread *vmThread, J9LoadROMClassData *loadData, U_8 * intermediateClassData, UDATA intermediateClassDataLength, UDATA translationFlags, UDATA classFileBytesReplacedByRIA, UDATA classFileBytesReplacedByRCA, J9TranslationLocalBuffer *localBuffer)
{
	J9JavaVM * vm = vmThread->javaVM;
	BOOLEAN createIntermediateROMClass = FALSE;
	UDATA result = BCT_ERR_NO_ERROR;
	U_8 *intermediateData = NULL;
	UDATA intermediateDataLength = 0;

	/* Pass the verification of stackmaps control flags to be used by static verification (verifyClassFunction - j9bcv_verifyClassStructure) */
	if (vm->bytecodeVerificationData) {
		translationFlags |= (vm->bytecodeVerificationData->verificationFlags & (J9_VERIFY_IGNORE_STACK_MAPS | J9_VERIFY_NO_FALLBACK));
	}

	if (J9_ARE_ANY_BITS_SET(loadData->options, J9_FINDCLASS_FLAG_SHRC_ROMCLASS_EXISTS)) {
		if (TRUE == classFileBytesReplacedByRCA) {
			if (FALSE == classFileBytesReplacedByRIA) {
				/* Intermediate data (either classfile bytes or unmodified J9ROMClass)
				 * is already present in BCI enabled shared cache and
				 * only retransformation capable agent modified the classfile.
				 * In such case existing intermediate data in shared cache can be used
				 * as intermediate data for the new ROMClass.
				 */
				intermediateData = J9ROMCLASS_INTERMEDIATECLASSDATA(loadData->romClass);
				intermediateDataLength = loadData->romClass->intermediateClassDataLength;
			} else {
				createIntermediateROMClass = TRUE;
			}
		}
		/* While creating new ROMClass, existing ROMClass should not be passed to ROMClassCreationContext. */
		loadData->romClass = NULL;
	} else {
		if (TRUE == classFileBytesReplacedByRCA) {
			if (J9_ARE_ANY_BITS_SET(loadData->options, J9_FINDCLASS_FLAG_RETRANSFORMING)) {
				/* class file bytes are modified during retransformation; use intermediate data from class being redefined */
				intermediateData = J9ROMCLASS_INTERMEDIATECLASSDATA(loadData->classBeingRedefined->romClass);
				intermediateDataLength = loadData->classBeingRedefined->romClass->intermediateClassDataLength;
			} else {
				createIntermediateROMClass = TRUE;
			}
		}
	}
	if (TRUE == createIntermediateROMClass) {
		BOOLEAN useClassfileAsIntermediateData = FALSE;

		if (J9_ARE_ALL_BITS_SET(vm->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_FORCE_CLASSFILE_AS_INTERMEDIATE_DATA)) {
			useClassfileAsIntermediateData = TRUE;
		} else {
			/* Create a ROMClass out of intermediateClassData
			 * and store it as intermediate data in active J9ROMClass.
			 */
			J9LoadROMClassData intermediateLoadData;
			IDATA rc;

			memset(&intermediateLoadData, 0, sizeof(J9LoadROMClassData));
			intermediateLoadData.classBeingRedefined = NULL;
			intermediateLoadData.className = loadData->className;
			intermediateLoadData.classNameLength = loadData->classNameLength;
			intermediateLoadData.classData = intermediateClassData;
			intermediateLoadData.classDataLength = intermediateClassDataLength;
			intermediateLoadData.classLoader = loadData->classLoader;
			intermediateLoadData.options = loadData->options;
			/* no need to set other fields in intermediateLoadData */

			rc = j9bcutil_buildRomClass(
				&intermediateLoadData,
				NULL, 0, vm,
				translationFlags,
				FALSE,
				TRUE /* isIntermediateROMClass */,
				localBuffer);

			if (BCT_ERR_NO_ERROR == rc) {
				intermediateData = (U_8 *)intermediateLoadData.romClass;
				intermediateDataLength = intermediateLoadData.romClass->romSize;
			} else {
				/* Failed to create intermediate ROMClass. Use classfile bytes as intermediate data */
				Trc_BCU_callDynamicLoader_IntermediateROMClassCreationFailed(loadData, rc);
				useClassfileAsIntermediateData = TRUE;
			}
		}
		if (TRUE == useClassfileAsIntermediateData) {
			intermediateData = intermediateClassData;
			intermediateDataLength = intermediateClassDataLength;
			translationFlags |= BCT_IntermediateDataIsClassfile;
		}
	}

	result = j9bcutil_buildRomClass(
			loadData,
			(U_8 *) intermediateData,
			intermediateDataLength,
			vm,
			translationFlags,
			classFileBytesReplacedByRIA | classFileBytesReplacedByRCA,
			FALSE, /* isIntermediateROMClass */
			localBuffer);

	/* The module of a class transformed by a JVMTI agent needs access to unnamed modules */
	if ((J2SE_VERSION(vm) >= J2SE_V11)
		&& (classFileBytesReplacedByRIA || classFileBytesReplacedByRCA)
		&& (NULL != loadData->romClass)
	) {
		J9Module *module = J9_VM_FUNCTION(vmThread, findModuleForPackage)(vmThread, loadData->classLoader,
				loadData->className, (U_32) packageNameLength(loadData->romClass));
		if (NULL != module) {
			module->isLoose = TRUE;
		}
	}

	if (J9_ARE_ANY_BITS_SET(vm->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_RECREATE_CLASSFILE_ONLOAD)) {
		if (BCT_ERR_NO_ERROR == result) {
			U_8 * classFileBytes = NULL;
			U_32 classFileBytesCount = 0;
			U_8 * prevClassData = loadData->classData;
			PORT_ACCESS_FROM_JAVAVM(vm);

			/* Use the ROMClass to recreate classfile bytes */
			result = j9bcutil_transformROMClass(vm, PORTLIB, loadData->romClass, &classFileBytes, &classFileBytesCount);
			if (BCT_ERR_NO_ERROR == result) {
				loadData->classData = classFileBytes;
				loadData->classDataLength = classFileBytesCount;
				loadData->romClass = NULL;

				result = j9bcutil_buildRomClass(
						loadData,
						intermediateData,
						intermediateDataLength,
						vm,
						translationFlags,
						classFileBytesReplacedByRIA | classFileBytesReplacedByRCA,
						FALSE, /* isIntermediateROMClass */
						localBuffer);

				j9mem_free_memory(classFileBytes);
			}
			/* Restore classData */
			loadData->classData = prevClassData;
		}
	}
	return result;
}

static J9ROMClass *
createROMClassFromClassFile(J9VMThread *currentThread, J9LoadROMClassData *loadData, J9TranslationLocalBuffer *localBuffer)
{
	J9JavaVM * vm = currentThread->javaVM;
	UDATA result = 0;
	U_8 * errorUTF = NULL;
	UDATA exceptionNumber = J9VMCONSTANTPOOL_JAVALANGCLASSFORMATERROR;
	void * className = NULL;
	UDATA classNameLength = 0;
	J9ClassLoader * classLoader = NULL;

	/* Attempt to create the romClass */

	Trc_BCU_createROMClassFromClassFile_Entry(currentThread, loadData);

	result = internalLoadROMClass(currentThread, loadData, localBuffer);
	className = loadData->className;
	classNameLength = loadData->classNameLength;
	classLoader = loadData->classLoader;

	/* If the romClass was successfully created, continue processing it */

	if (result == BCT_ERR_NO_ERROR) {
		J9ROMClass * romClass = loadData->romClass;
		J9UTF8 * romName = J9ROMCLASS_CLASSNAME(romClass);

		/* If a class name was specified, verify that the loaded class has the same name */

		Trc_BCU_createROMClassFromClassFile_postLoadNoErr(currentThread, J9UTF8_LENGTH(romName), J9UTF8_DATA(romName), classLoader, romClass, NULL); /* TODO update trace point*/

		Trc_BCU_createROMClassFromClassFile_Exit(currentThread, romClass);
		return romClass;
	}

	/* Always throw load errors */

	Trc_BCU_Assert_True(NULL != vm->dynamicLoadBuffers);

	switch (result) {
		case BCT_ERR_CLASS_READ: {
			J9CfrError * cfrError = (J9CfrError *) vm->dynamicLoadBuffers->classFileError;

			errorUTF = (U_8 *) buildVerifyErrorString(vm, cfrError, className, classNameLength);
			exceptionNumber = cfrError->errorAction;
			/* We don't free vm->dynamicLoadBuffers->classFileError because it is also used as a classFileBuffer in ROMClassBuilder */
			break;
		}

		case BCT_ERR_INVALID_BYTECODE:
		case BCT_ERR_STACK_MAP_FAILED:
		case BCT_ERR_VERIFY_ERROR_INLINING:
		case BCT_ERR_BYTECODE_TRANSLATION_FAILED:
		case BCT_ERR_UNKNOWN_ANNOTATION:
			exceptionNumber = J9VMCONSTANTPOOL_JAVALANGVERIFYERROR;
			break;
			
		case BCT_ERR_INVALID_ANNOTATION:
			errorUTF = vm->dynamicLoadBuffers->classFileError;
			exceptionNumber = J9VMCONSTANTPOOL_JAVALANGVERIFYERROR;
			break;

		case BCT_ERR_ILLEGAL_PACKAGE_NAME:
			exceptionNumber = J9VMCONSTANTPOOL_JAVALANGSECURITYEXCEPTION;
			break;

		case BCT_ERR_OUT_OF_ROM:
		case BCT_ERR_OUT_OF_MEMORY:
			exceptionNumber = J9VMCONSTANTPOOL_JAVALANGOUTOFMEMORYERROR;
			break;

		case BCT_ERR_CLASS_NAME_MISMATCH:
			exceptionNumber = J9VMCONSTANTPOOL_JAVALANGNOCLASSDEFFOUNDERROR;
			/* FALLTHROUGH */
			
		default:
			/* default value for exceptionNumber (J9VMCONSTANTPOOL_JAVALANGCLASSFORMATERROR) assigned before switch */
			errorUTF = vm->dynamicLoadBuffers->classFileError;
			if (NULL == errorUTF) {
				PORT_ACCESS_FROM_JAVAVM(vm);
				errorUTF = j9mem_allocate_memory(loadData->classNameLength + 1, J9MEM_CATEGORY_CLASSES);
				if (NULL != errorUTF) {
					memcpy(errorUTF, loadData->className, loadData->classNameLength);
					errorUTF[loadData->classNameLength] = (U_8) '\0';
				}
			}
			break;
	}

	Trc_BCU_Assert_True((NULL == vm->dynamicLoadBuffers->classFileError) || (NULL != errorUTF));
	vm->dynamicLoadBuffers->classFileError = NULL;

#ifdef J9VM_THR_PREEMPTIVE
	omrthread_monitor_exit(vm->classTableMutex);
#endif

	Trc_BCU_createROMClassFromClassFile_throwError(currentThread, exceptionNumber);

	/* Do not throw OutOfMemoryError here, instead set the private flags bit */

	if (exceptionNumber == J9VMCONSTANTPOOL_JAVALANGOUTOFMEMORYERROR) {
		currentThread->privateFlags |= J9_PRIVATE_FLAGS_CLOAD_NO_MEM;
		/*Trc_BCU_internalLoadROMClass_NoMemory(vmThread);*/
	} else {
		if (errorUTF == NULL) {
			J9_VM_FUNCTION(currentThread, setCurrentException)(currentThread, exceptionNumber, NULL);
		} else {
			PORT_ACCESS_FROM_JAVAVM(vm);
			J9_VM_FUNCTION(currentThread, setCurrentExceptionUTF)(currentThread, exceptionNumber, (const char*)errorUTF);
			j9mem_free_memory(errorUTF);
		}
	}

	Trc_BCU_createROMClassFromClassFile_Exit(currentThread, NULL);
	return NULL;
}

static UDATA 
classCouldPossiblyBeShared(J9VMThread * vmThread, J9LoadROMClassData * loadData)
{
	J9JavaVM * vm = vmThread->javaVM;
	return ((0 != (loadData->classLoader->flags & J9CLASSLOADER_SHARED_CLASSES_ENABLED)) && !j9shr_Query_IsCacheFull(vm));
}

/* Return TRUE if anonClass and hostClass have the same package name.
 * If anonymous class has no package name, then consider it to be part
 * of host class's package. Return TRUE if anonymous class has no
 * package name. Otherwise, return FALSE.
 */
static BOOLEAN
hasSamePackageName(J9ROMClass *anonROMClass, J9ROMClass *hostROMClass) {
	BOOLEAN rc = FALSE;
	const UDATA anonClassPackageNameLength = packageNameLength(anonROMClass);

	if (0 == anonClassPackageNameLength) {
		rc = TRUE;
	} else {
		const U_8 *anonClassName = J9UTF8_DATA(J9ROMCLASS_CLASSNAME(anonROMClass));
		const U_8 *hostClassName = J9UTF8_DATA(J9ROMCLASS_CLASSNAME(hostROMClass));
		const UDATA hostClassPackageNameLength = packageNameLength(hostROMClass);
		if (J9UTF8_DATA_EQUALS(anonClassName, anonClassPackageNameLength, hostClassName, hostClassPackageNameLength)) {
			rc = TRUE;
		}
	}

	return rc;
}

/* Create error message with host class and anonymous class. */
static char*
createErrorMessage(J9VMThread *vmStruct, J9ROMClass *anonROMClass, J9ROMClass *hostROMClass, const char* errorMsg) {
	PORT_ACCESS_FROM_VMC(vmStruct);
	char *buf = NULL;

	if (NULL != errorMsg) {
		UDATA bufLen = 0;
		const J9UTF8 *anonClassName = J9ROMCLASS_CLASSNAME(anonROMClass);
		const J9UTF8 *hostClassName = J9ROMCLASS_CLASSNAME(hostROMClass);
		const U_8 *hostClassNameData = J9UTF8_DATA(hostClassName);
		const U_8 *anonClassNameData = J9UTF8_DATA(anonClassName);
		const UDATA hostClassNameLength = J9UTF8_LENGTH(hostClassName);

		/* Anonymous class name has trailing digits. Example - "test/DummyClass/00000000442F098".
		 * The code below removes the trailing digits, "/00000000442F098", from the anonymous class name.
		 */
		IDATA anonClassNameLength = J9UTF8_LENGTH(anonClassName) - 1;
		for (; anonClassNameLength >= 0; anonClassNameLength--) {
			if (anonClassNameData[anonClassNameLength] == '/') {
				break;
			}
		}

		bufLen = j9str_printf(PORTLIB, NULL, 0, errorMsg,
						hostClassNameLength, hostClassNameData,
						anonClassNameLength, anonClassNameData);
		if (bufLen > 0) {
			buf = j9mem_allocate_memory(bufLen, OMRMEM_CATEGORY_VM);
			if (NULL != buf) {
				j9str_printf(PORTLIB, buf, bufLen, errorMsg,
						hostClassNameLength, hostClassNameData,
						anonClassNameLength, anonClassNameData);
			}
		}
	}

	return buf;
}

/* From Java 9 and onwards, set IllegalArgumentException when host class and anonymous class have different packages. */
static void
setIllegalArgumentExceptionHostClassAnonClassHaveDifferentPackages(J9VMThread *vmStruct, J9ROMClass *anonROMClass, J9ROMClass *hostROMClass) {
	PORT_ACCESS_FROM_VMC(vmStruct);
	const J9JavaVM *vm = vmStruct->javaVM;

	/* Construct error string */
	const char *errorMsg = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_JCL_HOSTCLASS_ANONCLASS_DIFFERENT_PACKAGES, NULL);
	char *buf = createErrorMessage(vmStruct, anonROMClass, hostROMClass, errorMsg);
	J9_VM_FUNCTION(vmStruct, setCurrentExceptionUTF)(vmStruct, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, buf);
	j9mem_free_memory(buf);
}

/* Free the memory segment corresponding to the anonymous ROM class. */
static void
freeAnonROMClass(J9JavaVM *vm, J9ROMClass *romClass) {
	if (NULL != romClass) {
		omrthread_monitor_t segmentMutex = vm->classMemorySegments->segmentMutex;
		omrthread_monitor_enter(segmentMutex);
		{
			J9MemorySegment **previousSegmentPointerROM = &vm->anonClassLoader->classSegments;
			J9MemorySegment *segmentROM = *previousSegmentPointerROM;
			BOOLEAN foundMemorySegment = FALSE;

			/* Walk all anonymous classloader's ROM memory segments. If ROM class
			 * is allocated there it would be one per segment.
			 */
			while (NULL != segmentROM) {
				J9MemorySegment *nextSegmentROM = segmentROM->nextSegmentInClassLoader;
				if (J9_ARE_ALL_BITS_SET(segmentROM->type, MEMORY_TYPE_ROM_CLASS)
					&& ((J9ROMClass *)segmentROM->heapBase == romClass)
				) {
					foundMemorySegment = TRUE;
					/* Found memory segment corresponding to the ROM class. Remove
					 * this memory segment from the list.
					 */
					*previousSegmentPointerROM = nextSegmentROM;
					/* Free memory segment corresponding to the ROM class. */
					J9_VM_FUNCTION_VIA_JAVAVM(vm, freeMemorySegment)(vm, segmentROM, 1);
					break;
				}
				previousSegmentPointerROM = &segmentROM->nextSegmentInClassLoader;
				segmentROM = nextSegmentROM;
			}
			/* Memory segment should always be found if the ROM class exists. */
			Trc_BCU_Assert_True(foundMemorySegment);
		}
		omrthread_monitor_exit(segmentMutex);
	}
}

#endif /* J9VM_OPT_DYNAMIC_LOAD_SUPPORT */ /* End File Level Build Flags */
