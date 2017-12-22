/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
#include "BytecodeAction.hpp"
#include "VMHelpers.hpp"
#include "ArrayCopyHelpers.hpp"

#undef FAST_JNI_ARRAYCOPY

extern "C" {

/* java.lang.System: public static native long currentTimeMillis(); */
jlong JNICALL
Fast_java_lang_System_currentTimeMillis(J9VMThread *currentThread)
{
	PORT_ACCESS_FROM_VMC(currentThread);
	return j9time_current_time_millis();
}

/* java.lang.System: public static native long nanoTime(); */
jlong JNICALL
Fast_java_lang_System_nanoTime(J9VMThread *currentThread)
{
	PORT_ACCESS_FROM_VMC(currentThread);
	return j9time_nano_time();
}

#if defined(FAST_JNI_ARRAYCOPY)

static VMINLINE VM_BytecodeAction
doReferenceArrayCopy(J9VMThread *currentThread, j9object_t srcObject, I_32 srcStart, j9object_t destObject, I_32 destStart, I_32 elementCount, I_32 *aiobIndex)
{
	VM_BytecodeAction rc = EXECUTE_BYTECODE;

	if (elementCount < 0) {
		*aiobIndex = elementCount;
		rc = THROW_AIOB;
	} else {
		I_32 value = VM_ArrayCopyHelpers::rangeCheck(currentThread, srcObject, srcStart, elementCount);
		if (0 != value) {
			*aiobIndex = value;
			rc = THROW_AIOB;
		} else {
			value = VM_ArrayCopyHelpers::rangeCheck(currentThread, destObject, destStart, elementCount);
			if (0 != value) {
				*aiobIndex = value;
				rc = THROW_AIOB;
			} else {
				value = VM_ArrayCopyHelpers::referenceArrayCopy(currentThread, srcObject, srcStart, destObject, destStart, elementCount);
				if (-1 != value) {
					rc = THROW_ARRAY_STORE;
				}
			}
		}
	}

	return rc;
}

static VMINLINE VM_BytecodeAction
doPrimitiveArrayCopy(J9VMThread *currentThread, j9object_t srcObject, I_32 srcStart, j9object_t destObject, I_32 destStart, U_32 arrayShape, I_32 elementCount, I_32 *aiobIndex)
{
	VM_BytecodeAction rc = EXECUTE_BYTECODE;
	/* Primitive array */
	if (elementCount < 0) {
		*aiobIndex = elementCount;
		rc = THROW_AIOB;
	} else {
		I_32 value = VM_ArrayCopyHelpers::rangeCheck(currentThread, srcObject, srcStart, elementCount);
		if (0 != value) {
			*aiobIndex = value;
			rc = THROW_AIOB;
		} else {
			value = VM_ArrayCopyHelpers::rangeCheck(currentThread, destObject, destStart, elementCount);
			if (0 != value) {
				*aiobIndex = value;
				rc = THROW_AIOB;
			} else {
				if (elementCount > 0) {
					VM_ArrayCopyHelpers::primitiveArrayCopy(currentThread, srcObject, srcStart, destObject, destStart, elementCount, arrayShape);
				}
			}
		}
	}
	return rc;
}

/* java.lang.System: public static native void arraycopy(Object src, int srcPos, Object dest, int destPos, int length); */
void JNICALL
Fast_java_lang_System_arraycopy(J9VMThread *currentThread, j9object_t src, jint srcPos, j9object_t dest, jint destPos, jint length)
{
	if ((NULL == src) || (NULL == dest)) {
		setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		J9Class *srcClass = J9OBJECT_CLAZZ(currentThread, src);
		J9ROMClass *srcRom = srcClass->romClass;
		if (J9ROMCLASS_IS_ARRAY(srcRom)) {
			J9Class *destClass = J9OBJECT_CLAZZ(currentThread, dest);
			J9ROMClass *destRom = destClass->romClass;
			if (!J9ROMCLASS_IS_ARRAY(destRom)) {
				goto throwArrayStore;
			} else {
				I_32 aiobIndex = 0;
				if (OBJECT_HEADER_SHAPE_POINTERS == J9CLASS_SHAPE(srcClass)) {
					/* Reference array */
					if (OBJECT_HEADER_SHAPE_POINTERS != J9CLASS_SHAPE(destClass)) {
						goto throwArrayStore;
					} else {
						VM_BytecodeAction rc = doReferenceArrayCopy(currentThread, src, srcPos, dest, destPos, length, &aiobIndex);
						switch(rc) {
						case THROW_AIOB:
							goto throwAIOB;
						case THROW_ARRAY_STORE:
							goto throwArrayStore;
						}
					}
				} else {
					if (srcClass != destClass) {
						goto throwArrayStore;
					} else {
						VM_BytecodeAction rc = doPrimitiveArrayCopy(currentThread, src, srcPos, dest, destPos, ((J9ROMArrayClass*)srcClass->romClass)->arrayShape & 0x0000FFFF, length, &aiobIndex);
						if (rc == THROW_AIOB) {
throwAIOB:
							setArrayIndexOutOfBoundsException(currentThread, aiobIndex);
						}
					}
				}
			}
		} else {
throwArrayStore:
			setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGARRAYSTOREEXCEPTION, NULL);
		}
	}
}
#endif /* FAST_JNI_ARRAYCOPY */

J9_FAST_JNI_METHOD_TABLE(java_lang_System)
	J9_FAST_JNI_METHOD("currentTimeMillis", "()J", Fast_java_lang_System_currentTimeMillis,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_NOT_GC_POINT | J9_FAST_JNI_NO_NATIVE_METHOD_FRAME | J9_FAST_JNI_NO_EXCEPTION_THROW |
		J9_FAST_JNI_NO_SPECIAL_TEAR_DOWN | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
	J9_FAST_JNI_METHOD("nanoTime", "()J", Fast_java_lang_System_nanoTime,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_NOT_GC_POINT | J9_FAST_JNI_NO_NATIVE_METHOD_FRAME | J9_FAST_JNI_NO_EXCEPTION_THROW |
		J9_FAST_JNI_NO_SPECIAL_TEAR_DOWN | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
#if defined(FAST_JNI_ARRAYCOPY)
	J9_FAST_JNI_METHOD("arraycopy", "(Ljava/lang/Object;ILjava/lang/Object;II)V", Fast_java_lang_System_arraycopy,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS | J9_FAST_JNI_DO_NOT_PASS_RECEIVER)
#endif /* FAST_JNI_ARRAYCOPY */
J9_FAST_JNI_METHOD_TABLE_END

}
