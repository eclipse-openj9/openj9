/*******************************************************************************
 * Copyright (c) 2012, 2018 IBM Corp. and others
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

#include "jnifield.h"

#include "j9accessbarrier.h"
#include "j9comp.h"
#include "omrgcconsts.h"
#include "j9vmnls.h"
#include "ut_j9vm.h"
#include "util_api.h"
#include "vm_api.h"
#include "vm_internal.h"

#include "AtomicSupport.hpp"
#include "VMAccess.hpp"

class BooleanField {
public:

	static jboolean VMINLINE readUnpacked(J9VMThread *vmThread, j9object_t object, UDATA valueOffset)
	{
		return (jboolean)J9OBJECT_U32_LOAD(vmThread, object, valueOffset);
	}
	static void VMINLINE writeUnpacked(J9VMThread *vmThread, j9object_t object, UDATA valueOffset, jboolean value)
	{
		J9OBJECT_U32_STORE(vmThread, object, valueOffset, (U_32)value);
	}
};


class ByteField {
public:

	static jbyte VMINLINE readUnpacked(J9VMThread *vmThread, j9object_t object, UDATA valueOffset)
	{
		return (jbyte)J9OBJECT_U32_LOAD(vmThread, object, valueOffset);
	}
	static void VMINLINE writeUnpacked(J9VMThread *vmThread, j9object_t object, UDATA valueOffset, jbyte value)
	{
		J9OBJECT_U32_STORE(vmThread, object, valueOffset, (U_32)value);
	}
};

class ShortField {
public:

	static jshort VMINLINE readUnpacked(J9VMThread *vmThread, j9object_t object, UDATA valueOffset)
	{
		return (jshort)J9OBJECT_U32_LOAD(vmThread, object, valueOffset);
	}
	static void VMINLINE writeUnpacked(J9VMThread *vmThread, j9object_t object, UDATA valueOffset, jshort value)
	{
		J9OBJECT_U32_STORE(vmThread, object, valueOffset, (U_32)value);
	}
};

class CharField {
public:

	static jchar VMINLINE readUnpacked(J9VMThread *vmThread, j9object_t object, UDATA valueOffset)
	{
		return (jchar)J9OBJECT_U32_LOAD(vmThread, object, valueOffset);
	}
	static void VMINLINE writeUnpacked(J9VMThread *vmThread, j9object_t object, UDATA valueOffset, jchar value)
	{
		J9OBJECT_U32_STORE(vmThread, object, valueOffset, (U_32)value);
	}
};

class IntField {
public:

	static jint VMINLINE readUnpacked(J9VMThread *vmThread, j9object_t object, UDATA valueOffset)
	{
		return J9OBJECT_I32_LOAD(vmThread, object, valueOffset);
	}
	static void VMINLINE writeUnpacked(J9VMThread *vmThread, j9object_t object, UDATA valueOffset, jint value)
	{
		J9OBJECT_I32_STORE(vmThread, object, valueOffset, value);
	}
};

class LongField {
public:

	static jlong VMINLINE readUnpacked(J9VMThread *vmThread, j9object_t object, UDATA valueOffset)
	{
		return J9OBJECT_I64_LOAD(vmThread, object, valueOffset);
	}
	static void VMINLINE writeUnpacked(J9VMThread *vmThread, j9object_t object, UDATA valueOffset, jlong value)
	{
		J9OBJECT_I64_STORE(vmThread, object, valueOffset, value);
	}
};

#ifdef J9VM_INTERP_FLOAT_SUPPORT
class FloatField {
public:

	static jfloat VMINLINE readUnpacked(J9VMThread *vmThread, j9object_t object, UDATA valueOffset)
	{
		U_32 value = J9OBJECT_U32_LOAD(vmThread, object, valueOffset);
		return *((jfloat *)&value);
	}
	static void VMINLINE writeUnpacked(J9VMThread *vmThread, j9object_t object, UDATA valueOffset, jfloat value)
	{
		J9OBJECT_U32_STORE(vmThread, object, valueOffset, *((U_32 *)&value));
	}
};
#endif /* J9VM_INTERP_FLOAT_SUPPORT */

#ifdef J9VM_INTERP_FLOAT_SUPPORT
class DoubleField {
public:

	static jdouble VMINLINE readUnpacked(J9VMThread *vmThread, j9object_t object, UDATA valueOffset)
	{
		U_64 value = J9OBJECT_U64_LOAD(vmThread, object, valueOffset);
		return *((jdouble *)&value);
	}

