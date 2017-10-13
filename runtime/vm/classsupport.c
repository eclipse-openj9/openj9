/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#include "j9.h"
#include "j9cfg.h"
#include "j9protos.h"
#include "j9consts.h"
#include "j2sever.h"
#include "vm_internal.h"
#include "j9cp.h"
#include "ut_j9vm.h"
#include "objhelp.h"
#include "j9vmnls.h"
#include <string.h>
#include "pcstack.h"
#include "rommeth.h"
#include "bcnames.h"

static UDATA classAndLoaderHashFn (void *key, void *userData);
static UDATA classAndLoaderHashEqualFn (void *leftKey, void *rightKey, void *userData);
static J9ContendedLoadTableEntry * contendedLoadTableAddThread (J9VMThread* vmThread, J9ClassLoader* classLoader, U_8* className,
		UDATA classNameLength, UDATA status);
static J9ContendedLoadTableEntry*
contendedLoadTableGet(J9JavaVM* vm, J9ClassLoader* classLoader, U_8* className, UDATA classNameLength);
static UDATA
contendedLoadTableRemoveThread(J9VMThread* vmThread, J9ContendedLoadTableEntry *tableEntry, UDATA status);
static J9Class* findPrimitiveArrayClass (J9JavaVM* vm, jchar sigChar);
static VMINLINE J9Class* arbitratedLoadClass(J9VMThread* vmThread, U_8* className, UDATA classNameLength,
		J9ClassLoader* classLoader, j9object_t * classNotFoundException);
static J9Class* internalFindArrayClass(J9VMThread* vmThread, J9Module *j9module, UDATA arity, U_8* name, UDATA length, J9ClassLoader* classLoader, UDATA options);

extern J9Method initialStaticMethod;
extern J9Method initialSpecialMethod;
#if !defined(J9VM_INTERP_USE_SPLIT_SIDE_TABLES)
extern J9Method initialSharedMethod;
#endif /* defined(J9VM_INTERP_USE_SPLIT_SIDE_TABLES) */

UDATA
totalStaticSlotsForClass( J9ROMClass *romClass ) {
	UDATA totalSlots;

	totalSlots = romClass->objectStaticCount + romClass->singleScalarStaticCount;
#ifndef J9VM_ENV_DATA64
	totalSlots = (totalSlots + 1) & ~(UDATA)1;
#endif

	totalSlots += romClass->doubleScalarStaticCount;
#ifndef J9VM_ENV_DATA64
	totalSlots += romClass->doubleScalarStaticCount;
#endif

	return totalSlots;
}


/**
 * @internal
 *
 * This function should only be called by internalFindClassUTF8()!
 *
 * Load the array class with the specified name.
 * If nameData does not refer to a valid array, return NULL.
 * If the array class cannot be loaded, return NULL and set the current exception.
 *
 * The caller must have VMAccess.
 * This function may release VMAccess.
 * This function may push objects on the stack. An appropriate stack frame must already be in place.
 */
static J9Class*
internalFindArrayClass(J9VMThread* vmThread, J9Module *j9module, UDATA arity, U_8* name, UDATA length, J9ClassLoader* classLoader, UDATA options)
{
	J9JavaVM* vm = vmThread->javaVM;
	jchar firstChar = 0;
	jchar lastChar = 0;
	J9Class* arrayClass = NULL;		/* class of array element */

	vmThread->privateFlags &= ~J9_PRIVATE_FLAGS_CLOAD_NO_MEM;

	if (length > arity) {
		firstChar = name[arity];
		lastChar = name[length-1];
	}

	if (length - arity == 1) {

		arrayClass = findPrimitiveArrayClass(vm, firstChar);

		/* the first level of arity is already present in the array class */
		arity -= 1;

	} else if (firstChar == 'L' && lastChar == ';') {

		name += arity + 1; /* 1 for 'L' */
		length -= arity + 2; /* 2 for 'L and ';' */

		arrayClass = internalFindClassInModule(vmThread, j9module, name, length, classLoader, options);

	} else {
		return NULL;
	}

	while (arrayClass && arity-- > 0) {
		if (arrayClass->arrayClass) {
			arrayClass = arrayClass->arrayClass;
		} else {
			if (options & J9_FINDCLASS_FLAG_EXISTING_ONLY) {
				arrayClass = NULL;
			} else {
				J9ROMArrayClass* arrayOfObjectsROMClass = (J9ROMArrayClass*)J9ROMIMAGEHEADER_FIRSTCLASS(vm->arrayROMClasses);

				arrayClass = internalCreateArrayClass(vmThread, arrayOfObjectsROMClass, arrayClass);
			}
		}
	}

	return arrayClass;
}


/**
 * @internal
 *
 * Calculate the arity of the array described by nameData.
 *
 * @return 0 if the name is not an array, or the number of '['s if it is
 *
 * @note The caller must have VMAccess.
 */
UDATA
calculateArity(J9VMThread* vmThread, U_8* name, UDATA length)
{
	U_32 arity = 0;

	while (length > 0 && *name == '[') {
		name += 1;
		length -= 1;
		arity += 1;
	}

	return arity;
}


static J9Class*
findPrimitiveArrayClass(J9JavaVM* vm, jchar sigChar)
{

	switch (sigChar) {
	case 'B':
		return vm->byteArrayClass;
	case 'C':
		return vm->charArrayClass;
		break;
	case 'I':
		return vm->intArrayClass;
		break;
	case 'J':
		return vm->longArrayClass;
		break;
	case 'S':
		return vm->shortArrayClass;
		break;
	case 'Z':
		return vm->booleanArrayClass;
		break;
#ifdef J9VM_INTERP_FLOAT_SUPPORT
	case 'D':
		return vm->doubleArrayClass;
		break;
	case 'F':
		return vm->floatArrayClass;
		break;
#endif
	default:
		return NULL;
	}
}


/**
 * @internal
 *
 * Allocates and fills in the relevant fields of an array class
 */
J9Class* 
internalCreateArrayClass(J9VMThread* vmThread, J9ROMArrayClass* romClass, J9Class* elementClass)
{
	J9Class *result;
	j9object_t heapClass = J9VM_J9CLASS_TO_HEAPCLASS(elementClass);
	j9object_t protectionDomain = NULL;
	J9ROMClass* arrayRomClass = (J9ROMClass*) romClass;
	J9JavaVM *const javaVM = vmThread->javaVM;
	UDATA options = 0;

	if (J9_ARE_ANY_BITS_SET(elementClass->classFlags, J9ClassIsAnonymous)) {
		options = J9_FINDCLASS_FLAG_ANON;
	}

	omrthread_monitor_enter(javaVM->classTableMutex);

	if (NULL != heapClass) {
		protectionDomain = J9VMJAVALANGCLASS_PROTECTIONDOMAIN(vmThread, heapClass);
	}

	result = internalCreateRAMClassFromROMClass(
		vmThread,
		elementClass->classLoader,
		arrayRomClass,
		options, /* options */
		elementClass,
		protectionDomain,
		NULL,
		J9_CP_INDEX_NONE,
		LOAD_LOCATION_UNKNOWN,
		NULL,
		NULL);
	return result;
}


J9Class*  
internalFindClassString(J9VMThread* currentThread, j9object_t moduleName, j9object_t className, J9ClassLoader* classLoader, UDATA options)
{
	J9Class *result = NULL;
	J9JavaVM* vm = currentThread->javaVM;
	BOOLEAN fastMode = J9_ARE_ALL_BITS_SET(vm->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_FAST_CLASS_HASH_TABLE);

	/* If -XX:+FastClassHashTable is enabled, do not lock anything to do the initial table peek */
	if (!fastMode) {
		omrthread_monitor_enter(vm->classTableMutex);
	}
	result = hashClassTableAtString(classLoader, (j9object_t) className);
	if (!fastMode) {
		omrthread_monitor_exit(vm->classTableMutex);
	}

	if (NULL == result) {
		J9Module **findResult = NULL;
		J9Module *j9module = NULL;
		char localBuf[256];
		char *utf8Name = NULL;
		UDATA utf8Length = 0;
		PORT_ACCESS_FROM_JAVAVM(vm);

		if (NULL != moduleName) {
			J9Module module = {0};
			J9Module *modulePtr = &module;

			modulePtr->moduleName = moduleName;
			findResult = hashTableFind(classLoader->moduleHashTable, &modulePtr);
			if (NULL != findResult) {
				j9module = *findResult;
			}
		}

		utf8Name = copyStringToUTF8WithMemAlloc(currentThread, className, J9_STR_XLAT, "", localBuf, sizeof(localBuf));
		if (NULL == utf8Name) {
			/* Throw out-of-memory */
			setNativeOutOfMemoryError(currentThread, 0, 0);
			return NULL;
		}
		utf8Length = (UDATA)getStringUTF8Length(currentThread, className);
		result = internalFindClassInModule(currentThread, j9module, (U_8 *)utf8Name, utf8Length, classLoader, options);
		if (utf8Name != localBuf) {
			j9mem_free_memory(utf8Name);
		}
	}
	return result;
}


