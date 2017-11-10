/*******************************************************************************
 * Copyright (c) 1998, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "jni.h"
#include "jcl.h"
#include "jclprots.h"
#include "jclglob.h"
#include "jniidcacheinit.h"
#include "j9.h"

#define J9CLASS_FROMCPINTERNALRAMCLASS(vmThread, j9ClassRef) J9VMJAVALANGINTERNALRAMCLASS_VMREF(vmThread, J9_JNI_UNWRAP_REFERENCE(j9ClassRef))

typedef enum {
	OK,
	CP_INDEX_OUT_OF_BOUNDS_EXCEPTION,
	WRONG_CP_ENTRY_TYPE_EXCEPTION,
	NULL_POINTER_EXCEPTION
} SunReflectCPResult;

static void
clearException(J9VMThread *vmThread)
{
	vmThread->currentException = NULL;
	vmThread->privateFlags &= ~J9_PRIVATE_FLAGS_REPORT_EXCEPTION_THROW;
}

static SunReflectCPResult
getRAMConstantRefAndType(J9VMThread *vmThread, jobject constantPoolOop, jint cpIndex, UDATA *cpType, J9RAMConstantRef **ramConstantRef)
{
	J9Class *ramClass = J9CLASS_FROMCPINTERNALRAMCLASS(vmThread, constantPoolOop);
	J9ROMClass *romClass = ramClass->romClass;
	SunReflectCPResult result = CP_INDEX_OUT_OF_BOUNDS_EXCEPTION;

	if ((0 <= cpIndex) && ((U_32)cpIndex < romClass->ramConstantPoolCount)) {
		J9ConstantPool *ramConstantPool = J9_CP_FROM_CLASS(ramClass);
		U_32 *cpShapeDescription = J9ROMCLASS_CPSHAPEDESCRIPTION(romClass);
		*cpType = J9_CP_TYPE(cpShapeDescription, cpIndex);
		*ramConstantRef = ((J9RAMConstantRef *) ramConstantPool) + cpIndex;
		result = OK;
	}

	return result;
}

static SunReflectCPResult
getRAMConstantRef(J9VMThread *vmThread, jobject constantPoolOop, jint cpIndex, UDATA expectedCPType, J9RAMConstantRef **ramConstantRef)
{
	UDATA cpType = J9CPTYPE_UNUSED;
	SunReflectCPResult result = getRAMConstantRefAndType(vmThread, constantPoolOop, cpIndex, &cpType, ramConstantRef);
	if ((OK == result) && (expectedCPType != cpType)) {
		result = WRONG_CP_ENTRY_TYPE_EXCEPTION;
	}
	return result;
}

static SunReflectCPResult
getROMCPItemAndType(J9VMThread *vmThread, jobject constantPoolOop, jint cpIndex, UDATA *cpType, J9ROMConstantPoolItem **romCPItem)
{
	J9Class *ramClass = J9CLASS_FROMCPINTERNALRAMCLASS(vmThread, constantPoolOop);
	J9ROMClass *romClass = ramClass->romClass;
	SunReflectCPResult result = CP_INDEX_OUT_OF_BOUNDS_EXCEPTION;

	if ((0 <= cpIndex) && ((U_32)cpIndex < romClass->romConstantPoolCount)) {
		J9ConstantPool *ramConstantPool = J9_CP_FROM_CLASS(ramClass);
		J9ROMConstantPoolItem *romConstantPool = J9_ROM_CP_FROM_CP(ramConstantPool);
		U_32 *cpShapeDescription = J9ROMCLASS_CPSHAPEDESCRIPTION(romClass);
		*cpType = J9_CP_TYPE(cpShapeDescription, cpIndex);
		*romCPItem = romConstantPool + cpIndex;
		result = OK;
	}

	return result;
}

static SunReflectCPResult
getROMCPItem(J9VMThread *vmThread, jobject constantPoolOop, jint cpIndex, UDATA expectedCPType, J9ROMConstantPoolItem **romCPItem)
{
	UDATA cpType = J9CPTYPE_UNUSED;
	SunReflectCPResult result = getROMCPItemAndType(vmThread, constantPoolOop, cpIndex, &cpType, romCPItem);
	if ((OK == result) && (expectedCPType != cpType)) {
		result = WRONG_CP_ENTRY_TYPE_EXCEPTION;
	}
	return result;
}

static SunReflectCPResult
getJ9ClassAt(J9VMThread *vmThread, jobject constantPoolOop, jint cpIndex, UDATA resolveFlags, J9Class **clazz)
{
	J9RAMClassRef *ramClassRefWrapper = NULL;
	J9InternalVMFunctions *vmFunctions = vmThread->javaVM->internalVMFunctions;
	SunReflectCPResult result = getRAMConstantRef(vmThread, constantPoolOop, cpIndex, J9CPTYPE_CLASS, (J9RAMConstantRef **) &ramClassRefWrapper);
	if (OK == result) {
		if (NULL != ramClassRefWrapper->value) {
			*clazz = ramClassRefWrapper->value;
		} else {
			J9Class *cpClass = J9CLASS_FROMCPINTERNALRAMCLASS(vmThread, constantPoolOop);
			*clazz = vmFunctions->resolveClassRef(vmThread, J9_CP_FROM_CLASS(cpClass), cpIndex, resolveFlags);
		}
	}
	return result;
}

static void
checkResult(JNIEnv *env, SunReflectCPResult result)
{
	switch (result) {
	case OK:
		break;
	case CP_INDEX_OUT_OF_BOUNDS_EXCEPTION:
		throwNewIllegalArgumentException (env, "Constant pool index out of bounds");
		break;
	case WRONG_CP_ENTRY_TYPE_EXCEPTION:
		throwNewIllegalArgumentException (env, "Wrong type at constant pool index");
		break;
	case NULL_POINTER_EXCEPTION:
		throwNewNullPointerException(env, "constantPoolOop is null");
		break;
	}
}
static jclass
getStringAt(JNIEnv *env, jobject unusedObject, jobject constantPoolOop, jint cpIndex, U_8 cpType)
{
	jobject returnValue = NULL;
	J9VMThread *vmThread = (J9VMThread *) env;
	J9InternalVMFunctions *vmFunctions = vmThread->javaVM->internalVMFunctions;
	SunReflectCPResult result = NULL_POINTER_EXCEPTION;

	if (NULL != constantPoolOop) {
		J9RAMStringRef *ramStringRef = NULL;
		vmFunctions->internalEnterVMFromJNI(vmThread);
		result = getRAMConstantRef(vmThread, constantPoolOop, cpIndex, cpType, (J9RAMConstantRef **) &ramStringRef);
		if (OK == result) {
			J9Class *ramClass = J9CLASS_FROMCPINTERNALRAMCLASS(vmThread, constantPoolOop);
			j9object_t stringObject = J9STATIC_OBJECT_LOAD(vmThread, ramClass, &ramStringRef->stringObject);

			if (NULL == stringObject) {
				stringObject = vmFunctions->resolveStringRef(vmThread, J9_CP_FROM_CLASS(ramClass), cpIndex, J9_RESOLVE_FLAG_RUNTIME_RESOLVE | J9_RESOLVE_FLAG_NO_THROW_ON_FAIL);
			}
			if (NULL != stringObject) {
				returnValue = vmFunctions->j9jni_createLocalRef(env, stringObject);
			}
		}
		vmFunctions->internalReleaseVMAccess(vmThread);
	}

	checkResult(env, result);

	return returnValue;
}

static jclass
getClassAt(JNIEnv *env, jobject constantPoolOop, jint cpIndex, UDATA resolveFlags)
{
	jclass returnValue = NULL;
	J9VMThread *vmThread = (J9VMThread *) env;
	J9InternalVMFunctions *vmFunctions = vmThread->javaVM->internalVMFunctions;
	SunReflectCPResult result = NULL_POINTER_EXCEPTION;

	if (NULL != constantPoolOop) {
		J9Class *clazz = NULL;
		vmFunctions->internalEnterVMFromJNI(vmThread);
		result = getJ9ClassAt(vmThread, constantPoolOop, cpIndex, resolveFlags, &clazz);
		if (NULL != clazz) {
			returnValue = vmFunctions->j9jni_createLocalRef(env, J9VM_J9CLASS_TO_HEAPCLASS(clazz));
		}
		vmFunctions->internalReleaseVMAccess(vmThread);
	}

	checkResult(env, result);

	return returnValue;
}

static jobject
getMethodAt(JNIEnv *env, jobject constantPoolOop, jint cpIndex, UDATA resolveFlags)
{
	jobject returnValue = NULL;
	J9VMThread *vmThread = (J9VMThread *) env;
	J9InternalVMFunctions *vmFunctions = vmThread->javaVM->internalVMFunctions;
	SunReflectCPResult result = NULL_POINTER_EXCEPTION;
	jmethodID methodID = NULL;
	UDATA cpType = J9CPTYPE_UNUSED;

	if (NULL != constantPoolOop) {
		J9RAMConstantRef *ramConstantRef = NULL;
		jclass jlClass = NULL;

		vmFunctions->internalEnterVMFromJNI(vmThread);
		result = getRAMConstantRefAndType(vmThread, constantPoolOop, cpIndex, &cpType, &ramConstantRef);
		if (OK == result) {
			J9Method *method = NULL;
			J9Class *cpClass = J9CLASS_FROMCPINTERNALRAMCLASS(vmThread, constantPoolOop);
			J9ConstantPool *constantPool = J9_CP_FROM_CLASS(cpClass);
			switch (cpType) {
			case J9CPTYPE_HANDLE_METHOD: /* fall through */
			case J9CPTYPE_INSTANCE_METHOD:
				/* Check for resolved special first, then try to resolve as virtual, then special and then static */
				method = ((J9RAMMethodRef *) ramConstantRef)->method;
				if ((NULL == method) || (NULL == method->constantPool)) {
					if (0 == vmFunctions->resolveVirtualMethodRef(vmThread, constantPool, cpIndex, resolveFlags, &method)) {
						clearException(vmThread);
						method = vmFunctions->resolveSpecialMethodRef(vmThread, constantPool, cpIndex, resolveFlags);
					}
					if (NULL == method) {
						clearException(vmThread);
						/* Do not update the cp entry for type J9CPTYPE_INSTANCE_METHOD when resolving the method ref as
						 * a static method as this may crash if an invokespecial bytecode targets this cpEntry.  The
						 * contract for J9CPTYPE_INSTANCE_METHOD requires that the cpEntry->method will only be a
						 * non-static method.
						 */
						method = vmFunctions->resolveStaticMethodRefInto(vmThread, constantPool, cpIndex, resolveFlags, NULL);
					}
				}
				break;
			case J9CPTYPE_STATIC_METHOD:
				method = ((J9RAMStaticMethodRef *) ramConstantRef)->method;
				/* TODO is this the right check for an unresolved method? Can I check against vm->initialMethods.initialStaticMethod */
				if ((NULL == method) || (NULL == method->constantPool)) {
					method = vmFunctions->resolveStaticMethodRef(vmThread, constantPool, cpIndex, resolveFlags);
					if (NULL == method) {
						clearException(vmThread);
						vmFunctions->resolveVirtualMethodRef(vmThread, constantPool, cpIndex, resolveFlags, &method);
					}
				}
				break;
			case J9CPTYPE_INTERFACE_METHOD:
				/* TODO check for resolved and lookup in itable */
				method = vmFunctions->resolveInterfaceMethodRef(vmThread, constantPool, cpIndex, resolveFlags);
				break;
			default:
				result = WRONG_CP_ENTRY_TYPE_EXCEPTION;
				break;
			}
			if (NULL != method) {
				methodID = (jmethodID) vmFunctions->getJNIMethodID(vmThread, method);
				jlClass = vmFunctions->j9jni_createLocalRef(env, cpClass->classObject);
			}
		}

		vmFunctions->internalReleaseVMAccess(vmThread);

		if (NULL != methodID) {
			if (NULL != jlClass) {
				returnValue = (*env)->ToReflectedMethod(env, jlClass, methodID, J9CPTYPE_STATIC_METHOD == cpType);
			} else {
				throwNativeOOMError(env, 0, 0);
			}
		}

	}

	checkResult(env, result);

	return returnValue;
}

