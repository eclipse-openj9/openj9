/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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

#include "j9.h"
#include "j9protos.h"
#include "j9consts.h"
#include "ut_j9vm.h"
#include <string.h>
#include "vm_internal.h"
#include "locknursery.h"
#include "j2sever.h"
#include "j9modifiers_api.h"
#include "ObjectFieldInfo.hpp"
#include "util_api.h"
#include "vm_api.h"

/* Extra hidden fields are lockword and finalizeLink. */
#define NUMBER_OF_EXTRA_HIDDEN_FIELDS 2

#if !defined (J9VM_SIZE_SMALL_CODE)

typedef struct J9FieldTableEntry {
	J9ROMFieldShape* field;
	UDATA offset;
} J9FieldTableEntry;

typedef struct J9FieldTable {
	J9FieldTableEntry* fieldList;
	UDATA length; /* number of entries in the list */
} J9FieldTable;

/* Descriptor for the field table (alias the field index) */
typedef struct fieldIndexTableEntry {
	J9Class *ramClass; /* key to the table */
	J9FieldTable *table; /* the base of the index for the ram class */
} fieldIndexTableEntry;
extern "C" {
static fieldIndexTableEntry* fieldIndexTableAdd(J9JavaVM* vm, J9Class *ramClass, J9FieldTable *table);
static J9FieldTable* fieldIndexTableGet(J9JavaVM* vm,  J9Class *ramClass);
static J9ROMFieldShape* findFieldInTable(J9VMThread *vmThread, J9Class *clazz, U_8 *fieldName, UDATA fieldNameLength, U_8 *signature, UDATA signatureLength, UDATA *offsetOrAddress);
#endif

#define SUPERCLASS(clazz) (((clazz)->superclasses[ J9CLASS_DEPTH(clazz) - 1 ]))

static J9ROMFieldShape * findFieldInClass (J9VMThread *vmStruct, J9Class *clazz, U_8 *fieldName, UDATA fieldNameLength, U_8 *signature, UDATA signatureLength, UDATA *offsetOrAddress, J9Class **definingClass);
static J9ROMFieldShape* findFieldAndCheckVisibility (J9VMThread *vmStruct, J9Class *clazz, U_8 *fieldName, UDATA fieldNameLength, U_8 *signature, UDATA signatureLength, J9Class **definingClass, UDATA *offsetOrAddress, UDATA options, J9Class *sourceClass);
static J9ROMFieldShape* findField (J9VMThread *vmStruct, J9Class *clazz, U_8 *fieldName, UDATA fieldNameLength, U_8 *signature, UDATA signatureLength, J9Class **definingClass, UDATA *offsetOrAddress, UDATA options);
VMINLINE static UDATA calculateJ9UTFSize(UDATA stringLength);
VMINLINE static UDATA calculateFakeJ9ROMFieldShapeSize(UDATA nameLength, UDATA signatureLength);
static void initFakeJ9ROMFieldShape(J9ROMFieldShape *shape, U_16 nameLength, U_8 *nameData, U_16 signatureLength, U_8 *signatureData);
static J9ROMFieldShape *allocAndInitFakeJ9ROMFieldShape(J9JavaVM *vm, const char *name, const char *signature);
VMINLINE static J9HiddenInstanceField *initJ9HiddenField(J9HiddenInstanceField *field, J9UTF8 *classNameUTF8, J9ROMFieldShape *shape, UDATA *offsetReturn, J9HiddenInstanceField *next);

static void fieldOffsetsFindNext(J9ROMFieldOffsetWalkState *state, J9ROMFieldShape *field);

/* Methods for managing hot fields when scavenger DynamicBreadthFirstScanOrdering is enabled */
VMINLINE bool createClassLoaderHotFieldPool(J9JavaVM *javaVM, J9ClassLoader* classLoader);
VMINLINE void createClassHotFieldsInfo(J9JavaVM *javaVM, J9Class* clazz, uint8_t fieldOffset, int32_t reducedCpuUtil, uint32_t reducedFrequency);
VMINLINE void addOrUpdateHotField(J9JavaVM *javaVM, J9Class* clazz, uint8_t fieldOffset, int32_t reducedCpuUtil, uint32_t reducedFrequency);

J9ROMFieldShape*
findFieldExt(J9VMThread *vmStruct, J9Class *clazz, U_8 *fieldName, UDATA fieldNameLength, U_8 *signature, UDATA signatureLength, J9Class **definingClass, UDATA *offsetOrAddress, UDATA options)
{
	return findField(vmStruct, clazz, fieldName, fieldNameLength, signature, signatureLength, definingClass, offsetOrAddress, options);
}

/*
		Returns NULL if an exception is thrown.
		definingClass is set to the class containing the field, if it is not NULL.
		offsetOrAddress is set to the instance offset (for instance field found) or static address (for static field found) if it is not NULL."
*/
static J9ROMFieldShape*
findField(J9VMThread *vmStruct, J9Class *clazz, U_8 *fieldName, UDATA fieldNameLength, U_8 *signature, UDATA signatureLength, J9Class **definingClass, UDATA *offsetOrAddress, UDATA options)
{
#ifdef J9VM_INTERP_TRACING
	PORT_ACCESS_FROM_VMC(vmStruct);
#endif
	J9Class* currentClass = clazz;
	J9ROMFieldShape* field;

#ifdef J9VM_INTERP_TRACING
	j9tty_printf(PORTLIB, "findField - %.*s %.*s\n", fieldNameLength, fieldName, signatureLength, signature);
#endif

	do {        
		J9ROMClass* romClass = NULL;
		J9SRP *interfaces = NULL;
		UDATA i = 0;

		field = findFieldInClass(vmStruct, 
			currentClass, 
			fieldName, fieldNameLength, 
			signature, signatureLength, 
			offsetOrAddress, definingClass);
		if (field != NULL) {
			return field;
		}

		/* walk the direct super interfaces, and all of their inherited interfaces */
		romClass = currentClass->romClass;
		interfaces = J9ROMCLASS_INTERFACES(romClass);
		for (i = 0; i < romClass->interfaceCount; i++, interfaces++) {
			/* By definition, the interfaces MUST already have been loaded as
			 * all superclasses & interfaces are loaded before the current class
			 */
			J9UTF8 * interfaceName = NNSRP_PTR_GET(interfaces, J9UTF8 *);
			J9Class* interfaceClass = peekClassHashTable(vmStruct, currentClass->classLoader, J9UTF8_DATA(interfaceName), J9UTF8_LENGTH(interfaceName));
			J9ITable* iTable = NULL;
			Assert_VM_notNull(interfaceClass);
			for(;;) {
				field = findFieldInClass(vmStruct,
					interfaceClass,
					fieldName, fieldNameLength,
					signature, signatureLength,
					offsetOrAddress, definingClass);
				if (field != NULL) {
					return field;
				}
				iTable = iTable ? iTable->next : (J9ITable *)interfaceClass->iTable;
				if (iTable == NULL) break;
				interfaceClass = iTable->interfaceClass;
			}
		}

		/* fetch the superclass */
		currentClass = SUPERCLASS(currentClass);
	} while (currentClass != NULL);

	if ( (options & (J9_LOOK_NO_THROW | J9_LOOK_NO_JAVA)) == 0) {
		J9UTF8* className = J9ROMCLASS_CLASSNAME(clazz->romClass);
		j9object_t message = catUtfToString4( vmStruct, 
			J9UTF8_DATA(className), J9UTF8_LENGTH(className),
			(const U_8 *) ".", 1,
			fieldName, fieldNameLength, 
			NULL, 0);
		if (NULL != message) {
			setCurrentException(vmStruct, J9VMCONSTANTPOOL_JAVALANGNOSUCHFIELDERROR, (UDATA*)message);
		}
	}

	return NULL;
}



static J9ROMFieldShape * 
findFieldInClass(J9VMThread *vmStruct, J9Class *clazz, U_8 *fieldName, UDATA fieldNameLength, U_8 *signature, UDATA signatureLength, UDATA *offsetOrAddress, J9Class **definingClass) 
{
	J9ROMClass * romClass = clazz->romClass;
	J9ROMFieldOffsetWalkState state;
	J9ROMFieldOffsetWalkResult *result;
	J9ROMFieldShape *shape = NULL;
	U_8 found = 0;
	J9JavaVM *javaVM = J9VMTHREAD_JAVAVM(vmStruct);
	
	if (definingClass) {
		*definingClass = clazz;
	}

	Trc_VM_findFieldInClass_Entry(vmStruct, clazz, fieldNameLength, fieldName, signatureLength, signature);

#if !defined (J9VM_SIZE_SMALL_CODE)
	if (romClass->romFieldCount > javaVM->fieldIndexThreshold) {
		J9ROMFieldShape *field;
		field = findFieldInTable(vmStruct, clazz, fieldName, fieldNameLength,
				signature, signatureLength, offsetOrAddress);
		if (field != NULL) {
			shape = field;
		} else {
			Trc_VM_findFieldInClass_NoIndex(vmStruct);
			/* couldn't build an index - fall back to linear search */
		}
	}
#endif

	if (NULL == shape){
		U_32 walkFlags = J9VM_FIELD_OFFSET_WALK_INCLUDE_STATIC | J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE;

#ifdef J9VM_IVE_RAW_BUILD /* J9VM_IVE_RAW_BUILD is not enabled by default */
		/* temporary workaround to allow access to vm-inserted hidden fields, needed for constant pool resolution */
		walkFlags |= J9VM_FIELD_OFFSET_WALK_INCLUDE_HIDDEN;
#endif /* J9VM_IVE_RAW_BUILD */

		/* walk all instance and static fields */
#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
		result = fieldOffsetsStartDo(javaVM, romClass, SUPERCLASS( clazz ), &state, walkFlags, clazz->flattenedClassCache);
#else /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */
		result = fieldOffsetsStartDo(javaVM, romClass, SUPERCLASS( clazz ), &state, walkFlags);
#endif /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */

		while (!found && (result->field != NULL)) {
			/* compare name and sig */
			J9UTF8* thisName = J9ROMFIELDSHAPE_NAME(result->field);
			J9UTF8* thisSig = J9ROMFIELDSHAPE_SIGNATURE(result->field);
			if ((fieldNameLength == J9UTF8_LENGTH(thisName)) 
			&& (signatureLength == J9UTF8_LENGTH(thisSig))
			&& (0 == memcmp(fieldName, J9UTF8_DATA(thisName), fieldNameLength))
			&& (0 == memcmp(signature, J9UTF8_DATA(thisSig), signatureLength))
			) {
				if (offsetOrAddress != NULL) {
					/* if we are returning a static field, convert the offset to the actual address of the static */
					if (result->field->modifiers & J9AccStatic) {
						result->offset += (UDATA) clazz->ramStatics;
					}
					*offsetOrAddress = result->offset;
				}
				shape = result->field;
				found = 1;
			}
			if (!found) {
				result = fieldOffsetsNextDo(&state);
			}
		}
	}

	Trc_VM_findFieldInClass_Exit(vmStruct, clazz, fieldNameLength, fieldName, signatureLength, signature, shape);


	return shape;
}



IDATA 
instanceFieldOffset(J9VMThread *vmStruct, J9Class *clazz, U_8 *fieldName, UDATA fieldNameLength, U_8 *signature, UDATA signatureLength, J9Class **definingClass, UDATA *instanceField, UDATA options)
{
	return instanceFieldOffsetWithSourceClass(vmStruct, clazz, fieldName, fieldNameLength, signature, signatureLength, definingClass, instanceField, options, NULL);
}



IDATA 
instanceFieldOffsetWithSourceClass(J9VMThread *vmStruct, J9Class *clazz, U_8 *fieldName, UDATA fieldNameLength, U_8 *signature, UDATA signatureLength, J9Class **definingClass, UDATA *instanceField, UDATA options, J9Class *sourceClass)
{
	UDATA fieldOffset = -1;
	UDATA offsetOrAddress;
	J9ROMFieldShape* field;

	/* Find a field matching the name and signature */

	field = findFieldAndCheckVisibility (vmStruct, 
											clazz, 
											fieldName, 
											fieldNameLength, 
											signature, 
											signatureLength, 
											definingClass, 
											&offsetOrAddress, 
											options,
											sourceClass);

	if (field) {
		if (field->modifiers & J9AccStatic) {
			if ((options & (J9_LOOK_NO_THROW | J9_LOOK_NO_JAVA)) == 0) {
				setCurrentException (vmStruct, J9VMCONSTANTPOOL_JAVALANGINCOMPATIBLECLASSCHANGEERROR, NULL);
			}
		} else {
			fieldOffset = offsetOrAddress;
			if (instanceField) {
				*instanceField = (UDATA) field;
			}
		}
	}

	return fieldOffset;
}



/*
		Returns NULL if an exception is thrown.
		definingClass is set to the class containing the field, if it is not NULL.
		offsetOrAddress is set to the instance offset (for instance field found) or static address (for static field found) if it is not NULL."
*/
static J9ROMFieldShape*
findFieldAndCheckVisibility (J9VMThread *vmStruct, J9Class *clazz, U_8 *fieldName, UDATA fieldNameLength, U_8 *signature, UDATA signatureLength, J9Class **definingClass, UDATA *offsetOrAddress, UDATA options, J9Class *sourceClass) 
{
	J9Class* defClass;
	J9ROMFieldShape* field;

	field = findField (vmStruct, 
						clazz, 
						fieldName, 
						fieldNameLength, 
						signature, 
						signatureLength, 
						&defClass, 
						offsetOrAddress, 
						options);

	if (definingClass) {
		*definingClass = defClass;
	}

	if (sourceClass && field) {
		IDATA checkResult = checkVisibility(vmStruct, sourceClass, defClass, field->modifiers, options);
		if (checkResult < J9_VISIBILITY_ALLOWED) {
			if ((options & (J9_LOOK_NO_THROW | J9_LOOK_NO_JAVA)) == 0) {
				char *errorMsg = NULL;
				if (J9_VISIBILITY_NON_MODULE_ACCESS_ERROR == checkResult) {
					errorMsg = illegalAccessMessage(vmStruct, field->modifiers, sourceClass, defClass, J9_VISIBILITY_NON_MODULE_ACCESS_ERROR);
				} else {
					errorMsg = illegalAccessMessage(vmStruct, -1, sourceClass, defClass, checkResult);
				}
				PORT_ACCESS_FROM_VMC(vmStruct);
				setCurrentExceptionUTF(vmStruct, J9VMCONSTANTPOOL_JAVALANGILLEGALACCESSERROR, errorMsg);
				j9mem_free_memory(errorMsg);
			}
			field = NULL;
		}
	}

	return field;
}


void *  
staticFieldAddress(J9VMThread *vmStruct, J9Class *clazz, U_8 *fieldName, UDATA fieldNameLength, U_8 *signature, UDATA signatureLength, J9Class **definingClass, UDATA *staticField, UDATA options, J9Class *sourceClass)
{
	UDATA *staticAddress = NULL;
	UDATA offsetOrAddress;
	J9Class *defClass;
	J9ROMFieldShape* field;

	/* Find a field matching the name and signature */
	field = findFieldAndCheckVisibility (vmStruct,
										clazz,
										fieldName,
										fieldNameLength,
										signature,
										signatureLength,
										&defClass,
										&offsetOrAddress,
										options,
										sourceClass);

	if (field) {
		staticAddress = (UDATA *) offsetOrAddress;
		if (0 == (field->modifiers & J9AccStatic)) {
			if ((options & (J9_LOOK_NO_THROW | J9_LOOK_NO_JAVA)) == 0) {
				setCurrentException (vmStruct, J9VMCONSTANTPOOL_JAVALANGINCOMPATIBLECLASSCHANGEERROR, NULL);
			}
			staticAddress = NULL;
		}
	}

	if (staticField) *staticField = (UDATA) field;

	if (definingClass) *definingClass = defClass;

	return staticAddress;
}


UDATA
getStaticFields(J9VMThread *currentThread, J9ROMClass *romClass, J9ROMFieldShape **outFields)
{
	J9JavaVM *vm = currentThread->javaVM;
	uint32_t walkFlags = J9VM_FIELD_OFFSET_WALK_INCLUDE_STATIC;
	J9Class *superclass = NULL; /* needed only for instance fields */
	UDATA count = 0;

	J9ROMFieldOffsetWalkState walkState;
	J9ROMFieldOffsetWalkResult *cursor;
#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
	J9FlattenedClassCache *flattenedClassCache = NULL; /* needed only for instance fields */
	cursor = fieldOffsetsStartDo(vm, romClass, superclass, &walkState, walkFlags, flattenedClassCache);
#else
	cursor = fieldOffsetsStartDo(vm, romClass, superclass, &walkState, walkFlags);
#endif

	while (NULL != cursor->field) {
		if (NULL != outFields) {
			outFields[count] = cursor->field;
		}

		count++;
		cursor = fieldOffsetsNextDo(&walkState);
	}

	return count;
}


VMINLINE static UDATA
calculateJ9UTFSize(UDATA stringLength)
{
	if (0 != (stringLength & 1)) {
		stringLength++;
	}

	return sizeof(J9UTF8) + stringLength;
}


VMINLINE static UDATA
calculateFakeJ9ROMFieldShapeSize(UDATA nameLength, UDATA signatureLength)
{
	return sizeof(J9ROMFieldShape) + calculateJ9UTFSize(nameLength) + calculateJ9UTFSize(signatureLength);
}


static void
initFakeJ9ROMFieldShape(J9ROMFieldShape *shape, U_16 nameLength, U_8 *nameData, U_16 signatureLength, U_8 *signatureData)
{
	J9UTF8 *nameUTF8 = (J9UTF8 *) ((UDATA)shape + sizeof(J9ROMFieldShape));
	J9UTF8 *signatureUTF8 = (J9UTF8 *) ((UDATA)nameUTF8 + calculateJ9UTFSize(nameLength));

	NNSRP_SET(shape->nameAndSignature.name, nameUTF8);
	NNSRP_SET(shape->nameAndSignature.signature, signatureUTF8);

	J9UTF8_SET_LENGTH(nameUTF8, nameLength);
	memcpy(J9UTF8_DATA(nameUTF8), nameData, J9UTF8_LENGTH(nameUTF8));

	J9UTF8_SET_LENGTH(signatureUTF8, signatureLength);
	memcpy(J9UTF8_DATA(signatureUTF8), signatureData, J9UTF8_LENGTH(signatureUTF8));

	shape->modifiers = (U_32)(fieldModifiersLookupTable[signatureData[0] - 'A'] << 16);
}


static J9ROMFieldShape *
allocAndInitFakeJ9ROMFieldShape(J9JavaVM *vm, const char *name, const char *signature)
{
	PORT_ACCESS_FROM_JAVAVM(vm);

	UDATA nameLength = strlen(name);
	UDATA signatureLength = strlen(signature);
	J9ROMFieldShape *shape = (J9ROMFieldShape *) j9mem_allocate_memory(calculateFakeJ9ROMFieldShapeSize(nameLength, signatureLength), OMRMEM_CATEGORY_VM);

	if (NULL != shape) {
		initFakeJ9ROMFieldShape(shape, (U_16)nameLength, (U_8*)name, (U_16)signatureLength, (U_8*)signature);
	}

	return shape;
}


VMINLINE static J9HiddenInstanceField *
initJ9HiddenField(J9HiddenInstanceField *field, J9UTF8 *classNameUTF8, J9ROMFieldShape *shape, UDATA *offsetReturn, J9HiddenInstanceField *next)
{
	field->className = classNameUTF8;
	field->shape = shape;
	field->fieldOffset = (UDATA)-1;
	field->offsetReturnPtr = offsetReturn;
	field->next = next;
	return field;
}

UDATA
initializeHiddenInstanceFieldsList(J9JavaVM *vm)
{
	UDATA const referenceSize = J9JAVAVM_REFERENCE_SIZE(vm);
	UDATA rc = 0;
	PORT_ACCESS_FROM_JAVAVM(vm);

	if (0 != omrthread_monitor_init_with_name(&vm->hiddenInstanceFieldsMutex, 0, "VM hidden fields list")) {
		rc = 1;
		goto exit;
	}

	vm->hiddenLockwordFieldShape = allocAndInitFakeJ9ROMFieldShape(vm, "lockword", (referenceSize == sizeof(U_32) ? "I" : "J"));
	if (NULL == vm->hiddenLockwordFieldShape) {
		goto destroyMutexAndCleanup;
	}

	vm->hiddenFinalizeLinkFieldShape = allocAndInitFakeJ9ROMFieldShape(vm, "finalizeLink", (referenceSize == sizeof(U_32) ? "I" : "J"));
	if (NULL == vm->hiddenFinalizeLinkFieldShape) {
		goto destroyMutexAndCleanup;
	}

	vm->hiddenInstanceFields = NULL;

exit:
	return rc;

destroyMutexAndCleanup:
	omrthread_monitor_destroy(vm->hiddenInstanceFieldsMutex);
	j9mem_free_memory(vm->hiddenLockwordFieldShape);
	vm->hiddenLockwordFieldShape = NULL;
	j9mem_free_memory(vm->hiddenFinalizeLinkFieldShape);
	vm->hiddenFinalizeLinkFieldShape = NULL;
	rc = 1;
	goto exit;
}


void
freeHiddenInstanceFieldsList(J9JavaVM *vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);

	/* Check if initializeHiddenInstanceFieldsList() has been called. */
	if (NULL != vm->hiddenLockwordFieldShape) {
		J9HiddenInstanceField *field = vm->hiddenInstanceFields;

		while (NULL != field) {
			J9HiddenInstanceField *next = field->next;

			j9mem_free_memory(field);
			field = next;
		}
		vm->hiddenInstanceFields = NULL;

		j9mem_free_memory(vm->hiddenLockwordFieldShape);
		vm->hiddenLockwordFieldShape = NULL;
		j9mem_free_memory(vm->hiddenFinalizeLinkFieldShape);
		vm->hiddenFinalizeLinkFieldShape = NULL;

		omrthread_monitor_destroy(vm->hiddenInstanceFieldsMutex);
	}
}