/* Assumes that the constant pool has been zeroed */
void
internalRunPreInitInstructions(J9Class * ramClass, J9VMThread * vmThread)
{
	J9JavaVM * vm = vmThread->javaVM;
	J9ROMClass * romClass = ramClass->romClass;
	UDATA methodTypeIndex = 0;
	UDATA ramConstantPoolCount = romClass->ramConstantPoolCount;


	if (ramConstantPoolCount != 0) {
		J9ConstantPool * ramConstantPool = J9_CP_FROM_CLASS(ramClass);
		J9ROMConstantPoolItem * romConstantPool = ramConstantPool->romConstantPool;
		U_32 * cpShapeDescription = J9ROMCLASS_CPSHAPEDESCRIPTION(romClass);
		J9UTF8 *className = J9ROMCLASS_CLASSNAME(romClass);
		UDATA descriptionCount = 0;
		U_32 description = 0;
		UDATA i;
		
		BOOLEAN isAnonClass = J9_ARE_ANY_BITS_SET(romClass->extraModifiers, J9AccClassAnonClass);

		for (i = 0; i < ramConstantPoolCount; ++i) {
			if (descriptionCount == 0) {
				description = *cpShapeDescription++;
				descriptionCount = J9_CP_DESCRIPTIONS_PER_U32;
			}

			switch(description & J9_CP_DESCRIPTION_MASK) {
				J9ROMMethodRef * romMethodRef;
				J9ROMNameAndSignature * nas;

				case J9CPTYPE_CLASS:
					if (isAnonClass) {
						/* anonClass RAM constant pool fix up */
						J9ROMClassRef* classEntry = ((J9ROMClassRef*) &(romConstantPool[i]));
						/* pointer comparison is done here because only one constant pool entry contains the className UTF8 */
						if (SRP_GET(classEntry->name, J9UTF8*) == className) {
							/* fill in classRef in RAM constantPool */
							((J9RAMClassRef *) ramConstantPool)[i].value = ramClass;
						}
					}
					break;
				case J9CPTYPE_INT:
				case J9CPTYPE_FLOAT:
					*((U_32 *) &(((J9RAMSingleSlotConstantRef *) ramConstantPool)[i].value)) = ((J9ROMSingleSlotConstantRef *) romConstantPool)[i].data;
					break;

				case J9CPTYPE_FIELD:
					((J9RAMFieldRef *) ramConstantPool)[i].valueOffset = -1;
					break;

#if !defined(J9VM_INTERP_USE_SPLIT_SIDE_TABLES)
				case J9CPTYPE_SHARED_METHOD: {
					J9UTF8 *nameUTF, *className;
					J9ROMClassRef *romClassRef;
					UDATA methodIndex = sizeof(J9Class) + sizeof(UDATA);
					romMethodRef = ((J9ROMMethodRef *) romConstantPool) + i;
					romClassRef = ((J9ROMClassRef *) romConstantPool) + romMethodRef->classRefCPIndex;
					className = J9ROMCLASSREF_NAME(romClassRef);
					nas = J9ROMMETHODREF_NAMEANDSIGNATURE(romMethodRef);
					nameUTF = J9ROMNAMEANDSIGNATURE_NAME(nas);
					if ((J9UTF8_LITERAL_EQUALS(J9UTF8_DATA(className), J9UTF8_LENGTH(className), "java/lang/invoke/MethodHandle"))
					&& (J9UTF8_LITERAL_EQUALS(J9UTF8_DATA(nameUTF), J9UTF8_LENGTH(nameUTF), "invokeExact")
					|| J9UTF8_LITERAL_EQUALS(J9UTF8_DATA(nameUTF), J9UTF8_LENGTH(nameUTF), "invoke"))) {
						methodIndex = methodTypeIndex++;
					}
					((J9RAMMethodRef *) ramConstantPool)[i].methodIndexAndArgCount = (methodIndex << 8) +
						getSendSlotsFromSignature(J9UTF8_DATA(J9ROMNAMEANDSIGNATURE_SIGNATURE(nas)));
					((J9RAMMethodRef *) ramConstantPool)[i].method = vm->initialMethods.initialSharedMethod;
					break;
				}
#endif /* !defined(J9VM_INTERP_USE_SPLIT_SIDE_TABLES) */
				case J9CPTYPE_HANDLE_METHOD:
					romMethodRef = ((J9ROMMethodRef *) romConstantPool) + i;
					nas = J9ROMMETHODREF_NAMEANDSIGNATURE(romMethodRef);
					((J9RAMMethodRef *) ramConstantPool)[i].methodIndexAndArgCount = (methodTypeIndex << 8) +
						getSendSlotsFromSignature(J9UTF8_DATA(J9ROMNAMEANDSIGNATURE_SIGNATURE(nas)));
					methodTypeIndex++;
#if defined(J9VM_INTERP_USE_SPLIT_SIDE_TABLES)
					/* In case MethodHandle.invoke[Exact] is called via invokestatic */
					((J9RAMMethodRef *) ramConstantPool)[i].method = vm->initialMethods.initialStaticMethod;
#endif /* defined(J9VM_INTERP_USE_SPLIT_SIDE_TABLES) */
					break;

				case J9CPTYPE_INSTANCE_METHOD:
					romMethodRef = ((J9ROMMethodRef *) romConstantPool) + i;
					nas = J9ROMMETHODREF_NAMEANDSIGNATURE(romMethodRef);
					((J9RAMMethodRef *) ramConstantPool)[i].methodIndexAndArgCount = ((sizeof(J9Class) + sizeof(UDATA)) << 8) +
						getSendSlotsFromSignature(J9UTF8_DATA(J9ROMNAMEANDSIGNATURE_SIGNATURE(nas)));
					((J9RAMMethodRef *) ramConstantPool)[i].method = vm->initialMethods.initialSpecialMethod;
					break;

				case J9CPTYPE_STATIC_METHOD:
#if defined(J9VM_INTERP_USE_SPLIT_SIDE_TABLES)
					romMethodRef = ((J9ROMMethodRef *) romConstantPool) + i;
					nas = J9ROMMETHODREF_NAMEANDSIGNATURE(romMethodRef);
					/* In case this CP entry is shared with invokevirtual */
					((J9RAMMethodRef *) ramConstantPool)[i].methodIndexAndArgCount = ((sizeof(J9Class) + sizeof(UDATA)) << 8) +
						getSendSlotsFromSignature(J9UTF8_DATA(J9ROMNAMEANDSIGNATURE_SIGNATURE(nas)));
#endif /* defined(J9VM_INTERP_USE_SPLIT_SIDE_TABLES) */
					((J9RAMStaticMethodRef *) ramConstantPool)[i].method = vm->initialMethods.initialStaticMethod;
					break;

				case J9CPTYPE_INTERFACE_METHOD:
					romMethodRef = ((J9ROMMethodRef *) romConstantPool) + i;
					nas = J9ROMMETHODREF_NAMEANDSIGNATURE(romMethodRef);
					((J9RAMInterfaceMethodRef *) ramConstantPool)[i].methodIndexAndArgCount = getSendSlotsFromSignature(J9UTF8_DATA(J9ROMNAMEANDSIGNATURE_SIGNATURE(nas)));
					break;

				case J9CPTYPE_METHOD_TYPE:
				{
					J9ROMMethodTypeRef *romMethodTypeRef = ((J9ROMMethodTypeRef *) romConstantPool) + i;
					J9UTF8 *signature = J9ROMMETHODTYPEREF_SIGNATURE(romMethodTypeRef);

					((J9RAMMethodTypeRef *) ramConstantPool)[i].type = 0;
					((J9RAMMethodTypeRef *) ramConstantPool)[i].slotCount = getSendSlotsFromSignature(J9UTF8_DATA(signature));
					break;
				}
			}

			description >>= J9_CP_BITS_PER_DESCRIPTION;
			--descriptionCount;
		}
	}
}



J9Class*  
resolveKnownClass(J9JavaVM * vm, UDATA index)
{
	J9VMThread * currentThread = currentVMThread(vm);
	J9ConstantPool * jclConstantPool = (J9ConstantPool *) vm->jclConstantPool;
	J9RAMFieldRef * ramConstantPool = (J9RAMFieldRef *) vm->jclConstantPool;
	J9ROMFieldRef * romConstantPool = (J9ROMFieldRef *) jclConstantPool->romConstantPool;
	J9ROMClass * jclROMClass = jclConstantPool->ramClass->romClass;
	U_32 * cpShapeDescription = J9ROMCLASS_CPSHAPEDESCRIPTION(jclROMClass);
	U_32 constPoolCount = jclROMClass->romConstantPoolCount;

	U_32 i;
	J9Class * clazz;

	Trc_VM_resolveKnownClass_Entry(currentThread, index);

	clazz = resolveClassRef(currentThread, jclConstantPool, index, J9_RESOLVE_FLAG_RUNTIME_RESOLVE);
	if (NULL != clazz) {

		/* Forcibly resolve all instance field refs in the JCL constant pool that belong to the resolved class. */
		for (i = 0; i < constPoolCount; i++) {
			if ((J9CPTYPE_FIELD == J9_CP_TYPE(cpShapeDescription, i))
			&& (index == romConstantPool[i].classRefCPIndex)) {
				if (-1 == resolveInstanceFieldRef(currentThread, NULL, jclConstantPool, i, J9_RESOLVE_FLAG_RUNTIME_RESOLVE | J9_RESOLVE_FLAG_NO_THROW_ON_FAIL, NULL)) {
					Trc_VM_resolveKnownClass_resolveInstanceFieldRef_Failed(currentThread, i, index, clazz);
				} else
				Trc_VM_resolveKnownClass_resolvedInstanceFieldRef(currentThread, i, index, clazz, ramConstantPool[i].valueOffset);
			}
		}
	}

	Trc_VM_resolveKnownClass_Exit(currentThread, clazz);

	return clazz;
}