static jobject
getFieldAt(JNIEnv *env, jobject constantPoolOop, jint cpIndex, UDATA resolveFlags)
{
	jobject returnValue = NULL;
	J9VMThread *vmThread = (J9VMThread *) env;
	J9InternalVMFunctions *vmFunctions = vmThread->javaVM->internalVMFunctions;
	SunReflectCPResult result = NULL_POINTER_EXCEPTION;
	jfieldID fieldID = NULL;
	UDATA cpType = J9CPTYPE_UNUSED;

	if (NULL != constantPoolOop) {
		J9RAMConstantRef *ramConstantRef = NULL;
		jclass jlClass = NULL;
		
		vmFunctions->internalEnterVMFromJNI(vmThread);
retry:
		ramConstantRef = NULL;
		result = getRAMConstantRefAndType(vmThread, constantPoolOop, cpIndex, &cpType, &ramConstantRef);
		if (OK == result) {
			J9ROMFieldShape *resolvedField = NULL;
			UDATA offset = 0;
			J9Class *cpClass = J9CLASS_FROMCPINTERNALRAMCLASS(vmThread, constantPoolOop);
			J9ConstantPool *constantPool = J9_CP_FROM_CLASS(cpClass);
			switch (cpType) {
			case J9CPTYPE_FIELD: {
				/* Try to resolve as instance field, then as a static */
				offset = (UDATA) vmFunctions->resolveInstanceFieldRef(vmThread, NULL, constantPool, cpIndex, resolveFlags, &resolvedField);
				if (NULL == resolvedField) {
					void *fieldAddress = vmFunctions->resolveStaticFieldRef(vmThread, NULL, constantPool, cpIndex, resolveFlags, &resolvedField);
					offset = (UDATA) fieldAddress - (UDATA) cpClass->ramStatics;
				}
				break;
			}
			default:
				result = WRONG_CP_ENTRY_TYPE_EXCEPTION;
				break;
			}
			if (NULL != resolvedField) {
				J9ROMFieldRef *romFieldRef = NULL;
				J9Class *declaringClass = NULL;
				result = getROMCPItem(vmThread, constantPoolOop, cpIndex, cpType, (J9ROMConstantPoolItem **) &romFieldRef);
				if (OK == result) {
					result = getJ9ClassAt(vmThread, constantPoolOop, romFieldRef->classRefCPIndex, resolveFlags, &declaringClass);
				}
				if (OK == result) {
					UDATA inconsistentData = 0;
					fieldID = (jfieldID) vmFunctions->getJNIFieldID(vmThread, declaringClass, resolvedField, offset, &inconsistentData);
					if (0 != inconsistentData) {
						/* Hotswap occurred - the resolvedField is not from declaringClass->romClass.
						 * Need to restart the operation.
						 */
						goto retry;
					}

					jlClass = vmFunctions->j9jni_createLocalRef(env, cpClass->classObject);
				}
			}
		}

		vmFunctions->internalReleaseVMAccess(vmThread);

		if (NULL != fieldID) {
			if (NULL != jlClass) {
				/* The isStatic argument is ignored. */
				returnValue = (*env)->ToReflectedField(env, jlClass, fieldID, FALSE);
			} else {
				throwNativeOOMError(env, 0, 0);
			}
		}
	}

	checkResult(env, result);

	return returnValue;
}

