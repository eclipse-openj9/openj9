/*******************************************************************************
 * Copyright IBM Corp. and others 2021
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
#include "jclprots.h"
#include "j9cp.h"
#include "j9protos.h"
#include "ut_j9jcl.h"
#include "j9port.h"
#include "jclglob.h"
#include "jcl_internal.h"
#include "util_api.h"
#include "j9vmconstantpool.h"
#include "ObjectAccessBarrierAPI.hpp"
#include "objhelp.h"

#include <string.h>
#include <assert.h>

#include "VMHelpers.hpp"

extern "C" {

#if defined(J9VM_OPT_OPENJDK_METHODHANDLE)

/* Constants mapped from java.lang.invoke.MethodHandleNatives$Constants
 * These constants are validated by the MethodHandleNatives$Constants.verifyConstants()
 * method when Java assertions are enabled
 */

#define MN_SEARCH_SUPERCLASSES	0x00100000
#define MN_SEARCH_INTERFACES	0x00200000

#if JAVA_SPEC_VERSION >= 16
#define MN_MODULE_MODE			0x00000010
#define MN_UNCONDITIONAL_MODE	0x00000020
#define MN_TRUSTED_MODE			-1
#endif /* JAVA_SPEC_VERSION >= 16 */

/* PlaceHolder value used for MN.vmindex that has default method conflict */
#define J9VM_RESOLVED_VMINDEX_FOR_DEFAULT_THROW 1

J9_DECLARE_CONSTANT_UTF8(mutableCallSite, "java/lang/invoke/MutableCallSite");

static bool
isPolymorphicMHMethod(J9JavaVM *vm, J9Class *declaringClass, J9UTF8 *methodName)
{
	if (declaringClass == J9VMJAVALANGINVOKEMETHODHANDLE(vm)) {
		U_8 *nameData = J9UTF8_DATA(methodName);
		U_16 nameLength = J9UTF8_LENGTH(methodName);

		if (J9UTF8_LITERAL_EQUALS(nameData, nameLength, "invoke")
		|| J9UTF8_LITERAL_EQUALS(nameData, nameLength, "invokeBasic")
		|| J9UTF8_LITERAL_EQUALS(nameData, nameLength, "linkToVirtual")
		|| J9UTF8_LITERAL_EQUALS(nameData, nameLength, "linkToStatic")
		|| J9UTF8_LITERAL_EQUALS(nameData, nameLength, "linkToSpecial")
		|| J9UTF8_LITERAL_EQUALS(nameData, nameLength, "linkToInterface")
		|| J9UTF8_LITERAL_EQUALS(nameData, nameLength, "linkToNative")
		) {
			return true;
		}
	}

	return false;
}

/**
 * @brief Add a MemberName to the list of MemberNames for the J9Class that will
 * correspond to its clazz field.
 *
 * This must be done immediately prior to initializing vmtarget.
 *
 * clazzObject must be the value of the MemberName's clazz field, or the value
 * that will be assigned to clazz immediately upon success.
 *
 * On error, the current exception will be set:
 * - to OutOfMemoryError for allocation failure.
 *
 * The caller must have VM access.
 *
 * @param[in] currentThread the J9VMThread of the current thread
 * @param[in] memberNameObject the MemberName object to add to the list
 * @param[in] clazzObject the value of memberNameObject.clazz
 * @return true for success, or false on error
 */
static bool
addMemberNameToClass(J9VMThread *currentThread, j9object_t memberNameObject, j9object_t clazzObject)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	J9Class *j9clazz = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, clazzObject);

	jobject weakRef = vmFuncs->j9jni_createGlobalRef((JNIEnv*)currentThread, memberNameObject, JNI_TRUE);

	omrthread_monitor_enter(vm->memberNameListsMutex);

	if (J9_ARE_ALL_BITS_SET(j9clazz->classFlags, J9ClassNeedToPruneMemberNames)) {
		VM_AtomicSupport::bitAndU32((volatile uint32_t*)&j9clazz->classFlags, ~(uint32_t)J9ClassNeedToPruneMemberNames);

		/* Remove all entries of memberNames for which the JNI weak ref has been cleared. */
		J9MemberNameListNode **cur = &j9clazz->memberNames;
		while (NULL != *cur) {
			j9object_t obj = J9_JNI_UNWRAP_REFERENCE((*cur)->memberName);
			if (NULL == obj) {
				/* The MemberName has been collected. Remove this entry. */
				J9MemberNameListNode *next = (*cur)->next;
				vmFuncs->j9jni_deleteGlobalRef((JNIEnv*)currentThread, (*cur)->memberName, JNI_TRUE);
				pool_removeElement(vm->memberNameListNodePool, *cur);
				*cur = next;
			} else {
				cur = &(*cur)->next;
			}
		}
	}

	J9MemberNameListNode *node = (J9MemberNameListNode *)pool_newElement(vm->memberNameListNodePool);

	bool success = false;
	if ((NULL != weakRef) && (NULL != node)) {
		/* Initialize node and push it onto the front of the list. */
		node->memberName = weakRef;
		node->next = j9clazz->memberNames;
		j9clazz->memberNames = node;
		success = true;
	} else {
		/* Failed to allocate either weakRef or node. */
		if (NULL != node) {
			pool_removeElement(vm->memberNameListNodePool, node);
		}
		if (NULL != weakRef) {
			vmFuncs->j9jni_deleteGlobalRef((JNIEnv*)currentThread, weakRef, JNI_TRUE);
		}
		vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
	}

	omrthread_monitor_exit(vm->memberNameListsMutex);
	return success;
}

/* Private MemberName object init helper
 *
 * Set the MemberName fields based on the refObject given:
 * For j.l.reflect.Field:
 *		find JNIFieldID for refObject, create j.l.String for name and signature and store in MN.name/type fields.
 *		set vmindex to the fieldID pointer and target to the field offset.
 *		set MN.clazz to declaring class in the fieldID struct.
 * For j.l.reflect.Method or j.l.reflect.Constructor:
 *		find JNIMethodID, set target to the J9Method and vmindex as appropriate for dispatch.
 *		set MN.clazz to the refObject's declaring class.
 *
 * Then for both, compute the MN.flags using access flags and invocation type based on the JNI-id.
 *
 * Throw an IllegalArgumentException if the refObject is not a Field/Method/Constructor
 *
 * Note: caller must have vmaccess before invoking this helper
 */
static void
initImpl(J9VMThread *currentThread, j9object_t membernameObject, j9object_t refObject)
{
	J9JavaVM *vm = currentThread->javaVM;
	const J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;

	J9Class* refClass = J9OBJECT_CLAZZ(currentThread, refObject);

	jint flags = 0;
	jlong vmindex = 0;
	jlong target = 0;
	j9object_t clazzObject = NULL;
	j9object_t nameObject = NULL;
	j9object_t typeObject = NULL;

	if (refClass == J9VMJAVALANGREFLECTFIELD(vm)) {
		J9JNIFieldID *fieldID = vm->reflectFunctions.idFromFieldObject(currentThread, NULL, refObject);
		J9ROMFieldShape *romField = fieldID->field;
		UDATA offset = fieldID->offset;

		if (J9_ARE_ANY_BITS_SET(romField->modifiers, J9AccStatic)) {
			offset |= J9_SUN_STATIC_FIELD_OFFSET_TAG;
			if (J9_ARE_ANY_BITS_SET(romField->modifiers, J9AccFinal)) {
				offset |= J9_SUN_FINAL_FIELD_OFFSET_TAG;
			}
		}
		vmindex = (jlong)fieldID;
		target = (jlong)offset;

		flags = fieldID->field->modifiers & CFR_FIELD_ACCESS_MASK;
		flags |= MN_IS_FIELD;
		flags |= (J9_ARE_ANY_BITS_SET(flags, J9AccStatic) ? MH_REF_GETSTATIC : MH_REF_GETFIELD) << MN_REFERENCE_KIND_SHIFT;
		if (VM_VMHelpers::isTrustedFinalField(fieldID->field, fieldID->declaringClass->romClass)) {
			flags |= MN_TRUSTED_FINAL;
		}
#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
		if (J9ROMFIELD_IS_NULL_RESTRICTED(romField)) {
			flags |= MN_NULL_RESTRICTED;
			if (vmFuncs->isFlattenableFieldFlattened(fieldID->declaringClass, fieldID->field)) {
				flags |= MN_FLAT_FIELD;
			}
		}
#endif /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */

		nameObject = J9VMJAVALANGREFLECTFIELD_NAME(currentThread, refObject);
		typeObject = J9VMJAVALANGREFLECTFIELD_TYPE(currentThread, refObject);

		clazzObject = J9VM_J9CLASS_TO_HEAPCLASS(fieldID->declaringClass);
	} else if (refClass == J9VMJAVALANGREFLECTMETHOD(vm)) {
		J9JNIMethodID *methodID = vm->reflectFunctions.idFromMethodObject(currentThread, refObject);
		target = (jlong)methodID->method;

		J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(methodID->method);

		J9Class *declaringClass = J9_CLASS_FROM_METHOD(methodID->method);
		J9UTF8 *methodName = J9ROMMETHOD_NAME(romMethod);
		if (isPolymorphicMHMethod(vm, declaringClass, methodName)
#if JAVA_SPEC_VERSION >= 9
		|| ((declaringClass == J9VMJAVALANGINVOKEVARHANDLE(vm))
			&& VM_VMHelpers::isPolymorphicVarHandleMethod(J9UTF8_DATA(methodName), J9UTF8_LENGTH(methodName)))
#endif
		) {
			/* Do not initialize polymorphic MH/VH methods as the Java code handles the "MemberName.clazz == null" case for them. */
			return;
		}

		flags = romMethod->modifiers & CFR_METHOD_ACCESS_MASK;
		if (J9_ARE_ANY_BITS_SET(romMethod->modifiers, J9AccMethodCallerSensitive)) {
			flags |= MN_CALLER_SENSITIVE;
		}
		flags |= MN_IS_METHOD;
		if (J9_ARE_ANY_BITS_SET(methodID->vTableIndex, J9_JNI_MID_INTERFACE)) {
			flags |= MH_REF_INVOKEINTERFACE << MN_REFERENCE_KIND_SHIFT;
		} else if (J9_ARE_ANY_BITS_SET(romMethod->modifiers , J9AccStatic)) {
			flags |= MH_REF_INVOKESTATIC << MN_REFERENCE_KIND_SHIFT;
		} else if (J9_ARE_ANY_BITS_SET(romMethod->modifiers , J9AccFinal) || !J9ROMMETHOD_HAS_VTABLE(romMethod)) {
			flags |= MH_REF_INVOKESPECIAL << MN_REFERENCE_KIND_SHIFT;
		} else {
			flags |= MH_REF_INVOKEVIRTUAL << MN_REFERENCE_KIND_SHIFT;
		}

		nameObject = J9VMJAVALANGREFLECTMETHOD_NAME(currentThread, refObject);
		clazzObject = J9VMJAVALANGREFLECTMETHOD_CLAZZ(currentThread, refObject);
		J9Class *clazz = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, clazzObject);
		vmindex = vmindexValueForMethodMemberName(methodID, clazz, flags);
	} else if (refClass == J9VMJAVALANGREFLECTCONSTRUCTOR(vm)) {
		J9JNIMethodID *methodID = vm->reflectFunctions.idFromConstructorObject(currentThread, refObject);
		vmindex = -1;
		target = (jlong)methodID->method;

		J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(methodID->method);

		flags = romMethod->modifiers & CFR_METHOD_ACCESS_MASK;
		if (J9_ARE_ANY_BITS_SET(romMethod->modifiers, J9AccMethodCallerSensitive)) {
			flags |= MN_CALLER_SENSITIVE;
		}
		flags |= MN_IS_CONSTRUCTOR | (MH_REF_INVOKESPECIAL << MN_REFERENCE_KIND_SHIFT);

		clazzObject = J9VMJAVALANGREFLECTMETHOD_CLAZZ(currentThread, refObject);
	} else {
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
	}

	if (!VM_VMHelpers::exceptionPending(currentThread)) {
		if (addMemberNameToClass(currentThread, membernameObject, clazzObject)) {
			J9VMJAVALANGINVOKEMEMBERNAME_SET_FLAGS(currentThread, membernameObject, flags);
			J9VMJAVALANGINVOKEMEMBERNAME_SET_NAME(currentThread, membernameObject, nameObject);
			if (NULL != typeObject) {
				Assert_JCL_true(OMR_ARE_ALL_BITS_SET(flags, MN_IS_FIELD));
				J9VMJAVALANGINVOKEMEMBERNAME_SET_TYPE(currentThread, membernameObject, typeObject);
			}
			J9VMJAVALANGINVOKEMEMBERNAME_SET_CLAZZ(currentThread, membernameObject, clazzObject);
			J9OBJECT_U64_STORE(currentThread, membernameObject, vm->vmindexOffset, (U_64)vmindex);
			J9OBJECT_U64_STORE(currentThread, membernameObject, vm->vmtargetOffset, (U_64)target);
			Trc_JCL_java_lang_invoke_MethodHandleNatives_initImpl_setData(currentThread, flags, nameObject, typeObject, clazzObject, vmindex, target);
		}
	}
}