/**
 * Add an extra hidden instance field to the specified bootstrap class when it is loaded. The
 * target class must not have been loaded yet.
 *
 * The field will be hidden as far as most operations are concerned - in particular it will
 * not be part of the description - which means the GC will not automatically know about it
 * (if it's an object).
 *
 * If the offsetReturn pointer passed in is not NULL, the caller is responsible for ensuring that
 * pointer remains valid for the lifetime of the VM. The offset written to *offsetReturn will be
 * relative to the object header address (same as lockOffset and finalizeLinkOffset).
 *
 * The function can fail (and return non-zero) for the following reasons:
 *   1. An invalid fieldSignature was specified.
 *   2. The specified class has already been loaded.
 *   3. Adding the field would exceed the maximum hidden fields per class limit.
 *
 * @param vm[in] pointer to the J9JavaVM
 * @param className[in] the fully qualified name of the class to be modified (e.g. "java/lang/String" )
 * @param fieldName[in] the name of the hidden field to be added (used for information purposes only - no checks for duplicate names will be made)
 * @param fieldSignature[in] the Java signature for the field (e.g. "J" (long), "I" (int), "Ljava/lang/String;", etc)
 * @param offsetReturn[out] the address to store the offset of the field once the class is loaded; may be NULL if not needed
 * @return 0 on success, non-zero if the field could not be recorded or some other error occurred
 */