/**
 * create RAM class from ROM class
 * @param vmThread Current VM thread
 * @param classLoader Class loader object
 * @param options
 * @param romClass pointer to romClass struct for the class
 * @param errorRomClassP pointer to the pointer to the error rom class struct
 * @param entryIndex class path or patch path index
 * @return RAM class, or NULL if ROM class is errored
 *
 * Precondition: class table mutex locked
 * Postcondition: class table mutex unlocked (NB)
 */

 J9Class *foundROMClass(J9VMThread* vmThread, J9ClassLoader* classLoader, UDATA options, J9ROMClass *romClass, J9ROMClass **errorRomClassP, UDATA entryIndex, I_32 locationType) {
	J9Class *foundClass = NULL;

	/* check to see if this is an errored ROM class */
	*errorRomClassP = NULL;
	if (NULL == checkRomClassForError(romClass, vmThread)) {
		*errorRomClassP = romClass;
		omrthread_monitor_exit(vmThread->javaVM->classTableMutex);
	} else {
		/* this function unlocks the class table mutex */
		foundClass = internalCreateRAMClassFromROMClass(vmThread,
			classLoader, romClass, options, NULL, NULL, NULL, entryIndex, locationType, NULL, NULL);
	}
	return foundClass;
 }

/**
 * Give the hook interface a go at loading the class and call the thread's findLocallyDefinedClass
 * @param vmThread Current VM thread
 * @param j9module Pointer to J9Module in which the class is to be searched
 * @param className Name of the class to load
 * @param classNameLength Length of the class name
 * @param classLoader Class loader object
 * @param options
 * @param flags contains 0 or BOOTSTRAP_ENTRIES_ONLY
 * @param [in/out] localBuffer contains values for entryIndex, loadLocationType and cpEntryUsed. This pointer can't be NULL.
 * @return pointer to class if locally defined, 0 if not defined, -1 if we cannot do dynamic load
 *
 * Preconditions: none
 * Postconditions: class segment mutex locked if dynamicLoadBuffers is null (this is likely a bug)
 *
 */

static IDATA
callFindLocallyDefinedClass(J9VMThread* vmThread, J9Module *j9module, U_8* className, UDATA classNameLength, J9ClassLoader* classLoader, UDATA options, UDATA flags, J9TranslationLocalBuffer *localBuffer) {
	IDATA findResult = -1;
	J9TranslationBufferSet *dynamicLoadBuffers = vmThread->javaVM->dynamicLoadBuffers;
	J9ROMClass* returnVal = NULL;
	IDATA classFound = 0;
	IDATA* returnPointer = (IDATA*) &returnVal;

	/* localBuffer should not be NULL */
	Assert_VM_true(NULL != localBuffer);

	omrthread_monitor_enter(vmThread->javaVM->classMemorySegments->segmentMutex);
	if (NULL != dynamicLoadBuffers) {
		 J9ClassPathEntry* classPathEntries = NULL;
		 if (classLoader == vmThread->javaVM->systemClassLoader) {
			 classPathEntries = classLoader->classPathEntries;
		 }


		 TRIGGER_J9HOOK_VM_FIND_LOCALLY_DEFINED_CLASS(vmThread->javaVM->hookInterface, vmThread, classLoader, j9module, (char*)className, classNameLength,
						classPathEntries, classLoader->classPathEntryCount, -1, NULL, 0, 0,
						(IDATA *) &localBuffer->entryIndex, returnVal);

		findResult = (IDATA) returnVal;

 		omrthread_monitor_exit(vmThread->javaVM->classMemorySegments->segmentMutex);
		if (0 == findResult) {
			TRIGGER_J9HOOK_VM_FIND_LOCALLY_DEFINED_CLASS_FROM_FILESYSTEM(vmThread->javaVM->hookInterface, 
																		 vmThread, 
																		 (const char *)className, 
																		 classNameLength,
																		 classLoader, 
																		 flags,
																		 &classFound,
																		 returnPointer);
			if (returnVal != NULL){
				findResult = classFound;
			} else {
				findResult = dynamicLoadBuffers->findLocallyDefinedClassFunction(vmThread, j9module, className, (U_32)classNameLength, classLoader, classPathEntries,
						classLoader->classPathEntryCount, options, flags, localBuffer);
				if ((-1 == findResult) && (J9_PRIVATE_FLAGS_REPORT_ERROR_LOADING_CLASS & vmThread->privateFlags)) {
					vmThread->privateFlags |= J9_PRIVATE_FLAGS_FAILED_LOADING_REQUIRED_CLASS;
				}
			}
		} else {
			/* The class is found in shared class cache. */
			if (J2SE_VERSION(vmThread->javaVM) >= J2SE_19) {
				if (localBuffer->entryIndex >= 0) {
					localBuffer->loadLocationType = LOAD_LOCATION_CLASSPATH;
				} else {
					localBuffer->loadLocationType = LOAD_LOCATION_MODULE;
				}
			} else {
				localBuffer->loadLocationType = LOAD_LOCATION_CLASSPATH;
			}
		}
	} else {
 		omrthread_monitor_exit(vmThread->javaVM->classMemorySegments->segmentMutex);
	}
	return findResult;
}

/**
 * Attempt to perform dynamic load
 * @param vmThread Current VM thread
 * @param j9module Pointer to J9Module in which the class is to be searched
 * @param className Name of the class to load
 * @param classNameLength Length of the class name
 * @param classLoader Class loader object
 * @param options Options for defineClass
 * @return loaded class object if success, NULL if fail
 *
 * Precondition: class table mutex locked
 * Postcondition: class table mutex unlocked (NB)
 */

static J9Class*  
attemptDynamicClassLoad(J9VMThread* vmThread, J9Module *j9module, U_8* className, UDATA classNameLength, J9ClassLoader* classLoader, UDATA options)
{
	J9Class *foundClass = NULL;

	Trc_VM_internalFindClass_attemptDynamicClassLoad_entry(vmThread, classLoader->classLoaderObject, classNameLength, className);

	/* try to load classes from system class loader */
	if ((J2SE_VERSION(vmThread->javaVM) >= J2SE_19)
		|| ((NULL != classLoader->classPathEntries) && (classLoader == vmThread->javaVM->systemClassLoader))
	) {
		IDATA findResult = -1;
		J9TranslationLocalBuffer localBuffer = {J9_CP_INDEX_NONE, LOAD_LOCATION_UNKNOWN, NULL};

		if (J9_JCL_FLAG_CLASSLOADERS & vmThread->javaVM->jclFlags) {
			findResult = callFindLocallyDefinedClass(vmThread, j9module, className, classNameLength, classLoader, options, 0, &localBuffer);
		} else {
			findResult = callFindLocallyDefinedClass(vmThread, j9module, className, classNameLength, classLoader, options, BCU_BOOTSTRAP_ENTRIES_ONLY, &localBuffer);
			if (-1 == findResult) {
				findResult = callFindLocallyDefinedClass(vmThread, j9module, className, classNameLength, classLoader, options, BCU_NON_BOOTSTRAP_ENTRIES_ONLY, &localBuffer);
			}
		}
		if (-1 != findResult) {
			J9TranslationBufferSet *dynamicLoadBuffers = vmThread->javaVM->dynamicLoadBuffers;

			if (0 == findResult) {
				/* Loaded a class from disk. Need to convert it into internal representation */
				if (NULL == dynamicLoadBuffers) {
					setCurrentExceptionUTF(vmThread, J9VMCONSTANTPOOL_JAVALANGNOCLASSDEFFOUNDERROR, "dynamic loader is unavailable");
				} else {
					UDATA defineClassOptions = options & ~J9_FINDCLASS_FLAG_USE_LOADER_CP_ENTRIES;	/* allow define class to use normal findClass */

					/* this function exits the class table mutex */
					foundClass = dynamicLoadBuffers->internalDefineClassFunction(vmThread, className, classNameLength,
						dynamicLoadBuffers->sunClassFileBuffer, dynamicLoadBuffers->currentSunClassFileSize,
						NULL, classLoader, NULL, defineClassOptions, NULL, NULL, &localBuffer); /* this function exits the class table mutex */
				}
			} else {
				/* Dynamically loaded a ROM class.
				 * An existing ROMClass is found in the shared class cache.
				 * If -Xshareclasses:enableBCI is present, need to give VM a chance to trigger ClassFileLoadHook event.
				 */
				J9ROMClass *romClass = (J9ROMClass *) findResult;
				if ((NULL != vmThread->javaVM->sharedClassConfig) && (0 != vmThread->javaVM->sharedClassConfig->isBCIEnabled(vmThread->javaVM))) {
					UDATA defineClassOptions = options & ~J9_FINDCLASS_FLAG_USE_LOADER_CP_ENTRIES;	/* allow define class to use normal findClass */

					defineClassOptions |= J9_FINDCLASS_FLAG_SHRC_ROMCLASS_EXISTS;

					foundClass = dynamicLoadBuffers->internalDefineClassFunction(vmThread, className, classNameLength,
							J9ROMCLASS_INTERMEDIATECLASSDATA(romClass), romClass->intermediateClassDataLength,
							NULL, classLoader, NULL, defineClassOptions, romClass, NULL, &localBuffer);
				} else {
					J9ROMClass *errorRomClass;
					/* this function exits the class table mutex */
					foundClass = foundROMClass(vmThread, classLoader, options, romClass, &errorRomClass, localBuffer.entryIndex, localBuffer.loadLocationType);

					if (NULL != errorRomClass) {
						if (0 != (J9_FINDCLASS_FLAG_THROW_ON_FAIL & options)) {
							setExceptionForErroredRomClass(errorRomClass, vmThread);
						}
					}
					/* class table mutex is now unlocked */
				}
			}
		} else {
			omrthread_monitor_exit(vmThread->javaVM->classTableMutex);
		}
	} else {
		omrthread_monitor_exit(vmThread->javaVM->classTableMutex);
	}

	Trc_VM_internalFindClass_attemptDynamicClassLoad_exit(vmThread, foundClass, &foundClass);
	return foundClass;
}