struct LocalJ9UTF8Buffer {
	/**
	 * Constructs an empty LocalJ9UTF8Buffer.
	 */
	LocalJ9UTF8Buffer()
		: utf8(nullptr)
		, capacity(0)
		, cursor(nullptr)
	{
	}

	/**
	 * Constructs a LocalJ9UTF8Buffer object from a J9UTF8 object pointer
	 * and its size.
	 * @param[in] buffer Pointer to the J9UTF8 buffer
	 * @param[in] length Length of the entire J9UTF8 buffer in bytes
	 */
	LocalJ9UTF8Buffer(J9UTF8 *buffer, size_t length)
		: utf8(buffer)
		, capacity(length - offsetof(J9UTF8, data))
		, cursor(J9UTF8_DATA(buffer))
	{
	}

	/**
	 * Calculate the remaining slots in the buffer.
	 * @return number of remaining slots in the buffer
	 */
	size_t remaining()
	{
		return capacity - static_cast<size_t>(cursor - J9UTF8_DATA(utf8));
	}

	/**
	 * Put a character into the buffer at the cursor, then advance the cursor.
	 * @param[in] c The character to put into the buffer
	 */
	void putCharAtCursor(char c)
	{
		*cursor = c;
		cursor += 1;
	}

	/**
	 * Advance the cursor n slots.
	 * @param[in] n The number of slots to advance the cursor by
	 */
	void advanceN(size_t n)
	{
		cursor += n;
	}

	/**
	 * Null-terminates the data, and sets the J9UTF8 length from a cursor-position calculation.
	 */
	void commitLength()
	{
		*cursor = '\0';
		J9UTF8_SET_LENGTH(utf8, static_cast<U_16>(cursor - J9UTF8_DATA(utf8)));
	}

	J9UTF8 *utf8; /**< Pointer to the J9UTF8 struct */
	size_t capacity; /**< Capacity of the J9UTF8 data buffer */
	U_8 *cursor; /**< Pointer to current position in J9UTF8 data buffer */
};

/**
 * Returns a character corresponding to a primitive-type class.
 * @param[in] vm J9JavaVM instance for current thread
 * @param[in] clazz The class of signature character of interest
 * @return character to newly allocated and filled-in signature buffer
 * @return the character corresponding to a primitive type
 */
static VMINLINE char
sigForPrimitiveOrVoid(J9JavaVM *vm, J9Class *clazz)
{
	char c = '\0';

	if (clazz == vm->booleanReflectClass) {
		c = 'Z';
	} else if (clazz == vm->byteReflectClass) {
		c = 'B';
	} else if (clazz == vm->charReflectClass) {
		c = 'C';
	} else if (clazz == vm->shortReflectClass) {
		c = 'S';
	} else if (clazz == vm->intReflectClass) {
		c = 'I';
	} else if (clazz == vm->longReflectClass) {
		c = 'J';
	} else if (clazz == vm->floatReflectClass) {
		c = 'F';
	} else if (clazz == vm->doubleReflectClass) {
		c = 'D';
	} else if (clazz == vm->voidReflectClass) {
		c = 'V';
	}

	return c;
}

/**
 * Gets a class signature's string length.
 * @param[in] currentThread The J9VMThread of the current thread
 * @param[in] clazz The class whose signature is being queried
 * @return length of a class' signature without null-termination
 */
static UDATA
getClassSignatureLength(J9VMThread *currentThread, J9Class *clazz)
{
	J9JavaVM *vm = currentThread->javaVM;
	UDATA signatureLength = 0;

	if (J9ROMCLASS_IS_PRIMITIVE_TYPE(clazz->romClass)) {
		signatureLength = 1;
	} else {
		j9object_t sigString = J9VMJAVALANGCLASS_CLASSNAMESTRING(currentThread, J9VM_J9CLASS_TO_HEAPCLASS(clazz));
		if (NULL != sigString) {
			/* +2 so that we can fit 'L' and ';' around the class name. */
			signatureLength = vm->internalVMFunctions->getStringUTF8Length(currentThread, sigString);
			if (signatureLength <= (UDATA_MAX - 2)) {
				signatureLength += 2;
			}
		} else {
			J9Class *myClass = clazz;
			UDATA numDims = 0;
			bool isPrimitive = false;
			if (J9CLASS_IS_ARRAY(myClass)) {
				J9ArrayClass *arrayClazz = reinterpret_cast<J9ArrayClass *>(myClass);
				numDims = arrayClazz->arity;

				J9Class *leafComponentType = arrayClazz->leafComponentType;
				isPrimitive = J9ROMCLASS_IS_PRIMITIVE_TYPE(leafComponentType->romClass);
				if (isPrimitive) {
					/* -1 to account for the '[' already prepended to the primitive array class' name.
					 * Result guaranteed to be >= 0 because the minimum arity for a J9ArrayClass is 1.
					 */
					numDims -= 1;
					myClass = leafComponentType->arrayClass;
				} else {
					myClass = leafComponentType;
				}
			}
			if (!isPrimitive) {
				/* +2 so that we can fit 'L' and ';' around the class name. */
				signatureLength += 2;
			}
			J9UTF8 *romName = J9ROMCLASS_CLASSNAME(myClass->romClass);
			U_32 nameLength = J9UTF8_LENGTH(romName);
			signatureLength += nameLength + numDims;
		}
	}

	return signatureLength;
}

/**
 * Fills in a class signature into a signature buffer.
 * @param[in] currentThread The J9VMThread of the current thread
 * @param[in] clazz The class whose signature is being queried
 * @param[in,out] stringBuffer The signature buffer to place the signature into
 * @return true if the signature fits into stringBuffer, false otherwise
 */
static bool
getClassSignatureInout(J9VMThread *currentThread, J9Class *clazz, LocalJ9UTF8Buffer *stringBuffer)
{
	J9JavaVM *vm = currentThread->javaVM;
	bool result = false;

	if (J9ROMCLASS_IS_PRIMITIVE_TYPE(clazz->romClass)) {
		/* 2 to ensure that a null-termination can be appended if this primitive type is
		 * the final type in the signature.
		 */
		if (2 <= stringBuffer->remaining()) {
			const char c = sigForPrimitiveOrVoid(vm, clazz);
			stringBuffer->putCharAtCursor(c);
			result = true;
		}
	} else {
		j9object_t sigString = J9VMJAVALANGCLASS_CLASSNAMESTRING(currentThread, J9VM_J9CLASS_TO_HEAPCLASS(clazz));
		if (NULL != sigString) {
			/* +3 so that we can fit 'L' and ';' around the class name and add null-terminator. */
			UDATA utfLength = vm->internalVMFunctions->getStringUTF8Length(currentThread, sigString);
			if (utfLength <= (UDATA_MAX - 3)) {
				utfLength += 3;
				if (utfLength <= stringBuffer->remaining()) {
					if (J9ROMCLASS_IS_ARRAY(clazz->romClass)) {
						vm->internalVMFunctions->copyStringToUTF8Helper(
							currentThread, sigString, J9_STR_XLAT, 0, J9VMJAVALANGSTRING_LENGTH(currentThread, sigString),
							stringBuffer->cursor, utfLength - 3);
						/* Adjust cursor to account for the call to copyStringToUTF8Helper. */
						stringBuffer->advanceN(utfLength - 3);
					} else {
						stringBuffer->putCharAtCursor('L');
						vm->internalVMFunctions->copyStringToUTF8Helper(
							currentThread, sigString, J9_STR_XLAT, 0, J9VMJAVALANGSTRING_LENGTH(currentThread, sigString),
							stringBuffer->cursor, utfLength - 3);
						/* Adjust cursor to account for the call to copyStringToUTF8Helper. */
						stringBuffer->advanceN(utfLength - 3);
						stringBuffer->putCharAtCursor(';');
					}
					result = true;
				}
			}
		} else {
			J9Class *myClass = clazz;
			UDATA numDims = 0;
			bool isPrimitive = false;
			if (J9CLASS_IS_ARRAY(myClass)) {
				J9ArrayClass *arrayClazz = reinterpret_cast<J9ArrayClass *>(myClass);
				numDims = arrayClazz->arity;

				J9Class *leafComponentType = arrayClazz->leafComponentType;
				isPrimitive = J9ROMCLASS_IS_PRIMITIVE_TYPE(leafComponentType->romClass);
				if (isPrimitive) {
					/* -1 to account for the '[' already prepended to the primitive array class' name.
					 * Result guaranteed to be >= 0 because the minimum arity for a J9ArrayClass is 1.
					 */
					numDims -= 1;
					myClass = leafComponentType->arrayClass;
				} else {
					myClass = leafComponentType;
				}
			}
			/* +1 to ensure we can add a null-terminator. */
			UDATA sigLength = 1;
			if (!isPrimitive) {
				/* +2 so that we can fit 'L' and ';' around the class name. */
				sigLength += 2;
			}
			J9UTF8 *romName = J9ROMCLASS_CLASSNAME(myClass->romClass);
			U_32 nameLength = J9UTF8_LENGTH(romName);
			const char *name = reinterpret_cast<const char *>(J9UTF8_DATA(romName));
			sigLength += nameLength + numDims;

			if (sigLength <= stringBuffer->remaining()) {
				for (UDATA i = 0; i < numDims; i++) {
					stringBuffer->putCharAtCursor('[');
				}

				if (!isPrimitive) {
					stringBuffer->putCharAtCursor('L');
				}

				memcpy(stringBuffer->cursor, name, nameLength);
				/* Adjust cursor to account for the memcpy. */
				stringBuffer->advanceN(nameLength);

				if (!isPrimitive) {
					stringBuffer->putCharAtCursor(';');
				}
				result = true;
			}
		}
	}

	return result;
}

/**
 * Allocates a J9UTF8 signature buffer and places a method signature constructed from a MethodType into it.
 * @param[in] currentThread The J9VMThread of the current thread
 * @param[in] typeObject A MethodType object that contains parameter and return type information
 * @return pointer to newly allocated and filled-in JUTF8 signature buffer
 */
