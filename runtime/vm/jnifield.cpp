/*******************************************************************************
 * Copyright IBM Corp. and others 2012
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
#include "ObjectAccessBarrierAPI.hpp"
#include "ValueTypeHelpers.hpp"

extern "C" {

static J9Method*
findFieldContext(J9VMThread *currentThread, IDATA *pcOffset)
{
	IDATA bytecodePCOffset = 0;
	J9Method *method = VM_VMHelpers::findNativeMethodFrame(currentThread)->method;
	if (NULL == method) {
		J9StackWalkState *walkState = currentThread->stackWalkState;
		walkState->walkThread = currentThread;
		walkState->flags = J9_STACKWALK_VISIBLE_ONLY | J9_STACKWALK_INCLUDE_NATIVES | J9_STACKWALK_COUNT_SPECIFIED | J9_STACKWALK_RECORD_BYTECODE_PC_OFFSET;
		walkState->maxFrames = 1;
		walkState->skipCount = 0;
		currentThread->javaVM->walkStackFrames(currentThread, walkState);
		IDATA offset = walkState->bytecodePCOffset;
		if (offset >= 0) {
			bytecodePCOffset = offset;
		}
		method = walkState->method;
	}
	*pcOffset = bytecodePCOffset;
	return method;
}

jboolean JNICALL
getBooleanField(JNIEnv *env, jobject obj, jfieldID fieldID)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	VM_VMAccess::inlineEnterVMFromJNI(currentThread);
	J9JNIFieldID *j9FieldID = (J9JNIFieldID *)fieldID;
	UDATA valueOffset = j9FieldID->offset;

	if (J9_EVENT_IS_HOOKED(vm->hookInterface, J9HOOK_VM_GET_FIELD)) {
		j9object_t object = J9_JNI_UNWRAP_REFERENCE(obj);
		if (J9_ARE_ANY_BITS_SET(J9OBJECT_CLAZZ(currentThread, object)->classFlags, J9ClassHasWatchedFields)) {
			IDATA location = 0;
			J9Method *method = findFieldContext(currentThread, &location);
			if (NULL != method) {
				ALWAYS_TRIGGER_J9HOOK_VM_GET_FIELD(vm->hookInterface, currentThread, method, location, object, j9FieldID->offset);
			}
		}
	}

	j9object_t object = J9_JNI_UNWRAP_REFERENCE(obj);
	valueOffset += J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread);
#if defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS)
	jboolean value = (jboolean)J9OBJECT_U8_LOAD(currentThread, object, valueOffset);
#else /* defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS) */
	jboolean value = (jboolean)J9OBJECT_U32_LOAD(currentThread, object, valueOffset);
#endif /* defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS) */

	if (j9FieldID->field->modifiers & J9AccVolatile) {
		VM_AtomicSupport::readBarrier();
	}

	VM_VMAccess::inlineExitVMToJNI(currentThread);
	return value;
}

jbyte JNICALL
getByteField(JNIEnv *env, jobject obj, jfieldID fieldID)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	VM_VMAccess::inlineEnterVMFromJNI(currentThread);
	J9JNIFieldID *j9FieldID = (J9JNIFieldID *)fieldID;
	UDATA valueOffset = j9FieldID->offset;

	if (J9_EVENT_IS_HOOKED(vm->hookInterface, J9HOOK_VM_GET_FIELD)) {
		j9object_t object = J9_JNI_UNWRAP_REFERENCE(obj);
		if (J9_ARE_ANY_BITS_SET(J9OBJECT_CLAZZ(currentThread, object)->classFlags, J9ClassHasWatchedFields)) {
			IDATA location = 0;
			J9Method *method = findFieldContext(currentThread, &location);
			if (NULL != method) {
				ALWAYS_TRIGGER_J9HOOK_VM_GET_FIELD(vm->hookInterface, currentThread, method, location, object, j9FieldID->offset);
			}
		}
	}

	j9object_t object = J9_JNI_UNWRAP_REFERENCE(obj);
	valueOffset += J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread);
#if defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS)
	jbyte value = (jbyte)J9OBJECT_I8_LOAD(currentThread, object, valueOffset);
#else /* defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS) */
	jbyte value = (jbyte)J9OBJECT_U32_LOAD(currentThread, object, valueOffset);
#endif /* defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS) */

	if (j9FieldID->field->modifiers & J9AccVolatile) {
		VM_AtomicSupport::readBarrier();
	}

	VM_VMAccess::inlineExitVMToJNI(currentThread);
	return value;
}

jchar JNICALL
getCharField(JNIEnv *env, jobject obj, jfieldID fieldID)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	VM_VMAccess::inlineEnterVMFromJNI(currentThread);
	J9JNIFieldID *j9FieldID = (J9JNIFieldID *)fieldID;
	UDATA valueOffset = j9FieldID->offset;

	if (J9_EVENT_IS_HOOKED(vm->hookInterface, J9HOOK_VM_GET_FIELD)) {
		j9object_t object = J9_JNI_UNWRAP_REFERENCE(obj);
		if (J9_ARE_ANY_BITS_SET(J9OBJECT_CLAZZ(currentThread, object)->classFlags, J9ClassHasWatchedFields)) {
			IDATA location = 0;
			J9Method *method = findFieldContext(currentThread, &location);
			if (NULL != method) {
				ALWAYS_TRIGGER_J9HOOK_VM_GET_FIELD(vm->hookInterface, currentThread, method, location, object, j9FieldID->offset);
			}
		}
	}

	j9object_t object = J9_JNI_UNWRAP_REFERENCE(obj);
	valueOffset += J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread);
#if defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS)
	jchar value = (jchar)J9OBJECT_U16_LOAD(currentThread, object, valueOffset);
#else /* defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS) */
	jchar value = (jchar)J9OBJECT_U32_LOAD(currentThread, object, valueOffset);
#endif /* defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS) */

	if (j9FieldID->field->modifiers & J9AccVolatile) {
		VM_AtomicSupport::readBarrier();
	}

	VM_VMAccess::inlineExitVMToJNI(currentThread);
	return value;
}

