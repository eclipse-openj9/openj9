/*******************************************************************************
 * Copyright (c) 2001, 2016 IBM Corp. and others
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

#include "fastJNI.h"

#include "j9protos.h"
#include "j9consts.h"
#include "VMHelpers.hpp"
#include "ObjectAllocationAPI.hpp"
#include "ut_j9vm.h"

extern "C" {

/* java.lang.reflect.Array: private static native Object newArrayImpl(Class componentType, int dimension); */
j9object_t JNICALL
Fast_java_lang_reflect_Array_newArrayImpl(J9VMThread *currentThread, j9object_t clazz, jint dimension)
{
	MM_ObjectAllocationAPI objectAllocate(currentThread);
	j9object_t array = NULL;
	J9Class *arrayClass = NULL;
	J9Class *componentType = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, clazz);

	if (J9ROMCLASS_IS_ARRAY(componentType->romClass) && ((((J9ArrayClass *)componentType)->arity + 1) > J9_ARRAY_DIMENSION_LIMIT)) {
		/* The spec says to throw this exception if the number of dimensions is greater than J9_ARRAY_DIMENSION_LIMIT */
		setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
		goto done;
	}

	arrayClass = componentType->arrayClass;
	if (NULL == arrayClass) {
		J9ROMImageHeader *romHeader = currentThread->javaVM->arrayROMClasses;
		Assert_VM_false(J9ROMCLASS_IS_PRIMITIVE_TYPE(componentType->romClass));
		arrayClass = internalCreateArrayClass(currentThread, (J9ROMArrayClass*)J9ROMIMAGEHEADER_FIRSTCLASS(romHeader), componentType);
		if (VM_VMHelpers::exceptionPending(currentThread)) {
			goto done;
		}
	}

	array = VM_VMHelpers::allocateIndexableObject(currentThread, &objectAllocate, arrayClass, dimension, true, false, false);
done:
	return array;
}

J9_FAST_JNI_METHOD_TABLE(java_lang_reflect_Array)
	J9_FAST_JNI_METHOD("newArrayImpl", "(Ljava/lang/Class;I)Ljava/lang/Object;", Fast_java_lang_reflect_Array_newArrayImpl,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
J9_FAST_JNI_METHOD_TABLE_END

}