static J9UTF8 *
getJ9UTF8SignatureFromMethodTypeWithMemAlloc(J9VMThread *currentThread, j9object_t typeObject)
{
	J9JavaVM *vm = currentThread->javaVM;
	j9object_t ptypes = J9VMJAVALANGINVOKEMETHODTYPE_PTYPES(currentThread, typeObject);
	U_32 numArgs = J9INDEXABLEOBJECT_SIZE(currentThread, ptypes);
	UDATA signatureLength = 2; /* space for '(', ')' */
	UDATA tempSignatureLength = 0;
	UDATA signatureUtf8Size = 0;
	J9UTF8 *result = NULL;
	j9object_t rtype = NULL;
	J9Class *rclass = NULL;
	PORT_ACCESS_FROM_JAVAVM(vm);

	/* Calculate total signature length, including all ptypes and rtype. */
	for (U_32 i = 0; i < numArgs; i++) {
		j9object_t pObject = J9JAVAARRAYOFOBJECT_LOAD(currentThread, ptypes, i);
		J9Class *pclass = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, pObject);
		tempSignatureLength = getClassSignatureLength(currentThread, pclass);
		if (signatureLength > (J9UTF8_MAX_LENGTH - tempSignatureLength)) {
			goto done;
		}
		signatureLength += tempSignatureLength;
	}
	rtype = J9VMJAVALANGINVOKEMETHODTYPE_RTYPE(currentThread, typeObject);
	rclass = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, rtype);
	tempSignatureLength = getClassSignatureLength(currentThread, rclass);
	if (signatureLength > (J9UTF8_MAX_LENGTH - tempSignatureLength)) {
		goto done;
	}
	signatureLength += tempSignatureLength;

	signatureUtf8Size = signatureLength + sizeof(J9UTF8) + 1; /* +1 for a null-terminator */
	result = reinterpret_cast<J9UTF8 *>(j9mem_allocate_memory(signatureUtf8Size, OMRMEM_CATEGORY_VM));

	if (NULL != result) {
		LocalJ9UTF8Buffer stringBuffer(result, signatureUtf8Size);

		stringBuffer.putCharAtCursor('(');
		for (U_32 i = 0; i < numArgs; i++) {
			j9object_t pObject = J9JAVAARRAYOFOBJECT_LOAD(currentThread, ptypes, i);
			J9Class *pclass = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, pObject);
			getClassSignatureInout(currentThread, pclass, &stringBuffer);
		}
		stringBuffer.putCharAtCursor(')');

		j9object_t rtype = J9VMJAVALANGINVOKEMETHODTYPE_RTYPE(currentThread, typeObject);
		J9Class *rclass = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, rtype);
		getClassSignatureInout(currentThread, rclass, &stringBuffer);
		stringBuffer.commitLength();
	}

done:
	return result;
}

/**
 * Attempts to fill in a method signature constructed from a MethodType into a passed in signature buffer.
 * Falls back to a dynamic buffer allocation if the statically allocated buffer's capacity is exceeded.
 * @param[in] currentThread The J9VMThread of the current thread
 * @param[in] typeObject A MethodType object that contains parameter and return type information
 * @param[in,out] stringBuffer The signature buffer to place the signature into
 * @return pointer to a filled-in JUTF8 signature buffer, either the dynamically or statically allocated buffer
 */
static J9UTF8 *
getJ9UTF8SignatureFromMethodType(J9VMThread *currentThread, j9object_t typeObject, LocalJ9UTF8Buffer *stringBuffer)
{
	j9object_t ptypes = J9VMJAVALANGINVOKEMETHODTYPE_PTYPES(currentThread, typeObject);
	U_32 numArgs = J9INDEXABLEOBJECT_SIZE(currentThread, ptypes);

	stringBuffer->putCharAtCursor('(');
	for (U_32 i = 0; i < numArgs; i++) {
		j9object_t pObject = J9JAVAARRAYOFOBJECT_LOAD(currentThread, ptypes, i);
		J9Class *pclass = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, pObject);
		if (!getClassSignatureInout(currentThread, pclass, stringBuffer)) {
			/* Failing getClassSignatureInout means stringBuffer exceeded capacity.
			 * Fall back to dynamic allocation.
			 */
			return getJ9UTF8SignatureFromMethodTypeWithMemAlloc(currentThread, typeObject);
		}
	}

	if (1 >= stringBuffer->remaining()) {
		/* Not enough space left in statically allocated buffer.
		 * Fall back to dynamic allocation.
		 */
		return getJ9UTF8SignatureFromMethodTypeWithMemAlloc(currentThread, typeObject);
	}
	stringBuffer->putCharAtCursor(')');

	/* Return type */
	j9object_t rtype = J9VMJAVALANGINVOKEMETHODTYPE_RTYPE(currentThread, typeObject);
	J9Class *rclass = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, rtype);
	if (!getClassSignatureInout(currentThread, rclass, stringBuffer)) {
		/* Failing getClassSignatureInout means stringBuffer exceeded capacity.
		 * Fall back to dynamic allocation.
		 */
		return getJ9UTF8SignatureFromMethodTypeWithMemAlloc(currentThread, typeObject);
	}

	if (0 == stringBuffer->remaining()) {
		/* Not enough space left in statically allocated buffer.
		 * Fall back to dynamic allocation.
		 */
		return getJ9UTF8SignatureFromMethodTypeWithMemAlloc(currentThread, typeObject);
	}

	stringBuffer->commitLength();
	return stringBuffer->utf8;
}

