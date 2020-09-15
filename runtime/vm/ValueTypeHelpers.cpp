/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
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

#include "ValueTypeHelpers.hpp"

#include "j9.h"
#include "ut_j9vm.h"
#include "ObjectAccessBarrierAPI.hpp"
#include "vm_api.h"

extern "C" {

j9object_t
getFlattenableField(J9VMThread *currentThread, J9RAMFieldRef *cpEntry, j9object_t receiver, BOOLEAN fastPath)
{
	MM_ObjectAccessBarrierAPI objectAccessBarrier(currentThread);
	MM_ObjectAllocationAPI objectAllocate(currentThread);
	return VM_ValueTypeHelpers::getFlattenableField(currentThread, objectAccessBarrier, objectAllocate, cpEntry, receiver, fastPath != FALSE);
}

j9object_t
cloneValueType(J9VMThread *currentThread, J9Class *receiverClass, j9object_t original, BOOLEAN fastPath)
{
	MM_ObjectAccessBarrierAPI objectAccessBarrier(currentThread);
	MM_ObjectAllocationAPI objectAllocate(currentThread);
	return VM_ValueTypeHelpers::cloneValueType(currentThread, objectAccessBarrier, objectAllocate, receiverClass, original, fastPath != FALSE);
}

void
putFlattenableField(J9VMThread *currentThread, J9RAMFieldRef *cpEntry, j9object_t receiver, j9object_t paramObject)
{
	MM_ObjectAccessBarrierAPI objectAccessBarrier(currentThread);
	VM_ValueTypeHelpers::putFlattenableField(currentThread, objectAccessBarrier, cpEntry, receiver, paramObject);
}

void
calculateFlattenedFieldAddresses(J9VMThread *currentThread, J9Class *clazz)
{
	UDATA length = clazz->flattenedClassCache->numberOfEntries;

	for (UDATA index = 0; index < length; index++) {
		J9FlattenedClassCacheEntry *entry = J9_VM_FCC_ENTRY_FROM_CLASS(clazz, index);
		J9Class *definingClass = NULL;
		J9ROMFieldShape *field = NULL;
		J9ROMFieldShape *entryField = entry->field;
		J9UTF8 *name = J9ROMFIELDSHAPE_NAME(entryField);
		J9UTF8 *signature = J9ROMFIELDSHAPE_SIGNATURE(entryField);

		if (J9_VM_FCC_ENTRY_IS_STATIC_FIELD(entry)) {
			entry->offset = (UDATA) staticFieldAddress(currentThread, clazz, J9UTF8_DATA(name), J9UTF8_LENGTH(name), J9UTF8_DATA(signature), J9UTF8_LENGTH(signature), &definingClass, (UDATA *)&field, 0, clazz);
		} else {
			entry->offset = instanceFieldOffset(currentThread, clazz, J9UTF8_DATA(name), J9UTF8_LENGTH(name), J9UTF8_DATA(signature), J9UTF8_LENGTH(signature), &definingClass, (UDATA *)&field, 0);
		}
		Assert_VM_unequal(UDATA_MAX, entry->offset);
	}
}

void
defaultValueWithUnflattenedFlattenables(J9VMThread *currentThread, J9Class *clazz, j9object_t instance)
{
	J9FlattenedClassCacheEntry * entry = NULL;
	J9Class * entryClazz = NULL;
	UDATA length = clazz->flattenedClassCache->numberOfEntries;
	UDATA const objectHeaderSize = J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread);
	MM_ObjectAccessBarrierAPI objectAccessBarrier(currentThread);
	for (UDATA index = 0; index < length; index++) {
		entry = J9_VM_FCC_ENTRY_FROM_CLASS(clazz, index);
		entryClazz = J9_VM_FCC_CLASS_FROM_ENTRY(entry);
		if (!J9_VM_FCC_ENTRY_IS_STATIC_FIELD(entry) && J9_ARE_NO_BITS_SET(J9ClassIsFlattened, entryClazz->classFlags)) {
			objectAccessBarrier.inlineMixedObjectStoreObject(currentThread,
												instance,
												entry->offset + objectHeaderSize,
												entryClazz->flattenedClassCache->defaultValue,
												false);
		}
	}
}

void
classPrepareWithWithUnflattenedFlattenables(J9VMThread *currentThread, J9Class *clazz, J9FlattenedClassCacheEntry *entry, J9Class *entryClazz)
{
	MM_ObjectAccessBarrierAPI objectAccessBarrier(currentThread);
	objectAccessBarrier.inlineStaticStoreObject(currentThread, clazz, (j9object_t *)entry->offset, entryClazz->flattenedClassCache->defaultValue);
}

BOOLEAN
valueTypeCapableAcmp(J9VMThread *currentThread, j9object_t lhs, j9object_t rhs)
{
	MM_ObjectAccessBarrierAPI objectAccessBarrier(currentThread);
	return VM_ValueTypeHelpers::acmp(currentThread, objectAccessBarrier, lhs, rhs);
}

BOOLEAN
isNameOrSignatureQtype(J9UTF8 *utfWrapper)
{
	return VM_ValueTypeHelpers::isNameOrSignatureQtype(utfWrapper);
}

BOOLEAN
isClassRefQtype(J9Class *cpContextClass, U_16 cpIndex)
{
	return VM_ValueTypeHelpers::isClassRefQtype((J9ConstantPool *) cpContextClass->ramConstantPool, cpIndex);
}