static U_32
getSingleSlotConstant(JNIEnv *env, jobject constantPoolOop, jint cpIndex, UDATA expectedCPType)
{
	U_32 returnValue = 0;
	J9VMThread *vmThread = (J9VMThread *) env;
	J9InternalVMFunctions *vmFunctions = vmThread->javaVM->internalVMFunctions;
	SunReflectCPResult result = NULL_POINTER_EXCEPTION;

	if (NULL != constantPoolOop) {
		J9ROMSingleSlotConstantRef *cpEntry = NULL;
		vmFunctions->internalEnterVMFromJNI(vmThread);
		result = getROMCPItem(vmThread, constantPoolOop, cpIndex, expectedCPType, (J9ROMConstantPoolItem **) &cpEntry);
		if (OK == result) {
			returnValue = cpEntry->data;
		}
		vmFunctions->internalReleaseVMAccess(vmThread);
	}

	checkResult(env, result);

	return returnValue;
}

static U_64
getDoubleSlotConstant(JNIEnv *env, jobject constantPoolOop, jint cpIndex, UDATA expectedCPType)
{
	U_64 returnValue = J9CONST64(0);
	J9VMThread *vmThread = (J9VMThread *) env;
	J9InternalVMFunctions *vmFunctions = vmThread->javaVM->internalVMFunctions;
	SunReflectCPResult result = NULL_POINTER_EXCEPTION;

	if (NULL != constantPoolOop) {
		J9ROMConstantRef *cpEntry = NULL;
		vmFunctions->internalEnterVMFromJNI(vmThread);
		result = getROMCPItem(vmThread, constantPoolOop, cpIndex, expectedCPType, (J9ROMConstantPoolItem **) &cpEntry);
		if (OK == result) {
#ifdef J9VM_ENV_LITTLE_ENDIAN
			returnValue = (((U_64)(cpEntry->slot2)) << 32) | ((U_64)(cpEntry->slot1));
#else
			returnValue = (((U_64)(cpEntry->slot1)) << 32) | ((U_64)(cpEntry->slot2));
#endif
		}
		vmFunctions->internalReleaseVMAccess(vmThread);
	}

	checkResult(env, result);

	return returnValue;
}

