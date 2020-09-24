/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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

#define MN_IS_METHOD		0x00010000
#define MN_IS_CONSTRUCTOR	0x00020000
#define MN_IS_FIELD			0x00040000
#define MN_IS_TYPE			0x00080000
#define MN_CALLER_SENSITIVE	0x00100000

#define MN_REFERENCE_KIND_SHIFT	24
#define MN_REFERENCE_KIND_MASK	0xF

#define MN_SEARCH_SUPERCLASSES	0x00100000
#define MN_SEARCH_INTERFACES	0x00200000

void
initImpl(J9VMThread *currentThread, j9object_t membernameObject, j9object_t refObject)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;

	J9Class* refClass = J9OBJECT_CLAZZ(currentThread, refObject);

	jint flags = 0;
	jlong vmindex = 0;
	jlong target = 0;
	j9object_t clazzObject = NULL;
	j9object_t nameObject = NULL;
	j9object_t typeObject = NULL;

	if (refClass == J9VMJAVALANGREFLECTFIELD(vm)) {
		J9JNIFieldID *fieldID = vm->reflectFunctions.idFromFieldObject(currentThread, NULL, refObject);
		vmindex = (jlong)fieldID;
		target = (jlong)fieldID->field;

		flags = fieldID->field->modifiers & CFR_FIELD_ACCESS_MASK;
		flags |= MN_IS_FIELD;
		flags |= (J9_ARE_ANY_BITS_SET(flags, J9AccStatic) ? MH_REF_GETSTATIC : MH_REF_GETFIELD) << MN_REFERENCE_KIND_SHIFT;
		J9UTF8 *name = J9ROMFIELDSHAPE_NAME(fieldID->field);
		J9UTF8 *signature = J9ROMFIELDSHAPE_SIGNATURE(fieldID->field);

		nameObject = vm->memoryManagerFunctions->j9gc_createJavaLangString(currentThread, J9UTF8_DATA(name), (U_32)J9UTF8_LENGTH(name), J9_STR_INTERN);
		typeObject = vm->memoryManagerFunctions->j9gc_createJavaLangString(currentThread, J9UTF8_DATA(signature), (U_32)J9UTF8_LENGTH(signature), J9_STR_INTERN);

		clazzObject = J9VM_J9CLASS_TO_HEAPCLASS(fieldID->declaringClass);

		J9VMJAVALANGINVOKEMEMBERNAME_SET_NAME(currentThread, membernameObject, nameObject);
		J9VMJAVALANGINVOKEMEMBERNAME_SET_TYPE(currentThread, membernameObject, typeObject);
		Trc_JCL_java_lang_invoke_MethodHandleNatives_initImpl_setField(currentThread, (U_32)J9UTF8_LENGTH(name), J9UTF8_DATA(name), (U_32)J9UTF8_LENGTH(signature), J9UTF8_DATA(signature));
	} else if (refClass == J9VMJAVALANGREFLECTMETHOD(vm)) {
		J9JNIMethodID *methodID = vm->reflectFunctions.idFromMethodObject(currentThread, refObject);
		vmindex = (jlong)methodID;
		target = (jlong)methodID->method;

		J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(methodID->method);

		flags = romMethod->modifiers & CFR_METHOD_ACCESS_MASK;
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

		clazzObject = J9VMJAVALANGREFLECTMETHOD_DECLARINGCLASS(currentThread, refObject);
	} else if (refClass == J9VMJAVALANGREFLECTCONSTRUCTOR(vm)) {
		J9JNIMethodID *methodID = vm->reflectFunctions.idFromConstructorObject(currentThread, refObject);
		vmindex = (jlong)methodID;
		target = (jlong)methodID->method;

		J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(methodID->method);

		flags = romMethod->modifiers & CFR_METHOD_ACCESS_MASK;
		flags |= MN_IS_CONSTRUCTOR | (MH_REF_INVOKESPECIAL << MN_REFERENCE_KIND_SHIFT);

		clazzObject = J9VMJAVALANGREFLECTMETHOD_DECLARINGCLASS(currentThread, refObject);
	} else {
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
	}

	if (!VM_VMHelpers::exceptionPending(currentThread)) {
		J9VMJAVALANGINVOKEMEMBERNAME_SET_FLAGS(currentThread, membernameObject, flags);
		J9VMJAVALANGINVOKEMEMBERNAME_SET_CLAZZ(currentThread, membernameObject, clazzObject);
		J9OBJECT_U64_STORE(currentThread, membernameObject, vm->vmindexOffset, (U_64)vmindex);
		J9OBJECT_U64_STORE(currentThread, membernameObject, vm->vmtargetOffset, (U_64)target);
		Trc_JCL_java_lang_invoke_MethodHandleNatives_initImpl_setData(currentThread, flags, clazzObject, vmindex, target);
	}
}

static char *
sigForPrimitiveOrVoid(J9JavaVM *vm, J9Class *clazz)
{
	char result = 0;
	if (clazz == vm->booleanReflectClass)
		result = 'Z';
	else if (clazz == vm->byteReflectClass)
		result = 'B';
	else if (clazz == vm->charReflectClass)
		result = 'C';
	else if (clazz == vm->shortReflectClass)
		result = 'S';
	else if (clazz == vm->intReflectClass)
		result = 'I';
	else if (clazz == vm->longReflectClass)
		result = 'J';
	else if (clazz == vm->floatReflectClass)
		result = 'F';
	else if (clazz == vm->doubleReflectClass)
		result = 'D';
	else if (clazz == vm->voidReflectClass)
		result = 'V';

	if (result) {
		PORT_ACCESS_FROM_JAVAVM(vm);
		char* signature = (char*)j9mem_allocate_memory(2 * sizeof(char*), OMRMEM_CATEGORY_VM);
		signature[0] = result;
		signature[1] = '\0';
		return signature;
	}
	return NULL;
}

char *
getClassSignature(J9VMThread *currentThread, J9Class * clazz)
{
	PORT_ACCESS_FROM_JAVAVM(currentThread->javaVM);

	U_32 numDims = 0;

	J9Class * myClass = clazz;
	while (J9ROMCLASS_IS_ARRAY(myClass->romClass))
	{
		J9Class * componentClass = (J9Class *)(((J9ArrayClass*)myClass)->componentType);
		if (J9ROMCLASS_IS_PRIMITIVE_TYPE(componentClass->romClass)) {
			break;
		}
		numDims++;
		myClass = componentClass;
	}
	J9UTF8 * romName = J9ROMCLASS_CLASSNAME(myClass->romClass);
	U_32 len = J9UTF8_LENGTH(romName);
	char * name =  (char *)J9UTF8_DATA(romName);
	U_32 length = len + numDims;
	if (* name != '[')
		length += 2;

	length++; //for null-termination
	char * sig = (char *)j9mem_allocate_memory(length, OMRMEM_CATEGORY_VM);
	U_32 i;
	for (i = 0; i < numDims; i++)
		sig[i] = '[';
	if (*name != '[')
		sig[i++] = 'L';
	memcpy(sig+i, name, len);
	i += len;
	if (*name != '[')
		sig[i++] = ';';

	sig[length-1] = '\0';
	return sig;
}

