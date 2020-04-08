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

/*
 * Determine if the two valueTypes are substitutable when rhs.class equals lhs.class
 * and rhs and lhs are not null
 *
 * @param[in] lhs the lhs object address
 * @param[in] rhs the rhs object address
 * @param[in] startOffset the initial offset for the object
 * @param[in] clazz the value type class
 * return true if they are substitutable and false otherwise
 */
bool
VM_ValueTypeHelpers::isSubstitutable(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI objectAccessBarrier, j9object_t lhs, j9object_t rhs, UDATA startOffset, J9Class *clazz)
{
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
	J9JavaVM *vm = currentThread->javaVM;
	U_32 walkFlags = J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE;
	J9ROMFieldOffsetWalkState state;
	J9ROMFieldOffsetWalkResult *result = fieldOffsetsStartDo(vm, clazz->romClass, VM_VMHelpers::getSuperclass(clazz), &state, walkFlags, clazz->flattenedClassCache);
	bool rc = true;

	Assert_VM_notNull(lhs);
	Assert_VM_notNull(rhs);
	Assert_VM_true(J9OBJECT_CLAZZ(currentThread, lhs) == J9OBJECT_CLAZZ(currentThread, rhs));

	/* If J9ClassCanSupportFastSubstitutability is set, we can use the barrier version of memcmp,
	 * else we recursively check the fields manually. */
	if (J9_ARE_ALL_BITS_SET(clazz->classFlags, J9ClassCanSupportFastSubstitutability)) {
		rc = objectAccessBarrier.structuralFlattenedCompareObjects(currentThread, clazz, lhs, rhs, startOffset);
	} else {
		while (NULL != result->field) {
			J9UTF8 *signature = J9ROMNAMEANDSIGNATURE_SIGNATURE(&result->field->nameAndSignature);
			U_8 *sigChar = J9UTF8_DATA(signature);

			switch (*sigChar) {
			case 'Z': /* boolean */
			case 'B': /* byte */
			case 'C': /* char */
			case 'I': /* int */
			case 'S': { /* short */
				I_32 lhsValue = objectAccessBarrier.inlineMixedObjectReadI32(currentThread, lhs, startOffset + result->offset);
				I_32 rhsValue = objectAccessBarrier.inlineMixedObjectReadI32(currentThread, rhs, startOffset + result->offset);
				if (lhsValue != rhsValue) {
					rc = false;
					goto done;
				}
				break;
			}
			case 'J': { /* long */
				I_64 lhsValue = objectAccessBarrier.inlineMixedObjectReadI64(currentThread, lhs, startOffset + result->offset);
				I_64 rhsValue = objectAccessBarrier.inlineMixedObjectReadI64(currentThread, rhs, startOffset + result->offset);
				if (lhsValue != rhsValue) {
					rc = false;
					goto done;
				}
				break;
			}
			case 'D': { /* double */
				U_64 lhsValue = objectAccessBarrier.inlineMixedObjectReadU64(currentThread, lhs, startOffset + result->offset);
				U_64 rhsValue = objectAccessBarrier.inlineMixedObjectReadU64(currentThread, rhs, startOffset + result->offset);

				if (!checkDoubleEquality(lhsValue, rhsValue)) {
					rc = false;
					goto done;
				}
				break;
			}
			case 'F': { /* float */
				U_32 lhsValue = objectAccessBarrier.inlineMixedObjectReadU32(currentThread, lhs, startOffset + result->offset);
				U_32 rhsValue = objectAccessBarrier.inlineMixedObjectReadU32(currentThread, rhs, startOffset + result->offset);

				if (!checkFloatEquality(lhsValue, rhsValue)) {
					rc = false;
					goto done;
				}
				break;
			}
			case '[': { /* Array */
				j9object_t lhsObject = objectAccessBarrier.inlineMixedObjectReadObject(currentThread, lhs, startOffset + result->offset);
				j9object_t rhsObject = objectAccessBarrier.inlineMixedObjectReadObject(currentThread, rhs, startOffset + result->offset);
				if (lhsObject != rhsObject) {
					rc = false;
					goto done;
				}
				break;
			}
			case 'L': { /* Nullable class type or interface type */
				j9object_t lhsObject = objectAccessBarrier.inlineMixedObjectReadObject(currentThread, lhs, startOffset + result->offset);
				j9object_t rhsObject = objectAccessBarrier.inlineMixedObjectReadObject(currentThread, rhs, startOffset + result->offset);

				if (!VM_ValueTypeHelpers::acmp(currentThread, objectAccessBarrier, lhsObject, rhsObject)) {
					rc = false;
					goto done;
				}
				break;
			}
			case 'Q': { /* Null-free class type */
				J9Class *fieldClass = findJ9ClassInFlattenedClassCache(clazz->flattenedClassCache, sigChar + 1, J9UTF8_LENGTH(signature) - 2);
				rc = false;

				if (J9_IS_J9CLASS_FLATTENED(fieldClass)) {
					rc = isSubstitutable(currentThread, objectAccessBarrier, lhs, rhs, startOffset + result->offset, fieldClass);
				} else {
					j9object_t lhsFieldObject = objectAccessBarrier.inlineMixedObjectReadObject(currentThread, lhs, startOffset + result->offset);
					j9object_t rhsFieldObject = objectAccessBarrier.inlineMixedObjectReadObject(currentThread, rhs, startOffset + result->offset);

					if (lhsFieldObject == rhsFieldObject) {
						rc = true;
					} else {
						/* When unflattened, we get our object from the specified offset, then increment past the header to the first field. */
						rc = isSubstitutable(currentThread, objectAccessBarrier, lhsFieldObject, rhsFieldObject, J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread), fieldClass);
					}
				}

				if (false == rc) {
					goto done;
				}
				break;
			}
			default:
				Assert_VM_unreachable();
			} /* switch */

			result = fieldOffsetsNextDo(&state);
		}
	}