jint JNICALL
Java_sun_reflect_ConstantPool_getSize0(JNIEnv *env, jobject unusedObject, jobject constantPoolOop)
{
	U_32 returnValue = 0;
	J9VMThread *vmThread = (J9VMThread *) env;
	J9InternalVMFunctions *vmFunctions = vmThread->javaVM->internalVMFunctions;
	SunReflectCPResult result = NULL_POINTER_EXCEPTION;

	if (NULL != constantPoolOop) {
		vmFunctions->internalEnterVMFromJNI(vmThread);
		{
			J9Class *ramClass = J9CLASS_FROMCPINTERNALRAMCLASS(vmThread, constantPoolOop);
			if (NULL != ramClass) {
				J9ROMClass *romClass = ramClass->romClass;
				returnValue = romClass->romConstantPoolCount;
				result = OK;
			}
		}
		vmFunctions->internalReleaseVMAccess(vmThread);
	}

	checkResult(env, result);

	return (jint) returnValue;
}

jclass JNICALL
Java_sun_reflect_ConstantPool_getClassAt0(JNIEnv *env, jobject unusedObject, jobject constantPoolOop, jint cpIndex)
{
	return getClassAt(env, constantPoolOop, cpIndex, J9_RESOLVE_FLAG_RUNTIME_RESOLVE | J9_RESOLVE_FLAG_NO_THROW_ON_FAIL);
}

jclass JNICALL
Java_sun_reflect_ConstantPool_getClassAtIfLoaded0(JNIEnv *env, jobject unusedObject, jobject constantPoolOop, jint cpIndex)
{
	return getClassAt(env, constantPoolOop, cpIndex, J9_RESOLVE_FLAG_JIT_COMPILE_TIME | J9_RESOLVE_FLAG_NO_THROW_ON_FAIL);
}

jobject JNICALL
Java_sun_reflect_ConstantPool_getMethodAt0(JNIEnv *env, jobject unusedObject, jobject constantPoolOop, jint cpIndex)
{
	return getMethodAt(env, constantPoolOop, cpIndex, J9_RESOLVE_FLAG_RUNTIME_RESOLVE | J9_RESOLVE_FLAG_NO_THROW_ON_FAIL);
}