	static void VMINLINE writeUnpacked(J9VMThread *vmThread, j9object_t object, UDATA valueOffset, jdouble value)
	{
		J9OBJECT_U64_STORE(vmThread, object, valueOffset, *((U_64 *)&value));
	}
};
#endif /* J9VM_INTERP_FLOAT_SUPPORT */

class FieldEvents {
private:
	static void VMINLINE initWalkState(J9VMThread *vmThread, J9StackWalkState *ws)
	{
		ws->walkThread = vmThread;
		ws->flags = J9_STACKWALK_VISIBLE_ONLY | J9_STACKWALK_INCLUDE_NATIVES | J9_STACKWALK_COUNT_SPECIFIED;
		ws->maxFrames = 1;
		ws->skipCount = 0;
	}
public:
	static void triggerGetEvents(J9VMThread *vmThread, J9JNIFieldID *j9FieldID, j9object_t object)
	{
		J9JavaVM *vm = vmThread->javaVM;
	
		if (J9_EVENT_IS_HOOKED(vm->hookInterface, J9HOOK_VM_GET_FIELD)) {
			if (J9_ARE_ANY_BITS_SET(J9OBJECT_CLAZZ(currentThread, object)->classFlags, J9ClassHasWatchedFields)) {
				J9StackWalkState *walkState = vmThread->stackWalkState;

				initWalkState(vmThread, walkState);
				vmThread->javaVM->walkStackFrames(vmThread, walkState);

				if (NULL != walkState->method) {
					ALWAYS_TRIGGER_J9HOOK_VM_GET_FIELD(vm->hookInterface, vmThread, walkState->method, 0, &object, j9FieldID->offset);
				}
			}
		}
	}

	static void triggerSetEvents(J9VMThread *vmThread, J9JNIFieldID *j9FieldID, j9object_t object, void *pvalue)
	{
		J9JavaVM *vm = vmThread->javaVM;

		if (J9_EVENT_IS_HOOKED(vm->hookInterface, J9HOOK_VM_PUT_FIELD)) {
			if (J9_ARE_ANY_BITS_SET(J9OBJECT_CLAZZ(currentThread, object)->classFlags, J9ClassHasWatchedFields)) {
				J9StackWalkState *walkState = vmThread->stackWalkState;

				initWalkState(vmThread, walkState);

				vmThread->javaVM->walkStackFrames(vmThread, walkState);

				if (NULL != walkState->method) {
					ALWAYS_TRIGGER_J9HOOK_VM_PUT_FIELD(vm->hookInterface, vmThread, walkState->method, 0, &object, j9FieldID->offset, pvalue);
				}
			}
		}

	}
};

/**
 * Get primitive field
 */
template <class FieldAccess, class ValueType>
static ValueType VMINLINE
getPrimitiveField(JNIEnv *env, jobject obj, jfieldID fieldID)
{
	J9VMThread *vmThread = (J9VMThread *)env;
	J9JNIFieldID *j9FieldID = (J9JNIFieldID *)fieldID;
	UDATA valueOffset = j9FieldID->offset;
	j9object_t object = NULL;
	ValueType value = 0;

	VM_VMAccess::inlineEnterVMFromJNI(vmThread);


	object = J9_JNI_UNWRAP_REFERENCE(obj);

	{
		valueOffset += J9_OBJECT_HEADER_SIZE;
		value = FieldAccess::readUnpacked(vmThread, object, valueOffset);
	}

	if (j9FieldID->field->modifiers & J9AccVolatile) {
		VM_AtomicSupport::readBarrier();
	}

	FieldEvents::triggerGetEvents(vmThread, j9FieldID, object);
	VM_VMAccess::inlineExitVMToJNI(vmThread);
	return value;
}

jboolean JNICALL
getBooleanField(JNIEnv *env, jobject obj, jfieldID fieldID)
{
	return getPrimitiveField<BooleanField, jboolean>(env, obj, fieldID);
}

jbyte JNICALL
getByteField(JNIEnv *env, jobject obj, jfieldID fieldID)
{
	return getPrimitiveField<ByteField, jbyte>(env, obj, fieldID);
}

jchar JNICALL
getCharField(JNIEnv *env, jobject obj, jfieldID fieldID)
{
	return getPrimitiveField<CharField, jchar>(env, obj, fieldID);
}

jshort JNICALL
getShortField(JNIEnv *env, jobject obj, jfieldID fieldID)
{
	return getPrimitiveField<ShortField, jshort>(env, obj, fieldID);
}

jint JNICALL
getIntField(JNIEnv *env, jobject obj, jfieldID fieldID)
{
	return getPrimitiveField<IntField, jint>(env, obj, fieldID);
}

jlong JNICALL
getLongField(JNIEnv *env, jobject obj, jfieldID fieldID)
{
	return getPrimitiveField<LongField, jlong>(env, obj, fieldID);
}