UDATA
addHiddenInstanceField(J9JavaVM *vm, const char *className, const char *fieldName, const char *fieldSignature, UDATA *offsetReturn)
{
	PORT_ACCESS_FROM_JAVAVM(vm);

	J9HiddenInstanceField *field;
	J9ROMFieldShape *shape;
	J9UTF8 *classNameUTF8;
	UDATA classNameLength = strlen(className);
	UDATA fieldNameLength = strlen(fieldName);
	UDATA fieldSignatureLength = strlen(fieldSignature);
	UDATA classNameUTF8Size = calculateJ9UTFSize(classNameLength);
	UDATA neededSize = sizeof(J9HiddenInstanceField) + classNameUTF8Size +
		calculateFakeJ9ROMFieldShapeSize(fieldNameLength, fieldSignatureLength);
	UDATA matchedFields;

	/* Verify that a valid field signature was specified. */
	if (verifyFieldSignatureUtf8((U_8*)fieldSignature, fieldSignatureLength, 0) < 0) {
		return 1;
	}

	/* Verify that the class hasn't yet been loaded. */
	if ((NULL != vm->systemClassLoader) && (NULL != hashClassTableAt(vm->systemClassLoader, (U_8*)className, classNameLength))) {
#if defined(J9VM_OPT_SNAPSHOTS)
		/* If its a restore run, the hidden field is already added */
		if (IS_RESTORE_RUN(vm)) {
			return 0;
		}
#endif /* defined(J9VM_OPT_SNAPSHOTS) */
		return 2;
	}

 	omrthread_monitor_enter(vm->hiddenInstanceFieldsMutex);

	/*
	 * Verify that adding this field won't exceed J9VM_FIELD_OFFSET_WALK_MAXIMUM_HIDDEN_FIELDS_PER_CLASS - 2.
	 * (Two fields are reserved to accommodate potential lockword and finalizeLink fields).
	 */
 	matchedFields = 0;
 	for (field = vm->hiddenInstanceFields; NULL != field; field = field->next) {
 		if (J9UTF8_DATA_EQUALS(J9UTF8_DATA(field->className), J9UTF8_LENGTH(field->className), className, classNameLength)) {
 			matchedFields++;
 		}
 	}
 	if (matchedFields > J9VM_FIELD_OFFSET_WALK_MAXIMUM_HIDDEN_FIELDS_PER_CLASS - NUMBER_OF_EXTRA_HIDDEN_FIELDS) {
 	 	omrthread_monitor_exit(vm->hiddenInstanceFieldsMutex);
 		return 3;
 	}

 	/* All is good - proceed to add the entry to the start of the list. */
	field = (J9HiddenInstanceField *) j9mem_allocate_memory(neededSize, OMRMEM_CATEGORY_VM);
	if (NULL == field) {
	 	omrthread_monitor_exit(vm->hiddenInstanceFieldsMutex);
		return 4;
	}

	classNameUTF8 = (J9UTF8 *) ((UDATA)field + sizeof(J9HiddenInstanceField));
	J9UTF8_SET_LENGTH(classNameUTF8, (U_16)classNameLength);
	memcpy(J9UTF8_DATA(classNameUTF8), className, classNameLength);

	shape = (J9ROMFieldShape *) ((UDATA)classNameUTF8 + classNameUTF8Size);
	initFakeJ9ROMFieldShape(shape, (U_16)fieldNameLength, (U_8*)fieldName, (U_16)fieldSignatureLength, (U_8*)fieldSignature);

	initJ9HiddenField(field, classNameUTF8, shape, offsetReturn, vm->hiddenInstanceFields);
 	vm->hiddenInstanceFields = field;

 	omrthread_monitor_exit(vm->hiddenInstanceFieldsMutex);

	return 0;
}

/**
 * Report a hot field if the JIT has determined that the field has met appropriate thresholds to be determined a hot field. 
 * Valid if dynamicBreadthFirstScanOrdering is enabled.
 *
 * @param javaVM[in] pointer to the J9JavaVM
 * @param reducedCpuUtil normalized cpu utilization of the method reporting the hot field
 * @param clazz pointer to the class where a hot field should be added/updated
 * @param fieldOffset value of the field offset that should be added/updated as a hot field for the given class
 * @param reducedFrequency normalized block frequency of the hot field for the method reporting the hot field
 */
void
reportHotField(J9JavaVM *javaVM, int32_t reducedCpuUtil, J9Class* clazz, uint8_t fieldOffset,  uint32_t reducedFrequency)
{
	/*
	 * Exit if the hotFieldClassInfoPool is NULL as it should of been initialized during jvm initialization.
	 * If the classLoader's hot field pool is null, create and initialize its hotFieldPool and hotFieldPoolMutex as it is required
	 * for each classLoader if scavenger dynamicBreadthFirstScanOrdering is enabled.
	 * If creating the classLoder's hotFieldPool or hotFieldPoolMutex fails, exit.
	 */
	if ((NULL == javaVM->hotFieldClassInfoPool) || ((NULL == clazz->classLoader->hotFieldPoolMutex) && (false == createClassLoaderHotFieldPool(javaVM, clazz->classLoader)))) {
		return;
	}
	/* 
	 * If the hotFieldsInfo pool element for the class does not exist already, create and initialize the class' hotFieldsInfo pool element
	 * otherwise, create/update the given hot field for the class
	 */
	if (NULL == clazz->hotFieldsInfo) {
		createClassHotFieldsInfo(javaVM, clazz, fieldOffset, reducedCpuUtil, reducedFrequency);
	}
	addOrUpdateHotField(javaVM, clazz, fieldOffset, reducedCpuUtil, reducedFrequency);
}

/**
 * Create and initialize a classLoaders hot field pool and hot field pool monitor.
 * Valid if dynamicBreadthFirstScanOrdering is enabled.
 *
 * @param javaVM[in] pointer to the J9JavaVM
 * @param classLoader pointer to the classLoader that requires a hot field pool and hot field monitor to be created for it
 */
VMINLINE bool
createClassLoaderHotFieldPool(J9JavaVM *javaVM, J9ClassLoader* classLoader)
{
	bool result = true;
	omrthread_monitor_enter(javaVM->globalHotFieldPoolMutex);
	if (NULL == classLoader->hotFieldPool) {
		classLoader->hotFieldPool = pool_new(sizeof(J9HotField),  0, 0, 0, J9_GET_CALLSITE(), J9MEM_CATEGORY_CLASSES, POOL_FOR_PORT(javaVM->portLibrary));
		if (NULL == classLoader->hotFieldPool) {
			result = false;
		} else if (0 != omrthread_monitor_init_with_name(&classLoader->hotFieldPoolMutex, 0, "Hot Field Pool")) {
			pool_kill(classLoader->hotFieldPool);
			classLoader->hotFieldPool = NULL;
			classLoader->hotFieldPoolMutex = NULL;
			result = false;
		}
	}
	omrthread_monitor_exit(javaVM->globalHotFieldPoolMutex);
	return result;
}

/**
 * Create and initialize a class' hotFieldsInfo pool element.
 * Valid if dynamicBreadthFirstScanOrdering is enabled.
 *
 * @param javaVM[in] pointer to the J9JavaVM
 * @param clazz pointer to the class where a hot field should be added
 * @param fieldOffset value of the field offset that should be added as a hot field for the given class
 * @param reducedCpuUtil normalized cpu utilization of the method reporting the hot field
 * @param reducedFrequency normalized block frequency of the hot field for the method reporting the hot field
 */