jobject JNICALL
Java_sun_reflect_ConstantPool_getMethodAtIfLoaded0(JNIEnv *env, jobject unusedObject, jobject constantPoolOop, jint cpIndex)
{
	return getMethodAt(env, constantPoolOop, cpIndex, J9_RESOLVE_FLAG_JIT_COMPILE_TIME | J9_RESOLVE_FLAG_NO_THROW_ON_FAIL);
}

jobject JNICALL
Java_sun_reflect_ConstantPool_getFieldAt0(JNIEnv *env, jobject unusedObject, jobject constantPoolOop, jint cpIndex)
{
	return getFieldAt(env, constantPoolOop, cpIndex, J9_RESOLVE_FLAG_RUNTIME_RESOLVE | J9_RESOLVE_FLAG_NO_THROW_ON_FAIL);
}

jobject JNICALL
Java_sun_reflect_ConstantPool_getFieldAtIfLoaded0(JNIEnv *env, jobject unusedObject, jobject constantPoolOop, jint cpIndex)
{
	return getFieldAt(env, constantPoolOop, cpIndex, J9_RESOLVE_FLAG_JIT_COMPILE_TIME | J9_RESOLVE_FLAG_NO_THROW_ON_FAIL);
}

/**
 * Get the class name from a constant pool class element, which is located
 * at the specified index in a class's constant pool.
 *
 * @param env[in]             the JNI env
 * @param unusedObject[in]    unused
 * @param constantPoolOop[in] the class - its constant pool is accessed
 * @param cpIndex[in]         the constant pool index
 *
 * @return  instance of String which contains the class name or NULL in
 *          case of error
 *
 * @throws  NullPointerException if constantPoolOop is null
 * @throws  IllegalArgumentException if cpIndex has wrong type
 */
jobject JNICALL
Java_java_lang_invoke_MethodHandle_getCPClassNameAt(JNIEnv *env, jobject unusedObject, jobject constantPoolOop, jint cpIndex)
{
	jobject classNameObject = NULL;
	J9VMThread *vmThread = (J9VMThread *) env;
	J9InternalVMFunctions *vmFunctions = vmThread->javaVM->internalVMFunctions;
	J9MemoryManagerFunctions *gcFunctions = vmThread->javaVM->memoryManagerFunctions;
	SunReflectCPResult result = NULL_POINTER_EXCEPTION;

	if (NULL != constantPoolOop) {
		UDATA cpType = J9CPTYPE_UNUSED;
		J9ROMConstantPoolItem *romCPItem = NULL;
		vmFunctions->internalEnterVMFromJNI(vmThread);
		result = getROMCPItemAndType(vmThread, constantPoolOop, cpIndex, &cpType, &romCPItem);
		if (OK == result) {
			switch (cpType) {
			case J9CPTYPE_CLASS: {
				J9UTF8 *className = J9ROMCLASSREF_NAME((J9ROMClassRef*)romCPItem);
				j9object_t internalClassNameObject = gcFunctions->j9gc_createJavaLangString(vmThread, J9UTF8_DATA(className), (U_32) J9UTF8_LENGTH(className), 0);
				classNameObject = vmFunctions->j9jni_createLocalRef(env, internalClassNameObject);
				break;
			}
			default:
				result = WRONG_CP_ENTRY_TYPE_EXCEPTION;
				break;
			}
		}
		vmFunctions->internalReleaseVMAccess(vmThread);
	}

	checkResult(env, result);

	return classNameObject;
}

