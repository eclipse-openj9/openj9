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
#include "j9vmnls.h"
#include "j2sever.h"
#include "vm_internal.h"

#include "VMHelpers.hpp"

#undef J9VM_TRACE_VTABLE_ACCESS

extern "C" {

#define VTABLE_SLOT_TAG_MASK 				3
#define ROM_METHOD_ID_TAG 					1
#define DEFAULT_CONFLICT_METHOD_ID_TAG 		2
#define EQUIVALENT_SET_ID_TAG 				3
#define INTERFACE_TAG 1

#define LOCAL_INTERFACE_ARRAY_SIZE 10

#define DEFAULT_SIZE_OF_CLASS_CACHE 8

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
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
	RAM_CLASS_FLATTENED_CLASS_CACHE,
#endif /* J9VM_OPT_VALHALLA_VALUE_TYPES */
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
static UDATA addInterfaceMethods(J9VMThread *vmStruct, J9ClassLoader *classLoader, J9Class *interfaceClass, UDATA vTableMethodCount, UDATA *vTableAddress, J9Class *superclass, J9ROMClass *romClass, UDATA *defaultConflictCount, J9Pool *equivalentSets, UDATA *equivSetCount, J9OverrideErrorData *errorData);
static UDATA* computeVTable(J9VMThread *vmStruct, J9ClassLoader *classLoader, J9Class *superclass, J9ROMClass *taggedClass, UDATA packageID, J9ROMMethod ** methodRemapArray, J9Class *interfaceHead, UDATA *defaultConflictCount, UDATA interfaceCount, UDATA inheritedInterfaceCount, J9OverrideErrorData *errorData);
static void copyVTable(J9VMThread *vmStruct, J9Class *ramClass, J9Class *superclass, UDATA *vTable, UDATA defaultConflictCount);
static UDATA processVTableMethod(J9VMThread *vmThread, J9ClassLoader *classLoader, UDATA *vTableAddress, J9Class *superclass, J9ROMClass *romClass, J9ROMMethod *romMethod, UDATA localPackageID, UDATA vTableMethodCount, void *storeValue, J9OverrideErrorData *errorData);
static VMINLINE UDATA growNewVTableSlot(UDATA *vTableAddress, UDATA vTableMethodCount, void *storeValue);
static UDATA getVTableIndexForNameAndSigStartingAt(UDATA *vTable, J9UTF8 *name, J9UTF8 *signature, UDATA vTableIndex);
static UDATA checkPackageAccess(J9VMThread *vmThread, J9Class *foundClass, UDATA classPreloadFlags);
static void setCurrentExceptionForBadClass(J9VMThread *vmThread, J9UTF8 *badClassName, UDATA exceptionIndex, U_32 nlsModuleName, U_32 nlsMessageID);
static BOOLEAN verifyClassLoadingStack(J9VMThread *vmThread, J9ClassLoader *classLoader, J9ROMClass *romClass);
static void popFromClassLoadingStack(J9VMThread *vmThread);
static VMINLINE BOOLEAN loadSuperClassAndInterfaces(J9VMThread *vmThread, J9ClassLoader *classLoader, J9ROMClass *romClass, UDATA options, J9Class *elementClass, UDATA packageID, BOOLEAN hotswapping, UDATA classPreloadFlags, J9Class **superclassOut, J9Module *module);
static J9Class* internalCreateRAMClassDropAndReturn(J9VMThread *vmThread, J9ROMClass *romClass, J9CreateRAMClassState *state);
static J9Class* internalCreateRAMClassDoneNoMutex(J9VMThread *vmThread, J9ROMClass *romClass, UDATA options, J9CreateRAMClassState *state);
static J9Class* internalCreateRAMClassDone(J9VMThread *vmThread, J9ClassLoader *classLoader, J9ROMClass *romClass, UDATA options, J9Class *elementClass,
	J9UTF8 *className, J9CreateRAMClassState *state);
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
static BOOLEAN loadFlattenableFieldValueClasses(J9VMThread *vmThread, J9ClassLoader *classLoader, J9ROMClass *romClass, UDATA classPreloadFlags, UDATA packageID, J9Module *module, UDATA *largestFieldType, J9FlattenedClassCache *flattenedClassCache);

static J9Class* internalCreateRAMClassFromROMClassImpl(J9VMThread *vmThread, J9ClassLoader *classLoader, J9ROMClass *romClass, UDATA options, J9Class *elementClass,
	J9ROMMethod **methodRemapArray, IDATA entryIndex, I_32 locationType, J9Class *classBeingRedefined, UDATA packageID, J9Class *superclass, J9CreateRAMClassState *state,
	J9ClassLoader* hostClassLoader, J9Class *hostClass, J9Module *module, J9FlattenedClassCache *flattenedClassCache);
#else /* J9VM_OPT_VALHALLA_VALUE_TYPES */
static J9Class* internalCreateRAMClassFromROMClassImpl(J9VMThread *vmThread, J9ClassLoader *classLoader, J9ROMClass *romClass, UDATA options, J9Class *elementClass,
	J9ROMMethod **methodRemapArray, IDATA entryIndex, I_32 locationType, J9Class *classBeingRedefined, UDATA packageID, J9Class *superclass, J9CreateRAMClassState *state,
	J9ClassLoader* hostClassLoader, J9Class *hostClass, J9Module *module);
#endif /* J9VM_OPT_VALHALLA_VALUE_TYPES */

static J9MemorySegment* internalAllocateRAMClass(J9JavaVM *javaVM, J9ClassLoader *classLoader, RAMClassAllocationRequest *allocationRequests, UDATA allocationRequestCount);
static I_32 interfaceDepthCompare(const void *a, const void *b);
#if defined(J9VM_INTERP_CUSTOM_SPIN_OPTIONS)
static void checkForCustomSpinOptions(void *element, void *userData);
#endif /* J9VM_INTERP_CUSTOM_SPIN_OPTIONS */
static void trcModulesSettingPackage(J9VMThread *vmThread, J9Class *ramClass, J9ClassLoader *classLoader, J9UTF8 *className);

/*
 * A class which extends (perhaps indirectly) the 'magic'
 * accessor class is exempt from the normal access rules.
 */
#if JAVA_SPEC_VERSION == 8
#define MAGIC_ACCESSOR_IMPL "sun/reflect/MagicAccessorImpl"
#else /* JAVA_SPEC_VERSION == 8 */
#define MAGIC_ACCESSOR_IMPL "jdk/internal/reflect/MagicAccessorImpl"
#endif /* JAVA_SPEC_VERSION == 8 */

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
			if ((foundCloneable != NULL) && (J9AccClassCloneable == (J9CLASS_FLAGS(interfaceClass) & J9AccClassCloneable))) {
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
addITableMethods(J9VMThread* vmStruct, J9Class *ramClass, J9Class *interfaceClass, UDATA **currentSlot)
{
	J9ROMClass *interfaceRomClass = interfaceClass->romClass;
	UDATA count = interfaceRomClass->romMethodCount;
	if (count != 0) {
		J9VTableHeader *vTableHeader = J9VTABLE_HEADER_FROM_RAM_CLASS(ramClass);
		UDATA vTableSize = vTableHeader->size;
		J9Method **vTable = J9VTABLE_FROM_HEADER(vTableHeader);
		J9Method *interfaceRamMethod = interfaceClass->ramMethods;
		U_32 *ordering = J9INTERFACECLASS_METHODORDERING(interfaceClass);
		UDATA index = 0;
		while (count-- > 0) {
			if (NULL != ordering) {
				interfaceRamMethod = interfaceClass->ramMethods + ordering[index++];
			}
			J9ROMMethod *interfaceRomMethod = J9_ROM_METHOD_FROM_RAM_METHOD(interfaceRamMethod);
			if (J9ROMMETHOD_IN_ITABLE(interfaceRomMethod)) {
				J9UTF8 *interfaceMethodName = J9ROMMETHOD_NAME(interfaceRomMethod);
				J9UTF8 *interfaceMethodSig = J9ROMMETHOD_SIGNATURE(interfaceRomMethod);
				UDATA vTableOffset = 0;
				UDATA searchIndex = 0;
			
				/* Search the vTable for a public method of the correct name. */
				while (searchIndex < vTableSize) {
					J9Method *vTableRamMethod = vTable[searchIndex];
					J9ROMMethod *vTableRomMethod = J9_ROM_METHOD_FROM_RAM_METHOD(vTableRamMethod);

					if (J9ROMMETHOD_IN_ITABLE(vTableRomMethod)) {
						J9UTF8 *vTableMethodName = J9ROMMETHOD_NAME(vTableRomMethod);
						J9UTF8 *vTableMethodSig = J9ROMMETHOD_SIGNATURE(vTableRomMethod);

						if ((J9UTF8_LENGTH(interfaceMethodName) == J9UTF8_LENGTH(vTableMethodName))
						&& (J9UTF8_LENGTH(interfaceMethodSig) == J9UTF8_LENGTH(vTableMethodSig))
						&& (memcmp(J9UTF8_DATA(interfaceMethodName), J9UTF8_DATA(vTableMethodName), J9UTF8_LENGTH(vTableMethodName)) == 0)
						&& (memcmp(J9UTF8_DATA(interfaceMethodSig), J9UTF8_DATA(vTableMethodSig), J9UTF8_LENGTH(vTableMethodSig)) == 0)
						) {
							/* fill in interface index --> vTableOffset mapping */
							vTableOffset = J9VTABLE_OFFSET_FROM_INDEX(searchIndex);
							**currentSlot = vTableOffset;
							(*currentSlot)++;
							break;
						}
					}
					searchIndex++;
				}

#if defined(J9VM_TRACE_ITABLE)
				{
					PORT_ACCESS_FROM_VMC(vmStruct);
					j9tty_printf(PORTLIB, "\n  map %.*s%.*s to vTableOffset=%d (%d)", J9UTF8_LENGTH(interfaceMethodName),
							J9UTF8_DATA(interfaceMethodName), J9UTF8_LENGTH(interfaceMethodSig), J9UTF8_DATA(interfaceMethodSig), vTableOffset, searchIndex);
				}
#endif
			}
			interfaceRamMethod++;
		}
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
	if (J9AccInterface != (ramClass->romClass->modifiers & J9AccInterface)) {
		/* iTables contain all methods from the local interface, and any interfaces it extends */
		J9ITable *allInterfaces = (J9ITable*)interfaceClass->iTable;
		do {
			addITableMethods(vmStruct, ramClass, allInterfaces->interfaceClass, currentSlot);
			allInterfaces = allInterfaces->next;
		} while (NULL != allInterfaces);
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
		j9tty_printf(PORTLIB, "\n<initializeRAMClassITable: %.*s %d>", J9UTF8_LENGTH(className), J9UTF8_DATA(className), (romClass->modifiers & J9AccInterface));
	}
#endif

	/* All array classes share the same iTable.  If this is an array class and [Z (the first allocated
	 * array class) has been filled in, copy the iTable from [Z.
	 */
	booleanArrayClass = vmStruct->javaVM->booleanArrayClass;
	if (J9ROMCLASS_IS_ARRAY(romClass) && (booleanArrayClass != NULL)) {
		ramClass->iTable = booleanArrayClass->iTable;
		if ((J9CLASS_FLAGS(booleanArrayClass) & J9AccClassCloneable) == J9AccClassCloneable) {
			ramClass->classDepthAndFlags |= J9AccClassCloneable;
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
		if ((romClass->modifiers & J9AccInterface) == J9AccInterface) {
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
 * @return true iff the names and signatures are the same.
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
addInterfaceMethods(J9VMThread *vmStruct, J9ClassLoader *classLoader, J9Class *interfaceClass, UDATA vTableMethodCount, UDATA *vTableAddress, J9Class *superclass, J9ROMClass *romClass, UDATA *defaultConflictCount, J9Pool *equivalentSets, UDATA *equivSetCount, J9OverrideErrorData *errorData)
{
	J9Method **vTableMethods = J9VTABLE_FROM_HEADER(vTableAddress);
	J9ROMClass *interfaceROMClass = interfaceClass->romClass;
	UDATA count = interfaceROMClass->romMethodCount;
	bool verifierEnabled = J9_ARE_ANY_BITS_SET(vmStruct->javaVM->runtimeFlags, J9_RUNTIME_VERIFY);
	
	if (0 != count) {
		const void * conflictRunAddress = J9_BCLOOP_ENCODE_SEND_TARGET(J9_BCLOOP_SEND_TARGET_DEFAULT_CONFLICT);
		J9Method *interfaceMethod = interfaceClass->ramMethods;
		UDATA interfaceDepth = ((J9ITable *)interfaceClass->iTable)->depth;
		UDATA j = 0;
		
		for (j=0; j < count; j++) {
			J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(interfaceMethod);
			/* Ignore the <clinit> from the interface class. */
			if (J9ROMMETHOD_IN_ITABLE(romMethod)) {
				J9UTF8 *interfaceMethodNameUTF = J9ROMMETHOD_NAME(romMethod);
				J9UTF8 *interfaceMethodSigUTF = J9ROMMETHOD_SIGNATURE(romMethod);
				UDATA tempIndex = vTableMethodCount;
				INTERFACE_STATE state = SLOT_IS_INVALID;
				
				/* If the vTable already has a public declaration of the method that isn't
				 * either an abstract interface method or default method conflict, do not
				 * process the interface method at all. */
				while (tempIndex > 0) {
					/* Decrement the index, convert from one based to zero based index */
					tempIndex -= 1;

					J9ROMClass *methodROMClass = NULL;
					J9Class *methodClass = NULL;
					J9ROMMethod *vTableMethod = (J9ROMMethod *)vTableMethods[tempIndex];

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
							vTableMethods[tempIndex] = interfaceMethod;
							goto continueInterfaceScan;
						case SLOT_IS_INTERFACE_RAM_METHOD:
							/* Here we need to determine if this is a potential conflict or equivalent set */
							if (((J9ITable *)methodClass->iTable)->depth < interfaceDepth) {
								/* If the interface method is less deep then the candidate method, the interface method
								 * was filled in by the super class's vtable build.  Replace with the current method
								 * and continue.
								 */
								vTableMethods[tempIndex] = interfaceMethod;
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
									if (J9_ARE_ANY_BITS_SET(combinedModifiers, J9AccAbstract)) {
										/* Convert to equivSet by adding the existing vtable method to the equivSet */
										J9EquivalentEntry *entry = (J9EquivalentEntry*) pool_newElement(equivalentSets);
										if (NULL == entry) {
											/* OOM will be thrown */
											goto fail;
										}
										entry->method = vTableMethods[tempIndex];
										entry->next = NULL;
										vTableMethods[tempIndex] = (J9Method *)((UDATA)entry | EQUIVALENT_SET_ID_TAG);
										/* This will either be the single default method or the first abstract method */
										/* ... and then adding the new method */
										*equivSetCount += 1;
										goto add_existing;
									} else {
										/* Conflict detected */
										vTableMethods[tempIndex] = (J9Method *)((UDATA)vTableMethods[tempIndex] | DEFAULT_CONFLICT_METHOD_ID_TAG);
										*defaultConflictCount += 1;
									}
								}
							}
							break;
						case SLOT_IS_EQUIVSET_TAG: {
add_existing:
							J9EquivalentEntry * existing_entry = (J9EquivalentEntry*)((UDATA)vTableMethods[tempIndex] & ~EQUIVALENT_SET_ID_TAG);
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
						if (J9AccPublic == (vTableMethod->modifiers & J9AccPublic)) {
							if (verifierEnabled) {
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
							}
							goto continueInterfaceScan;
						}
					}
				}
				/* Add the interface method as the Local class does not implement a public method of the given name. */
				vTableMethodCount = growNewVTableSlot((UDATA *)vTableMethods, vTableMethodCount, interfaceMethod);
#if defined(J9VM_TRACE_VTABLE_ACCESS)
				{
					PORT_ACCESS_FROM_VMC(vmStruct);
					J9UTF8 *classNameUTF = J9ROMCLASS_CLASSNAME(romClass);
					j9tty_printf(PORTLIB, "\n<vtbl_init: adding @ index=%d %.*s.%.*s%.*s (0x%x)>", 
								vTableMethodCount, 
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
	return vTableMethodCount;
fail:
	vTableMethodCount = (UDATA)-1;
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
 * @param[in] vTableMethodCount The number of methods in vtable
 * @param[in] vTableAddress The vtable itself
 */
static void
processEquivalentSets(J9VMThread *vmStruct, UDATA *defaultConflictCount, UDATA vTableMethodCount, UDATA *vTableAddress)
{
	UDATA *vTableMethods = (UDATA *)J9VTABLE_FROM_HEADER(vTableAddress);
	for (UDATA i = 0; i < vTableMethodCount; i++) {
		UDATA slotValue = vTableMethods[i];

		if (EQUIVALENT_SET_ID_TAG == (slotValue & VTABLE_SLOT_TAG_MASK)) {
			/* Process set and determine if there is a method that satisfies or if this is a conflict */
			J9EquivalentEntry *entry = (J9EquivalentEntry*)(slotValue & ~EQUIVALENT_SET_ID_TAG);
			J9Method *candidate = entry->method;
			bool foundNonAbstract = false;
			while (NULL != entry) {
				if (J9_ARE_NO_BITS_SET(J9_ROM_METHOD_FROM_RAM_METHOD(entry->method)->modifiers, J9AccAbstract)) {
					if (foundNonAbstract) {
						/* Two non-abstract methods -> conflict */
						vTableMethods[i] = ((UDATA)entry->method | DEFAULT_CONFLICT_METHOD_ID_TAG);
						*defaultConflictCount += 1;
						goto continueProcessingVTable;
					}
					foundNonAbstract = true;
					candidate = entry->method;
				}
				entry = entry->next;
			}
			/* This will either be the single default method or the first abstract method */
			vTableMethods[i] = (UDATA)candidate;
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
	
	if ((romClass->modifiers & J9AccInterface) == J9AccInterface) {
		maxSlots = 1;
	} else {
		/* All methods in the current class might need new slots in the vTable. */
		maxSlots = romClass->romMethodCount;
		
		/* Add in all real method slots from the superclass. */
		if (superclass != NULL) {
			J9VTableHeader *superVTable = J9VTABLE_HEADER_FROM_RAM_CLASS(superclass);
			maxSlots += superVTable->size;
		}
		
		/* header slots */
		maxSlots += (sizeof(J9VTableHeader) / sizeof(UDATA));

		/* For non-array classes, compute the total possible size of implementing all interface methods. */
		if (J9ROMCLASS_IS_ARRAY(romClass) == 0) {
			J9Class *interfaceWalk = interfaceHead;
			while (interfaceWalk != NULL) {
				maxSlots += J9INTERFACECLASS_ITABLEMETHODCOUNT(interfaceWalk);
				interfaceWalk = (J9Class *)((UDATA)interfaceWalk->instanceDescription & ~INTERFACE_TAG);
			}
		}
	}

	/* convert slots to bytes */
	maxSlots *= sizeof(UDATA);
	
#if defined(J9VM_INTERP_HOT_CODE_REPLACEMENT)
	if (taggedClass != romClass) {
		vTableAddress = (UDATA *)J9VTABLE_HEADER_FROM_RAM_CLASS(taggedClass);
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
		UDATA vTableMethodCount = 0;
		J9VTableHeader *vTableHeader = (J9VTableHeader *)vTableAddress;

		/* Write size of 0 */
		if (J9AccInterface == (romClass->modifiers & J9AccInterface)) {
			vTableHeader->size = 0;
			goto done;
		}

		if (superclass == NULL) {
			/* no inherited slots, write default slot in header */
			vTableHeader->initialVirtualMethod = (J9Method *)vm->initialMethods.initialVirtualMethod;
#if defined(J9VM_OPT_VALHALLA_NESTMATES)
			vTableHeader->invokePrivateMethod = (J9Method *)vm->initialMethods.invokePrivateMethod;
#endif /* J9VM_OPT_VALHALLA_NESTMATES */
		} else {
			J9VTableHeader *superVTable = J9VTABLE_HEADER_FROM_RAM_CLASS(superclass);
			vTableMethodCount = superVTable->size;
			/* + 1 to account for size slot */
			memcpy(vTableAddress, superVTable, ((vTableMethodCount * sizeof(UDATA)) + sizeof(J9VTableHeader)));
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
					if (J9_ARE_ANY_BITS_SET(romMethod->modifiers, J9AccMethodVTable | J9AccPrivate)
					&& J9_ARE_NO_BITS_SET(romMethod->modifiers, J9AccStatic)
					&& ('<' != J9UTF8_DATA(methodName)[0])
					) {
						vTableMethodCount = processVTableMethod(vmStruct, classLoader, vTableAddress, superclass, romClass, romMethod,
								packageID, vTableMethodCount, (J9ROMMethod *)((UDATA)romMethod + ROM_METHOD_ID_TAG), errorData);
						if ((UDATA)-1 == vTableMethodCount) {
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
					vTableMethodCount = addInterfaceMethods(vmStruct, classLoader, interfaces[i - 1], vTableMethodCount, vTableAddress, superclass, romClass, defaultConflictCount, equivalentSet, &equivSetCount, errorData);
					if ((UDATA)-1 == vTableMethodCount) {
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
					processEquivalentSets(vmStruct, defaultConflictCount, vTableMethodCount, vTableAddress);
				}
				if (NULL != equivalentSet) {
					pool_kill(equivalentSet);
				}
				if (interfaces != localBuffer) {
					j9mem_free_memory(interfaces);
				}
			}
			
			/* record number of slots used */
			*vTableAddress = vTableMethodCount;
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
	UDATA superCount = 0;
	UDATA count;
	J9VTableHeader *vTableAddress;
	J9Method **sourceVTable;
	J9Method **newVTable;
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
	
	if (superclass != NULL) {
		/* add superclass vtable size */
		superCount = J9VTABLE_HEADER_FROM_RAM_CLASS(superclass)->size;
	}
	
	count = ((J9VTableHeader *)vTable)->size;
	vTableAddress = J9VTABLE_HEADER_FROM_RAM_CLASS(ramClass);
	*vTableAddress = *(J9VTableHeader *)vTable;

	sourceVTable = J9VTABLE_FROM_HEADER(vTable);
	newVTable = J9VTABLE_FROM_HEADER(vTableAddress);

	for (index = 0; index < count; index++) {
		J9Method *vTableMethod = sourceVTable[index];
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
		
		newVTable[index] = vTableMethod;
		/* once we've walked the inherited entries, we can optimize the ramMethod search
		 * by remembering where the search ended last time. Since the methods occur in
		 * order in the VTable, we can start searching at the previous method
		 */
		if (index >= superCount) {
			ramMethods = vTableMethod;
		}
	}
	
	/* Fill in the JIT vTable */
#if defined(J9VM_INTERP_NATIVE_SUPPORT)
	jitConfig = vmStruct->javaVM->jitConfig;
	if (jitConfig != NULL) {
		UDATA *vTableWriteCursor = JIT_VTABLE_START_ADDRESS(ramClass);

		/* only copy in the real methods */
		UDATA vTableWriteIndex = vTableAddress->size;
		J9Method **vTableReadCursor;
		if (vTableWriteIndex != 0) {
			if ((jitConfig->runtimeFlags & J9JIT_TOSS_CODE) != 0) {
				vTableWriteCursor -= vTableWriteIndex;
			} else {
				UDATA superVTableSize;
				J9Method **superVTableReadCursor;
				UDATA *superVTableWriteCursor = JIT_VTABLE_START_ADDRESS(superclass);
				if (superclass == NULL) {
					superVTableReadCursor = NULL;
					superVTableSize = 0;
				} else {
					superVTableReadCursor = (J9Method **)J9VTABLE_HEADER_FROM_RAM_CLASS(superclass);
					superVTableSize = ((J9VTableHeader *)superVTableReadCursor)->size;
					/* initialize pointer to first real vTable method */
					superVTableReadCursor = J9VTABLE_FROM_HEADER(superVTableReadCursor);
				}
				/* initialize pointer to first real vTable method */
				vTableReadCursor = J9VTABLE_FROM_HEADER(vTableAddress);
				for (; vTableWriteIndex > 0; vTableWriteIndex--) {
					J9Method *currentMethod = *vTableReadCursor;
					if (superclass != NULL && currentMethod == *superVTableReadCursor) {
						*vTableWriteCursor = *superVTableWriteCursor;
					} else {
						fillJITVTableSlot(vmStruct, vTableWriteCursor, currentMethod);
					}

					/* Always consume an entry from the super vTable.  Note that once the size hits zero and superclass becomes NULL,
					 * superVTableReadCursor will never be dereferenced again, so it's value does not matter.  Also, there's no possibility
					 * of superVTableSize rolling over again (once it's decremented beyond 0), and it wouldn't matter if it did, since
					 * superclass will already be NULL.
					 */
					superVTableSize--;
					if (superVTableSize == 0) {
						superclass = NULL;
					}
					superVTableReadCursor++;
					superVTableWriteCursor--;
					vTableReadCursor++;
					vTableWriteCursor--;
				}
			}
		}
		
		/* The SRP to the start of the RAM class is written by internalAllocateRAMClass() */
	}
#endif
	
#if defined(J9VM_INTERP_HOT_CODE_REPLACEMENT)
	if (vTable != (UDATA *)vTableAddress) {
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
processVTableMethod(J9VMThread *vmThread, J9ClassLoader *classLoader, UDATA *vTableAddress, J9Class *superclass, J9ROMClass *romClass, J9ROMMethod *romMethod, UDATA localPackageID, UDATA vTableMethodCount, void *storeValue, J9OverrideErrorData *errorData)
{
	UDATA newModifiers = romMethod->modifiers;
	bool verifierEnabled = J9_ARE_ANY_BITS_SET(vmThread->javaVM->runtimeFlags, J9_RUNTIME_VERIFY);

	/* Private methods do not appear in the vTable or take part in any overriding decision */
	if (J9_ARE_NO_BITS_SET(newModifiers, J9AccPrivate)) {
		UDATA *vTableMethods = (UDATA *)J9VTABLE_FROM_HEADER(vTableAddress);
		J9UTF8 *nameUTF = J9ROMMETHOD_NAME(romMethod);
		J9UTF8 *sigUTF = J9ROMMETHOD_SIGNATURE(romMethod);
		UDATA newSlotRequired = TRUE;

		if (NULL != superclass) {
			J9VTableHeader *superclassVTable = J9VTABLE_HEADER_FROM_RAM_CLASS(superclass);
			UDATA *superclassVTableMethods = (UDATA *)J9VTABLE_FROM_HEADER(superclassVTable);
			UDATA superclassVTableIndex = superclassVTable->size;

			if (methodIsFinalInObject(J9UTF8_LENGTH(nameUTF), J9UTF8_DATA(nameUTF), J9UTF8_LENGTH(sigUTF), J9UTF8_DATA(sigUTF))) {
				vmThread->tempSlot = (UDATA)romMethod;
			}

			/* See if this method overrides any methods from any superclass. */
			while ((superclassVTableIndex = getVTableIndexForNameAndSigStartingAt(superclassVTableMethods, nameUTF, sigUTF,
					superclassVTableIndex)) != (UDATA)-1)
			{
				UDATA overridden = FALSE;
				/* fetch vTable entry */
				J9Method *superclassVTableMethod = (J9Method *)superclassVTableMethods[superclassVTableIndex];
				J9Class *superclassVTableMethodClass = J9_CLASS_FROM_METHOD(superclassVTableMethod);
				J9ROMMethod *superclassVTableROMMethod = J9_ROM_METHOD_FROM_RAM_METHOD(superclassVTableMethod);
				UDATA modifiers = superclassVTableROMMethod->modifiers;

				/* Look at the vTable of each superclass, not just the immediate one */
				J9Class *currentSuperclass = superclass;
				do {
					J9VTableHeader *currentSuperclassVTable = J9VTABLE_HEADER_FROM_RAM_CLASS(currentSuperclass);
					/* Stop the search if the superclass does not contain an entry at the search index */
					if (currentSuperclassVTable->size <= superclassVTableIndex) {
						break;
					}
					J9Method **currentSuperVTableMethods = J9VTABLE_FROM_HEADER(currentSuperclassVTable);
					J9Method *currentSuperclassVTableMethod = currentSuperVTableMethods[superclassVTableIndex];
					J9Class *currentSuperclassVTableMethodClass = J9_CLASS_FROM_METHOD(currentSuperclassVTableMethod);
					J9ROMMethod *currentSuperclassVTableROMMethod = J9_ROM_METHOD_FROM_RAM_METHOD(currentSuperclassVTableMethod);
					UDATA currentSuperclassModifiers = currentSuperclassVTableROMMethod->modifiers;
					/* Private methods are not stored in the vTable.
					 * Public and protected are always overridden.
					 * Package private (i.e. default) is overridden only by a method in the same package.
					 */
					if (J9_ARE_ANY_BITS_SET(currentSuperclassModifiers, J9AccProtected | J9AccPublic)
						|| (currentSuperclassVTableMethodClass->packageID == localPackageID)
					) {
						overridden = TRUE;
						newSlotRequired = FALSE;
						break;
					}
					/* For class file versions below 51, check only the immediate superclass */
					if (romClass->majorVersion < 51) {
						break;
					}
					currentSuperclass = VM_VMHelpers::getSuperclass(currentSuperclass);
				} while(NULL != currentSuperclass);
				if (overridden) {
					if ((((UDATA)storeValue & VTABLE_SLOT_TAG_MASK) == ROM_METHOD_ID_TAG)
						|| (vTableMethods[superclassVTableIndex] == (UDATA)superclassVTableMethod)
					) {
						if ((modifiers & J9AccFinal) == J9AccFinal) {
							vmThread->tempSlot = (UDATA)romMethod;
						}
						if (verifierEnabled) {
							/* fill in vtable, override parent slot */
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
									vTableMethodCount = (UDATA)-1;
									goto done;
								}
							}
						}
						vTableMethods[superclassVTableIndex] = (UDATA)storeValue;
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
				/* Keep checking the rest of the methods in the superclass.
				 * no need to decrease as the search result is zero based and search input is 1 based
				 */
			}
		}

		/* Package private (i.e. default) methods always requires a new slot unless the new method is final */
		if (J9_ARE_NO_BITS_SET(newModifiers, J9AccProtected | J9AccPublic | J9AccPrivate | J9AccFinal)) {
			newSlotRequired = TRUE;
		}

		/* If the method requires a new slot in the vTable, allocate it */
		if (newSlotRequired) {
			/* allocate vTable slot */
			vTableMethodCount = growNewVTableSlot(vTableMethods, vTableMethodCount, storeValue);
#if defined(J9VM_TRACE_VTABLE_ACCESS)
			{
				PORT_ACCESS_FROM_VMC(vmThread);
				J9UTF8 *classNameUTF = J9ROMCLASS_CLASSNAME(romClass);
				j9tty_printf(PORTLIB, "\n<vtbl_init: adding @ index=%d %.*s.%.*s%.*s (0x%x)>", vTableMethodCount, J9UTF8_LENGTH(classNameUTF),
						J9UTF8_DATA(classNameUTF), J9UTF8_LENGTH(nameUTF), J9UTF8_DATA(nameUTF), J9UTF8_LENGTH(sigUTF), J9UTF8_DATA(sigUTF), romMethod);
			}
#endif
		}
	}
done:
	return vTableMethodCount;
}

/**
 * Grow the VTable by 1 slot and add 'storeValue' to that slot.
 *
 * @param vTableAddress[in] A pointer to the buffer that holds the vtable (must be correctly sized)
 * @param vTableMethodCount[in] The number of method currently in the vtable
 * @param storeValue[in] the value to store into the vtable slot - either a J9Method* or a tagged J9ROMMethod
 * @return The new vTableMethodCount
 */
static VMINLINE UDATA
growNewVTableSlot(UDATA *vTableAddress, UDATA vTableMethodCount, void *storeValue)
{
	/* fill in vtable, add new entry */
	vTableAddress[vTableMethodCount] = (UDATA)storeValue;
	vTableMethodCount += 1;

	return vTableMethodCount;
}

static UDATA
getVTableIndexForNameAndSigStartingAt(UDATA *vTable, J9UTF8 *name, J9UTF8 *signature, UDATA vTableIndex)
{
	U_8 *nameData = J9UTF8_DATA(name);
	UDATA nameLength = J9UTF8_LENGTH(name);
	U_8 *signatureData = J9UTF8_DATA(signature);
	UDATA signatureLength = J9UTF8_LENGTH(signature);

	while (vTableIndex != 0) {
		/* The vTableIndex passed in is 1 based, converting it to zero based search index */
		/* move to previous vTable index */
		vTableIndex -= 1;
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
	}
	return (UDATA)-1;
}

UDATA
getVTableOffsetForMethod(J9Method * method, J9Class *clazz, J9VMThread *vmThread)
{
	UDATA vTableIndex;
	J9Class *methodClass = J9_CLASS_FROM_METHOD(method);
	UDATA modifiers = methodClass->romClass->modifiers;

	/* If this method came from an interface class, handle it specially. */
	if ((modifiers & J9AccInterface) == J9AccInterface) {
		J9VTableHeader *vTable = J9VTABLE_HEADER_FROM_RAM_CLASS(clazz);
		UDATA vTableSize = vTable->size;
		J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
		J9UTF8 *nameUTF = J9ROMMETHOD_NAME(romMethod);
		J9UTF8 *sigUTF = J9ROMMETHOD_SIGNATURE(romMethod);
		if (vTableSize == 0) {
			return 0;
		} else {
			vTableIndex = getVTableIndexForNameAndSigStartingAt((UDATA *)J9VTABLE_FROM_HEADER(vTable), nameUTF, sigUTF, vTableSize);
			if (vTableIndex != (UDATA)-1) {
				return J9VTABLE_OFFSET_FROM_INDEX(vTableIndex);
			}
		}
	} else {
		/* Iterate over the vtable from the end to the beginning, skipping 
		 * the "magic" header entries.  This ensures that the most "recent" override of the
		 * method is found first.  Critically important for super sends 
		 * ie: invokespecial) being correct
		 */
		J9VTableHeader *vTable = J9VTABLE_HEADER_FROM_RAM_CLASS(methodClass);
		J9Method **vTableMethods = J9VTABLE_FROM_HEADER(vTable);
		UDATA vTableIndex = vTable->size;

		while (vTableIndex > 0) {
			vTableIndex -= 1;
			if (method == vTableMethods[vTableIndex]) {
				return J9VTABLE_OFFSET_FROM_INDEX(vTableIndex);
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
 * Sets the current exception using the detailed error message plus the specified class name.
 */
static void
setCurrentExceptionForBadClass(J9VMThread *vmThread, J9UTF8 *badClassName, UDATA exceptionIndex, U_32 nlsModuleName, U_32 nlsMessageID)
{
	PORT_ACCESS_FROM_VMC(vmThread);
	char * errorMsg = NULL;
	const char * nlsMessage = OMRPORT_FROM_J9PORT(PORTLIB)->nls_lookup_message(
			OMRPORT_FROM_J9PORT(PORTLIB),
			J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE,
			nlsModuleName, nlsMessageID,
			NULL);

	if (NULL != nlsMessage) {
		U_16 badClassNameLength = J9UTF8_LENGTH(badClassName);
		U_8 * badClassNameStr = J9UTF8_DATA(badClassName);

		UDATA errorMsgLen = j9str_printf(PORTLIB, NULL, 0, nlsMessage, badClassNameLength, badClassNameStr);
		errorMsg = (char*)j9mem_allocate_memory(errorMsgLen, OMRMEM_CATEGORY_VM);
		if (NULL == errorMsg) {
			J9MemoryManagerFunctions *gcFuncs = vmThread->javaVM->memoryManagerFunctions;
			j9object_t detailMessage = gcFuncs->j9gc_createJavaLangString(vmThread, badClassNameStr, badClassNameLength, J9_STR_XLAT);
			setCurrentException(vmThread, exceptionIndex, (UDATA *)detailMessage);
			return;
		}
		j9str_printf(PORTLIB, errorMsg, errorMsgLen, nlsMessage, badClassNameLength, badClassNameStr);
	}

	setCurrentExceptionUTF(vmThread, exceptionIndex, errorMsg);
	j9mem_free_memory(errorMsg);
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
loadSuperClassAndInterfaces(J9VMThread *vmThread, J9ClassLoader *classLoader, J9ROMClass *romClass, UDATA options, J9Class *elementClass,
	UDATA packageID, BOOLEAN hotswapping, UDATA classPreloadFlags, J9Class **superclassOut, J9Module *module)
{
	J9JavaVM *vm = vmThread->javaVM;
	BOOLEAN isExemptFromValidation = J9_ARE_ANY_BITS_SET(options, J9_FINDCLASS_FLAG_UNSAFE);
	J9UTF8 *className = J9ROMCLASS_CLASSNAME(romClass);
	J9UTF8 *superclassName = NULL;
	J9Class *superclass = NULL;

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
			if (J9CLASS_IS_EXEMPT_FROM_VALIDATION(superclass)) {
				/* we will inherit exemption from superclass */
				isExemptFromValidation = TRUE;
			}
			if (!isExemptFromValidation
				&& requirePackageAccessCheck(vm, classLoader, module, superclass)
				&& (checkPackageAccess(vmThread, superclass, classPreloadFlags) != 0)
			) {
				return FALSE;
			}

			/* ensure that the superclass isn't an interface or final */
			if (J9_ARE_ANY_BITS_SET(superclass->romClass->modifiers, J9AccFinal)) {
				Trc_VM_CreateRAMClassFromROMClass_superclassIsFinal(vmThread, superclass);
				setCurrentExceptionForBadClass(vmThread, superclassName, J9VMCONSTANTPOOL_JAVALANGVERIFYERROR, J9NLS_VM_CLASS_LOADING_ERROR_EXTEND_FINAL_SUPERCLASS);
				return FALSE;
			}
			if (J9_ARE_ANY_BITS_SET(superclass->romClass->modifiers, J9AccInterface)) {
				Trc_VM_CreateRAMClassFromROMClass_superclassIsInterface(vmThread, superclass);
				setCurrentExceptionForBadClass(vmThread, superclassName, J9VMCONSTANTPOOL_JAVALANGINCOMPATIBLECLASSCHANGEERROR, J9NLS_VM_CLASS_LOADING_ERROR_SUPERCLASS_IS_INTERFACE);
				return FALSE;
			}

			/* ensure that the superclass is visible */
			if (!isExemptFromValidation) {
				/*
				 * Failure occurs if
				 * 1) The superClass class is not public and does not belong to
				 * the same package as the class being loaded.
				 * 2) The superClass class is public but the class being loaded
				 * belongs to a module that doesn't have access to the module that
				 * owns the superClass class.
				 */
				bool superClassIsPublic = J9_ARE_ALL_BITS_SET(superclass->romClass->modifiers, J9AccPublic);
				if ((!superClassIsPublic && (packageID != superclass->packageID))
					|| (superClassIsPublic && (J9_VISIBILITY_ALLOWED != checkModuleAccess(vmThread, vm, romClass, module, superclass->romClass, superclass->module, superclass->packageID, 0)))
				) {
					Trc_VM_CreateRAMClassFromROMClass_superclassNotVisible(vmThread, superclass, superclass->classLoader, classLoader);
					setCurrentExceptionForBadClass(vmThread, superclassName, J9VMCONSTANTPOOL_JAVALANGILLEGALACCESSERROR, J9NLS_VM_CLASS_LOADING_ERROR_INVISIBLE_CLASS_OR_INTERFACE);
					return FALSE;
				}
			}

			if (J9_ARE_ANY_BITS_SET(options, J9_FINDCLASS_FLAG_NAME_IS_INVALID) && !isExemptFromValidation) {
				/*
				 * The caller signalled that the name is invalid and this class is not exempt.
				 * Don't set a pending exception - the caller will do that.
				 */
				return FALSE;
			}

			/* force the interfaces to be loaded without holding the mutex */
			if (romClass->interfaceCount != 0) {
				UDATA i = 0;
				J9SRP *interfaceNames = J9ROMCLASS_INTERFACES(romClass);

				for (i = 0; i<romClass->interfaceCount; i++) {
					J9UTF8 *interfaceName = NNSRP_GET(interfaceNames[i], J9UTF8*);
					J9Class *interfaceClass = internalFindClassUTF8(vmThread, J9UTF8_DATA(interfaceName), J9UTF8_LENGTH(interfaceName), classLoader, classPreloadFlags);

					Trc_VM_CreateRAMClassFromROMClass_loadedInterface(vmThread, J9UTF8_LENGTH(interfaceName), J9UTF8_DATA(interfaceName), interfaceClass);
					if (interfaceClass == NULL) {
						return FALSE;
					}
					if (requirePackageAccessCheck(vm, classLoader, module, interfaceClass)
						&& (checkPackageAccess(vmThread, interfaceClass, classPreloadFlags) != 0)
					) {
						return FALSE;
					}
					/* ensure that the interface is in fact an interface */
					if ((interfaceClass->romClass->modifiers & J9AccInterface) != J9AccInterface) {
						Trc_VM_CreateRAMClassFromROMClass_interfaceIsNotAnInterface(vmThread, interfaceClass);
						setCurrentExceptionForBadClass(vmThread, J9ROMCLASS_CLASSNAME(interfaceClass->romClass), J9VMCONSTANTPOOL_JAVALANGINCOMPATIBLECLASSCHANGEERROR, J9NLS_VM_CLASS_LOADING_ERROR_NON_INTERFACE);
						return FALSE;
					}
					if (!isExemptFromValidation) {
						/*
						 * Failure occurs if
						 * 1) The interface class is not public and does not belong to
						 * the same package as the class being loaded.
						 * 2) The interface class is public but the class being loaded
						 * belongs to a module that doesn't have access to the module that
						 * owns the interface class.
						 */
						bool interfaceIsPublic = J9_ARE_ALL_BITS_SET(interfaceClass->romClass->modifiers, J9AccPublic);
						if ((!interfaceIsPublic && (packageID != interfaceClass->packageID))
							|| (interfaceIsPublic && (J9_VISIBILITY_ALLOWED != checkModuleAccess(vmThread, vm, romClass, module, interfaceClass->romClass, interfaceClass->module, interfaceClass->packageID, 0)))
						) {
							Trc_VM_CreateRAMClassFromROMClass_interfaceNotVisible(vmThread, interfaceClass, interfaceClass->classLoader, classLoader);
							setCurrentExceptionForBadClass(vmThread, J9ROMCLASS_CLASSNAME(interfaceClass->romClass), J9VMCONSTANTPOOL_JAVALANGILLEGALACCESSERROR, J9NLS_VM_CLASS_LOADING_ERROR_INVISIBLE_CLASS_OR_INTERFACE);
							return FALSE;
						}
					}
				}
			}
		}
	}

	return TRUE;
}

#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
/**
 * Attempts to pre-load classes of fields with Q signatures and no static
 * access modifier set. Also records largest field type in largestFieldType.
 * 0 for single 1 for ref and 2 for double. Records total number of
 * flattenable instance fields in flattenableInstanceFieldCount.
 *
 * Caller should not hold the classTableMutex.
 *
 * Return TRUE on success. On failure, returns FALSE and sets the
 * appropriate Java error on the VM.
 */
static BOOLEAN
loadFlattenableFieldValueClasses(J9VMThread *currentThread, J9ClassLoader *classLoader, J9ROMClass *romClass, UDATA classPreloadFlags, UDATA packageID, J9Module *module, UDATA *largestFieldType, J9FlattenedClassCache *flattenedClassCache)
{
	J9ROMFieldWalkState fieldWalkState = {0};
	J9ROMFieldShape *field = romFieldsStartDo(romClass, &fieldWalkState);
	BOOLEAN result = TRUE;
	UDATA flattenableInstanceFieldCount = 0;

	/* iterate over fields and load classes of fields marked as QTypes */
	while (NULL != field) {
		const U_32 modifiers = field->modifiers;
		if (J9_ARE_NO_BITS_SET(modifiers, J9AccStatic)) {
			J9UTF8 *signature = J9ROMFIELDSHAPE_SIGNATURE(field);
			U_8 *signatureChars = J9UTF8_DATA(signature);
			switch (signatureChars[0]) {
			case 'Q':
			{
				J9Class *valueClass = internalFindClassUTF8(currentThread, signatureChars + 1, J9UTF8_LENGTH(signature) - 2, classLoader, classPreloadFlags);
				if (NULL == valueClass) {
					result = FALSE;
					goto done;
				} else {
					J9ROMClass *valueROMClass = valueClass->romClass;

					if (J9_ARE_NO_BITS_SET(valueROMClass->modifiers, J9AccValueType)) {
						J9UTF8 *badClass = NNSRP_GET(valueROMClass->className, J9UTF8*);
						setCurrentExceptionNLSWithArgs(currentThread, J9NLS_VM_ERROR_QTYPE_NOT_VALUE_TYPE, J9VMCONSTANTPOOL_JAVALANGINCOMPATIBLECLASSCHANGEERROR, J9UTF8_LENGTH(badClass), J9UTF8_DATA(badClass));
						result = FALSE;
						goto done;
					}

					bool classIsPublic = J9_ARE_ALL_BITS_SET(valueROMClass->modifiers, J9AccPublic);

					if ((!classIsPublic && (packageID != valueClass->packageID))
						|| (classIsPublic && (J9_VISIBILITY_ALLOWED != checkModuleAccess(currentThread, currentThread->javaVM, romClass, module, valueROMClass, valueClass->module, valueClass->packageID, 0)))
					) {
						Trc_VM_CreateRAMClassFromROMClass_nestedValueClassNotVisible(currentThread, valueClass, valueClass->classLoader, classLoader);
						setCurrentExceptionForBadClass(currentThread, J9ROMCLASS_CLASSNAME(valueROMClass), J9VMCONSTANTPOOL_JAVALANGILLEGALACCESSERROR, J9NLS_VM_CLASS_LOADING_ERROR_INVISIBLE_CLASS_OR_INTERFACE);
						result = FALSE;
						goto done;
					}

					J9FlattenedClassCache *entry = flattenedClassCache + (flattenableInstanceFieldCount + 1);
					entry->clazz = valueClass;
					entry->nameAndSignature = &(field->nameAndSignature);
					entry->offset = 0;
					flattenableInstanceFieldCount += 1;
				}
				*largestFieldType |= (valueClass->classFlags & (J9ClassLargestAlignmentConstraintDouble | J9ClassLargestAlignmentConstraintReference));
				break;
			}
			case 'J':
			case 'D':
				*largestFieldType |= J9ClassLargestAlignmentConstraintDouble;
				break;
			case 'L':
				*largestFieldType |= J9ClassLargestAlignmentConstraintReference;
				break;
			default:
				/* Do nothing for now. If we eventually decide to pack fields smaller than 32 bits we'll
				 * need to modify this code.
				 */
				break;
			}
		}
		field = romFieldsNextDo(&fieldWalkState);
	}
	flattenedClassCache[0].offset = flattenableInstanceFieldCount;

done:
	return result;
}
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */

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
#ifndef J9VM_IVE_RAW_BUILD /* J9VM_IVE_RAW_BUILD is not enabled by default */
			j9object_t protectionDomain = NULL;

			J9VMJAVALANGCLASS_SET_CLASSLOADER(vmThread, state->classObject, classLoader->classLoaderObject);
			protectionDomain = (j9object_t)PEEK_OBJECT_IN_SPECIAL_FRAME(vmThread, 0);
			J9VMJAVALANGCLASS_SET_PROTECTIONDOMAIN(vmThread, state->classObject, protectionDomain);
#endif /* !J9VM_IVE_RAW_BUILD */

			/* Link J9Class and object after possible access barrier failures. */
			J9VMJAVALANGCLASS_SET_VMREF(vmThread, state->classObject, state->ramClass);
			/* store the classObject using an access barrier */
			J9STATIC_OBJECT_STORE(vmThread, state->ramClass, (j9object_t*)&state->ramClass->classObject, (j9object_t)state->classObject);

			if (J9_ARE_ALL_BITS_SET(javaVM->runtimeFlags, J9_RUNTIME_JAVA_BASE_MODULE_CREATED)) {
				J9Module *module = state->ramClass->module;
				j9object_t moduleObject = NULL;
				if (J9_IS_J9MODULE_UNNAMED(javaVM, module)) {
					moduleObject = J9VMJAVALANGCLASSLOADER_UNNAMEDMODULE(vmThread, classLoader->classLoaderObject);
				} else {
					moduleObject = module->moduleObject;
				}
				Assert_VM_notNull(moduleObject);
				J9VMJAVALANGCLASS_SET_MODULE(vmThread, state->ramClass->classObject, moduleObject);
			}
		}

		/* Ensure all previous writes have completed before making the new class visible. */
		VM_AtomicSupport::writeBarrier();

		/* Put the new class in the table or arrayClass field. */
		if ((!fastHCR)
			&& (0 == J9ROMCLASS_IS_PRIMITIVE_OR_ARRAY(romClass))
			&& J9_ARE_NO_BITS_SET(options, J9_FINDCLASS_FLAG_ANON)
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
			vmThread, ramClass->module->moduleName, J9_STR_NULL_TERMINATE_RESULT, "", 0, moduleNameBuf, J9VM_PACKAGE_NAME_BUFFER_LENGTH, NULL);
	}
	j9object_t classLoaderName = NULL;
	if (NULL != classLoader->classLoaderObject) {
		classLoaderName = J9VMJAVALANGCLASSLOADER_CLASSLOADERNAME(vmThread, classLoader->classLoaderObject);
	}
	if (NULL != classLoaderName) {
		classLoaderNameUTF = vmFuncs->copyStringToUTF8WithMemAlloc(
			vmThread, classLoaderName, J9_STR_NULL_TERMINATE_RESULT, "", 0, classLoaderNameBuf, J9VM_PACKAGE_NAME_BUFFER_LENGTH, NULL);
	} else {
#define UNNAMED_NAMED_MODULE   "system classloader"
		memcpy(classLoaderNameBuf, UNNAMED_NAMED_MODULE, sizeof(UNNAMED_NAMED_MODULE));
#undef	UNNAMED_NAMED_MODULE
		classLoaderNameUTF = classLoaderNameBuf;
	}
	if ((NULL != classLoaderNameUTF) && (NULL != moduleNameUTF)) {
		Trc_MODULE_setPackage(vmThread, J9UTF8_LENGTH(className), J9UTF8_DATA(className), classLoaderNameUTF, classLoader, moduleNameUTF, ramClass->module);
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
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
static J9Class*
internalCreateRAMClassFromROMClassImpl(J9VMThread *vmThread, J9ClassLoader *classLoader, J9ROMClass *romClass,
	UDATA options, J9Class *elementClass, J9ROMMethod **methodRemapArray, IDATA entryIndex, I_32 locationType, J9Class *classBeingRedefined,
	UDATA packageID, J9Class *superclass, J9CreateRAMClassState *state, J9ClassLoader* hostClassLoader, J9Class *hostClass, J9Module *module, J9FlattenedClassCache *flattenedClassCache)
#else /* J9VM_OPT_VALHALLA_VALUE_TYPES */
static J9Class*
internalCreateRAMClassFromROMClassImpl(J9VMThread *vmThread, J9ClassLoader *classLoader, J9ROMClass *romClass,
	UDATA options, J9Class *elementClass, J9ROMMethod **methodRemapArray, IDATA entryIndex, I_32 locationType, J9Class *classBeingRedefined,
	UDATA packageID, J9Class *superclass, J9CreateRAMClassState *state, J9ClassLoader* hostClassLoader, J9Class *hostClass, J9Module *module)
#endif /* J9VM_OPT_VALHALLA_VALUE_TYPES */
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

#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
		romWalkResult = fieldOffsetsStartDo(javaVM, romClass, superclass, &romWalkState,
			(J9VM_FIELD_OFFSET_WALK_CALCULATE_INSTANCE_SIZE | J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE |
			 J9VM_FIELD_OFFSET_WALK_ONLY_OBJECT_SLOTS), flattenedClassCache);
#else /* J9VM_OPT_VALHALLA_VALUE_TYPES */
		romWalkResult = fieldOffsetsStartDo(javaVM, romClass, superclass, &romWalkState,
			(J9VM_FIELD_OFFSET_WALK_CALCULATE_INSTANCE_SIZE | J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE |
			 J9VM_FIELD_OFFSET_WALK_ONLY_OBJECT_SLOTS));
#endif /* J9VM_OPT_VALHALLA_VALUE_TYPES */

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
			vTable = computeVTable(vmThread, hostClassLoader, superclass, romClass, packageID, methodRemapArray, interfaceHead, &defaultConflictCount, interfaceCount, inheritedInterfaceCount, &errorData);
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
			vTableSlots = ((J9VTableHeader *)vTable)->size;
			/* account for header slots */
			vTableSlots += (sizeof(J9VTableHeader) / sizeof(UDATA));
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
			if ((romClass->modifiers & J9AccInterface) == J9AccInterface) {
				/* The iTables for interface classes do not contain entries for methods. */
				iTableSlotCount += sizeof(J9ITable) / sizeof(UDATA);
			} else {
				J9Class *interfaceWalk = interfaceHead;
				while (interfaceWalk != NULL) {
					/* The iTables for interface classes do not contain entries for methods. */
					/* add methods supported by this interface to tally */
					J9ITable *allInterfaces = (J9ITable*)interfaceWalk->iTable;
					do {
						iTableSlotCount += allInterfaces->interfaceClass->romClass->romMethodCount;	
						allInterfaces = allInterfaces->next;
					} while (NULL != allInterfaces);
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

	if (!hotswapping) {
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

			/* flattened classes cache */
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
			allocationRequests[RAM_CLASS_FLATTENED_CLASS_CACHE].prefixSize = 0;
			allocationRequests[RAM_CLASS_FLATTENED_CLASS_CACHE].alignment = OMR_MAX(sizeof(J9Class *), sizeof(UDATA));
			allocationRequests[RAM_CLASS_FLATTENED_CLASS_CACHE].alignedSize = sizeof(J9FlattenedClassCache) * (flattenedClassCache[0].offset > 0 ? flattenedClassCache[0].offset + 1 : 0);
			allocationRequests[RAM_CLASS_FLATTENED_CLASS_CACHE].address = NULL;
#endif /* J9VM_OPT_VALHALLA_VALUE_TYPES */

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
				for (U_16 i = 0; i < romClass->staticSplitMethodRefCount; ++i) {
					ramClass->staticSplitMethodTable[i] = (J9Method*)javaVM->initialMethods.initialStaticMethod;
				}
				ramClass->specialSplitMethodTable = (J9Method **) allocationRequests[RAM_SPECIAL_SPLIT_TABLE_FRAGMENT].address;
				for (U_16 i = 0; i < romClass->specialSplitMethodRefCount; ++i) {
					ramClass->specialSplitMethodTable[i] = (J9Method*)javaVM->initialMethods.initialSpecialMethod;
				}
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
				ramClass->flattenedClassCache = (J9FlattenedClassCache *) allocationRequests[RAM_CLASS_FLATTENED_CLASS_CACHE].address;
				if (flattenedClassCache[0].offset > 0) {
					memcpy(ramClass->flattenedClassCache, flattenedClassCache, sizeof(J9FlattenedClassCache) * (flattenedClassCache[0].offset + 1));
				}
#endif /* J9VM_OPT_VALHALLA_VALUE_TYPES */
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
			UDATA iTableMethodCount = 0;
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

					if (J9ROMMETHOD_IN_ITABLE(romMethod)) {
						iTableMethodCount += 1;
					}
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
			 *         + AccClassIsContended (from romClass->extraModifiers and inherited)
			 *        + AccClassHasFinalFields (from romClass->extraModifiers and inherited)
			 *       + AccClassHotSwappedOut (not set during creation, not inherited)
			 *      + AccClassDying (not set during creation, inherited but that can't actually occur)
			 *
			 *    + reference (from romClass->extraModifiers, not inherited)
			 *   + reference (from romClass->extraModifiers, not inherited)
			 *  + AccClassFinalizeNeeded (from romClass->extraModifiers and inherited, cleared for empty finalize)
			 * + AccClassCloneable (from romClass->extraModifiers and inherited)

			 * classFlags - what does each bit represent?
			 *
			 * 0000 0000 0000 0000 0000 0000 0000 0000
			 *                                       + J9ClassDoNotAttemptToSetInitCache
			 *                                      + J9ClassHasIllegalFinalFieldModifications
			 *                                     + J9ClassReusedStatics
			 *                                    + J9ClassContainsJittedMethods
			 *
			 *                                  + J9ClassContainsMethodsPresentInMCCHash
			 *                                 + J9ClassGCScanned
			 *                                + J9ClassIsAnonymous
			 *                               + J9ClassIsFlattened
			 *
			 *                             + J9ClassHasWatchedFields (inherited)
			 *                            + J9ClassReservableLockWordInit
			 *                           + J9ClassIsValueType
			 *                          + J9ClassLargestAlignmentConstraintReference
			 *
			 *                        + J9ClassLargestAlignmentConstraintDouble
			 *                       + J9ClassIsExemptFromValidation (inherited)
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
				tempClassDepthAndFlags &= ~J9AccClassFinalizeNeeded;
			}
#endif
			
			if ((javaVM->jclFlags & J9_JCL_FLAG_REFERENCE_OBJECTS) != J9_JCL_FLAG_REFERENCE_OBJECTS) {
				tempClassDepthAndFlags &= ~J9AccClassReferenceMask;
			}

			tempClassDepthAndFlags &= J9AccClassRomToRamMask;
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
				tempClassDepthAndFlags &= ~(J9AccClassHotSwappedOut | J9AccClassHasBeenOverridden | J9AccClassHasJDBCNatives | J9AccClassRAMArray | (OBJECT_HEADER_SHAPE_MASK << J9AccClassRAMShapeShift));
				tempClassDepthAndFlags++;
				
#if defined(J9VM_GC_FINALIZATION)
				/* if this class has an empty finalize() method, make sure not to inherit javaFlagsClassFinalizeNeeded from the superclass */
				if (J9ROMCLASS_HAS_EMPTY_FINALIZE(romClass)) {
					tempClassDepthAndFlags &= ~J9AccClassFinalizeNeeded;
				}
#endif
				
				/* fill in superclass array */
				if (superclassCount != 0) {
					memcpy(ramClass->superclasses, superclass->superclasses, superclassCount * sizeof(UDATA));
				}
				ramClass->superclasses[superclassCount] = superclass;
			}
			tempClassDepthAndFlags |= ((romClass->instanceShape & OBJECT_HEADER_SHAPE_MASK) << J9AccClassRAMShapeShift);
			if (J9ROMCLASS_IS_ARRAY(romClass)) {
				tempClassDepthAndFlags |= J9AccClassRAMArray;
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
				ramClass->classDepthAndFlags |= J9AccClassCloneable;
			}

			/* Prevent certain classes from having Cloneable subclasses */

			if (J9CLASS_FLAGS(ramClass) & J9AccClassCloneable) {
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
							ramClass->classDepthAndFlags &= ~J9AccClassCloneable;
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

			/* If the class is an interface, fill in the iTableMethodCount and method ordering table */
			if (J9ROMCLASS_IS_INTERFACE(romClass)) {
				J9INTERFACECLASS_SET_ITABLEMETHODCOUNT(ramClass, iTableMethodCount);
				J9INTERFACECLASS_SET_METHODORDERING(ramClass, NULL);
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
				if ((NULL == classBeingRedefined) && (LOAD_LOCATION_UNKNOWN != locationType) && J9_ARE_NO_BITS_SET(options, J9_FINDCLASS_FLAG_UNSAFE)) {
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
			if ((J2SE_VERSION(javaVM) >= J2SE_V11) && (NULL == classBeingRedefined)) {
				ramClass->module = module;
				if (TrcEnabled_Trc_MODULE_setPackage) {
					trcModulesSettingPackage(vmThread, ramClass, classLoader, className);
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
				if (J9_IS_J9CLASS_VALUETYPE(elementClass) && J9_IS_J9CLASS_FLATTENED(elementClass)) {
					if (J9_ARE_ALL_BITS_SET(elementClass->classFlags, J9ClassLargestAlignmentConstraintDouble)) {
						J9ARRAYCLASS_SET_STRIDE(ramClass, ROUND_UP_TO_POWEROF2(J9_VALUETYPE_FLATTENED_SIZE(elementClass), sizeof(U_64)));
					} else if (J9_ARE_ALL_BITS_SET(elementClass->classFlags, J9ClassLargestAlignmentConstraintReference)) {
						J9ARRAYCLASS_SET_STRIDE(ramClass, ROUND_UP_TO_POWEROF2(J9_VALUETYPE_FLATTENED_SIZE(elementClass), sizeof(fj9object_t)));
					} else { /* VT only contains singles (int, short, etc) at this point */
						J9ARRAYCLASS_SET_STRIDE(ramClass, J9_VALUETYPE_FLATTENED_SIZE(elementClass));
					}
				} else {
					J9ARRAYCLASS_SET_STRIDE(ramClass, (((UDATA) 1) << (((J9ROMArrayClass*)romClass)->arrayShape & 0x0000FFFF)));
				}
			} else if (J9ROMCLASS_IS_PRIMITIVE_TYPE(ramClass->romClass)) {
				ramClass->module = module;
			} else {
				Assert_VM_unreachable();
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
	J9Class *superclass = NULL;
	J9UTF8 *className = NULL;
	UDATA packageID = 0;
	UDATA classPreloadFlags = 0;
	BOOLEAN hotswapping = (0 != (options & J9_FINDCLASS_FLAG_NO_DEBUG_EVENTS));
	BOOLEAN fastHCR = (0 != (options & J9_FINDCLASS_FLAG_FAST_HCR));
	J9CreateRAMClassState state = {0};
	J9Class *result = NULL;
	J9ClassLoader* hostClassLoader = classLoader;
	J9Module* module = NULL;
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
	UDATA romFieldCount = romClass->romFieldCount;
	UDATA flattenedClassCacheAllocSize = sizeof(J9FlattenedClassCache) * (romFieldCount + 1);
	UDATA largestFieldType = 0;
	/* The +1 is for the size field */
	J9FlattenedClassCache flattenedClassCacheBuffer[DEFAULT_SIZE_OF_CLASS_CACHE + 1] = {0};
	J9FlattenedClassCache *flattenedClassCache = flattenedClassCacheBuffer;
	PORT_ACCESS_FROM_VMC(vmThread);
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */

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

	if (J2SE_VERSION(javaVM) >= J2SE_V11) {
		if (NULL == classBeingRedefined) {
			if (J9ROMCLASS_IS_ARRAY(romClass)) {
				/* At this point the elementClass has been loaded. No
				 * need to check if java.base was created and no need to
				 * do a lookup. Just assign the arrayClass the same module
				 * which was used for the elementClass.
				 */
				module = elementClass->module;
			} else if (J9_ARE_ALL_BITS_SET(javaVM->runtimeFlags, J9_RUNTIME_JAVA_BASE_MODULE_CREATED)) {
				/* VM does not create J9Module for unnamed modules except the unnamed module for bootloader.
				 * Therefore J9Class.module should be NULL for classes loaded from classpath by non-bootloaders.
				 * The call to findModuleForPackage() should correctly set J9Class.module as it would return
				 * NULL for classes loaded from classpath.
				 * For bootloader, unnamed module is represented by J9JavaVM.unamedModuleForSystemLoader.
				 * Therefore for classes loaded by bootloader from boot classpath,
				 * J9Class.module should be set to J9JavaVM.unamedModuleForSystemLoader.
				 */
				if (J9_ARE_ALL_BITS_SET(options, J9_FINDCLASS_FLAG_ANON)) {
					module = hostClass->module;
				} else {
					bool findModule = false;

					if (classLoader != javaVM->systemClassLoader) {
						findModule = true;
					} else if ((LOAD_LOCATION_PATCH_PATH == locationType)
					|| (LOAD_LOCATION_MODULE == locationType)
					) {
						findModule = true;
					} else {
						J9UTF8 *superclassName = J9ROMCLASS_SUPERCLASSNAME(romClass);

						if ((NULL != superclassName)
						&& J9UTF8_LITERAL_EQUALS(J9UTF8_DATA(superclassName), J9UTF8_LENGTH(superclassName), "java/lang/reflect/Proxy")
						) {
							/*
							 * Proxy classes loaded by the system classloader may belong to a dynamically
							 * created module (which has been granted access to the interfaces it must implement).
							 */
							findModule = true;
						}
					}

					if (findModule) {
						U_32 pkgNameLength = (U_32) packageNameLength(romClass);

						omrthread_monitor_enter(javaVM->classLoaderModuleAndLocationMutex);
						module = findModuleForPackage(vmThread, classLoader, J9UTF8_DATA(className), pkgNameLength);
						omrthread_monitor_exit(javaVM->classLoaderModuleAndLocationMutex);
					} else {
						module = javaVM->unamedModuleForSystemLoader;
					}
				}
			} else {
				/* Ignore locationType and assign all classes created before the java.base module is created to java.base.
				 * This matches the reference implementation. Validated on JDK9 through JDK11.
				 */
				module = javaVM->javaBaseModule;
			}
		}
	}
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
	if (romFieldCount > DEFAULT_SIZE_OF_CLASS_CACHE) {
		flattenedClassCache = (J9FlattenedClassCache *) j9mem_allocate_memory(flattenedClassCacheAllocSize, J9MEM_CATEGORY_CLASSES);
		if (NULL == flattenedClassCache) {
			setNativeOutOfMemoryError(vmThread, 0, 0);
			omrthread_monitor_enter(javaVM->classTableMutex);
			return internalCreateRAMClassDone(vmThread, classLoader, romClass, options, elementClass, className, &state);
		}
		memset(flattenedClassCache, 0, flattenedClassCacheAllocSize);
	}

#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
	if (!loadSuperClassAndInterfaces(vmThread, hostClassLoader, romClass, options, elementClass, packageID, hotswapping, classPreloadFlags, &superclass, module)
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
		|| !loadFlattenableFieldValueClasses(vmThread, hostClassLoader, romClass, classPreloadFlags, packageID, module, &largestFieldType, flattenedClassCache)
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
	) {
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
		if (flattenedClassCache != flattenedClassCacheBuffer) {
			j9mem_free_memory(flattenedClassCache);
		}
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
		omrthread_monitor_enter(javaVM->classTableMutex);
		return internalCreateRAMClassDone(vmThread, classLoader, romClass, options, elementClass, className, &state);
	}

#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
	result = internalCreateRAMClassFromROMClassImpl(vmThread, classLoader, romClass, options, elementClass,
		methodRemapArray, entryIndex, locationType, classBeingRedefined, packageID, superclass, &state, hostClassLoader, hostClass, module, flattenedClassCache);

		if (flattenedClassCache != flattenedClassCacheBuffer) {
			j9mem_free_memory(flattenedClassCache);
		}
#else
	result = internalCreateRAMClassFromROMClassImpl(vmThread, classLoader, romClass, options, elementClass,
		methodRemapArray, entryIndex, locationType, classBeingRedefined, packageID, superclass, &state, hostClassLoader, hostClass, module);
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
	if (state.retry) {
		goto retry;
	}

	if (NULL != result) {
		U_32 classFlags = result->classFlags;
		if (NULL != superclass) {
			/* watched fields tag and exemption from validation are inherited from the superclass */
			const U_32 inheritedFlags = J9ClassHasWatchedFields | J9ClassIsExemptFromValidation;
			classFlags |= (superclass->classFlags & inheritedFlags);
		}
		if (0 != (J9_FINDCLASS_FLAG_ANON & options)) {
			/* if anonClass replace classLoader with hostClassLoader, no one can know about anonClassLoader */
			result->classLoader = hostClassLoader;
			if (NULL != result->classObject) {
				/* no object is created when doing hotswapping */
				J9VMJAVALANGCLASS_SET_CLASSLOADER(vmThread, result->classObject, hostClassLoader->classLoaderObject);
			}
			classFlags |= J9ClassIsAnonymous;
		}
		if (J9_ARE_NO_BITS_SET(classFlags, J9ClassIsExemptFromValidation)) {
			J9UTF8 *className = J9ROMCLASS_CLASSNAME(romClass);

			if (J9UTF8_LITERAL_EQUALS(J9UTF8_DATA(className), J9UTF8_LENGTH(className), MAGIC_ACCESSOR_IMPL)) {
				classFlags |= J9ClassIsExemptFromValidation;
			}
		}

#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
		if (J9_ARE_ALL_BITS_SET(romClass->modifiers, J9AccValueType)) {
			classFlags |= J9ClassIsValueType;
			if ((result->totalInstanceSize <= javaVM->valueFlatteningThreshold) && !J9ROMCLASS_IS_CONTENDED(romClass)) {
				Trc_VM_CreateRAMClassFromROMClass_valueTypeIsFlattened(vmThread, J9UTF8_LENGTH(className), J9UTF8_DATA(className), result);
				classFlags |= J9ClassIsFlattened;
			}

			if (J9_ARE_ALL_BITS_SET(largestFieldType, J9ClassLargestAlignmentConstraintDouble)) {
				classFlags |= J9ClassLargestAlignmentConstraintDouble;
			} else if (J9_ARE_ALL_BITS_SET(largestFieldType, J9ClassLargestAlignmentConstraintReference)) {
				classFlags |= J9ClassLargestAlignmentConstraintReference;
			}
		}
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */

		result->classFlags = classFlags;
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