VMINLINE void
createClassHotFieldsInfo(J9JavaVM *javaVM, J9Class* clazz, uint8_t fieldOffset, int32_t reducedCpuUtil, uint32_t reducedFrequency)
{
	omrthread_monitor_enter(javaVM->hotFieldClassInfoPoolMutex);
	/*
	* Create and initialize new hotFieldsInfo pool element if it does not exist already.
	*/
	if (NULL == clazz->hotFieldsInfo) {
		J9ClassHotFieldsInfo* hotFieldsInfo = (J9ClassHotFieldsInfo *)pool_newElement(javaVM->hotFieldClassInfoPool);
		if (NULL != hotFieldsInfo) {
			hotFieldsInfo->hotFieldListLength = 0;
			hotFieldsInfo->consecutiveHotFieldSelections = 0;
			hotFieldsInfo->hotFieldOffset1 = U_8_MAX;
			hotFieldsInfo->hotFieldOffset2 = U_8_MAX;
			hotFieldsInfo->hotFieldOffset3 = U_8_MAX;
			hotFieldsInfo->classLoader = clazz->classLoader;
			clazz->hotFieldsInfo = hotFieldsInfo;
		}
	}
	omrthread_monitor_exit(javaVM->hotFieldClassInfoPoolMutex);
}

/**
 * Add or update an existing hot field for a given class.
 * Valid if dynamicBreadthFirstScanOrdering is enabled.
 *
 * @param clazz pointer to the class where a hot field should be added/updated
 * @param fieldOffset value of the field offset that should be added/updated as a hot field for the given class
 * @param reducedCpuUtil normalized cpu utilization of the method reporting the hot field
 * @param reducedFrequency normalized block frequency of the hot field for the method reporting the hot field
 */
VMINLINE void
addOrUpdateHotField(J9JavaVM *javaVM, J9Class* clazz, uint8_t fieldOffset, int32_t reducedCpuUtil, uint32_t reducedFrequency)
{
	if (NULL != clazz->hotFieldsInfo) {
		J9ClassHotFieldsInfo* hotFieldsInfo = clazz->hotFieldsInfo;
		J9ClassLoader* classLoader = clazz->classLoader;
		omrthread_monitor_enter(classLoader->hotFieldPoolMutex);		
		/* 
		 * Search the hot field list of the class to check if the hot field exists already
		 */
		J9HotField* previous = NULL;
		J9HotField* current = hotFieldsInfo->hotFieldListHead;
		while (NULL != current) {
			if (current->hotFieldOffset == fieldOffset) {
				break;
			}
			previous = current;
			current = current->next;
		}
		/* 
		 * If the hot field does not exist, create and initialize the new hot field.
		 */
		if (NULL == current) {
			if (hotFieldsInfo->hotFieldListLength >= javaVM->memoryManagerFunctions->j9gc_max_hot_field_list_length(javaVM)) {
				goto releaseMutex;
			} else {
				current = (J9HotField *)pool_newElement(classLoader->hotFieldPool);
				if (NULL == current) {
					goto releaseMutex;
				}
				hotFieldsInfo->hotFieldListLength++;
				current->hotFieldOffset = fieldOffset;
				current->hotness = 0;
				current->cpuUtil = 0;
				current->next = NULL;
			}
		}
		/*
		 * Update the existing or newly created hot field with the newly reported hot field information.
		 */
		current->hotness += (reducedFrequency * reducedCpuUtil);
		current->cpuUtil += reducedCpuUtil;
		hotFieldsInfo->isClassHotFieldListDirty = true;		
		/* 
		 * Initialize hotFieldListHead if the hotFieldList for the class is empty - this is the case if the previous pointer is NULL. 
		 * Otherwise, add the new hot field to the end of the hot field list for the class.
		 */
		if (NULL == previous) {
			hotFieldsInfo->hotFieldListHead = current;
		} else {
			previous->next = current;
		}

releaseMutex:
		omrthread_monitor_exit(classLoader->hotFieldPoolMutex);
	}
}