jobject JNICALL
Java_sun_reflect_ConstantPool_getMemberRefInfoAt0(JNIEnv *env, jobject unusedObject, jobject constantPoolOop, jint cpIndex)
{
	jobject returnValue = NULL;
	jobject classNameObject = NULL;
	jobject nameObject = NULL;
	jobject signatureObject = NULL;
	J9VMThread *vmThread = (J9VMThread *) env;
	J9InternalVMFunctions *vmFunctions = vmThread->javaVM->internalVMFunctions;	
	J9MemoryManagerFunctions *gcFunctions = vmThread->javaVM->memoryManagerFunctions;
	SunReflectCPResult result = NULL_POINTER_EXCEPTION;

	/* Cache java/lang/String class ID */
	if (!initializeJavaLangStringIDCache(env)) {
		return returnValue;
	}

	if (NULL != constantPoolOop) {
		UDATA cpType = J9CPTYPE_UNUSED;
		J9ROMConstantPoolItem *romCPItem = NULL;
		vmFunctions->internalEnterVMFromJNI(vmThread);
		result = getROMCPItemAndType(vmThread, constantPoolOop, cpIndex, &cpType, &romCPItem);
		if (OK == result) {
			U_32 classRefCPIndex = 0;
			J9ROMNameAndSignature *nameAndSignature = NULL;
			switch (cpType) {
			case J9CPTYPE_HANDLE_METHOD: /* fall thru */
			case J9CPTYPE_INSTANCE_METHOD: /* fall thru */
			case J9CPTYPE_STATIC_METHOD: /* fall thru */
			case J9CPTYPE_INTERFACE_METHOD:
				classRefCPIndex = ((J9ROMMethodRef *) romCPItem)->classRefCPIndex;
				nameAndSignature = J9ROMMETHODREF_NAMEANDSIGNATURE((J9ROMMethodRef *) romCPItem);
				break;
			case J9CPTYPE_FIELD:
				classRefCPIndex = ((J9ROMFieldRef *) romCPItem)->classRefCPIndex;
				nameAndSignature = J9ROMFIELDREF_NAMEANDSIGNATURE((J9ROMFieldRef *) romCPItem);
				break;
			default:
				result = WRONG_CP_ENTRY_TYPE_EXCEPTION;
				break;
			}
			if (NULL != nameAndSignature) {
				J9UTF8 *name = J9ROMNAMEANDSIGNATURE_NAME(nameAndSignature);
				J9UTF8 *signature = J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSignature);
				J9ROMClassRef *classRef = NULL;
				result = getROMCPItem(vmThread, constantPoolOop, classRefCPIndex, J9CPTYPE_CLASS, (J9ROMConstantPoolItem **) &classRef);
				if (OK == result) {
					J9UTF8 *className = J9ROMCLASSREF_NAME(classRef);
					j9object_t internalClassNameObject = gcFunctions->j9gc_createJavaLangString(vmThread, J9UTF8_DATA(className), (U_32) J9UTF8_LENGTH(className), 0);
					j9object_t internalNameObject = gcFunctions->j9gc_createJavaLangString(vmThread, J9UTF8_DATA(name), (U_32) J9UTF8_LENGTH(name), 0);
					j9object_t internalSignatureObject = gcFunctions->j9gc_createJavaLangString(vmThread, J9UTF8_DATA(signature), (U_32) J9UTF8_LENGTH(signature), 0);
					classNameObject = vmFunctions->j9jni_createLocalRef(env, internalClassNameObject);
					nameObject = vmFunctions->j9jni_createLocalRef(env, internalNameObject);
					signatureObject = vmFunctions->j9jni_createLocalRef(env, internalSignatureObject);
				}
			}
		}
		vmFunctions->internalReleaseVMAccess(vmThread);
	}

	if ((NULL != classNameObject) && (NULL != nameObject) && (NULL != signatureObject)) {
		jclass jlString = JCL_CACHE_GET(env, CLS_java_lang_String);
		jobject array = (*env)->NewObjectArray(env, 3, jlString, NULL);
		(*env)->SetObjectArrayElement(env, array, 0, classNameObject);
		(*env)->SetObjectArrayElement(env, array, 1, nameObject);
		(*env)->SetObjectArrayElement(env, array, 2, signatureObject);
		(*env)->DeleteLocalRef(env, classNameObject);
		(*env)->DeleteLocalRef(env, nameObject);
		(*env)->DeleteLocalRef(env, signatureObject);
		if (!(*env)->ExceptionCheck(env)) {
			returnValue = array;
		}

	}

	checkResult(env, result);

	return returnValue;
}

jint JNICALL
Java_sun_reflect_ConstantPool_getIntAt0(JNIEnv *env, jobject unusedObject, jobject constantPoolOop, jint cpIndex)
{
	U_32 returnValue = getSingleSlotConstant(env, constantPoolOop, cpIndex, J9CPTYPE_INT);
	return *(jint *) &returnValue;
}

jlong JNICALL
Java_sun_reflect_ConstantPool_getLongAt0(JNIEnv *env, jobject unusedObject, jobject constantPoolOop, jint cpIndex)
{
	U_64 returnValue = getDoubleSlotConstant(env, constantPoolOop, cpIndex, J9CPTYPE_LONG);
	return *(jlong *) &returnValue;
}

jfloat JNICALL
Java_sun_reflect_ConstantPool_getFloatAt0(JNIEnv *env, jobject unusedObject, jobject constantPoolOop, jint cpIndex)
{
	U_32 returnValue = getSingleSlotConstant(env, constantPoolOop, cpIndex, J9CPTYPE_FLOAT);
	return *(jfloat *) &returnValue;
}

jdouble JNICALL
Java_sun_reflect_ConstantPool_getDoubleAt0(JNIEnv *env, jobject unusedObject, jobject constantPoolOop, jint cpIndex)
{
	U_64 returnValue = getDoubleSlotConstant(env, constantPoolOop, cpIndex, J9CPTYPE_DOUBLE);
	return *(jdouble *) &returnValue;
}

jobject JNICALL
Java_sun_reflect_ConstantPool_getStringAt0(JNIEnv *env, jobject unusedObject, jobject constantPoolOop, jint cpIndex)
{
	return getStringAt(env, unusedObject, constantPoolOop, cpIndex, J9CPTYPE_STRING);
}

jobject JNICALL
Java_sun_reflect_ConstantPool_getUTF8At0(JNIEnv *env, jobject unusedObject, jobject constantPoolOop, jint cpIndex)
{
	return getStringAt(env, unusedObject, constantPoolOop, cpIndex, J9CPTYPE_ANNOTATION_UTF8);
}

/*
 * Return the result of J9_CP_TYPE(J9Class->romClass->cpShapeDescription, index)
 */