#if defined(J9VM_INTERP_FLOAT_SUPPORT)
jfloat JNICALL
getFloatField(JNIEnv *env, jobject obj, jfieldID fieldID)
{
	return getPrimitiveField<FloatField, jfloat>(env, obj, fieldID);
}
#endif /* J9VM_INTERP_FLOAT_SUPPORT */

#if defined(J9VM_INTERP_FLOAT_SUPPORT)
jdouble JNICALL
getDoubleField(JNIEnv *env, jobject obj, jfieldID fieldID)
{
	return getPrimitiveField<DoubleField, jdouble>(env, obj, fieldID);
}
#endif /* J9VM_INTERP_FLOAT_SUPPORT */


/**
 * Put into a primitive field
 */
template <class FieldAccess, class ValueType>
static void VMINLINE
putPrimitiveField(JNIEnv *env, jobject obj, jfieldID fieldID, ValueType value)
{
	J9VMThread *vmThread = (J9VMThread *)env;
	J9JNIFieldID *j9FieldID = (J9JNIFieldID *)fieldID;
	UDATA valueOffset = j9FieldID->offset;
	j9object_t object = NULL;

	VM_VMAccess::inlineEnterVMFromJNI(vmThread);

	object = J9_JNI_UNWRAP_REFERENCE(obj);

	if (j9FieldID->field->modifiers & J9AccVolatile) {
		VM_AtomicSupport::writeBarrier();
	}

	{
		valueOffset += J9_OBJECT_HEADER_SIZE;
		FieldAccess::writeUnpacked(vmThread, object, valueOffset, value);
	}

	if (j9FieldID->field->modifiers & J9AccVolatile) {
		VM_AtomicSupport::readWriteBarrier();
	}

	FieldEvents::triggerSetEvents(vmThread, j9FieldID, object, &value);
	VM_VMAccess::inlineExitVMToJNI(vmThread);
}

void JNICALL
setByteField(JNIEnv *env, jobject obj, jfieldID fieldID, jbyte value)
{
	putPrimitiveField<ByteField, jbyte>(env, obj, fieldID, value);
}

void JNICALL
setBooleanField(JNIEnv *env, jobject obj, jfieldID fieldID, jboolean value)
{
	putPrimitiveField<BooleanField, jboolean>(env, obj, fieldID, value);
}

void JNICALL
setCharField(JNIEnv *env, jobject obj, jfieldID fieldID, jchar value)
{
	putPrimitiveField<CharField, jchar>(env, obj, fieldID, value);
}

void JNICALL
setShortField(JNIEnv *env, jobject obj, jfieldID fieldID, jshort value)
{
	putPrimitiveField<ShortField, jshort>(env, obj, fieldID, value);
}

void JNICALL
setIntField(JNIEnv *env, jobject obj, jfieldID fieldID, jint value)
{
	putPrimitiveField<IntField, jint>(env, obj, fieldID, value);
}

void JNICALL
setLongField(JNIEnv *env, jobject obj, jfieldID fieldID, jlong value)
{
	putPrimitiveField<LongField, jlong>(env, obj, fieldID, value);
}

#if defined(J9VM_INTERP_FLOAT_SUPPORT)
void JNICALL
setFloatField(JNIEnv *env, jobject obj, jfieldID fieldID, jfloat value)
{
	putPrimitiveField<FloatField, jfloat>(env, obj, fieldID, value);
}
#endif /* J9VM_INTERP_FLOAT_SUPPORT */

#if defined(J9VM_INTERP_FLOAT_SUPPORT)
void JNICALL
setDoubleField(JNIEnv *env, jobject obj, jfieldID fieldID, jdouble value)
{
	putPrimitiveField<DoubleField, jdouble>(env, obj, fieldID, value);
}
#endif /* J9VM_INTERP_FLOAT_SUPPORT */

/*
 * These differ from the primitive field set/get by:
 * - return values need to be wrapped by local references
 * - setObjectField fails on a nested packed field (but not a p-ref field)
 */
jobject JNICALL
getObjectField(JNIEnv *env, jobject obj, jfieldID fieldID)
{
	J9VMThread *vmThread = (J9VMThread *)env;
	J9JNIFieldID *j9FieldID = (J9JNIFieldID *)fieldID;
	UDATA valueOffset = j9FieldID->offset;
	j9object_t object = NULL;
	jobject valueRef = NULL;
	j9object_t value = NULL;

	VM_VMAccess::inlineEnterVMFromJNI(vmThread);

	object = J9_JNI_UNWRAP_REFERENCE(obj);

	{
		valueOffset += J9_OBJECT_HEADER_SIZE;
		value = J9OBJECT_OBJECT_LOAD(vmThread, object, valueOffset);
	}

	if (j9FieldID->field->modifiers & J9AccVolatile) {
		VM_AtomicSupport::readBarrier();
	}

	valueRef = VM_VMHelpers::createLocalRef(env, value);

	FieldEvents::triggerGetEvents(vmThread, j9FieldID, object);
	VM_VMAccess::inlineExitVMToJNI(vmThread);
	return valueRef;
}