jshort JNICALL
getShortField(JNIEnv *env, jobject obj, jfieldID fieldID)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	VM_VMAccess::inlineEnterVMFromJNI(currentThread);
	J9JNIFieldID *j9FieldID = (J9JNIFieldID *)fieldID;
	UDATA valueOffset = j9FieldID->offset;

	if (J9_EVENT_IS_HOOKED(vm->hookInterface, J9HOOK_VM_GET_FIELD)) {
		j9object_t object = J9_JNI_UNWRAP_REFERENCE(obj);
		if (J9_ARE_ANY_BITS_SET(J9OBJECT_CLAZZ(currentThread, object)->classFlags, J9ClassHasWatchedFields)) {
			IDATA location = 0;
			J9Method *method = findFieldContext(currentThread, &location);
			if (NULL != method) {
				ALWAYS_TRIGGER_J9HOOK_VM_GET_FIELD(vm->hookInterface, currentThread, method, location, object, j9FieldID->offset);
			}
		}
	}

	j9object_t object = J9_JNI_UNWRAP_REFERENCE(obj);
	valueOffset += J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread);
#if defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS)
	jshort value = (jshort)J9OBJECT_I16_LOAD(currentThread, object, valueOffset);
#else /* defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS) */
	jshort value = (jshort)J9OBJECT_U32_LOAD(currentThread, object, valueOffset);
#endif /* defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS) */

	if (j9FieldID->field->modifiers & J9AccVolatile) {
		VM_AtomicSupport::readBarrier();
	}

	VM_VMAccess::inlineExitVMToJNI(currentThread);
	return value;
}

jint JNICALL
getIntField(JNIEnv *env, jobject obj, jfieldID fieldID)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	VM_VMAccess::inlineEnterVMFromJNI(currentThread);
	J9JNIFieldID *j9FieldID = (J9JNIFieldID *)fieldID;
	UDATA valueOffset = j9FieldID->offset;

	if (J9_EVENT_IS_HOOKED(vm->hookInterface, J9HOOK_VM_GET_FIELD)) {
		j9object_t object = J9_JNI_UNWRAP_REFERENCE(obj);
		if (J9_ARE_ANY_BITS_SET(J9OBJECT_CLAZZ(currentThread, object)->classFlags, J9ClassHasWatchedFields)) {
			IDATA location = 0;
			J9Method *method = findFieldContext(currentThread, &location);
			if (NULL != method) {
				ALWAYS_TRIGGER_J9HOOK_VM_GET_FIELD(vm->hookInterface, currentThread, method, location, object, j9FieldID->offset);
			}
		}
	}

	j9object_t object = J9_JNI_UNWRAP_REFERENCE(obj);
	valueOffset += J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread);
	jint value = J9OBJECT_I32_LOAD(currentThread, object, valueOffset);

	if (j9FieldID->field->modifiers & J9AccVolatile) {
		VM_AtomicSupport::readBarrier();
	}

	VM_VMAccess::inlineExitVMToJNI(currentThread);
	return value;
}

jlong JNICALL
getLongField(JNIEnv *env, jobject obj, jfieldID fieldID)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	VM_VMAccess::inlineEnterVMFromJNI(currentThread);
	J9JNIFieldID *j9FieldID = (J9JNIFieldID *)fieldID;
	UDATA valueOffset = j9FieldID->offset;

	if (J9_EVENT_IS_HOOKED(vm->hookInterface, J9HOOK_VM_GET_FIELD)) {
		j9object_t object = J9_JNI_UNWRAP_REFERENCE(obj);
		if (J9_ARE_ANY_BITS_SET(J9OBJECT_CLAZZ(currentThread, object)->classFlags, J9ClassHasWatchedFields)) {
			IDATA location = 0;
			J9Method *method = findFieldContext(currentThread, &location);
			if (NULL != method) {
				ALWAYS_TRIGGER_J9HOOK_VM_GET_FIELD(vm->hookInterface, currentThread, method, location, object, j9FieldID->offset);
			}
		}
	}

	j9object_t object = J9_JNI_UNWRAP_REFERENCE(obj);
	valueOffset += J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread);
	jlong value = J9OBJECT_I64_LOAD(currentThread, object, valueOffset);

	if (j9FieldID->field->modifiers & J9AccVolatile) {
		VM_AtomicSupport::readBarrier();
	}

	VM_VMAccess::inlineExitVMToJNI(currentThread);
	return value;
}

jfloat JNICALL
getFloatField(JNIEnv *env, jobject obj, jfieldID fieldID)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	VM_VMAccess::inlineEnterVMFromJNI(currentThread);
	J9JNIFieldID *j9FieldID = (J9JNIFieldID *)fieldID;
	UDATA valueOffset = j9FieldID->offset;

	if (J9_EVENT_IS_HOOKED(vm->hookInterface, J9HOOK_VM_GET_FIELD)) {
		j9object_t object = J9_JNI_UNWRAP_REFERENCE(obj);
		if (J9_ARE_ANY_BITS_SET(J9OBJECT_CLAZZ(currentThread, object)->classFlags, J9ClassHasWatchedFields)) {
			IDATA location = 0;
			J9Method *method = findFieldContext(currentThread, &location);
			if (NULL != method) {
				ALWAYS_TRIGGER_J9HOOK_VM_GET_FIELD(vm->hookInterface, currentThread, method, location, object, j9FieldID->offset);
			}
		}
	}

	j9object_t object = J9_JNI_UNWRAP_REFERENCE(obj);
	valueOffset += J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread);
	U_32 uvalue = J9OBJECT_U32_LOAD(currentThread, object, valueOffset);
	jfloat value = *((jfloat *)&uvalue);

	if (j9FieldID->field->modifiers & J9AccVolatile) {
		VM_AtomicSupport::readBarrier();
	}

	VM_VMAccess::inlineExitVMToJNI(currentThread);
	return value;
}