j9object_t
resolveRefToObject(J9VMThread *currentThread, J9ConstantPool *ramConstantPool, U_16 cpIndex, bool resolve)
{
	J9JavaVM *vm = currentThread->javaVM;
	const J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;

	j9object_t result = NULL;

	J9RAMSingleSlotConstantRef *ramCP = (J9RAMSingleSlotConstantRef*)ramConstantPool + cpIndex;
	U_32 *cpShapeDescription = J9ROMCLASS_CPSHAPEDESCRIPTION(J9_CLASS_FROM_CP(ramConstantPool)->romClass);

	switch (J9_CP_TYPE(cpShapeDescription, cpIndex)) {
	case J9CPTYPE_CLASS: {
		J9Class *clazz = (J9Class*)ramCP->value;
		if ((NULL == clazz) && resolve) {
			clazz = vmFuncs->resolveClassRef(currentThread, ramConstantPool, cpIndex, J9_RESOLVE_FLAG_RUNTIME_RESOLVE);
		}
		if (NULL != clazz) {
			result = J9VM_J9CLASS_TO_HEAPCLASS(clazz);
		}
		break;
	}
	case J9CPTYPE_STRING: {
		result = (j9object_t)ramCP->value;
		if ((NULL == result) && resolve) {
			result = vmFuncs->resolveStringRef(currentThread, ramConstantPool, cpIndex, J9_RESOLVE_FLAG_RUNTIME_RESOLVE);
		}
		break;
	}
	case J9CPTYPE_INT: {
		J9ROMSingleSlotConstantRef *romCP = (J9ROMSingleSlotConstantRef*)J9_ROM_CP_FROM_CP(ramConstantPool) + cpIndex;
		result = vm->memoryManagerFunctions->J9AllocateObject(currentThread, J9VMJAVALANGINTEGER_OR_NULL(vm), J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
		if (NULL == result) {
			vmFuncs->setHeapOutOfMemoryError(currentThread);
			goto done;
		}
		J9VMJAVALANGINTEGER_SET_VALUE(currentThread, result, romCP->data);
		break;
	}
	case J9CPTYPE_FLOAT: {
		J9ROMSingleSlotConstantRef *romCP = (J9ROMSingleSlotConstantRef*)J9_ROM_CP_FROM_CP(ramConstantPool) + cpIndex;
		result = vm->memoryManagerFunctions->J9AllocateObject(currentThread, J9VMJAVALANGFLOAT_OR_NULL(vm), J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
		if (NULL == result) {
			vmFuncs->setHeapOutOfMemoryError(currentThread);
			goto done;
		}
		J9VMJAVALANGFLOAT_SET_VALUE(currentThread, result, romCP->data);
		break;
	}
	case J9CPTYPE_LONG: {
		J9ROMConstantRef *romCP = (J9ROMConstantRef*)J9_ROM_CP_FROM_CP(ramConstantPool) + cpIndex;
#ifdef J9VM_ENV_LITTLE_ENDIAN
		U_64 value = (((U_64)(romCP->slot2)) << 32) | ((U_64)(romCP->slot1));
#else
		U_64 value = (((U_64)(romCP->slot1)) << 32) | ((U_64)(romCP->slot2));
#endif
		result = vm->memoryManagerFunctions->J9AllocateObject(currentThread, J9VMJAVALANGLONG_OR_NULL(vm), J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
		if (NULL == result) {
			vmFuncs->setHeapOutOfMemoryError(currentThread);
			goto done;
		}
		J9VMJAVALANGLONG_SET_VALUE(currentThread, result, value);
		break;
	}
	case J9CPTYPE_DOUBLE: {
		J9ROMConstantRef *romCP = (J9ROMConstantRef*)J9_ROM_CP_FROM_CP(ramConstantPool) + cpIndex;
#ifdef J9VM_ENV_LITTLE_ENDIAN
		U_64 value = (((U_64)(romCP->slot2)) << 32) | ((U_64)(romCP->slot1));
#else
		U_64 value = (((U_64)(romCP->slot1)) << 32) | ((U_64)(romCP->slot2));
#endif
		result = vm->memoryManagerFunctions->J9AllocateObject(currentThread, J9VMJAVALANGDOUBLE_OR_NULL(vm), J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
		if (NULL == result) {
			vmFuncs->setHeapOutOfMemoryError(currentThread);
			goto done;
		}
		J9VMJAVALANGDOUBLE_SET_VALUE(currentThread, result, value);
		break;
	}
	case J9CPTYPE_METHOD_TYPE: {
		result = (j9object_t)ramCP->value;
		if ((NULL == result) && resolve) {
			result = vmFuncs->resolveMethodTypeRef(currentThread, ramConstantPool, cpIndex, J9_RESOLVE_FLAG_RUNTIME_RESOLVE);
		}
		break;
	}
	case J9CPTYPE_METHODHANDLE: {
		result = (j9object_t)ramCP->value;
		if ((NULL == result) && resolve) {
			result = vmFuncs->resolveMethodHandleRef(currentThread, ramConstantPool, cpIndex, J9_RESOLVE_FLAG_RUNTIME_RESOLVE | J9_RESOLVE_FLAG_NO_CLASS_INIT);
		}
		break;
	}
	case J9CPTYPE_CONSTANT_DYNAMIC: {
		result = (j9object_t)ramCP->value;
		if ((NULL == result) && resolve) {
			result = vmFuncs->resolveConstantDynamic(currentThread, ramConstantPool, cpIndex, J9_RESOLVE_FLAG_RUNTIME_RESOLVE);
		}
		break;
	}
	} /* switch */
done:
	return result;
}

J9Method *
lookupMethod(J9VMThread *currentThread, J9Class *resolvedClass, J9UTF8 *name, J9UTF8 *signature, J9Class *callerClass, UDATA lookupOptions)
{
	J9Method *result = NULL;
	J9NameAndSignature nas;
	J9UTF8 nullSignature = {0};

	nas.name = name;
	nas.signature = signature;
	lookupOptions |= J9_LOOK_DIRECT_NAS;

	/* If looking for a MethodHandle polymorphic INL method, allow any caller signature. */
	if (isPolymorphicMHMethod(currentThread->javaVM, resolvedClass, name)) {
		nas.signature = &nullSignature;
		/* Set flag for partial signature lookup. Signature length is already initialized to 0. */
		lookupOptions |= J9_LOOK_PARTIAL_SIGNATURE;
	}

	result = (J9Method*)currentThread->javaVM->internalVMFunctions->javaLookupMethod(currentThread, resolvedClass, (J9ROMNameAndSignature*)&nas, callerClass, lookupOptions);

	return result;
}

static void
setCallSiteTargetImpl(J9VMThread *currentThread, jobject callsite, jobject target, bool isVolatile)
{
	J9JavaVM *javaVM = currentThread->javaVM;
	const J9InternalVMFunctions *vmFuncs = javaVM->internalVMFunctions;
	vmFuncs->internalEnterVMFromJNI(currentThread);

	if ((NULL == callsite) || (NULL == target)) {
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		j9object_t callsiteObject = J9_JNI_UNWRAP_REFERENCE(callsite);
		j9object_t targetObject = J9_JNI_UNWRAP_REFERENCE(target);

		J9Class *clazz = J9OBJECT_CLAZZ(currentThread, callsiteObject);
		UDATA offset = (UDATA)vmFuncs->instanceFieldOffset(
			currentThread,
			clazz,
			(U_8*)"target",
			LITERAL_STRLEN("target"),
			(U_8*)"Ljava/lang/invoke/MethodHandle;",
			LITERAL_STRLEN("Ljava/lang/invoke/MethodHandle;"),
			NULL, NULL, 0);
		offset += J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread);
		MM_ObjectAccessBarrierAPI objectAccessBarrier = MM_ObjectAccessBarrierAPI(currentThread);

		/* Check for MutableCallSite modification. */
		J9JITConfig* jitConfig = javaVM->jitConfig;
		J9Class *mcsClass = vmFuncs->peekClassHashTable(
			currentThread,
			javaVM->systemClassLoader,
			J9UTF8_DATA(&mutableCallSite),
			J9UTF8_LENGTH(&mutableCallSite));

		if (!isVolatile /* MutableCallSite uses setTargetNormal(). */
		&& (NULL != jitConfig)
		&& (NULL != mcsClass)
		&& VM_VMHelpers::inlineCheckCast(clazz, mcsClass)
		) {
			jitConfig->jitSetMutableCallSiteTarget(currentThread, callsiteObject, targetObject);
		} else {
			/* There are no runtime assumptions to invalidate (either because
			 * the call site is not a MutableCallSite, or because the JIT
			 * compiler is not loaded). */
			objectAccessBarrier.inlineMixedObjectStoreObject(currentThread, callsiteObject, offset, targetObject, isVolatile);
		}
	}
	vmFuncs->internalExitVMToJNI(currentThread);
}


/**
 * static native void init(MemberName self, Object ref);
 *
 * Initializes a MemberName object using the given ref object.
 *		see initImpl for detail
 * Throw NPE if self or ref is null
 * Throw IllegalArgumentException if ref is not a field/method/constructor
 */
void JNICALL
Java_java_lang_invoke_MethodHandleNatives_init(JNIEnv *env, jclass clazz, jobject self, jobject ref)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	vmFuncs->internalEnterVMFromJNI(currentThread);

	Trc_JCL_java_lang_invoke_MethodHandleNatives_init_Entry(env, self, ref);
	if ((NULL == self) || (NULL == ref)) {
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		j9object_t membernameObject = J9_JNI_UNWRAP_REFERENCE(self);
		j9object_t refObject = J9_JNI_UNWRAP_REFERENCE(ref);

		initImpl(currentThread, membernameObject, refObject);
	}
	Trc_JCL_java_lang_invoke_MethodHandleNatives_init_Exit(env);
	vmFuncs->internalExitVMToJNI(currentThread);
}

/**
 * static native void expand(MemberName self);
 *
 * Given a MemberName object, try to set the uninitialized fields from existing VM metadata.
 * Uses VM metadata (vmindex & vmtarget) to set symblic data fields (name & type & defc)
 *
 * Throws NullPointerException if MemberName object is null.
 * Throws IllegalArgumentException if MemberName doesn't contain required data to expand.
 * Throws InternalError if the MemberName object contains invalid data or completely uninitialized.
 */
void JNICALL
Java_java_lang_invoke_MethodHandleNatives_expand(JNIEnv *env, jclass clazz, jobject self)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	const J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	vmFuncs->internalEnterVMFromJNI(currentThread);

	Trc_JCL_java_lang_invoke_MethodHandleNatives_expand_Entry(env, self);
	if (NULL == self) {
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		j9object_t membernameObject = J9_JNI_UNWRAP_REFERENCE(self);
		jint flags = J9VMJAVALANGINVOKEMEMBERNAME_FLAGS(currentThread, membernameObject);
		jlong vmindex = (jlong)J9OBJECT_ADDRESS_LOAD(currentThread, membernameObject, vm->vmindexOffset);

		Trc_JCL_java_lang_invoke_MethodHandleNatives_expand_Data(env, membernameObject, flags, vmindex);
		if (J9_ARE_ANY_BITS_SET(flags, MN_IS_FIELD)) {
			/* For Field MemberName, the clazz and vmindex fields must be set. */
			if ((NULL != J9VMJAVALANGINVOKEMEMBERNAME_CLAZZ(currentThread, membernameObject)) && (NULL != (void*)vmindex)) {
				J9JNIFieldID *field = (J9JNIFieldID*)vmindex;

				/* if name/type field is uninitialized, create j.l.String from ROM field name/sig and store in MN fields. */
				if (NULL == J9VMJAVALANGINVOKEMEMBERNAME_NAME(currentThread, membernameObject)) {
					J9UTF8 *name = J9ROMFIELDSHAPE_NAME(field->field);
					j9object_t nameString = vm->memoryManagerFunctions->j9gc_createJavaLangStringWithUTFCache(currentThread, name);
					if (NULL != nameString) {
						/* Refetch reference after GC point */
						membernameObject = J9_JNI_UNWRAP_REFERENCE(self);
						J9VMJAVALANGINVOKEMEMBERNAME_SET_NAME(currentThread, membernameObject, nameString);
					}
				}
				if (NULL == J9VMJAVALANGINVOKEMEMBERNAME_TYPE(currentThread, membernameObject)) {
					J9UTF8 *signature = J9ROMFIELDSHAPE_SIGNATURE(field->field);
					j9object_t signatureString = vm->memoryManagerFunctions->j9gc_createJavaLangStringWithUTFCache(currentThread, signature);
					if (NULL != signatureString) {
						/* Refetch reference after GC point */
						membernameObject = J9_JNI_UNWRAP_REFERENCE(self);
						J9VMJAVALANGINVOKEMEMBERNAME_SET_TYPE(currentThread, membernameObject, signatureString);
					}
				}
			} else {
				vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
			}
		} else if (J9_ARE_ANY_BITS_SET(flags, MN_IS_METHOD | MN_IS_CONSTRUCTOR)) {
			J9Method *method = (J9Method *)(UDATA)J9OBJECT_U64_LOAD(_currentThread, membernameObject, vm->vmtargetOffset);
			if (NULL != method) {
				/* Retrieve method info using the J9Method and store to MN fields. */
				J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
				if (NULL == J9VMJAVALANGINVOKEMEMBERNAME_CLAZZ(currentThread, membernameObject)) {
					j9object_t newClassObject = J9VM_J9CLASS_TO_HEAPCLASS(J9_CLASS_FROM_METHOD(method));
					J9VMJAVALANGINVOKEMEMBERNAME_SET_CLAZZ(currentThread, membernameObject, newClassObject);
				}
				if (NULL == J9VMJAVALANGINVOKEMEMBERNAME_NAME(currentThread, membernameObject)) {
					J9UTF8 *name = J9ROMMETHOD_NAME(romMethod);
					j9object_t nameString = vm->memoryManagerFunctions->j9gc_createJavaLangStringWithUTFCache(currentThread, name);
					if (NULL != nameString) {
						/* Refetch reference after GC point */
						membernameObject = J9_JNI_UNWRAP_REFERENCE(self);
						J9VMJAVALANGINVOKEMEMBERNAME_SET_NAME(currentThread, membernameObject, nameString);
					}
				}
				if (NULL == J9VMJAVALANGINVOKEMEMBERNAME_TYPE(currentThread, membernameObject)) {
					J9UTF8 *signature = J9ROMMETHOD_SIGNATURE(romMethod);
					j9object_t signatureString = vm->memoryManagerFunctions->j9gc_createJavaLangStringWithUTFCache(currentThread, signature);
					if (NULL != signatureString) {
						/* Refetch reference after GC point */
						membernameObject = J9_JNI_UNWRAP_REFERENCE(self);
						J9VMJAVALANGINVOKEMEMBERNAME_SET_TYPE(currentThread, membernameObject, signatureString);
					}
				}
			} else {
				vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
			}
		} else {
			vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
		}
	}
	Trc_JCL_java_lang_invoke_MethodHandleNatives_expand_Exit(env);
	vmFuncs->internalExitVMToJNI(currentThread);
}

/**
 * [JDK8] static native MemberName resolve(MemberName self, Class<?> caller)
 * 	             throws LinkageError, ClassNotFoundException;
 *
 * [JDK11] static native MemberName resolve(MemberName self, Class<?> caller,
 * 	             boolean speculativeResolve) throws LinkageError, ClassNotFoundException;
 *
 * [JDK16+] static native MemberName resolve(MemberName self, Class<?> caller, int lookupMode,
 * 	             boolean speculativeResolve) throws LinkageError, ClassNotFoundException;
 *
 * Resolve the method/field represented by the MemberName's symbolic data (name & type & defc)
 * with the supplied caller. Store the resolved Method/Field's JNI-id in vmindex, field offset
 * or method pointer in vmtarget.
 *
 * If the speculativeResolve flag is not set, failed resolution will throw the corresponding exception.
 *
 * If the resolution failed with no exception,
 *   - Throw NoSuchFieldError for field MemberName issues.
 *   - Throw NoSuchMethodError for method/constructor MemberName issues.
 *   - Throw OutOfMemoryError for failure to allocate memory.
 *   - Throw LinkageError for other issues.
 */
jobject JNICALL
Java_java_lang_invoke_MethodHandleNatives_resolve(
#if JAVA_SPEC_VERSION == 8
                                                  JNIEnv *env, jclass clazz, jobject self, jclass caller
#elif JAVA_SPEC_VERSION == 11 /* JAVA_SPEC_VERSION == 8 */
                                                  JNIEnv *env, jclass clazz, jobject self, jclass caller,
                                                  jboolean speculativeResolve
#elif JAVA_SPEC_VERSION >= 16 /* JAVA_SPEC_VERSION == 11 */
                                                  JNIEnv *env, jclass clazz, jobject self, jclass caller,
                                                  jint lookupMode, jboolean speculativeResolve
#endif /* JAVA_SPEC_VERSION == 8 */
) {
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	const J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	jobject result = NULL;
	J9UTF8 *name = NULL;
	char nameBuffer[256];
	nameBuffer[0] = 0;
	J9UTF8 *signature = NULL;
	char signatureBuffer[256];
	signatureBuffer[0] = 0;
	PORT_ACCESS_FROM_JAVAVM(vm);
	vmFuncs->internalEnterVMFromJNI(currentThread);

#if JAVA_SPEC_VERSION >= 11
	Trc_JCL_java_lang_invoke_MethodHandleNatives_resolve_Entry(env, self, caller, (speculativeResolve ? "true" : "false"));
#else /* JAVA_SPEC_VERSION >= 11 */
	Trc_JCL_java_lang_invoke_MethodHandleNatives_resolve_Entry(env, self, caller, "false");
#endif /* JAVA_SPEC_VERSION >= 11 */

	if (NULL == self) {
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
	} else {
		j9object_t membernameObject = J9_JNI_UNWRAP_REFERENCE(self);
		jlong vmindex = (jlong)(UDATA)J9OBJECT_U64_LOAD(currentThread, membernameObject, vm->vmindexOffset);
		jlong target = (jlong)(UDATA)J9OBJECT_U64_LOAD(currentThread, membernameObject, vm->vmtargetOffset);

		jint flags = J9VMJAVALANGINVOKEMEMBERNAME_FLAGS(currentThread, membernameObject);
		jint new_flags = 0;
		j9object_t new_clazz = NULL;

		Trc_JCL_java_lang_invoke_MethodHandleNatives_resolve_Data(env, membernameObject, caller, flags, vmindex, target);
		/* Check if MemberName is already resolved */
		if (0 != target) {
			result = self;
		} else {
			/* Initialize nameObject after creating typeString which could trigger GC */
			j9object_t nameObject = NULL;
			j9object_t typeObject = J9VMJAVALANGINVOKEMEMBERNAME_TYPE(currentThread, membernameObject);
			j9object_t clazzObject = J9VMJAVALANGINVOKEMEMBERNAME_CLAZZ(currentThread, membernameObject);
			J9Class *resolvedClass = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, clazzObject);

			jint ref_kind = (flags >> MN_REFERENCE_KIND_SHIFT) & MN_REFERENCE_KIND_MASK;

			J9Class *typeClass = J9OBJECT_CLAZZ(currentThread, typeObject);

			/* The type field of a MemberName could be in:
			 *     MethodType:	MethodType representing method/constructor MemberName's method signature
			 *     String:		String representing MemberName's signature (field or method)
			 *     Class:		Class representing field MemberName's field type
			 */
			if (J9VMJAVALANGINVOKEMETHODTYPE(vm) == typeClass) {
				j9object_t sigString = J9VMJAVALANGINVOKEMETHODTYPE_METHODDESCRIPTOR(currentThread, typeObject);
				if (NULL != sigString) {
					signature = vmFuncs->copyStringToJ9UTF8WithMemAlloc(currentThread, sigString, J9_STR_XLAT, "", 0, signatureBuffer, sizeof(signatureBuffer));
				} else {
					LocalJ9UTF8Buffer stringBuffer(reinterpret_cast<J9UTF8 *>(signatureBuffer), sizeof(signatureBuffer));
					signature = getJ9UTF8SignatureFromMethodType(currentThread, typeObject, &stringBuffer);
					if (NULL == signature) {
						vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
						goto done;
					}
				}
			} else if (J9VMJAVALANGSTRING_OR_NULL(vm) == typeClass) {
				signature = vmFuncs->copyStringToJ9UTF8WithMemAlloc(currentThread, typeObject, J9_STR_XLAT, "", 0, signatureBuffer, sizeof(signatureBuffer));
			} else if (J9VMJAVALANGCLASS(vm) == typeClass) {
				J9Class *rclass = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, typeObject);
				UDATA signatureLength = getClassSignatureLength(currentThread, rclass);
				if (signatureLength > J9UTF8_MAX_LENGTH) {
					vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
					goto done;
				}
				signatureLength += sizeof(J9UTF8) + 1 /* null-terminator */;
				LocalJ9UTF8Buffer stringBuffer;
				if (signatureLength <= sizeof(signatureBuffer)) {
					stringBuffer = LocalJ9UTF8Buffer(reinterpret_cast<J9UTF8 *>(signatureBuffer), sizeof(signatureBuffer));
				} else {
					signature = reinterpret_cast<J9UTF8 *>(j9mem_allocate_memory(signatureLength, OMRMEM_CATEGORY_VM));
					if (NULL == signature) {
						vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
						goto done;
					}
					stringBuffer = LocalJ9UTF8Buffer(signature, static_cast<size_t>(signatureLength));
				}
				getClassSignatureInout(currentThread, rclass, &stringBuffer);
				stringBuffer.commitLength();
				signature = stringBuffer.utf8;
			} else {
				vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
				goto done;
			}

			/* Check if signature string is correctly generated */
			if (NULL == signature) {
				if (!VM_VMHelpers::exceptionPending(currentThread)) {
					vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
				}
				goto done;
			}

			/* Refetch reference after GC point */
			membernameObject = J9_JNI_UNWRAP_REFERENCE(self);
			nameObject = J9VMJAVALANGINVOKEMEMBERNAME_NAME(currentThread, membernameObject);
			name = vmFuncs->copyStringToJ9UTF8WithMemAlloc(currentThread, nameObject, J9_STR_NONE, "", 0, nameBuffer, sizeof(nameBuffer));
			if (NULL == name) {
				vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
				goto done;
			}

			j9object_t callerObject = (NULL == caller) ? NULL : J9_JNI_UNWRAP_REFERENCE(caller);
			J9Class *callerClass = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, callerObject);

			Trc_JCL_java_lang_invoke_MethodHandleNatives_resolve_NAS(env, J9UTF8_LENGTH(name), J9UTF8_DATA(name), J9UTF8_LENGTH(signature), J9UTF8_DATA(signature));

			if (J9_ARE_ANY_BITS_SET(flags, MN_IS_METHOD | MN_IS_CONSTRUCTOR)) {
				UDATA lookupOptions = 0;

#if JAVA_SPEC_VERSION >= 11
				if (JNI_TRUE == speculativeResolve) {
					lookupOptions |= J9_LOOK_NO_THROW;
				}
#endif /* JAVA_SPEC_VERSION >= 11 */

#if JAVA_SPEC_VERSION >= 16
				/* Check CL constraint only for lookup that is not trusted (with caller) and not public (UNCONDITIONAL mode not set)
				 * Note: UNCONDITIONAL lookupMode can only access unconditionally exported public members which assumes readablity
				 * ie lookup class not used to determine lookup context
				 * See java.lang.invoke.MethodHandles$Lookup.publicLookup()
				 */
				if ((NULL != callerClass) && J9_ARE_NO_BITS_SET(lookupMode, MN_UNCONDITIONAL_MODE)) {
					lookupOptions |= J9_LOOK_CLCONSTRAINTS;
				}
#else /* JAVA_SPEC_VERSION >= 16 */
				if (NULL != callerClass) {
					lookupOptions |= J9_LOOK_CLCONSTRAINTS;
				}
#endif /* JAVA_SPEC_VERSION >= 16 */

				/* Determine the lookup type based on reference kind and resolved class flags */
				switch (ref_kind)
				{
					case MH_REF_INVOKEINTERFACE:
						lookupOptions |= J9_LOOK_INTERFACE;
						break;
					case MH_REF_INVOKESPECIAL:
						lookupOptions |= (J9_LOOK_VIRTUAL | J9_LOOK_HANDLE_DEFAULT_METHOD_CONFLICTS);
						break;
					case MH_REF_INVOKESTATIC:
						lookupOptions |= J9_LOOK_STATIC;
						if (J9_ARE_ANY_BITS_SET(resolvedClass->romClass->modifiers, J9AccInterface)) {
							lookupOptions |= J9_LOOK_INTERFACE;
						}
						break;
					default:
						lookupOptions |= J9_LOOK_VIRTUAL;
						break;
				}

				/* Check if signature polymorphic native calls */
				J9Method *method = lookupMethod(currentThread, resolvedClass, name, signature, callerClass, lookupOptions);

				/* Check for resolution exception */
				if (VM_VMHelpers::exceptionPending(currentThread)) {
					J9Class *exceptionClass = J9OBJECT_CLAZZ(currentThread, currentThread->currentException);
					if ((ref_kind == MH_REF_INVOKESPECIAL) && (exceptionClass == J9VMJAVALANGINCOMPATIBLECLASSCHANGEERROR(vm))) {
						/* Special handling for default method conflict, defer the exception throw until invocation. */
						VM_VMHelpers::clearException(currentThread);
						/* Attempt to lookup method without checking for conflict.
						 * If this failed with error, then throw that error.
						 * Otherwise exception throw will be defered to the MH.invoke call.
						 */
						method = lookupMethod(
							currentThread, resolvedClass, name, signature, callerClass,
							(lookupOptions & ~J9_LOOK_HANDLE_DEFAULT_METHOD_CONFLICTS));

						if (!VM_VMHelpers::exceptionPending(currentThread)) {
							/* Set placeholder values for MemberName fields. */
							vmindex = (jlong)J9VM_RESOLVED_VMINDEX_FOR_DEFAULT_THROW;
							new_clazz = J9VM_J9CLASS_TO_HEAPCLASS(J9_CLASS_FROM_METHOD(method));
							new_flags = flags;

							/* Load special sendTarget to throw the exception during invocation */
							target = (jlong)(UDATA)vm->initialMethods.throwDefaultConflict;
						} else {
							goto done;
						}
					} else {
						goto done;
					}
				} else if (NULL != method) {
					J9JNIMethodID *methodID = vmFuncs->getJNIMethodID(currentThread, method);
					target = (jlong)(UDATA)method;

					J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(methodID->method);
					J9UTF8 *methodName = J9ROMMETHOD_NAME(romMethod);
					U_32 methodModifiers = romMethod->modifiers;
					new_clazz = J9VM_J9CLASS_TO_HEAPCLASS(J9_CLASS_FROM_METHOD(method));
					new_flags = methodModifiers & CFR_METHOD_ACCESS_MASK;

					if (J9_ARE_ANY_BITS_SET(methodModifiers, J9AccMethodCallerSensitive)) {
						new_flags |= MN_CALLER_SENSITIVE;
					}

					if (J9_ARE_ALL_BITS_SET(flags, MN_IS_METHOD)) {
						new_flags |= MN_IS_METHOD;
						if (MH_REF_INVOKEINTERFACE == ref_kind) {
							Assert_JCL_true(J9_ARE_NO_BITS_SET(methodModifiers, J9AccStatic));
#if JAVA_SPEC_VERSION < 11
							/* Ensure findVirtual throws an IllegalAccessException (by wrapping this IncompatibleClassChangeError)
							 * when trying to access a private interface method for Java 10- with OpenJDK MHs.
							 */
							if (J9_ARE_NO_BITS_SET(lookupOptions, J9_LOOK_STATIC)
							&& J9_ARE_ALL_BITS_SET(methodModifiers, J9AccPrivate)
							) {
								vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGINCOMPATIBLECLASSCHANGEERROR, NULL);
								goto done;
							}
#endif /* JAVA_SPEC_VERSION < 11 */
							if (J9_ARE_ALL_BITS_SET(methodID->vTableIndex, J9_JNI_MID_INTERFACE)) {
								new_flags |= MH_REF_INVOKEINTERFACE << MN_REFERENCE_KIND_SHIFT;
							} else if (!J9ROMMETHOD_HAS_VTABLE(romMethod)) {
								new_flags |= MH_REF_INVOKESPECIAL << MN_REFERENCE_KIND_SHIFT;
							} else {
								new_flags |= MH_REF_INVOKEVIRTUAL << MN_REFERENCE_KIND_SHIFT;
							}
						} else if (MH_REF_INVOKESPECIAL == ref_kind) {
							Assert_JCL_true(J9_ARE_NO_BITS_SET(methodModifiers, J9AccStatic));
							new_flags |= MH_REF_INVOKESPECIAL << MN_REFERENCE_KIND_SHIFT;
						} else if (MH_REF_INVOKESTATIC == ref_kind) {
							Assert_JCL_true(J9_ARE_ALL_BITS_SET(methodModifiers, J9AccStatic));
							new_flags |= MH_REF_INVOKESTATIC << MN_REFERENCE_KIND_SHIFT;
						} else if (MH_REF_INVOKEVIRTUAL == ref_kind) {
							Assert_JCL_true(J9_ARE_NO_BITS_SET(methodModifiers, J9AccStatic));
							if (!J9ROMMETHOD_HAS_VTABLE(romMethod)) {
								new_flags |= MH_REF_INVOKESPECIAL << MN_REFERENCE_KIND_SHIFT;
							} else {
								if (J9_ARE_ALL_BITS_SET(methodID->vTableIndex, J9_JNI_MID_INTERFACE)) {
									new_clazz = J9VM_J9CLASS_TO_HEAPCLASS(resolvedClass);
								}
								new_flags |= MH_REF_INVOKEVIRTUAL << MN_REFERENCE_KIND_SHIFT;
							}
						} else {
							Assert_JCL_unreachable();
						}
					} else if (J9_ARE_NO_BITS_SET(methodModifiers, J9AccStatic) && ('<' == (char)*J9UTF8_DATA(methodName))) {
						new_flags |= MN_IS_CONSTRUCTOR;
						new_flags |= MH_REF_INVOKESPECIAL << MN_REFERENCE_KIND_SHIFT;
					} else {
						Assert_JCL_unreachable();
					}

					J9Class *newJ9Clazz = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, new_clazz);
					vmindex = vmindexValueForMethodMemberName(methodID, newJ9Clazz, new_flags);
				}
			} else if (J9_ARE_ANY_BITS_SET(flags, MN_IS_FIELD)) {
				J9Class *declaringClass;
				J9ROMFieldShape *romField;
				UDATA lookupOptions = 0;
				UDATA offset = 0;
#if JAVA_SPEC_VERSION >= 11
				if (JNI_TRUE == speculativeResolve) {
					lookupOptions |= J9_RESOLVE_FLAG_NO_THROW_ON_FAIL;
				}
#endif /* JAVA_SPEC_VERSION >= 11 */

				/* MemberName doesn't differentiate if a field is static or not,
				 * the resolve code have to attempt to resolve as instance field first,
				 * then as static field if the first attempt failed.
				 */
				offset = vmFuncs->instanceFieldOffset(currentThread,
					resolvedClass,
					J9UTF8_DATA(name), J9UTF8_LENGTH(name),
					J9UTF8_DATA(signature), J9UTF8_LENGTH(signature),
					&declaringClass, (UDATA*)&romField,
					lookupOptions);

				if (offset == (UDATA)-1) {
					declaringClass = NULL;

					if (VM_VMHelpers::exceptionPending(currentThread)) {
						VM_VMHelpers::clearException(currentThread);
					}

					void* fieldAddress = vmFuncs->staticFieldAddress(currentThread,
						resolvedClass,
						J9UTF8_DATA(name), J9UTF8_LENGTH(name),
						J9UTF8_DATA(signature), J9UTF8_LENGTH(signature),
						&declaringClass, (UDATA*)&romField,
						lookupOptions,
						NULL);

					if (fieldAddress == NULL) {
						declaringClass = NULL;
					} else {
						offset = (UDATA)fieldAddress - (UDATA)declaringClass->ramStatics;
					}
				}

				if (NULL != declaringClass) {
					if ((NULL != callerClass)
#if JAVA_SPEC_VERSION >= 16
					 && (J9_ARE_NO_BITS_SET(lookupMode, MN_UNCONDITIONAL_MODE))
#endif
					) {
						if (callerClass->classLoader != declaringClass->classLoader) {
							U_16 sigOffset = 0;

							/* Skip the '[' or 'L' prefix */
							while ('[' == J9UTF8_DATA(signature)[sigOffset]) {
								sigOffset += 1;
							}
							if (IS_CLASS_SIGNATURE(J9UTF8_DATA(signature)[sigOffset])) {
								sigOffset += 1;
								J9BytecodeVerificationData *verifyData = vm->bytecodeVerificationData;
								if (NULL != verifyData) {
									omrthread_monitor_enter(vm->classTableMutex);
									UDATA clConstraintResult = verifyData->checkClassLoadingConstraintForNameFunction(
																	currentThread,
																	declaringClass->classLoader,
																	callerClass->classLoader,
																	J9UTF8_DATA(signature) + sigOffset,
																	J9UTF8_DATA(signature) + sigOffset,
																	J9UTF8_LENGTH(signature) - sigOffset - 1, /* -1 to remove the trailing ;*/
																	true,
																	true);
									omrthread_monitor_exit(vm->classTableMutex);
									if (0 != clConstraintResult) {
										vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGLINKAGEERROR, NULL);
										goto done;
									}
								}
							}
						}
					}

					UDATA inconsistentData = 0;
					J9JNIFieldID *fieldID = vmFuncs->getJNIFieldID(currentThread, declaringClass, romField, offset, &inconsistentData);
					vmindex = (jlong)(UDATA)fieldID;

					new_clazz = J9VM_J9CLASS_TO_HEAPCLASS(declaringClass);
					new_flags = MN_IS_FIELD | (fieldID->field->modifiers & CFR_FIELD_ACCESS_MASK);
					if (VM_VMHelpers::isTrustedFinalField(fieldID->field, fieldID->declaringClass->romClass)) {
						new_flags |= MN_TRUSTED_FINAL;
					}

					romField = fieldID->field;

					if (J9_ARE_ANY_BITS_SET(romField->modifiers, J9AccStatic)) {
						offset = fieldID->offset | J9_SUN_STATIC_FIELD_OFFSET_TAG;
						if (J9_ARE_ANY_BITS_SET(romField->modifiers, J9AccFinal)) {
							offset |= J9_SUN_FINAL_FIELD_OFFSET_TAG;
						}

						if ((MH_REF_PUTFIELD == ref_kind) || (MH_REF_PUTSTATIC == ref_kind)) {
							new_flags |= (MH_REF_PUTSTATIC << MN_REFERENCE_KIND_SHIFT);
						} else {
							new_flags |= (MH_REF_GETSTATIC << MN_REFERENCE_KIND_SHIFT);
						}
					} else {
						if ((MH_REF_PUTFIELD == ref_kind) || (MH_REF_PUTSTATIC == ref_kind)) {
							new_flags |= (MH_REF_PUTFIELD << MN_REFERENCE_KIND_SHIFT);
						} else {
							new_flags |= (MH_REF_GETFIELD << MN_REFERENCE_KIND_SHIFT);
						}
					}

					target = (jlong)offset;
				}
			}

			if ((0 != target) && ((0 != vmindex) || J9_ARE_ANY_BITS_SET(flags, MN_IS_METHOD | MN_IS_CONSTRUCTOR))) {
				/* Refetch reference after GC point */
				membernameObject = J9_JNI_UNWRAP_REFERENCE(self);
				if (addMemberNameToClass(currentThread, membernameObject, new_clazz)) {
					J9VMJAVALANGINVOKEMEMBERNAME_SET_FLAGS(currentThread, membernameObject, new_flags);
					J9VMJAVALANGINVOKEMEMBERNAME_SET_CLAZZ(currentThread, membernameObject, new_clazz);
					J9OBJECT_U64_STORE(currentThread, membernameObject, vm->vmindexOffset, (U_64)vmindex);
					J9OBJECT_U64_STORE(currentThread, membernameObject, vm->vmtargetOffset, (U_64)target);

					Trc_JCL_java_lang_invoke_MethodHandleNatives_resolve_resolved(env, vmindex, target, new_clazz, new_flags);

					result = vmFuncs->j9jni_createLocalRef(env, membernameObject);
				}
			}

			if ((NULL == result)
#if JAVA_SPEC_VERSION >= 11
			&& (JNI_TRUE != speculativeResolve)
#endif /* JAVA_SPEC_VERSION >= 11 */
			&& !VM_VMHelpers::exceptionPending(currentThread)
			) {
				if (J9_ARE_ANY_BITS_SET(flags, MN_IS_FIELD)) {
					vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGNOSUCHFIELDERROR, NULL);
				} else if (J9_ARE_ANY_BITS_SET(flags, MN_IS_CONSTRUCTOR | MN_IS_METHOD)) {
					vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGNOSUCHMETHODERROR, NULL);
				} else {
					vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGLINKAGEERROR, NULL);
				}
			}
		}
	}