/**
 * Call the class loader to get the specified class.
 * Throw ClassNotFound exception if load failed
 * @param vmThread Current VM thread
 * @param className Name of the class to load
 * @param classNameLength Length of the class name
 * @param classLoader Class loader object
 * @param classNotFoundException Pointer to store the ClassNotFoundException into if one is cleared
 * @return loaded class object if success, NULL if fail
 * If the class is not found, the exception is cleared and NULL returned.
 * If a different exception occurs, that exception is retained and NULL returned
 *
 * Precondition: no mutexes locked, vm access, loader object locked
 * Postcondition: no mutexes locked, vm access, loader object locked
*/

static VMINLINE J9Class*   
callLoadClass(J9VMThread* vmThread, U_8* className, UDATA classNameLength, J9ClassLoader* classLoader, j9object_t * classNotFoundException)
{
	j9object_t classNameString, sendLoadClassResult;
	J9Class *foundClass = NULL;

	Assert_VM_mustHaveVMAccess(vmThread);

	classNameString = vmThread->javaVM->memoryManagerFunctions->j9gc_createJavaLangString(vmThread, className, classNameLength, J9_STR_XLAT);
	if (classNameString != NULL) {
		J9JavaVM * vm = vmThread->javaVM;

		Trc_VM_internalFindClass_sendLoadClass(vmThread, classNameLength, className, classNameString, classLoader->classLoaderObject);
		sendLoadClass(vmThread, classLoader->classLoaderObject, classNameString, 0, 0);
		sendLoadClassResult = (j9object_t) vmThread->returnValue;
		if (NULL == sendLoadClassResult) {
			j9object_t exception;

			exception = vmThread->currentException;
			vmThread->currentException = NULL;
			if (exception != NULL) {
				J9Class* notFound;

				/*
				 * if the load failed, see if it raised a ClassNotFound exception.  If so,
				 * clear the exception and let the caller handle it (map to NoClassDefFound)
				 * copy the exception to a special stack frame to prevent it from being lost in GCs
				 */
				Trc_VM_callLoadClass_foundClass_Null(vmThread);
				PUSH_OBJECT_IN_SPECIAL_FRAME(vmThread, exception);
				notFound = internalFindKnownClass(vmThread, J9VMCONSTANTPOOL_JAVALANGCLASSNOTFOUNDEXCEPTION, J9_FINDKNOWNCLASS_FLAG_EXISTING_ONLY);
				exception = POP_OBJECT_IN_SPECIAL_FRAME(vmThread);
				vmThread->currentException = exception;

				/* Test if the current exception is a ClassNotFound exception or subclass thereof */
				if (notFound != NULL) {
					J9Class *exceptionClass;

					exceptionClass = J9OBJECT_CLAZZ(vmThread, exception);
				 	if (isSameOrSuperClassOf(notFound, exceptionClass)) {
				 		*classNotFoundException = exception;
			 			vmThread->currentException = NULL;
			 			vmThread->privateFlags &= ~J9_PRIVATE_FLAGS_REPORT_EXCEPTION_THROW;
				 	}
				}
			}
 		} else { /* sendLoadClassResult is not NULL */
 			J9Class * ramClass;
 			J9UTF8* foundClassName;

 			Trc_VM_internalFindClass_sentLoadClass(vmThread, classNameLength, className, sendLoadClassResult);
 			Assert_VM_true(J9VM_IS_INITIALIZED_HEAPCLASS(vmThread, sendLoadClassResult));
			foundClass = J9VM_J9CLASS_FROM_HEAPCLASS(vmThread, sendLoadClassResult);
			omrthread_monitor_enter(vmThread->javaVM->classTableMutex);
			/* Verify that the actual name matches the expected */
			foundClassName = J9ROMCLASS_CLASSNAME(foundClass->romClass);
			if (!J9UTF8_DATA_EQUALS(className, classNameLength, J9UTF8_DATA(foundClassName), J9UTF8_LENGTH(foundClassName))) {
				/* force failure */
				foundClass = NULL;
			} else {
				/* See if a class of this name is already in the table - if not, add it */

				ramClass = hashClassTableAt(classLoader, className, classNameLength);
				if (NULL == ramClass) {
					/* Ensure loading constraints have not been violated */

					if (vm->runtimeFlags & J9_RUNTIME_VERIFY) {
			 			J9Class * loadingConstraintError = j9bcv_satisfyClassLoadingConstraint(vmThread, classLoader, foundClass);

						if (loadingConstraintError != NULL) {
							omrthread_monitor_exit(vmThread->javaVM->classTableMutex);
							setClassLoadingConstraintError(vmThread, classLoader, loadingConstraintError);
							return NULL;
						}
					}


					/* Add the found class to our class table since we are an initiating loader */

					if (hashClassTableAtPut(vmThread, classLoader, className, classNameLength, foundClass)) {
						/* Failed to store the class - GC and retry */

						omrthread_monitor_exit(vmThread->javaVM->classTableMutex);
						vmThread->javaVM->memoryManagerFunctions->j9gc_modron_global_collect_with_overrides(vmThread, J9MMCONSTANT_EXPLICIT_GC_NATIVE_OUT_OF_MEMORY);
						omrthread_monitor_enter(vmThread->javaVM->classTableMutex);

						/* See if a class of this name is already in the table - if not, try the add again */

						ramClass = hashClassTableAt(classLoader, className, classNameLength);
						if (NULL == ramClass) {
							if (hashClassTableAtPut(vmThread, classLoader, className, classNameLength, foundClass)) {
								/* Add failed again, throw native OOM */

								omrthread_monitor_exit(vmThread->javaVM->classTableMutex);
								setNativeOutOfMemoryError(vmThread, 0, 0);
								return NULL;
							}
						} else {
							/* If the existing class in the table is different than the value from loadClass, fail */

							if (ramClass != foundClass) {
								foundClass = NULL;
							}
						}
					}
				} else {
					/* If the existing class in the table is different than the value from loadClass, fail */

					if (ramClass != foundClass) {
						foundClass = NULL;
					}
				}
			}
			omrthread_monitor_exit(vmThread->javaVM->classTableMutex);
 		}
	} else {
		foundClass = NULL;
	}
	return foundClass;
}

/**
 * Waits for another thread to load the same class (using the same classloader).
 * This is called if there is a classloading contention.
 * @param vmStruct Current VM thread
 * @param tableEntry: contains name of the class to load (see below), length of the class name
 * and class loader object
 * @param className: pointer to this thread's copy of the classname string
 * @param classNameLength: length of the classname string
 * 
 * @return loaded class object if success, NULL if fail
 *
 * Precondition: classTableMutex locked, vm access
 * Postcondition: classTableMutex locked, vm access
 * The className pointer becomes null when the loading thread completes the load.
 */