jdouble JNICALL
getDoubleField(JNIEnv *env, jobject obj, jfieldID fieldID)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	VM_VMAccess::inlineEnterVMFromJNI(currentThread);
	J9JNIFieldID *j9FieldID = (J9JNIFieldID *)fieldID;
	UDATA valueOffset = j9FieldID->offset;

	if (J9_EVENT_IS_HOOKED(vm->hookInterface, J9HOOK_VM_GET_FIELD)) {
		j9object_t object = J9_JNI_UNWRAP_REFERENCE(obj);
		if (J9_ARE_ANY_BITS_SET(J9OBJECT_CLAZZ(currentThread, object)->classFlags, J9ClassHasWatchedFields)) {
			IDATA location = 0;
			J9Method *method = findFieldContext(currentThread, &location);
			if (NULL != method) {
				ALWAYS_TRIGGER_J9HOOK_VM_GET_FIELD(vm->hookInterface, currentThread, method, location, object, j9FieldID->offset);
			}
		}
	}

	j9object_t object = J9_JNI_UNWRAP_REFERENCE(obj);
	valueOffset += J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread);
	U_64 uvalue = J9OBJECT_U64_LOAD(currentThread, object, valueOffset);
	jdouble value = *((jdouble *)&uvalue);

	if (j9FieldID->field->modifiers & J9AccVolatile) {
		VM_AtomicSupport::readBarrier();
	}

	VM_VMAccess::inlineExitVMToJNI(currentThread);
	return value;
}

void JNICALL
setByteField(JNIEnv *env, jobject obj, jfieldID fieldID, jbyte value)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	VM_VMAccess::inlineEnterVMFromJNI(currentThread);
	J9JNIFieldID *j9FieldID = (J9JNIFieldID *)fieldID;
	UDATA valueOffset = j9FieldID->offset;

	if (J9_EVENT_IS_HOOKED(vm->hookInterface, J9HOOK_VM_PUT_FIELD)) {
		j9object_t object = J9_JNI_UNWRAP_REFERENCE(obj);
		if (J9_ARE_ANY_BITS_SET(J9OBJECT_CLAZZ(currentThread, object)->classFlags, J9ClassHasWatchedFields)) {
			IDATA location = 0;
			J9Method *method = findFieldContext(currentThread, &location);
			if (NULL != method) {
				U_64 newValue = 0;
#if defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS)
				*(I_8*)&newValue = (I_8)value;
#else /* defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS) */
				*(I_32*)&newValue = (I_32)value;
#endif /* defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS) */
				ALWAYS_TRIGGER_J9HOOK_VM_PUT_FIELD(vm->hookInterface, currentThread, method, location, object, j9FieldID->offset, newValue);
			}
		}
	}

	bool isVolatile = J9_ARE_ANY_BITS_SET(j9FieldID->field->modifiers, J9AccVolatile);
	if (isVolatile) {
		VM_AtomicSupport::writeBarrier();
	}

	j9object_t object = J9_JNI_UNWRAP_REFERENCE(obj);
	valueOffset += J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread);
#if defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS)
	J9OBJECT_I8_STORE(currentThread, object, valueOffset, value);
#else /* defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS) */
	J9OBJECT_U32_STORE(currentThread, object, valueOffset, (U_32)value);
#endif /* defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS) */

	if (isVolatile) {
		VM_AtomicSupport::readWriteBarrier();
	}

	VM_VMAccess::inlineExitVMToJNI(currentThread);
}

void JNICALL
setBooleanField(JNIEnv *env, jobject obj, jfieldID fieldID, jboolean value)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	VM_VMAccess::inlineEnterVMFromJNI(currentThread);
	J9JNIFieldID *j9FieldID = (J9JNIFieldID *)fieldID;
	UDATA valueOffset = j9FieldID->offset;

	value &= 1;

	if (J9_EVENT_IS_HOOKED(vm->hookInterface, J9HOOK_VM_PUT_FIELD)) {
		j9object_t object = J9_JNI_UNWRAP_REFERENCE(obj);
		if (J9_ARE_ANY_BITS_SET(J9OBJECT_CLAZZ(currentThread, object)->classFlags, J9ClassHasWatchedFields)) {
			IDATA location = 0;
			J9Method *method = findFieldContext(currentThread, &location);
			if (NULL != method) {
				U_64 newValue = 0;
#if defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS)
				*(U_8*)&newValue = (U_8)value;
#else /* defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS) */
				*(I_32*)&newValue = (I_32)value;
#endif /* defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS) */
				ALWAYS_TRIGGER_J9HOOK_VM_PUT_FIELD(vm->hookInterface, currentThread, method, location, object, j9FieldID->offset, newValue);
			}
		}
	}

	bool isVolatile = J9_ARE_ANY_BITS_SET(j9FieldID->field->modifiers, J9AccVolatile);
	if (isVolatile) {
		VM_AtomicSupport::writeBarrier();
	}

	j9object_t object = J9_JNI_UNWRAP_REFERENCE(obj);
	valueOffset += J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread);
#if defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS)
	J9OBJECT_U8_STORE(currentThread, object, valueOffset, value);
#else /* defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS) */
	J9OBJECT_U32_STORE(currentThread, object, valueOffset, (U_32)value);
#endif /* defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS) */

	if (isVolatile) {
		VM_AtomicSupport::readWriteBarrier();
	}

	VM_VMAccess::inlineExitVMToJNI(currentThread);
}

void JNICALL
setCharField(JNIEnv *env, jobject obj, jfieldID fieldID, jchar value)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	VM_VMAccess::inlineEnterVMFromJNI(currentThread);
	J9JNIFieldID *j9FieldID = (J9JNIFieldID *)fieldID;
	UDATA valueOffset = j9FieldID->offset;

	if (J9_EVENT_IS_HOOKED(vm->hookInterface, J9HOOK_VM_PUT_FIELD)) {
		j9object_t object = J9_JNI_UNWRAP_REFERENCE(obj);
		if (J9_ARE_ANY_BITS_SET(J9OBJECT_CLAZZ(currentThread, object)->classFlags, J9ClassHasWatchedFields)) {
			IDATA location = 0;
			J9Method *method = findFieldContext(currentThread, &location);
			if (NULL != method) {
				U_64 newValue = 0;
#if defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS)
				*(U_16*)&newValue = (U_16)value;
#else /* defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS) */
				*(I_32*)&newValue = (I_32)value;
#endif /* defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS) */
				ALWAYS_TRIGGER_J9HOOK_VM_PUT_FIELD(vm->hookInterface, currentThread, method, location, object, j9FieldID->offset, newValue);
			}
		}
	}

	bool isVolatile = J9_ARE_ANY_BITS_SET(j9FieldID->field->modifiers, J9AccVolatile);
	if (isVolatile) {
		VM_AtomicSupport::writeBarrier();
	}

	j9object_t object = J9_JNI_UNWRAP_REFERENCE(obj);
	valueOffset += J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread);