jint JNICALL
Java_java_lang_invoke_MethodHandle_getCPTypeAt(JNIEnv *env, jclass unusedClass, jobject constantPoolOop, jint cpIndex)
{
	UDATA cpType = J9CPTYPE_UNUSED;
	J9VMThread *vmThread = (J9VMThread *) env;
	J9InternalVMFunctions *vmFunctions = vmThread->javaVM->internalVMFunctions;
	SunReflectCPResult result = NULL_POINTER_EXCEPTION;

	if (NULL != constantPoolOop) {
		J9Class *ramClass;
		J9ROMClass *romClass;
		result = CP_INDEX_OUT_OF_BOUNDS_EXCEPTION;
		vmFunctions->internalEnterVMFromJNI(vmThread);
		ramClass = J9CLASS_FROMCPINTERNALRAMCLASS(vmThread, constantPoolOop);
		romClass = ramClass->romClass;

		if ((0 <= cpIndex) && ((U_32)cpIndex < romClass->romConstantPoolCount)) {
			U_32 *cpShapeDescription = J9ROMCLASS_CPSHAPEDESCRIPTION(romClass);
			cpType = J9_CP_TYPE(cpShapeDescription, cpIndex);
			result = OK;
		}
		vmFunctions->internalReleaseVMAccess(vmThread);
	}

	checkResult(env, result);

	return (jint) cpType;
}

/*
 * sun.reflect.ConstantPool doesn't have a getMethodTypeAt method. This is the
 * equivalent for MethodType.
 */
jobject JNICALL
Java_java_lang_invoke_MethodHandle_getCPMethodTypeAt(JNIEnv *env, jclass unusedClass, jobject constantPoolOop, jint cpIndex)
{
	jobject returnValue = NULL;
	J9VMThread *vmThread = (J9VMThread *) env;
	J9InternalVMFunctions *vmFunctions = vmThread->javaVM->internalVMFunctions;
	SunReflectCPResult result = NULL_POINTER_EXCEPTION;

	if (NULL != constantPoolOop) {
		J9RAMMethodTypeRef *ramMethodTypeRef = NULL;
		vmFunctions->internalEnterVMFromJNI(vmThread);
		result = getRAMConstantRef(vmThread, constantPoolOop, cpIndex, J9CPTYPE_METHOD_TYPE, (J9RAMConstantRef **) &ramMethodTypeRef);
		if (OK == result) {
			J9Class *ramClass = J9CLASS_FROMCPINTERNALRAMCLASS(vmThread, constantPoolOop);
			j9object_t methodTypeObject = J9STATIC_OBJECT_LOAD(vmThread, ramClass, &ramMethodTypeRef->type);

			if (NULL == methodTypeObject) {
				methodTypeObject = vmFunctions->resolveMethodTypeRef(vmThread, J9_CP_FROM_CLASS(ramClass), cpIndex, J9_RESOLVE_FLAG_RUNTIME_RESOLVE);
			}
			if (NULL != methodTypeObject) {
				returnValue = vmFunctions->j9jni_createLocalRef(env, methodTypeObject);
			}
		}
		vmFunctions->internalReleaseVMAccess(vmThread);
	}

	checkResult(env, result);

	return returnValue;
}

/*
 * sun.reflect.ConstantPool doesn't have a getMethodHandleAt method. This is the
 * equivalent for MethodHandle.
 */
jobject JNICALL
Java_java_lang_invoke_MethodHandle_getCPMethodHandleAt(JNIEnv *env, jclass unusedClass, jobject constantPoolOop, jint cpIndex)
{
	jobject returnValue = NULL;
	J9VMThread *vmThread = (J9VMThread *) env;
	J9InternalVMFunctions *vmFunctions = vmThread->javaVM->internalVMFunctions;
	SunReflectCPResult result = NULL_POINTER_EXCEPTION;

	if (NULL != constantPoolOop) {
		J9RAMMethodHandleRef *ramMethodHandleRef = NULL;
		vmFunctions->internalEnterVMFromJNI(vmThread);
		result = getRAMConstantRef(vmThread, constantPoolOop, cpIndex, J9CPTYPE_METHODHANDLE, (J9RAMConstantRef **) &ramMethodHandleRef);
		if (OK == result) {
			J9Class *ramClass = J9CLASS_FROMCPINTERNALRAMCLASS(vmThread, constantPoolOop);
			j9object_t methodHandleObject = J9STATIC_OBJECT_LOAD(vmThread, ramClass, &ramMethodHandleRef->methodHandle);

			if (NULL == methodHandleObject) {
				methodHandleObject = vmFunctions->resolveMethodHandleRef(vmThread, J9_CP_FROM_CLASS(ramClass), cpIndex, J9_RESOLVE_FLAG_RUNTIME_RESOLVE);
			}
			if (NULL != methodHandleObject) {
				returnValue = vmFunctions->j9jni_createLocalRef(env, methodHandleObject);
			}
		}
		vmFunctions->internalReleaseVMAccess(vmThread);
	}

	checkResult(env, result);

	return returnValue;
}

jint JNICALL
Java_jdk_internal_reflect_ConstantPool_getClassRefIndexAt0(JNIEnv *env, jobject unusedObject, jobject constantPoolOop, jint cpIndex)
{
	Assert_JCL_unimplemented();
	return 0;
}