J9ROMFieldOffsetWalkResult *
#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
fieldOffsetsStartDo(J9JavaVM *vm, J9ROMClass *romClass, J9Class *superClazz, J9ROMFieldOffsetWalkState *state, U_32 flags, J9FlattenedClassCache *flattenedClassCache)
#else /* J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES */
fieldOffsetsStartDo(J9JavaVM *vm, J9ROMClass *romClass, J9Class *superClazz, J9ROMFieldOffsetWalkState *state, U_32 flags)
#endif /* J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES */
{

	Trc_VM_romFieldOffsetsStartDo_Entry( NULL, romClass, superClazz, flags );

	U_32 const referenceSize = J9JAVAVM_REFERENCE_SIZE(vm);
	U_32 const objectHeaderSize = J9JAVAVM_OBJECT_HEADER_SIZE(vm);

	/* init the walk state, including all counters to 0 */
	memset( state, 0, sizeof( *state ) );
	state->vm = vm;
	state->walkFlags = flags;
	state->romClass = romClass;
	state->hiddenInstanceFieldWalkIndex = (UDATA)-1;

#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
	state->flattenedClassCache = flattenedClassCache;

	ObjectFieldInfo fieldInfo(vm, romClass, flattenedClassCache);
#else /* J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES */
	ObjectFieldInfo fieldInfo(vm, romClass);
#endif /* J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES */

	/* Calculate instance size. Skip the following if we  care about only about statics */
	if (J9_ARE_ANY_BITS_SET(state->walkFlags, (J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE | J9VM_FIELD_OFFSET_WALK_INCLUDE_HIDDEN | J9VM_FIELD_OFFSET_WALK_CALCULATE_INSTANCE_SIZE))) {

		/*
		 * Step 1: Calculate the size of the superclass and backfill offset.
		 * Inherit the instance size and backfillOffset from the superclass.
		 */
		if (NULL != superClazz) {
			/*
			 * Note that in the J9Class, we do not store -1 to indicate no back fill,
			 * we store the total instance size (including the header) instead.
			 */
			fieldInfo.setSuperclassFieldsSize((U_32) superClazz->totalInstanceSize);
			if ((superClazz->totalInstanceSize + objectHeaderSize) != (UDATA)superClazz->backfillOffset) {
				fieldInfo.setSuperclassBackfillOffset(superClazz->backfillOffset - objectHeaderSize);
			}
		} else {
			fieldInfo.setSuperclassFieldsSize(0);
		}

		UDATA lockwordNeeded = (UDATA)checkLockwordNeeded( vm, romClass, superClazz, state->walkFlags );
		/*
		 * remove the lockword from Object (if there is one) only if we don't need a lockword or we do need one
		 * and we are not re-using the one from Object which we can tell because lockwordNeeded is LOCKWORD_NEEDED as
		 * opposed to the value of the existing offset.
		 */
		if ((LOCKWORD_NEEDED == lockwordNeeded)||(NO_LOCKWORD_NEEDED == lockwordNeeded)) {
			if ((NULL != superClazz) && ((UDATA)-1 != superClazz->lockOffset) && (0 == J9CLASS_DEPTH(superClazz))) {
				U_32 newSuperSize = fieldInfo.getSuperclassFieldsSize() - referenceSize;
				/*
				 * superClazz is java.lang.Object: subtract off non-inherited monitor field.
				 * Note that java.lang.Object's backfill slot can be only at the end.
				 */
				/* this may  have been rounded to 8 bytes so also get rid of the padding */
				if (fieldInfo.isSuperclassBackfillSlotAvailable()) { /* j.l.Object was not end aligned */
					newSuperSize -= ObjectFieldInfo::BACKFILL_SIZE;
					fieldInfo.setSuperclassBackfillOffset(ObjectFieldInfo::NO_BACKFILL_AVAILABLE);
				}
				fieldInfo.setSuperclassFieldsSize(newSuperSize);
			}
		}

		/*
		 * Step 2: Determine which extra hidden fields we need and prepend them to the list of hidden fields.
		 */
		J9HiddenInstanceField *extraHiddenFields = vm->hiddenInstanceFields;

		state->finalizeLinkOffset = 0;
		if ((NULL != superClazz) && (0!= superClazz->finalizeLinkOffset)) {
			/* Superclass is finalizeable */
			state->finalizeLinkOffset = superClazz->finalizeLinkOffset;
		} else {
			/*
			 * Superclass is not finalizable.
			 * We add the field if the class marked as needing finalization.  In addition when HCR is enabled we also add the field if the
			 * the class is not the class for java.lang.Object (superclass name in java.lang.object isnull) and the class has a finalize
			 * method regardless of whether this method is empty or not.  We need to do this because when HCR is enabled it is possible
			 * that the method will be re-transformed such that there finalization is needed and in that case
			 * we need the field to be in the shape of the existing objects
			 */
			if (
					(J9ROMCLASS_FINALIZE_NEEDED(romClass)) ||
					(J9ROMCLASS_HAS_EMPTY_FINALIZE(romClass) && (NULL != J9ROMCLASS_SUPERCLASSNAME(romClass)))
			) {
				extraHiddenFields = initJ9HiddenField(	&state->hiddenFinalizeLinkField, NULL,
						vm->hiddenFinalizeLinkFieldShape,
						&state->finalizeLinkOffset, extraHiddenFields);
			}
		}

		state->lockOffset = lockwordNeeded;
		if (LOCKWORD_NEEDED == state->lockOffset) {
			extraHiddenFields = initJ9HiddenField(&state->hiddenLockwordField, NULL,
					vm->hiddenLockwordFieldShape,
					&state->lockOffset, extraHiddenFields);
		}

		/*
		 * Step 3: Calculate the number of various categories of fields: single word primitive, double word primitive, and object references.
		 * Iterate over fields to count instance fields by size.
		 */

		fieldInfo.countInstanceFields();

		if (NULL != extraHiddenFields) {
			state->hiddenInstanceFieldCount = fieldInfo.countAndCopyHiddenFields(extraHiddenFields, state->hiddenInstanceFields);
			Assert_VM_true(state->hiddenInstanceFieldCount < sizeof(state->hiddenInstanceFields)/sizeof(state->hiddenInstanceFields[0]));
		}

		state->result.totalInstanceSize = fieldInfo.calculateTotalFieldsSizeAndBackfill();
#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
		state->flatBackFillSize = fieldInfo.getBackfillSize();
		state->classRequiresPrePadding = fieldInfo.doesClassRequiresPrePadding();
		state->firstFlatDoubleOffset = fieldInfo.calculateFieldDataStart();
		state->firstDoubleOffset = fieldInfo.addFlatDoublesArea(state->firstFlatDoubleOffset);
		state->firstFlatObjectOffset = fieldInfo.addDoublesArea(state->firstDoubleOffset);
		state->firstObjectOffset = fieldInfo.addFlatObjectsArea(state->firstFlatObjectOffset);
		state->firstFlatSingleOffset = fieldInfo.addObjectsArea(state->firstObjectOffset);
		state->firstSingleOffset = fieldInfo.addFlatSinglesArea(state->firstFlatSingleOffset);
#else /* J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES */
		state->firstDoubleOffset = fieldInfo.calculateFieldDataStart();
		state->firstObjectOffset = fieldInfo.addDoublesArea(state->firstDoubleOffset);
		state->firstSingleOffset = fieldInfo.addObjectsArea(state->firstObjectOffset);
#endif /* J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES */


		if (fieldInfo.isMyBackfillSlotAvailable() && fieldInfo.isBackfillSuitableFieldAvailable() ) {
#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
			BOOLEAN objectBackfillAvailable = fieldInfo.isBackfillSuitableObjectAvailable();
			if (0 != fieldInfo.getInstanceSingleCount()) {
				state->walkFlags |= J9VM_FIELD_OFFSET_WALK_BACKFILL_SINGLE_FIELD;
			} else if (objectBackfillAvailable && (0 != fieldInfo.getInstanceObjectCount())) {
				state->walkFlags |= J9VM_FIELD_OFFSET_WALK_BACKFILL_OBJECT_FIELD;
			} else if (0 != fieldInfo.getFlatAlignedSingleInstanceBackfillSize()) {
				state->walkFlags |= J9VM_FIELD_OFFSET_WALK_BACKFILL_FLAT_SINGLE_FIELD;
			} else if (objectBackfillAvailable && (0 != fieldInfo.getFlatAlignedObjectInstanceBackfillSize())) {
				state->walkFlags |= J9VM_FIELD_OFFSET_WALK_BACKFILL_FLAT_OBJECT_FIELD;
			} else if (0 != fieldInfo.getFlatUnAlignedSingleInstanceBackfillSize()) {
				state->walkFlags |= J9VM_FIELD_OFFSET_WALK_BACKFILL_FLAT_SINGLE_FIELD;
			} else if (objectBackfillAvailable && (0 != fieldInfo.getFlatUnAlignedObjectInstanceBackfillSize())) {
				state->walkFlags |= J9VM_FIELD_OFFSET_WALK_BACKFILL_FLAT_OBJECT_FIELD;
			}
#else /* J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES */
			if (fieldInfo.isBackfillSuitableInstanceSingleAvailable()) {
				state->walkFlags |= J9VM_FIELD_OFFSET_WALK_BACKFILL_SINGLE_FIELD;
			} else if (fieldInfo.isBackfillSuitableInstanceObjectAvailable()) {
				state->walkFlags |= J9VM_FIELD_OFFSET_WALK_BACKFILL_OBJECT_FIELD;
			}
#endif /* J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES */
		}

		/*
		 * Calculate offsets (from the object header) for hidden fields.  Hidden fields follow immediately the instance fields of the same type.
		 * Give instance fields priority for backfill slots.
		 * Note that the hidden fields remember their offsets, so this need be done once only.
		 */
		if (fieldInfo.isHiddenFieldOffsetResolutionRequired()) {
			UDATA hiddenSingleOffset = objectHeaderSize;
			UDATA hiddenDoubleOffset = objectHeaderSize;
			UDATA hiddenObjectOffset = objectHeaderSize;
			if (fieldInfo.isContendedClassLayout()) { /* hidden fields go immediately after the superclass fields and the instance fields which are placed on the following cache line */
				/* hidden doubles go right at the start.  No adjustment required */
				hiddenObjectOffset = hiddenDoubleOffset + (fieldInfo.getTotalDoubleCount() * sizeof(U_64));
				hiddenSingleOffset = hiddenObjectOffset + (fieldInfo.getTotalObjectCount() * referenceSize);
			} else {
				hiddenSingleOffset += state->firstSingleOffset + (fieldInfo.getNonBackfilledInstanceSingleCount() * sizeof(U_32));
				hiddenDoubleOffset += state->firstDoubleOffset + (fieldInfo.getInstanceDoubleCount() * sizeof(U_64));
				hiddenObjectOffset += state->firstObjectOffset + (fieldInfo.getNonBackfilledInstanceObjectCount() * referenceSize);
			}
			bool useBackfillForObject = false;
			bool useBackfillForSingle = false;

			if (fieldInfo.isMyBackfillSlotAvailable() && J9_ARE_NO_BITS_SET(state->walkFlags, J9VM_FIELD_OFFSET_WALK_BACKFILL_OBJECT_FIELD | J9VM_FIELD_OFFSET_WALK_BACKFILL_SINGLE_FIELD)) {
				/* There are no backfill-suitable instance fields, so let the hidden fields use the backfill slot */
				if (fieldInfo.isBackfillSuitableSingleAvailable()) {
					useBackfillForSingle = true;
				} else if (fieldInfo.isBackfillSuitableObjectAvailable()) {
					useBackfillForObject = true;
				}
			}

			for (UDATA fieldIndex = 0; fieldIndex < fieldInfo.getHiddenFieldCount(); ++fieldIndex) {
				J9HiddenInstanceField *hiddenField = state->hiddenInstanceFields[fieldIndex];
				U_32 modifiers = hiddenField->shape->modifiers;

				if (J9_ARE_ALL_BITS_SET(modifiers, J9FieldFlagObject)) {
					if (useBackfillForObject) {
						hiddenField->fieldOffset = fieldInfo.getMyBackfillOffsetForHiddenField();
						useBackfillForObject = false;
					} else {
						hiddenField->fieldOffset = hiddenObjectOffset;
						hiddenObjectOffset += referenceSize;
					}
				} else if (J9_ARE_ALL_BITS_SET(modifiers, J9FieldSizeDouble)) {
					hiddenField->fieldOffset = hiddenDoubleOffset;
					hiddenDoubleOffset += sizeof(U_64);
				} else {
					if (useBackfillForSingle) {
						hiddenField->fieldOffset = fieldInfo.getMyBackfillOffsetForHiddenField();
						useBackfillForSingle = false;
					} else {
						hiddenField->fieldOffset = hiddenSingleOffset;
						hiddenSingleOffset += sizeof(U_32);
					}
				}

				Assert_VM_unequal(hiddenField->fieldOffset, (UDATA)-1);
				if (NULL != hiddenField->offsetReturnPtr) {
					*hiddenField->offsetReturnPtr = hiddenField->fieldOffset;
					hiddenField->offsetReturnPtr = NULL;
				}
			}
		}
	}

	state->result.superTotalInstanceSize =  fieldInfo.getSuperclassFieldsSize();
	state->result.backfillOffset = fieldInfo.getSubclassBackfillOffset(); /* backfill available to subclasses */
	state->backfillOffsetToUse = fieldInfo.getMyBackfillOffset(); /* backfill offset for this class's fields */

	/* if we are not being asked to walk any fields (ie. the caller just wants the instance size), we're done now */
	if (
			J9_ARE_NO_BITS_SET( state->walkFlags, J9VM_FIELD_OFFSET_WALK_INCLUDE_STATIC | J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE | J9VM_FIELD_OFFSET_WALK_INCLUDE_HIDDEN)
			&& !fieldInfo.isContendedClassLayout()
	)	{
		state->result.field = NULL;
		Trc_VM_romFieldOffsetsStartDo_earlyExit(NULL, state->result.field, state->result.offset, state->result.index, state->result.totalInstanceSize, state->result.superTotalInstanceSize);
		return &state->result;
	}

	/* Start walking the fields. (J9ROMFieldShape*)-1 is used to tell fieldOffsetsNextDo() that it's the start. */
	state->result.field = (J9ROMFieldShape*)-1;
	fieldOffsetsNextDo(state);

	Trc_VM_romFieldOffsetsStartDo_Exit(NULL, state->result.field, state->result.offset, state->result.index, state->result.totalInstanceSize, state->result.superTotalInstanceSize);

	return &state->result;
}

J9ROMFieldOffsetWalkResult *
fieldOffsetsNextDo(J9ROMFieldOffsetWalkState *state)
{
	BOOLEAN isStartDo = ((J9ROMFieldShape*)-1 == state->result.field);
	BOOLEAN walkHiddenFields = FALSE;

	state->result.field = NULL;
#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
	state->result.flattenedClass = NULL;
#endif /* J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES */

	/*
	 * Walk regular ROM fields until we run out of them. Then switch
	 * to hidden instance fields if the caller wants them.
	 */
	if ((UDATA)-1 == state->hiddenInstanceFieldWalkIndex) {
		J9ROMFieldShape *field;

		if (isStartDo) {
			field = romFieldsStartDo(state->romClass, &(state->fieldWalkState));
		} else {
			field = romFieldsNextDo(&(state->fieldWalkState));
		}

		fieldOffsetsFindNext(state, field);
		if ((NULL == state->result.field) && (0 != (state->walkFlags & J9VM_FIELD_OFFSET_WALK_INCLUDE_HIDDEN))) {
			walkHiddenFields = TRUE;
			/* Start the walk over the hidden fields array in reverse order. */
			state->hiddenInstanceFieldWalkIndex = state->hiddenInstanceFieldCount;
		}
	} else {
		walkHiddenFields = TRUE;
	}

	if (walkHiddenFields && (0 != state->hiddenInstanceFieldWalkIndex)) {
		UDATA const objectHeaderSize = J9JAVAVM_OBJECT_HEADER_SIZE(state->vm);
		/* Note: hiddenInstanceFieldWalkIndex is the index of the last hidden instance field that was returned. */

		while (0 != state->hiddenInstanceFieldWalkIndex) {
			J9HiddenInstanceField *hiddenField = state->hiddenInstanceFields[--state->hiddenInstanceFieldWalkIndex];
			if (J9_ARE_NO_BITS_SET(state->walkFlags, J9VM_FIELD_OFFSET_WALK_ONLY_OBJECT_SLOTS)
				|| J9_ARE_ALL_BITS_SET(hiddenField->shape->modifiers, J9FieldFlagObject)
			) {
				/* If we are only looking for o-slots we've found one, or we can return anything */
				state->result.field = hiddenField->shape;
				/*
				 * This function returns offsets relative to the end of the object header,
				 * whereas fieldOffset is relative to the start of the header.
				 */
				state->result.offset = hiddenField->fieldOffset - objectHeaderSize;
				/* Hidden fields do not have a valid JVMTI index. */
				state->result.index = (UDATA)-1;
				break;
			}
		}
	}

	Trc_VM_romFieldOffsetsNextDo_result(NULL, state->result.field, state->result.offset, state->result.index);
	return &state->result;
}