#if defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS)
	J9OBJECT_U16_STORE(currentThread, object, valueOffset, value);
#else /* defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS) */
	J9OBJECT_U32_STORE(currentThread, object, valueOffset, (U_32)value);
#endif /* defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS) */

	if (isVolatile) {
		VM_AtomicSupport::readWriteBarrier();
	}

	VM_VMAccess::inlineExitVMToJNI(currentThread);
}

void JNICALL
setShortField(JNIEnv *env, jobject obj, jfieldID fieldID, jshort value)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	VM_VMAccess::inlineEnterVMFromJNI(currentThread);
	J9JNIFieldID *j9FieldID = (J9JNIFieldID *)fieldID;
	UDATA valueOffset = j9FieldID->offset;

	if (J9_EVENT_IS_HOOKED(vm->hookInterface, J9HOOK_VM_PUT_FIELD)) {
		j9object_t object = J9_JNI_UNWRAP_REFERENCE(obj);
		if (J9_ARE_ANY_BITS_SET(J9OBJECT_CLAZZ(currentThread, object)->classFlags, J9ClassHasWatchedFields)) {
			IDATA location = 0;
			J9Method *method = findFieldContext(currentThread, &location);
			if (NULL != method) {
				U_64 newValue = 0;
#if defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS)
				*(I_16*)&newValue = (I_16)value;
#else /* defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS) */
				*(I_32*)&newValue = (I_32)value;
#endif /* defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS) */
				ALWAYS_TRIGGER_J9HOOK_VM_PUT_FIELD(vm->hookInterface, currentThread, method, location, object, j9FieldID->offset, newValue);
			}
		}
	}

	bool isVolatile = J9_ARE_ANY_BITS_SET(j9FieldID->field->modifiers, J9AccVolatile);
	if (isVolatile) {
		VM_AtomicSupport::writeBarrier();
	}

	j9object_t object = J9_JNI_UNWRAP_REFERENCE(obj);
	valueOffset += J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread);
#if defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS)
	J9OBJECT_I16_STORE(currentThread, object, valueOffset, value);
#else /* defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS) */
	J9OBJECT_U32_STORE(currentThread, object, valueOffset, (U_32)value);
#endif /* defined(J9VM_OPT_VALHALLA_COMPACT_LAYOUTS) */

	if (isVolatile) {
		VM_AtomicSupport::readWriteBarrier();
	}

	VM_VMAccess::inlineExitVMToJNI(currentThread);
}

void JNICALL
setIntField(JNIEnv *env, jobject obj, jfieldID fieldID, jint value)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	VM_VMAccess::inlineEnterVMFromJNI(currentThread);
	J9JNIFieldID *j9FieldID = (J9JNIFieldID *)fieldID;
	UDATA valueOffset = j9FieldID->offset;

	if (J9_EVENT_IS_HOOKED(vm->hookInterface, J9HOOK_VM_PUT_FIELD)) {
		j9object_t object = J9_JNI_UNWRAP_REFERENCE(obj);
		if (J9_ARE_ANY_BITS_SET(J9OBJECT_CLAZZ(currentThread, object)->classFlags, J9ClassHasWatchedFields)) {
			IDATA location = 0;
			J9Method *method = findFieldContext(currentThread, &location);
			if (NULL != method) {
				U_64 newValue = 0;
				*(I_32*)&newValue = (I_32)value;
				ALWAYS_TRIGGER_J9HOOK_VM_PUT_FIELD(vm->hookInterface, currentThread, method, location, object, j9FieldID->offset, newValue);
			}
		}
	}

	bool isVolatile = J9_ARE_ANY_BITS_SET(j9FieldID->field->modifiers, J9AccVolatile);
	if (isVolatile) {
		VM_AtomicSupport::writeBarrier();
	}

	j9object_t object = J9_JNI_UNWRAP_REFERENCE(obj);
	valueOffset += J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread);
	J9OBJECT_I32_STORE(currentThread, object, valueOffset, value);

	if (isVolatile) {
		VM_AtomicSupport::readWriteBarrier();
	}

	VM_VMAccess::inlineExitVMToJNI(currentThread);
}

void JNICALL
setLongField(JNIEnv *env, jobject obj, jfieldID fieldID, jlong value)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	VM_VMAccess::inlineEnterVMFromJNI(currentThread);
	J9JNIFieldID *j9FieldID = (J9JNIFieldID *)fieldID;
	UDATA valueOffset = j9FieldID->offset;

	if (J9_EVENT_IS_HOOKED(vm->hookInterface, J9HOOK_VM_PUT_FIELD)) {
		j9object_t object = J9_JNI_UNWRAP_REFERENCE(obj);
		if (J9_ARE_ANY_BITS_SET(J9OBJECT_CLAZZ(currentThread, object)->classFlags, J9ClassHasWatchedFields)) {
			IDATA location = 0;
			J9Method *method = findFieldContext(currentThread, &location);
			if (NULL != method) {
				U_64 newValue = (U_64)value;
				ALWAYS_TRIGGER_J9HOOK_VM_PUT_FIELD(vm->hookInterface, currentThread, method, location, object, j9FieldID->offset, newValue);
			}
		}
	}

	bool isVolatile = J9_ARE_ANY_BITS_SET(j9FieldID->field->modifiers, J9AccVolatile);
	if (isVolatile) {
		VM_AtomicSupport::writeBarrier();
	}

	j9object_t object = J9_JNI_UNWRAP_REFERENCE(obj);
	valueOffset += J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread);
	J9OBJECT_I64_STORE(currentThread, object, valueOffset, value);

	if (isVolatile) {
		VM_AtomicSupport::readWriteBarrier();
	}

	VM_VMAccess::inlineExitVMToJNI(currentThread);
}