char *
getSignatureFromMethodType(J9VMThread *currentThread, j9object_t typeObject, UDATA *signatureLength)
{
	J9JavaVM *vm = currentThread->javaVM;
	j9object_t ptypes = J9VMJAVALANGINVOKEMETHODTYPE_PTYPES(currentThread, typeObject);
	U_32 numArgs = J9INDEXABLEOBJECT_SIZE(currentThread, ptypes);

	PORT_ACCESS_FROM_JAVAVM(vm);

	char** signatures = (char**)j9mem_allocate_memory((numArgs + 1) * sizeof(char*), OMRMEM_CATEGORY_VM);
	*signatureLength = 2; //space for '(', ')'
	for (U_32 i = 0; i < numArgs; i++) {
		j9object_t pObject = J9JAVAARRAYOFOBJECT_LOAD(currentThread, ptypes, i);
		J9Class *pclass = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, pObject);
		signatures[i] = sigForPrimitiveOrVoid(vm, pclass);
		if (!signatures[i])
			signatures[i] = getClassSignature(currentThread, pclass);

		*signatureLength += strlen(signatures[i]);
	}

	// Return type
	j9object_t rtype = J9VMJAVALANGINVOKEMETHODTYPE_RTYPE(currentThread, typeObject);
	J9Class *rclass = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, rtype);
	char* rSignature = sigForPrimitiveOrVoid(vm, rclass);
	if (!rSignature)
		rSignature = getClassSignature(currentThread, rclass);

	*signatureLength += strlen(rSignature);

	char* methodDescriptor = (char*)j9mem_allocate_memory(*signatureLength+1, OMRMEM_CATEGORY_VM);
	char* cursor = methodDescriptor;
	*cursor++ = '(';

	// Copy class signatures to descriptor string
	for (U_32 i = 0; i < numArgs; i++) {
		U_32 len = strlen(signatures[i]);
		strncpy(cursor, signatures[i], len);
		cursor += len;
		j9mem_free_memory(signatures[i]);
	}

	*cursor++ = ')';
	// Copy return type signature to descriptor string
	strcpy(cursor, rSignature);
	j9mem_free_memory(signatures);
	return methodDescriptor;
}

static J9Class *
classForSignature(struct J9VMThread *vmThread, U_8 **sigDataPtr, struct J9ClassLoader *classLoader)
{
	U_8 *sigData = *sigDataPtr;
	J9JavaVM *vm = vmThread->javaVM;
	J9Class *clazz = NULL;
	UDATA arity = 0;
	U_8 c = 0;
	UDATA i = 0;

	for (c = *sigData++; '[' == c; c = *sigData++) {
		arity++;
	}

	/* Non-array case */
	switch (c) {
	case 'Q':
	case 'L': {
		/* object case */
		U_8 *tempData = sigData;
		UDATA length = 0;

		/* find the signature length -> up to the ";" */
		while (';' != *sigData++) {
			length++;
		}

		/* find the class from the signature chunk */
		clazz = vm->internalVMFunctions->internalFindClassUTF8(vmThread, tempData, length, classLoader, J9_FINDCLASS_FLAG_THROW_ON_FAIL);
		break;
	}
	case 'I':
		clazz = vm->intReflectClass;
		break;
	case 'Z':
		clazz = vm->booleanReflectClass;
		break;
	case 'J':
		clazz = vm->longReflectClass;
		break;
#if defined(J9VM_INTERP_FLOAT_SUPPORT)
	case 'D':
		clazz = vm->doubleReflectClass;
		break;
	case 'F':
		clazz = vm->floatReflectClass;
		break;
#endif
	case 'C':
		clazz = vm->charReflectClass;
		break;
	case 'B':
		clazz = vm->byteReflectClass;
		break;
	case 'S':
		clazz = vm->shortReflectClass;
		break;
	case 'V':
		clazz = vm->voidReflectClass;
		break;
	}

	for (i = 0; (i < arity) && (NULL != clazz); i++) {
		clazz = fetchArrayClass(vmThread, clazz);
	}

	if (NULL != clazz) {
		*sigDataPtr = sigData;
	}

	return clazz;
}