static J9Class*   
waitForContendedLoadClass(J9VMThread* vmThread, J9ContendedLoadTableEntry *tableEntry, U_8* className, UDATA classNameLength)
{
	UDATA recursionCount  = 0;
	UDATA i;
	J9Class* foundClass = NULL;
	J9VMThread*  monitorOwner = NULL;
	UDATA status = CLASSLOADING_DUMMY;

	Trc_VM_waitForContendedLoadClass_getObjectMonitorOwner(vmThread, vmThread, tableEntry->classLoader, classNameLength, className);
	Assert_VM_mustHaveVMAccess(vmThread);
	/* get here if and only if someone else is loading the class */
	/* give up the classloader monitor to allow other threads to run */
	monitorOwner = getObjectMonitorOwner(vmThread->javaVM, vmThread, tableEntry->classLoader->classLoaderObject, &recursionCount);
	if (monitorOwner == vmThread) {
		Trc_VM_waitForContendedLoadClass_release_object_monitor(vmThread, vmThread, tableEntry->classLoader, classNameLength, className);
		for (i = 0; i < recursionCount; ++i) {
			objectMonitorExit(vmThread, tableEntry->classLoader->classLoaderObject);
		}
	} else {
		recursionCount = 0;
	}
	internalReleaseVMAccess(vmThread);
	do {
		omrthread_monitor_wait(vmThread->javaVM->classTableMutex);
	} while ((status = tableEntry->status) == CLASSLOADING_LOAD_IN_PROGRESS);
	/* still have classTableMutex here */
	Trc_VM_waitForContendedLoadClass_waited(vmThread, vmThread, tableEntry->classLoader, classNameLength, className, status);
	foundClass = hashClassTableAt(tableEntry->classLoader, className, classNameLength);
	omrthread_monitor_exit(vmThread->javaVM->classTableMutex);
	internalAcquireVMAccess(vmThread);
	Trc_VM_waitForContendedLoadClass_acquired_vm_access(vmThread, vmThread, tableEntry->classLoader, classNameLength, className);
	for (i = 0; i < recursionCount; ++i) {
		objectMonitorEnter(vmThread, tableEntry->classLoader->classLoaderObject);
	}
	Assert_VM_mustHaveVMAccess(vmThread);
	omrthread_monitor_enter(vmThread->javaVM->classTableMutex);
	return foundClass;
}


/**
 * Test if another thread is already loading the class.  If not, load it, otherwise wait for it to be loaded.
 * Disable serialization if the classloader has registered as parallel capable.
 * @param vmStruct Current VM thread
 * @param className Name of the class to load
 * @param classNameLength Length of the class name
 * @param classLoader Class loader object
 * @param classNotFoundException Pointer to store the ClassNotFoundException into if one is cleared
 *
 * @return loaded class object if success, NULL if fail
 *
 * Precondition: classTableMutex locked, vm access
 * Postcondition: classTableMutex locked, vm access
 *
 * Locking order:
 * get classLoader mutex
 * get VM Access
 * get classTable mutex
 */

static VMINLINE J9Class*   
arbitratedLoadClass(J9VMThread* vmThread, U_8* className, UDATA classNameLength, J9ClassLoader* classLoader, j9object_t * classNotFoundException)
{
	J9Class* foundClass;

	Assert_VM_mustHaveVMAccess(vmThread);
	Trc_VM_arbitratedLoadClass(vmThread, vmThread, classLoader, classNameLength, className);

	*classNotFoundException = NULL;

	if ((NULL == vmThread->javaVM->contendedLoadTable) /* allow contended classloading in all loaders*/
			|| (J9CLASSLOADER_PARALLEL_CAPABLE == (J9CLASSLOADER_PARALLEL_CAPABLE & classLoader->flags)) /* this loader is parallel capable */
	) {
		/* start old algorithm */
		omrthread_monitor_exit(vmThread->javaVM->classTableMutex);
		foundClass = callLoadClass(vmThread, className, classNameLength, classLoader, classNotFoundException);
		omrthread_monitor_enter(vmThread->javaVM->classTableMutex);
		/* end old algorithm */
	} else {
		J9ContendedLoadTableEntry *tableEntry = contendedLoadTableAddThread(vmThread, classLoader, className, classNameLength, CLASSLOADING_LOAD_IN_PROGRESS);
		U_8 done = 1;
		UDATA count = 0;

		do {
			if (tableEntry->thread == vmThread) {
				/* get here if and only if I am the first to try loading the class or if I am retrying because the first guy failed. */

				omrthread_monitor_exit(vmThread->javaVM->classTableMutex);
				Trc_VM_arbitratedLoadClass_callLoadClass(vmThread, vmThread, classLoader, classNameLength, className);
				foundClass = callLoadClass(vmThread, className, classNameLength, tableEntry->classLoader, classNotFoundException);
				omrthread_monitor_enter(vmThread->javaVM->classTableMutex);
				done = 1;
			} else {
				foundClass = waitForContendedLoadClass(vmThread, tableEntry, className, classNameLength);
				if (NULL == foundClass) {
					done = 0;
					if (NULL == tableEntry->thread) { /* nobody else owns the record, so I'll take it */
						tableEntry->thread = vmThread;
						tableEntry->status = CLASSLOADING_LOAD_IN_PROGRESS;
						/* The original owner's name and length are no longer valid */
						tableEntry->className = className;
						tableEntry->classNameLength = classNameLength;
					}
					Trc_VM_arbitratedLoadClass_retry(vmThread, vmThread, classLoader, classNameLength, className);
				}
			}
		} while (!done);
		count = contendedLoadTableRemoveThread(vmThread, tableEntry,
				(NULL == foundClass) ? CLASSLOADING_LOAD_FAILED: CLASSLOADING_LOAD_SUCCEEDED);
		if (count > 0) { /* only the owner(s) should do the notification */
			if (tableEntry->thread == vmThread) {
				tableEntry->thread = NULL; /* indicate that nobody owns this entry */
				omrthread_monitor_notify_all(vmThread->javaVM->classTableMutex);
				Trc_VM_arbitratedLoadClass_notify(vmThread, vmThread, tableEntry->classLoader, classNameLength, className);
				/* May awaken threads waiting for other [className, classLoader] values */
			}
		}
	}

	Assert_VM_mustHaveVMAccess(vmThread);
	Trc_VM_arbitratedLoadClass_exit(vmThread, vmThread, classLoader, classNameLength, className, foundClass);
	return foundClass;
}

/**
 * Load non-array class.
 * @param vmThread Current VM thread
 * @param j9module Pointer to J9Module in which the class is to be searched
 * @param className Name of the class to load
 * @param classNameLength Length of the class name
 * @param classLoader Class loader object
 * @param options load options such as J9_FINDCLASS_FLAG_EXISTING_ONLY
 * @param exception Pointer to storage for loading exception
 * @return loaded class object if success, NULL if fail
 *
 * Precondition: either J9_FINDCLASS_FLAG_EXISTING_ONLY set or classTableMutex not owned by this thread; this thread has vm access
 * Postcondition: either J9_FINDCLASS_FLAG_EXISTING_ONLY set or classTableMutex not owned by this thread; this thread has vm access
 */