void JNICALL
setFloatField(JNIEnv *env, jobject obj, jfieldID fieldID, jfloat value)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	VM_VMAccess::inlineEnterVMFromJNI(currentThread);
	J9JNIFieldID *j9FieldID = (J9JNIFieldID *)fieldID;
	UDATA valueOffset = j9FieldID->offset;

	if (J9_EVENT_IS_HOOKED(vm->hookInterface, J9HOOK_VM_PUT_FIELD)) {
		j9object_t object = J9_JNI_UNWRAP_REFERENCE(obj);
		if (J9_ARE_ANY_BITS_SET(J9OBJECT_CLAZZ(currentThread, object)->classFlags, J9ClassHasWatchedFields)) {
			IDATA location = 0;
			J9Method *method = findFieldContext(currentThread, &location);
			if (NULL != method) {
				U_64 newValue = 0;
				*(jfloat*)&newValue = value;
				ALWAYS_TRIGGER_J9HOOK_VM_PUT_FIELD(vm->hookInterface, currentThread, method, location, object, j9FieldID->offset, newValue);
			}
		}
	}

	bool isVolatile = J9_ARE_ANY_BITS_SET(j9FieldID->field->modifiers, J9AccVolatile);
	if (isVolatile) {
		VM_AtomicSupport::writeBarrier();
	}

	j9object_t object = J9_JNI_UNWRAP_REFERENCE(obj);
	valueOffset += J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread);
	J9OBJECT_U32_STORE(currentThread, object, valueOffset, *((U_32 *)&value));

	if (isVolatile) {
		VM_AtomicSupport::readWriteBarrier();
	}

	VM_VMAccess::inlineExitVMToJNI(currentThread);
}

void JNICALL
setDoubleField(JNIEnv *env, jobject obj, jfieldID fieldID, jdouble value)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	VM_VMAccess::inlineEnterVMFromJNI(currentThread);
	J9JNIFieldID *j9FieldID = (J9JNIFieldID *)fieldID;
	UDATA valueOffset = j9FieldID->offset;


	if (J9_EVENT_IS_HOOKED(vm->hookInterface, J9HOOK_VM_PUT_FIELD)) {
		j9object_t object = J9_JNI_UNWRAP_REFERENCE(obj);
		if (J9_ARE_ANY_BITS_SET(J9OBJECT_CLAZZ(currentThread, object)->classFlags, J9ClassHasWatchedFields)) {
			IDATA location = 0;
			J9Method *method = findFieldContext(currentThread, &location);
			if (NULL != method) {
				U_64 newValue = 0;
				*(jdouble*)&newValue = value;
				ALWAYS_TRIGGER_J9HOOK_VM_PUT_FIELD(vm->hookInterface, currentThread, method, location, object, j9FieldID->offset, newValue);
			}
		}
	}

	bool isVolatile = J9_ARE_ANY_BITS_SET(j9FieldID->field->modifiers, J9AccVolatile);
	if (isVolatile) {
		VM_AtomicSupport::writeBarrier();
	}

	j9object_t object = J9_JNI_UNWRAP_REFERENCE(obj);
	valueOffset += J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread);
	J9OBJECT_U64_STORE(currentThread, object, valueOffset, *((U_64 *)&value));

	if (isVolatile) {
		VM_AtomicSupport::readWriteBarrier();
	}

	VM_VMAccess::inlineExitVMToJNI(currentThread);
}

jobject JNICALL
getObjectField(JNIEnv *env, jobject obj, jfieldID fieldID)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	VM_VMAccess::inlineEnterVMFromJNI(currentThread);
	J9JNIFieldID *j9FieldID = (J9JNIFieldID *)fieldID;
	UDATA valueOffset = j9FieldID->offset;

	if (J9_EVENT_IS_HOOKED(vm->hookInterface, J9HOOK_VM_GET_FIELD)) {
		j9object_t object = J9_JNI_UNWRAP_REFERENCE(obj);
		if (J9_ARE_ANY_BITS_SET(J9OBJECT_CLAZZ(currentThread, object)->classFlags, J9ClassHasWatchedFields)) {
			IDATA location = 0;
			J9Method *method = findFieldContext(currentThread, &location);
			if (NULL != method) {
				ALWAYS_TRIGGER_J9HOOK_VM_GET_FIELD(vm->hookInterface, currentThread, method, location, object, j9FieldID->offset);
			}
		}
	}

	j9object_t object = J9_JNI_UNWRAP_REFERENCE(obj);
	valueOffset += J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread);
	j9object_t value = NULL;

#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
	J9ROMFieldShape *field = j9FieldID->field;
	J9Class *definingClass = j9FieldID->declaringClass;
	bool fieldIsFlattened = false;

	if (J9ROMFIELD_IS_NULL_RESTRICTED(field) && (NULL != definingClass->flattenedClassCache) && J9_ARE_NO_BITS_SET(field->modifiers, J9AccStatic)) {
		J9Class *fieldClass = getFlattenableFieldType(definingClass, field);
		if ((NULL != fieldClass) && J9_IS_FIELD_FLATTENED(fieldClass, field)) {
			fieldIsFlattened = true;
			/* Allocate value object and copy flattened field data into it */
			MM_ObjectAccessBarrierAPI objectAccessBarrier(currentThread);
			MM_ObjectAllocationAPI objectAllocate(currentThread);
			/* Try fast path first */
			value = VM_ValueTypeHelpers::getFlattenedFieldAtOffset(
					currentThread,
					objectAccessBarrier,
					objectAllocate,
					fieldClass,
					object,
					valueOffset,
					true);

			/* If fast path failed, fall back to slow path (allows GC) */
			if (NULL == value) {
				value = VM_ValueTypeHelpers::getFlattenedFieldAtOffset(
						currentThread,
						objectAccessBarrier,
						objectAllocate,
						fieldClass,
						object,
						valueOffset,
						false);
			}
		}
	}

	/* If field is not flattened, read as regular object pointer */
	if (!fieldIsFlattened)
#endif /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */
	{
		/* Regular object reference field - read the pointer */
		object = J9_JNI_UNWRAP_REFERENCE(obj);
		value = J9OBJECT_OBJECT_LOAD(currentThread, object, valueOffset);
	}

	if (j9FieldID->field->modifiers & J9AccVolatile) {
		VM_AtomicSupport::readBarrier();
	}

	jobject valueRef = VM_VMHelpers::createLocalRef(env, value);
	VM_VMAccess::inlineExitVMToJNI(currentThread);
	return valueRef;
}

