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
#include "j9protos.h"
#include "j9consts.h"
#include "rommeth.h"
#include "ut_j9vm.h"
#include "vm_internal.h"
#include "locknursery.h"
#include "util_api.h"

static const UDATA slotsPerShapeElement = sizeof(UDATA) * 8; /* Each slot is represented by one bit */

#ifdef J9VM_GC_LEAF_BITS
/*
 * Determine if the specified field is a leaf field, i.e. a field which can only
 * contain leaf types, such as primitive arrays. (In the current implementation
 * only primitive arrays are considered leaves.
 */
static BOOLEAN
isLeafField(J9ROMFieldShape* field)
{
	BOOLEAN isLeaf = FALSE;
	J9UTF8 *sig = J9ROMFIELDSHAPE_SIGNATURE(field);
	
	/* an object field is a leaf reference if its signature is a base type array - all base type arrays
	 * have signatures of length 2, and nothing else has a signature of length 2 
	 */
	if( J9UTF8_LENGTH(sig) == 2 ) {
		isLeaf = TRUE;
	}
	
	return isLeaf;
}
#endif /* J9VM_GC_LEAF_BITS */

/* Calculate total instance size, instance description bits, and leaf descriptions bits (if build flag enabled)
	for the given class. Uses *storage if the description doesn't fit in one tagged slot (ie. class has more than
	# bits in slot - 1 fields), otherwise does not write to *storage.
	Parameters:
		ramClass: the ram class to calculate description info for, must not be NULL
		ramSuperClass: the parent class which already has an instance description, may be NULL (eg. java.lang.Object)
		storage: space to store the description if it does not fit in a slot
*/
void
calculateInstanceDescription( J9VMThread *vmThread, J9Class *ramClass, J9Class *ramSuperClass, UDATA *storage, J9ROMFieldOffsetWalkState *walkState, J9ROMFieldOffsetWalkResult *walkResult)
{
	UDATA const referenceSize = J9VMTHREAD_REFERENCE_SIZE(vmThread);
	UDATA const objectHeaderSize = J9VMTHREAD_OBJECT_HEADER_SIZE(vmThread);
	UDATA superClassSize, totalSize, temp, shapeSlots;
	UDATA *shape;

	J9UTF8 *className = J9ROMCLASS_CLASSNAME(ramClass->romClass);
	BOOLEAN shouldSaveSelfReferencingFields = J9_ARE_ALL_BITS_SET(vmThread->javaVM->extendedRuntimeFlags2, J9_EXTENDED_RUNTIME2_ENABLE_DEEPSCAN);

#ifdef J9VM_GC_LEAF_BITS
	UDATA leafTemp;
	UDATA *leafShape;
	UDATA isString = J9UTF8_LITERAL_EQUALS(J9UTF8_DATA(className), J9UTF8_LENGTH(className), "java/lang/String");
#endif

	Trc_VM_calculateInstanceDescription_Entry( vmThread, ramClass, ramSuperClass, storage );

	{
		/* Store the totalInstanceSize and backfillOffset into the J9Class.
		 *
		 * Note that in the J9Class, we do not store -1 to indicate no back fill,
		 * we store the total instance size (including the header) instead.
		 */
		ramClass->totalInstanceSize = walkResult->totalInstanceSize;
		ramClass->backfillOffset = objectHeaderSize + ((walkResult->backfillOffset == -1) ?	walkResult->totalInstanceSize : walkResult->backfillOffset);

#ifdef J9VM_OPT_VALHALLA_VALUE_TYPES
		if (J9ROMCLASS_IS_VALUE(ramClass->romClass)) {
			ramClass->backfillOffset = walkResult->backfillOffset;
		}
#endif /* J9VM_OPT_VALHALLA_VALUE_TYPES */

		/* write lockword offset into ramClass */
		ramClass->lockOffset =	walkState->lockOffset;
		ramClass->finalizeLinkOffset = walkState->finalizeLinkOffset;
	}
	
	/* convert all sizes from bytes to object slots */
	superClassSize = walkResult->superTotalInstanceSize / referenceSize;
	totalSize = walkResult->totalInstanceSize / referenceSize;

	/* calculate number of slots required to store description bits */
	shapeSlots = ROUND_UP_TO_POWEROF2(totalSize, slotsPerShapeElement);
	shapeSlots = shapeSlots / slotsPerShapeElement;

	/* pick the space to store the description into */
	if (totalSize < slotsPerShapeElement) {
		temp = 0;
		shape = &temp;
#ifdef J9VM_GC_LEAF_BITS
		leafTemp = 0;
		leafShape = &leafTemp;
#endif
	} else {
		/* this storage will be in the ram class, which is zeroed for us */
		shape = storage;
#ifdef J9VM_GC_LEAF_BITS
		leafShape = storage + shapeSlots;
#endif
	}

	/* Copy superclass description */
	if (ramSuperClass) {
		if ((UDATA)ramSuperClass->instanceDescription & 1) {
			/* superclass description is tagged and fits in one slot */
			*shape = (UDATA)ramSuperClass->instanceDescription >> 1;
#ifdef J9VM_GC_LEAF_BITS
			*leafShape = (UDATA)ramSuperClass->instanceLeafDescription >> 1;
#endif
		} else {
			/* superclass description is stored indirect */
			UDATA i, superSlots;
			superSlots = ROUND_UP_TO_POWEROF2(superClassSize, slotsPerShapeElement);
			superSlots = superSlots / slotsPerShapeElement;

			for (i = 0; i < superSlots; i++) {
				shape[i] = ramSuperClass->instanceDescription[i];
#ifdef J9VM_GC_LEAF_BITS
				leafShape[i] = ramSuperClass->instanceLeafDescription[i];
#endif
			}
		}

		/* If there are self referencing field offsets being inherited then self referencing fields of this class should be ignored.*/
		if (shouldSaveSelfReferencingFields) {
			shouldSaveSelfReferencingFields = (ramSuperClass->selfReferencingField1 == 0);
		}
	}

	/* calculate the description for this class - walk object instance fields and 
	 * set the corresponding description bit for each 
	 */
	{
		while (walkResult->field) {
			UDATA slotOffset = walkResult->offset / (referenceSize * slotsPerShapeElement);
			J9UTF8 *fieldSig = J9ROMFIELDSHAPE_SIGNATURE(walkResult->field);
			U_8 *fieldSigBytes = J9UTF8_DATA(fieldSig);
			U_16 fieldSigLength = J9UTF8_LENGTH(fieldSig);

			/* If the field is self referencing then store the offset to it (at most 2). Self referencing fields
			 * are to be scanned with priority during GC. Both self referencing fields must be from the same class.
			 */
			if (shouldSaveSelfReferencingFields && ((ramClass->selfReferencingField1 == 0) || (ramClass->selfReferencingField2 == 0))) {
				if (J9UTF8_DATA_EQUALS(J9UTF8_DATA(className), J9UTF8_LENGTH(className), fieldSigBytes + 1, fieldSigLength - 2)) {
					if (ramClass->selfReferencingField1 == 0) {
						ramClass->selfReferencingField1 = walkResult->offset + objectHeaderSize;
					} else {
						ramClass->selfReferencingField2 = walkResult->offset + objectHeaderSize;
					}
				}
			}

#ifdef J9VM_OPT_VALHALLA_VALUE_TYPES
			if ('Q' == *fieldSigBytes) {
				J9Class *fieldClass = walkResult->flattenedClass;
				if ((NULL != fieldClass) && J9_ARE_ALL_BITS_SET(fieldClass->classFlags, J9ClassIsFlattened)) {
					UDATA size = fieldClass->totalInstanceSize;

					/* positive means the field will spill over to the next slot in the shape array */
					IDATA spillAmount = ((walkResult->offset + size) - (slotOffset * (referenceSize * slotsPerShapeElement))) - (referenceSize * slotsPerShapeElement);
					UDATA shift = ((walkResult->offset  % (referenceSize * slotsPerShapeElement)) / referenceSize);
					if (0 >= spillAmount) {
						/* If we are here this means the description bits for the field fits within the current
						 * slot shape word. This also means they are all low tagged.
						 */
						UDATA bits = 0;
						UDATA description = (UDATA) fieldClass->instanceDescription;

						/* remove low tag bit */
						description >>= 1;
						bits = (UDATA) description << shift;
						shape[slotOffset] |= bits;
					} else {
						UDATA description = (UDATA) fieldClass->instanceDescription;

						if (J9_ARE_ALL_BITS_SET(description, 1)) {
							/* simple case where the field has less than 64 (or 32 slots if in 32bit mode) slots. Split the instance
							 * bits into two parts, put the first part at the current slot offset put the last part in the next one */
							UDATA nextDescription = 0;
							UDATA spillBits = spillAmount/referenceSize;

							/* remove low tag bit */
							description >>= 1;

							nextDescription = description;
							nextDescription >>= ((size/referenceSize) - spillBits);
							description <<= shift;
							shape[slotOffset] |= description;
							slotOffset += 1;
							shape[slotOffset] |= nextDescription;
						} else {
							/* complex case were field is larger than 64 slots. Just add the bits
							 * one at a time. */
							UDATA *descriptionPtr = (UDATA *)description;
							UDATA totalAmountLeft = size/referenceSize;
							UDATA positionInDescriptionWord = 0;
							UDATA bitsLeftInShapeSlot = slotsPerShapeElement - shift;

							description = *descriptionPtr;
							descriptionPtr += 1;

							while (totalAmountLeft > 0) {
								shape[slotOffset] |= (1 & description) << (slotsPerShapeElement - bitsLeftInShapeSlot);
								description >>= 1;
								positionInDescriptionWord++;

								if (slotsPerShapeElement == positionInDescriptionWord) {
									description = *descriptionPtr;
									descriptionPtr += 1;
									positionInDescriptionWord = 0;
								}

								bitsLeftInShapeSlot--;
								if (0 == bitsLeftInShapeSlot) {
									slotOffset++;
									bitsLeftInShapeSlot = 64;
								}
								totalAmountLeft--;
							}
						}
					}
				} else {
					UDATA bit = (UDATA)1 << ((walkResult->offset % (referenceSize * slotsPerShapeElement)) / referenceSize);
					shape[slotOffset] |= bit;
				}
			} else
#endif /* J9VM_OPT_VALHALLA_VALUE_TYPES */
			{
				UDATA bit = (UDATA)1 << ((walkResult->offset % (referenceSize * slotsPerShapeElement)) / referenceSize);
				shape[slotOffset] |= bit;
#ifdef J9VM_GC_LEAF_BITS
				if (isLeafField(walkResult->field)) {
					leafShape[slotOffset] |= bit;
				} else if (isString) {
					J9UTF8 *fieldName = J9ROMFIELDSHAPE_NAME(walkResult->field);

					if (J9UTF8_LITERAL_EQUALS(J9UTF8_DATA(fieldName), J9UTF8_LENGTH(fieldName), "value")) {
						leafShape[slotOffset] |= bit;
					}
				}
#endif
			}

			walkResult = fieldOffsetsNextDo(walkState);
		}
	}

	if (totalSize < slotsPerShapeElement) {
		/* tag low bit, store in place */
		temp <<= 1;
		temp |= 1;
		ramClass->instanceDescription = (UDATA *)temp;
#ifdef J9VM_GC_LEAF_BITS
		leafTemp <<= 1;
		leafTemp |= 1;
		ramClass->instanceLeafDescription = (UDATA *)leafTemp;
#endif

		Trc_VM_calculateInstanceDescription_taggedResult(NULL, temp);
	} else {
		/* used extra space, store the address */
		ramClass->instanceDescription = storage;
#ifdef J9VM_GC_LEAF_BITS
		ramClass->instanceLeafDescription = storage + shapeSlots;

		Trc_VM_calculateInstanceDescription_indirectResult(NULL, storage[0]);
#endif
	}
}