UDATA
findIndexInFlattenedClassCache(J9FlattenedClassCache *flattenedClassCache, J9ROMNameAndSignature *nameAndSignature)
{
        UDATA index = UDATA_MAX;
        UDATA length = flattenedClassCache->numberOfEntries;

        for (UDATA i = 0; i < length; i++) {
                J9ROMFieldShape *fccEntryField = J9_VM_FCC_ENTRY_FROM_FCC(flattenedClassCache, i)->field;
                if (J9UTF8_EQUALS(J9ROMNAMEANDSIGNATURE_NAME(nameAndSignature), J9ROMFIELDSHAPE_NAME(fccEntryField))
                        && J9UTF8_EQUALS(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSignature), J9ROMFIELDSHAPE_SIGNATURE(fccEntryField))
                ) {
                        index = i;
                        break;
                }
        }
        return index;
}

J9Class *
findJ9ClassInFlattenedClassCache(J9FlattenedClassCache *flattenedClassCache, U_8 *className, UDATA classNameLength)
{
        UDATA length = flattenedClassCache->numberOfEntries;
        J9Class *clazz = NULL;

        for (UDATA i = 0; i < length; i++) {
                J9UTF8* currentClassName = J9ROMCLASS_CLASSNAME(J9_VM_FCC_ENTRY_FROM_FCC(flattenedClassCache, i)->clazz->romClass);
                if (J9UTF8_DATA_EQUALS(J9UTF8_DATA(currentClassName), J9UTF8_LENGTH(currentClassName), className, classNameLength)) {
                        clazz = J9_VM_FCC_CLASS_FROM_ENTRY(J9_VM_FCC_ENTRY_FROM_FCC(flattenedClassCache, i));
                        break;
                }
        }

        Assert_VM_notNull(clazz);
        return clazz;
}

UDATA
getFlattenableFieldOffset(J9Class *fieldOwner, J9ROMFieldShape *field)
{
        Assert_VM_notNull(fieldOwner);
        Assert_VM_notNull(field);

        J9FlattenedClassCache *flattenedClassCache = fieldOwner->flattenedClassCache;
        J9ROMNameAndSignature *nameAndSig = &field->nameAndSignature;
        UDATA fieldIndex = findIndexInFlattenedClassCache(flattenedClassCache, nameAndSig);
        Assert_VM_unequal(UDATA_MAX, fieldIndex);
        J9FlattenedClassCacheEntry * flattenedClassCacheEntry = J9_VM_FCC_ENTRY_FROM_FCC(flattenedClassCache, fieldIndex);
        UDATA fieldOffset = flattenedClassCacheEntry->offset;
        return fieldOffset;
}

BOOLEAN
isFlattenableFieldFlattened(J9Class *fieldOwner, J9ROMFieldShape *field)
{
        Assert_VM_notNull(fieldOwner);
        Assert_VM_notNull(field);

        J9Class* clazz = getFlattenableFieldType(fieldOwner, field);
        BOOLEAN fieldFlattened = J9_ARE_ALL_BITS_SET((clazz)->classFlags, J9ClassIsFlattened);

        return fieldFlattened;
}

J9Class *
getFlattenableFieldType(J9Class *fieldOwner, J9ROMFieldShape *field)
{
        Assert_VM_notNull(fieldOwner);
        Assert_VM_notNull(field);

        J9FlattenedClassCache *flattenedClassCache = fieldOwner->flattenedClassCache;
        J9ROMNameAndSignature *nameAndSig = &field->nameAndSignature;
        UDATA fieldIndex = findIndexInFlattenedClassCache(flattenedClassCache, nameAndSig);
        Assert_VM_unequal(UDATA_MAX, fieldIndex);
        J9Class * fieldType = J9_VM_FCC_CLASS_FROM_ENTRY(J9_VM_FCC_ENTRY_FROM_FCC(flattenedClassCache, fieldIndex));

        return fieldType;
}

UDATA
getFlattenableFieldSize(J9VMThread *currentThread, J9Class *fieldOwner, J9ROMFieldShape *field)
{
        Assert_VM_notNull(fieldOwner);
        Assert_VM_notNull(field);

        UDATA instanceSize = J9VMTHREAD_REFERENCE_SIZE(currentThread);
        if (isFlattenableFieldFlattened(fieldOwner, field)) {
                J9Class* clazz = getFlattenableFieldType(fieldOwner, field);
                instanceSize = (clazz)->totalInstanceSize - (clazz)->backfillOffset;
        }
        return instanceSize;
}

UDATA
arrayElementSize(J9ArrayClass* arrayClass)
{
        Assert_VM_notNull(arrayClass);
        return arrayClass->flattenedElementSize;
}

void
storeFlattenableArrayElement(J9VMThread *currentThread, j9object_t receiverObject, U_32 index, j9object_t paramObject)
{
        MM_ObjectAccessBarrierAPI objectAccessBarrier(currentThread);
        VM_ValueTypeHelpers::storeFlattenableArrayElement(currentThread, objectAccessBarrier, receiverObject, index, paramObject);
}

j9object_t
loadFlattenableArrayElement(J9VMThread *currentThread, j9object_t receiverObject, U_32 index, BOOLEAN fast)
{
        MM_ObjectAccessBarrierAPI objectAccessBarrier(currentThread);
        MM_ObjectAllocationAPI objectAllocate(currentThread);
        return VM_ValueTypeHelpers::loadFlattenableArrayElement(currentThread, objectAccessBarrier, objectAllocate, receiverObject, index, fast != false);
}

} /* extern "C" */