void JNICALL
setObjectField(JNIEnv *env, jobject obj, jfieldID fieldID, jobject valueRef)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	VM_VMAccess::inlineEnterVMFromJNI(currentThread);
	J9JNIFieldID *j9FieldID = (J9JNIFieldID *)fieldID;
	UDATA valueOffset = j9FieldID->offset;

	if (J9_EVENT_IS_HOOKED(vm->hookInterface, J9HOOK_VM_PUT_FIELD)) {
		j9object_t object = J9_JNI_UNWRAP_REFERENCE(obj);
		if (J9_ARE_ANY_BITS_SET(J9OBJECT_CLAZZ(currentThread, object)->classFlags, J9ClassHasWatchedFields)) {
			IDATA location = 0;
			J9Method *method = findFieldContext(currentThread, &location);
			if (NULL != method) {
				U_64 newValue = 0;
				if (NULL != valueRef) {
					*(j9object_t*)&newValue = J9_JNI_UNWRAP_REFERENCE(valueRef);
				}
				ALWAYS_TRIGGER_J9HOOK_VM_PUT_FIELD(vm->hookInterface, currentThread, method, location, object, j9FieldID->offset, newValue);
			}
		}
	}

	bool isVolatile = J9_ARE_ANY_BITS_SET(j9FieldID->field->modifiers, J9AccVolatile);
	if (isVolatile) {
		VM_AtomicSupport::writeBarrier();
	}

	j9object_t object = J9_JNI_UNWRAP_REFERENCE(obj);
	j9object_t value = (NULL == valueRef) ? NULL : J9_JNI_UNWRAP_REFERENCE(valueRef);
	valueOffset += J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread);

#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
	J9Class *definingClass = j9FieldID->declaringClass;
	bool fieldIsFlattened = false;
	J9ROMFieldShape *field = j9FieldID->field;

	if (J9ROMFIELD_IS_NULL_RESTRICTED(field)) {
		/* Check if field is null-restricted and value is NULL */
		if (NULL == value) {
			setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
			VM_VMAccess::inlineExitVMToJNI(currentThread);
			return;
		}

		if ((NULL != definingClass->flattenedClassCache) && J9_ARE_NO_BITS_SET(field->modifiers, J9AccStatic)) {
			J9Class *fieldClass = getFlattenableFieldType(definingClass, field);
			if ((NULL != fieldClass) && J9_IS_FIELD_FLATTENED(fieldClass, field)) {
				fieldIsFlattened = true;
				/* Copy value object data into flattened field */
				MM_ObjectAccessBarrierAPI objectAccessBarrier(currentThread);
				VM_ValueTypeHelpers::putFlattenedFieldAtOffset(
						currentThread,
						objectAccessBarrier,
						fieldClass,
						value,
						object,
						valueOffset);
			}
		}
	}

	/* If field is not flattened, store as regular object pointer */
	if (!fieldIsFlattened)
#endif /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */
	{
		/* Regular object reference field - store the pointer */
		J9OBJECT_OBJECT_STORE(currentThread, object, valueOffset, value);
	}

	if (isVolatile) {
		VM_AtomicSupport::readWriteBarrier();
	}

	VM_VMAccess::inlineExitVMToJNI(currentThread);
}

jint JNICALL
getStaticIntField(JNIEnv *env, jclass clazz, jfieldID fieldID)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	VM_VMAccess::inlineEnterVMFromJNI(currentThread);
	J9JNIFieldID *id = (J9JNIFieldID*)fieldID;
	J9Class *declaringClass = id->declaringClass;
	UDATA offset = id->offset;
	J9ROMFieldShape *field = id->field;
	U_32 modifiers = field->modifiers;
	void *valueAddress = (void*)((UDATA)declaringClass->ramStatics + offset);
	if (J9_EVENT_IS_HOOKED(vm->hookInterface, J9HOOK_VM_GET_STATIC_FIELD)) {
		if (J9_ARE_ANY_BITS_SET(declaringClass->classFlags, J9ClassHasWatchedFields)) {
			IDATA location = 0;
			J9Method *method = findFieldContext(currentThread, &location);
			if (NULL != method) {
				ALWAYS_TRIGGER_J9HOOK_VM_GET_STATIC_FIELD(vm->hookInterface, currentThread, method, location, declaringClass, valueAddress);
			}
		}
	}
	bool isVolatile = J9_ARE_ANY_BITS_SET(modifiers, J9AccVolatile);
	MM_ObjectAccessBarrierAPI objectAccessBarrier(currentThread);
	jint result = (jint)objectAccessBarrier.inlineStaticReadU32(currentThread, declaringClass, (U_32*)valueAddress, isVolatile);
	VM_VMAccess::inlineExitVMToJNI(currentThread);
	return result;
}

void JNICALL
setStaticIntField(JNIEnv *env, jclass clazz, jfieldID fieldID, jint value)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	VM_VMAccess::inlineEnterVMFromJNI(currentThread);
	J9JNIFieldID *id = (J9JNIFieldID*)fieldID;
	J9Class *declaringClass = id->declaringClass;
	UDATA offset = id->offset;
	J9ROMFieldShape *field = id->field;
	U_32 modifiers = field->modifiers;
	void *valueAddress = (void*)((UDATA)declaringClass->ramStatics + offset);
	if (J9_EVENT_IS_HOOKED(vm->hookInterface, J9HOOK_VM_PUT_STATIC_FIELD)) {
		if (J9_ARE_ANY_BITS_SET(declaringClass->classFlags, J9ClassHasWatchedFields)) {
			IDATA location = 0;
			J9Method *method = findFieldContext(currentThread, &location);
			if (NULL != method) {
				U_64 newValue = 0;
				*(I_32*)&newValue = (I_32)value;
				ALWAYS_TRIGGER_J9HOOK_VM_PUT_STATIC_FIELD(vm->hookInterface, currentThread, method, location, declaringClass, valueAddress, newValue);
			}
		}
	}

	if (J9_ARE_ANY_BITS_SET(modifiers, J9AccFinal)) {
		VM_VMHelpers::reportFinalFieldModified(currentThread, declaringClass);
	}

	bool isVolatile = J9_ARE_ANY_BITS_SET(modifiers, J9AccVolatile);
	MM_ObjectAccessBarrierAPI objectAccessBarrier(currentThread);
	objectAccessBarrier.inlineStaticStoreU32(currentThread, declaringClass, (U_32*)valueAddress, (U_32)value, isVolatile);
	VM_VMAccess::inlineExitVMToJNI(currentThread);
}