/*
 * Find the next appropriate field, starting with the field passed in, storing
 * the result in state->result.
 */
static void
fieldOffsetsFindNext(J9ROMFieldOffsetWalkState *state, J9ROMFieldShape *field)
{
	J9ROMClass * romClass = state->romClass;
	UDATA const referenceSize = J9JAVAVM_REFERENCE_SIZE(state->vm);

	/* loop in case we have been told to only consider instance or static fields */
	while( field != NULL ) {
		U_32 modifiers = field->modifiers;

		/* bump the jvmti field index */
		state->result.index++;

		if( modifiers & J9AccStatic ) {
			if( state->walkFlags & J9VM_FIELD_OFFSET_WALK_INCLUDE_STATIC ) {
				if (modifiers & J9FieldFlagObject) {
					state->result.offset = state->objectStaticsSeen * sizeof(UDATA);
					state->objectStaticsSeen++;
					break;
				} else if ( 0 == (state->walkFlags & J9VM_FIELD_OFFSET_WALK_ONLY_OBJECT_SLOTS) ) {
					if (modifiers & J9FieldSizeDouble) {
						/* Add single scalar and object counts together, round up to 2 and divide by 2 to get number of doubles used by singles */
#ifdef J9VM_ENV_DATA64
						UDATA doubleSlots = romClass->objectStaticCount + romClass->singleScalarStaticCount;
#else
						UDATA doubleSlots = (romClass->objectStaticCount + romClass->singleScalarStaticCount + 1) >> 1;
#endif
						state->result.offset = doubleSlots * sizeof( U_64 ) + state->doubleStaticsSeen * sizeof( U_64 );
						state->doubleStaticsSeen++;
					} else {
						state->result.offset = romClass->objectStaticCount * sizeof(UDATA) + state->singleStaticsSeen * sizeof( UDATA );
						state->singleStaticsSeen++;
					}
					break;
				}
			}
		} else {
			if( state->walkFlags & J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE ) {
				{
					if( modifiers & J9FieldFlagObject ) {
#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
						if (J9_ARE_ALL_BITS_SET(modifiers, J9FieldFlagIsNullRestricted)) {
							J9UTF8 *fieldSig = J9ROMFIELDSHAPE_SIGNATURE(field);
							U_8 *fieldSigBytes = J9UTF8_DATA(fieldSig);
							J9Class *fieldClass = NULL;
							fieldClass = findJ9ClassInFlattenedClassCache(state->flattenedClassCache, fieldSigBytes + 1, J9UTF8_LENGTH(fieldSig) - 2);
							if (!J9_IS_FIELD_FLATTENED(fieldClass, field)) {
								if (J9_ARE_ALL_BITS_SET(state->walkFlags, J9VM_FIELD_OFFSET_WALK_BACKFILL_OBJECT_FIELD)) {
									Assert_VM_true(state->backfillOffsetToUse >= 0);
									state->result.offset = state->backfillOffsetToUse;
									state->walkFlags &= ~(UDATA)J9VM_FIELD_OFFSET_WALK_BACKFILL_OBJECT_FIELD;
								} else {
									state->result.offset = state->firstObjectOffset + state->objectsSeen * referenceSize;
									state->objectsSeen++;
								}
							} else {
								U_32 size = (U_32)fieldClass->totalInstanceSize;
								bool forceDoubleAlignment = false;
								if (sizeof(U_32) == referenceSize) {
									/** 
									 * Flattened volatile valueType that is 8 bytes should be put at 8-byte aligned address. Currently flattening is disabled
									 * for such valueType > 8 bytes.
									 */
									forceDoubleAlignment = (J9_ARE_ALL_BITS_SET(field->modifiers, J9AccVolatile) && (sizeof(U_64) == J9CLASS_UNPADDED_INSTANCE_SIZE(fieldClass)));
								} else {
									/* copyObjectFields() uses U_64 load/store. Put all nested fields at 8-byte aligned address. */
									forceDoubleAlignment = true;
								}
								state->result.flattenedClass = fieldClass;
								if (forceDoubleAlignment
									|| J9_ARE_ALL_BITS_SET(fieldClass->classFlags, J9ClassLargestAlignmentConstraintDouble)
								) {
									if (J9CLASS_HAS_4BYTE_PREPADDING(fieldClass)) {
										size -= sizeof(U_32);
									}
									state->result.offset = state->firstFlatDoubleOffset + state->currentFlatDoubleOffset;
									Assert_VM_true((state->result.offset + referenceSize) % sizeof(U_64) == 0);
									state->currentFlatDoubleOffset += ROUND_UP_TO_POWEROF2(size, sizeof(U_64));
								} else if (J9_ARE_ALL_BITS_SET(fieldClass->classFlags, J9ClassLargestAlignmentConstraintReference)) {
									size = (U_32)ROUND_UP_TO_POWEROF2(size, referenceSize);
									if (J9_ARE_ALL_BITS_SET(state->walkFlags, J9VM_FIELD_OFFSET_WALK_BACKFILL_FLAT_OBJECT_FIELD)
										&& (state->flatBackFillSize == size)
									) {
										Assert_VM_true(state->backfillOffsetToUse >= 0);
										state->result.offset = state->backfillOffsetToUse;
										state->walkFlags &= ~(UDATA)J9VM_FIELD_OFFSET_WALK_BACKFILL_FLAT_OBJECT_FIELD;
									} else {
										state->result.offset = state->firstFlatObjectOffset + state->currentFlatObjectOffset;
										state->currentFlatObjectOffset += size;
									}
								} else {
									if (J9_ARE_ALL_BITS_SET(state->walkFlags, J9VM_FIELD_OFFSET_WALK_BACKFILL_FLAT_SINGLE_FIELD)
										&& (state->flatBackFillSize == size)
									) {
										Assert_VM_true(state->backfillOffsetToUse >= 0);
										state->result.offset = state->backfillOffsetToUse;
										state->walkFlags &= ~(UDATA)J9VM_FIELD_OFFSET_WALK_BACKFILL_FLAT_SINGLE_FIELD;
									} else {
										state->result.offset = state->firstFlatSingleOffset + state->currentFlatSingleOffset;
										state->currentFlatSingleOffset += size;
									}
								}
							}
						} else {
							if (J9_ARE_ALL_BITS_SET(state->walkFlags, J9VM_FIELD_OFFSET_WALK_BACKFILL_OBJECT_FIELD)) {
								Assert_VM_true(state->backfillOffsetToUse >= 0);
								state->result.offset = state->backfillOffsetToUse;
								state->walkFlags &= ~(UDATA)J9VM_FIELD_OFFSET_WALK_BACKFILL_OBJECT_FIELD;
							} else {
								state->result.offset = state->firstObjectOffset + state->objectsSeen * referenceSize;
								state->objectsSeen++;
							}
						}
#else /* J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES */
						if (state->walkFlags & J9VM_FIELD_OFFSET_WALK_BACKFILL_OBJECT_FIELD) {
							Assert_VM_true(state->backfillOffsetToUse >= 0);
							state->result.offset = state->backfillOffsetToUse;
							state->walkFlags &= ~(UDATA)J9VM_FIELD_OFFSET_WALK_BACKFILL_OBJECT_FIELD;
						} else {
							state->result.offset = state->firstObjectOffset + state->objectsSeen * referenceSize;
							state->objectsSeen++;
						}
#endif /* J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES */
						break;
					} else if ( 0 == (state->walkFlags & J9VM_FIELD_OFFSET_WALK_ONLY_OBJECT_SLOTS) ) {
						if( modifiers & J9FieldSizeDouble ) {
							state->result.offset = state->firstDoubleOffset + state->doublesSeen * sizeof( U_64 );
							state->doublesSeen++;
						} else {
							if (state->walkFlags & J9VM_FIELD_OFFSET_WALK_BACKFILL_SINGLE_FIELD) {
								Assert_VM_true(state->backfillOffsetToUse >= 0);
								state->result.offset = state->backfillOffsetToUse;
								state->walkFlags &= ~(UDATA)J9VM_FIELD_OFFSET_WALK_BACKFILL_SINGLE_FIELD;
							} else {
								state->result.offset = state->firstSingleOffset + state->singlesSeen * sizeof( U_32 );
								state->singlesSeen++;
							}
						}
						break;
					}
				}
			}
		}
		field = romFieldsNextDo( &(state->fieldWalkState) );
	}

	state->result.field = field;
}