done:
	if (name != reinterpret_cast<J9UTF8 *>(nameBuffer)) {
		j9mem_free_memory(name);
	}
	if (signature != reinterpret_cast<J9UTF8 *>(signatureBuffer)) {
		j9mem_free_memory(signature);
	}
#if JAVA_SPEC_VERSION >= 11
	if ((JNI_TRUE == speculativeResolve) && VM_VMHelpers::exceptionPending(currentThread)) {
		VM_VMHelpers::clearException(currentThread);
	}
#endif /* JAVA_SPEC_VERSION >= 11 */
	Trc_JCL_java_lang_invoke_MethodHandleNatives_resolve_Exit(env);
	vmFuncs->internalExitVMToJNI(currentThread);
	return result;
}

/**
 * static native int getMembers(Class<?> defc, String matchName, String matchSig,
 *      int matchFlags, Class<?> caller, int skip, MemberName[] results);
 *
 * Search the defc (defining class) chain for field/method that matches the given search parameters,
 * for each matching result found, initialize the next MemberName object in results array to reference the found field/method
 *
 *  - defc: the class to start the search
 *  - matchName: name to match, NULL to match any field/method name
 *  - matchSig: signature to match, NULL to match any field/method type
 *  - matchFlags: flags defining search options:
 *  		MN_IS_FIELD - search for fields
 *  		MN_IS_CONSTRUCTOR | MN_IS_METHOD - search for method/constructor
 *  		MN_SEARCH_SUPERCLASSES - search the superclasses of the defining class
 *  		MN_SEARCH_INTERFACES - search the interfaces implemented by the defining class
 *  - caller: the caller class performing the lookup
 *  - skip: number of matching results to skip before storing
 *  - results: an array of MemberName objects to hold the matched field/method
 */