void JNICALL
setStaticFloatField(JNIEnv *env, jclass cls, jfieldID fieldID, jfloat value)
{
	setStaticIntField(env, cls, fieldID, *(jint*)&value);
}

jlong JNICALL
getStaticLongField(JNIEnv *env, jclass clazz, jfieldID fieldID)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	VM_VMAccess::inlineEnterVMFromJNI(currentThread);
	J9JNIFieldID *id = (J9JNIFieldID*)fieldID;
	J9Class *declaringClass = id->declaringClass;
	UDATA offset = id->offset;
	J9ROMFieldShape *field = id->field;
	U_32 modifiers = field->modifiers;
	void *valueAddress = (void*)((UDATA)declaringClass->ramStatics + offset);
	if (J9_EVENT_IS_HOOKED(vm->hookInterface, J9HOOK_VM_GET_STATIC_FIELD)) {
		if (J9_ARE_ANY_BITS_SET(declaringClass->classFlags, J9ClassHasWatchedFields)) {
			IDATA location = 0;
			J9Method *method = findFieldContext(currentThread, &location);
			if (NULL != method) {
				ALWAYS_TRIGGER_J9HOOK_VM_GET_STATIC_FIELD(vm->hookInterface, currentThread, method, location, declaringClass, valueAddress);
			}
		}
	}
	bool isVolatile = J9_ARE_ANY_BITS_SET(modifiers, J9AccVolatile);
	MM_ObjectAccessBarrierAPI objectAccessBarrier(currentThread);
	jlong result = (jlong)objectAccessBarrier.inlineStaticReadU64(currentThread, declaringClass, (U_64*)valueAddress, isVolatile);
	VM_VMAccess::inlineExitVMToJNI(currentThread);
	return result;
}

static void JNICALL
setStaticDoubleFieldIndirect(JNIEnv *env, jobject clazz, jfieldID fieldID, void *value)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	VM_VMAccess::inlineEnterVMFromJNI(currentThread);
	J9JNIFieldID *id = (J9JNIFieldID*)fieldID;
	J9Class *declaringClass = id->declaringClass;
	UDATA offset = id->offset;
	J9ROMFieldShape *field = id->field;
	U_32 modifiers = field->modifiers;
	void *valueAddress = (void*)((UDATA)declaringClass->ramStatics + offset);
	if (J9_EVENT_IS_HOOKED(vm->hookInterface, J9HOOK_VM_PUT_STATIC_FIELD)) {
		if (J9_ARE_ANY_BITS_SET(declaringClass->classFlags, J9ClassHasWatchedFields)) {
			IDATA location = 0;
			J9Method *method = findFieldContext(currentThread, &location);
			if (NULL != method) {
				ALWAYS_TRIGGER_J9HOOK_VM_PUT_STATIC_FIELD(vm->hookInterface, currentThread, method, location, declaringClass, valueAddress, *(U_64*)value);
			}
		}
	}

	if (J9_ARE_ANY_BITS_SET(modifiers, J9AccFinal)) {
		VM_VMHelpers::reportFinalFieldModified(currentThread, declaringClass);
	}

	bool isVolatile = J9_ARE_ANY_BITS_SET(modifiers, J9AccVolatile);
	MM_ObjectAccessBarrierAPI objectAccessBarrier(currentThread);
	objectAccessBarrier.inlineStaticStoreU64(currentThread, declaringClass, (U_64*)valueAddress, *(U_64*)value, isVolatile);
	VM_VMAccess::inlineExitVMToJNI(currentThread);
}

void JNICALL
setStaticDoubleField(JNIEnv *env, jclass cls, jfieldID fieldID, jdouble value)
{
	setStaticDoubleFieldIndirect(env, cls, fieldID, &value);
}

void JNICALL
setStaticLongField(JNIEnv *env, jclass cls, jfieldID fieldID, jlong value)
{
	setStaticDoubleFieldIndirect(env, cls, fieldID, &value);
}

jobject JNICALL
getStaticObjectField(JNIEnv *env, jclass clazz, jfieldID fieldID)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	VM_VMAccess::inlineEnterVMFromJNI(currentThread);
	J9JNIFieldID *id = (J9JNIFieldID*)fieldID;
	J9Class *declaringClass = id->declaringClass;
	UDATA offset = id->offset;
	J9ROMFieldShape *field = id->field;
	U_32 modifiers = field->modifiers;
	void *valueAddress = (void*)((UDATA)declaringClass->ramStatics + offset);
	if (J9_EVENT_IS_HOOKED(vm->hookInterface, J9HOOK_VM_GET_STATIC_FIELD)) {
		if (J9_ARE_ANY_BITS_SET(declaringClass->classFlags, J9ClassHasWatchedFields)) {
			IDATA location = 0;
			J9Method *method = findFieldContext(currentThread, &location);
			if (NULL != method) {
				ALWAYS_TRIGGER_J9HOOK_VM_GET_STATIC_FIELD(vm->hookInterface, currentThread, method, location, declaringClass, valueAddress);
			}
		}
	}
	bool isVolatile = J9_ARE_ANY_BITS_SET(modifiers, J9AccVolatile);
	MM_ObjectAccessBarrierAPI objectAccessBarrier(currentThread);
	j9object_t resultObject = objectAccessBarrier.inlineStaticReadObject(currentThread, declaringClass, (j9object_t*)valueAddress, isVolatile);
	jobject result = VM_VMHelpers::createLocalRef(env, resultObject);
	VM_VMAccess::inlineExitVMToJNI(currentThread);
	return result;
}