static VMINLINE J9Class *  
loadNonArrayClass(J9VMThread* vmThread, J9Module *j9module, U_8* className, UDATA classNameLength, J9ClassLoader* classLoader, UDATA options, j9object_t *exception)
{
	J9Class * foundClass = NULL;
	BOOLEAN lockLoaderMonitor = FALSE;
	BOOLEAN fastMode = J9_ARE_ALL_BITS_SET(vmThread->javaVM->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_FAST_CLASS_HASH_TABLE);
	BOOLEAN loaderMonitorLocked = FALSE;

	vmThread->privateFlags &= ~J9_PRIVATE_FLAGS_CLOAD_NO_MEM;

	if (J9CLASSLOADER_PARALLEL_CAPABLE == (J9CLASSLOADER_PARALLEL_CAPABLE & classLoader->flags)) {
		Trc_VM_loadNonArrayClass_parallelCapable(vmThread, classNameLength, className, classLoader);
	} else {
		/*
		 * Test in decreasing probability of failure.
		 * J9_EXTENDED_RUNTIME_CLASSLOADER_LOCKING_ENABLED is true by default
		 */
		lockLoaderMonitor = (classLoader != vmThread->javaVM->systemClassLoader)
				&& (NULL != classLoader->classLoaderObject)
				&& (0 == (options & J9_FINDCLASS_FLAG_EXISTING_ONLY))
				&& (J9_EXTENDED_RUNTIME_CLASSLOADER_LOCKING_ENABLED ==
						(vmThread->javaVM->extendedRuntimeFlags & J9_EXTENDED_RUNTIME_CLASSLOADER_LOCKING_ENABLED));
	}

	/* If -XX:+FastClassHashTable is enabled, do not lock anything to do the initial table peek */
	if (!fastMode) {
		/* Match RI behaviour by implicitly locking the classloader */
		if (lockLoaderMonitor) {
			/* Must lock the classloader before the classTableMutex, otherwise we deadlock */
			Assert_VM_mustNotOwnMonitor(vmThread->javaVM->classTableMutex);
			Trc_VM_loadNonArrayClass_enter_object_monitor(vmThread, classLoader, classNameLength, className);
			objectMonitorEnter(vmThread, classLoader->classLoaderObject);
			loaderMonitorLocked = TRUE;
		}
		omrthread_monitor_enter(vmThread->javaVM->classTableMutex);
	}

	foundClass = hashClassTableAt(classLoader, className, classNameLength);
	if (NULL != foundClass) {
		if (!fastMode) {
			omrthread_monitor_exit(vmThread->javaVM->classTableMutex);
		}
	} else {
		if (options & J9_FINDCLASS_FLAG_EXISTING_ONLY) {
			if (!fastMode) {
				omrthread_monitor_exit(vmThread->javaVM->classTableMutex);
			}
		} else {
			/* If -XX:+FastClassHashTable is enabled, do the locking now */
			if (fastMode) {
				/* Match RI behaviour by implicitly locking the classloader */
				if (lockLoaderMonitor) {
					/* Must lock the classloader before the classTableMutex, otherwise we deadlock */
					Assert_VM_mustNotOwnMonitor(vmThread->javaVM->classTableMutex);
					Trc_VM_loadNonArrayClass_enter_object_monitor(vmThread, classLoader, classNameLength, className);
					objectMonitorEnter(vmThread, classLoader->classLoaderObject);
					loaderMonitorLocked = TRUE;
				}
				omrthread_monitor_enter(vmThread->javaVM->classTableMutex);

				/* check again if somebody else already loaded the class */
				foundClass = hashClassTableAt(classLoader, className, classNameLength);
				if (NULL != foundClass) {
					omrthread_monitor_exit(vmThread->javaVM->classTableMutex);
					goto done;
				}
			}
			/* Do not do the primtive type optimization if -Xfuture is on */
			if (0 == (vmThread->javaVM->runtimeFlags & J9_RUNTIME_XFUTURE)) {
				if ((classNameLength <= 7) && ((classLoader == vmThread->javaVM->systemClassLoader) || (classLoader == vmThread->javaVM->applicationClassLoader))) {
					switch(classNameLength) {
						case 3:
							if (memcmp(className, "int" , 3) == 0) {
primitiveClass:
								omrthread_monitor_exit(vmThread->javaVM->classTableMutex);
								if (loaderMonitorLocked) {
									Trc_VM_loadNonArrayClass_exit_object_monitor(vmThread, classLoader, classNameLength, className);
									objectMonitorExit(vmThread, classLoader->classLoaderObject);
								}
								return NULL;
							}
							break;
						case 4:
							if (memcmp(className, "char" , 4) == 0) {
								goto primitiveClass;
							}
							if (memcmp(className, "byte" , 4) == 0) {
								goto primitiveClass;
							}
							if (memcmp(className, "long" , 4) == 0) {
								goto primitiveClass;
							}
							if (memcmp(className, "void" , 4) == 0) {
								goto primitiveClass;
							}
							break;
						case 5:
							if (memcmp(className, "short" , 5) == 0) {
								goto primitiveClass;
							}
							if (memcmp(className, "float" , 5) == 0) {
								goto primitiveClass;
							}
							break;
						case 6:
							if (memcmp(className, "double" , 6) == 0) {
								goto primitiveClass;
							}
							break;
						case 7:
							if (memcmp(className, "boolean" , 7) == 0) {
								goto primitiveClass;
							}
							break;
					}
				}
			}
			/* class table mutex is locked */
			/* go directly to the dynamic loader if the flag is set or we are the bootstrap loader and we do not have a Java object to send loadClass() to */
			if (((classLoader == vmThread->javaVM->systemClassLoader) && (NULL == classLoader->classLoaderObject))
				|| (options & J9_FINDCLASS_FLAG_USE_LOADER_CP_ENTRIES)
			) {
#ifdef J9VM_OPT_DYNAMIC_LOAD_SUPPORT
				foundClass = attemptDynamicClassLoad(vmThread, j9module, className, classNameLength, classLoader, options);
				/* class table mutex is now unlocked */
#endif
			} else {
				foundClass = arbitratedLoadClass(vmThread, className, classNameLength, classLoader, exception);
				omrthread_monitor_exit(vmThread->javaVM->classTableMutex);
			}
			/* class table mutex is now unlocked */
		}
	}
	/* class table mutex is now unlocked */
done:
	if (loaderMonitorLocked) {
		Trc_VM_loadNonArrayClass_exit_object_monitor(vmThread, classLoader, classNameLength, className);
		objectMonitorExit(vmThread, classLoader->classLoaderObject);
	}

	return foundClass;
}

/**
 * It is a wrapper around internalFindClassInModule().
 */
J9Class*  
internalFindClassUTF8(J9VMThread* vmThread, U_8* className, UDATA classNameLength, J9ClassLoader* classLoader, UDATA options)
{
	return internalFindClassInModule(vmThread, NULL, className, classNameLength, classLoader, options);
}

J9Class*  
internalFindClassInModule(J9VMThread* vmThread, J9Module *j9module, U_8* className, UDATA classNameLength, J9ClassLoader* classLoader, UDATA options)
{
	J9JavaVM *vm = vmThread->javaVM;
	J9MemoryManagerFunctions *gcFuncs = vm->memoryManagerFunctions;
	j9object_t exception = NULL;
	J9Class* foundClass = NULL;
	UDATA arity;
	PORT_ACCESS_FROM_JAVAVM(vm);

	Trc_VM_internalFindClass_Entry(vmThread, classLoader, options, classNameLength, className, 0, className);

	Assert_VM_mustHaveVMAccess(vmThread);
	Assert_VM_false(NULL == classLoader);

	/* see if it is an array class */
	arity = calculateArity(vmThread, className, classNameLength);
	if (0 != arity) {
		foundClass = internalFindArrayClass(vmThread, j9module, arity, className, classNameLength, classLoader, options);
	} else {
		foundClass = loadNonArrayClass(vmThread, j9module, className, classNameLength, classLoader, options, &exception);
		if (vmThread->privateFlags & J9_PRIVATE_FLAGS_CLOAD_NO_MEM) {
			Trc_VM_internalFindClass_gcAndRetry(vmThread);
			gcFuncs->j9gc_modron_global_collect_with_overrides(vmThread, J9MMCONSTANT_EXPLICIT_GC_NATIVE_OUT_OF_MEMORY);
			foundClass = loadNonArrayClass(vmThread, j9module, className, classNameLength, classLoader, options, &exception);
			if (vmThread->privateFlags & J9_PRIVATE_FLAGS_CLOAD_NO_MEM) {
				setNativeOutOfMemoryError(vmThread, 0, 0);
			}
		}
	}

	if (NULL == foundClass) {
		/* Always throw pending error */
		if (NULL == vmThread->currentException) {
			if (0 == (vmThread->publicFlags & J9_PUBLIC_FLAGS_POP_FRAMES_INTERRUPT)) { /* no immediate async interrupt pending */
				/* Don't throw an exception if we are loading required classes. It can lead to OOM */
				BOOLEAN reportErrorFlags = (J9_PRIVATE_FLAGS_REPORT_ERROR_LOADING_CLASS |
											J9_PRIVATE_FLAGS_FAILED_LOADING_REQUIRED_CLASS);

				if (reportErrorFlags == (reportErrorFlags & vmThread->privateFlags)) {
					UDATA i;
					/* same error message as displayed by internalFindKnownClass() on failure */
#if defined (J9VM_SIZE_SMALL_CODE)
					j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_VM_UNABLE_TO_FIND_AND_INITIALIZE_REQUIRED_CLASS, classNameLength, className);
#else
					j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_BEGIN_MULTI_LINE, J9NLS_VM_UNABLE_TO_FIND_AND_INITIALIZE_REQUIRED_CLASS, classNameLength, className);
#endif
					for (i = 0; i < classLoader->classPathEntryCount; i++) {
						J9ClassPathEntry *entry = &classLoader->classPathEntries[i];
						j9nls_printf(PORTLIB, J9NLS_INFO, J9NLS_VM_SEARCHED_IN_DIR, entry->pathLength, entry->path);
					}
					j9nls_printf(PORTLIB, J9NLS_INFO, J9NLS_VM_CHECK_JAVA_HOME);
					vmThread->privateFlags &= ~(J9_PRIVATE_FLAGS_REPORT_ERROR_LOADING_CLASS);
				} else if ((0 == (J9_PRIVATE_FLAGS_FAILED_LOADING_REQUIRED_CLASS & vmThread->privateFlags)) &&
							(0 != (J9_FINDCLASS_FLAG_THROW_ON_FAIL & options))) {
					j9object_t detailMessage;

					PUSH_OBJECT_IN_SPECIAL_FRAME(vmThread, exception);
					detailMessage = gcFuncs->j9gc_createJavaLangString(vmThread, className, classNameLength, J9_STR_XLAT);
					exception = POP_OBJECT_IN_SPECIAL_FRAME(vmThread);
					if (NULL != detailMessage) {
						setCurrentExceptionWithCause(vmThread, J9VMCONSTANTPOOL_JAVALANGNOCLASSDEFFOUNDERROR, (UDATA *) detailMessage, exception);
					}
				}
			}
		}
	}

	Trc_VM_internalFindClass_Exit(vmThread, classLoader, classNameLength, className, foundClass);
	return foundClass;
}