J9ROMFieldShape *
fullTraversalFieldOffsetsStartDo(J9JavaVM *vm, J9Class *clazz, J9ROMFullTraversalFieldOffsetWalkState *state, U_32 flags)
{
	J9ROMFieldShape * field = NULL;
	J9ROMFieldOffsetWalkResult *result;
	J9ITable *iTable;

	/* init the walk state, including all counters to 0 */
	memset( state, 0, sizeof( *state ) );
	state->javaVM = vm;
	state->walkFlags = flags;
	state->clazz = clazz;
	state->walkSuperclasses = clazz->superclasses;
	state->remainingClassDepth = (U_32)(J9CLASS_DEPTH(clazz));

	if (state->remainingClassDepth > 0) {
		state->currentClass = *state->walkSuperclasses;
		state->walkSuperclasses++;
		state->remainingClassDepth--;
	} else {
		state->currentClass = state->clazz;
		state->clazz = NULL;
	}


	if (state->walkFlags & J9VM_FIELD_OFFSET_WALK_PREINDEX_INTERFACE_FIELDS) {
		iTable = (J9ITable *) clazz->iTable;
		while (iTable) {
			/* two cases here, 
			 *   1. for classes we want to count the fields in all interfaces and super interfaces it implements
			 *   2. for interface classes we want to count only the fields in the super interfaces, not in the interface class being looked at */
			if (!(clazz->romClass->modifiers & J9AccInterface) ||
				((clazz->romClass->modifiers & J9AccInterface) && (iTable->interfaceClass != clazz))) {
				state->referenceIndexOffset += iTable->interfaceClass->romClass->singleScalarStaticCount;
				state->referenceIndexOffset += iTable->interfaceClass->romClass->objectStaticCount;
				state->referenceIndexOffset += iTable->interfaceClass->romClass->doubleScalarStaticCount;
			}
			iTable = iTable->next;
		}
	}


	while(state->currentClass) {

		if ((state->walkFlags & J9VM_FIELD_OFFSET_WALK_PREINDEX_INTERFACE_FIELDS) == 0) {
			/* add the slots for the interfaces to the index */
			iTable = (J9ITable *) state->currentClass->iTable;
			while (iTable != state->superITable) {
				/* if we are an interface, don't process ourself */
				if (state->currentClass != iTable->interfaceClass) {
					state->classIndexAdjust += iTable->interfaceClass->romClass->singleScalarStaticCount;
					state->classIndexAdjust	+= iTable->interfaceClass->romClass->objectStaticCount;
					state->classIndexAdjust	+= iTable->interfaceClass->romClass->doubleScalarStaticCount;
				}
				iTable = iTable->next;
			}
		}
#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
		result = fieldOffsetsStartDo(state->javaVM, state->currentClass->romClass, SUPERCLASS(state->currentClass), &(state->fieldOffsetWalkState), state->walkFlags, state->currentClass->flattenedClassCache);
#else /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */
		result = fieldOffsetsStartDo(state->javaVM, state->currentClass->romClass, SUPERCLASS(state->currentClass), &(state->fieldOffsetWalkState), state->walkFlags);
#endif /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */
		field = result->field;
		if (NULL != field) {
			state->fieldOffset = result->offset;
			return field;
		} else {
			state->classIndexAdjust += result->index;
		}

		state->superITable = (J9ITable *)state->currentClass->iTable;

		if (state->remainingClassDepth > 0) {
			state->currentClass = *state->walkSuperclasses;
			state->walkSuperclasses++;
			state->remainingClassDepth--;
		} else {
			state->currentClass = state->clazz;
			state->clazz = NULL;
		}
	}

	return NULL;
}


J9ROMFieldShape *
fullTraversalFieldOffsetsNextDo(J9ROMFullTraversalFieldOffsetWalkState *state)
{
	J9ROMFieldShape * field = NULL;
	J9ROMFieldOffsetWalkResult *result;
    J9ITable *iTable;

	/* have we finished walking the fields for this class? */
	result = fieldOffsetsNextDo(&(state->fieldOffsetWalkState));
	field = result->field;
	if (NULL != field) {
		state->fieldOffset = result->offset;
		return field;
	} else {
		state->classIndexAdjust += result->index;
	}

	state->superITable = (J9ITable *)state->currentClass->iTable;
	if (state->remainingClassDepth > 0) {
		state->currentClass = *state->walkSuperclasses;
		state->walkSuperclasses++;
		state->remainingClassDepth--;
	} else {
		state->currentClass = state->clazz;
		state->clazz = NULL;
	}

	while(state->currentClass) {
		
		if ((state->walkFlags & J9VM_FIELD_OFFSET_WALK_PREINDEX_INTERFACE_FIELDS) == 0) { 
			/* add the slots for the interfaces to the index */
			iTable = (J9ITable *)state->currentClass->iTable;
			while (iTable != state->superITable) {
				/* if we are an interface, don't process ourself */
				if (state->currentClass != iTable->interfaceClass) {
					state->classIndexAdjust += iTable->interfaceClass->romClass->singleScalarStaticCount;
					state->classIndexAdjust += iTable->interfaceClass->romClass->objectStaticCount;
					state->classIndexAdjust += iTable->interfaceClass->romClass->doubleScalarStaticCount;
				}
				iTable = iTable->next;
			}
		}

#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
		result = fieldOffsetsStartDo(state->javaVM, state->currentClass->romClass, SUPERCLASS(state->currentClass), &(state->fieldOffsetWalkState), state->walkFlags, state->currentClass->flattenedClassCache);
#else /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */
		result = fieldOffsetsStartDo(state->javaVM, state->currentClass->romClass, SUPERCLASS(state->currentClass), &(state->fieldOffsetWalkState), state->walkFlags);
#endif /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */

		field = result->field;
		if (NULL != field) {
			state->fieldOffset = result->offset;
			return field;
		} else {
			state->classIndexAdjust += result->index;
		}

		state->superITable = (J9ITable *)state->currentClass->iTable;
		if (state->remainingClassDepth > 0) {
			state->currentClass = *state->walkSuperclasses;
			state->walkSuperclasses++;
			state->remainingClassDepth--;
		} else {
			state->currentClass = state->clazz;
			state->clazz = NULL;
		}
	}

	return NULL;
}

#if !defined (J9VM_SIZE_SMALL_CODE)

/* returns 0 if a and b are have identical fieldName, fieldNameLength, signature, signatureLength
 * returns -1 if a is "greater than" b, 1 if "less than".
 * "greater than" and "less than" are implementation-dependent functions of the fieldName, fieldNameLength, signature, signatureLength
 */

static IDATA
compareNameAndSignature(U_8 *aName, IDATA aNameLength, U_8 *aSig, IDATA aSigLength, J9UTF8 *bName, J9UTF8 *bSig) {
	U_8 *aData;
	U_8 *bData;
	IDATA index;

	if (aNameLength != J9UTF8_LENGTH(bName)) {
		return (aNameLength < J9UTF8_LENGTH(bName)) ? 1: -1;
	} else if (aSigLength != J9UTF8_LENGTH(bSig)) {
		return  (aSigLength < J9UTF8_LENGTH(bSig))? 1: -1;
	} else {
		aData = aName;
		bData = J9UTF8_DATA(bName);
		/* search from the back since many names have common prefixes */
		index = aNameLength - 1;
		while ((index >= 0) && (aData[index] == bData[index])) {
			--index;
		}
		if (index >= 0) { /* there was a mismatch */
			return (aData[index] < bData[index])? 1: -1;
		}
		aData = aSig;
		bData = J9UTF8_DATA(bSig);
		/* search from the back since many names have common prefixes */
		index = aSigLength - 1;
		while ((index >= 0) && (aData[index] == bData[index])) {
			--index;
		}
		if (index >= 0) { /* there was a mismatch */
			return (aData[index] < bData[index])? 1: -1;
		} else {
			return 0;
		}
	}
}

static IDATA
compareFieldIDs(J9FieldTableEntry *a, J9FieldTableEntry *b) {
	J9UTF8 *aName, *bName;
	J9UTF8 *aSig, *bSig;
	
	aName = J9ROMFIELDSHAPE_NAME(a->field);
	aSig = J9ROMFIELDSHAPE_SIGNATURE(a->field);
	bName = J9ROMFIELDSHAPE_NAME(b->field);
	bSig = J9ROMFIELDSHAPE_SIGNATURE(b->field);
	return compareNameAndSignature(J9UTF8_DATA(aName), J9UTF8_LENGTH(aName), J9UTF8_DATA(aSig), J9UTF8_LENGTH(aSig), bName, bSig);
}		

static IDATA
compareNameAndSigWithFieldID(U_8 *aName, IDATA aNameLength, U_8 *aSig, IDATA aSigLength, J9FieldTableEntry *b) {
	J9UTF8 *bName;
	J9UTF8 *bSig;
	
	bName = J9ROMFIELDSHAPE_NAME(b->field);
	bSig = J9ROMFIELDSHAPE_SIGNATURE(b->field);
	return compareNameAndSignature(aName,  aNameLength, aSig, aSigLength, bName, bSig);
}		

/** 
 * quicksort the list of fields, via one level of indirection: the array of structs is
 * unsorted, but the pointers to the struct are in sorted order
 * @param list
 * Vector of pointers to field descriptors
 * @start index of the first element to be sorted
 * @end index of the last element to be sorted
 * precondition: start < end
 * precondition: all items in list are unique
 */

static void
sortFieldIndex(J9FieldTableEntry* list, IDATA start, IDATA end) {
	J9FieldTableEntry pivot;
	J9FieldTableEntry swap;
	IDATA scanUp, scanDown, result;
	
	scanUp = start; scanDown = end;
	pivot.field = list[(start+end)/2].field;
	do {
		while ((compareFieldIDs(&list[scanUp], &pivot) == 1)&& (scanUp < scanDown)) {
			++scanUp;
		}
		while (((result = compareFieldIDs(&pivot, &list[scanDown])) == 1)&& (scanUp < scanDown)) {
			--scanDown;
		}
		if (scanUp < scanDown) {
			swap.field = list[scanDown].field;
			swap.offset = list[scanDown].offset;
			list[scanDown].field = list[scanUp].field;
			list[scanDown].offset = list[scanUp].offset;
			list[scanUp].field = swap.field;
			list[scanUp].offset = swap.offset;
		}
	}  while (scanUp < scanDown);
	/* 
	 * invariant: scanUp == scanDown
	 * invariant: result contains the comparison of the value at scanUp
	 * invariant: if scanUp == end, the entire list, with the possible exception of the last element, is less than or equal to the pivot
	 * invariant: everything from scanUp+1 and up is greater than the pivot value 
	 * the value at scanUp may be greater, less, or same as the pivot
	 */
	if ((end - start) >= 2) { /* list of length 1 or 2 is already sorted */
		if (result != -1) { /* scanUp value is greater than or equal to pivot */
			--scanUp;
		} 
		if (result != 1) { /* scanUp value is less than or equal to pivot */
			++scanDown;
		}
		if (scanUp > start) {
			sortFieldIndex(list, start, scanUp);
		}
		if (scanDown < end) {
			sortFieldIndex(list, scanDown, end);
		}
	}
}

/**
 * Build a list of field descriptors and a list of pointers to the descriptors.
 * Add the list to a linked list of indexes.
 * The descriptors are in the class file order, the pointers are in sorted order as defined
 * by compareFields
 */
 