void JNICALL
setStaticObjectField(JNIEnv *env, jclass clazz, jfieldID fieldID, jobject value)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	VM_VMAccess::inlineEnterVMFromJNI(currentThread);
	J9JNIFieldID *id = (J9JNIFieldID*)fieldID;
	J9Class *declaringClass = id->declaringClass;
	UDATA offset = id->offset;
	J9ROMFieldShape *field = id->field;
	U_32 modifiers = field->modifiers;
	void *valueAddress = (void*)((UDATA)declaringClass->ramStatics + offset);
	if (J9_EVENT_IS_HOOKED(vm->hookInterface, J9HOOK_VM_PUT_STATIC_FIELD)) {
		if (J9_ARE_ANY_BITS_SET(declaringClass->classFlags, J9ClassHasWatchedFields)) {
			IDATA location = 0;
			J9Method *method = findFieldContext(currentThread, &location);
			if (NULL != method) {
				U_64 newValue = 0;
				if (NULL != value) {
					*(j9object_t*)&newValue = J9_JNI_UNWRAP_REFERENCE(value);
				}
				ALWAYS_TRIGGER_J9HOOK_VM_PUT_STATIC_FIELD(vm->hookInterface, currentThread, method, location, declaringClass, valueAddress, newValue);
			}
		}
	}

	if (J9_ARE_ANY_BITS_SET(modifiers, J9AccFinal)) {
		VM_VMHelpers::reportFinalFieldModified(currentThread, declaringClass);
	}

	bool isVolatile = J9_ARE_ANY_BITS_SET(modifiers, J9AccVolatile);
	MM_ObjectAccessBarrierAPI objectAccessBarrier(currentThread);
	j9object_t valueObject = (NULL == value) ? NULL : J9_JNI_UNWRAP_REFERENCE(value);
	objectAccessBarrier.inlineStaticStoreObject(currentThread, declaringClass, (j9object_t*)valueAddress, valueObject, isVolatile);
	VM_VMAccess::inlineExitVMToJNI(currentThread);
}

jfloat JNICALL
getStaticFloatField(JNIEnv *env, jclass clazz, jfieldID fieldID)
{
	jint value = getStaticIntField(env, clazz, fieldID);
	return *(jfloat*)&value;
}

jdouble JNICALL
getStaticDoubleField(JNIEnv *env, jclass clazz, jfieldID fieldID)
{
	jlong value = getStaticLongField(env, clazz, fieldID);
	return *(jdouble*)&value;
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
	J9VMThread *currentThread = (J9VMThread *)env;
	jobject valueRef = NULL;
	j9object_t value = NULL;
	j9array_t array = NULL;
	jsize arrayBound = -1;

	VM_VMAccess::inlineEnterVMFromJNI(currentThread);

	array = (j9array_t)J9_JNI_UNWRAP_REFERENCE(arrayRef);

	arrayBound = J9INDEXABLEOBJECT_SIZE(currentThread, array);
	/* unsigned comparison includes index < 0 case */
	if ((UDATA)arrayBound <= (UDATA)index) {
		setArrayIndexOutOfBoundsException(currentThread, index);
		VM_VMAccess::inlineExitVMToJNI(currentThread);
		return valueRef;
	}

	MM_ObjectAccessBarrierAPI objectAccessBarrier(currentThread);
	MM_ObjectAllocationAPI objectAllocate(currentThread);
	/* Use helper that handles both flattened and non-flattened arrays */
	/* Try fast path first */
	value = VM_ValueTypeHelpers::loadFlattenableArrayElement(
			currentThread,
			objectAccessBarrier,
			objectAllocate,
			(j9object_t)array,
			index,
			true);

	/* If fast path failed, fall back to slow path (allows GC) */
	if (NULL == value) {
		value = VM_ValueTypeHelpers::loadFlattenableArrayElement(
				currentThread,
				objectAccessBarrier,
				objectAllocate,
				(j9object_t)array,
				index,
				false);
	}

	valueRef = VM_VMHelpers::createLocalRef(env, value);

	VM_VMAccess::inlineExitVMToJNI(currentThread);
	return valueRef;
}

void JNICALL
setObjectArrayElement(JNIEnv *env, jobjectArray arrayRef, jsize index, jobject valueRef)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	j9object_t value = NULL;
	j9array_t array = NULL;
	jsize arrayBound = -1;

	VM_VMAccess::inlineEnterVMFromJNI(currentThread);

	array = (j9array_t)J9_JNI_UNWRAP_REFERENCE(arrayRef);

	arrayBound = J9INDEXABLEOBJECT_SIZE(currentThread, array);
	if ((UDATA)arrayBound <= (UDATA)index) {
		setArrayIndexOutOfBoundsException(currentThread, index);
		VM_VMAccess::inlineExitVMToJNI(currentThread);
		return;
	}

	/* A NULL value is ok */
	value = (NULL == valueRef)? NULL : (j9object_t)J9_JNI_UNWRAP_REFERENCE(valueRef);

	/* type check */
	if (NULL != value) {
		if (0 ==
			instanceOfOrCheckCast(J9OBJECT_CLAZZ(currentThread, value),
					((J9ArrayClass *)J9OBJECT_CLAZZ(currentThread, array))->componentType)
		) {
			setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGARRAYSTOREEXCEPTION, NULL);
			VM_VMAccess::inlineExitVMToJNI(currentThread);
			return;
		}
	}

#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
	J9Class *arrayClass = J9OBJECT_CLAZZ(currentThread, (j9object_t)array);
	/* Check if array is null-restricted and value is NULL */
	if (J9_IS_J9ARRAYCLASS_NULL_RESTRICTED(arrayClass) && (NULL == value)) {
		setCurrentException(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
		VM_VMAccess::inlineExitVMToJNI(currentThread);
		return;
	}
#endif /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */

	MM_ObjectAccessBarrierAPI objectAccessBarrier(currentThread);
	/* Use helper that handles both flattened and non-flattened arrays */
	VM_ValueTypeHelpers::storeFlattenableArrayElement(
			currentThread,
			objectAccessBarrier,
			(j9object_t)array,
			index,
			value);

	VM_VMAccess::inlineExitVMToJNI(currentThread);
}

} /* extern "C" */