done:
	return rc;
#else /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
	Assert_VM_unreachable();
	return true;
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
}

extern "C" {
void
defaultValueWithUnflattenedFlattenables(J9VMThread *currentThread, J9Class *clazz, j9object_t instance)
{
        J9FlattenedClassCacheEntry * entry = NULL;
        J9Class * entryClazz = NULL;
        UDATA length = clazz->flattenedClassCache->numberOfEntries;
        UDATA const objectHeaderSize = J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread);
        for (UDATA index = 0; index < length; index++) {
                entry = J9_VM_FCC_ENTRY_FROM_CLASS(clazz, index);
                entryClazz = entry->clazz;
                if (J9_ARE_NO_BITS_SET(J9ClassIsFlattened, entryClazz->classFlags)) {
                        if (entry->offset == UDATA_MAX) {
                                J9Class *definingClass = NULL;
                                J9ROMFieldShape *field = NULL;
                                J9ROMFieldShape *entryField = entry->field;
                                J9UTF8 *name = J9ROMFIELDSHAPE_NAME(entryField);
                                J9UTF8 *signature = J9ROMFIELDSHAPE_SIGNATURE(entryField);
                                entry->offset = instanceFieldOffset(currentThread, clazz, J9UTF8_DATA(name), J9UTF8_LENGTH(name), J9UTF8_DATA(signature), J9UTF8_LENGTH(signature), &definingClass, (UDATA *)&field, 0);
                                Assert_VM_notNull(field);
                        }
                        MM_ObjectAccessBarrierAPI objectAccessBarrier(currentThread);
                        objectAccessBarrier.inlineMixedObjectStoreObject(currentThread, 
                                                                                instance, 
                                                                                entry->offset + objectHeaderSize, 
                                                                                entryClazz->flattenedClassCache->defaultValue, 
                                                                                false);
                }
        }
}

BOOLEAN
valueTypeCapableAcmp(J9VMThread *currentThread, j9object_t lhs, j9object_t rhs)
{
	MM_ObjectAccessBarrierAPI objectAccessBarrier(currentThread);
	return VM_ValueTypeHelpers::acmp(currentThread, objectAccessBarrier, lhs, rhs);
}
} /* extern "C" */