/* Calculate the lock offset for this class using the following heuristic
	if the superclass has an inheritable lockword (lockOffset != -1) then inherit it
	if the class has a synchronized virtual method and no lockword, allocate a
		lockword as the first instance field of the class
	if the class is java.lang.Class set the lock offset to be the offset of myLockword
		lockOffset for java.lang.Class is set by jclinit::initializeClassLibrary
	if the class is java.lang.Object, then it has a non-inheritable lockword that we allocate here
	Parameters:
		romClass: the rom class to calculate the info for
		ramSuperClass: the parent class which already has an instance description, may be NULL (java.lang.Object)
	Returns:  one of NO_LOCKWORD_NEEDED, LOCKWORD_NEEDED or the offset of the lockword already present in a superclass
*/
UDATA
checkLockwordNeeded(J9JavaVM *vm, J9ROMClass *romClass, J9Class *ramSuperClass, U_32 walkFlags )
{
	J9ROMMethod* romMethod;
	UDATA i;
	J9UTF8 * className = J9ROMCLASS_CLASSNAME(romClass);
	UDATA lockAssignmentAlgorithm = 0;
	J9HashTable* lockwordExceptions= NULL;


	lockwordExceptions = vm->lockwordExceptions;
	lockAssignmentAlgorithm = vm->lockwordMode;

	/* Arrays can't have a monitor */
	if (J9ROMCLASS_IS_ARRAY(romClass)) {
		return NO_LOCKWORD_NEEDED;
	}
#ifdef J9VM_OPT_VALHALLA_VALUE_TYPES
	/* ValueTypes don not have lockwords */
	if (J9ROMCLASS_IS_VALUE(romClass)) {
		return NO_LOCKWORD_NEEDED;
	}
#endif
	
	/* check for primitive types or java.lang.Object */
	if (ramSuperClass == NULL) {
		if (J9ROMCLASS_IS_PRIMITIVE_TYPE(romClass)) {
			/* primitive type (int, char, etc).  Don't alloc lockword */
			return NO_LOCKWORD_NEEDED;
		} else {
			/* java.lang.Object: alloc space for lockword */
			return LOCKWORD_NEEDED;
		}
	}
	
	/* See if we inherit a lockword from our superclass. If direct subclass of java.lang.Object then don't inherit automatically!!! */
	if ((ramSuperClass->lockOffset != -1) && (J9CLASS_DEPTH(ramSuperClass) > 0)) {
		/* we inherit superclass's lockword */
		return ramSuperClass->lockOffset;
	}

	/* check if this class is one of the exceptions */
	if (LOCKNURSERY_ALGORITHM_ALL_INHERIT != lockAssignmentAlgorithm){
		if (lockwordExceptions){
			J9UTF8** entry = (J9UTF8**) hashTableFind(lockwordExceptions,&className);
			if (entry != NULL){
				if (IS_LOCKNURSERY_HASHTABLE_ENTRY_MASKED(*entry)){
					/* we will only get here is the superclass did not already have a lockword */
					return NO_LOCKWORD_NEEDED;
				} else {
					if (ramSuperClass->lockOffset != -1) {
						/* if possible we inherit superclass's lockword, this only kicks in
						 * when this class is a direct subclass of Object, otherwise we will have inherited
						 * it due to the earlier check */
						return ramSuperClass->lockOffset;
					}
					return LOCKWORD_NEEDED;
				}
			}
		}
	}


	/* check if we are creating the ram class for java.lang.Class */
	if (J9UTF8_DATA_EQUALS(J9UTF8_DATA(className),J9UTF8_LENGTH(className),JAVA_LANG_CLASS_CLASSNAME,sizeof(JAVA_LANG_CLASS_CLASSNAME)-1)){
		/* we always want a lockword for classes, make sure we inherit from Object*/
		if (ramSuperClass->lockOffset != -1) {
			/* we inherit superclass's lockword */
			return ramSuperClass->lockOffset;
		}
		return LOCKWORD_NEEDED;
	}

	if (LOCKNURSERY_ALGORITHM_MINIMAL_AND_SYNCHRONIZED_METHODS_AND_INNER_LOCK_CANDIDATES == lockAssignmentAlgorithm){
		/* if this class is a direct subclass of object and is an inner class then give it
		 * a lock word
		 */
		if ((ramSuperClass->lockOffset != -1)&&(J9ROMCLASS_OUTERCLASSNAME(romClass) != NULL)){
			/* if we are one of the classes that gets a lockword just inherit it from Object
			 * if our parent was Object.  If the parent was not Object and had a lockword then the earlier check
			 * will already result in us using the lockword from the parent
			 */
			if (ramSuperClass->lockOffset != -1) {
				/* we inherit superclass's lockword */
				return ramSuperClass->lockOffset;
			}

			return LOCKWORD_NEEDED;
		}
	}

	switch(lockAssignmentAlgorithm){
		case LOCKNURSERY_ALGORITHM_ALL_BUT_ARRAY:
			/* if we are one of the classes that gets a lockword just inherit it from Object
			 * if our parent was Object.  If the parent was not Object and had a lockword then the earlier check
			 * will already result in us using the lockword from the parent
			 */
			if (ramSuperClass->lockOffset != -1) {
				/* we inherit superclass's lockword */
				return ramSuperClass->lockOffset;
			}
			return LOCKWORD_NEEDED;
			break;
		case LOCKNURSERY_ALGORITHM_ALL_INHERIT:
			/* for this option everybody gets a lock and we re-use the offset in java.lang.Object so that
			 * the lockword is always at the same offset from the start of the header. There is an additional
			 * check for this mode in fieldOffsetsStartDo in resolvefield.c makes sure that we do not strip
			 * off the offset from java.lang.Object in this case
			 */

			/* See if we inherit a lockword from our superclass.  In this case we say yes even
			 * if our parent was java.lang.Object as we always want to re-use that one */
			if (ramSuperClass->lockOffset != -1) {
				/* we inherit superclass's lockword */
				return ramSuperClass->lockOffset;
			}

			return LOCKWORD_NEEDED;
			break;
		case LOCKNURSERY_ALGORITHM_MINIMAL_WITH_SYNCHRONIZED_METHODS:
		case LOCKNURSERY_ALGORITHM_MINIMAL_AND_SYNCHRONIZED_METHODS_AND_INNER_LOCK_CANDIDATES:

			/* If this class declares a synchronized virtual method, then allocate a lockword
			 * this was the default in earlier lock nursery attempts
			 */
			romMethod = (J9ROMMethod *)J9ROMCLASS_ROMMETHODS(romClass);
			for (i=0; i < (UDATA) romClass->romMethodCount; i++) {
				if ((romMethod->modifiers & (CFR_ACC_SYNCHRONIZED | CFR_ACC_STATIC)) == CFR_ACC_SYNCHRONIZED) {
					/* found a synchronized virtual method; alloc lockword */
					/* if we are one of the classes that gets a lockword just inherit it from Object
					 * if our parent was Object.  If the parent was not Object and had a lockword then the earlier check
					 * will already result in us using the lockword from the parent
					 */
					if (ramSuperClass->lockOffset != -1) {
						/* we inherit superclass's lockword */
						return ramSuperClass->lockOffset;
					}

					return LOCKWORD_NEEDED;
				}
				romMethod = J9_NEXT_ROM_METHOD(romMethod);
			}

			/* this class does not have a lockword */
			return NO_LOCKWORD_NEEDED;
			break;
		default:
			/* We should never get here due to the checking done in jvmint */
			break;
	}

	/* We should never get here due to the checking done in jvmint */
	return NO_LOCKWORD_NEEDED;
}



