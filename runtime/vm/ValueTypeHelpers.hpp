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

#if !defined(VALUETYPEHELPERS_HPP_)
#define VALUETYPEHELPERS_HPP_

#include "j9.h"
#include "fltconst.h"

#include "ObjectAccessBarrierAPI.hpp"
#include "VMHelpers.hpp"

#include "objectreferencesmacros_undefine.inc"

class VM_ValueTypeHelpers {
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
	static bool
	isSubstitutable(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI objectAccessBarrier, j9object_t lhs, j9object_t rhs, UDATA startOffset, J9Class *clazz);

	static VMINLINE bool
	checkDoubleEquality(U_64 a, U_64 b)
	{
		bool result = false;

		if (a == b) {
			result = true;
		} else if (IS_NAN_DBL(*(jdouble*)&a) && IS_NAN_DBL(*(jdouble*)&b)) {
			result = true;
		}
		return result;
	}

	static VMINLINE bool
	checkFloatEquality(U_32 a, U_32 b)
	{
		bool result = false;

		if (a == b) {
			result = true;
		} else if (IS_NAN_SNGL(*(jfloat*)&a) && IS_NAN_SNGL(*(jfloat*)&b)) {
			result = true;
		}
		return result;
	}
protected:

public:
	static VMINLINE bool
	acmp(J9VMThread *currentThread, MM_ObjectAccessBarrierAPI objectAccessBarrier, j9object_t lhs, j9object_t rhs)
	{
		bool acmpResult = (rhs == lhs);
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
		if (!acmpResult) {
			if ((NULL != rhs) && (NULL != lhs)) {
				J9Class * lhsClass = J9OBJECT_CLAZZ(_currentThread, lhs);
				J9Class * rhsClass = J9OBJECT_CLAZZ(_currentThread, rhs);
				if (J9_IS_J9CLASS_VALUETYPE(lhsClass)
					&& (rhsClass == lhsClass)
				) {
					acmpResult = VM_ValueTypeHelpers::isSubstitutable(currentThread, objectAccessBarrier, lhs, rhs, J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread), lhsClass);
				}
			}
		}
#endif /* J9VM_OPT_VALHALLA_VALUE_TYPES */
		return acmpResult;
	}

};

#include "objectreferencesmacros_define.inc"

#endif /* VALUETYPEHELPERS_HPP_ */