jint JNICALL
Java_java_lang_invoke_MethodHandleNatives_getMembers(
                                                     JNIEnv *env, jclass clazz, jclass defc, jstring matchName,
                                                     jstring matchSig, jint matchFlags, jclass caller, jint skip,
                                                     jobjectArray results
) {
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	const J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	vmFuncs->internalEnterVMFromJNI(currentThread);
	jint result = 0;
	J9UTF8 *name = NULL;
	char nameBuffer[256];
	nameBuffer[0] = 0;
	J9UTF8 *signature = NULL;
	char signatureBuffer[256];
	signatureBuffer[0] = 0;
	j9object_t callerObject = ((NULL == caller) ? NULL : J9_JNI_UNWRAP_REFERENCE(caller));

	PORT_ACCESS_FROM_JAVAVM(vm);

	Trc_JCL_java_lang_invoke_MethodHandleNatives_getMembers_Entry(env, defc, matchName, matchSig, matchFlags, caller, skip, results);

	if ((NULL == defc) || (NULL == results) || ((NULL != callerObject) && (J9VMJAVALANGCLASS(vm) != J9OBJECT_CLAZZ(currentThread, callerObject)))) {
		result = -1;
	} else {
		j9object_t defcObject = J9_JNI_UNWRAP_REFERENCE(defc);
		J9Class *defClass = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, defcObject);

		if (NULL != matchName) {
			name = vmFuncs->copyStringToJ9UTF8WithMemAlloc(currentThread, J9_JNI_UNWRAP_REFERENCE(matchName), J9_STR_NONE, "", 0, nameBuffer, sizeof(nameBuffer));
			if (NULL == name) {
				vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
				goto done;
			}
		}
		if (NULL != matchSig) {
			signature = vmFuncs->copyStringToJ9UTF8WithMemAlloc(currentThread, J9_JNI_UNWRAP_REFERENCE(matchSig), J9_STR_NONE, "", 0, signatureBuffer, sizeof(signatureBuffer));
			if (NULL == signature) {
				vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
				goto done;
			}
		}

		if (!(((NULL != matchName) && (0 == J9UTF8_LENGTH(name))) || ((NULL != matchSig) && (0 == J9UTF8_LENGTH(signature))))) {
			j9array_t resultsArray = (j9array_t)J9_JNI_UNWRAP_REFERENCE(results);
			UDATA length = J9INDEXABLEOBJECT_SIZE(currentThread, resultsArray);
			UDATA index = 0;

			if (J9ROMCLASS_IS_INTERFACE(defClass->romClass)) {
				result = -1;
			} else {
				if (J9_ARE_ANY_BITS_SET(matchFlags, MN_IS_FIELD)) {
					J9ROMFieldShape *romField = NULL;
					J9ROMFieldWalkState walkState;

					UDATA classDepth = 0;
					if (J9_ARE_ANY_BITS_SET(matchFlags, MN_SEARCH_SUPERCLASSES)) {
						/* walk superclasses */
						classDepth = J9CLASS_DEPTH(defClass);
					}
					J9Class *currentClass = defClass;

					while (NULL != currentClass) {
						/* walk currentClass */
						memset(&walkState, 0, sizeof(walkState));
						romField = romFieldsStartDo(currentClass->romClass, &walkState);

						while (NULL != romField) {
							J9UTF8 *nameUTF = J9ROMFIELDSHAPE_NAME(romField);
							J9UTF8 *signatureUTF = J9ROMFIELDSHAPE_SIGNATURE(romField);

							if (((NULL == matchName) || J9UTF8_EQUALS(name, nameUTF))
							&& ((NULL == matchSig) || J9UTF8_EQUALS(signature, signatureUTF))
							) {
								if (skip > 0) {
									skip -=1;
								} else {
									if (index < length) {
										/* Refetch reference after GC point */
										resultsArray = (j9array_t)J9_JNI_UNWRAP_REFERENCE(results);
										j9object_t memberName = J9JAVAARRAYOFOBJECT_LOAD(currentThread, resultsArray, index);
										if (NULL == memberName) {
											result = -99;
											goto done;
										}
										PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, memberName);

										/* create static field object */
										j9object_t fieldObj = vm->reflectFunctions.createFieldObject(currentThread, romField, defClass, (romField->modifiers & J9AccStatic) == J9AccStatic);
										memberName = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);

										if (NULL != fieldObj) {
											initImpl(currentThread, memberName, fieldObj);
										}
										if (VM_VMHelpers::exceptionPending(currentThread)) {
											goto done;
										}
									}
								}
								result += 1;
							}
							romField = romFieldsNextDo(&walkState);
						}

						/* get the superclass */
						if (classDepth >= 1) {
							classDepth -= 1;
							currentClass = defClass->superclasses[classDepth];
						} else {
							currentClass = NULL;
						}
					}

					/* walk interfaces */
					if (J9_ARE_ANY_BITS_SET(matchFlags, MN_SEARCH_INTERFACES)) {
						J9ITable *currentITable = (J9ITable *)defClass->iTable;

						while (NULL != currentITable) {
							memset(&walkState, 0, sizeof(walkState));
							romField = romFieldsStartDo(currentITable->interfaceClass->romClass, &walkState);
							while (NULL != romField) {
								J9UTF8 *nameUTF = J9ROMFIELDSHAPE_NAME(romField);
								J9UTF8 *signatureUTF = J9ROMFIELDSHAPE_SIGNATURE(romField);

								if (((NULL == matchName) || J9UTF8_EQUALS(name, nameUTF))
								&& ((NULL == matchSig) || J9UTF8_EQUALS(signature, signatureUTF))
								) {
									if (skip > 0) {
										skip -=1;
									} else {
										if (index < length) {
											/* Refetch reference after GC point */
											resultsArray = (j9array_t)J9_JNI_UNWRAP_REFERENCE(results);
											j9object_t memberName = J9JAVAARRAYOFOBJECT_LOAD(currentThread, resultsArray, index);
											if (NULL == memberName) {
												result = -99;
												goto done;
											}
											PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, memberName);

											/* create field object */
											j9object_t fieldObj = vm->reflectFunctions.createFieldObject(currentThread, romField, defClass, (romField->modifiers & J9AccStatic) == J9AccStatic);
											memberName = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);

											if (NULL != fieldObj) {
												initImpl(currentThread, memberName, fieldObj);
											}
											if (VM_VMHelpers::exceptionPending(currentThread)) {
												goto done;
											}
										}
									}
									result += 1;
								}
								romField = romFieldsNextDo(&walkState);
							}
							currentITable = currentITable->next;
						}
					}
				} else if (J9_ARE_ANY_BITS_SET(matchFlags, MN_IS_CONSTRUCTOR | MN_IS_METHOD)) {
					UDATA classDepth = 0;
					if (J9_ARE_ANY_BITS_SET(matchFlags, MN_SEARCH_SUPERCLASSES)) {
						/* walk superclasses */
						classDepth = J9CLASS_DEPTH(defClass);
					}
					J9Class *currentClass = defClass;

					while (NULL != currentClass) {
						if (!J9ROMCLASS_IS_PRIMITIVE_OR_ARRAY(currentClass->romClass)) {
							J9Method *currentMethod = currentClass->ramMethods;
							J9Method *endOfMethods = currentMethod + currentClass->romClass->romMethodCount;
							while (currentMethod != endOfMethods) {
								J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(currentMethod);
								J9UTF8 *nameUTF = J9ROMMETHOD_SIGNATURE(romMethod);
								J9UTF8 *signatureUTF = J9ROMMETHOD_SIGNATURE(romMethod);

								if (((NULL == matchName) || J9UTF8_EQUALS(name, nameUTF))
								&& ((NULL == matchSig) || J9UTF8_EQUALS(signature, signatureUTF))
								) {
									if (skip > 0) {
										skip -=1;
									} else {
										if (index < length) {
											/* Refetch reference after GC point */
											resultsArray = (j9array_t)J9_JNI_UNWRAP_REFERENCE(results);
											j9object_t memberName = J9JAVAARRAYOFOBJECT_LOAD(currentThread, resultsArray, index);
											if (NULL == memberName) {
												result = -99;
												goto done;
											}
											PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, memberName);

											j9object_t methodObj = NULL;
											if (J9_ARE_NO_BITS_SET(romMethod->modifiers, J9AccStatic) && ('<' == (char)*J9UTF8_DATA(J9ROMMETHOD_NAME(romMethod)))) {
												/* create constructor object */
												methodObj = vm->reflectFunctions.createConstructorObject(currentMethod, currentClass, NULL, currentThread);
											} else {
												/* create method object */
												methodObj = vm->reflectFunctions.createMethodObject(currentMethod, currentClass, NULL, currentThread);
											}
											memberName = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);

											if (NULL != methodObj) {
												initImpl(currentThread, memberName, methodObj);
											}
											if (VM_VMHelpers::exceptionPending(currentThread)) {
												goto done;
											}
										}
									}
									result += 1;
								}
								currentMethod += 1;
							}
						}

						/* get the superclass */
						if (classDepth >= 1) {
							classDepth -= 1;
							currentClass = defClass->superclasses[classDepth];
						} else {
							currentClass = NULL;
						}
					}

					/* walk interfaces */
					if (J9_ARE_ANY_BITS_SET(matchFlags, MN_SEARCH_INTERFACES)) {
						J9ITable *currentITable = (J9ITable *)defClass->iTable;

						while (NULL != currentITable) {
							J9Class *currentClass = currentITable->interfaceClass;
							J9Method *currentMethod = currentClass->ramMethods;
							J9Method *endOfMethods = currentMethod + currentClass->romClass->romMethodCount;
							while (currentMethod != endOfMethods) {
								J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(currentMethod);
								J9UTF8 *nameUTF = J9ROMMETHOD_SIGNATURE(romMethod);
								J9UTF8 *signatureUTF = J9ROMMETHOD_SIGNATURE(romMethod);

								if (((NULL == matchName) || J9UTF8_EQUALS(name, nameUTF))
								&& ((NULL == matchSig) || J9UTF8_EQUALS(signature, signatureUTF))
								) {
									if (skip > 0) {
										skip -=1;
									} else {
										if (index < length) {
											/* Refetch reference after GC point */
											resultsArray = (j9array_t)J9_JNI_UNWRAP_REFERENCE(results);
											j9object_t memberName = J9JAVAARRAYOFOBJECT_LOAD(currentThread, resultsArray, index);
											if (NULL == memberName) {
												result = -99;
												goto done;
											}
											PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, memberName);

											j9object_t methodObj = NULL;
											if (J9_ARE_NO_BITS_SET(romMethod->modifiers, J9AccStatic) && ('<' == (char)*J9UTF8_DATA(J9ROMMETHOD_NAME(romMethod)))) {
												/* create constructor object */
												methodObj = vm->reflectFunctions.createConstructorObject(currentMethod, currentClass, NULL, currentThread);
											} else {
												/* create method object */
												methodObj = vm->reflectFunctions.createMethodObject(currentMethod, currentClass, NULL, currentThread);
											}
											memberName = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);

											if (NULL != methodObj) {
												initImpl(currentThread, memberName, methodObj);
											}
											if (VM_VMHelpers::exceptionPending(currentThread)) {
												goto done;
											}
										}
									}
									result += 1;
								}
								currentMethod += 1;
							}
							currentITable = currentITable->next;
						}
					}
				}
			}
		}
	}