static j9object_t
createFieldFromID(struct J9VMThread *vmThread, J9JNIFieldID *j9FieldID)
{
	J9UTF8 *nameUTF = NULL;
	j9object_t nameString = NULL;
	U_8 *signatureData = NULL;
	J9Class *typeClass = NULL;
	j9object_t fieldObject = NULL;
	J9Class *jlrFieldClass = J9VMJAVALANGREFLECTFIELD(vmThread->javaVM);
	UDATA initStatus;

	if (NULL == jlrFieldClass) {
		return NULL;
	}

	initStatus = jlrFieldClass->initializeStatus;
	if (initStatus != J9ClassInitSucceeded && initStatus != (UDATA) vmThread) {
		vmThread->javaVM->internalVMFunctions->initializeClass(vmThread, jlrFieldClass);
		if (vmThread->currentException != NULL) {
			return NULL;
		}
	}

	fieldObject = vmThread->javaVM->memoryManagerFunctions->J9AllocateObject(vmThread, jlrFieldClass, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
	if (NULL == fieldObject) {
		vmThread->javaVM->internalVMFunctions->setHeapOutOfMemoryError(vmThread);
		return NULL;
	}

	PUSH_OBJECT_IN_SPECIAL_FRAME(vmThread, fieldObject);

	signatureData = J9UTF8_DATA(J9ROMFIELDSHAPE_SIGNATURE(j9FieldID->field));
	typeClass = classForSignature(vmThread, &signatureData, j9FieldID->declaringClass->classLoader);
	if (NULL == typeClass) {
		DROP_OBJECT_IN_SPECIAL_FRAME(vmThread); /* fieldObject */
		return NULL;
	}

	fieldObject = PEEK_OBJECT_IN_SPECIAL_FRAME(vmThread, 0);
	J9VMJAVALANGREFLECTFIELD_SET_TYPE(vmThread, fieldObject, J9VM_J9CLASS_TO_HEAPCLASS(typeClass));

	nameUTF = J9ROMFIELDSHAPE_NAME(j9FieldID->field);
	nameString = vmThread->javaVM->memoryManagerFunctions->j9gc_createJavaLangString(vmThread, J9UTF8_DATA(nameUTF), (U_32) J9UTF8_LENGTH(nameUTF), J9_STR_INTERN);
	if (NULL == nameString) {
		DROP_OBJECT_IN_SPECIAL_FRAME(vmThread); /* fieldObject */
		return NULL;
	}

	fieldObject = PEEK_OBJECT_IN_SPECIAL_FRAME(vmThread, 0);
	J9VMJAVALANGREFLECTFIELD_SET_NAME(vmThread, fieldObject, nameString);

	if (0 != (j9FieldID->field->modifiers & J9FieldFlagHasGenericSignature)) {
		J9UTF8 *sigUTF = romFieldGenericSignature(j9FieldID->field);
		j9object_t sigString = vmThread->javaVM->memoryManagerFunctions->j9gc_createJavaLangString(vmThread, J9UTF8_DATA(sigUTF), (U_32) J9UTF8_LENGTH(sigUTF), 0);
		if (NULL == sigString) {
			DROP_OBJECT_IN_SPECIAL_FRAME(vmThread); /* fieldObject */
			return NULL;
		}

		fieldObject = PEEK_OBJECT_IN_SPECIAL_FRAME(vmThread, 0);
#if defined(USE_SUN_REFLECT)
		J9VMJAVALANGREFLECTFIELD_SET_SIGNATURE(vmThread, fieldObject, sigString);
#endif
	}

	{
		j9object_t byteArray = getFieldAnnotationData(vmThread, j9FieldID->declaringClass, j9FieldID);
		if (NULL != vmThread->currentException) {
			DROP_OBJECT_IN_SPECIAL_FRAME(vmThread); /* fieldObject */
			return NULL;
		}
		if (NULL != byteArray) {
			fieldObject = PEEK_OBJECT_IN_SPECIAL_FRAME(vmThread, 0);
			J9VMJAVALANGREFLECTFIELD_SET_ANNOTATIONS(vmThread, fieldObject, byteArray);
		}
	}

	fieldObject = POP_OBJECT_IN_SPECIAL_FRAME(vmThread);

	/* Java 7 uses int field ID's to avoid modifying Sun JCL code */
	J9VMJAVALANGREFLECTFIELD_SET_INTFIELDID(vmThread, fieldObject, (U_32)(j9FieldID->index));

	J9VMJAVALANGREFLECTFIELD_SET_DECLARINGCLASS(vmThread, fieldObject, J9VM_J9CLASS_TO_HEAPCLASS(j9FieldID->declaringClass));
#if defined(USE_SUN_REFLECT)
	J9VMJAVALANGREFLECTFIELD_SET_MODIFIERS(vmThread, fieldObject, j9FieldID->field->modifiers & CFR_FIELD_ACCESS_MASK);
#endif

	return fieldObject;
}

j9object_t
createFieldObject(J9VMThread *currentThread, J9ROMFieldShape *romField, J9Class *declaringClass, J9UTF8 *name, J9UTF8 *sig, bool isStaticField)
{
	J9InternalVMFunctions *vmFuncs = currentThread->javaVM->internalVMFunctions;

	J9JNIFieldID *fieldID = NULL;
	UDATA offset = 0;
	UDATA inconsistentData = 0;

	if (isStaticField) {
		offset = (UDATA)vmFuncs->staticFieldAddress(currentThread, declaringClass, J9UTF8_DATA(name), J9UTF8_LENGTH(name), J9UTF8_DATA(sig), J9UTF8_LENGTH(sig), NULL, NULL, 0, NULL);
		offset -= (UDATA)declaringClass->ramStatics;
	} else {
		offset = vmFuncs->instanceFieldOffset(currentThread, declaringClass, J9UTF8_DATA(name), J9UTF8_LENGTH(name), J9UTF8_DATA(sig), J9UTF8_LENGTH(sig), NULL, NULL, 0);
	}

	fieldID = vmFuncs->getJNIFieldID(currentThread, declaringClass, romField, offset, &inconsistentData);

	return createFieldFromID(currentThread, fieldID);
}

j9object_t
resolveRefToObject(J9VMThread *currentThread, J9ConstantPool *ramConstantPool, U_16 cpIndex, bool resolve)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;

	j9object_t result = NULL;

	J9RAMSingleSlotConstantRef *ramCP = (J9RAMSingleSlotConstantRef*)ramConstantPool + cpIndex;
	U_32 *cpShapeDescription = J9ROMCLASS_CPSHAPEDESCRIPTION(J9_CLASS_FROM_CP(ramConstantPool)->romClass);

	switch (J9_CP_TYPE(cpShapeDescription, cpIndex)) {
		case J9CPTYPE_CLASS:
		{
			J9Class *clazz = (J9Class*)ramCP->value;
			if ((NULL == clazz) && resolve) {
				clazz = vmFuncs->resolveClassRef(currentThread, ramConstantPool, cpIndex, J9_RESOLVE_FLAG_RUNTIME_RESOLVE);
			}
			if (NULL != clazz) {
				result = J9VM_J9CLASS_TO_HEAPCLASS(clazz);
			}
			break;
		}
		case J9CPTYPE_STRING:
		{
			result = (j9object_t)ramCP->value;
			if ((NULL == result) && resolve) {
				result = vmFuncs->resolveStringRef(currentThread, ramConstantPool, cpIndex, J9_RESOLVE_FLAG_RUNTIME_RESOLVE);
			}
			break;
		}
		case J9CPTYPE_INT:
		{
			result = vm->memoryManagerFunctions->J9AllocateObject(currentThread, J9VMJAVALANGINTEGER_OR_NULL(vm), J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
			J9VMJAVALANGINTEGER_SET_VALUE(currentThread, result, ramCP->value);
			break;
		}
		case J9CPTYPE_FLOAT:
		{
			result = vm->memoryManagerFunctions->J9AllocateObject(currentThread, J9VMJAVALANGFLOAT_OR_NULL(vm), J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
			J9VMJAVALANGFLOAT_SET_VALUE(currentThread, result, ramCP->value);
			break;
		}
		case J9CPTYPE_LONG:
		{
			J9ROMConstantRef *romCP = (J9ROMConstantRef*)J9_ROM_CP_FROM_CP(ramConstantPool) + cpIndex;
			U_64 value = romCP->slot1;
			value = (value << 32) + romCP->slot2;
			result = vm->memoryManagerFunctions->J9AllocateObject(currentThread, J9VMJAVALANGLONG_OR_NULL(vm), J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
			J9VMJAVALANGLONG_SET_VALUE(currentThread, result, value);
			break;
		}
		case J9CPTYPE_DOUBLE:
		{
			J9ROMConstantRef *romCP = (J9ROMConstantRef*)J9_ROM_CP_FROM_CP(ramConstantPool) + cpIndex;
			U_64 value = romCP->slot1;
			value = (value << 32) + romCP->slot2;
			result = vm->memoryManagerFunctions->J9AllocateObject(currentThread, J9VMJAVALANGDOUBLE_OR_NULL(vm), J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
			J9VMJAVALANGDOUBLE_SET_VALUE(currentThread, result, value);
			break;
		}
		case J9CPTYPE_METHOD_TYPE:
		{
			result = (j9object_t)ramCP->value;
			if ((NULL == result) && resolve) {
				result = vmFuncs->resolveMethodTypeRef(currentThread, ramConstantPool, cpIndex, J9_RESOLVE_FLAG_RUNTIME_RESOLVE);
			}
			break;
		}
		case J9CPTYPE_METHODHANDLE:
		{
			result = (j9object_t)ramCP->value;
			if ((NULL == result) && resolve) {
				result = vmFuncs->resolveMethodHandleRef(currentThread, ramConstantPool, cpIndex, J9_RESOLVE_FLAG_RUNTIME_RESOLVE | J9_RESOLVE_FLAG_NO_CLASS_INIT);
			}
			break;
		}
		case J9CPTYPE_CONSTANT_DYNAMIC:
		{
			result = (j9object_t)ramCP->value;
			if ((NULL == result) && resolve) {
				result = vmFuncs->resolveConstantDynamic(currentThread, ramConstantPool, cpIndex, J9_RESOLVE_FLAG_RUNTIME_RESOLVE);
			}
			break;
		}
	}

	return result;
}

J9Method *
lookupMethod(J9VMThread *currentThread, J9Class *resolvedClass, J9JNINameAndSignature *nas, J9Class *callerClass, UDATA lookupOptions)
{
	J9Method *result = NULL;
	if (resolvedClass == J9VMJAVALANGINVOKEMETHODHANDLE(currentThread->javaVM)) {
		if ((0 == strcmp(nas->name, "linkToVirtual"))
		|| (0 == strcmp(nas->name, "linkToStatic"))
		|| (0 == strcmp(nas->name, "linkToSpecial"))
		|| (0 == strcmp(nas->name, "linkToInterface"))
		|| (0 == strcmp(nas->name, "invokeBasic"))
		) {
			nas->signature = NULL;
			nas->signatureLength = 0;

			/* Set flag for partial signature lookup. Signature length is already initialized to 0. */
			lookupOptions |= J9_LOOK_PARTIAL_SIGNATURE;
		}
	}

	result = (J9Method*)currentThread->javaVM->internalVMFunctions->javaLookupMethod(currentThread, resolvedClass, (J9ROMNameAndSignature*)nas, callerClass, lookupOptions);

	return result;
}


/**
 * static native void init(MemberName self, Object ref);
 */
void JNICALL
Java_java_lang_invoke_MethodHandleNatives_init(JNIEnv *env, jclass clazz, jobject self, jobject ref)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	vmFuncs->internalEnterVMFromJNI(currentThread);

	Trc_JCL_java_lang_invoke_MethodHandleNatives_init_Entry(env, self, ref);
	if (NULL == self || NULL == ref) {
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
 */
void JNICALL
Java_java_lang_invoke_MethodHandleNatives_expand(JNIEnv *env, jclass clazz, jobject self)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	vmFuncs->internalEnterVMFromJNI(currentThread);

	Trc_JCL_java_lang_invoke_MethodHandleNatives_expand_Entry(env, self);
	if (NULL == self) {
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		j9object_t membernameObject = J9_JNI_UNWRAP_REFERENCE(self);
		j9object_t clazzObject = J9VMJAVALANGINVOKEMEMBERNAME_CLAZZ(currentThread, membernameObject);
		j9object_t nameObject = J9VMJAVALANGINVOKEMEMBERNAME_NAME(currentThread, membernameObject);
		j9object_t typeObject = J9VMJAVALANGINVOKEMEMBERNAME_TYPE(currentThread, membernameObject);
		jint flags = J9VMJAVALANGINVOKEMEMBERNAME_FLAGS(currentThread, membernameObject);
		jlong vmindex = (jlong)J9OBJECT_ADDRESS_LOAD(currentThread, membernameObject, vm->vmindexOffset);

		Trc_JCL_java_lang_invoke_MethodHandleNatives_expand_Data(env, membernameObject, clazzObject, nameObject, typeObject, flags, vmindex);
		if (J9_ARE_ANY_BITS_SET(flags, MN_IS_FIELD)) {
			if ((NULL != clazzObject) && (NULL != (void*)vmindex)) {
				// J9Class *declaringClass = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, clazzObject);

				J9JNIFieldID *field = (J9JNIFieldID*)vmindex;
				J9UTF8 *name = J9ROMFIELDSHAPE_NAME(field->field);
				J9UTF8 *signature = J9ROMFIELDSHAPE_SIGNATURE(field->field);

				if (nameObject == NULL) {
					j9object_t nameString = vm->memoryManagerFunctions->j9gc_createJavaLangString(currentThread, J9UTF8_DATA(name), (U_32)J9UTF8_LENGTH(name), J9_STR_INTERN);
					J9VMJAVALANGINVOKEMEMBERNAME_SET_NAME(currentThread, membernameObject, nameString);
				}
				if (typeObject == NULL) {
					j9object_t signatureString = vm->memoryManagerFunctions->j9gc_createJavaLangString(currentThread, J9UTF8_DATA(signature), (U_32)J9UTF8_LENGTH(signature), J9_STR_INTERN);
					J9VMJAVALANGINVOKEMEMBERNAME_SET_TYPE(currentThread, membernameObject, signatureString);
				}
			} else {
				vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGILLEGALARGUMENTEXCEPTION, NULL);
			}
		} else if (J9_ARE_ANY_BITS_SET(flags, MN_IS_METHOD | MN_IS_CONSTRUCTOR)) {
			if (NULL != (void*)vmindex) {
				J9JNIMethodID *methodID = (J9JNIMethodID*)vmindex;
				J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(methodID->method);

				if (clazzObject == NULL) {
					j9object_t newClassObject = J9VM_J9CLASS_TO_HEAPCLASS(J9_CLASS_FROM_METHOD(methodID->method));
					J9VMJAVALANGINVOKEMEMBERNAME_SET_CLAZZ(currentThread, membernameObject, newClassObject);
				}
				if (nameObject == NULL) {
					J9UTF8 *name = J9ROMMETHOD_NAME(romMethod);
					j9object_t nameString = vm->memoryManagerFunctions->j9gc_createJavaLangString(currentThread, J9UTF8_DATA(name), (U_32)J9UTF8_LENGTH(name), J9_STR_INTERN);
					J9VMJAVALANGINVOKEMEMBERNAME_SET_NAME(currentThread, membernameObject, nameString);
				}
				if (typeObject == NULL) {
					J9UTF8 *signature = J9ROMMETHOD_SIGNATURE(romMethod);
					j9object_t signatureString = vm->memoryManagerFunctions->j9gc_createJavaLangString(currentThread, J9UTF8_DATA(signature), (U_32)J9UTF8_LENGTH(signature), J9_STR_INTERN);
					J9VMJAVALANGINVOKEMEMBERNAME_SET_TYPE(currentThread, membernameObject, signatureString);
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
 * static native MemberName resolve(MemberName self, Class<?> caller,
 *      boolean speculativeResolve) throws LinkageError, ClassNotFoundException;
 */
jobject JNICALL
Java_java_lang_invoke_MethodHandleNatives_resolve(JNIEnv *env, jclass clazz, jobject self, jclass caller, jboolean speculativeResolve)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	jobject result = NULL;
	vmFuncs->internalEnterVMFromJNI(currentThread);

	Trc_JCL_java_lang_invoke_MethodHandleNatives_resolve_Entry(env, self, caller, (speculativeResolve ? "true" : "false"));
	if (NULL == self) {
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
	} else {
		j9object_t callerObject = (NULL == caller) ? NULL : J9_JNI_UNWRAP_REFERENCE(caller);
		j9object_t membernameObject = J9_JNI_UNWRAP_REFERENCE(self);
		j9object_t clazzObject = J9VMJAVALANGINVOKEMEMBERNAME_CLAZZ(currentThread, membernameObject);
		j9object_t nameObject = J9VMJAVALANGINVOKEMEMBERNAME_NAME(currentThread, membernameObject);
		j9object_t typeObject = J9VMJAVALANGINVOKEMEMBERNAME_TYPE(currentThread, membernameObject);
		jlong vmindex = (jlong)(UDATA)J9OBJECT_U64_LOAD(currentThread, membernameObject, vm->vmindexOffset);
		jlong target = (jlong)(UDATA)J9OBJECT_U64_LOAD(currentThread, membernameObject, vm->vmtargetOffset);

		jint flags = J9VMJAVALANGINVOKEMEMBERNAME_FLAGS(currentThread, membernameObject);
		jint new_flags = 0;
		j9object_t new_clazz = NULL;

		Trc_JCL_java_lang_invoke_MethodHandleNatives_resolve_Data(env, membernameObject, clazzObject, callerObject, nameObject, typeObject, flags, vmindex, target);
		if (0 != target) {
			result = self;
		} else {
			J9Class *resolvedClass = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, clazzObject);
			J9Class *callerClass = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, callerObject);

			int ref_kind = (flags >> MN_REFERENCE_KIND_SHIFT) & MN_REFERENCE_KIND_MASK;

			PORT_ACCESS_FROM_JAVAVM(vm);

			UDATA nameLength = vmFuncs->getStringUTF8Length(currentThread, nameObject) + 1;
			char *name = (char*)j9mem_allocate_memory(nameLength, OMRMEM_CATEGORY_VM);
			UDATA signatureLength = 0;
			char *signature = NULL;
			J9Class *typeClass = J9OBJECT_CLAZZ(currentThread, typeObject);
			if (J9VMJAVALANGINVOKEMETHODTYPE(vm) == typeClass) {
				j9object_t sigString = J9VMJAVALANGINVOKEMETHODTYPE_METHODDESCRIPTOR(currentThread, typeObject);
				if (NULL != sigString) {
					signatureLength = vmFuncs->getStringUTF8Length(currentThread, sigString) + 1;
					signature = (char*)j9mem_allocate_memory(signatureLength, OMRMEM_CATEGORY_VM);
					vmFuncs->copyStringToUTF8Helper(currentThread, sigString, J9_STR_NULL_TERMINATE_RESULT | J9_STR_XLAT , 0, J9VMJAVALANGSTRING_LENGTH(currentThread, sigString), (U_8 *)signature, signatureLength);
				} else {
					signature = getSignatureFromMethodType(currentThread, typeObject, &signatureLength);
				}
			} else if (J9VMJAVALANGSTRING_OR_NULL(vm) == typeClass) {
				signatureLength = vmFuncs->getStringUTF8Length(currentThread, typeObject) + 1;
				signature = (char*)j9mem_allocate_memory(signatureLength, OMRMEM_CATEGORY_VM);
				UDATA stringLength = J9VMJAVALANGSTRING_LENGTH(currentThread, typeObject);
				vmFuncs->copyStringToUTF8Helper(currentThread, typeObject, J9_STR_NULL_TERMINATE_RESULT | J9_STR_XLAT , 0, stringLength, (U_8 *)signature, signatureLength);
			} else if (J9VMJAVALANGCLASS(vm) == typeClass) {
				J9Class *rclass = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, typeObject);
				signature = sigForPrimitiveOrVoid(vm, rclass);
				if (!signature) {
					signature = getClassSignature(currentThread, rclass);
				}

				signatureLength = strlen(signature);
			} else {
				vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
				vmFuncs->internalExitVMToJNI(currentThread);
				return NULL;
			}
			vmFuncs->copyStringToUTF8Helper(currentThread, nameObject, J9_STR_NULL_TERMINATE_RESULT | J9_STR_XLAT , 0, J9VMJAVALANGSTRING_LENGTH(currentThread, nameObject), (U_8 *)name, nameLength);

			Trc_JCL_java_lang_invoke_MethodHandleNatives_resolve_NAS(env, name, signature);

			if (J9_ARE_ANY_BITS_SET(flags, MN_IS_METHOD | MN_IS_CONSTRUCTOR)) {
				J9JNINameAndSignature nas;
				UDATA lookupOptions = J9_LOOK_JNI;

				if (JNI_TRUE == speculativeResolve) {
					lookupOptions |= J9_LOOK_NO_THROW;
				}
				switch (ref_kind)
				{
					case MH_REF_INVOKEINTERFACE:
						lookupOptions |= J9_LOOK_INTERFACE;
						break;
					case MH_REF_INVOKESPECIAL:
						lookupOptions |= (J9_LOOK_VIRTUAL | J9_LOOK_ALLOW_FWD | J9_LOOK_HANDLE_DEFAULT_METHOD_CONFLICTS);
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

				nas.name = name;
				nas.signature = signature;
				nas.nameLength = (U_32)strlen(name);
				nas.signatureLength = (U_32)strlen(signature);

				/* Check if signature polymorphic native calls */
				J9Method *method = lookupMethod(currentThread, resolvedClass, &nas, callerClass, lookupOptions);

				if (NULL != method) {
					J9JNIMethodID *methodID = vmFuncs->getJNIMethodID(currentThread, method);
					vmindex = (jlong)(UDATA)methodID;
					target = (jlong)(UDATA)method;

					new_clazz = J9VM_J9CLASS_TO_HEAPCLASS(J9_CLASS_FROM_METHOD(method));

					J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(methodID->method);
					new_flags = flags | (romMethod->modifiers & CFR_METHOD_ACCESS_MASK);
				}
			} if (J9_ARE_ANY_BITS_SET(flags, MN_IS_FIELD)) {
				J9Class *declaringClass;
				J9ROMFieldShape *romField;
				UDATA lookupOptions = 0;
				UDATA offset = 0;
				if (JNI_TRUE == speculativeResolve) {
					lookupOptions |= J9_RESOLVE_FLAG_NO_THROW_ON_FAIL;
				}

				offset = vmFuncs->instanceFieldOffset(currentThread,
					resolvedClass,
					(U_8*)name, strlen(name),
					(U_8*)signature, strlen(signature),
					&declaringClass, (UDATA*)&romField,
					lookupOptions);

				if (offset == (UDATA)-1) {
					declaringClass = NULL;

					if (VM_VMHelpers::exceptionPending(currentThread)) {
						VM_VMHelpers::clearException(currentThread);
					}

					void* fieldAddress = vmFuncs->staticFieldAddress(currentThread,
						resolvedClass,
						(U_8*)name, strlen(name),
						(U_8*)signature, strlen(signature),
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
					UDATA inconsistentData = 0;
					J9JNIFieldID *fieldID = vmFuncs->getJNIFieldID(currentThread, declaringClass, romField, offset, &inconsistentData);
					vmindex = (jlong)(UDATA)fieldID;

					new_clazz = J9VM_J9CLASS_TO_HEAPCLASS(declaringClass);
					new_flags = MN_IS_FIELD | (fieldID->field->modifiers & CFR_FIELD_ACCESS_MASK);
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

			if ((0 != vmindex) && (0 != target)) {
				membernameObject = J9_JNI_UNWRAP_REFERENCE(self);
				J9VMJAVALANGINVOKEMEMBERNAME_SET_FLAGS(currentThread, membernameObject, new_flags);
				J9VMJAVALANGINVOKEMEMBERNAME_SET_CLAZZ(currentThread, membernameObject, new_clazz);
				J9OBJECT_U64_STORE(currentThread, membernameObject, vm->vmindexOffset, (U_64)vmindex);
				J9OBJECT_U64_STORE(currentThread, membernameObject, vm->vmtargetOffset, (U_64)target);

				Trc_JCL_java_lang_invoke_MethodHandleNatives_resolve_resolved(env, vmindex, target, new_clazz, flags);

				result = vmFuncs->j9jni_createLocalRef(env, membernameObject);
			}
			j9mem_free_memory(name);
			j9mem_free_memory(signature);

			if ((NULL == result) && (JNI_TRUE != speculativeResolve) && !VM_VMHelpers::exceptionPending(currentThread)) {
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
	Trc_JCL_java_lang_invoke_MethodHandleNatives_resolve_Exit(env);
	vmFuncs->internalExitVMToJNI(currentThread);
	return result;
}

/**
 * static native int getMembers(Class<?> defc, String matchName, String matchSig,
 *      int matchFlags, Class<?> caller, int skip, MemberName[] results);
 */
jint JNICALL
Java_java_lang_invoke_MethodHandleNatives_getMembers(JNIEnv *env, jclass clazz, jclass defc, jstring matchName, jstring matchSig, jint matchFlags, jclass caller, jint skip, jobjectArray results)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	vmFuncs->internalEnterVMFromJNI(currentThread);
	jint result = 0;

	Trc_JCL_java_lang_invoke_MethodHandleNatives_getMembers_Entry(env, defc, matchName, matchSig, matchFlags, caller, skip, results);

	if ((NULL == defc) || (NULL == results)) {
		result = -1;
	} else {
		j9object_t defcObject = J9_JNI_UNWRAP_REFERENCE(defc);
		j9array_t resultsArray = (j9array_t)J9_JNI_UNWRAP_REFERENCE(results);
		j9object_t nameObject = ((NULL == matchName) ? NULL : J9_JNI_UNWRAP_REFERENCE(matchName));
		j9object_t sigObject = ((NULL == matchSig) ? NULL : J9_JNI_UNWRAP_REFERENCE(matchSig));
		// j9object_t callerObject = ((NULL == caller) ? NULL : J9_JNI_UNWRAP_REFERENCE(caller));

		if (NULL == defcObject) {
			result = -1;
		} else if (!(((NULL != matchName) && (NULL == nameObject)) || ((NULL != matchSig) && (NULL == sigObject)))) {
			J9Class *defClass = J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, defcObject);
			UDATA length = J9INDEXABLEOBJECT_SIZE(currentThread, resultsArray);
			UDATA index = 0;
			if (!((NULL != nameObject) && (0 == J9VMJAVALANGSTRING_LENGTH(currentThread, nameObject)))) {
				if (J9ROMCLASS_IS_INTERFACE(defClass->romClass)) {
					result = -1;
				} else {
					if (J9_ARE_ANY_BITS_SET(matchFlags, MN_IS_FIELD)) {
						J9ROMFieldShape *romField = NULL;
						J9ROMFieldWalkState walkState;

						UDATA classDepth = 0;
						if (J9_ARE_ANY_BITS_SET(matchFlags, MN_SEARCH_SUPERCLASSES)) {
							/* walk superclasses */
							J9CLASS_DEPTH(defClass);
						}
						J9Class *currentClass = defClass;

						while (NULL != currentClass) {
							/* walk currentClass */
							memset(&walkState, 0, sizeof(walkState));
							romField = romFieldsStartDo(currentClass->romClass, &walkState);

							while (NULL != romField) {
								J9UTF8 *nameUTF = J9ROMFIELDSHAPE_NAME(romField);
								J9UTF8 *signatureUTF = J9ROMFIELDSHAPE_SIGNATURE(romField);
								if (((NULL == nameObject) || (0 != vmFuncs->compareStringToUTF8(currentThread, nameObject, FALSE, J9UTF8_DATA(nameUTF), J9UTF8_LENGTH(nameUTF))))
								&& ((NULL != sigObject) && (0 == vmFuncs->compareStringToUTF8(currentThread, sigObject, FALSE, J9UTF8_DATA(signatureUTF), J9UTF8_LENGTH(signatureUTF))))
								) {
									if (skip > 0) {
										skip -=1;
									} else {
										if (index < length) {
											j9object_t memberName = J9JAVAARRAYOFOBJECT_LOAD(currentThread, resultsArray, index);
											if (NULL == memberName) {
												vmFuncs->internalExitVMToJNI(currentThread);
												return -99;
											}
											j9object_t fieldObj = NULL;
											if (romField->modifiers & J9AccStatic) {
												/* create static field object */
												fieldObj = createFieldObject(currentThread, romField, defClass, nameUTF, signatureUTF, true);
											} else {
												/* create instance field object */
												fieldObj = createFieldObject(currentThread, romField, defClass, nameUTF, signatureUTF, false);
											}
											initImpl(currentThread, memberName, fieldObj);
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
									if (((NULL == nameObject) || (0 != vmFuncs->compareStringToUTF8(currentThread, nameObject, FALSE, J9UTF8_DATA(nameUTF), J9UTF8_LENGTH(nameUTF))))
									&& ((NULL != sigObject) && (0 == vmFuncs->compareStringToUTF8(currentThread, sigObject, FALSE, J9UTF8_DATA(signatureUTF), J9UTF8_LENGTH(signatureUTF))))
									) {
										if (skip > 0) {
											skip -=1;
										} else {
											if (index < length) {
												j9object_t memberName = J9JAVAARRAYOFOBJECT_LOAD(currentThread, resultsArray, index);
												if (NULL == memberName) {
													vmFuncs->internalExitVMToJNI(currentThread);
													return -99;
												}
												j9object_t fieldObj = NULL;
												if (romField->modifiers & J9AccStatic) {
													/* create static field object */
													fieldObj = createFieldObject(currentThread, romField, defClass, nameUTF, signatureUTF, true);
												} else {
													/* create instance field object */
													fieldObj = createFieldObject(currentThread, romField, defClass, nameUTF, signatureUTF, false);
												}
												initImpl(currentThread, memberName, fieldObj);
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
							J9CLASS_DEPTH(defClass);
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
									if (((NULL == nameObject) || (0 != vmFuncs->compareStringToUTF8(currentThread, nameObject, FALSE, J9UTF8_DATA(nameUTF), J9UTF8_LENGTH(nameUTF))))
									&& ((NULL != sigObject) && (0 == vmFuncs->compareStringToUTF8(currentThread, sigObject, FALSE, J9UTF8_DATA(signatureUTF), J9UTF8_LENGTH(signatureUTF))))
									) {
										if (skip > 0) {
											skip -=1;
										} else {
											if (index < length) {
												j9object_t memberName = J9JAVAARRAYOFOBJECT_LOAD(currentThread, resultsArray, index);
												if (NULL == memberName) {
													vmFuncs->internalExitVMToJNI(currentThread);
													return -99;
												}
												j9object_t methodObj = NULL;
												if (J9_ARE_NO_BITS_SET(romMethod->modifiers, J9AccStatic) && ('<' == (char)*J9UTF8_DATA(J9ROMMETHOD_NAME(romMethod)))) {
													/* create constructor object */
													methodObj = vm->reflectFunctions.createConstructorObject(currentMethod, currentClass, NULL, currentThread);
												} else {
													/* create method object */
													methodObj = vm->reflectFunctions.createMethodObject(currentMethod, currentClass, NULL, currentThread);
												}
												initImpl(currentThread, memberName, methodObj);
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
									if (((NULL == nameObject) || (0 != vmFuncs->compareStringToUTF8(currentThread, nameObject, FALSE, J9UTF8_DATA(nameUTF), J9UTF8_LENGTH(nameUTF))))
									&& ((NULL != sigObject) && (0 == vmFuncs->compareStringToUTF8(currentThread, sigObject, FALSE, J9UTF8_DATA(signatureUTF), J9UTF8_LENGTH(signatureUTF))))
									) {
										if (skip > 0) {
											skip -=1;
										} else {
											if (index < length) {
												j9object_t memberName = J9JAVAARRAYOFOBJECT_LOAD(currentThread, resultsArray, index);
												if (NULL == memberName) {
													vmFuncs->internalExitVMToJNI(currentThread);
													return -99;
												}
												j9object_t methodObj = NULL;
												if (J9_ARE_NO_BITS_SET(romMethod->modifiers, J9AccStatic) && ('<' == (char)*J9UTF8_DATA(J9ROMMETHOD_NAME(romMethod)))) {
													/* create constructor object */
													methodObj = vm->reflectFunctions.createConstructorObject(currentMethod, currentClass, NULL, currentThread);
												} else {
													/* create method object */
													methodObj = vm->reflectFunctions.createMethodObject(currentMethod, currentClass, NULL, currentThread);
												}
												initImpl(currentThread, memberName, methodObj);
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
	}
	Trc_JCL_java_lang_invoke_MethodHandleNatives_getMembers_Exit(env, result);
	vmFuncs->internalExitVMToJNI(currentThread);
	return result;
}

/**
 * static native long objectFieldOffset(MemberName self);  // e.g., returns vmindex
 */
jlong JNICALL
Java_java_lang_invoke_MethodHandleNatives_objectFieldOffset(JNIEnv *env, jclass clazz, jobject self)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
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
 */
jlong JNICALL
Java_java_lang_invoke_MethodHandleNatives_staticFieldOffset(JNIEnv *env, jclass clazz, jobject self)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
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
 */
jobject JNICALL
Java_java_lang_invoke_MethodHandleNatives_staticFieldBase(JNIEnv *env, jclass clazz, jobject self)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
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
 */
jobject JNICALL
Java_java_lang_invoke_MethodHandleNatives_getMemberVMInfo(JNIEnv *env, jclass clazz, jobject self)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	jobject result = NULL;
	vmFuncs->internalEnterVMFromJNI(currentThread);

	Trc_JCL_java_lang_invoke_MethodHandleNatives_getMemberVMInfo_Entry(env, self);
	if (NULL != self) {
		j9object_t membernameObject = J9_JNI_UNWRAP_REFERENCE(self);
		j9object_t clazzObject = J9VMJAVALANGINVOKEMEMBERNAME_CLAZZ(currentThread, membernameObject);
		jint flags = J9VMJAVALANGINVOKEMEMBERNAME_FLAGS(currentThread, membernameObject);
		jlong vmindex = (jlong)(UDATA)J9OBJECT_U64_LOAD(currentThread, membernameObject, vm->vmindexOffset);
		j9object_t target;
		if (J9_ARE_ANY_BITS_SET(flags, MN_IS_FIELD)) {
			vmindex = ((J9JNIFieldID*)vmindex)->offset;
			target = clazzObject;
		} else {
			J9JNIMethodID *methodID = (J9JNIMethodID*)vmindex;
			if (J9_ARE_ANY_BITS_SET(methodID->vTableIndex, J9_JNI_MID_INTERFACE)) {
				vmindex = methodID->vTableIndex & ~J9_JNI_MID_INTERFACE;
			} else if (0 == methodID->vTableIndex) {
				vmindex = -1;
			} else {
				vmindex = methodID->vTableIndex;
			}
			target = membernameObject;
		}

		j9object_t box = vm->memoryManagerFunctions->J9AllocateObject(currentThread, J9VMJAVALANGLONG_OR_NULL(vm), J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
		J9VMJAVALANGLONG_SET_VALUE(currentThread, box, vmindex);

		J9Class *arrayClass = fetchArrayClass(currentThread, J9VMJAVALANGOBJECT(vm));
		j9object_t arrayObject = vm->memoryManagerFunctions->J9AllocateIndexableObject(currentThread, arrayClass, 2, J9_GC_ALLOCATE_OBJECT_INSTRUMENTABLE);
		J9JAVAARRAYOFOBJECT_STORE(currentThread, arrayObject, 0, box);
		J9JAVAARRAYOFOBJECT_STORE(currentThread, arrayObject, 0, target);

		result = vmFuncs->j9jni_createLocalRef(env, clazzObject);
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
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	vmFuncs->internalEnterVMFromJNI(currentThread);

	if ((NULL == callsite) || (NULL == target)) {
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		j9object_t callsiteObject = J9_JNI_UNWRAP_REFERENCE(callsite);
		j9object_t targetObject = J9_JNI_UNWRAP_REFERENCE(target);

		UDATA offset = (UDATA)vmFuncs->instanceFieldOffset(currentThread, J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, callsiteObject), (U_8*)"target", strlen("target"),  (U_8*)"Ljava/lang/invoke/MethodHandle;", strlen("Ljava/lang/invoke/MethodHandle;"), NULL, NULL, 0);
		MM_ObjectAccessBarrierAPI objectAccessBarrier = MM_ObjectAccessBarrierAPI(currentThread);
		objectAccessBarrier.inlineMixedObjectStoreObject(currentThread, callsiteObject, offset, targetObject, false);
	}
	vmFuncs->internalExitVMToJNI(currentThread);
}

/**
 * static native void setCallSiteTargetVolatile(CallSite site, MethodHandle target);
 */
void JNICALL
Java_java_lang_invoke_MethodHandleNatives_setCallSiteTargetVolatile(JNIEnv *env, jclass clazz, jobject callsite, jobject target)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	vmFuncs->internalEnterVMFromJNI(currentThread);

	if ((NULL == callsite) || (NULL == target)) {
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		j9object_t callsiteObject = J9_JNI_UNWRAP_REFERENCE(callsite);
		j9object_t targetObject = J9_JNI_UNWRAP_REFERENCE(target);

		UDATA offset = (UDATA)vmFuncs->instanceFieldOffset(currentThread, J9VM_J9CLASS_FROM_HEAPCLASS(currentThread, callsiteObject), (U_8*)"target", strlen("target"), (U_8*)"Ljava/lang/invoke/MethodHandle;", strlen("Ljava/lang/invoke/MethodHandle;"), NULL, NULL, 0);
		MM_ObjectAccessBarrierAPI objectAccessBarrier = MM_ObjectAccessBarrierAPI(currentThread);
		objectAccessBarrier.inlineMixedObjectStoreObject(currentThread, callsiteObject, offset, targetObject, true);
	}
	vmFuncs->internalExitVMToJNI(currentThread);
}

/**
 * static native void copyOutBootstrapArguments(Class<?> caller, int[] indexInfo,
												int start, int end,
												Object[] buf, int pos,
												boolean resolve,
												Object ifNotAvailable);
 */
void JNICALL
Java_java_lang_invoke_MethodHandleNatives_copyOutBootstrapArguments(JNIEnv *env, jclass clazz, jclass caller, jintArray indexInfo, jint start, jint end, jobjectArray buf, jint pos, jboolean resolve, jobject ifNotAvailable)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	vmFuncs->internalEnterVMFromJNI(currentThread);

	if ((NULL == caller) || (NULL == indexInfo) || (NULL == buf)) {
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
	} else {
		J9Class *callerClass = J9VM_J9CLASS_FROM_JCLASS(currentThread, caller);
		j9array_t indexInfoArray = (j9array_t)J9_JNI_UNWRAP_REFERENCE(indexInfo);
		j9array_t bufferArray = (j9array_t)J9_JNI_UNWRAP_REFERENCE(buf);

		if ((NULL == callerClass) || (NULL == indexInfoArray) || (NULL == bufferArray) || (J9INDEXABLEOBJECT_SIZE(currentThread, indexInfoArray) < 2)) {
			vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGINTERNALERROR, NULL);
		} else if (((start < -4) || (start > end) || (pos < 0)) || ((jint)J9INDEXABLEOBJECT_SIZE(currentThread, bufferArray) <= pos) || ((jint)J9INDEXABLEOBJECT_SIZE(currentThread, bufferArray) <= (pos + end - start))) {
			vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGLINKAGEERROR, NULL);
		} else {
			// U_16 bsmArgCount = (U_16)J9JAVAARRAYOFINT_LOAD(currentThread, indexInfoArray, 0);
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
				bsmData += 2;

				j9object_t obj;
				j9object_t ifNotAvailableObject = NULL;
				if (NULL != ifNotAvailable) {
					ifNotAvailableObject = J9_JNI_UNWRAP_REFERENCE(ifNotAvailable);
				}
				while (start < end) {
					obj = NULL;
					if (start >= 0) {
						U_16 argIndex = bsmData[start];
						J9ConstantPool *ramConstantPool = J9_CP_FROM_CLASS(callerClass);
						obj = resolveRefToObject(currentThread, ramConstantPool, argIndex, (JNI_TRUE == resolve));
						if ((NULL == obj) && (JNI_TRUE != resolve)) {
							obj = ifNotAvailableObject;
						}
					} else if (start == -4) {
						obj = resolveRefToObject(currentThread, J9_CP_FROM_CLASS(callerClass), bsmCPIndex, true);
					} else if (start == -3) {
						J9UTF8 *name = J9ROMNAMEANDSIGNATURE_NAME(nameAndSig);
						obj = vm->memoryManagerFunctions->j9gc_createJavaLangString(currentThread, J9UTF8_DATA(name), (U_32)J9UTF8_LENGTH(name), J9_STR_INTERN);
					} else if (start == -2) {
						J9UTF8 *signature = J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig);
						/* Call VM Entry point to create the MethodType - Result is put into the
						* currentThread->returnValue as entry points don't "return" in the expected way
						*/
						vmFuncs->sendFromMethodDescriptorString(currentThread, signature, callerClass->classLoader, NULL);
						obj = (j9object_t)currentThread->returnValue;
					} else if (start == -1) {
						obj = vm->memoryManagerFunctions->J9AllocateObject(currentThread, J9VMJAVALANGINTEGER_OR_NULL(vm), J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
						J9VMJAVALANGINTEGER_SET_VALUE(currentThread, obj, argCount);
					}
					J9JAVAARRAYOFOBJECT_STORE(currentThread, bufferArray, pos, obj);
					start += 1;
					pos += 1;
				}
			} else {
				vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGLINKAGEERROR, NULL);
			}
		}
	}
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


} /* extern "C" */