J9Class*  
internalFindKnownClass(J9VMThread *vmThread, UDATA index, UDATA flags)
{
	J9JavaVM *vm = vmThread->javaVM;
	J9ConstantPool *jclConstantPool = (J9ConstantPool *)vm->jclConstantPool;
	J9ROMStringRef *romStringRef = &((J9ROMStringRef *)jclConstantPool->romConstantPool)[index];
	J9UTF8 *utfWrapper = J9ROMSTRINGREF_UTF8DATA(romStringRef);
	J9ClassLoader *classLoader = vm->systemClassLoader;
	J9Class *knownClass;
	PORT_ACCESS_FROM_JAVAVM(vm);

	/* Get the class for the index, loading and initializing the class if necessary. */
	knownClass = ((J9RAMClassRef *)vm->jclConstantPool)[index].value;
	if (NULL == knownClass) {
		UDATA options = 0;

		/* A few known classes can cause recursive death if they're not loadable.  Detect that condition and abort. */
		if ((J9VMCONSTANTPOOL_JAVALANGCLASSNOTFOUNDEXCEPTION == index) || (J9VMCONSTANTPOOL_JAVALANGNOCLASSDEFFOUNDERROR == index)) {
			if (0 != (J9_PRIVATE_FLAGS_LOADING_KNOWN_CLASS & vmThread->privateFlags)) {
				goto _fail;
			}
			vmThread->privateFlags |= J9_PRIVATE_FLAGS_LOADING_KNOWN_CLASS;
		}

		if ((J9_FINDKNOWNCLASS_FLAG_EXISTING_ONLY & flags) != 0) {
			options = J9_FINDCLASS_FLAG_EXISTING_ONLY;
		}
		knownClass = internalFindClassUTF8(vmThread, J9UTF8_DATA(utfWrapper), J9UTF8_LENGTH(utfWrapper), classLoader, options);

		if ((J9VMCONSTANTPOOL_JAVALANGCLASSNOTFOUNDEXCEPTION == index) || (J9VMCONSTANTPOOL_JAVALANGNOCLASSDEFFOUNDERROR == index)) {
			vmThread->privateFlags &= (~J9_PRIVATE_FLAGS_LOADING_KNOWN_CLASS);
		}
		/* If a doReturn is pending, do not cache the found class.  It will be looked up and correctly initialized the next time it is requested. */
		if (0 != (vmThread->publicFlags & J9_PUBLIC_FLAGS_POP_FRAMES_INTERRUPT)) {
			goto _done;
		}

		if (NULL == knownClass) {
			/* If J9_PRIVATE_FLAGS_FAILED_LOADING_REQUIRED_CLASS is set, then we have already generated error message in internalFindClassUTF8() */
			if ((0 != (J9_FINDKNOWNCLASS_FLAG_EXISTING_ONLY & flags)) ||
				(0 != (J9_PRIVATE_FLAGS_FAILED_LOADING_REQUIRED_CLASS & vmThread->privateFlags))) {
				goto _done;
			} else {
				goto _fail;
			}
		} else {
			J9ClassPathEntry *cpEntry;
			J9ClassLocation *classLocation = NULL;

			omrthread_monitor_enter(vm->classLoaderModuleAndLocationMutex);
			classLocation = findClassLocationForClass(vmThread, knownClass);
			omrthread_monitor_exit(vm->classLoaderModuleAndLocationMutex);

			if ((NULL != classLocation) && (J9_CP_INDEX_NONE != classLocation->entryIndex) && (LOAD_LOCATION_CLASSPATH == classLocation->locationType)) {
				cpEntry = &(knownClass->classLoader->classPathEntries[classLocation->entryIndex]);
				if (NULL != cpEntry) {
					if (0 == (CPE_FLAG_BOOTSTRAP & cpEntry->flags)) {
						j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_VM_REQUIRED_CLASS_ON_WRONG_PATH, J9UTF8_LENGTH(utfWrapper), J9UTF8_DATA(utfWrapper), cpEntry->pathLength, cpEntry->path);
						goto _exit;
					}
				}
			}
			resolveKnownClass(vm, index);	/* TODO: Pass the vmThread in */
		}
	}

	if (NULL != knownClass) {
		if (0 != (J9_FINDKNOWNCLASS_FLAG_INITIALIZE & flags)) {
			if ((J9ClassInitSucceeded != knownClass->initializeStatus) && ((UDATA)vmThread != knownClass->initializeStatus)) {
				initializeClass(vmThread, knownClass);
#ifdef J9VM_INTERP_HOT_CODE_REPLACEMENT
				if (J9AccClassHotSwappedOut & J9CLASS_FLAGS(knownClass)) {
					knownClass = knownClass->arrayClass;
				}
#endif
			}
			if (0 != (vmThread->publicFlags & J9_PUBLIC_FLAGS_POP_FRAMES_INTERRUPT)) {
				goto _done;
			}
			if (NULL != vmThread->currentException) {
				goto _fail;
			}
		}
		goto _done;
	}

_fail:
	if ((0 == (J9_RUNTIME_INITIALIZED & vm->runtimeFlags)) || (0 == (J9_FINDKNOWNCLASS_FLAG_NON_FATAL & flags))) {
		UDATA i;
#if defined (J9VM_SIZE_SMALL_CODE)
		j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_VM_UNABLE_TO_FIND_AND_INITIALIZE_REQUIRED_CLASS, J9UTF8_LENGTH(utfWrapper), J9UTF8_DATA(utfWrapper));
#else
		j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_BEGIN_MULTI_LINE, J9NLS_VM_UNABLE_TO_FIND_AND_INITIALIZE_REQUIRED_CLASS, J9UTF8_LENGTH(utfWrapper), J9UTF8_DATA(utfWrapper));
#endif
		for (i = 0; i < classLoader->classPathEntryCount; i++) {
			J9ClassPathEntry *entry = &classLoader->classPathEntries[i];
			j9nls_printf(PORTLIB, J9NLS_INFO, J9NLS_VM_SEARCHED_IN_DIR, entry->pathLength, entry->path);
		}
		j9nls_printf(PORTLIB, J9NLS_INFO, J9NLS_VM_CHECK_JAVA_HOME);
	}

_exit:
	if (0 != (J9_FINDKNOWNCLASS_FLAG_NON_FATAL & flags)) {
		knownClass = NULL;
		goto _done;
	}
	Assert_VM_internalFindKnownClassFailure();

_done:
	return knownClass;
}

/* ============================================ contendedLoadTable methods ===================================*/
/**
 *  Create a hash key based on the class name and the loader
 * @param key Table entry struct
 * @param userData not used
 */

static UDATA
classAndLoaderHashFn (void *key, void *userData)
{
	J9ContendedLoadTableEntry* entry;
	UDATA hashValue;

	entry = key;
	if (NULL != entry->className) {
		hashValue = (UDATA) entry->classLoader;
		hashValue = computeHashForUTF8(entry->className, entry->classNameLength) ^ hashValue;
	} 	else {
		hashValue = entry->hashValue;
	}

	return hashValue;
}

 /**
  * Compare two J9ContendedLoadTableEntry structs for equality
  * @param leftKey J9ContendedLoadTableEntry struct
  * @param rightKey J9ContendedLoadTableEntry struct
  * @param userData not used
  */
static UDATA
classAndLoaderHashEqualFn (void *leftKey, void *rightKey, void *userData)
{
	J9ContendedLoadTableEntry *leftEntry = leftKey;
	J9ContendedLoadTableEntry *rightEntry = rightKey;
	if ((NULL == leftEntry->className) ||(NULL == rightEntry->className)) {
		/*
		 * the entry is still in use but should be unfindable unless I have a pointer to the hash record.
		 * The latter occurs when I am deleting the record.
		 * */
		return (leftKey == rightKey);
	}
	return (leftEntry->classLoader == rightEntry->classLoader) &&
		J9UTF8_DATA_EQUALS(leftEntry->className, leftEntry->classNameLength,
		rightEntry->className, rightEntry->classNameLength);
}

/**
 * Create a new hash table to hold a list of classes being loaded and the number of non-loading threads waiting
 * @param vmThread
 * Reference to the VM thread, used to locate the VM which will create and hold the hash table.
 * @return  pointer to the hash table
 */
J9HashTable *
contendedLoadTableNew(J9JavaVM* vm, J9PortLibrary *portLib)
{
	J9HashTable *result;

	result = vm->contendedLoadTable = hashTableNew(OMRPORT_FROM_J9PORT(portLib), J9_GET_CALLSITE(), 64,
		sizeof(J9ContendedLoadTableEntry), sizeof(U_8* ), 0, J9MEM_CATEGORY_CLASSES, classAndLoaderHashFn, classAndLoaderHashEqualFn, NULL, vm);
	return result;
}

/**
 * Delete the contendedLoadTable hash table from the VM datastructure and free the hash table
 * @param vmThread
 * Reference to the VM thread, used to locate the VM and hence the table to be freed
 */

void
contendedLoadTableFree(J9JavaVM* vm)
{
	if (vm->contendedLoadTable != NULL) {
		hashTableFree(vm->contendedLoadTable);
		vm->contendedLoadTable = NULL;
	}
}

/**
 * Adds the {classLoader,className} entry in the contendedLoadTable table.
 * Note: a thread must call this exactly once per loading attempt. The className pointer becomes null when the loading thread completes the load.
 * @param vmThread
 * Reference to the VM thread, used to locate the VM and hence the table.  Also used to detect circularity.
 * @param classLoader
 * classLoader object. Used to distinguish different classes with the same name being loaded by different loaders
 * @param className
 * @param classNameLength
 * @returns new entry
 * @exceptions throws internalError exception if the add fails
 */
