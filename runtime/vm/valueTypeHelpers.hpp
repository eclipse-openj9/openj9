/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

#if !defined(VALUETYPEHELPERS_HPP_)
#define VALUETYPEHELPERS_HPP_

#include "j9.h"
#include "ObjectAccessBarrierAPI.hpp"
#include "fltconst.h"
class ValueTypeHelpers
{
/*
 * Data members
 */
private:

protected:

public:

/*
 * Function members
 */
private:

protected:

public:
	static VMINLINE bool
	checkDoubleEquality(U_64 a, U_64 b)
	{
		jdouble adouble = *(jdouble*)&a;
		jdouble bdouble = *(jdouble*)&b;
		bool result = false;

		if (adouble == bdouble) {
			result = true;
		} else if (IS_NAN_DBL(adouble) && IS_NAN_DBL(bdouble)) {
			result = true;
		}
		return result;
	}

	static VMINLINE bool 
	checkFloatEquality(U_32 a, U_32 b)
	{
		jfloat afloat = *(jfloat*)&a;
		jfloat bfloat = *(jfloat*)&b;
		bool result = false;

		if (afloat == bfloat) {
			result = true;
		} else if (IS_NAN_SNGL(afloat) && IS_NAN_SNGL(bfloat)) {
			result = true;
		}
		return result;
	}

    /*
    * Determine if the two valueTypes are substitutable when rhs.class equals lhs.class
    *
    * @param[in] lhs the lhs object address
    * @param[in] rhs the rhs object address
    * @param[in] startOffset the initial offset for the object
    * @param[in] clazz the value type class
    * return true if they are substitutable and false otherwise
    */
    static bool
    isSubstitutable(
        J9VMThread *const _currentThread,
        J9JavaVM *const _vm,
        MM_ObjectAccessBarrierAPI *_objectAccessBarrier,
        j9object_t lhs,
        j9object_t rhs,
        UDATA startOffset,
        J9Class *clazz
    ) {
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
		U_32 walkFlags = J9VM_FIELD_OFFSET_WALK_INCLUDE_INSTANCE;
		J9ROMFieldOffsetWalkState state;
		J9ROMFieldOffsetWalkResult *result = fieldOffsetsStartDo(_vm, clazz->romClass, VM_VMHelpers::getSuperclass(clazz), &state, walkFlags, clazz->flattenedClassCache);

		while (NULL != result->field) {
			J9UTF8 *signature = J9ROMNAMEANDSIGNATURE_SIGNATURE(&result->field->nameAndSignature);
			U_8 *sigChar = J9UTF8_DATA(signature);

			switch (*sigChar) {
			case 'Z': /* Boolean */
			case 'B': /* Byte */
			case 'C': /* Char */
			case 'I': /* Int */
			case 'S': { /* Short */ 
				I_32 lhsValue = _objectAccessBarrier->inlineMixedObjectReadI32(_currentThread, lhs, startOffset + result->offset);
				I_32 rhsValue = _objectAccessBarrier->inlineMixedObjectReadI32(_currentThread, rhs, startOffset + result->offset);
				if (lhsValue != rhsValue) {
						return false;
				}
				break;
			}
			case 'J': { /* Long */
				I_64 lhsValue = _objectAccessBarrier->inlineMixedObjectReadI64(_currentThread, lhs, startOffset + result->offset);
				I_64 rhsValue = _objectAccessBarrier->inlineMixedObjectReadI64(_currentThread, rhs, startOffset + result->offset);
				if (lhsValue != rhsValue) {
						return false;
				}
				break;
			}
			case 'D': { /* Double */
				U_64 lhsValue = _objectAccessBarrier->inlineMixedObjectReadU64(_currentThread, lhs, startOffset + result->offset);
				U_64 rhsValue = _objectAccessBarrier->inlineMixedObjectReadU64(_currentThread, rhs, startOffset + result->offset);
				bool result = checkDoubleEquality(lhsValue, rhsValue);
				if (!result) {
					return false;
				}
				break;
			}
			case 'F': { /* Float */
				U_32 lhsValue = _objectAccessBarrier->inlineMixedObjectReadU32(_currentThread, lhs, startOffset + result->offset);
				U_32 rhsValue = _objectAccessBarrier->inlineMixedObjectReadU32(_currentThread, rhs, startOffset + result->offset);
				bool result = checkFloatEquality(lhsValue, rhsValue);
				if (!result) {
					return false;
				}
				break;
			}
			case '[': { /* Array */
				j9object_t lhsObject = _objectAccessBarrier->inlineMixedObjectReadObject(_currentThread, lhs, startOffset + result->offset);
				j9object_t rhsObject = _objectAccessBarrier->inlineMixedObjectReadObject(_currentThread, rhs, startOffset + result->offset);
				if (lhsObject != rhsObject) {
					return false;
				}
				break;
			}
			case 'L': { /* Nullable class type or interface type */
				j9object_t lhsObject = _objectAccessBarrier->inlineMixedObjectReadObject(_currentThread, lhs, startOffset + result->offset);
				j9object_t rhsObject = _objectAccessBarrier->inlineMixedObjectReadObject(_currentThread, rhs, startOffset + result->offset);
				if (!J9_IS_J9CLASS_VALUETYPE(J9OBJECT_CLAZZ(_currentThread, lhsObject))) {
					if (lhsObject != rhsObject) {
						return false;
					}
					break;
				}
				/* Fall through if we find this to be a value type at runtime */
			}
			case 'Q': { /* Null-free class type */
				J9Class *fieldClass = findJ9ClassInFlattenedClassCache(clazz->flattenedClassCache, sigChar + 1, J9UTF8_LENGTH(signature) - 2);
				bool recursiveResult = false;

				if (J9_IS_J9CLASS_FLATTENED(fieldClass)) {
					/* If J9ClassCanSupportFastSubstitutability is set, we can use the barrier version of memcmp, else we recursively check the fields manually. */
					if (J9_ARE_ALL_BITS_SET(fieldClass->classFlags, J9ClassCanSupportFastSubstitutability)) {
						if (!_objectAccessBarrier->structuralFlattenedCompareObjects(_currentThread, fieldClass, lhs, rhs, startOffset + result->offset)) {
							return false;
						}
						break;
					} else {
						recursiveResult = isSubstitutable(_currentThread, _vm, _objectAccessBarrier, lhs, rhs, startOffset + result->offset, fieldClass);
					}
				} else {
					/* When unflattened, we get our object from the specified offset, then increment past the header to the first field. */
					j9object_t lhsFieldObject = _objectAccessBarrier->inlineMixedObjectReadObject(_currentThread, lhs, startOffset + result->offset);
					j9object_t rhsFieldObject = _objectAccessBarrier->inlineMixedObjectReadObject(_currentThread, rhs, startOffset + result->offset);
					if (NULL != lhsFieldObject && NULL != rhsFieldObject) {
						recursiveResult = isSubstitutable(_currentThread, _vm, _objectAccessBarrier, lhsFieldObject, rhsFieldObject, J9VMTHREAD_OBJECT_HEADER_SIZE(_currentThread), fieldClass);
					} else {
						/* Either both lhsFieldObject and rhsFieldObject are null or only one of them is null */
						recursiveResult = (lhsFieldObject == rhsFieldObject);
					} 
				}

				if (!recursiveResult) {
					return false;
				}
				break;
			}
			default:
				Assert_VM_unreachable();
			} /* switch */

			result = fieldOffsetsNextDo(&state);
		}
#endif /* J9VM_OPT_VALHALLA_VALUE_TYPES */

		return true;
    }

};

#endif /* VALUETYPEHELPERS_HPP_ */