static J9FieldTable*
createFieldTable(J9VMThread *vmThread, J9Class *clazz) {
	J9ROMClass* romClass = clazz->romClass;
	J9ROMFieldOffsetWalkState state;
	J9ROMFieldOffsetWalkResult *result;
	J9FieldTable* newTable; /* list of field descriptors.  This is used as the searchable index */
	UDATA count = 0;
	J9FieldTableEntry* fieldList;
	J9JavaVM *javaVM = J9VMTHREAD_JAVAVM(vmThread);

	PORT_ACCESS_FROM_VMC(vmThread);
	/* check the following for allocation failures */
	Trc_VM_createFieldTable_Entry(vmThread, clazz, romClass->romFieldCount);
	newTable = (J9FieldTable*) j9mem_allocate_memory(sizeof(J9FieldTable), OMRMEM_CATEGORY_VM);
	fieldList = (J9FieldTableEntry*) j9mem_allocate_memory(romClass->romFieldCount * sizeof(J9FieldTableEntry), OMRMEM_CATEGORY_VM);
	
	/* get the field names and put them in the list in unsorted order */
#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
	result = fieldOffsetsStartDo(javaVM, romClass, SUPERCLASS(clazz),
		&state, J9VM_FIELD_OFFSET_WALK_INCLUDE_STATIC | J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE, clazz->flattenedClassCache);
#else /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */
	result = fieldOffsetsStartDo(javaVM, romClass, SUPERCLASS(clazz),
		&state, J9VM_FIELD_OFFSET_WALK_INCLUDE_STATIC | J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE);
#endif /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */

	while (result->field != NULL) {
		/* compare name and sig */
		fieldList[count].field = result->field;
		fieldList[count].offset = result->offset;
		if (result->field->modifiers & J9AccStatic) {
			fieldList[count].offset += (UDATA)clazz->ramStatics;
		}
		if (TrcEnabled_Trc_VM_FieldOffset) {
			J9UTF8* className = J9ROMCLASS_CLASSNAME(clazz->romClass);
			J9UTF8 *fieldName = J9ROMFIELDSHAPE_NAME(result->field);
			Trc_VM_FieldOffset(vmThread, J9UTF8_LENGTH(className), J9UTF8_DATA(className),  J9UTF8_LENGTH(fieldName), J9UTF8_DATA(fieldName), result->offset);
		}
		result = fieldOffsetsNextDo(&state);
		++count;
	}

	newTable->length = count;
	newTable->fieldList = fieldList;
	sortFieldIndex(newTable->fieldList, 0, count-1);

	Trc_VM_createFieldTable_Exit(vmThread, clazz, newTable, newTable->fieldList, newTable->length);
	return newTable;
}

static J9ROMFieldShape *
findFieldInTable(J9VMThread *vmThread, J9Class *clazz, U_8 *fieldName, UDATA fieldNameLength, U_8 *signature, UDATA signatureLength, UDATA *offsetOrAddress) {
	J9FieldTable* fieldTable;
	UDATA hi, lo, probe; /* used to search the list */
	J9FieldTableEntry* fieldList;
	IDATA mag; /* result of magnitude comparison */
	
	Trc_VM_findFieldInTable_Entry(vmThread, clazz, fieldNameLength, fieldName, signatureLength, signature);
	fieldTable = fieldIndexTableGet(vmThread->javaVM,  clazz);
	
	if (fieldTable == NULL) {
		fieldTable = createFieldTable(vmThread, clazz);
		fieldIndexTableAdd(vmThread->javaVM,  clazz, fieldTable);
	}
	lo = 0;
	hi = fieldTable->length-1;
	probe = hi/2;
	fieldList = fieldTable->fieldList;
	
	while (((mag = compareNameAndSigWithFieldID(fieldName,fieldNameLength,signature,signatureLength, &fieldList[probe])) != 0) &&
		(hi != lo)) { /* stop when we find a match or narrow it to a single item */
		if (mag == -1) { /* key is before probe */
			lo = probe + 1;
		} else {
			hi = (probe == lo)? probe: probe - 1;
		}
		probe = (lo + hi)/2;
	}
	Trc_VM_findFieldInTable_Exit(vmThread, clazz, fieldNameLength, fieldName, signatureLength, signature);
	if (mag == 0) { /* found match */
		if (offsetOrAddress != NULL) {
			*offsetOrAddress = fieldList[probe].offset;
		}
		return fieldList[probe].field;
	} else {
		return NULL;
	}
}

/* ============================================ J9FieldTable methods ===================================*/
/**
 *  Create a hash key based on the RAM class
 * @param key RAM class pointer
 * @param userData not used
 */
 
static UDATA 
ramClassHashFn (void *key, void *userData) 
{
	fieldIndexTableEntry *tableEntry = (fieldIndexTableEntry *) key;
	
	return (UDATA) tableEntry->ramClass;
}
 
/**
 * Compare two RAM class pointers for equality
 * @param leftKey RAM class pointer
 * @param rightKey RAM class pointer
 * @param userData not used
 */
static UDATA 
ramClassHashEqualFn (void *leftKey, void *rightKey, void *userData)
{
	fieldIndexTableEntry *leftEntry = (fieldIndexTableEntry *) leftKey;
	fieldIndexTableEntry *rightEntry = (fieldIndexTableEntry *) rightKey;

	return (leftEntry->ramClass == rightEntry->ramClass);
}

/**
 * Remove fieldTables when ANY classes are unloaded. A fieldTable is rebuilt if required.
 * Deletes the hash table entries and any data structure pointed to by that entry.
 * This is not thread safe: must be called when the caller has exclusive VM access.
 * userData: java VM
 */
static void
hookFieldTablePurge(J9HookInterface** hook, UDATA eventNum, void* eventData, void* userData)
{
	J9JavaVM *vm = (J9JavaVM *) userData;
	J9HashTableState handle;
	fieldIndexTableEntry *fitEntry;

	PORT_ACCESS_FROM_VMC(vm);
	fitEntry = (fieldIndexTableEntry *) hashTableStartDo(vm->fieldIndexTable,  &handle);
	while (fitEntry ) {
		Trc_VM_hookFieldTablePurge_Entry(fitEntry, fitEntry->table, (fitEntry->table? (fitEntry->table->fieldList) : NULL));
		j9mem_free_memory(fitEntry->table->fieldList);
		j9mem_free_memory(fitEntry->table);
		hashTableDoRemove(&handle);
		fitEntry = (fieldIndexTableEntry *) hashTableNextDo(&handle);
	}
}

/**
 * Create a new hash table to hold a list of classes being indexed.
 * This is not thread safe. It is called at VM startup.
 * @param vm: Reference to the VM, used to locate the table.
 * @return  pointer to the hash table
 */
J9HashTable * 
fieldIndexTableNew(J9JavaVM* vm, J9PortLibrary *portLib) 
{
	J9HashTable *result = NULL;
	const IDATA initialSize = 64;
	J9HookInterface ** vmHooks = J9_VM_FUNCTION_VIA_JAVAVM(vm, getVMHookInterface)(vm);

#if defined(J9VM_GC_DYNAMIC_CLASS_UNLOADING)
	(*vmHooks)->J9HookRegisterWithCallSite(vmHooks, J9HOOK_VM_CLASSES_UNLOAD, hookFieldTablePurge, OMR_GET_CALLSITE(), vm);
#endif
	result = vm->fieldIndexTable = hashTableNew(OMRPORT_FROM_J9PORT(portLib), J9_GET_CALLSITE(), initialSize,
		sizeof(fieldIndexTableEntry), sizeof(U_8* ), 0, OMRMEM_CATEGORY_VM, ramClassHashFn, ramClassHashEqualFn, NULL, vm);
	vm->fieldIndexTable = result;
    Trc_VM_fieldIndexTableNew(result);
	return result;
}

/**
 * Delete the fieldIndexTable hash table from the VM datastructure and free the hash table.
 * This is not thread safe.  Called during VM shutdown.
 * @param vm: Reference to the VM, used to locate the table.
 */

void
fieldIndexTableFree(J9JavaVM* vm)
{
	if (vm->fieldIndexTable != NULL) {
		hookFieldTablePurge(NULL, 0, NULL, vm);
		hashTableFree(vm->fieldIndexTable);
		vm->fieldIndexTable = NULL;
	}
}

/**
 * Adds the ramClass in the fieldIndexTable table.
 * @param vm: Reference to the VM, used to locate the table.
 * @param ramClass: key for the table
 * index: struct describing the field index for the class
 * @returns new entry
 * @exceptions none
 */
static fieldIndexTableEntry*
fieldIndexTableAdd(J9JavaVM* vm, J9Class *ramClass, J9FieldTable *table)
{	
	struct fieldIndexTableEntry query;
	struct fieldIndexTableEntry *result;

	query.ramClass = ramClass;
	query.table = table;
	omrthread_monitor_enter(vm->fieldIndexMutex);
	result = (fieldIndexTableEntry *) hashTableAdd(vm->fieldIndexTable,  &query);
	omrthread_monitor_exit(vm->fieldIndexMutex);
	Trc_VM_fieldIndexTableAdd(result, query.ramClass, query.table);
	return result;
}

/**
 * Locates the {classLoader,className} entry exists in the fieldIndexTable.
 * Return the entry struct.
 * @param vm: used for looking up hash table and reporting tracepoints
 * @param ramClass: used to locate the index
 * @return the index for ramClass.  If the entry does not exist, return NULL
 */
static J9FieldTable*
fieldIndexTableGet(J9JavaVM* vm,  J9Class *ramClass)
{
	struct fieldIndexTableEntry *result;
	struct fieldIndexTableEntry query;
	
	query.ramClass = ramClass;
	omrthread_monitor_enter(vm->fieldIndexMutex);
	result = (fieldIndexTableEntry *) hashTableFind(vm->fieldIndexTable, &query);
	omrthread_monitor_exit(vm->fieldIndexMutex);
	return (result == NULL)? NULL: result->table;
}

 /**
 * Removes the ramClass from the fieldIndexTable table.
 * @param vm: Reference to the VM, used to locate the table.
 * @param ramClass: key for the table
 * @returns none
 * @exceptions none
 */
void
fieldIndexTableRemove(J9JavaVM* vm, J9Class *ramClass)
{	
	/* the functions that add to the table are ifdef-ed out, so the table is never populated */
	struct fieldIndexTableEntry query;
	U_32 result;

	query.ramClass = ramClass;
	omrthread_monitor_enter(vm->fieldIndexMutex);
	result = hashTableRemove(vm->fieldIndexTable,  &query);
	omrthread_monitor_exit(vm->fieldIndexMutex);
	Trc_VM_fieldIndexTableRemove(query.ramClass, result);
	return;
}
 


#endif
} /* extern "C" */