/**
 * We can set a p-ref field, but not a nested packed field.
 */
void JNICALL
setObjectField(JNIEnv *env, jobject obj, jfieldID fieldID, jobject valueRef)
{
	J9VMThread *vmThread = (J9VMThread *)env;
	J9JNIFieldID *j9FieldID = (J9JNIFieldID *)fieldID;
	UDATA valueOffset = j9FieldID->offset;
	j9object_t object = NULL;
	j9object_t value = NULL;

	VM_VMAccess::inlineEnterVMFromJNI(vmThread);

	object = J9_JNI_UNWRAP_REFERENCE(obj);

	/* A NULL value is ok */
	value = (NULL == valueRef)? NULL : (j9object_t)J9_JNI_UNWRAP_REFERENCE(valueRef);

	if (j9FieldID->field->modifiers & J9AccVolatile) {
		VM_AtomicSupport::writeBarrier();
	}

	{
		valueOffset += J9_OBJECT_HEADER_SIZE;
		J9OBJECT_OBJECT_STORE(vmThread, object, valueOffset, value);
	}

	if (j9FieldID->field->modifiers & J9AccVolatile) {
		VM_AtomicSupport::readWriteBarrier();
	}

	FieldEvents::triggerSetEvents(vmThread, j9FieldID, object, &value);
	VM_VMAccess::inlineExitVMToJNI(vmThread);
}

/*
 * These differ from the object field set/get by:
 * - array bounds checks
 * - array store type checks (there is no comparable type check in SetObjectField())
 * - no need to worry whether an array element is volatile
 */
jobject JNICALL
getObjectArrayElement(JNIEnv *env, jobjectArray arrayRef, jsize index)
{
	J9VMThread *vmThread = (J9VMThread *)env;
	jobject valueRef = NULL;
	j9object_t value = NULL;
	j9array_t array = NULL;
	jsize arrayBound = -1;

	VM_VMAccess::inlineEnterVMFromJNI(vmThread);

	array = (j9array_t)J9_JNI_UNWRAP_REFERENCE(arrayRef);

	arrayBound = J9INDEXABLEOBJECT_SIZE(vmThread, array);
	/* unsigned comparison includes index < 0 case */
	if ((UDATA)arrayBound <= (UDATA)index) {
		setArrayIndexOutOfBoundsException(vmThread, index);
		goto done;
	}

	value = J9JAVAARRAYOFOBJECT_LOAD(vmThread, array, index);
	valueRef = VM_VMHelpers::createLocalRef(env, value);

done:
	VM_VMAccess::inlineExitVMToJNI(vmThread);
	return valueRef;
}

void JNICALL
setObjectArrayElement(JNIEnv *env, jobjectArray arrayRef, jsize index, jobject valueRef)
{
	J9VMThread *vmThread = (J9VMThread *)env;
	j9object_t value = NULL;
	j9array_t array = NULL;
	jsize arrayBound = -1;

	VM_VMAccess::inlineEnterVMFromJNI(vmThread);

	array = (j9array_t)J9_JNI_UNWRAP_REFERENCE(arrayRef);

	arrayBound = J9INDEXABLEOBJECT_SIZE(vmThread, array);
	if ((UDATA)arrayBound <= (UDATA)index) {
		setArrayIndexOutOfBoundsException(vmThread, index);
		goto done;
	}

	/* A NULL value is ok */
	value = (NULL == valueRef)? NULL : (j9object_t)J9_JNI_UNWRAP_REFERENCE(valueRef);

	/* type check */
	if (NULL != value) {
		if (0 ==
			instanceOfOrCheckCast(J9OBJECT_CLAZZ(vmThread, value),
					((J9ArrayClass *)J9OBJECT_CLAZZ(vmThread, array))->componentType)
		) {
			setCurrentException(vmThread, J9VMCONSTANTPOOL_JAVALANGARRAYSTOREEXCEPTION, NULL);
			goto done;
		}
	}

	J9JAVAARRAYOFOBJECT_STORE(vmThread, array, index, value);

done:
	VM_VMAccess::inlineExitVMToJNI(vmThread);
}