static J9ContendedLoadTableEntry*
contendedLoadTableAdd(J9VMThread* vmThread,
	J9ClassLoader* classLoader, U_8* className, UDATA classNameLength)
{
	struct J9ContendedLoadTableEntry query;
	struct J9ContendedLoadTableEntry *result;

	query.className = className;
	query.classNameLength = classNameLength;
	query.classLoader = classLoader;
	query.thread = vmThread;
	query.hashValue = classAndLoaderHashFn (&query, NULL); /* SO I can get the hash table if the className pointer is null but I have a pointer to the record */
	/* caller fills in the count and status fields */
	result = hashTableAdd(vmThread->javaVM->contendedLoadTable,  &query);
	if (NULL == result) {
		Trc_VM_classsupport_contendThreadsAddFail(vmThread, className, classNameLength, classLoader);
		setNativeOutOfMemoryError(vmThread, 0, 0);
	}
	return result;
}

/**
 * Deletes the {classLoader,className} entry exists in the contendedLoadTable table.
 * @param vmThread
 * Reference to the VM thread, used to locate the VM and hence the table
 * @param entry pointer to J9ContendedLoadTableEntry struct to be deleted
 */
static void
contendedLoadTableDelete(J9VMThread* vmThread, J9ContendedLoadTableEntry* entry)
{
	if (NULL == entry->className) {
		Trc_VM_contendedLoadTableDelete_notFound(vmThread, vmThread, entry);
	} else {
		Trc_VM_contendedLoadTableDelete_entry(vmThread, vmThread, entry->classNameLength, entry->className);
	}
	hashTableRemove(vmThread->javaVM->contendedLoadTable, entry);
}

/**
 * Locates the {classLoader,className} entry exists in the contendedLoadTable.
 * Return the entry struct.
 * @param vmThread current thread, used for looking up hash table and reporting tracepoints
 * @param classLoader
 * classLoader object. Used to distinguish different classes with the same name being loaded by different loaders
 * @param className
 * @param classNameLength
 * @return the struct.  If the entry does not exist, return NULL
 */
static J9ContendedLoadTableEntry*
contendedLoadTableGet(J9JavaVM* vm, J9ClassLoader* classLoader, U_8* className, UDATA classNameLength)
{
	struct J9ContendedLoadTableEntry query;
	struct J9ContendedLoadTableEntry *result;

	query.className = className;
	query.classNameLength = classNameLength;
	query.classLoader = classLoader;
	result = hashTableFind(vm->contendedLoadTable, &query);
	return result;
}

/**
 * Test if the {classLoader,className} entry exists in the contendedLoadTable.
 * If not, it adds an entry and sets the count to 1.  If the entry exists, increment count.
 * @param vmThread current thread, used for looking up hash table and reporting tracepoints
 * @param classLoader
 * classLoader object. Used to distinguish different classes with the same name being loaded by different loaders
 * @param className
 * @param classNameLength
 * @param status initialization value for the status field if the entry is being created
 * @return tableEntry either created or looked up in the table
  * Note: a thread trying to load a class which it is already trying to load may be the result of class circularity,
 * which will be detected via defineClass, or by a recursive call to handle such things as security checks.
 * @
 */
static J9ContendedLoadTableEntry *
contendedLoadTableAddThread (J9VMThread* vmThread, J9ClassLoader* classLoader, U_8* className,
	UDATA classNameLength, UDATA status)
{
	J9ContendedLoadTableEntry *result;

	result = contendedLoadTableGet(vmThread->javaVM, classLoader, className, classNameLength);
	if (NULL == result) {
		result = contendedLoadTableAdd(vmThread, classLoader, className, classNameLength);
		if (NULL == result) {
			Trc_VM_classsupport_contendThreadsRecordMissing(vmThread, className, classNameLength, classLoader);
			setNativeOutOfMemoryError(vmThread, 0, 0);
		} else {
			result->count = 1;
			result->status = status;
		}
	} else {
		Trc_VM_contendedLoadTableAddThread(vmThread, vmThread, classLoader, classNameLength, className, result->count);
		++(result->count);
	}
	Trc_VM_contendedLoadTableAddThreadExit(vmThread, vmThread, classLoader, classNameLength, className, (NULL != result ? result->count : 0));
	return result;
}

/**
 * Locates the {classLoader,className} entry exists in the contendedLoadTable.
 * If it does not exist, exit with error. If the entry exists, decrement count. If the new count is zero,
 * delete the entry.  Exits if the entry does not exist.
 * If the deleter is the loading thread, the name pointer is invalid and is set to null.
 * @param vmThread current thread, used for looking up hash table and reporting tracepoints
 * @param tableEntry pointer to a new or existing struct containing the classname and loader to be deleted
 * @param status new value for the status field.  Ignored if it is "loadingDontCare"
 */

static UDATA
contendedLoadTableRemoveThread(J9VMThread* vmThread, J9ContendedLoadTableEntry *tableEntry, UDATA status)
{
	IDATA newCount = 0;

	Assert_VM_mustHaveVMAccess(vmThread);
	--(tableEntry->count);
	if (NULL == tableEntry->className) {
		Trc_VM_contendedLoadTableRemoveThreadNull(vmThread, vmThread, tableEntry->classLoader, tableEntry->count);
	} else {
		Trc_VM_contendedLoadTableRemoveThread(vmThread, vmThread, tableEntry->classLoader, tableEntry->classNameLength, tableEntry->className, tableEntry->count);
	}
	if (vmThread == tableEntry->thread) {
		tableEntry->className = NULL;
		tableEntry->classNameLength = 0;
	}
	if ((newCount = tableEntry->count) == 0) {
		contendedLoadTableDelete(vmThread, tableEntry);
	} else {
		if (status != CLASSLOADING_DONT_CARE) {
			tableEntry->status = status;
		}
	}
	return newCount;
}



#ifdef J9VM_OPT_SIDECAR

void
fixCPShapeDescription(J9Class * clazz, UDATA cpIndex)
{
	UDATA wordIndex = (UDATA) (cpIndex / (sizeof(U_32)*2));
	UDATA shiftAmount = (UDATA) ((cpIndex % (sizeof(U_32)*2)) * 4);
	U_32 * cpShapeDescription = J9ROMCLASS_CPSHAPEDESCRIPTION(clazz->romClass);

	cpShapeDescription[wordIndex] = (cpShapeDescription[wordIndex] & ~(J9_CP_DESCRIPTION_MASK << shiftAmount)) | (J9CPTYPE_INSTANCE_METHOD << shiftAmount);
}

void
fixUnsafeMethods(J9VMThread* currentThread, jclass clazz)
{
	J9Class *j9clazz = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, J9_JNI_UNWRAP_REFERENCE(clazz));
	J9ROMClass *romClass = j9clazz->romClass;
	J9ConstantPool *ramCP = J9_CP_FROM_CLASS(j9clazz);
	J9ROMConstantPoolItem *romCP = ramCP->romConstantPool;
	J9Method *currentMethod = j9clazz->ramMethods;
	J9Method *endOfMethods = currentMethod + romClass->romMethodCount;
	while (currentMethod != endOfMethods) {
		J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(currentMethod);
		U_8 *bytecodes = currentMethod->bytecodes;
		UDATA pc = 0;
		UDATA endPC = J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod);
		while (pc < endPC) {
			U_8 bc = bytecodes[pc];
			switch(bc) {
			case JBlookupswitch:
			case JBtableswitch: {
				I_32 numEntries = 0;
				I_32 low = 0;
				pc = pc + (4 - (pc & 3));
				pc += sizeof(U_32);
				low = *((I_32*)(bytecodes + pc));
				pc += sizeof(U_32);
				if (bc == JBtableswitch) {
					I_32 high = *((I_32*)(bytecodes + pc));
					pc += sizeof(U_32);
					numEntries = high - low + 1;
				} else {
					numEntries = low * 2;
				}
				pc += (numEntries * 4);
				break;
			}
			case JBinvokevirtual: {
				U_16 cpIndex = *(U_16*)(bytecodes + pc + 1);
				J9ROMMethodRef *romMethodRef = (J9ROMMethodRef*)romCP + cpIndex;
				J9Class *resolvedClass = resolveClassRef(currentThread, ramCP, romMethodRef->classRefCPIndex, J9_RESOLVE_FLAG_RUNTIME_RESOLVE);
				if (NULL == resolvedClass) {
					currentThread->currentException = NULL;
					currentThread->privateFlags &= ~(UDATA)J9_PRIVATE_FLAGS_REPORT_EXCEPTION_THROW;
				} else {
					J9Method *method = (J9Method*)javaLookupMethod(currentThread, resolvedClass, J9ROMMETHODREF_NAMEANDSIGNATURE(romMethodRef), NULL, J9_LOOK_VIRTUAL | J9_LOOK_NO_THROW);
					if (NULL != method) {
						if (!J9ROMMETHOD_HAS_VTABLE(J9_ROM_METHOD_FROM_RAM_METHOD(method))) {
							bytecodes[pc] = JBinvokespecial;
							fixCPShapeDescription(j9clazz, cpIndex);
							memset(ramCP + 1, 0, sizeof(J9RAMConstantPoolItem) * (romClass->ramConstantPoolCount - 1));
							internalRunPreInitInstructions(j9clazz, currentThread);
						}
					}
				}
				/* Intentional fall through */
			}
			default:
				pc += (J9JavaInstructionSizeAndBranchActionTable[bc] & 7);
				break;
			}
		}
		currentMethod += 1;
	}
}
#endif