done:
	if (name != reinterpret_cast<J9UTF8 *>(nameBuffer)) {
		j9mem_free_memory(name);
	}
	if (signature != reinterpret_cast<J9UTF8 *>(signatureBuffer)) {
		j9mem_free_memory(signature);
	}

	Trc_JCL_java_lang_invoke_MethodHandleNatives_getMembers_Exit(env, result);
	vmFuncs->internalExitVMToJNI(currentThread);
	return result;
}

/**
 * static native long objectFieldOffset(MemberName self);  // e.g., returns vmindex
 *
 * Returns the objectFieldOffset of the field represented by the MemberName
 * result should be same as if calling Unsafe.objectFieldOffset with the actual field object
 */
jlong JNICALL
Java_java_lang_invoke_MethodHandleNatives_objectFieldOffset(JNIEnv *env, jclass clazz, jobject self)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	const J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	vmFuncs->internalEnterVMFromJNI(currentThread);
	jlong result = 0;

	Trc_JCL_java_lang_invoke_MethodHandleNatives_objectFieldOffset_Entry(env, self);

	if (NULL == self) {
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		j9object_t membernameObject = J9_JNI_UNWRAP_REFERENCE(self);
		j9object_t clazzObject = J9VMJAVALANGINVOKEMEMBERNAME_CLAZZ(currentThread, membernameObject);

		if (NULL == clazzObject) {
			vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
		} else {
			jint flags = J9VMJAVALANGINVOKEMEMBERNAME_FLAGS(currentThread, membernameObject);
			if (J9_ARE_ALL_BITS_SET(flags, MN_IS_FIELD) && J9_ARE_NO_BITS_SET(flags, J9AccStatic)) {
				J9JNIFieldID *fieldID = (J9JNIFieldID*)(UDATA)J9OBJECT_U64_LOAD(currentThread, membernameObject, vm->vmindexOffset);
				result = (jlong)fieldID->offset + J9VMTHREAD_OBJECT_HEADER_SIZE(currentThread);
			} else {
				vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
			}
		}
	}

	Trc_JCL_java_lang_invoke_MethodHandleNatives_objectFieldOffset_Exit(env, result);
	vmFuncs->internalExitVMToJNI(currentThread);
	return result;
}

/**
 * static native long staticFieldOffset(MemberName self);  // e.g., returns vmindex
 *
 * Returns the staticFieldOffset of the field represented by the MemberName
 * result should be same as if calling Unsafe.staticFieldOffset with the actual field object
 */
jlong JNICALL
Java_java_lang_invoke_MethodHandleNatives_staticFieldOffset(JNIEnv *env, jclass clazz, jobject self)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	const J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	vmFuncs->internalEnterVMFromJNI(currentThread);
	jlong result = 0;

	Trc_JCL_java_lang_invoke_MethodHandleNatives_staticFieldOffset_Entry(env, self);

	if (NULL == self) {
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		j9object_t membernameObject = J9_JNI_UNWRAP_REFERENCE(self);
		j9object_t clazzObject = J9VMJAVALANGINVOKEMEMBERNAME_CLAZZ(currentThread, membernameObject);

		if (NULL == clazzObject) {
			vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
		} else {
			jint flags = J9VMJAVALANGINVOKEMEMBERNAME_FLAGS(currentThread, membernameObject);
			if (J9_ARE_ALL_BITS_SET(flags, MN_IS_FIELD & J9AccStatic)) {
				result = (jlong)(UDATA)J9OBJECT_U64_LOAD(currentThread, membernameObject, vm->vmtargetOffset);
			} else {
				vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
			}
		}
	}
	Trc_JCL_java_lang_invoke_MethodHandleNatives_staticFieldOffset_Exit(env, result);
	vmFuncs->internalExitVMToJNI(currentThread);
	return result;
}

/**
 * static native Object staticFieldBase(MemberName self);  // e.g., returns clazz
 *
 * Returns the staticFieldBase of the field represented by the MemberName
 * result should be same as if calling Unsafe.staticFieldBase with the actual field object
 */
jobject JNICALL
Java_java_lang_invoke_MethodHandleNatives_staticFieldBase(JNIEnv *env, jclass clazz, jobject self)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	const J9InternalVMFunctions *vmFuncs = currentThread->javaVM->internalVMFunctions;
	vmFuncs->internalEnterVMFromJNI(currentThread);
	jobject result = NULL;

	Trc_JCL_java_lang_invoke_MethodHandleNatives_staticFieldBase_Entry(env, self);
	if (NULL == self) {
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		j9object_t membernameObject = J9_JNI_UNWRAP_REFERENCE(self);
		j9object_t clazzObject = J9VMJAVALANGINVOKEMEMBERNAME_CLAZZ(currentThread, membernameObject);

		if (NULL == clazzObject) {
			vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
		} else {
			jint flags = J9VMJAVALANGINVOKEMEMBERNAME_FLAGS(currentThread, membernameObject);
			if (J9_ARE_ALL_BITS_SET(flags, MN_IS_FIELD & J9AccStatic)) {
				result = vmFuncs->j9jni_createLocalRef(env, clazzObject);
			} else {
				vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
			}
		}
	}
	Trc_JCL_java_lang_invoke_MethodHandleNatives_staticFieldBase_Exit(env, result);
	vmFuncs->internalExitVMToJNI(currentThread);
	return result;
}

/**
 * static native Object getMemberVMInfo(MemberName self);  // returns {vmindex,vmtarget}
 *
 * Return a 2-element java array containing the vm offset/target data
 * For a field MemberName, array contains:
 * 		(field offset, declaring class)
 * For a method MemberName, array contains:
 * 		(vtable index, MemberName object)
 */
