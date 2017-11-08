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

#include <stdlib.h>
#include "j9.h"
#include "j9protos.h"
#include "j9consts.h"
#include "omrgcconsts.h"
#include "rommeth.h"
/* include stackwalk.h to get value of J9SW_NEEDS_JIT_2_INTERP_THUNKS */
#include "stackwalk.h"
#include "objhelp.h"
#include "../util/ut_module.h"
#undef UT_MODULE_LOADED
#undef UT_MODULE_UNLOADED
#include "ut_j9vm.h"
#include "j9bcvnls.h"
#include "j2sever.h"

#include "VMHelpers.hpp"

extern "C" {

#define VTABLE_SLOT_TAG_MASK 				3
#define ROM_METHOD_ID_TAG 					1
#define DEFAULT_CONFLICT_METHOD_ID_TAG 		2
#define EQUIVALENT_SET_ID_TAG 				3
#define INTERFACE_TAG 1

#define LOCAL_INTERFACE_ARRAY_SIZE 10

enum J9ClassFragments {
	RAM_CLASS_HEADER_FRAGMENT,
	RAM_METHODS_FRAGMENT,
	RAM_SUPERCLASSES_FRAGMENT,
	RAM_INSTANCE_DESCRIPTION_FRAGMENT,
	RAM_ITABLE_FRAGMENT,
	RAM_STATICS_FRAGMENT,
	RAM_CONSTANT_POOL_FRAGMENT,
	RAM_CALL_SITES_FRAGMENT,
	RAM_METHOD_TYPES_FRAGMENT,
	RAM_VARHANDLE_METHOD_TYPES_FRAGMENT,
	RAM_STATIC_SPLIT_TABLE_FRAGMENT,
	RAM_SPECIAL_SPLIT_TABLE_FRAGMENT,
	RAM_CLASS_FRAGMENT_COUNT
};

/*
 * RAM_CLASS_FRAGMENT_LIMIT is the inclusive lower bound of "large" RAM class free blocks. Its value
 * is the largest alignment constraint amongst RAM class fragments: J9_REQUIRED_CLASS_ALIGNMENT.
 * RAM_CLASS_SMALL_FRAGMENT_LIMIT is the inclusive lower bound of "small" RAM class free blocks.
 * Smaller blocks are placed on the "tiny" free block list. Its value is chosen experimentally to
 * provide optimal performance on the shared classes class loading benchmark.
 */
#define RAM_CLASS_FRAGMENT_LIMIT J9_REQUIRED_CLASS_ALIGNMENT
#define RAM_CLASS_SMALL_FRAGMENT_LIMIT ((sizeof(UDATA) + 4) * sizeof(UDATA))

/* align method table start such that CP is correctly aligned */
#if defined(J9VM_INTERP_NATIVE_SUPPORT)
/* 3 bits of tag needed in J9Method->constantPool */
#if defined(J9VM_JIT_DYNAMIC_LOOP_TRANSFER)
/* 4 bits of tag needed in J9Method->constantPool */
#define REQUIRED_CONSTANT_POOL_ALIGNMENT 16
#endif
#endif
#if !defined(REQUIRED_CONSTANT_POOL_ALIGNMENT)
#define REQUIRED_CONSTANT_POOL_ALIGNMENT 8
#endif

/* Static J9ITable used as a non-NULL iTable cache value by classes that don't implement any interfaces. Defined in hshelp.c. */
extern const J9ITable invalidITable;

typedef struct J9RAMClassFreeListLargeBlock {
    UDATA size;
    struct J9RAMClassFreeListLargeBlock* nextFreeListBlock;
    UDATA maxSizeInList;
} J9RAMClassFreeListLargeBlock;

typedef struct RAMClassAllocationRequest {
	UDATA prefixSize;
	UDATA alignment;
	UDATA alignedSize;
	UDATA *address;
	UDATA index;
	UDATA fragmentSize;
	struct RAMClassAllocationRequest *next;
} RAMClassAllocationRequest;

typedef struct J9CreateRAMClassState {
	J9Class *ramClass;
	j9object_t classObject;
	BOOLEAN retry;
} J9CreateRAMClassState;

typedef struct J9EquivalentEntry {
	J9Method *method;
	struct J9EquivalentEntry *next;
} J9EquivalentEntry;

typedef struct J9OverrideErrorData {
	J9ClassLoader *loader1;
	J9UTF8 *class1NameUTF;
	J9ClassLoader *loader2;
	J9UTF8 *class2NameUTF;
	J9UTF8 *exceptionClassNameUTF;
	J9UTF8 *methodNameUTF;
	J9UTF8 *methodSigUTF;
} J9OverrideErrorData;

static J9Class* markInterfaces(J9ROMClass *romClass, J9Class *superclass, J9ClassLoader *classLoader, BOOLEAN *foundCloneable, UDATA *markedInterfaceCount, UDATA *inheritedInterfaceCount, IDATA *maxInterfaceDepth);
static void unmarkInterfaces(J9Class *interfaceHead);
static void createITable(J9VMThread* vmStruct, J9Class *ramClass, J9Class *interfaceClass, J9ITable ***previousLink, UDATA **currentSlot, UDATA depth);
static UDATA* initializeRAMClassITable(J9VMThread* vmStruct, J9Class *ramClass, J9Class *superclass, UDATA* currentSlot, J9Class *interfaceHead, IDATA maxInterfaceDepth);
static UDATA addInterfaceMethods(J9VMThread *vmStruct, J9ClassLoader *classLoader, J9Class *interfaceClass, UDATA vTableWriteIndex, UDATA *vTableAddress, J9Class *superclass, J9ROMClass *romClass, UDATA *defaultConflictCount, J9Pool *equivalentSets, UDATA *equivSetCount, J9OverrideErrorData *errorData);
static UDATA* computeVTable(J9VMThread *vmStruct, J9ClassLoader *classLoader, J9Class *superclass, J9ROMClass *taggedClass, UDATA packageID, J9ROMMethod ** methodRemapArray, J9Class *interfaceHead, UDATA *defaultConflictCount, UDATA interfaceCount, UDATA inheritedInterfaceCount, J9OverrideErrorData *errorData);
static void copyVTable(J9VMThread *vmStruct, J9Class *ramClass, J9Class *superclass, UDATA *vTable, UDATA defaultConflictCount);
static UDATA processVTableMethod(J9VMThread *vmThread, J9ClassLoader *classLoader, UDATA *vTableAddress, J9Class *superclass, J9ROMClass *romClass, J9ROMMethod *romMethod, UDATA localPackageID, UDATA vTableWriteIndex, void *storeValue, J9OverrideErrorData *errorData);
static VMINLINE UDATA growNewVTableSlot(UDATA *vTableAddress, UDATA vTableWriteIndex, void *storeValue);
static UDATA getVTableIndexForNameAndSigStartingAt(UDATA *vTable, J9UTF8 *name, J9UTF8 *signature, UDATA vTableIndex);
static UDATA checkPackageAccess(J9VMThread *vmThread, J9Class *foundClass, UDATA classPreloadFlags);
static void setCurrentExceptionForBadClass(J9VMThread *vmThread, J9UTF8 *badClassName, UDATA exceptionIndex);
static BOOLEAN verifyClassLoadingStack(J9VMThread *vmThread, J9ClassLoader *classLoader, J9ROMClass *romClass);
static void popFromClassLoadingStack(J9VMThread *vmThread);
static VMINLINE BOOLEAN loadSuperClassAndInterfaces(J9VMThread *vmThread, J9ClassLoader *classLoader, J9ROMClass *romClass, J9Class *elementClass, UDATA packageID, BOOLEAN hotswapping, UDATA classPreloadFlags, J9Class **superclassOut);
#if defined(J9VM_OPT_VALHALLA_NESTMATES)
static J9Class *loadNestTop(J9VMThread *vmThread, J9ClassLoader *classLoader, J9UTF8 *nestTopName, UDATA classPreloadFlags);
#endif /* defined(J9VM_OPT_VALHALLA_NESTMATES) */
static J9Class* internalCreateRAMClassDropAndReturn(J9VMThread *vmThread, J9ROMClass *romClass, J9CreateRAMClassState *state);
static J9Class* internalCreateRAMClassDoneNoMutex(J9VMThread *vmThread, J9ROMClass *romClass, UDATA options, J9CreateRAMClassState *state);
static J9Class* internalCreateRAMClassDone(J9VMThread *vmThread, J9ClassLoader *classLoader, J9ROMClass *romClass, UDATA options, J9Class *elementClass,
	J9UTF8 *className, J9CreateRAMClassState *state);
static J9Class* internalCreateRAMClassFromROMClassImpl(J9VMThread *vmThread, J9ClassLoader *classLoader, J9ROMClass *romClass, UDATA options, J9Class *elementClass,
	J9ROMMethod **methodRemapArray, IDATA entryIndex, I_32 locationType, J9Class *classBeingRedefined, UDATA packageID, J9Class *superclass, J9CreateRAMClassState *state,
	J9ClassLoader* hostClassLoader, J9Class *hostClass);
static J9MemorySegment* internalAllocateRAMClass(J9JavaVM *javaVM, J9ClassLoader *classLoader, RAMClassAllocationRequest *allocationRequests, UDATA allocationRequestCount);
static I_32 interfaceDepthCompare(const void *a, const void *b);
#if defined(J9VM_INTERP_CUSTOM_SPIN_OPTIONS)
static void checkForCustomSpinOptions(void *element, void *userData);
#endif /* J9VM_INTERP_CUSTOM_SPIN_OPTIONS */
static void trcModulesSettingPackage(J9VMThread *vmThread, J9Class *ramClass, J9ClassLoader *classLoader, J9UTF8 *className);


/**
 * Mark all of the interfaces supported by this class, including all interfaces
 * inherited by superinterfaces. Unmark all interfaces which are inherited from
 * the superclass.
 * 
 * Returns the head of a linked list of interfaces (through the tagged instanceDescription field).
 */
static J9Class *
markInterfaces(J9ROMClass *romClass, J9Class *superclass, J9ClassLoader *classLoader, BOOLEAN *foundCloneable, UDATA *markedInterfaceCount, UDATA *inheritedInterfaceCount, IDATA *maxInterfaceDepth)
{
	J9Class *interfaceHead = NULL;
	J9Class *lastInterface = NULL;
	UDATA interfaceCount = romClass->interfaceCount;
	UDATA foundInterfaces = 0;
	UDATA localInheritedInterfaceCount = 0;
	IDATA localMaxInterfaceDepth = *maxInterfaceDepth;

	/* Mark all interfaces which are inherited from the superclass. */
	if (superclass != NULL) {
		J9ITable *iTable = (J9ITable *)superclass->iTable;
		while (iTable != NULL) {
			/* No need to check if the new class is an interface, since all
			 * interfaces have Object as their superclass, which will not
			 * incorrectly implement any interfaces.
			 */
			localMaxInterfaceDepth = OMR_MAX(localMaxInterfaceDepth, (IDATA)iTable->depth);
			localInheritedInterfaceCount += 1;
			iTable->interfaceClass->instanceDescription = (UDATA *)-1;
			iTable = iTable->next;
		}
	}

	/* Mark all of the interfaces supported by this class, including all interfaces inherited by superinterfaces. */
	if (interfaceCount != 0) {
		UDATA i;
		J9SRP *interfaceNames = J9ROMCLASS_INTERFACES(romClass);
		for (i = 0; i<interfaceCount; i++) {
			J9ITable *iTable;
			J9UTF8 *interfaceName = NNSRP_GET(interfaceNames[i], J9UTF8*);
			/* peek the table, do not load */
			J9Class *interfaceClass = hashClassTableAt(classLoader, J9UTF8_DATA(interfaceName), J9UTF8_LENGTH(interfaceName));
			/* the interface classes are not NULL, as this was checked by the caller */
			if ((foundCloneable != NULL) && (J9_JAVA_CLASS_CLONEABLE == (J9CLASS_FLAGS(interfaceClass) & J9_JAVA_CLASS_CLONEABLE))) {
				*foundCloneable = TRUE;
			}
			iTable = (J9ITable *)interfaceClass->iTable;
			while (iTable != NULL) {
				/* the marked field is shared with the instanceDescription
				 * field. Always keep the low bit tagged so that, if anyone
				 * does look at it,	they don't try to indirect it. Interfaces
				 * which are inherited are marked, so skip them.
				 */
				if (iTable->interfaceClass->instanceDescription == (UDATA *)1) {
					foundInterfaces++;
					/* Mark the current interface so it cannot be linked to itself. */
					iTable->interfaceClass->instanceDescription = (UDATA *)-1;
					if (lastInterface == NULL) {
						interfaceHead = lastInterface = iTable->interfaceClass;
					} else {
						lastInterface->instanceDescription = (UDATA *)((UDATA)iTable->interfaceClass | INTERFACE_TAG);
						lastInterface = iTable->interfaceClass; 
					}
				}
				localMaxInterfaceDepth = OMR_MAX(localMaxInterfaceDepth, (IDATA)iTable->depth);
				iTable = iTable->next;
			}
		}
	}
	if (lastInterface != NULL) {
		/* Unmark the last interface. */
		lastInterface->instanceDescription = (UDATA *)1;
	}
	
	/* Unmark all interfaces which are inherited from the superclass. */
	if (superclass != NULL) {
		J9ITable *iTable = (J9ITable *)superclass->iTable;
		while (iTable != NULL) {
			/* No need to check if the new class is an interface, since all
			 * interfaces have Object as their superclass, which will not
			 * incorrectly implement any interfaces.
			 */
			iTable->interfaceClass->instanceDescription = (UDATA *)1;
			iTable = iTable->next;
		}			
	}

	*markedInterfaceCount = foundInterfaces;
	*inheritedInterfaceCount = localInheritedInterfaceCount;
	*maxInterfaceDepth = localMaxInterfaceDepth;
	return interfaceHead;
}

static void
unmarkInterfaces(J9Class *interfaceHead)
{
	while (interfaceHead != NULL) {
		J9Class *nextInterface = (J9Class *)((UDATA)interfaceHead->instanceDescription & ~INTERFACE_TAG);
		interfaceHead->instanceDescription = (UDATA *)1;
		interfaceHead = nextInterface;
	}
}

static void
createITable(J9VMThread* vmStruct, J9Class *ramClass, J9Class *interfaceClass, J9ITable ***previousLink, UDATA **currentSlot, UDATA depth)
{
	/* Fill in the iTable header and link it into the list. */
	J9ITable *iTable = (J9ITable *)(*currentSlot);
	**previousLink = iTable;
	*previousLink = &iTable->next;
	iTable->interfaceClass = interfaceClass;
	iTable->depth = depth;
	*currentSlot = (UDATA *)((UDATA)(*currentSlot) + sizeof(J9ITable));

	/* If the newly-built class is not an interface class, fill in the iTable. */
	if (J9_JAVA_INTERFACE != (ramClass->romClass->modifiers & J9_JAVA_INTERFACE)) {
		J9ROMClass *interfaceRomClass = interfaceClass->romClass;
		UDATA count = interfaceRomClass->romMethodCount;
		if (count != 0) {
			UDATA *vTable = (UDATA *)(ramClass + 1);
			UDATA vTableSize = *vTable;
			J9Method *interfaceRamMethod = interfaceClass->ramMethods;
			while (count-- > 0) {
				J9ROMMethod *interfaceRomMethod = J9_ROM_METHOD_FROM_RAM_METHOD(interfaceRamMethod);
				J9UTF8 *interfaceMethodName = J9ROMMETHOD_NAME(interfaceRomMethod);
				J9UTF8 *interfaceMethodSig = J9ROMMETHOD_SIGNATURE(interfaceRomMethod);
				UDATA vTableIndex = 0;
				UDATA searchIndex = 2;
				
				/* Search the vTable for a public method of the correct name. */
				while (searchIndex <= vTableSize) {
					J9Method *vTableRamMethod = (J9Method *)vTable[searchIndex];
					J9ROMMethod *vTableRomMethod = J9_ROM_METHOD_FROM_RAM_METHOD(vTableRamMethod);
					J9UTF8 *vTableMethodName = J9ROMMETHOD_NAME(vTableRomMethod);
					J9UTF8 *vTableMethodSig = J9ROMMETHOD_SIGNATURE(vTableRomMethod);

					if (vTableRomMethod->modifiers & J9_JAVA_PUBLIC) {
						if ((J9UTF8_LENGTH(interfaceMethodName) == J9UTF8_LENGTH(vTableMethodName))
						&& (J9UTF8_LENGTH(interfaceMethodSig) == J9UTF8_LENGTH(vTableMethodSig))
						&& (memcmp(J9UTF8_DATA(interfaceMethodName), J9UTF8_DATA(vTableMethodName), J9UTF8_LENGTH(vTableMethodName)) == 0)
						&& (memcmp(J9UTF8_DATA(interfaceMethodSig), J9UTF8_DATA(vTableMethodSig), J9UTF8_LENGTH(vTableMethodSig)) == 0)
						) {
							vTableIndex = (searchIndex * sizeof(UDATA))	+ sizeof(J9Class);
							break;
						}
					}
					searchIndex++;
				}

#if defined(J9VM_TRACE_ITABLE)
				{
					PORT_ACCESS_FROM_VMC(vmStruct);
					j9tty_printf(PORTLIB, "\n  map %.*s%.*s to vTableIndex=%d (%d)", J9UTF8_LENGTH(interfaceMethodName),
							J9UTF8_DATA(interfaceMethodName), J9UTF8_LENGTH(interfaceMethodSig), J9UTF8_DATA(interfaceMethodSig), vTableIndex, searchIndex);
				}
#endif

				/* fill in interface index --> vTableIndex mapping */
				**currentSlot = vTableIndex;
				(*currentSlot)++;

				interfaceRamMethod++;
			}
		}
	}
}


static UDATA *
initializeRAMClassITable (J9VMThread* vmStruct, J9Class *ramClass, J9Class *superclass, UDATA* currentSlot, J9Class *interfaceHead, IDATA maxInterfaceDepth)
{
	J9Class *booleanArrayClass;
	J9ROMClass *romClass = ramClass->romClass;
	
#if defined(J9VM_TRACE_ITABLE)
	{
		PORT_ACCESS_FROM_VMC(vmStruct);
		J9UTF8 *className = J9ROMCLASS_CLASSNAME(romClass);
		j9tty_printf(PORTLIB, "\n<initializeRAMClassITable: %.*s %d>", J9UTF8_LENGTH(className), J9UTF8_DATA(className), (romClass->modifiers & J9_JAVA_INTERFACE));
	}
#endif

	/* All array classes share the same iTable.  If this is an array class and [Z (the first allocated
	 * array class) has been filled in, copy the iTable from [Z.
	 */
	booleanArrayClass = vmStruct->javaVM->booleanArrayClass;
	if (J9ROMCLASS_IS_ARRAY(romClass) && (booleanArrayClass != NULL)) {
		ramClass->iTable = booleanArrayClass->iTable;
		if ((J9CLASS_FLAGS(booleanArrayClass) & J9_JAVA_CLASS_CLONEABLE) == J9_JAVA_CLASS_CLONEABLE) {
			ramClass->classDepthAndFlags |= J9_JAVA_CLASS_CLONEABLE;
		}
		unmarkInterfaces(interfaceHead);
	} else {
		J9ITable *superclassInterfaces = NULL;
		J9ITable **previousLink;

		if (superclass != NULL) {
			superclassInterfaces = (J9ITable *)superclass->iTable;
		}

		/* Create the iTables. Interface classes must add themselves to their iTables. */
		previousLink = (J9ITable **)&ramClass->iTable;
		if ((romClass->modifiers & J9_JAVA_INTERFACE) == J9_JAVA_INTERFACE) {
			createITable(vmStruct, ramClass, ramClass, &previousLink, &currentSlot, (UDATA)(maxInterfaceDepth + 1));
		}

		while (interfaceHead != NULL) {
			J9Class *nextInterface;
			createITable(vmStruct, ramClass, interfaceHead, &previousLink, &currentSlot, ((J9ITable*)interfaceHead->iTable)->depth);
			nextInterface = (J9Class *)((UDATA)interfaceHead->instanceDescription & ~INTERFACE_TAG);
			/* This is the last walk, so unmark the interfaces */
			interfaceHead->instanceDescription = (UDATA *)1;
			interfaceHead = nextInterface;
		}
		*previousLink = superclassInterfaces;
	}
	
	return currentSlot;
}

/* Helper function to compare two name and sigs.
 * It compares the lengths of both name and sig first before doing any memcmp.
 *
 * @param[in] name1 The first name
 * @param[in] sig1 The first signature
 * @param[in] name2 The second name
 * @param[in] sig2 The second signature
 * @return true iff the names and signtures are the same.
 */
static bool VMINLINE
areNamesAndSignaturesEqual(J9UTF8 *name1, J9UTF8 *sig1, J9UTF8 *name2, J9UTF8 *sig2)
{
	/* utf8 pointers may be equal due to utf sharing */
	return ((name1 == name2) && (sig1 == sig2))
	|| /* Do length checks first as they are less expensive than memcmp */
	(((J9UTF8_LENGTH(name1) == J9UTF8_LENGTH(name2))
	&& (J9UTF8_LENGTH(sig1) == J9UTF8_LENGTH(sig2))
	&& (0 == memcmp(J9UTF8_DATA(name1), J9UTF8_DATA(name2), J9UTF8_LENGTH(name2)))
	&& (0 == memcmp(J9UTF8_DATA(sig1), J9UTF8_DATA(sig2), J9UTF8_LENGTH(sig2)))));
}

typedef enum {
	SLOT_IS_ROM_METHOD,
	SLOT_IS_RAM_METHOD,
	SLOT_IS_INTERFACE_RAM_METHOD,
	SLOT_IS_CONFLICT_METHOD,
	SLOT_IS_CONFLICT_TAG,
	SLOT_IS_EQUIVSET_TAG,
	SLOT_IS_INVALID = 0x80000000 /* force wide enums */
}INTERFACE_STATE;



static UDATA
addInterfaceMethods(J9VMThread *vmStruct, J9ClassLoader *classLoader, J9Class *interfaceClass, UDATA vTableWriteIndex, UDATA *vTableAddress, J9Class *superclass, J9ROMClass *romClass, UDATA *defaultConflictCount, J9Pool *equivalentSets, UDATA *equivSetCount, J9OverrideErrorData *errorData)
{
	J9ROMClass *interfaceROMClass = interfaceClass->romClass;
	UDATA count = interfaceROMClass->romMethodCount;
	if (0 != count) {
		const void * conflictRunAddress = J9_BCLOOP_ENCODE_SEND_TARGET(J9_BCLOOP_SEND_TARGET_DEFAULT_CONFLICT);
		J9Method *interfaceMethod = interfaceClass->ramMethods;
		UDATA interfaceDepth = ((J9ITable *)interfaceClass->iTable)->depth;
		UDATA j = 0;
		
		for (j=0; j < count; j++) {
			J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(interfaceMethod);
			/* Ignore the <clinit> from the interface class. */
			if (J9_ARE_NO_BITS_SET(romMethod->modifiers, J9_JAVA_PRIVATE | J9_JAVA_STATIC)) {
				J9UTF8 *interfaceMethodNameUTF = J9ROMMETHOD_NAME(romMethod);
				J9UTF8 *interfaceMethodSigUTF = J9ROMMETHOD_SIGNATURE(romMethod);
				UDATA tempIndex = vTableWriteIndex;
				INTERFACE_STATE state = SLOT_IS_INVALID;
				
				/* If the vTable already has a public declaration of the method that isn't
				 * either an abstract interface method or default method conflict, do not
				 * process the interface method at all. */
				while (tempIndex > 1) {
					J9ROMClass *methodROMClass = NULL;
					J9Class *methodClass = NULL;
					J9ROMMethod *vTableMethod = (J9ROMMethod *)vTableAddress[tempIndex];

					if (ROM_METHOD_ID_TAG == ((UDATA)vTableMethod & VTABLE_SLOT_TAG_MASK)) {
						methodClass = NULL;
						methodROMClass = romClass;
						vTableMethod = (J9ROMMethod *)((UDATA)vTableMethod & ~ROM_METHOD_ID_TAG);
						state = SLOT_IS_ROM_METHOD;
					} else if (DEFAULT_CONFLICT_METHOD_ID_TAG == ((UDATA)vTableMethod & VTABLE_SLOT_TAG_MASK)) {
						J9Method *conflictMethod = (J9Method *)((UDATA)vTableMethod & ~DEFAULT_CONFLICT_METHOD_ID_TAG);
						methodClass = J9_CLASS_FROM_METHOD(conflictMethod);
						methodROMClass = methodClass->romClass;
						vTableMethod = J9_ROM_METHOD_FROM_RAM_METHOD(conflictMethod);
						state = SLOT_IS_CONFLICT_TAG;
					} else if (EQUIVALENT_SET_ID_TAG == ((UDATA)vTableMethod & VTABLE_SLOT_TAG_MASK)) {
						state = SLOT_IS_EQUIVSET_TAG;
						J9EquivalentEntry *entry = (J9EquivalentEntry*)((UDATA)vTableMethod & ~EQUIVALENT_SET_ID_TAG);
						methodClass = J9_CLASS_FROM_METHOD((J9Method *)entry->method);
						methodROMClass = methodClass->romClass;
						vTableMethod = J9_ROM_METHOD_FROM_RAM_METHOD(entry->method);
					} else {
						methodClass = J9_CLASS_FROM_METHOD((J9Method *)vTableMethod);
						methodROMClass = methodClass->romClass;
						vTableMethod = J9_ROM_METHOD_FROM_RAM_METHOD(vTableMethod);
						state = SLOT_IS_RAM_METHOD;
						if (conflictRunAddress == ((J9Method *)vTableMethod)->methodRunAddress) {
							state = SLOT_IS_CONFLICT_METHOD;
						} else if (J9ROMCLASS_IS_INTERFACE(methodROMClass)) {
							state = SLOT_IS_INTERFACE_RAM_METHOD;
						}
					}
					J9UTF8 *vTableMethodNameUTF = J9ROMMETHOD_NAME(vTableMethod);
					J9UTF8 *vTableMethodSigUTF = J9ROMMETHOD_SIGNATURE(vTableMethod);
					if (areNamesAndSignaturesEqual(interfaceMethodNameUTF, interfaceMethodSigUTF, vTableMethodNameUTF, vTableMethodSigUTF)) {
						switch(state) {
						case SLOT_IS_RAM_METHOD:
							/* Existing, non-interface method found - only need to do the public check */
							break;
						case SLOT_IS_ROM_METHOD:
							/* Found method defined in this class - only need to do the public check */
							break;
						case SLOT_IS_CONFLICT_METHOD:
							/* conflict detected when building the superclass vtable.  Replace with current interface method
							 * and allow the conflict to be re-detected if it hasn't been resolved by subsequent interfaces
							 */
							vTableAddress[tempIndex] = (UDATA)interfaceMethod;
							goto continueInterfaceScan;
						case SLOT_IS_INTERFACE_RAM_METHOD:
							/* Here we need to determine if this is a potential conflict or equivalent set */
							if (((J9ITable *)methodClass->iTable)->depth < interfaceDepth) {
								/* If the interface method is less deep then the candidate method, the interface method
								 * was filled in by the super class's vtable build.  Replace with the current method
								 * and continue.
								 */
								vTableAddress[tempIndex] = (UDATA)interfaceMethod;
								goto continueInterfaceScan;
							} else {
								/* Locally added interface method - look for either a merge, conflict or equivSet */
								if (isSameOrSuperInterfaceOf(interfaceClass, methodClass)) {
									/* Valid defender merge - do nothing as the current vtable method is
									 * the most *specific* one.
									 */
								} else {
									/* if either is abstract, convert to equivSet, else conflict */
									const UDATA combinedModifiers = J9_ROM_METHOD_FROM_RAM_METHOD(interfaceMethod)->modifiers | vTableMethod->modifiers;
									if (J9_ARE_ANY_BITS_SET(combinedModifiers, J9_JAVA_ABSTRACT)) {
										/* Convert to equivSet by adding the existing vtable method to the equivSet */
										J9EquivalentEntry *entry = (J9EquivalentEntry*) pool_newElement(equivalentSets);
										if (NULL == entry) {
											/* OOM will be thrown */
											goto fail;
										}
										entry->method = (J9Method *)vTableAddress[tempIndex];
										entry->next = NULL;
										vTableAddress[tempIndex] = (UDATA)entry | EQUIVALENT_SET_ID_TAG;
										/* This will either be the single default method or the first abstract method */
										/* ... and then adding the new method */
										*equivSetCount += 1;
										goto add_existing;
									} else {
										/* Conflict detected */
										vTableAddress[tempIndex] |= DEFAULT_CONFLICT_METHOD_ID_TAG;
										*defaultConflictCount += 1;
									}
								}
							}
							break;
						case SLOT_IS_EQUIVSET_TAG: {
add_existing:
							J9EquivalentEntry * existing_entry = (J9EquivalentEntry*)((UDATA)vTableAddress[tempIndex] & ~EQUIVALENT_SET_ID_TAG);
							J9EquivalentEntry * previous_entry = existing_entry;
							while (NULL != existing_entry) {
								if (isSameOrSuperInterfaceOf(interfaceClass, J9_CLASS_FROM_METHOD(existing_entry->method))) {
									/* Do nothing - this is already shadowed by an existing method */
									goto continueInterfaceScan;
								}
								previous_entry = existing_entry;
								existing_entry = existing_entry->next;
							}
							existing_entry = previous_entry;
							J9EquivalentEntry * new_entry = (J9EquivalentEntry*) pool_newElement(equivalentSets);
							if (NULL == new_entry) {
								/* OOM will be thrown */
								goto fail;
							}
							new_entry->method = interfaceMethod;
							new_entry->next = existing_entry->next;
							existing_entry->next = new_entry;
							goto continueInterfaceScan;
						}
						case SLOT_IS_CONFLICT_TAG:
							/* Already marked as a conflict found in *this* class */
							goto continueInterfaceScan;
						case SLOT_IS_INVALID:
						default:
							Assert_VM_unreachable();
							break;
						}
						if (J9_JAVA_PUBLIC == (vTableMethod->modifiers & J9_JAVA_PUBLIC)) {
							J9ClassLoader *interfaceLoader = interfaceClass->classLoader;
							J9ClassLoader *vTableMethodLoader = classLoader;
							if (NULL != methodClass) {
								vTableMethodLoader = methodClass->classLoader;
							}
							if (interfaceLoader != vTableMethodLoader) {
								if (0 != j9bcv_checkClassLoadingConstraintsForSignature(vmStruct, vTableMethodLoader, interfaceLoader, vTableMethodSigUTF, interfaceMethodSigUTF)) {
									J9UTF8 *vTableMethodClassNameUTF = J9ROMCLASS_CLASSNAME(romClass);
									if (NULL != methodClass) {
										vTableMethodClassNameUTF = J9ROMCLASS_CLASSNAME(methodClass->romClass);
									}
									J9UTF8 *interfaceClassNameUTF = J9ROMCLASS_CLASSNAME(interfaceClass->romClass);
									/* LinkageError will be thrown */
									errorData->loader1 = vTableMethodLoader;
									errorData->class1NameUTF = vTableMethodClassNameUTF;
									errorData->loader2 = interfaceLoader;
									errorData->class2NameUTF = interfaceClassNameUTF;
									errorData->exceptionClassNameUTF = interfaceClassNameUTF;
									errorData->methodNameUTF = vTableMethodNameUTF;
									errorData->methodSigUTF = vTableMethodSigUTF;
									goto fail;
								}

							}
							goto continueInterfaceScan;
						}
					}
					tempIndex -= 1;
				}
				/* Add the interface method as the Local class does not implement a public method of the given name. */
				vTableWriteIndex = growNewVTableSlot(vTableAddress, vTableWriteIndex, interfaceMethod);
#if defined(J9VM_TRACE_VTABLE_ACCESS)
				{
					PORT_ACCESS_FROM_VMC(vmStruct);
					J9UTF8 *classNameUTF = J9ROMCLASS_CLASSNAME(romClass);
					j9tty_printf(PORTLIB, "\n<vtbl_init: adding @ index=%d %.*s.%.*s%.*s (0x%x)>", 
								vTableWriteIndex, 
								J9UTF8_LENGTH(classNameUTF), J9UTF8_DATA(classNameUTF),
								J9UTF8_LENGTH(interfaceMethodNameUTF), J9UTF8_DATA(interfaceMethodNameUTF),
								J9UTF8_LENGTH(interfaceMethodSigUTF), J9UTF8_DATA(interfaceMethodSigUTF),
								romMethod);
				}
#endif
			}
continueInterfaceScan:
			interfaceMethod++;
		}
	}
done:
	return vTableWriteIndex;
fail:
	vTableWriteIndex = 0;
	goto done;
}

/*
 * Compare two classes based on the itable->depth field.
 * Results in shallowest -> deepest ordering.
 * Return
 * 			0 if the depths are the same
 * 			- if a->depth < b->depth
 * 			+ if a->depth > b->depth
 */
static I_32
interfaceDepthCompare(const void *a, const void *b)
{
	J9ITable *iA = (J9ITable *)(*(J9Class **)a)->iTable;
	J9ITable *iB = (J9ITable *)(*(J9Class **)b)->iTable;
	return (I_32)(iA->depth - iB->depth);
}

/*
 * Process the "equivalent sets" stored in the vtable slots.  The sets
 * will contain either:
 * 		A) All abstract interface methods
 * 		B) A single default method and 1+ abstract interface methods
 * 		C) Multiple default methods and 1+ abstract interface methods
 * The equivalent set will be replaced by:
 * 		A) One of the abstract interface methods so that AME will be thrown when invoked
 * 		B) The single default method
 * 		C) Converted to a 'default conflict method'
 * @param[in] vmStruct The J9VMThread
 * @param[in,out] defaultConflictCount The count of conflict methods.  Possibly updated
 * @param[in] vTableWriteIndex The last written vtable index
 * @param[in] vTableAddress The vtable itself
 */
static void
processEquivalentSets(J9VMThread *vmStruct, UDATA *defaultConflictCount, UDATA vTableWriteIndex, UDATA *vTableAddress)
{
	for (UDATA i = 1; i <= vTableWriteIndex; i++) {
		UDATA slotValue = vTableAddress[i];

		if (EQUIVALENT_SET_ID_TAG == (slotValue & VTABLE_SLOT_TAG_MASK)) {
			/* Process set and determine if there is a method that satisifies or if this is a conflict */
			J9EquivalentEntry *entry = (J9EquivalentEntry*)(slotValue & ~EQUIVALENT_SET_ID_TAG);
			J9Method *candidate = entry->method;
			bool foundNonAbstract = false;
			while (NULL != entry) {
				if (J9_ARE_NO_BITS_SET(J9_ROM_METHOD_FROM_RAM_METHOD(entry->method)->modifiers, J9_JAVA_ABSTRACT)) {
					if (foundNonAbstract) {
						/* Two non-abstract methods -> conflict */
						vTableAddress[i] = ((UDATA)entry->method | DEFAULT_CONFLICT_METHOD_ID_TAG);
						*defaultConflictCount += 1;
						goto continueProcessingVTable;
					}
					foundNonAbstract = true;
					candidate = entry->method;
				}
				entry = entry->next;
			}
			/* This will either be the single default method or the first abstract method */
			vTableAddress[i] = (UDATA)candidate;
		}

continueProcessingVTable: ;
	}
}

/**
 * Computes the virtual function dispatch table from the specified
 * superclass and ROM information.  Only virtual methods (not constructors) appear
 * in the vtable -- the vtable inherits the number of slots from the superclass and
 * adds a single slot for every method defined at this level.
 *
 * Return the malloc'ed vTable, or the ramClass vTable in the case of hotswap vTable rebuild.
 *
 * The caller must hold the class table mutex or exclusive access.
 */
static UDATA *
computeVTable(J9VMThread *vmStruct, J9ClassLoader *classLoader, J9Class *superclass, J9ROMClass *taggedClass, UDATA packageID, J9ROMMethod ** methodRemapArray, J9Class *interfaceHead, UDATA *defaultConflictCount, UDATA interfaceCount, UDATA inheritedInterfaceCount, J9OverrideErrorData *errorData)
{
	J9JavaVM *vm = vmStruct->javaVM;
	J9ROMClass *romClass = taggedClass;
	UDATA maxSlots;
	UDATA *vTableAddress = NULL;
	bool vTableAllocated = false;

	PORT_ACCESS_FROM_VMC(vmStruct);

#if defined(J9VM_INTERP_HOT_CODE_REPLACEMENT)
	if (((UDATA)taggedClass & ROM_METHOD_ID_TAG) == ROM_METHOD_ID_TAG) {
		taggedClass = (J9ROMClass *)((UDATA)taggedClass - 1);
		romClass = ((J9Class *)taggedClass)->romClass;
	}
#endif
	
#if defined(J9VM_TRACE_VTABLE_ACCESS)
	{
		J9UTF8 *className = J9ROMCLASS_CLASSNAME(romClass);
		j9tty_printf(PORTLIB, "\n<computeVTable: %.*s>", J9UTF8_LENGTH(className), J9UTF8_DATA(className));
	}
#endif
	
	vmStruct->tempSlot = 0;

	/* Compute the absolute maximum size of the vTable and allocate it. */
	
	if ((romClass->modifiers & J9_JAVA_INTERFACE) == J9_JAVA_INTERFACE) {
		maxSlots = 1;
	} else {
		/* All methods in the current class might need new slots in the vTable. */
		maxSlots = romClass->romMethodCount;
		
		/* Add in all slots from the superclass. */
		if (superclass == NULL) {
			/* reserved method slot */
			maxSlots += 1;
		} else {
			UDATA *superVTable = (UDATA *)(superclass + 1);
			maxSlots += *superVTable;
		}
		
		/* size field */
		maxSlots += 1;

		/* For non-array classes, compute the total possible size of implementing all interface methods. */
		if (J9ROMCLASS_IS_ARRAY(romClass) == 0) {
			J9Class *interfaceWalk = interfaceHead;
			while (interfaceWalk != NULL) {
				/* TODO: this over-estimates due to private + static methods. */
				maxSlots += interfaceWalk->romClass->romMethodCount;
				interfaceWalk = (J9Class *)((UDATA)interfaceWalk->instanceDescription & ~INTERFACE_TAG);
			}
		}
	}

	/* convert slots to bytes */
	maxSlots *= sizeof(UDATA);
	
#if defined(J9VM_INTERP_HOT_CODE_REPLACEMENT)
	if (taggedClass != romClass) {
		vTableAddress = (UDATA *)((J9Class *)taggedClass + 1);
	}
#endif
	
	if (NULL == vTableAddress) {
		if (maxSlots > vm->vTableScratchSize) {
			vTableAddress = (UDATA *)j9mem_allocate_memory(maxSlots, J9MEM_CATEGORY_CLASSES);
			vTableAllocated = true;
		} else {
			vTableAddress = vm->vTableScratch;
		}
	}

	if (NULL != vTableAddress) {
		UDATA vTableWriteIndex = 0;

		if (J9_JAVA_INTERFACE == (romClass->modifiers & J9_JAVA_INTERFACE)) {
			*vTableAddress = 0;
			goto done;
		}

		if (superclass == NULL) {
			/* no inherited slots, 1 default slot */
			vTableWriteIndex = 1;
			vTableAddress[1] = (UDATA)vm->initialMethods.initialVirtualMethod;
		} else {
			UDATA *superVTable = (UDATA *)(superclass + 1);
			vTableWriteIndex = *superVTable;
			/* + 1 to account for size slot */
			memcpy(vTableAddress, superVTable, (vTableWriteIndex + 1) * sizeof(UDATA));
		}
		
		if (J9ROMCLASS_IS_ARRAY(romClass) == 0) {
			J9ROMMethod *romMethod = NULL;
			UDATA romMethodIndex = 0;
			UDATA count = romClass->romMethodCount;
			
			/* Walk over ROM Methods. If the methodRemapArray is supplied, use the array as a source of
			 * J9ROMMethods instead of romClass->romMethods.  The methodRemapArray is specified by 
			 * HCR to ensure that the replacement class vtable has the same method order as the original class
			 */
			if (methodRemapArray == NULL) {
				romMethod = J9ROMCLASS_ROMMETHODS(romClass);
			} else {
				romMethod = methodRemapArray[romMethodIndex];
			}
			
			if (count != 0) {
				UDATA i;
				for (i=count; i>0; i--) {
					J9UTF8* methodName = J9ROMMETHOD_NAME(romMethod);
					/* Check for '<' to exclude <init> from being processed */
					if (J9_ARE_ANY_BITS_SET(romMethod->modifiers, J9AccMethodVTable | J9_JAVA_PRIVATE)
					&& J9_ARE_NO_BITS_SET(romMethod->modifiers, J9_JAVA_STATIC)
					&& ('<' != J9UTF8_DATA(methodName)[0])
					) {
						vTableWriteIndex = processVTableMethod(vmStruct, classLoader, vTableAddress, superclass, romClass, romMethod,
								packageID, vTableWriteIndex, (J9ROMMethod *)((UDATA)romMethod + ROM_METHOD_ID_TAG), errorData);
						if (0 == vTableWriteIndex) {
							goto fail;
						}
					}
					if (i > 1) {
						if (methodRemapArray == NULL) {
							romMethod = nextROMMethod(romMethod);
						} else {
							romMethodIndex++;
							romMethod = methodRemapArray[romMethodIndex];
						}
					}
				}
			}
			/* TODO: optimize this so only done if the class's interfaces have defenders? */
			if (interfaceCount > 0) {
				UDATA totalInterfaces = interfaceCount + inheritedInterfaceCount;
				J9Class *interfaceWalk = interfaceHead;
				J9Class *localBuffer[LOCAL_INTERFACE_ARRAY_SIZE];
				J9Class **interfaces = NULL;
				J9Class **iterator = NULL;
				UDATA i = 0;

				if (totalInterfaces <= LOCAL_INTERFACE_ARRAY_SIZE) {
					memset(localBuffer, 0, sizeof(localBuffer));
					interfaces = localBuffer;
				} else {
					/* No memset required as interfaces will be completely initialized */
					interfaces = (J9Class **)j9mem_allocate_memory(totalInterfaces * sizeof(J9Class *), J9MEM_CATEGORY_CLASSES);
					if (NULL == interfaces) {
						goto fail;
					}
				}

				iterator = interfaces;
				/* Add the local interfaces to interfaces[] */
				while (NULL != interfaceWalk) {
					*iterator = interfaceWalk;
					iterator += 1;
					interfaceWalk = (J9Class *)((UDATA)interfaceWalk->instanceDescription & ~INTERFACE_TAG);
				}

				/* Add the superclass's interfaces to interfaces[] */

				if (NULL != superclass) {
					J9ITable *iTableIter = (J9ITable *)superclass->iTable;
					while (NULL != iTableIter) {
						*iterator = iTableIter->interfaceClass;
						iTableIter = iTableIter->next;
						iterator += 1;
					}
				}

				J9_SORT(interfaces, totalInterfaces, sizeof(J9Class *), &interfaceDepthCompare);
#if defined(VERBOSE_INTERFACE_METHODS)
				{
					J9UTF8 *className = J9ROMCLASS_CLASSNAME(romClass);
					j9tty_printf(PORTLIB, "\nAdding interfaceMethods for class <%.*s> using interface[%i]=%p.  #Local=%i\t#Inherited=", J9UTF8_LENGTH(className), J9UTF8_DATA(className), totalInterfaces, interfaces, interfaceCount, inheritedInterfaceCount);
				}
#endif /* VERBOSE_INTERFACE_METHODS */
				J9Pool *equivalentSet = pool_new(sizeof(J9EquivalentEntry),  0, 0, 0, J9_GET_CALLSITE(), J9MEM_CATEGORY_CLASSES, POOL_FOR_PORT(vm->portLibrary));
				if (NULL == equivalentSet) {
					/* OOM will be thrown */
					goto fail;
				}
				UDATA equivSetCount = 0;
				for (i = totalInterfaces; i > 0; i--) {
#if defined(VERBOSE_INTERFACE_METHODS)
					{
						J9UTF8 *className = J9ROMCLASS_CLASSNAME(interfaces[i - 1]->romClass);
						j9tty_printf(PORTLIB, "\n\t<%.*s>", J9UTF8_LENGTH(className), J9UTF8_DATA(className));
					}
#endif /* VERBOSE_INTERFACE_METHODS */
					vTableWriteIndex = addInterfaceMethods(vmStruct, classLoader, interfaces[i - 1], vTableWriteIndex, vTableAddress, superclass, romClass, defaultConflictCount, equivalentSet, &equivSetCount, errorData);
					if (0 == vTableWriteIndex) {
						if (NULL != equivalentSet) {
							pool_kill(equivalentSet);
						}
						if (interfaces != localBuffer) {
							j9mem_free_memory(interfaces);
						}
						goto fail;
					}
				}
				if (equivSetCount > 0) {
					processEquivalentSets(vmStruct, defaultConflictCount, vTableWriteIndex, vTableAddress);
				}
				if (NULL != equivalentSet) {
					pool_kill(equivalentSet);
				}
				if (interfaces != localBuffer) {
					j9mem_free_memory(interfaces);
				}
			}
			
			/* record number of slots used */
			*vTableAddress = vTableWriteIndex;
		}
	}
	
done:
	return vTableAddress;
fail:
	if (vTableAllocated) {
		j9mem_free_memory(vTableAddress);
	}
	vTableAddress = NULL;
	goto done;
}


/**
 * Copy the vTable, converting local ROM method to their RAM equivalents.
 */
static void
copyVTable(J9VMThread *vmStruct, J9Class *ramClass, J9Class *superclass, UDATA *vTable, UDATA defaultConflictCount)
{
	UDATA superCount;
	UDATA count;
	UDATA *vTableAddress;
	UDATA index;
	J9Method *ramMethods = ramClass->ramMethods;
#if defined(J9VM_INTERP_NATIVE_SUPPORT)
	J9JITConfig *jitConfig;
#endif
	/* Point to the first J9Method to use for default method conflicts */
	J9Method *conflictMethodPtr = ramMethods + ramClass->romClass->romMethodCount;
	PORT_ACCESS_FROM_VMC(vmStruct);

#if defined(J9VM_TRACE_VTABLE_ACCESS)
	{
		J9UTF8 *className = J9ROMCLASS_CLASSNAME(ramClass->romClass);
		j9tty_printf(PORTLIB, "\n<initializeRAMClassVTable: %.*s>", J9UTF8_LENGTH(className), J9UTF8_DATA(className));
	}
#endif
	
	/* skip the virtualMethodResolve pseudo-method */
	superCount = 1;
	if (superclass != NULL) {
		/* add superclass vtable size */
		superCount += *((UDATA *)(superclass + 1));
	}
	
	count = *vTable;
	vTableAddress = (UDATA *)(ramClass + 1);
	*vTableAddress = count;
	/* start at 1 to skip the size field */
	for (index = 1; index <= count; index++) {
		J9Method *vTableMethod = (J9Method *)vTable[index];
		UDATA temp = (UDATA)vTableMethod;
		
		if (ROM_METHOD_ID_TAG == (temp & VTABLE_SLOT_TAG_MASK)) {
			/* convert vTable method to RAM method */
#if defined(J9VM_INTERP_HOT_CODE_REPLACEMENT)
			J9Method *afterLastMethod;
			J9ROMMethod *romMethod;
#endif

			temp &= ~ROM_METHOD_ID_TAG;
			vTableMethod = ramMethods;
#if defined(J9VM_INTERP_HOT_CODE_REPLACEMENT)
			/* calculate the pointer to the after-last method */
			afterLastMethod = &ramClass->ramMethods[ramClass->romClass->romMethodCount];
			while (vTableMethod != afterLastMethod) {
				if ((J9ROMMethod *)temp == J9_ROM_METHOD_FROM_RAM_METHOD(vTableMethod)) goto found;
				vTableMethod++;
			}
			/* could not find it. It must be a breakpointed method */
			vTableMethod = ramClass->ramMethods;
			romMethod = J9ROMCLASS_ROMMETHODS(ramClass->romClass);
			while ((J9ROMMethod *)temp != romMethod) {
				vTableMethod++;
				romMethod = nextROMMethod(romMethod);
			}
found:
			;
#else
			while ((J9ROMMethod *)temp != J9_ROM_METHOD_FROM_RAM_METHOD(vTableMethod)) {
				vTableMethod += 1;
			}
#endif
		} else if (DEFAULT_CONFLICT_METHOD_ID_TAG == (temp & VTABLE_SLOT_TAG_MASK)) {
			/* Default Method conflict */
			J9Method *defaultMethod = (J9Method*)(temp & ~DEFAULT_CONFLICT_METHOD_ID_TAG);
			conflictMethodPtr->bytecodes = (U_8*)(J9_ROM_METHOD_FROM_RAM_METHOD(defaultMethod) + 1);
			conflictMethodPtr->constantPool = (J9ConstantPool *)ramClass->ramConstantPool;
			conflictMethodPtr->methodRunAddress = J9_BCLOOP_ENCODE_SEND_TARGET(J9_BCLOOP_SEND_TARGET_DEFAULT_CONFLICT);
			conflictMethodPtr->extra = (void *)((UDATA)defaultMethod | J9_STARTPC_NOT_TRANSLATED);
			vTableMethod = conflictMethodPtr;
			conflictMethodPtr++;
		}
		
		vTableAddress[index] = (UDATA)vTableMethod;
		/* once we've walked the inherited entries, we can optimize the ramMethod search
		 * by remembering where the search ended last time. Since the methods occur in
		 * order in the VTable, we can start searching at the previous method
		 */
		if (index > superCount) {
			ramMethods = vTableMethod;
		}
	}
	
	/* Fill in the JIT vTable */
#if defined(J9VM_INTERP_NATIVE_SUPPORT)
	jitConfig = vmStruct->javaVM->jitConfig;
	if (jitConfig != NULL) {
		UDATA *vTableWriteCursor = &((UDATA *)ramClass)[-1];
		UDATA vTableWriteIndex = *vTableAddress;
		UDATA *vTableReadCursor;
		if (vTableWriteIndex != 0) {
			/* do not copy in the default method */
			vTableWriteIndex--;
			if ((jitConfig->runtimeFlags & J9JIT_TOSS_CODE) != 0) {
				vTableWriteCursor -= vTableWriteIndex;
			} else {
				UDATA superVTableSize;
				UDATA *superVTableReadCursor;
				UDATA *superVTableWriteCursor = &((UDATA *)superclass)[-1];
				if (superclass == NULL) {
					superVTableReadCursor = NULL;
					superVTableSize = 0;
				} else {
					superVTableReadCursor = (UDATA *)(superclass + 1);
					superVTableSize = *superVTableReadCursor;
					/* do not copy in the default method */
					superVTableSize--;
				}
				/* initialize pointer to first real vTable method */
				superVTableReadCursor = &superVTableReadCursor[2];
				/* initialize pointer to first real vTable method */
				vTableReadCursor = &vTableAddress[2];
				for (; vTableWriteIndex > 0; vTableWriteIndex--) {
					J9Method *currentMethod = (J9Method *)*vTableReadCursor++;
					superVTableWriteCursor--;
					if (superclass != NULL && currentMethod == (J9Method *)*superVTableReadCursor) {
						*--vTableWriteCursor = *superVTableWriteCursor;
					} else {
						fillJITVTableSlot(vmStruct, --vTableWriteCursor, currentMethod);
					}

					/* Always consume an entry from the super vTable.  Note that once the size hits zero and superclass becomes NULL,
					 * superVTableReadCursor will never be dereferenced again, so it's value does not matter.  Also, there's no possibilty
					 * of superVTableSize rolling over again (once it's decremented beyond 0), and it wouldn't matter if it did, since
					 * superclass will already be NULL.
					 */
					superVTableSize--;
					if (superVTableSize == 0) {
						superclass = NULL;
					}
					superVTableReadCursor++;
				}
			}
			vTableWriteCursor--;
		}
		
		/* The SRP to the start of the RAM class is written by internalAllocateRAMClass() */
	}
#endif
	
#if defined(J9VM_INTERP_HOT_CODE_REPLACEMENT)
	if (vTable != vTableAddress) {
		if (vTable != vmStruct->javaVM->vTableScratch) {
			j9mem_free_memory(vTable);
		}
	}
#else 
	if (vTable != vmStruct->javaVM->vTableScratch) {
		j9mem_free_memory(vTable);
	}
#endif
}

#if defined(J9VM_INTERP_NATIVE_SUPPORT)
void
fillJITVTableSlot(J9VMThread *vmStruct, UDATA *currentSlot, J9Method *currentMethod)
{
	J9JITConfig *jitConfig = vmStruct->javaVM->jitConfig;
	UDATA frameBuilder;
	if (((UDATA)currentMethod->extra & J9_STARTPC_NOT_TRANSLATED) != J9_STARTPC_NOT_TRANSLATED) {
		frameBuilder = (UDATA)currentMethod->extra;
#if defined(J9SW_PARAMETERS_IN_REGISTERS)
		/* Add the interpreter preprologue size to get to the JIT->JIT address */
		frameBuilder += (((U_32 *)frameBuilder)[-1] >> 16);
#endif
	} else {
#if defined(J9SW_NEEDS_JIT_2_INTERP_THUNKS)
		frameBuilder = (UDATA)jitConfig->patchupVirtual;
#else
		/* choose which stack frame builder/destroyer to use */
		J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(currentMethod);
		UDATA *sendTargetTable = (UDATA *)jitConfig->jitSendTargetTable;
		if ((romMethod->modifiers & (J9AccNative | J9AccAbstract)) != 0) {
			/* fetch the address of the native target (known to be at the end of the table) */
			frameBuilder = sendTargetTable[J9_JIT_RETURN_TYPES_SIZE * 2];
		} else {
			UDATA returnType;
			/* parse return type to find table offset */
			J9UTF8 *sig = J9ROMMETHOD_SIGNATURE(romMethod);
			if (J9UTF8_DATA(sig)[J9UTF8_LENGTH(sig) - 2] == ')') {
				/* the last character in the signature is a primitive return type */
				switch (J9UTF8_DATA(sig)[J9UTF8_LENGTH(sig) - 1]) {
					case 'D':
						returnType = J9AccMethodReturnD;
						break;
					case 'F':
						returnType = J9AccMethodReturnF;
						break;
					case 'J':
						returnType = J9AccMethodReturn2;
						break;
					case 'V':
						returnType = J9AccMethodReturn0;
						break;
					default:
						/* default case handles BCISZ */
						returnType = J9AccMethodReturn1;
				}
			} else {
				/* handles [L. if the second last character is not ), its not a
				 * primitive return type. Either an array or other object. */
				returnType = J9AccMethodReturn1;
			}
			returnType = (returnType & J9AccMethodReturnMask) >> J9AccMethodReturnShift;
			
			frameBuilder = sendTargetTable[returnType];
		}
#endif
	}
	*currentSlot = frameBuilder;
}
#endif

static UDATA
processVTableMethod(J9VMThread *vmThread, J9ClassLoader *classLoader, UDATA *vTableAddress, J9Class *superclass, J9ROMClass *romClass, J9ROMMethod *romMethod, UDATA localPackageID, UDATA vTableWriteIndex, void *storeValue, J9OverrideErrorData *errorData)
{
	BOOLEAN anyOverrides = FALSE;
	J9UTF8 *nameUTF = J9ROMMETHOD_NAME(romMethod);
	J9UTF8 *sigUTF = J9ROMMETHOD_SIGNATURE(romMethod);
	BOOLEAN isPrivate = (J9_JAVA_PRIVATE == (romMethod->modifiers & J9_JAVA_PRIVATE));

	if (!isPrivate && (NULL != superclass)) { /* Java 8 JVMS 5.4.5: ACC_PRIVATE methods do not override superclass methods */
		UDATA *superclassVTable;
		UDATA superclassVTableIndex;

		if (methodIsFinalInObject(J9UTF8_LENGTH(nameUTF), J9UTF8_DATA(nameUTF), J9UTF8_LENGTH(sigUTF), J9UTF8_DATA(sigUTF))) {
			vmThread->tempSlot = (UDATA)romMethod;
		}
		
		superclassVTable = (UDATA *)(superclass + 1);
		superclassVTableIndex = *superclassVTable;

		/* See if this method overrides any methods from the superclass. */
		while ((superclassVTableIndex = getVTableIndexForNameAndSigStartingAt(superclassVTable, nameUTF, sigUTF,
				superclassVTableIndex)) != 0)
		{
			/* fetch vTable entry */
			J9Method *superclassVTableMethod = (J9Method *)superclassVTable[superclassVTableIndex];
			J9Class *superclassVTableMethodClass = J9_CLASS_FROM_METHOD(superclassVTableMethod);
			J9ROMMethod *superclassVTableROMMethod = J9_ROM_METHOD_FROM_RAM_METHOD(superclassVTableMethod);
				/* Assumes that the new method is at least as visible as the superclass method.
				 * The subclass method overrides the superclass method if the superclass method
				 * could run in this class.
				 */
				UDATA modifiers = superclassVTableROMMethod->modifiers;
				/* Private methods are not overridden. */
				if ((modifiers & J9_JAVA_PRIVATE) != J9_JAVA_PRIVATE) {
					/* Protected and public are always overridden. */
					if (((modifiers & (J9_JAVA_PROTECTED | J9_JAVA_PUBLIC)) != 0) ||
							/* Default is overridden by any method in the same package. */
							(superclassVTableMethodClass->packageID == localPackageID))
					{
						if ((((UDATA)storeValue & VTABLE_SLOT_TAG_MASK) == ROM_METHOD_ID_TAG)
						|| (vTableAddress[superclassVTableIndex] == (UDATA)superclassVTableMethod)
						) {
							anyOverrides = TRUE;
							if ((modifiers & J9_JAVA_FINAL) == J9_JAVA_FINAL) {
								vmThread->tempSlot = (UDATA)romMethod;
							}
							/* fill in vtable, override parent slot */
							if (!isPrivate) {
								J9ClassLoader *superclassVTableMethodLoader = superclassVTableMethodClass->classLoader;
								if (superclassVTableMethodLoader != classLoader) {
									J9UTF8 *superclassVTableMethodSigUTF = J9ROMMETHOD_SIGNATURE(superclassVTableROMMethod);
									if (0 != j9bcv_checkClassLoadingConstraintsForSignature(vmThread, classLoader, superclassVTableMethodLoader, sigUTF, superclassVTableMethodSigUTF)) {
										J9UTF8 *superclassVTableMethodClassNameUTF = J9ROMCLASS_CLASSNAME(superclassVTableMethodClass->romClass);
										J9UTF8 *newClassNameUTF = J9ROMCLASS_CLASSNAME(romClass);
										J9UTF8 *superclassVTableMethodNameUTF = J9ROMMETHOD_NAME(superclassVTableROMMethod);
										errorData->loader1 = classLoader;
										errorData->class1NameUTF = newClassNameUTF;
										errorData->loader2 = superclassVTableMethodLoader;
										errorData->class2NameUTF = superclassVTableMethodClassNameUTF;
										errorData->exceptionClassNameUTF = superclassVTableMethodClassNameUTF;
										errorData->methodNameUTF = superclassVTableMethodNameUTF;
										errorData->methodSigUTF = superclassVTableMethodSigUTF;
										vTableWriteIndex = 0;
										goto done;
									}

								}
								vTableAddress[superclassVTableIndex] = (UDATA)storeValue;
							}
	
#if defined(J9VM_TRACE_VTABLE_ACCESS)
							{
								PORT_ACCESS_FROM_VMC(vmThread);
								J9UTF8 *classNameUTF = J9ROMCLASS_CLASSNAME(romClass);
							j9tty_printf(PORTLIB, "\noverriding @ index %d with %.*s.%.*s%.*s", superclassVTableIndex, J9UTF8_LENGTH(classNameUTF),
									J9UTF8_DATA(classNameUTF), J9UTF8_LENGTH(nameUTF), J9UTF8_DATA(nameUTF), J9UTF8_LENGTH(sigUTF), J9UTF8_DATA(sigUTF));
							}
						} else {
							if (ROM_METHOD_ID_TAG != ((UDATA)storeValue & VTABLE_SLOT_TAG_MASK)) {
								PORT_ACCESS_FROM_VMC(vmThread);
								J9UTF8 *classNameUTF = J9ROMCLASS_CLASSNAME(romClass);
								J9Method *method = (J9Method *)storeValue;
								J9Class *methodClass = ((J9ConstantPool *)((UDATA)method->constantPool & ~J9_STARTPC_STATUS))->ramClass;
								J9UTF8 *methodClassNameUTF = J9ROMCLASS_CLASSNAME(methodClass->romClass);
								J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
								J9UTF8 *nameUTF = J9ROMMETHOD_NAME(romMethod);
								J9UTF8 *sigUTF = J9ROMMETHOD_SIGNATURE(romMethod);
								j9tty_printf(PORTLIB, "\nnot overriding %.*s.%.*s%.*s in class %.*s - already overridden",
									J9UTF8_LENGTH(methodClassNameUTF), J9UTF8_DATA(methodClassNameUTF),
									J9UTF8_LENGTH(nameUTF), J9UTF8_DATA(nameUTF), J9UTF8_LENGTH(sigUTF), J9UTF8_DATA(sigUTF),
									J9UTF8_LENGTH(classNameUTF), J9UTF8_DATA(classNameUTF));
							}
#endif
						}
					}
				}
			/* Keep checking the rest of the methods in the superclass. */
			superclassVTableIndex--;
		}
	}

	/* If the method did not override any parent method, allocate a new slot for it. */
	if (!anyOverrides && !isPrivate) {
		/* allocate vTable slot */
		vTableWriteIndex = growNewVTableSlot(vTableAddress, vTableWriteIndex, storeValue);
#if defined(J9VM_TRACE_VTABLE_ACCESS)
		{
			PORT_ACCESS_FROM_VMC(vmThread);
			J9UTF8 *classNameUTF = J9ROMCLASS_CLASSNAME(romClass);
			j9tty_printf(PORTLIB, "\n<vtbl_init: adding @ index=%d %.*s.%.*s%.*s (0x%x)>", vTableWriteIndex, J9UTF8_LENGTH(classNameUTF),
					J9UTF8_DATA(classNameUTF), J9UTF8_LENGTH(nameUTF), J9UTF8_DATA(nameUTF), J9UTF8_LENGTH(sigUTF), J9UTF8_DATA(sigUTF), romMethod);
		}
#endif
	}
done:
	return vTableWriteIndex;
}

/**
 * Grow the VTable by 1 slot and add 'storeValue' to that slot.
 *
 * @param vTableAddress[in] A pointer to the buffer that holds the vtable (must be correctly sized)
 * @param vTableWriteIndex[in] The highest used index in the vtable
 * @param storeValue[in] the value to store into the vtable slot - either a J9Method* or a tagged J9ROMMethod
 * @return The new vTableWriteIndex
 */
static VMINLINE UDATA
growNewVTableSlot(UDATA *vTableAddress, UDATA vTableWriteIndex, void *storeValue)
{
	/* allocate vTable slot */
	vTableWriteIndex++;
	/* fill in vtable, add new entry */
	vTableAddress[vTableWriteIndex] = (UDATA)storeValue;
	return vTableWriteIndex;
}

static UDATA
getVTableIndexForNameAndSigStartingAt(UDATA *vTable, J9UTF8 *name, J9UTF8 *signature, UDATA vTableIndex)
{
	U_8 *nameData = J9UTF8_DATA(name);
	UDATA nameLength = J9UTF8_LENGTH(name);
	U_8 *signatureData = J9UTF8_DATA(signature);
	UDATA signatureLength = J9UTF8_LENGTH(signature);

	while (vTableIndex != 1) {
		/* fetch next vTable entry */
		J9Method *method = (J9Method *)vTable[vTableIndex];
		J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
		J9UTF8 *nameUTF = J9ROMMETHOD_NAME(romMethod);
		J9UTF8 *sigUTF = J9ROMMETHOD_SIGNATURE(romMethod);
		if ((nameLength == J9UTF8_LENGTH(nameUTF))
		&& (signatureLength == J9UTF8_LENGTH(sigUTF))
		&& (memcmp(J9UTF8_DATA(nameUTF), nameData, nameLength) == 0)
		&& (memcmp(J9UTF8_DATA(sigUTF), signatureData, signatureLength) == 0)
		) {
			return vTableIndex;
		}
		/* move to previous vTable index */
		vTableIndex--;
	}
	return 0;
}

UDATA
getITableIndexForMethod(J9Method * method)
{
	/* The iTableIndex is the same as the (ram/rom)method index. 
	 * This includes static and private methods - they just exist 
	 * as dead entries in the iTable.
	 * 
	 * Code below is the equivalent of doing:
	 *	for (; methodIndex < methodCount; methodIndex++) {
	 *		if (ramMethod == method) {
	 *				return methodIndex;
	 *		}
	 *		ramMethod++;
	 *	}
	 */
	J9Class *methodClass = J9_CLASS_FROM_METHOD(method);
	const UDATA methodCount = methodClass->romClass->romMethodCount;
	const UDATA methodIndex = method - methodClass->ramMethods;
	if (methodIndex < methodCount) {
		return methodIndex;
	}
	return -1;
}

UDATA
getVTableIndexForMethod(J9Method * method, J9Class *clazz, J9VMThread *vmThread)
{
	UDATA vTableIndex;
	J9Class *methodClass = J9_CLASS_FROM_METHOD(method);
	UDATA modifiers = methodClass->romClass->modifiers;

	/* If this method came from an interface class, handle it specially. */
	if ((modifiers & J9_JAVA_INTERFACE) == J9_JAVA_INTERFACE) {
		UDATA *vTable = (UDATA *)(clazz + 1);
		UDATA vTableSize = *vTable;
		J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
		J9UTF8 *nameUTF = J9ROMMETHOD_NAME(romMethod);
		J9UTF8 *sigUTF = J9ROMMETHOD_SIGNATURE(romMethod);
		if (vTableSize == 0) {
			return 0;
		} else {
			vTableIndex = getVTableIndexForNameAndSigStartingAt(vTable, nameUTF, sigUTF, vTableSize);
			if (vTableIndex != 0) {
				return (vTableIndex * sizeof(UDATA)) + sizeof(J9Class);
			}
		}
	} else {
		/* Iterate over the vtable from the end to the beginning, skipping 
		 * the "magic" first entry.  This ensures that the most "recent" override of the
		 * method is found first.  Critically important for super sends 
		 * ie: invokespecial) being correct
		 */
		UDATA *vTable = (UDATA *)(methodClass + 1);
		UDATA vTableSize = *vTable;
		for (vTableIndex = vTableSize; vTableIndex > 1; vTableIndex--) {
			if (method == (J9Method *)vTable[vTableIndex]) {
				return (vTableIndex * sizeof(UDATA)) + sizeof(J9Class);
			}
		}
	}
	return 0;
}

static UDATA
checkPackageAccess(J9VMThread *vmThread, J9Class *foundClass, UDATA classPreloadFlags)
{
	if ((classPreloadFlags & J9_FINDCLASS_FLAG_CHECK_PKG_ACCESS) == J9_FINDCLASS_FLAG_CHECK_PKG_ACCESS) {

		if (!packageAccessIsLegal(vmThread, foundClass, PEEK_OBJECT_IN_SPECIAL_FRAME(vmThread, 0), TRUE))
		{
			if ((classPreloadFlags & J9_FINDCLASS_FLAG_THROW_ON_FAIL) != J9_FINDCLASS_FLAG_THROW_ON_FAIL) {
				vmThread->currentException = NULL;
				vmThread->privateFlags &= ~J9_PRIVATE_FLAGS_REPORT_EXCEPTION_THROW;
			}
			return 1;
		}
	}
	return 0;
}

/**
 * Sets the current exception using the specified class name.
 */
static void
setCurrentExceptionForBadClass(J9VMThread *vmThread, J9UTF8 *badClassName, UDATA exceptionIndex)
{
	J9MemoryManagerFunctions *gcFuncs = vmThread->javaVM->memoryManagerFunctions;
	j9object_t detailMessage = gcFuncs->j9gc_createJavaLangString(vmThread, J9UTF8_DATA(badClassName), J9UTF8_LENGTH(badClassName), J9_STR_XLAT);

	setCurrentException(vmThread, exceptionIndex, (UDATA *)detailMessage);
}

/**
 * Verifies the class loading stack for circularity and overflows.
 *
 * Caller is assumed to hold the classTableMutex. In the case of a failure,
 * the classTableMutex will be exited prior to returning. (This is necessary
 * because setCurrentException() cannot be called while holding a mutex.)
 *
 * Return TRUE on success. On failure, returns FALSE and sets the
 * appropriate Java error on the VM.
 */
static BOOLEAN
verifyClassLoadingStack(J9VMThread *vmThread, J9ClassLoader *classLoader, J9ROMClass *romClass)
{
	J9JavaVM *javaVM = vmThread->javaVM;
	J9ClassLoadingStackElement *currentElement;
	J9ClassLoadingStackElement *newTopOfStack;
	J9UTF8 *className = J9ROMCLASS_CLASSNAME(romClass);
	UDATA count = 0;
	UDATA maxStack = javaVM->classLoadingMaxStack;

	currentElement = vmThread->classLoadingStack;
	while (currentElement != NULL) {
		count++;
		if (currentElement->classLoader == classLoader) {
			J9UTF8 *currentRomName;
			currentRomName = J9ROMCLASS_CLASSNAME(currentElement->romClass);
			if (compareUTF8Length(J9UTF8_DATA(currentRomName), J9UTF8_LENGTH(currentRomName),
					J9UTF8_DATA(className), J9UTF8_LENGTH(className)) == 0)
			{
				/* class circularity problem.  Fail. */
				Trc_VM_CreateRAMClassFromROMClass_circularity(vmThread);
				omrthread_monitor_exit(javaVM->classTableMutex);
				setCurrentException(vmThread, J9VMCONSTANTPOOL_JAVALANGCLASSCIRCULARITYERROR, NULL);
				return FALSE;
			}
		}
		currentElement = currentElement->previous;
	}

	if ((0 != maxStack) && (count >= maxStack)) {
		if ((vmThread->privateFlags & J9_PRIVATE_FLAGS_CLOAD_OVERFLOW) != J9_PRIVATE_FLAGS_CLOAD_OVERFLOW) {
			/* too many simultaneous class loads.  Fail. */
			Trc_VM_CreateRAMClassFromROMClass_overflow(vmThread, count);
			omrthread_monitor_exit(javaVM->classTableMutex);
			vmThread->privateFlags |= J9_PRIVATE_FLAGS_CLOAD_OVERFLOW;
			setCurrentException(vmThread, J9VMCONSTANTPOOL_JAVALANGSTACKOVERFLOWERROR, NULL);
			vmThread->privateFlags &= ~J9_PRIVATE_FLAGS_CLOAD_OVERFLOW;
			return FALSE;
		}
	}

	newTopOfStack = (J9ClassLoadingStackElement *)pool_newElement(javaVM->classLoadingStackPool);
	if (newTopOfStack == NULL) {
		Trc_VM_CreateRAMClassFromROMClass_classLoadingStackOOM(vmThread);
		omrthread_monitor_exit(javaVM->classTableMutex);
		setNativeOutOfMemoryError(vmThread, 0, 0);
		return FALSE;
	}
	newTopOfStack->romClass = romClass;
	newTopOfStack->previous = vmThread->classLoadingStack;
	newTopOfStack->classLoader = classLoader;
	vmThread->classLoadingStack = newTopOfStack;

	return TRUE;
}

/**
 * Pops the top element of the thread's classLoadingStack.
 *
 * Caller is assumed to hold the classTableMutex, which guards
 * the use of vm->classLoadingStackPool.
 */
static void
popFromClassLoadingStack(J9VMThread *vmThread)
{
	J9ClassLoadingStackElement *topOfStack = vmThread->classLoadingStack;

	vmThread->classLoadingStack = topOfStack->previous;
	pool_removeElement(vmThread->javaVM->classLoadingStackPool, topOfStack);
}


/**
 * Attempts to recursively load (if necessary) the required superclass and
 * interfaces for the class being loaded.
 *
 * Caller should not hold the classTableMutex.
 *
 * Return TRUE on success. On failure, returns FALSE and sets the
 * appropriate Java error on the VM.
 */
static VMINLINE BOOLEAN
loadSuperClassAndInterfaces(J9VMThread *vmThread, J9ClassLoader *classLoader, J9ROMClass *romClass, J9Class *elementClass,
	UDATA packageID, BOOLEAN hotswapping, UDATA classPreloadFlags, J9Class **superclassOut)
{
	const BOOLEAN isROMClassUnsafe = (J9ROMCLASS_IS_UNSAFE(romClass) != 0);
	J9UTF8 *className = J9ROMCLASS_CLASSNAME(romClass);
	J9UTF8 *superclassName;
	J9Class *superclass;

	superclassName = J9ROMCLASS_SUPERCLASSNAME(romClass);
	if (superclassName == NULL) {
		superclass = NULL;
		*superclassOut = superclass;

	} else {
		if (!hotswapping) {
			/* If the superclass of the current class is itself, it's a ClassCircularityError. */
			if (compareUTF8Length(J9UTF8_DATA(superclassName), J9UTF8_LENGTH(superclassName),
					J9UTF8_DATA(className), J9UTF8_LENGTH(className)) == 0)
			{
				setCurrentException(vmThread, J9VMCONSTANTPOOL_JAVALANGCLASSCIRCULARITYERROR, NULL);
				return FALSE;
			}
		}

		superclass = internalFindClassUTF8(vmThread, J9UTF8_DATA(superclassName), J9UTF8_LENGTH(superclassName), classLoader, classPreloadFlags);
		*superclassOut = superclass;
		Trc_VM_CreateRAMClassFromROMClass_loadedSuperclass(vmThread, J9UTF8_LENGTH(superclassName), J9UTF8_DATA(superclassName), superclass);
		if (NULL == superclass) {
			return FALSE;
		}

		if (!hotswapping) {
			if (checkPackageAccess(vmThread, superclass, classPreloadFlags) != 0) {
				return FALSE;
			}

			/* ensure that the superclass isn't an interface or final */
			if ((superclass->romClass->modifiers & J9_JAVA_FINAL) != 0) {
				Trc_VM_CreateRAMClassFromROMClass_superclassIsFinal(vmThread, superclass);
				setCurrentExceptionForBadClass(vmThread, superclassName, J9VMCONSTANTPOOL_JAVALANGVERIFYERROR);
				return FALSE;
			}
			if ((superclass->romClass->modifiers & J9_JAVA_INTERFACE) != 0) {
				Trc_VM_CreateRAMClassFromROMClass_superclassIsInterface(vmThread, superclass);
				setCurrentExceptionForBadClass(vmThread, superclassName, J9VMCONSTANTPOOL_JAVALANGINCOMPATIBLECLASSCHANGEERROR);
				return FALSE;
			}

			/* ensure that the superclass is visible */
			if (!isROMClassUnsafe) {
				if ((superclass->romClass->modifiers & J9_JAVA_PUBLIC) != J9_JAVA_PUBLIC) {
					if (packageID != superclass->packageID) {
						Trc_VM_CreateRAMClassFromROMClass_superclassNotVisible(vmThread, superclass, superclass->classLoader, classLoader);
						setCurrentExceptionForBadClass(vmThread, superclassName, J9VMCONSTANTPOOL_JAVALANGILLEGALACCESSERROR);
						return FALSE;
					}
				}
			}

			/* force the interfaces to be loaded without holding the mutex */
			if (romClass->interfaceCount != 0) {
				UDATA i;
				J9SRP *interfaceNames = J9ROMCLASS_INTERFACES(romClass);

				for (i = 0; i<romClass->interfaceCount; i++) {
					J9UTF8 *interfaceName = NNSRP_GET(interfaceNames[i], J9UTF8*);
					J9Class *interfaceClass = internalFindClassUTF8(vmThread, J9UTF8_DATA(interfaceName), J9UTF8_LENGTH(interfaceName), classLoader, classPreloadFlags);

					Trc_VM_CreateRAMClassFromROMClass_loadedInterface(vmThread, J9UTF8_LENGTH(interfaceName), J9UTF8_DATA(interfaceName), interfaceClass);
					if (interfaceClass == NULL) {
						return FALSE;
					}
					if (checkPackageAccess(vmThread, interfaceClass, classPreloadFlags) != 0) {
						return FALSE;
					}
					/* ensure that the interface is in fact an interface */
					if ((interfaceClass->romClass->modifiers & J9_JAVA_INTERFACE) != J9_JAVA_INTERFACE) {
						Trc_VM_CreateRAMClassFromROMClass_interfaceIsNotAnInterface(vmThread, interfaceClass);
						setCurrentExceptionForBadClass(vmThread, J9ROMCLASS_CLASSNAME(interfaceClass->romClass), J9VMCONSTANTPOOL_JAVALANGINCOMPATIBLECLASSCHANGEERROR);
						return FALSE;
					}
					if (!isROMClassUnsafe) {
						if ((interfaceClass->romClass->modifiers & J9_JAVA_PUBLIC) != J9_JAVA_PUBLIC) {
							if (packageID != interfaceClass->packageID) {
								Trc_VM_CreateRAMClassFromROMClass_interfaceNotVisible(vmThread, interfaceClass, interfaceClass->classLoader, classLoader);
								setCurrentExceptionForBadClass(vmThread, J9ROMCLASS_CLASSNAME(interfaceClass->romClass), J9VMCONSTANTPOOL_JAVALANGILLEGALACCESSERROR);
								return FALSE;
							}
						}
					}
				}
			}
		}
	}

	return TRUE;
}

#if defined(J9VM_OPT_VALHALLA_NESTMATES)
static J9Class *
loadNestTop(J9VMThread *vmThread, J9ClassLoader *classLoader, J9UTF8 *nestTopName, UDATA classPreloadFlags)
{
	J9Class *nestTop = internalFindClassUTF8(vmThread, J9UTF8_DATA(nestTopName), J9UTF8_LENGTH(nestTopName), classLoader, classPreloadFlags);
	Trc_VM_CreateRAMClassFromROMClass_nestTopLoaded(vmThread, J9UTF8_LENGTH(nestTopName), J9UTF8_DATA(nestTopName), nestTop);
	return nestTop;
}
#endif

static J9Class*
internalCreateRAMClassDropAndReturn(J9VMThread *vmThread, J9ROMClass *romClass, J9CreateRAMClassState *state)
{
	/* pop protectionDomain */
	DROP_OBJECT_IN_SPECIAL_FRAME(vmThread);

	Trc_VM_CreateRAMClassFromROMClass_Exit(vmThread, state->ramClass, romClass);

	return state->ramClass;
}

static J9Class*
internalCreateRAMClassDoneNoMutex(J9VMThread *vmThread, J9ROMClass *romClass, UDATA options, J9CreateRAMClassState *state)
{
	J9JavaVM *javaVM = vmThread->javaVM;
	BOOLEAN hotswapping = (0 != (options & J9_FINDCLASS_FLAG_NO_DEBUG_EVENTS));

	if (!hotswapping) {
		if (state->ramClass != NULL) {
			TRIGGER_J9HOOK_VM_CLASS_LOAD(javaVM->hookInterface, vmThread, state->ramClass);
			if ((vmThread->publicFlags & J9_PUBLIC_FLAGS_POP_FRAMES_INTERRUPT) != 0) {
				state->ramClass = NULL;
			}
		}
	}

	return internalCreateRAMClassDropAndReturn(vmThread, romClass, state);
}

static J9Class*
internalCreateRAMClassDone(J9VMThread *vmThread, J9ClassLoader *classLoader, J9ROMClass *romClass,
	UDATA options, J9Class *elementClass, J9UTF8 *className, J9CreateRAMClassState *state)
{
	J9JavaVM *javaVM = vmThread->javaVM;
	BOOLEAN hotswapping = (0 != (options & J9_FINDCLASS_FLAG_NO_DEBUG_EVENTS));
	BOOLEAN fastHCR = (0 != (options & J9_FINDCLASS_FLAG_FAST_HCR));

	if (!hotswapping) {
		popFromClassLoadingStack(vmThread);
	}

	if (state->ramClass != NULL) {
		UDATA failed = FALSE;
		J9Class * alreadyLoadedClass = NULL;

		if (J9_ARE_ALL_BITS_SET(options, J9_FINDCLASS_FLAG_ANON)) {
			javaVM->anonClassCount += 1;
		}


		TRIGGER_J9HOOK_VM_INTERNAL_CLASS_LOAD(javaVM->hookInterface, vmThread, state->ramClass, failed);
		if (failed) {
			if (hotswapping) {
				omrthread_monitor_exit(javaVM->classTableMutex);
				state->ramClass = NULL;
				return internalCreateRAMClassDoneNoMutex(vmThread, romClass, options, state);
			}

			/* Release the mutex, GC, and reacquire the mutex. */
			omrthread_monitor_exit(javaVM->classTableMutex);
			PUSH_OBJECT_IN_SPECIAL_FRAME(vmThread, (j9object_t)state->classObject);
			javaVM->memoryManagerFunctions->j9gc_modron_global_collect_with_overrides(vmThread, J9MMCONSTANT_EXPLICIT_GC_NATIVE_OUT_OF_MEMORY);
			state->classObject = POP_OBJECT_IN_SPECIAL_FRAME(vmThread);
			omrthread_monitor_enter(javaVM->classTableMutex);

			/* If the class was successfully loaded while we were GCing, use that one. */
			if (elementClass == NULL) {
				alreadyLoadedClass = hashClassTableAt(classLoader, J9UTF8_DATA(className), J9UTF8_LENGTH(className));
			} else {
				alreadyLoadedClass = elementClass->arrayClass;
			}
			if (alreadyLoadedClass != NULL) {
alreadyLoaded:
				/* We are discarding this class - mark it as hot swapped to prevent it showing up in JVMTI class queries */

				state->ramClass->classDepthAndFlags |= J9AccClassHotSwappedOut;
				state->ramClass->arrayClass = alreadyLoadedClass;
				J9STATIC_OBJECT_STORE(vmThread, state->ramClass, (j9object_t*)&state->ramClass->classObject, (j9object_t)alreadyLoadedClass->classObject);

				omrthread_monitor_exit(javaVM->classTableMutex);
				state->ramClass = alreadyLoadedClass;
				return internalCreateRAMClassDropAndReturn(vmThread, romClass, state);
			}

			/* Try the hook again - if it fails again, throw native OOM. */
			TRIGGER_J9HOOK_VM_INTERNAL_CLASS_LOAD(javaVM->hookInterface, vmThread, state->ramClass, failed);
			if (failed) {
nativeOOM:
				omrthread_monitor_exit(javaVM->classTableMutex);
				setNativeOutOfMemoryError(vmThread, 0, 0);
				state->ramClass = NULL;
				return internalCreateRAMClassDoneNoMutex(vmThread, romClass, options, state);
			}
		}

		/* Initialize class object and link the object and the J9Class. */
		if (state->classObject != NULL) {
			if (J2SE_SHAPE_RAW != J2SE_SHAPE(javaVM)) {
				j9object_t protectionDomain = NULL;

				J9VMJAVALANGCLASS_SET_CLASSLOADER(vmThread, state->classObject, classLoader->classLoaderObject);
				protectionDomain = (j9object_t)PEEK_OBJECT_IN_SPECIAL_FRAME(vmThread, 0);
				J9VMJAVALANGCLASS_SET_PROTECTIONDOMAIN(vmThread, state->classObject, protectionDomain);
			}

			/* Link J9Class and object after possible access barrier failures. */
			J9VMJAVALANGCLASS_SET_VMREF(vmThread, state->classObject, state->ramClass);
			/* store the classObject using an access barrier */
			J9STATIC_OBJECT_STORE(vmThread, state->ramClass, (j9object_t*)&state->ramClass->classObject, (j9object_t)state->classObject);

			if (J9_ARE_ALL_BITS_SET(javaVM->runtimeFlags, J9_RUNTIME_JAVA_BASE_MODULE_CREATED)) {
				j9object_t moduleObject = NULL;
				if (J9_ARE_ALL_BITS_SET(options, J9_FINDCLASS_FLAG_ANON)) {
					moduleObject = J9VMJAVALANGCLASS_MODULE(vmThread, state->ramClass->hostClass->classObject);
				} else {
					J9Module *module = NULL;
					U_8 *packageUTF = NULL;
					UDATA length = 0;

					if (J9CLASS_IS_ARRAY(state->ramClass)) {
						J9ROMClass *leafType = ((J9ArrayClass*)state->ramClass)->leafComponentType->romClass;
						packageUTF = J9UTF8_DATA(J9ROMCLASS_CLASSNAME(leafType));
						length = packageNameLength(leafType);
					} else {
						packageUTF = J9UTF8_DATA(className);
						length = packageNameLength(romClass);
					}
					omrthread_monitor_enter(javaVM->classLoaderModuleAndLocationMutex);
					module = findModuleForPackage(vmThread, classLoader, packageUTF, (U_32)length);
					omrthread_monitor_exit(javaVM->classLoaderModuleAndLocationMutex);
					if (NULL != module) {
						moduleObject = module->moduleObject;
					} else {
						moduleObject = J9VMJAVALANGCLASSLOADER_UNNAMEDMODULE(vmThread, classLoader->classLoaderObject);
						Assert_VM_notNull(moduleObject);
					}
				}
				J9VMJAVALANGCLASS_SET_MODULE(vmThread, state->ramClass->classObject, moduleObject);
			}
		}

		/* Ensure all previous writes have completed before making the new class visible. */
		VM_AtomicSupport::writeBarrier();

		/* Put the new class in the table or arrayClass field. */
		if ((!fastHCR)
			&& (0 == J9ROMCLASS_IS_PRIMITIVE_OR_ARRAY(romClass))
			&& J9_ARE_NO_BITS_SET(options, J9_FINDCLASS_FLAG_ANON)
#if defined(J9VM_OPT_VALHALLA_MVT)
			&& J9_ARE_NO_BITS_SET(options, J9_FINDCLASS_FLAG_DERIVED_VALUE_TYPE)
#endif /* defined(J9VM_OPT_VALHALLA_MVT) */
		) {
			if (hashClassTableAtPut(vmThread, classLoader, J9UTF8_DATA(className), J9UTF8_LENGTH(className), state->ramClass)) {
				if (hotswapping) {
					omrthread_monitor_exit(javaVM->classTableMutex);
					state->ramClass = NULL;
					return internalCreateRAMClassDoneNoMutex(vmThread, romClass, options, state);
				}

				/* Release the mutex, GC, and reacquire the mutex */
				omrthread_monitor_exit(javaVM->classTableMutex);
				javaVM->memoryManagerFunctions->j9gc_modron_global_collect_with_overrides(vmThread, J9MMCONSTANT_EXPLICIT_GC_NATIVE_OUT_OF_MEMORY);
				omrthread_monitor_enter(javaVM->classTableMutex);

				/* If the class was successfully loaded while we were GCing, use that one */
				if (elementClass == NULL) {
					alreadyLoadedClass = hashClassTableAt(classLoader, J9UTF8_DATA(className), J9UTF8_LENGTH(className));
				} else {
					alreadyLoadedClass = elementClass->arrayClass;
				}
				if (alreadyLoadedClass != NULL) {
					goto alreadyLoaded;
				}

				/* Try the store again - if it fails again, throw native OOM */
				if (hashClassTableAtPut(vmThread, classLoader, J9UTF8_DATA(className), J9UTF8_LENGTH(className), state->ramClass)) {
					goto nativeOOM;
				}
			}

		} else {
			if (J9ROMCLASS_IS_ARRAY(romClass)) {
				((J9ArrayClass *)elementClass)->arrayClass = state->ramClass;
				/* Assigning into the arrayClass field creates an implicit reference to the class from its class loader */
				javaVM->memoryManagerFunctions->j9gc_objaccess_postStoreClassToClassLoader(vmThread, classLoader, state->ramClass);
			}
		}
	}

	omrthread_monitor_exit(javaVM->classTableMutex);

	return internalCreateRAMClassDoneNoMutex(vmThread, romClass, options, state);
}

#if defined(J9VM_INTERP_CUSTOM_SPIN_OPTIONS)
static void
checkForCustomSpinOptions(void *element, void *userData)
{
	J9Class *ramClass = (J9Class *)userData;
	J9VMCustomSpinOptions *option = (J9VMCustomSpinOptions *)element;
	J9UTF8 *className = J9ROMCLASS_CLASSNAME(ramClass->romClass);
	if (J9UTF8_DATA_EQUALS(J9UTF8_DATA(className), J9UTF8_LENGTH(className), option->className, strlen(option->className))) {
		ramClass->customSpinOption = option;
	}
}
#endif /* J9VM_INTERP_CUSTOM_SPIN_OPTIONS */

static void
trcModulesSettingPackage(J9VMThread *vmThread, J9Class *ramClass, J9ClassLoader *classLoader, J9UTF8 *className)
{
	J9JavaVM *javaVM = vmThread->javaVM;
	J9InternalVMFunctions const * const vmFuncs = javaVM->internalVMFunctions;
	char moduleNameBuf[J9VM_PACKAGE_NAME_BUFFER_LENGTH];
	char *moduleNameUTF = NULL;
	char classLoaderNameBuf[J9VM_PACKAGE_NAME_BUFFER_LENGTH];
	char *classLoaderNameUTF = NULL;

	if (javaVM->unamedModuleForSystemLoader == ramClass->module) {
#define SYSTEMLOADER_UNNAMED_NAMED_MODULE   "unnamed module for system loader"
		memcpy(moduleNameBuf, SYSTEMLOADER_UNNAMED_NAMED_MODULE, sizeof(SYSTEMLOADER_UNNAMED_NAMED_MODULE));
#undef	SYSTEMLOADER_UNNAMED_NAMED_MODULE
		moduleNameUTF = moduleNameBuf;
	} else if (javaVM->javaBaseModule == ramClass->module) {
#define JAVABASE_MODULE   "java.base module"
		memcpy(moduleNameBuf, JAVABASE_MODULE, sizeof(JAVABASE_MODULE));
#undef	JAVABASE_MODULE
		moduleNameUTF = moduleNameBuf;
	} else if (NULL == ramClass->module) {
#define UNNAMED_NAMED_MODULE   "unnamed module"
		memcpy(moduleNameBuf, UNNAMED_NAMED_MODULE, sizeof(UNNAMED_NAMED_MODULE));
#undef	UNNAMED_NAMED_MODULE
		moduleNameUTF = moduleNameBuf;
	} else {
		moduleNameUTF = vmFuncs->copyStringToUTF8WithMemAlloc(
			vmThread, ramClass->module->moduleName, J9_STR_NONE, "", moduleNameBuf, J9VM_PACKAGE_NAME_BUFFER_LENGTH);
	}
	j9object_t classLoaderName = NULL;
	if (NULL != classLoader->classLoaderObject) {
		classLoaderName = J9VMJAVALANGCLASSLOADER_CLASSLOADERNAME(vmThread, classLoader->classLoaderObject);
	}
	if (NULL != classLoaderName) {
		classLoaderNameUTF = vmFuncs->copyStringToUTF8WithMemAlloc(
			vmThread, classLoaderName, J9_STR_NONE, "", classLoaderNameBuf, J9VM_PACKAGE_NAME_BUFFER_LENGTH);
	} else {
#define UNNAMED_NAMED_MODULE   "system classloader"
		memcpy(classLoaderNameBuf, UNNAMED_NAMED_MODULE, sizeof(UNNAMED_NAMED_MODULE));
#undef	UNNAMED_NAMED_MODULE
		classLoaderNameUTF = classLoaderNameBuf;
	}
	if ((NULL != classLoaderNameUTF) && (NULL != moduleNameUTF)) {
		Trc_MODULE_setting_package(vmThread, J9UTF8_LENGTH(className), J9UTF8_DATA(className), classLoaderNameUTF, moduleNameUTF);
		if (moduleNameBuf != moduleNameUTF) {
			PORT_ACCESS_FROM_VMC(vmThread);
			j9mem_free_memory(moduleNameUTF);
		}
		if (classLoaderNameBuf != classLoaderNameUTF) {
			PORT_ACCESS_FROM_VMC(vmThread);
			j9mem_free_memory(classLoaderNameUTF);
		}
	}
}

static J9Class*
internalCreateRAMClassFromROMClassImpl(J9VMThread *vmThread, J9ClassLoader *classLoader, J9ROMClass *romClass,
	UDATA options, J9Class *elementClass, J9ROMMethod **methodRemapArray, IDATA entryIndex, I_32 locationType, J9Class *classBeingRedefined,
	UDATA packageID, J9Class *superclass, J9CreateRAMClassState *state, J9ClassLoader* hostClassLoader, J9Class *hostClass)
{
	J9JavaVM *javaVM = vmThread->javaVM;
	BOOLEAN retried = state->retry;
	J9Class *ramClass = NULL;
	J9UTF8 *className = J9ROMCLASS_CLASSNAME(romClass);
	J9ROMMethod *badMethod = NULL;
	UDATA vTableSlots = 0;
	UDATA jitVTableSlots = 0;
	UDATA *vTable = NULL;
	UDATA classSize;
	UDATA result;
	J9Class *interfaceHead;
	BOOLEAN foundCloneable = FALSE;
	UDATA extendedMethodBlockSize = 0;
	UDATA totalStaticSlots;
	UDATA interfaceCount = 0;
	J9ROMFieldOffsetWalkState romWalkState;
	J9ROMFieldOffsetWalkResult *romWalkResult;
	BOOLEAN hotswapping = (0 != (options & J9_FINDCLASS_FLAG_NO_DEBUG_EVENTS));
	BOOLEAN fastHCR = (0 != (options & J9_FINDCLASS_FLAG_FAST_HCR));
	UDATA *iTable = NULL;
	UDATA *instanceDescription = NULL;
	UDATA instanceDescriptionSlotCount = 0;
	UDATA iTableSlotCount = 0;
	IDATA maxInterfaceDepth = -1;
	UDATA inheritedInterfaceCount = 0;
	UDATA defaultConflictCount = 0;
	UDATA length = 0;
	J9OverrideErrorData errorData = {0};

	PORT_ACCESS_FROM_JAVAVM(javaVM);

	state->retry = FALSE;
	/* Ensure we have enough space in the emergency stackmap buffer for this class. */
	result = j9maxmap_setMapMemoryBuffer(javaVM, romClass);
	if (result != 0) {
		Trc_VM_CreateRAMClassFromROMClass_outOfMemory(vmThread, 0);
		if (hotswapping) {
fail:
			omrthread_monitor_enter(javaVM->classTableMutex);
			return internalCreateRAMClassDone(vmThread, classLoader, romClass, options, elementClass, className, state);
		}
		javaVM->memoryManagerFunctions->j9gc_modron_global_collect_with_overrides(vmThread, J9MMCONSTANT_EXPLICIT_GC_NATIVE_OUT_OF_MEMORY);
		result = j9maxmap_setMapMemoryBuffer(javaVM, romClass);
		if (result != 0) {
			setNativeOutOfMemoryError(vmThread, 0, 0);
			goto fail;
		}
	}
	/* Allocate class object */
	if (!hotswapping) {
		J9Class *jlClass = J9VMCONSTANTPOOL_CLASSREF_AT(javaVM, J9VMCONSTANTPOOL_JAVALANGCLASS)->value;

		if (jlClass != NULL) {
			J9Class * lockClass = J9VMJAVALANGJ9VMINTERNALSCLASSINITIALIZATIONLOCK_OR_NULL(javaVM);
			/* Note: in RTJ, tenured implies immortal. Classes must be in the immortal space so we must set the tenured flag. */
			state->classObject = javaVM->memoryManagerFunctions->J9AllocateObject(vmThread, jlClass, J9_GC_ALLOCATE_OBJECT_TENURED | J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE | J9_GC_ALLOCATE_OBJECT_HASHED);
			if (state->classObject == NULL) {
				Trc_VM_CreateRAMClassFromROMClass_classObjOOM(vmThread);
				setHeapOutOfMemoryError(vmThread);
				goto fail;
			}

			/* If the lock class is not yet loaded, skip allocating the instance - will be fixed up once the class is loaded */
			if (lockClass != NULL) {
				j9object_t lockObject;
				UDATA allocateFlags = J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE;

				PUSH_OBJECT_IN_SPECIAL_FRAME(vmThread, (j9object_t)state->classObject);
			lockObject = javaVM->memoryManagerFunctions->J9AllocateObject(vmThread, lockClass, allocateFlags);

				state->classObject = POP_OBJECT_IN_SPECIAL_FRAME(vmThread);
				if (lockObject == NULL) {
					Trc_VM_CreateRAMClassFromROMClass_classObjOOM(vmThread);
					setHeapOutOfMemoryError(vmThread);
					goto fail;
				}
				J9VMJAVALANGJ9VMINTERNALSCLASSINITIALIZATIONLOCK_SET_THECLASS(vmThread, lockObject, (j9object_t)state->classObject);
				J9VMJAVALANGCLASS_SET_INITIALIZATIONLOCK(vmThread, (j9object_t)state->classObject, lockObject);
			}
		}
	}


	/* Now that all required classes are loaded, reacquire the classTableMutex and see if the new class has appeared in the table. 
	 * If so, return that one.  If not, create the new class and put it in the class table.
	 */
	omrthread_monitor_enter(javaVM->classTableMutex);
	/* computeRAMSizeForROMClass */
	{
		classSize = sizeof(J9Class) / sizeof(void *);

		/* add in size of method trace array  if extended method block active */
		if ((javaVM->runtimeFlags & J9_RUNTIME_EXTENDED_METHOD_BLOCK) == J9_RUNTIME_EXTENDED_METHOD_BLOCK) {
			extendedMethodBlockSize = romClass->romMethodCount + (sizeof(UDATA) - 1);
			extendedMethodBlockSize &= ~(sizeof(UDATA) - 1);
			extendedMethodBlockSize /= sizeof(UDATA);
			classSize += extendedMethodBlockSize;
		}

		/* add in the methods */
		classSize += romClass->romMethodCount * (sizeof(J9Method) / sizeof(void *));
		
		/* add in the constant pool items, convert from count to # of slots */
		classSize += romClass->ramConstantPoolCount * 2;
		
		/* add in number of statics */
		totalStaticSlots = totalStaticSlotsForClass(romClass);
		classSize += totalStaticSlots;
		
		/* add in the call sites */
		classSize += romClass->callSiteCount;

		/* add in the method types */
		classSize += romClass->methodTypeCount;

		/* add in the varhandle method types */
		classSize += romClass->varHandleMethodTypeCount;

		/* add in the static and special split tables */
		classSize += (romClass->staticSplitMethodRefCount + romClass->specialSplitMethodRefCount);

		romWalkResult = fieldOffsetsStartDo(javaVM, romClass, superclass, &romWalkState,
			(J9VM_FIELD_OFFSET_WALK_CALCULATE_INSTANCE_SIZE | J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE |
			 J9VM_FIELD_OFFSET_WALK_ONLY_OBJECT_SLOTS));
		/* inherited from superclass: superclasses array, instance shape and interface slots */
		if (superclass == NULL) {
			/* java.lang.Object has a NULL at superclasses[-1] for fast superclass fetch. */
			classSize += 1;
		} else {
			static const UDATA highestBitInSlot = sizeof(UDATA) * 8 - 1;
			/* add in my instanceShape size (0 if <= highestBitInSlot) */
			UDATA temp = romWalkResult->totalInstanceSize / sizeof(fj9object_t);

			if (temp > highestBitInSlot) {
				/* reserve additional space for the instance description bits */
				temp += highestBitInSlot;
				temp &= ~highestBitInSlot;
				instanceDescriptionSlotCount = temp / (sizeof(UDATA) * 8);
#if defined(J9VM_GC_LEAF_BITS)
				/* add space for leaf bits description, same size as instance description */
				instanceDescriptionSlotCount += instanceDescriptionSlotCount;
#endif
				classSize += instanceDescriptionSlotCount;
			}

			classSize += J9CLASS_DEPTH(superclass);
			classSize++;
		}
		if (fastHCR) {
			interfaceHead = NULL;
			/* Obsolete classes do not need a vTable since no new method invocations should be done through them. */
			vTableSlots = 1; /* size slot only */
		} else {
			interfaceHead = markInterfaces(romClass, superclass, hostClassLoader, &foundCloneable, &interfaceCount, &inheritedInterfaceCount, &maxInterfaceDepth);
			/* Compute the number of slots required for the interpreter and jit (if enabled) vTables. */
			vTable = computeVTable(vmThread, classLoader, superclass, romClass, packageID, methodRemapArray, interfaceHead, &defaultConflictCount, interfaceCount, inheritedInterfaceCount, &errorData);
			if (vTable == NULL) {
				unmarkInterfaces(interfaceHead);
				popFromClassLoadingStack(vmThread);
				omrthread_monitor_exit(javaVM->classTableMutex);
				if (NULL != errorData.loader1) {
					J9UTF8 *methodNameUTF = errorData.methodNameUTF;
					J9UTF8 *methodSigUTF = errorData.methodSigUTF;
					setClassLoadingConstraintOverrideError(vmThread, J9ROMCLASS_CLASSNAME(romClass), errorData.loader1, errorData.class1NameUTF, errorData.loader2, errorData.class2NameUTF, errorData.exceptionClassNameUTF, J9UTF8_DATA(methodNameUTF), J9UTF8_LENGTH(methodNameUTF), J9UTF8_DATA(methodSigUTF), J9UTF8_LENGTH(methodSigUTF));
				} else {
					setNativeOutOfMemoryError(vmThread, 0, 0);
				}
				return internalCreateRAMClassDoneNoMutex(vmThread, romClass, options, state);
			}
			vTableSlots = *vTable;
			/* account for size slot */
			vTableSlots++;
			/* If there is a bad methods, vmThread->tempSlot will be set by computeVTable() - yuck! */
			badMethod = (J9ROMMethod *)vmThread->tempSlot;
		}

		/* If the JIT is enabled, add in it's vTable size. The JIT vTable must be padded so that
		 * the class (which follows it) is properly aligned.
		 */
		jitVTableSlots = 0;
#if defined(J9VM_INTERP_NATIVE_SUPPORT)
		if (javaVM->jitConfig != NULL) {
			/* Subtract 1 since the vTable contains a size slot that isn't required for the jitVTable.
			 * There is a size (or SRP) written so the next class can be found, but this is handled by
			 * internalAllocateRAMClass().
			 */
			jitVTableSlots = vTableSlots - 1;
		}
#endif
		/* commit the number of vTable slots */
		classSize += vTableSlots;

		/* Compute space required for all local iTables.
		 * All array classes share the same iTable.  If this is an array class and [Z (the first
		 * allocated array class) has been filled in, no iTable is generated for this class.
		 */
		if (!fastHCR && !(J9ROMCLASS_IS_ARRAY(romClass) && (javaVM->booleanArrayClass != NULL))) {
			/* Compute the space required for the remaining interfaces.  Interface
			 * classes must appear in their own iTable list.
			 */
			iTableSlotCount = (sizeof(J9ITable) / sizeof(UDATA)) * interfaceCount;
			if ((romClass->modifiers & J9_JAVA_INTERFACE) == J9_JAVA_INTERFACE) {
				/* The iTables for interface classes do not contain entries for methods. */
				iTableSlotCount += sizeof(J9ITable) / sizeof(UDATA);
			} else {
				J9Class *interfaceWalk = interfaceHead;
				while (interfaceWalk != NULL) {
					/* The iTables for interface classes do not contain entries for methods. */
					/* add methods supported by this interface to tally */
					iTableSlotCount += interfaceWalk->romClass->romMethodCount;
					interfaceWalk = (J9Class *)((UDATA)interfaceWalk->instanceDescription & ~INTERFACE_TAG);
				}
			}
			classSize += iTableSlotCount;
		}
		
		/* Convert count to bytes and round to required alignment */
		classSize *= sizeof(UDATA);
	}
	Trc_VM_CreateRAMClassFromROMClass_calculatedRAMSize(vmThread, classSize);

	if ((javaVM->runtimeFlags & J9_RUNTIME_VERIFY) && (badMethod != NULL)) {
		J9BytecodeVerificationData *bcvd = javaVM->bytecodeVerificationData;
		J9UTF8 *badName = J9ROMMETHOD_NAME(badMethod);
		J9UTF8 *badSig = J9ROMMETHOD_SIGNATURE(badMethod);
		
		unmarkInterfaces(interfaceHead);

		Trc_VM_CreateRAMClassFromROMClass_overriddenFinalMethod(vmThread, J9UTF8_LENGTH(badName), J9UTF8_DATA(badName), J9UTF8_LENGTH(badSig), J9UTF8_DATA(badSig));
		
		if (!hotswapping) {
			U_8 *verifyErrorString;

			popFromClassLoadingStack(vmThread);
			omrthread_monitor_exit(javaVM->classTableMutex);
			/*
			 * java_lang_J9VMInternals_verifyImpl enters verifierMutex then classTableMutex.
			 * This cause deadlock if we try to enter the mutexes in the opposite order.
			 * Assert that we have fully exited the classTableMutex.
			 */
			Assert_VM_true(0 == omrthread_monitor_owned_by_self(javaVM->classTableMutex));
			omrthread_monitor_enter(bcvd->verifierMutex);
			bcvd->romClass = romClass;
			bcvd->romMethod = badMethod;
			bcvd->errorModule = J9NLS_BCV_ERR_FINAL_METHOD_OVERRIDE__MODULE;
			bcvd->errorCode = J9NLS_BCV_ERR_FINAL_METHOD_OVERRIDE__ID;
			verifyErrorString = j9bcv_createVerifyErrorString(PORTLIB, bcvd);
			omrthread_monitor_exit(bcvd->verifierMutex);

			setCurrentExceptionUTF(vmThread, J9VMCONSTANTPOOL_JAVALANGVERIFYERROR, (char *) verifyErrorString);
			j9mem_free_memory(verifyErrorString);
			return internalCreateRAMClassDoneNoMutex(vmThread, romClass, options, state);
		}

		return internalCreateRAMClassDone(vmThread, classLoader, romClass, options, elementClass, className, state);
	}

	if (!hotswapping 
#if defined(J9VM_OPT_VALHALLA_MVT)
		&& J9_ARE_NO_BITS_SET(romClass->extraModifiers, J9AccClassIsValueCapable)
#endif /* defined(J9VM_OPT_VALHALLA_MVT) */
	) {
		if (elementClass == NULL) {
			ramClass = hashClassTableAt(classLoader, J9UTF8_DATA(className), J9UTF8_LENGTH(className));
		} else {
			ramClass = elementClass->arrayClass;
		}
		state->ramClass = ramClass;

		if (ramClass != NULL) {
			unmarkInterfaces(interfaceHead);
			popFromClassLoadingStack(vmThread);
			omrthread_monitor_exit(javaVM->classTableMutex);
			Trc_VM_CreateRAMClassFromROMClass_alreadyLoaded(vmThread, state->ramClass);
			return internalCreateRAMClassDropAndReturn(vmThread, romClass, state);
		}
	}
	if (classSize != 0) {
		if (ramClass == NULL) {
			J9MemorySegment *segment;
			RAMClassAllocationRequest allocationRequests[RAM_CLASS_FRAGMENT_COUNT];
			UDATA minimumSuperclassArraySizeBytes = (sizeof(UDATA) * javaVM->minimumSuperclassArraySize);
			UDATA superclassSizeBytes = 0;

			/* RAM class header fragment */
			allocationRequests[RAM_CLASS_HEADER_FRAGMENT].prefixSize = jitVTableSlots * sizeof(UDATA);
			allocationRequests[RAM_CLASS_HEADER_FRAGMENT].alignment = J9_REQUIRED_CLASS_ALIGNMENT;
			allocationRequests[RAM_CLASS_HEADER_FRAGMENT].alignedSize = sizeof(J9Class) + vTableSlots * sizeof(UDATA);
			allocationRequests[RAM_CLASS_HEADER_FRAGMENT].address = NULL;

			/* RAM methods fragment */
			allocationRequests[RAM_METHODS_FRAGMENT].prefixSize = extendedMethodBlockSize * sizeof(UDATA);
			allocationRequests[RAM_METHODS_FRAGMENT].alignment = sizeof(UDATA);
			allocationRequests[RAM_METHODS_FRAGMENT].alignedSize = (romClass->romMethodCount + defaultConflictCount) * sizeof(J9Method);
			allocationRequests[RAM_METHODS_FRAGMENT].address = NULL;

			/* superclasses fragment */
			allocationRequests[RAM_SUPERCLASSES_FRAGMENT].prefixSize = 0;
			allocationRequests[RAM_SUPERCLASSES_FRAGMENT].alignment = sizeof(UDATA);
			superclassSizeBytes = sizeof(UDATA);
			minimumSuperclassArraySizeBytes += sizeof(UDATA);
			if (NULL != superclass) {
				superclassSizeBytes += (J9CLASS_DEPTH(superclass)) * sizeof(UDATA);
			}
			allocationRequests[RAM_SUPERCLASSES_FRAGMENT].alignedSize = OMR_MAX(superclassSizeBytes, minimumSuperclassArraySizeBytes);
			allocationRequests[RAM_SUPERCLASSES_FRAGMENT].address = NULL;

			/* instance description fragment */
			allocationRequests[RAM_INSTANCE_DESCRIPTION_FRAGMENT].prefixSize = 0;
			allocationRequests[RAM_INSTANCE_DESCRIPTION_FRAGMENT].alignment = sizeof(UDATA);
			allocationRequests[RAM_INSTANCE_DESCRIPTION_FRAGMENT].alignedSize = instanceDescriptionSlotCount * sizeof(UDATA);
			allocationRequests[RAM_INSTANCE_DESCRIPTION_FRAGMENT].address = NULL;

			/* iTable fragment */
			allocationRequests[RAM_ITABLE_FRAGMENT].prefixSize = 0;
			allocationRequests[RAM_ITABLE_FRAGMENT].alignment = sizeof(UDATA);
			allocationRequests[RAM_ITABLE_FRAGMENT].alignedSize = iTableSlotCount * sizeof(UDATA);
			allocationRequests[RAM_ITABLE_FRAGMENT].address = NULL;

			/* static slots fragment */
			allocationRequests[RAM_STATICS_FRAGMENT].prefixSize = 0;
			allocationRequests[RAM_STATICS_FRAGMENT].alignment = sizeof(U_64);
			allocationRequests[RAM_STATICS_FRAGMENT].alignedSize = totalStaticSlots * sizeof(UDATA);
			allocationRequests[RAM_STATICS_FRAGMENT].address = NULL;

			/* constant pool fragment */
			allocationRequests[RAM_CONSTANT_POOL_FRAGMENT].prefixSize = 0;
			allocationRequests[RAM_CONSTANT_POOL_FRAGMENT].alignment = REQUIRED_CONSTANT_POOL_ALIGNMENT;
			allocationRequests[RAM_CONSTANT_POOL_FRAGMENT].alignedSize = romClass->ramConstantPoolCount * 2 * sizeof(UDATA);
			allocationRequests[RAM_CONSTANT_POOL_FRAGMENT].address = NULL;

			/* call sites fragment */
			allocationRequests[RAM_CALL_SITES_FRAGMENT].prefixSize = 0;
			allocationRequests[RAM_CALL_SITES_FRAGMENT].alignment = sizeof(UDATA);
			allocationRequests[RAM_CALL_SITES_FRAGMENT].alignedSize = romClass->callSiteCount * sizeof(j9object_t);
			allocationRequests[RAM_CALL_SITES_FRAGMENT].address = NULL;

			/* method types fragment */
			allocationRequests[RAM_METHOD_TYPES_FRAGMENT].prefixSize = 0;
			allocationRequests[RAM_METHOD_TYPES_FRAGMENT].alignment = sizeof(UDATA);
			allocationRequests[RAM_METHOD_TYPES_FRAGMENT].alignedSize = romClass->methodTypeCount * sizeof(j9object_t);
			allocationRequests[RAM_METHOD_TYPES_FRAGMENT].address = NULL;

			/* varhandle method types fragment */
			allocationRequests[RAM_VARHANDLE_METHOD_TYPES_FRAGMENT].prefixSize = 0;
			allocationRequests[RAM_VARHANDLE_METHOD_TYPES_FRAGMENT].alignment = sizeof(UDATA);
			allocationRequests[RAM_VARHANDLE_METHOD_TYPES_FRAGMENT].alignedSize = romClass->varHandleMethodTypeCount * sizeof(j9object_t);
			allocationRequests[RAM_VARHANDLE_METHOD_TYPES_FRAGMENT].address = NULL;

			/* static split table fragment */
			allocationRequests[RAM_STATIC_SPLIT_TABLE_FRAGMENT].prefixSize = 0;
			allocationRequests[RAM_STATIC_SPLIT_TABLE_FRAGMENT].alignment = sizeof(UDATA);
			allocationRequests[RAM_STATIC_SPLIT_TABLE_FRAGMENT].alignedSize = romClass->staticSplitMethodRefCount * sizeof(J9Method *);
			allocationRequests[RAM_STATIC_SPLIT_TABLE_FRAGMENT].address = NULL;

			/* special split table fragment */
			allocationRequests[RAM_SPECIAL_SPLIT_TABLE_FRAGMENT].prefixSize = 0;
			allocationRequests[RAM_SPECIAL_SPLIT_TABLE_FRAGMENT].alignment = sizeof(UDATA);
			allocationRequests[RAM_SPECIAL_SPLIT_TABLE_FRAGMENT].alignedSize = romClass->specialSplitMethodRefCount * sizeof(J9Method *);
			allocationRequests[RAM_SPECIAL_SPLIT_TABLE_FRAGMENT].address = NULL;

			if (fastHCR) {
				/* For shared fragments, set alignedSize and prefixSize to 0 to make internalAllocateRAMClass() ignore them. */
				allocationRequests[RAM_SUPERCLASSES_FRAGMENT].address = (UDATA *) classBeingRedefined->superclasses;
				allocationRequests[RAM_SUPERCLASSES_FRAGMENT].prefixSize = 0;
				allocationRequests[RAM_SUPERCLASSES_FRAGMENT].alignedSize = 0;
				allocationRequests[RAM_INSTANCE_DESCRIPTION_FRAGMENT].address = classBeingRedefined->instanceDescription;
				allocationRequests[RAM_INSTANCE_DESCRIPTION_FRAGMENT].prefixSize = 0;
				allocationRequests[RAM_INSTANCE_DESCRIPTION_FRAGMENT].alignedSize = 0;
				allocationRequests[RAM_ITABLE_FRAGMENT].address = (UDATA *) classBeingRedefined->iTable;
				allocationRequests[RAM_ITABLE_FRAGMENT].prefixSize = 0;
				allocationRequests[RAM_ITABLE_FRAGMENT].alignedSize = 0;
				allocationRequests[RAM_STATICS_FRAGMENT].address = classBeingRedefined->ramStatics;
				allocationRequests[RAM_STATICS_FRAGMENT].prefixSize = 0;
				allocationRequests[RAM_STATICS_FRAGMENT].alignedSize = 0;
			}

			segment = internalAllocateRAMClass(javaVM, classLoader, allocationRequests, RAM_CLASS_FRAGMENT_COUNT);
			if (NULL != segment) {
				ramClass = (J9Class *) allocationRequests[RAM_CLASS_HEADER_FRAGMENT].address;
				state->ramClass = ramClass;
				ramClass->nextClassInSegment = *(J9Class **) segment->heapBase;
				*(J9Class **)segment->heapBase = ramClass;
				ramClass->ramMethods = (J9Method *) allocationRequests[RAM_METHODS_FRAGMENT].address;
				if (fastHCR) {
					/* Share iTable and instanceDescription (and associated fields) with class being redefined. */
					ramClass->iTable = classBeingRedefined->iTable;
					ramClass->instanceDescription = classBeingRedefined->instanceDescription;
#if defined(J9VM_GC_LEAF_BITS)
					ramClass->instanceLeafDescription = classBeingRedefined->instanceLeafDescription;
#endif
					ramClass->totalInstanceSize = classBeingRedefined->totalInstanceSize;
					ramClass->backfillOffset = classBeingRedefined->backfillOffset;
					ramClass->finalizeLinkOffset = classBeingRedefined->finalizeLinkOffset;
#if defined(J9VM_THR_LOCK_NURSERY)
					ramClass->lockOffset = classBeingRedefined->lockOffset;
#endif
				} else {
					instanceDescription = allocationRequests[RAM_INSTANCE_DESCRIPTION_FRAGMENT].address;
					iTable = allocationRequests[RAM_ITABLE_FRAGMENT].address;
				}
				ramClass->superclasses = (J9Class **) allocationRequests[RAM_SUPERCLASSES_FRAGMENT].address;
				ramClass->ramStatics = allocationRequests[RAM_STATICS_FRAGMENT].address;
				ramClass->ramConstantPool = allocationRequests[RAM_CONSTANT_POOL_FRAGMENT].address;
				ramClass->callSites = (j9object_t *) allocationRequests[RAM_CALL_SITES_FRAGMENT].address;
				ramClass->methodTypes = (j9object_t *) allocationRequests[RAM_METHOD_TYPES_FRAGMENT].address;
				ramClass->varHandleMethodTypes = (j9object_t *) allocationRequests[RAM_VARHANDLE_METHOD_TYPES_FRAGMENT].address;
				ramClass->staticSplitMethodTable = (J9Method **) allocationRequests[RAM_STATIC_SPLIT_TABLE_FRAGMENT].address;
				ramClass->specialSplitMethodTable = (J9Method **) allocationRequests[RAM_SPECIAL_SPLIT_TABLE_FRAGMENT].address;
			}
		}
		
		if (ramClass == NULL) {
			unmarkInterfaces(interfaceHead);

			Trc_VM_CreateRAMClassFromROMClass_outOfMemory(vmThread, classSize);
			
			if (!hotswapping) {
				popFromClassLoadingStack(vmThread);
			}

			omrthread_monitor_exit(javaVM->classTableMutex);

			if (!hotswapping) {
				if (!retried) {
					javaVM->memoryManagerFunctions->j9gc_modron_global_collect_with_overrides(vmThread, J9MMCONSTANT_EXPLICIT_GC_NATIVE_OUT_OF_MEMORY);
					state->retry = TRUE;
					return NULL;
				}

				setNativeOutOfMemoryError(vmThread, 0, 0);
			}
			return internalCreateRAMClassDoneNoMutex(vmThread, romClass, options, state);
		}
		
		/* initialize RAM Class */
		{
			J9ConstantPool *ramConstantPool = (J9ConstantPool *) (ramClass->ramConstantPool);
			UDATA ramConstantPoolCount = romClass->ramConstantPoolCount * 2; /* 2 slots per CP entry */
			U_32 tempClassDepthAndFlags = 0;
			Trc_VM_initializeRAMClass_Start(vmThread, J9UTF8_LENGTH(className), J9UTF8_DATA(className), classLoader);

			/* Default to no class path entry. */
			ramClass->romClass = romClass;
			ramClass->eyecatcher = 0x99669966;
			ramClass->module = NULL;
			
			/* hostClass is exclusively defined only in Unsafe.defineAnonymousClass.
			 * For all other cases, clazz->hostClass points to itself (clazz).
			 */
			if (NULL != hostClass) {
				ramClass->hostClass = hostClass;
			} else {
				ramClass->hostClass = ramClass;
			}

#if defined(J9VM_OPT_VALHALLA_NESTMATES)
			{
				J9Class *nestTop = NULL;
				J9UTF8 *nestTopName = J9ROMCLASS_NESTTOPNAME(romClass);

				/* If no nest top is named, class is own nest top */
				if (NULL == nestTopName) {
					nestTop = ramClass;
				} else {
					UDATA nestTopClassPreloadFlags = 0;
					if (hotswapping) {
						nestTopClassPreloadFlags = J9_FINDCLASS_FLAG_EXISTING_ONLY;
					} else {
						nestTopClassPreloadFlags = J9_FINDCLASS_FLAG_THROW_ON_FAIL;
						if (classLoader != javaVM->systemClassLoader) {
							nestTopClassPreloadFlags |= J9_FINDCLASS_FLAG_CHECK_PKG_ACCESS;
						}
					}
					nestTop = loadNestTop(vmThread, hostClassLoader, nestTopName, nestTopClassPreloadFlags);
				}
				/* If nest top loading failed, an exception has been set; end loading early */
				if (NULL == nestTop) {
					return internalCreateRAMClassDone(vmThread, classLoader, romClass, options, elementClass, className, state);
				}
				ramClass->memberOfNest = nestTop;
			}
#endif /* defined(J9VM_OPT_VALHALLA_NESTMATES) */

			/* Initialize the methods. */
			if (romClass->romMethodCount != 0) {
				J9Method *currentRAMMethod = ramClass->ramMethods;
				UDATA i;
				UDATA methodCount = romClass->romMethodCount;
				J9ROMMethod *romMethod = J9ROMCLASS_ROMMETHODS(romClass);
				for (i = 0; i < methodCount; i++) {
					J9UTF8 *className = J9ROMCLASS_CLASSNAME(romClass);
					J9UTF8 *methodName = J9ROMMETHOD_NAME(romMethod);
					J9UTF8 *methodSig = J9ROMMETHOD_SIGNATURE(romMethod);
	
					Trc_VM_internalCreateRAMClassFromROMClass_createRAMMethod(vmThread, J9UTF8_LENGTH(className), J9UTF8_DATA(className), J9UTF8_LENGTH(methodName),
							J9UTF8_DATA(methodName), J9UTF8_LENGTH(methodSig), J9UTF8_DATA(methodSig), currentRAMMethod);
					
					currentRAMMethod->bytecodes = (U_8 *)(romMethod + 1);
					currentRAMMethod->constantPool = ramConstantPool;
					currentRAMMethod++;
					
					romMethod = nextROMMethod(romMethod);
				}
			}
			
			if (ramConstantPoolCount != 0) {
				ramConstantPool->ramClass = ramClass;
				ramConstantPool->romConstantPool = (J9ROMConstantPoolItem *)(romClass + 1);
			}


			/*
			 * classDepthAndFlags - what does each bit represent?
			 *
			 * 0000 0000 0000 0000 0000 0000 0000 0000
			 *                                       + depth
			 *                                      + depth
			 *                                     + depth
			 *                                    + depth
			 *
			 *                                  + depth
			 *                                 + depth
			 *                                + depth
			 *                               + depth
			 *
			 *                             + depth
			 *                            + depth
			 *                           + depth
			 *                          + depth
			 *
			 *                        + depth
			 *                       + depth
			 *                      + depth
			 *                     + depth
			 *
			 *                   + AccClassRAMArray (based on romClass->modifiers, not inherited)
			 *                  + shape (from romClass->instanceShape, not inherited)
			 *                 + shape (from romClass->instanceShape, not inherited)
			 *                + shape (from romClass->instanceShape, not inherited)
			 *
			 *              + AccClassHasBeenOverridden (set during internal class load hook, not inherited)
			 *             + AccClassOwnableSynchronizer (set during internal class load hook and inherited)
			 *            + AccClassHasJDBCNatives (set during native method binding, not inherited)
			 *           + AccClassGCSpecial (set during internal class load hook and inherited)
			 *
			 *         + AccClassNeedsPerTenantInitialization (from romClass->extraModifiers and inherited)
			 *        + AccClassHasFinalFields (from romClass->extraModifiers and inherited)
			 *       + AccClassHotSwappedOut (not set during creation, not inherited)
			 *      + AccClassDying (not set during creation, inherited but that can't actually occur)
			 *
			 *    + reference (from romClass->extraModifiers, not inherited)
			 *   + reference (from romClass->extraModifiers, not inherited)
			 *  + AccClassFinalizeNeeded (from romClass->extraModifiers and inherited, cleared for empty finalize)
			 * + AccClassCloneable (from romClass->extraModifiers and inherited)

			 * extendedClassFlags - what does each bit represent?
			 *
			 * 0000 0000 0000 0000 0000 0000 0000 0000
			 *                                       + DoNotAttemptToSetInitCache
			 *                                      + Unused
			 *                                     + ClassReusedStatics
			 *                                    + ClassContainsJittedMethods
			 *
			 *                                  + ClassContainsMethodsPresentInMCCHash
			 *                                 + ClassGCScanned
			 *                                + ClassIsAnonymous
			 *                               + Unused
			 *
			 *                             + Unused
			 *                            + Unused
			 *                           + Unused
			 *                          + Unused
			 *
			 *                        + Unused
			 *                       + Unused
			 *                      + Unused
			 *                     + Unused
			 *
			 *                   + Unused
			 *                  + Unused
			 *                 + Unused
			 *                + Unused
			 *
			 *              + Unused
			 *             + Unused
			 *            + Unused
			 *           + Unused
			 *
			 *         + Unused
			 *        + Unused
			 *       + Unused
			 *      + Unused
			 *
			 *    + Unused
			 *   + Unused
			 *  + Unused
			 * + Unused
			 */

			/* set up the superclass array, totalInstSize and instanceDescription */
			tempClassDepthAndFlags = romClass->extraModifiers;

#if defined(J9VM_GC_FINALIZATION)
			if ((javaVM->jclFlags & J9_JCL_FLAG_FINALIZATION) != J9_JCL_FLAG_FINALIZATION) {
				tempClassDepthAndFlags &= ~J9_JAVA_CLASS_FINALIZE;
			}
#endif
			
			if ((javaVM->jclFlags & J9_JCL_FLAG_REFERENCE_OBJECTS) != J9_JCL_FLAG_REFERENCE_OBJECTS) {
				tempClassDepthAndFlags &= ~J9_JAVA_CLASS_REFERENCE_MASK;
			}

			tempClassDepthAndFlags &= J9_JAVA_CLASS_ROMRAMMASK;
			ramClass->subclassTraversalLink = NULL;
			ramClass->subclassTraversalReverseLink = NULL;
			if (superclass == NULL) {
				ramClass->subclassTraversalLink = ramClass;
				ramClass->subclassTraversalReverseLink = ramClass;
				/* Place a NULL at superclasses[-1] for quick get superclass on java.lang.Object. */
				*ramClass->superclasses++ = superclass;
			} else {
				UDATA superclassCount = J9CLASS_DEPTH(superclass);

				if ((options & J9_FINDCLASS_FLAG_NO_SUBCLASS_LINK) == 0) {
					J9Class* nextLink = superclass->subclassTraversalLink;
					
					ramClass->subclassTraversalLink = nextLink;
					nextLink->subclassTraversalReverseLink = ramClass;
					superclass->subclassTraversalLink = ramClass;
					ramClass->subclassTraversalReverseLink = superclass;
				} else {
					ramClass->subclassTraversalLink = ramClass;
					ramClass->subclassTraversalReverseLink = ramClass;
				}
				/* fill in class depth */
				tempClassDepthAndFlags |= superclass->classDepthAndFlags;
				tempClassDepthAndFlags &= ~(J9_JAVA_CLASS_HOT_SWAPPED_OUT | J9_JAVA_CLASS_HAS_BEEN_OVERRIDDEN | J9_JAVA_CLASS_HAS_JDBC_NATIVES | J9_JAVA_CLASS_RAM_ARRAY | (OBJECT_HEADER_SHAPE_MASK << J9_JAVA_CLASS_RAM_SHAPE_SHIFT));
				tempClassDepthAndFlags++;
				
#if defined(J9VM_GC_FINALIZATION)
				/* if this class has an empty finalize() method, make sure not to inherit javaFlagsClassFinalizeNeeded from the superclass */
				if (J9ROMCLASS_HAS_EMPTY_FINALIZE(romClass)) {
					tempClassDepthAndFlags &= ~J9_JAVA_CLASS_FINALIZE;
				}
#endif
				
				/* fill in superclass array */
				if (superclassCount != 0) {
					memcpy(ramClass->superclasses, superclass->superclasses, superclassCount * sizeof(UDATA));
				}
				ramClass->superclasses[superclassCount] = superclass;
			}
			tempClassDepthAndFlags |= ((romClass->instanceShape & OBJECT_HEADER_SHAPE_MASK) << J9_JAVA_CLASS_RAM_SHAPE_SHIFT);
			if (J9ROMCLASS_IS_ARRAY(romClass)) {
				tempClassDepthAndFlags |= J9_JAVA_CLASS_RAM_ARRAY;
			}

			
			ramClass->classDepthAndFlags = tempClassDepthAndFlags;

			if (!fastHCR) {
				/* calculate the instanceDescription field */
				calculateInstanceDescription(vmThread, ramClass, superclass, instanceDescription, &romWalkState, romWalkResult);
			}

			/* fill in the classLoader slot */
			ramClass->classLoader = classLoader;

			/* fill in the packageID */
			ramClass->packageID = packageID;
			
			/* Initialize the method send targets (requires itable built for AOT) */
			if (romClass->romMethodCount != 0) {
				UDATA i;
				UDATA count = romClass->romMethodCount;
				J9Method *ramMethod = ramClass->ramMethods;
				for (i = 0; i< count; i++) {
					initializeMethodRunAddress(vmThread, ramMethod);
					ramMethod++;
				}
			}
			
			/* fill in the vtable */
			if (fastHCR) {
				vTable = (UDATA *)(ramClass + 1);
				*vTable = 0;
			} else {
				copyVTable(vmThread, ramClass, superclass, vTable, defaultConflictCount);
			}

			if (!fastHCR) {
				/* Fill in the itable. This will unmark the linked interfaces. */
				initializeRAMClassITable(vmThread, ramClass, superclass, iTable, interfaceHead, maxInterfaceDepth);
			}
			/* Ensure that lastITable is never NULL */
			ramClass->lastITable = (J9ITable *) ramClass->iTable;
			if (NULL == ramClass->lastITable) {
				ramClass->lastITable = (J9ITable *) &invalidITable;
			}

			if (foundCloneable) {
				ramClass->classDepthAndFlags |= J9_JAVA_CLASS_CLONEABLE;
			}

			/* Prevent certain classes from having Cloneable subclasses */

			if (J9CLASS_FLAGS(ramClass) & J9_JAVA_CLASS_CLONEABLE) {
				static UDATA uncloneableClasses[] = {
						J9VMCONSTANTPOOL_JAVALANGCLASSLOADER,
						J9VMCONSTANTPOOL_JAVALANGTHREAD,
				};
				UDATA i;

				for (i = 0; i < sizeof(uncloneableClasses) / sizeof(UDATA); i++) {
					J9Class * uncloneableClass = J9VMCONSTANTPOOL_CLASSREF_AT(javaVM, uncloneableClasses[i])->value;
					if (uncloneableClass != NULL) {
						UDATA uncloneableClassDepth = J9CLASS_DEPTH(uncloneableClass);
						UDATA currentClassDepth = J9CLASS_DEPTH(ramClass);
					
						if ((currentClassDepth > uncloneableClassDepth) && (ramClass->superclasses[uncloneableClassDepth]) == uncloneableClass) {
							ramClass->classDepthAndFlags &= ~J9_JAVA_CLASS_CLONEABLE;
							break;
						}
					}
				}
			}

			/* run pre-init (requires vTable to be in place) */
			internalRunPreInitInstructions(ramClass, vmThread);
			
			ramClass->initializeStatus = J9ClassInitUnverified;
			
			/* Mark array and primitive classes as fully initialized. */
			if (J9ROMCLASS_IS_PRIMITIVE_OR_ARRAY(romClass)) {
				ramClass->initializeStatus = J9ClassInitSucceeded;
			}
			
			Trc_VM_initializeRAMClass_End(vmThread, J9UTF8_LENGTH(className), J9UTF8_DATA(className), classSize, classLoader);
		}
		
		if (FALSE == J9ROMCLASS_IS_PRIMITIVE_OR_ARRAY(romClass)) {
#if defined(J9VM_INTERP_CUSTOM_SPIN_OPTIONS)
			ramClass->customSpinOption = NULL;
			if (NULL != javaVM->customSpinOptions) {
				pool_do(javaVM->customSpinOptions, checkForCustomSpinOptions, (void *)ramClass);
				J9VMCustomSpinOptions *option = (J9VMCustomSpinOptions *)ramClass->customSpinOption;
				if (NULL != option) {
					const J9ObjectMonitorCustomSpinOptions *const j9monitorOptions = &option->j9monitorOptions;
					Trc_VM_CreateRAMClassFromROMClass_CustomSpinOption(option->className,
																	   j9monitorOptions->thrMaxSpins1BeforeBlocking,
																	   j9monitorOptions->thrMaxSpins2BeforeBlocking,
																	   j9monitorOptions->thrMaxYieldsBeforeBlocking,
																	   j9monitorOptions->thrMaxTryEnterSpins1BeforeBlocking,
																	   j9monitorOptions->thrMaxTryEnterSpins2BeforeBlocking,
																	   j9monitorOptions->thrMaxTryEnterYieldsBeforeBlocking);
#if defined(OMR_THR_CUSTOM_SPIN_OPTIONS)
					const J9ThreadCustomSpinOptions *const j9threadOptions = &option->j9threadOptions;
#if defined(OMR_THR_THREE_TIER_LOCKING)																	   
					Trc_VM_CreateRAMClassFromROMClass_CustomSpinOption2(option->className,																	
																	   j9threadOptions->customThreeTierSpinCount1,
																	   j9threadOptions->customThreeTierSpinCount2,
																	   j9threadOptions->customThreeTierSpinCount3);
#endif /* OMR_THR_THREE_TIER_LOCKING */
#if defined(OMR_THR_ADAPTIVE_SPIN)																	   
					Trc_VM_CreateRAMClassFromROMClass_CustomSpinOption3(option->className,																													   
																 	   j9threadOptions->customAdaptSpin);
#endif /* OMR_THR_ADAPTIVE_SPIN */
#endif /* OMR_THR_CUSTOM_SPIN_OPTIONS */																 	 
				}
			}
#endif /* J9VM_INTERP_CUSTOM_SPIN_OPTIONS */

			if ((javaVM->runtimeFlags & J9_RUNTIME_VERIFY) != 0) {
				J9Class *existingClass = j9bcv_satisfyClassLoadingConstraint(vmThread, classLoader, ramClass);
				if (existingClass != NULL) {
					Trc_VM_CreateRAMClassFromROMClass_classLoadingConstraintViolation(vmThread);
					state->ramClass = NULL;
					if (hotswapping) {
						return internalCreateRAMClassDone(vmThread, classLoader, romClass, options, elementClass, className, state);
					}
					popFromClassLoadingStack(vmThread);
					omrthread_monitor_exit(javaVM->classTableMutex);
					setClassLoadingConstraintError(vmThread, classLoader, existingClass);
					return internalCreateRAMClassDoneNoMutex(vmThread, romClass, options, state);
				}
			}
			if (classLoader == javaVM->systemClassLoader) {
				/* We don't add class location entries with locationType, LOAD_LOCATION_UNKNOWN, in the classLocationHashTable.
				 * This should save footprint. Same applies for classes created with Unsafe.defineclass or Unsafe.defineAnonClass.
				 */
				if ((NULL == classBeingRedefined) && (LOAD_LOCATION_UNKNOWN != locationType) && (!J9ROMCLASS_IS_UNSAFE(romClass))) {
					J9ClassLocation classLocation;

					classLocation.clazz = ramClass;
					classLocation.entryIndex = entryIndex;
					classLocation.locationType = locationType;

					omrthread_monitor_enter(javaVM->classLoaderModuleAndLocationMutex);
					/* Failure to add ClassLocation to hashtable is not a fatal error */
					hashTableAdd(classLoader->classLocationHashTable, (void *)&classLocation);
					omrthread_monitor_exit(javaVM->classLoaderModuleAndLocationMutex);
				}
			}
			length = packageNameLength(romClass);

			if (J2SE_VERSION(javaVM) >= J2SE_19) {
				if (NULL == classBeingRedefined) {
					if (J9_ARE_ALL_BITS_SET(javaVM->runtimeFlags, J9_RUNTIME_JAVA_BASE_MODULE_CREATED)) {
						/* VM does not create J9Module for unnamed modules except the unnamed module for bootloader.
 						 * Therefore J9Class.module should be NULL for classes loaded from classpath by non-bootloaders.
						 * The call to findModuleForPackage() should correctly set J9Class.module as it would return
						 * NULL for classes loaded from classpath.
						 * For bootloader, unnamed module is represented by J9JavaVM.unamedModuleForSystemLoader.
						 * Therefore for classes loaded by bootloader from boot classpath, 
						 * J9Class.module should be set to J9JavaVM.unamedModuleForSystemLoader. 
						 */
						if (J9_ARE_ALL_BITS_SET(options, J9_FINDCLASS_FLAG_ANON)) {
							ramClass->module = hostClass->module;
						} else if ((classLoader != javaVM->systemClassLoader)
							|| ((LOAD_LOCATION_PATCH_PATH == locationType) || (LOAD_LOCATION_MODULE == locationType))
						) {
							omrthread_monitor_enter(javaVM->classLoaderModuleAndLocationMutex);
							ramClass->module = findModuleForPackage(vmThread, classLoader, (U_8 *)J9UTF8_DATA(className), (U_32)length);
							omrthread_monitor_exit(javaVM->classLoaderModuleAndLocationMutex);
						} else {
							ramClass->module = javaVM->unamedModuleForSystemLoader;
						}
					} else {
						if ((LOAD_LOCATION_PATCH_PATH == locationType) || (LOAD_LOCATION_MODULE == locationType)) {
							ramClass->module = javaVM->javaBaseModule;
						} else {
							ramClass->module = javaVM->unamedModuleForSystemLoader;
						}
					}
					if (TrcEnabled_Trc_MODULE_setting_package) {
						trcModulesSettingPackage(vmThread, ramClass, classLoader, className);
					}
				}
			}
		} else {
			if (J9ROMCLASS_IS_ARRAY(romClass)) {
				/* fill in the special array class fields */
				UDATA arity;
				J9Class *leafComponentType;
				J9ArrayClass *ramArrayClass = (J9ArrayClass *)ramClass;
				J9ArrayClass *elementArrayClass = (J9ArrayClass *)elementClass;
				/* Is the elementClass an array or an object? */
				if ((elementArrayClass->romClass->instanceShape & OBJECT_HEADER_INDEXABLE) == OBJECT_HEADER_INDEXABLE) {
					arity = elementArrayClass->arity + 1;
					leafComponentType = elementArrayClass->leafComponentType; 
				} else {
					arity = 1;
					leafComponentType = elementClass;
				}
				ramArrayClass->leafComponentType = leafComponentType;
				ramArrayClass->arity = arity;
				ramArrayClass->componentType = elementClass;
				ramArrayClass->module = leafComponentType->module;
				
#if defined(J9VM_THR_LOCK_NURSERY) && defined(J9VM_THR_LOCK_NURSERY_FAT_ARRAYS)
				ramArrayClass->lockOffset = (UDATA)TMP_OFFSETOF_J9INDEXABLEOBJECT_MONITOR;
#endif
			}
		}
	}

	return internalCreateRAMClassDone(vmThread, classLoader, romClass, options, elementClass, className, state);
}

/**
 * Allocates and fills in the relevant fields of a base type reflect class.
 *
 * NOTE: You must own the class table mutex before calling this.  It will be released
 * when the function returns.
 */
J9Class *   
internalCreateRAMClassFromROMClass(J9VMThread *vmThread, J9ClassLoader *classLoader, J9ROMClass *romClass,
	UDATA options, J9Class *elementClass, j9object_t protectionDomain, J9ROMMethod **methodRemapArray,
	IDATA entryIndex, I_32 locationType, J9Class *classBeingRedefined, J9Class *hostClass)
{
	J9JavaVM *javaVM = vmThread->javaVM;
	J9Class *superclass;
	J9UTF8 *className;
	UDATA packageID;
	UDATA classPreloadFlags = 0;
	BOOLEAN hotswapping = (0 != (options & J9_FINDCLASS_FLAG_NO_DEBUG_EVENTS));
	BOOLEAN fastHCR = (0 != (options & J9_FINDCLASS_FLAG_FAST_HCR));
	J9CreateRAMClassState state;
	J9Class *result;
	J9ClassLoader* hostClassLoader = classLoader;

	/* if this is an anon class classLoader should be anonClassLoader */
	if (J9_ARE_ALL_BITS_SET(options, J9_FINDCLASS_FLAG_ANON)) {
		classLoader = javaVM->anonClassLoader;
	}

	memset(&state, 0, sizeof(state));

	Trc_VM_CreateRAMClassFromROMClass_Entry(vmThread, romClass, classLoader);
	
	/* If this class is being loaded due to hotswap, then we have exclusive access, so do not run any java code. */
	if (hotswapping) {
		classPreloadFlags = J9_FINDCLASS_FLAG_EXISTING_ONLY;
	} else {
		classPreloadFlags = J9_FINDCLASS_FLAG_THROW_ON_FAIL;
		if (classLoader != javaVM->systemClassLoader) {
			/* Non-bootstrap loader must check superclass/superinterfaces for package access. */
			classPreloadFlags |= J9_FINDCLASS_FLAG_CHECK_PKG_ACCESS;
		}
	}

	/* if elementClass is non-null then this is an array */

	/* OTHER THINGS NOT DONE:
	 *	1) Magic number for RAM segment size
	 */

	/* push */
	PUSH_OBJECT_IN_SPECIAL_FRAME(vmThread, protectionDomain);

	className = J9ROMCLASS_CLASSNAME(romClass);
	Trc_VM_CreateRAMClassFromROMClass_className(vmThread, romClass, J9UTF8_LENGTH(className), J9UTF8_DATA(className));

retry:
	if (!hotswapping) {
		/* check to see if this class is already in my list */
		if (!verifyClassLoadingStack(vmThread, classLoader, romClass)) {
			return internalCreateRAMClassDropAndReturn(vmThread, romClass, &state);
		}
	}

	if (fastHCR) {
		packageID = classBeingRedefined->packageID;
	} else {
		/*
		 * The invocation of initializePackageID needs to be protected by classTableMutex
		 * as it may add/grow the classAndPackageID hashtable
		 */
		packageID = initializePackageID(hostClassLoader, romClass, vmThread, entryIndex, locationType);
	}

	/* To prevent deadlock, release the classTableMutex before loading the classes required for the new class. */
	omrthread_monitor_exit(javaVM->classTableMutex);


	if (!loadSuperClassAndInterfaces(vmThread, hostClassLoader, romClass, elementClass, packageID, hotswapping, classPreloadFlags, &superclass)) {
		omrthread_monitor_enter(javaVM->classTableMutex);
		return internalCreateRAMClassDone(vmThread, classLoader, romClass, options, elementClass, className, &state);
	}

	
	result = internalCreateRAMClassFromROMClassImpl(vmThread, classLoader, romClass, options, elementClass,
		methodRemapArray, entryIndex, locationType, classBeingRedefined, packageID, superclass, &state, hostClassLoader, hostClass);
	if (state.retry) {
		goto retry;
	}

#if defined(J9VM_OPT_VALHALLA_MVT)
	/* If the class is value capable (VCC), derive a value type (DVT) */
	if (J9_ARE_ALL_BITS_SET(romClass->extraModifiers, J9AccClassIsValueCapable)) {
		/* The class is expected to be on the class loading stack, but has already been popped. Push again. */
		if (!hotswapping) {
			/* check to see if this class is already in my list */
			if (!verifyClassLoadingStack(vmThread, classLoader, romClass)) {
				return internalCreateRAMClassDropAndReturn(vmThread, romClass, &state);
			}
		}

		/* Create J9Class with DVT option */
		UDATA customOptions = (options | J9_FINDCLASS_FLAG_DERIVED_VALUE_TYPE);
		J9Class *derivedValueType = internalCreateRAMClassFromROMClassImpl(vmThread, classLoader, romClass, customOptions, elementClass,
				methodRemapArray, entryIndex, locationType, classBeingRedefined, packageID, superclass, &state, hostClassLoader, hostClass);
		derivedValueType->classFlags |= J9ClassIsDerivedValueType;

		/* Connect the VCC and the DVT */
		result->derivedValueType = derivedValueType;
		derivedValueType->derivedValueType = result;
	}
#endif /* defined(J9VM_OPT_VALHALLA_MVT) */

	if ((NULL != result) && (0 != (J9_FINDCLASS_FLAG_ANON & options))) {
		/* if anonClass replace classLoader with hostClassLoader, no one can know about anonClassLoader */
		result->classLoader = hostClassLoader;
		if (NULL != result->classObject) {
			/* no object is created when doing hotswapping */
			J9VMJAVALANGCLASS_SET_CLASSLOADER(vmThread, result->classObject, hostClassLoader->classLoaderObject);
		}
		result->classFlags |= J9ClassIsAnonymous;
	}

	return result;
}

static VMINLINE void
addBlockToLargeFreeList(J9ClassLoader *classLoader, J9RAMClassFreeListLargeBlock *block)
{
	J9RAMClassFreeListLargeBlock *tailBlock = (J9RAMClassFreeListLargeBlock *) classLoader->ramClassLargeBlockFreeList;

	block->nextFreeListBlock = tailBlock;
	classLoader->ramClassLargeBlockFreeList = (J9RAMClassFreeListBlock *) block;

	if ((NULL != tailBlock) && (tailBlock->maxSizeInList > block->size)) {
		block->maxSizeInList = tailBlock->maxSizeInList;
	} else {
		block->maxSizeInList = block->size;
	}
}

static void
addBlockToFreeList(J9ClassLoader *classLoader, UDATA address, UDATA size)
{
	if (J9_ARE_ANY_BITS_SET(classLoader->flags, J9CLASSLOADER_ANON_CLASS_LOADER)) {
		/* We support individual class unloading for anonymous classes, so each anonymous class
		 * has its own segment. We therefore don't use free blocks from these segments.
		 */
		return;
	}
	if (sizeof(UDATA) == size) {
		UDATA *block = (UDATA *) address;
		*block = (UDATA) classLoader->ramClassUDATABlockFreeList;
		classLoader->ramClassUDATABlockFreeList = block;
	} else if (sizeof(J9RAMClassFreeListBlock) <= size) {
		J9RAMClassFreeListBlock *block = (J9RAMClassFreeListBlock *) address;
		block->size = size;
		if (RAM_CLASS_SMALL_FRAGMENT_LIMIT > size) {
			block->nextFreeListBlock = classLoader->ramClassTinyBlockFreeList;
			classLoader->ramClassTinyBlockFreeList = block;
		} else if (RAM_CLASS_FRAGMENT_LIMIT > size) {
			block->nextFreeListBlock = classLoader->ramClassSmallBlockFreeList;
			classLoader->ramClassSmallBlockFreeList = block;
		} else {
			addBlockToLargeFreeList(classLoader, (J9RAMClassFreeListLargeBlock *) block);
		}
	}
}

/**
 * Removes freeListBlock from ramClassLargeBlockFreeList. If maxSizeInList values need
 * updating, re-adds blocks from the beginning of the list up to freeListBlock to the
 * list to update their maxSizeInList values.
 */
static void
removeBlockFromLargeFreeList(J9ClassLoader *classLoader, J9RAMClassFreeListLargeBlock **freeListBlockPtr, J9RAMClassFreeListLargeBlock *freeListBlock)
{
	J9RAMClassFreeListLargeBlock *nextBlock = freeListBlock->nextFreeListBlock;

	if ((NULL == nextBlock) || (freeListBlock->maxSizeInList != nextBlock->maxSizeInList)) {
		/* Re-compute the maxSizeInList values on earlier blocks by re-adding them to the list. */
		J9RAMClassFreeListLargeBlock *block = (J9RAMClassFreeListLargeBlock *) classLoader->ramClassLargeBlockFreeList;

		classLoader->ramClassLargeBlockFreeList = (J9RAMClassFreeListBlock *) freeListBlock->nextFreeListBlock;
		while (block != freeListBlock) {
			J9RAMClassFreeListLargeBlock *nextBlock = block->nextFreeListBlock;

			addBlockToLargeFreeList(classLoader, block);
			block = nextBlock;
		}
	} else {
		/* Simply remove this block from the list. */
		*freeListBlockPtr = freeListBlock->nextFreeListBlock;
	}
	freeListBlock->nextFreeListBlock = NULL;
}

/**
 * Attempts to allocate the fragment "allocationsRequests[i]" using a free block from "freeList".
 * Assumes prefixSize and alignedSize of requests in "allocationRequests" are padded to sizeof(UDATA).
 * Assumes alignment of requests in "allocationRequests" are powers of 2.
 *
 * Returns TRUE if the fragment was allocated.
 */
static BOOLEAN
allocateRAMClassFragmentFromFreeList(RAMClassAllocationRequest *request, J9RAMClassFreeListBlock **freeList, J9ClassLoader *classLoader)
{
	J9RAMClassFreeListBlock **freeListBlockPtr = freeList;
	J9RAMClassFreeListBlock *freeListBlock = *freeListBlockPtr;
	const BOOLEAN islargeBlocksList = (freeList == &classLoader->ramClassLargeBlockFreeList);
	const UDATA alignmentMask = (request->alignment == sizeof(UDATA)) ? 0 : (request->alignment - 1);
	const UDATA prefixSize = request->prefixSize;
	const UDATA fragmentSize = request->fragmentSize;
	const UDATA alignment = request->alignment;

	if (islargeBlocksList) {
		/* Fail fast if the requested size is larger than anything in the free list. */
		if (fragmentSize + alignmentMask > ((J9RAMClassFreeListLargeBlock *) freeListBlock)->maxSizeInList) {
			return FALSE;
		}
	}

	Trc_VM_internalAllocateRAMClass_ScanFreeList(freeListBlock);

	while (NULL != freeListBlock) {
		/* Allocate from the start of the block */
		UDATA addressForAlignedArea = ((UDATA) freeListBlock) + prefixSize;
		UDATA alignmentMod = addressForAlignedArea & alignmentMask;
		UDATA alignmentShift = (0 == alignmentMod) ? 0 : (alignment - alignmentMod);

		if (freeListBlock->size >= alignmentShift + fragmentSize) {
			UDATA newBlockSize = freeListBlock->size - (alignmentShift + fragmentSize);

			request->address = (UDATA *) (addressForAlignedArea + alignmentShift);

			Trc_VM_internalAllocateRAMClass_AllocatedFromFreeList(request->index, freeListBlock, freeListBlock->size, request->address, request->prefixSize, request->alignedSize, request->alignment);

			if (islargeBlocksList) {
				removeBlockFromLargeFreeList(classLoader, (J9RAMClassFreeListLargeBlock **) freeListBlockPtr, (J9RAMClassFreeListLargeBlock *) freeListBlock);
			} else {
				*freeListBlockPtr = freeListBlock->nextFreeListBlock;
				freeListBlock->nextFreeListBlock = NULL;
			}

			/* Add a new block with the remaining space at the start of this block, if any, to an appropriate free list */
			if (0 != alignmentShift) {
				addBlockToFreeList(classLoader, (UDATA) freeListBlock, alignmentShift);
			}

			/* Add a new block with the remaining space at the end of this block, if any, to an appropriate free list */
			if (0 != newBlockSize) {
				addBlockToFreeList(classLoader, ((UDATA) freeListBlock) + alignmentShift + request->fragmentSize, newBlockSize);
			}

			return TRUE;
		}

		/* Advance to the next block */
		freeListBlockPtr = &freeListBlock->nextFreeListBlock;
		freeListBlock = *freeListBlockPtr;
	}

	return FALSE;
}

/**
 * Allocates fragments for a RAM class.
 *
 * Returns a pointer to the segment containing the first RAM class fragment allocated.
 */
static J9MemorySegment *
internalAllocateRAMClass(J9JavaVM *javaVM, J9ClassLoader *classLoader, RAMClassAllocationRequest *allocationRequests, UDATA allocationRequestCount)
{
	J9MemorySegment *segment;
	UDATA classStart, i;
	UDATA fragmentsLeftToAllocate = 0;
	RAMClassAllocationRequest *requests = NULL;
	RAMClassAllocationRequest *request = NULL;
	RAMClassAllocationRequest *prev = NULL;
	RAMClassAllocationRequest dummyHead;
	BOOLEAN isNotLoadedByAnonClassLoader = classLoader != javaVM->anonClassLoader;

	/* Initialize results of allocation requests and ensure sizes are at least UDATA-aligned */
	for (i = 0; i < allocationRequestCount; i++) {
		static const UDATA slotSizeMask = sizeof(UDATA) - 1;
		UDATA prefixMod = allocationRequests[i].prefixSize & slotSizeMask;
		UDATA alignedMod = allocationRequests[i].alignedSize & slotSizeMask;

		if (0 != prefixMod) {
			allocationRequests[i].prefixSize += sizeof(UDATA) - prefixMod;
		}

		if (0 != alignedMod) {
			allocationRequests[i].alignedSize += sizeof(UDATA) - alignedMod;
		}

		allocationRequests[i].next = NULL;
		allocationRequests[i].index = i;
		allocationRequests[i].fragmentSize = allocationRequests[i].prefixSize + allocationRequests[i].alignedSize;
		if (0 != allocationRequests[i].fragmentSize) {
			fragmentsLeftToAllocate++;
			/* Order requests by size */
			if ((NULL == requests) || (allocationRequests[i].fragmentSize > requests->fragmentSize)) {
				allocationRequests[i].next = requests;
				requests = allocationRequests + i;
			} else {
				for (prev = requests; NULL != prev->next; prev = prev->next) {
					if (allocationRequests[i].fragmentSize > prev->next->fragmentSize) {
						allocationRequests[i].next = prev->next;
						prev->next = allocationRequests + i;
						break;
					}
				}
				if (NULL == prev->next) {
					prev->next = allocationRequests + i;
				}
			}
		}
	}

	Trc_VM_internalAllocateRAMClass_Entry(classLoader, fragmentsLeftToAllocate);
	/* make sure we always make a new segment if its an anonClass */

	if (isNotLoadedByAnonClassLoader) {
		dummyHead.next = requests;
		prev = &dummyHead;
		for (request = requests; NULL != request; request = request->next) {
			if ((sizeof(UDATA) == request->fragmentSize) && (NULL != classLoader->ramClassUDATABlockFreeList)) {
				UDATA *block = classLoader->ramClassUDATABlockFreeList;
				if (sizeof(UDATA) == request->alignment) {
					request->address = classLoader->ramClassUDATABlockFreeList;
					classLoader->ramClassUDATABlockFreeList = *(UDATA **) classLoader->ramClassUDATABlockFreeList;
				} else {
					UDATA **blockPtr = &classLoader->ramClassUDATABlockFreeList;
					while (NULL != block) {
						/* Check alignment constraint */
						if (0 == (((UDATA) block) & (request->alignment - 1))) {
							/* Unhook block from list */
							*blockPtr = *(UDATA **) block;
							*block = (UDATA) NULL;

							/* Record allocation & adjust for alignment */
							request->address = block;
							break;
						}

						/* Advance to next block */
						blockPtr = *(UDATA ***) block;
						block = *blockPtr;
					}
				}
				if (NULL != request->address) {
					if (request->prefixSize != 0) {
						request->address++;
					}
					Trc_VM_internalAllocateRAMClass_AllocatedFromFreeList(request->index, block, sizeof(UDATA), request->address, request->prefixSize, request->alignedSize, request->alignment);
					prev->next = request->next;
					continue;
				}
			}
			if ((RAM_CLASS_SMALL_FRAGMENT_LIMIT > request->fragmentSize) && (NULL != classLoader->ramClassTinyBlockFreeList)) {
				if (allocateRAMClassFragmentFromFreeList(request, &classLoader->ramClassTinyBlockFreeList, classLoader)) {
					prev->next = request->next;
					continue;
				}
			}
			/* Avoid scanning the small free block list to allocate RAM class headers. The alignment constraint will rarely be satisfied. */
			if ((RAM_CLASS_FRAGMENT_LIMIT > request->fragmentSize + request->alignment) && (NULL != classLoader->ramClassSmallBlockFreeList)) {
				if (allocateRAMClassFragmentFromFreeList(request, &classLoader->ramClassSmallBlockFreeList, classLoader)) {
					prev->next = request->next;
					continue;
				}
			}
			if (NULL != classLoader->ramClassLargeBlockFreeList) {
				if (allocateRAMClassFragmentFromFreeList(request, &classLoader->ramClassLargeBlockFreeList, classLoader)) {
					prev->next = request->next;
					continue;
				}
			}
			prev = request;
		}
		requests = dummyHead.next;
	}


	/* If any fragments remain unallocated, allocate a new segment to (at least) fit them */
	if (NULL != requests) {
		/* Calculate required space in new segment, including maximum alignment padding */
		UDATA newSegmentSize = 0;
		J9MemorySegment *newSegment = NULL;
		UDATA allocAddress = 0;

		fragmentsLeftToAllocate = 0;
		for (request = requests; NULL != request; request = request->next) {
			fragmentsLeftToAllocate++;
			newSegmentSize += request->fragmentSize + request->alignment;
		}

		/* Add sizeof(UDATA) to hold the "lastAllocatedClass" pointer */
		newSegmentSize += sizeof(UDATA);

		/* Allocate a new segment of the required size */

		UDATA classAllocationIncrement = javaVM->ramClassAllocationIncrement;
		if (!isNotLoadedByAnonClassLoader) {
			classAllocationIncrement = 0;
		}

		Trc_VM_internalAllocateRAMClass_AllocateClassMemorySegment(fragmentsLeftToAllocate, newSegmentSize, classAllocationIncrement);
		newSegment = allocateClassMemorySegment(javaVM, newSegmentSize, MEMORY_TYPE_RAM_CLASS, classLoader, classAllocationIncrement);

		if (NULL == newSegment) {
			/* Free allocated fragments */
			/* TODO attempt to coalesce free blocks? */
			for (i = 0; i < allocationRequestCount; i++) {
				if (NULL != allocationRequests[i].address) {
					UDATA fragmentAddress = ((UDATA) allocationRequests[i].address) - allocationRequests[i].prefixSize;
					addBlockToFreeList(classLoader, fragmentAddress, allocationRequests[i].fragmentSize);
					allocationRequests[i].address = NULL;
				}
			}
			Trc_VM_internalAllocateRAMClass_SegmentAllocationFailed();
			return NULL;
		}
		Trc_VM_internalAllocateRAMClass_AllocatedClassMemorySegment(newSegment, newSegment->size, newSegment->heapBase, newSegment->heapTop);

		/* Initialize the "lastAllocatedClass" pointer */
		*(J9Class **) newSegment->heapBase = NULL;

		/* Bump the heapAlloc pointer to the end - don't use it again */
		newSegment->heapAlloc = newSegment->heapTop;

		/* Allocate the remaining fragments in the new segment, adding holes to the free list */
		allocAddress = ((UDATA) newSegment->heapBase) + sizeof(UDATA);
		for (request = requests; NULL != request; request = request->next) {
			/* Allocate from the start of the segment */
			UDATA addressForAlignedArea = allocAddress + request->prefixSize;
			UDATA alignmentMod = addressForAlignedArea & (request->alignment - 1);
			UDATA alignmentShift = (0 == alignmentMod) ? 0 : (request->alignment - alignmentMod);

			request->address = (UDATA *) (addressForAlignedArea + alignmentShift);

			Trc_VM_internalAllocateRAMClass_AllocatedFromNewSegment(request->index, newSegment, request->address, request->prefixSize, request->alignedSize, request->alignment);

			/* Add a new block with the remaining space at the start of this block, if any, to an appropriate free list */
			if (0 != alignmentShift) {
				addBlockToFreeList(classLoader, (UDATA) allocAddress, alignmentShift);
			}

			allocAddress += alignmentShift + request->fragmentSize;

			fragmentsLeftToAllocate--;
		}

		/* Add a new block with the remaining space at the end of this segment, if any, to an appropriate free list */
		if (allocAddress != (UDATA) newSegment->heapTop) {
			addBlockToFreeList(classLoader, allocAddress, ((UDATA) newSegment->heapTop) - allocAddress);
		}
	}

	/* Clear all allocated fragments */
	for (i = 0; i < allocationRequestCount; i++) {
		if (NULL != allocationRequests[i].address) {
			memset((void *) (((UDATA) allocationRequests[i].address) - allocationRequests[i].prefixSize), 0, allocationRequests[i].fragmentSize);
		}
	}

	/* Find the segment containing the first fragment */
	classStart = (UDATA)allocationRequests[0].address;

	omrthread_monitor_enter(javaVM->classMemorySegments->segmentMutex);
	
	for (segment = classLoader->classSegments; NULL != segment; segment = segment->nextSegmentInClassLoader) {
		if (MEMORY_TYPE_RAM_CLASS == (segment->type & MEMORY_TYPE_RAM_CLASS)) {
			if ((((UDATA)segment->heapBase) < classStart) && (classStart < (UDATA)segment->heapTop)) {
				/* Segment for allocation found */
				break;
			}
		}
	}

	omrthread_monitor_exit(javaVM->classMemorySegments->segmentMutex);

	Trc_VM_internalAllocateRAMClass_Exit(classStart, segment);

	return segment;
}

} /* extern "C" */