jint JNICALL
Java_jdk_internal_reflect_ConstantPool_getNameAndTypeRefIndexAt0(JNIEnv *env, jobject unusedObject, jobject constantPoolOop, jint cpIndex)
{
	Assert_JCL_unimplemented();
	return 0;
}

jobject JNICALL
Java_jdk_internal_reflect_ConstantPool_getNameAndTypeRefInfoAt0(JNIEnv *env, jobject unusedObject, jobject constantPoolOop, jint cpIndex)
{
	Assert_JCL_unimplemented();
	return NULL;
}

jbyte JNICALL
Java_jdk_internal_reflect_ConstantPool_getTagAt0(JNIEnv *env, jobject unusedObject, jobject constantPoolOop, jint cpIndex)
{
	Assert_JCL_unimplemented();
	return 0;
}

/**
 * Registers natives for jdk.internal.reflect.ConstantPool.
 *
 * Returns 0 on success, negative value on failure.
 */
jint
registerJdkInternalReflectConstantPoolNatives(JNIEnv *env) {
	jint result = 0;
	JNINativeMethod natives[] = {
		{
			(char*)"getSize0",
			(char*)"(Ljava/lang/Object;)I",
			(void *)&Java_sun_reflect_ConstantPool_getSize0,
		},
		{
			(char*)"getClassAt0",
			(char*)"(Ljava/lang/Object;I)Ljava/lang/Class;",
			(void *)&Java_sun_reflect_ConstantPool_getClassAt0,
		},
		{
			(char*)"getClassAtIfLoaded0",
			(char*)"(Ljava/lang/Object;I)Ljava/lang/Class;",
			(void *)&Java_sun_reflect_ConstantPool_getClassAtIfLoaded0,
		},
		{
			(char*)"getMethodAt0",
			(char*)"(Ljava/lang/Object;I)Ljava/lang/reflect/Member;",
			(void *)&Java_sun_reflect_ConstantPool_getMethodAt0,
		},
		{
			(char*)"getMethodAtIfLoaded0",
			(char*)"(Ljava/lang/Object;I)Ljava/lang/reflect/Member;",
			(void *)&Java_sun_reflect_ConstantPool_getMethodAtIfLoaded0,
		},
		{
			(char*)"getFieldAt0",
			(char*)"(Ljava/lang/Object;I)Ljava/lang/reflect/Field;",
			(void *)&Java_sun_reflect_ConstantPool_getFieldAt0,
		},
		{
			(char*)"getFieldAtIfLoaded0",
			(char*)"(Ljava/lang/Object;I)Ljava/lang/reflect/Field;",
			(void *)&Java_sun_reflect_ConstantPool_getFieldAtIfLoaded0,
		},
		{
			(char*)"getMemberRefInfoAt0",
			(char*)"(Ljava/lang/Object;I)[Ljava/lang/String;",
			(void *)&Java_sun_reflect_ConstantPool_getMemberRefInfoAt0,
		},
		{
			(char*)"getIntAt0",
			(char*)"(Ljava/lang/Object;I)I",
			(void *)&Java_sun_reflect_ConstantPool_getIntAt0,
		},
		{
			(char*)"getLongAt0",
			(char*)"(Ljava/lang/Object;I)J",
			(void *)&Java_sun_reflect_ConstantPool_getLongAt0,
		},
		{
			(char*)"getFloatAt0",
			(char*)"(Ljava/lang/Object;I)F",
			(void *)&Java_sun_reflect_ConstantPool_getFloatAt0,
		},
		{
			(char*)"getDoubleAt0",
			(char*)"(Ljava/lang/Object;I)D",
			(void *)&Java_sun_reflect_ConstantPool_getDoubleAt0,
		},
		{
			(char*)"getStringAt0",
			(char*)"(Ljava/lang/Object;I)Ljava/lang/String;",
			(void *)&Java_sun_reflect_ConstantPool_getStringAt0,
		},
		{
			(char*)"getUTF8At0",
			(char*)"(Ljava/lang/Object;I)Ljava/lang/String;",
			(void *)&Java_sun_reflect_ConstantPool_getUTF8At0,
		},
	};

	/* jdk.internal.reflect.ConstantPool is currently cached in CLS_sun_reflect_ConstantPool */
	jclass jdk_internal_reflect_ConstantPool = JCL_CACHE_GET(env, CLS_sun_reflect_ConstantPool);

	if (NULL == jdk_internal_reflect_ConstantPool) {
		BOOLEAN rc = initializeSunReflectConstantPoolIDCache(env);

		if (FALSE == rc) {
			result = -1;
			goto _end;
		}
		jdk_internal_reflect_ConstantPool = JCL_CACHE_GET(env, CLS_sun_reflect_ConstantPool);
		Assert_JCL_true(NULL != jdk_internal_reflect_ConstantPool);
	}

	result = (*env)->RegisterNatives(env, jdk_internal_reflect_ConstantPool, natives, sizeof(natives)/sizeof(JNINativeMethod));

_end:
	return result;
}