jobject JNICALL
Java_java_lang_invoke_MethodHandleNatives_getMemberVMInfo(JNIEnv *env, jclass clazz, jobject self)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	const J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	jobject result = NULL;
	vmFuncs->internalEnterVMFromJNI(currentThread);

	Trc_JCL_java_lang_invoke_MethodHandleNatives_getMemberVMInfo_Entry(env, self);
	if (NULL != self) {
		J9Class *arrayClass = fetchArrayClass(currentThread, J9VMJAVALANGOBJECT(vm));
		j9object_t arrayObject = vm->memoryManagerFunctions->J9AllocateIndexableObject(currentThread, arrayClass, 2, J9_GC_ALLOCATE_OBJECT_INSTRUMENTABLE);
		if (NULL == arrayObject) {
			vmFuncs->setHeapOutOfMemoryError(currentThread);
		} else {
			PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, arrayObject);
			j9object_t box = vm->memoryManagerFunctions->J9AllocateObject(currentThread, J9VMJAVALANGLONG_OR_NULL(vm), J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
			if (NULL == box) {
				/* Drop arrayObject */
				DROP_OBJECT_IN_SPECIAL_FRAME(currentThread);
				vmFuncs->setHeapOutOfMemoryError(currentThread);
			} else {
				arrayObject = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);
				j9object_t membernameObject = J9_JNI_UNWRAP_REFERENCE(self);
				jint flags = J9VMJAVALANGINVOKEMEMBERNAME_FLAGS(currentThread, membernameObject);
				j9object_t target = NULL;

				/* For fields, vmindexOffset (J9JNIFieldID) is initialized using the field offset in
				 * jnicsup.cpp::getJNIFieldID. For methods, vmindexOffset already has the right value
				 * (vTable offset for MH_REF_INVOKEVIRTUAL, iTable index for MH_REF_INVOKEINTERFACE,
				 * and -1 for other ref kinds).
				 */
				jlong vmindex = (jlong)(UDATA)J9OBJECT_U64_LOAD(currentThread, membernameObject, vm->vmindexOffset);

				if (J9_ARE_ANY_BITS_SET(flags, MN_IS_FIELD)) {
					/* vmindex points to the field offset. */
					vmindex = ((J9JNIFieldID*)vmindex)->offset;
					target = J9VMJAVALANGINVOKEMEMBERNAME_CLAZZ(currentThread, membernameObject);
				} else {
					target = membernameObject;
				}

				J9VMJAVALANGLONG_SET_VALUE(currentThread, box, vmindex);
				J9JAVAARRAYOFOBJECT_STORE(currentThread, arrayObject, 0, box);
				J9JAVAARRAYOFOBJECT_STORE(currentThread, arrayObject, 1, target);

				result = vmFuncs->j9jni_createLocalRef(env, arrayObject);
			}
		}
	}
	Trc_JCL_java_lang_invoke_MethodHandleNatives_getMemberVMInfo_Exit(env, result);
	vmFuncs->internalExitVMToJNI(currentThread);
	return result;

}

/**
 * static native void setCallSiteTargetNormal(CallSite site, MethodHandle target)
 */
void JNICALL
Java_java_lang_invoke_MethodHandleNatives_setCallSiteTargetNormal(JNIEnv *env, jclass clazz, jobject callsite, jobject target)
{
	setCallSiteTargetImpl((J9VMThread*)env, callsite, target, false);
}

/**
 * static native void setCallSiteTargetVolatile(CallSite site, MethodHandle target);
 */
void JNICALL
Java_java_lang_invoke_MethodHandleNatives_setCallSiteTargetVolatile(JNIEnv *env, jclass clazz, jobject callsite, jobject target)
{
	setCallSiteTargetImpl((J9VMThread*)env, callsite, target, true);
}

#if JAVA_SPEC_VERSION >= 11
/**
 * static native void copyOutBootstrapArguments(Class<?> caller, int[] indexInfo,
 *                                              int start, int end,
 *                                              Object[] buf, int pos,
 *                                              boolean resolve,
 *                                              Object ifNotAvailable);
 */
void JNICALL
Java_java_lang_invoke_MethodHandleNatives_copyOutBootstrapArguments(
                                                                    JNIEnv *env, jclass clazz, jclass caller,
                                                                    jintArray indexInfo, jint start, jint end,
                                                                    jobjectArray buf, jint pos, jboolean resolve,
                                                                    jobject ifNotAvailable
) {
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	const J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	vmFuncs->internalEnterVMFromJNI(currentThread);

	if ((NULL == caller) || (NULL == indexInfo) || (NULL == buf)) {
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
	} else {
		J9Class *callerClass = J9VM_J9CLASS_FROM_JCLASS(currentThread, caller);
		j9array_t indexInfoArray = (j9array_t)J9_JNI_UNWRAP_REFERENCE(indexInfo);
		j9array_t bufferArray = (j9array_t)J9_JNI_UNWRAP_REFERENCE(buf);

		if ((J9INDEXABLEOBJECT_SIZE(currentThread, indexInfoArray) < 2)) {
			vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
		} else if (((start < -4) || (start > end) || (pos < 0)) || ((jint)J9INDEXABLEOBJECT_SIZE(currentThread, bufferArray) <= pos) || ((jint)J9INDEXABLEOBJECT_SIZE(currentThread, bufferArray) <= (pos + end - start))) {
			vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGLINKAGEERROR, NULL);
		} else {
			U_16 bsmArgCount = (U_16)J9JAVAARRAYOFINT_LOAD(currentThread, indexInfoArray, 0);
			U_16 cpIndex = (U_16)J9JAVAARRAYOFINT_LOAD(currentThread, indexInfoArray, 1);
			J9ROMClass *romClass = callerClass->romClass;
			U_32 * cpShapeDescription = J9ROMCLASS_CPSHAPEDESCRIPTION(romClass);
			if (J9_CP_TYPE(cpShapeDescription, cpIndex) == J9CPTYPE_CONSTANT_DYNAMIC) {
				J9ROMConstantDynamicRef *romConstantRef = (J9ROMConstantDynamicRef*)(J9_ROM_CP_FROM_ROM_CLASS(romClass) + cpIndex);
				J9SRP *callSiteData = (J9SRP *) J9ROMCLASS_CALLSITEDATA(romClass);
				U_16 *bsmIndices = (U_16 *) (callSiteData + romClass->callSiteCount);
				U_16 *bsmData = bsmIndices + romClass->callSiteCount;

				/* clear the J9DescriptionCpPrimitiveType flag with mask to get bsmIndex */
				U_32 bsmIndex = (romConstantRef->bsmIndexAndCpType >> J9DescriptionCpTypeShift) & J9DescriptionCpBsmIndexMask;
				J9ROMNameAndSignature* nameAndSig = SRP_PTR_GET(&romConstantRef->nameAndSignature, J9ROMNameAndSignature*);

				/* Walk bsmData - skip all bootstrap methods before bsmIndex */
				for (U_32 i = 0; i < bsmIndex; i++) {
					/* increment by size of bsm data plus header */
					bsmData += (bsmData[1] + 2);
				}

				U_16 bsmCPIndex = bsmData[0];
				U_16 argCount = bsmData[1];

				/* Check the argCount from indexInfo array matches actual value */
				if (bsmArgCount != argCount) {
					vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
					goto done;
				}

				bsmData += 2;

				while (start < end) {
					/* Copy the arguments between start and end to the buf array
					 *
					 * Negative start index refer to the mandatory arguments for a bootstrap method
					 * -4 -> Lookup
					 * -3 -> name (String)
					 * -2 -> signature (MethodType)
					 * -1 -> argCount of optional arguments
					 */
					j9object_t obj = NULL;
					if (start >= 0) {
						U_16 argIndex = bsmData[start];
						J9ConstantPool *ramConstantPool = J9_CP_FROM_CLASS(callerClass);
						obj = resolveRefToObject(currentThread, ramConstantPool, argIndex, (JNI_TRUE == resolve));
						if ((NULL == obj) && (JNI_TRUE != resolve) && (NULL != ifNotAvailable)) {
							obj = J9_JNI_UNWRAP_REFERENCE(ifNotAvailable);
						}
					} else if (start == -4) {
						obj = resolveRefToObject(currentThread, J9_CP_FROM_CLASS(callerClass), bsmCPIndex, true);
					} else if (start == -3) {
						J9UTF8 *name = J9ROMNAMEANDSIGNATURE_NAME(nameAndSig);
						obj = vm->memoryManagerFunctions->j9gc_createJavaLangStringWithUTFCache(currentThread, name);
					} else if (start == -2) {
						J9UTF8 *signature = J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig);
						/* Call VM Entry point to create the MethodType - Result is put into the
						* currentThread->returnValue as entry points don't "return" in the expected way
						*/
						vmFuncs->sendFromMethodDescriptorString(currentThread, signature, callerClass->classLoader, NULL);
						obj = (j9object_t)currentThread->returnValue;
					} else if (start == -1) {
						obj = vm->memoryManagerFunctions->J9AllocateObject(currentThread, J9VMJAVALANGINTEGER_OR_NULL(vm), J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
						if (NULL == obj) {
							vmFuncs->setHeapOutOfMemoryError(currentThread);
						} else {
							J9VMJAVALANGINTEGER_SET_VALUE(currentThread, obj, argCount);
						}
					}
					if (VM_VMHelpers::exceptionPending(currentThread)) {
						goto done;
					}
					/* Refetch reference after GC point */
					bufferArray = (j9array_t)J9_JNI_UNWRAP_REFERENCE(buf);
					J9JAVAARRAYOFOBJECT_STORE(currentThread, bufferArray, pos, obj);
					start += 1;
					pos += 1;
				}
			} else {
				vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGLINKAGEERROR, NULL);
			}
		}
	}
done:
	vmFuncs->internalExitVMToJNI(currentThread);
}

/**
 * private static native void clearCallSiteContext(CallSiteContext context);
 */
void JNICALL
Java_java_lang_invoke_MethodHandleNatives_clearCallSiteContext(JNIEnv *env, jclass clazz, jobject context)
{
	return;
}
#endif /* JAVA_SPEC_VERSION >= 11 */

/**
 * private static native int getNamedCon(int which, Object[] name);
 */
jint JNICALL
Java_java_lang_invoke_MethodHandleNatives_getNamedCon(JNIEnv *env, jclass clazz, jint which, jobjectArray name)
{
	return 0;
}

/**
 * private static native void registerNatives();
 */
void JNICALL
Java_java_lang_invoke_MethodHandleNatives_registerNatives(JNIEnv *env, jclass clazz)
{
	return;
}

#if JAVA_SPEC_VERSION == 8
jint JNICALL
Java_java_lang_invoke_MethodHandleNatives_getConstant(JNIEnv *env, jclass clazz, jint kind)
{
	return 0;
}
#endif /* JAVA_SPEC_VERSION == 8 */

/**
 * static native void markClassForMemberNamePruning(Class<?> c);
 *
 * Inform the VM that a MemberName belonging to class c has been collected.
 */
void JNICALL
Java_java_lang_invoke_MethodHandleNatives_markClassForMemberNamePruning(JNIEnv *env, jclass clazz, jclass c)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	vmFuncs->internalEnterVMFromJNI(currentThread);
	J9Class *j9c = J9VM_J9CLASS_FROM_JCLASS(currentThread, c);
	if (NULL == j9c) {
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		VM_AtomicSupport::bitOrU32((volatile uint32_t*)&j9c->classFlags, J9ClassNeedToPruneMemberNames);
	}
	vmFuncs->internalExitVMToJNI(currentThread);
}
#endif /* defined(J9VM_OPT_OPENJDK_METHODHANDLE) */

#if defined (J9VM_OPT_METHOD_HANDLE) || defined(J9VM_OPT_OPENJDK_METHODHANDLE)
jobject JNICALL
Java_java_lang_invoke_MethodHandle_invokeExact(JNIEnv *env, jclass ignored, jobject handle, jobject args)
{
	throwNewUnsupportedOperationException(env);
	return NULL;
}

jobject JNICALL
Java_java_lang_invoke_MethodHandle_invoke(JNIEnv *env, jclass ignored, jobject handle, jobject args)
{
	throwNewUnsupportedOperationException(env);
	return NULL;
}
#endif /* defined (J9VM_OPT_METHOD_HANDLE) || defined(J9VM_OPT_OPENJDK_METHODHANDLE) */
} /* extern "C" */
