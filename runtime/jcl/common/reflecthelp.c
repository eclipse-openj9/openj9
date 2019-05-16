/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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


#include "j2sever.h"
#include "j9.h"
#include "j9comp.h"
#include "omrgcconsts.h"
#include "j9port.h"
#include "j9protos.h"
#include "jclprots.h"
#include "objhelp.h"
#include "rommeth.h"
#include "ut_j9jcl.h"
#include "util_api.h"
#include "jclglob.h"

#include "vmaccess.h"

#define USE_SUN_REFLECT 1

#include "sunvmi_api.h"

typedef struct FindFieldData {
	J9VMThread *currentThread;
	j9object_t fieldNameObj;
	J9ROMFieldShape *foundField;  /**< [out] field with name matching fieldNameObj */
	J9Class *declaringClass;  /**< [out] class that declares foundField */
} FindFieldData;

typedef struct CountFieldData {
	U_32 fieldCount; /**< [out] */
} CountFieldData;

typedef struct AllFieldData {
	J9VMThread *currentThread;
	J9Class *lookupClass;
	jarray fieldArray; /**< array of fields */
	U_32 fieldIndex; /**< index into fieldArray */
	U_32 restartRequired; /**< restart operation required due to inconsistency between romFieldShape and J9Class->romClass caused by class redefinition */
} AllFieldData;

static J9WalkFieldAction findFieldIterator(J9ROMFieldShape *romField, J9Class *declaringClass, void *userData);
static J9WalkFieldAction countFieldIterator(J9ROMFieldShape *romField, J9Class *declaringClass, void *userData);
static J9WalkFieldAction allFieldIterator(J9ROMFieldShape *romField, J9Class *declaringClass, void *userData);
static jmethodID reflectMethodToID(J9VMThread *vmThread, jobject reflectMethod);

static UDATA
isConstructor(J9Method *ramMethod)
{
	J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(ramMethod);
	return (0 == (romMethod->modifiers & J9AccStatic)) && ('<' == J9UTF8_DATA(J9ROMMETHOD_GET_NAME(methodClass->romClass, romMethod))[0]);
}

/*
 * The following functions implement access to the arrays of SRPs to annotation data.
 * If/when the annotation data is moved, these functions must be updated.
 */
static j9object_t
getAnnotationDataAsByteArray(struct J9VMThread *vmThread, U_32 *annotationData)
{
	U_32 i = 0;
	U_32 byteCount = *annotationData;
	U_8 *byteData = (U_8 *)(annotationData + 1);
	j9object_t byteArray = vmThread->javaVM->memoryManagerFunctions->J9AllocateIndexableObject(
		vmThread, vmThread->javaVM->byteArrayClass, byteCount, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
	if (NULL == byteArray) {
		vmThread->javaVM->internalVMFunctions->setHeapOutOfMemoryError(vmThread);
		return NULL;
	}

	/* TODO this is likely horribly inefficient. Rewrite in Builder. */
	for (i = 0; i < byteCount; ++i) {
		J9JAVAARRAYOFBYTE_STORE(vmThread, byteArray, i, byteData[i]);
	}

	return byteArray;
}

static j9object_t
getAnnotationDataFromROMMethodHelper(struct J9VMThread *vmThread, J9Method *ramMethod, U_32 *(*getAnnotationDataFromROMMethod)(J9ROMMethod *romMethod) )
{
	j9object_t result = NULL;
	J9ROMMethod *romMethod = (J9ROMMethod *) (ramMethod->bytecodes - sizeof(J9ROMMethod));
	U_32 *annotationData = getAnnotationDataFromROMMethod(romMethod);
	if (NULL != annotationData) {
		result = getAnnotationDataAsByteArray(vmThread, annotationData);
	}
	return result;
}

j9object_t
getClassAnnotationData(struct J9VMThread *vmThread, struct J9Class *declaringClass)
{
	j9object_t result = NULL;
	U_32 *annotationData = getClassAnnotationsDataForROMClass(declaringClass->romClass);
	if (NULL != annotationData) {
		result = getAnnotationDataAsByteArray(vmThread, annotationData);
	}
	return result;
}

jbyteArray
getClassTypeAnnotationsAsByteArray(JNIEnv *env, jclass jlClass) {
    jobject result = NULL;
    j9object_t clazz = NULL;
    J9VMThread *vmThread = (J9VMThread *) env;

    enterVMFromJNI(vmThread);
    clazz = J9_JNI_UNWRAP_REFERENCE(jlClass);
    if (NULL != clazz) {
    	struct J9Class *declaringClass = J9VM_J9CLASS_FROM_HEAPCLASS(vmThread, clazz);
    	U_32 *annotationData = getClassTypeAnnotationsDataForROMClass(declaringClass->romClass);
    	if (NULL != annotationData) {
    		j9object_t annotationsByteArray = getAnnotationDataAsByteArray(vmThread, annotationData);
    		if (NULL != annotationsByteArray) {
    			result = vmThread->javaVM->internalVMFunctions->j9jni_createLocalRef(env, annotationsByteArray);
    		}
    	}
    }
    exitVMToJNI(vmThread);
    return result;
}

j9object_t
getFieldAnnotationData(struct J9VMThread *vmThread, struct J9Class *declaringClass, J9JNIFieldID *j9FieldID)
{
	j9object_t result = NULL;
	U_32 *annotationData = getFieldAnnotationsDataFromROMField(j9FieldID->field);
	if ( NULL != annotationData ) {
		result = getAnnotationDataAsByteArray(vmThread, annotationData);
	}
	return result;
}

jbyteArray
getFieldTypeAnnotationsAsByteArray(JNIEnv *env, jobject jlrField) {
    jobject result = NULL;
    j9object_t fieldObject = NULL;
    J9VMThread *vmThread = (J9VMThread *) env;

    enterVMFromJNI(vmThread);
    fieldObject = J9_JNI_UNWRAP_REFERENCE(jlrField);
    if (NULL != fieldObject) {
        J9JNIFieldID *fieldID = vmThread->javaVM->reflectFunctions.idFromFieldObject(vmThread, NULL, fieldObject);
    	U_32 *annotationData = getFieldTypeAnnotationsDataFromROMField(fieldID->field);
    	if ( NULL != annotationData ) {
    		j9object_t annotationsByteArray = getAnnotationDataAsByteArray(vmThread, annotationData);
    		if (NULL != annotationsByteArray) {
    			result = vmThread->javaVM->internalVMFunctions->j9jni_createLocalRef(env, annotationsByteArray);
    		}
    	}
    }
    exitVMToJNI(vmThread);
    return result;
}

/**
 * Looks up a class by name and creates a GlobalRef to maintain
 * a reference to it.
 * @param env
 * @param className The name of the class to find.
 * @return A JNI GlobalRef on success, NULL on failure.
 */
static jclass
findClassAndCreateGlobalRef(JNIEnv *env, char* className)
{
	jclass globalRef = NULL;
	jclass localRef = NULL;
	Trc_JCL_findClassAndCreateGlobalRef_Entry(env, className);

	localRef = (*env)->FindClass(env, className);
	if (localRef == NULL) {
		Trc_JCL_findClassAndCreateGlobalRef_Failed_To_Find_Class(env, className);
		Trc_JCL_findClassAndCreateGlobalRef_ExitError(env);
		return NULL;
	}

	globalRef = (*env)->NewGlobalRef(env, localRef);

	/* Allow the local reference to be garbage collected immediately */
	(*env)->DeleteLocalRef(env, localRef);

	Trc_JCL_findClassAndCreateGlobalRef_ExitOK(env, globalRef);
	return globalRef;
}

/**
 * Computes the number of arguments method has.
 * a reference to it.
 * @param method J9ROMMethod to calculate its number of arguments.
 * @return number of arguments method has.
 */
static U_8
computeArgCount(J9ROMMethod *method)
{
	J9ROMNameAndSignature *methodNAS = &method->nameAndSignature;
	J9UTF8 * signature = J9ROMNAMEANDSIGNATURE_SIGNATURE(methodNAS);
	U_16 count = J9UTF8_LENGTH(signature);
	U_8 * bytes = J9UTF8_DATA(signature);
	U_8 argsCount = 0;
	BOOLEAN done = FALSE;
	U_16 index = 1;

	for (index = 1; (index < count) && (done == FALSE); index++) { /* 1 to skip the opening '(' */
		switch (bytes[index]) {
		case ')':
			done = TRUE;
			break;
		case '[':
			/* skip all '['s */
			while ((index < count) && ('[' == bytes[index])) {
				index += 1;
			}
			if ('L' != bytes[index]) {
				break;
			}
			/* fall through */
		case 'L':
			index += 1;
			while ((index < count) && (';' != bytes[index])) {
				index += 1;
			}
			break;
		default:
			/* all primitive types */
			break;
		}
		if (done == FALSE) {
			argsCount += 1;
		}
	}

	return argsCount;
}

/**
 * Computes the number of arguments method has.
 * a reference to it.
 * @param env
 * @param jlrExecutable Object that represents the method whose parameters are calculated.
 * @return Object array of java.lang.reflect.Parameter
 */
jobjectArray
getMethodParametersAsArray(JNIEnv *env, jobject jlrExecutable)
{
	jobjectArray parametersArray = NULL;
	J9VMThread *vmThread = (J9VMThread *) env;
	J9InternalVMFunctions *vmFuncs = vmThread->javaVM->internalVMFunctions;
	J9JNIMethodID *executableID = NULL;
	jclass jlrParameterClass = NULL;
	jmethodID initMethodID = NULL;

	Trc_JCL_getMethodParametersAsArray_Entry(env, jlrExecutable);

	jlrParameterClass = JCL_CACHE_GET(env, CLS_java_lang_reflect_Parameter);
	if (NULL == jlrParameterClass) {
		jlrParameterClass = findClassAndCreateGlobalRef(env, "java/lang/reflect/Parameter");
		if (NULL == jlrParameterClass) {
			/* No trace point is needed. Exception trace point is thrown in findClassAndCreateGlobalRef */
			goto finished;
		}
		JCL_CACHE_SET(env, CLS_java_lang_reflect_Parameter, jlrParameterClass);
	}
	Trc_JCL_getMethodParametersAsArray_Event1(env, jlrParameterClass);

	initMethodID = JCL_CACHE_GET(env, MID_java_lang_reflect_Parameter_init);
	if (NULL == initMethodID) {
		initMethodID = (*env)->GetMethodID(env, jlrParameterClass, "<init>", "(Ljava/lang/String;ILjava/lang/reflect/Executable;I)V");
		if (NULL == initMethodID) {
			Trc_JCL_getMethodParametersAsArray_Failed_To_getMethodID_For_Parameter_init(env);
			goto finished;
		}
		JCL_CACHE_SET(env, MID_java_lang_reflect_Parameter_init, initMethodID);
	}
	Trc_JCL_getMethodParametersAsArray_Event2(env, initMethodID);

	vmFuncs->internalEnterVMFromJNI(vmThread);
	executableID = (J9JNIMethodID *)reflectMethodToID(vmThread, jlrExecutable);
	vmFuncs->internalExitVMToJNI(vmThread);

	Trc_JCL_getMethodParametersAsArray_Event3(env, executableID);

	if (NULL != executableID) {
		PORT_ACCESS_FROM_VMC(vmThread);
		J9Method *ramMethod = executableID->method;
		J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(ramMethod);
		U_8 numberOfParameters = computeArgCount(romMethod);
		J9MethodParametersData * parametersData = getMethodParametersFromROMMethod(romMethod);
		U_8 index = 0;
		char buffer[256];
		J9MethodParameter * parameters = NULL;
		BOOLEAN parameterListOkay = TRUE;
		U_8 methodParametersLimit = computeArgCount(romMethod);
		U_32 extMods = getExtendedModifiersDataFromROMMethod(romMethod);

		if (NULL == parametersData) {
			/* NULL return value from this method eventually JVM_GetMethodParameters triggers j.l.r.Executable.synthesizeAllParams() */
			goto finished;
		}
		
		if (J9_ARE_ANY_BITS_SET(extMods, CFR_METHOD_EXT_INVALID_CP_ENTRY)) {
			parameterListOkay = FALSE;
		}
		Trc_JCL_getMethodParametersAsArray_Event4(env, parametersData, numberOfParameters);
		if (NULL != parametersData) {
			parameters = &parametersData->parameters;
		}
		parametersArray = (*env)->NewObjectArray(env, numberOfParameters, jlrParameterClass, NULL);
		if (NULL == parametersArray) {
			Trc_JCL_getMethodParametersAsArray_Failed_To_Create_ParametersArray(env, numberOfParameters);
			goto finished;
		}
		Trc_JCL_getMethodParametersAsArray_Event5(env, parametersArray);
		if ((NULL != parametersData) && (methodParametersLimit != parametersData->parameterCount)) {
			/* PR 97987 force an error if mismatch between parameters attribute and actual argument count */
			Trc_JCL_getMethodParametersAsArray_WrongParameterCount(env, parametersData->parameterCount);
			parameterListOkay = FALSE;
		}
		for (index = 0; index < methodParametersLimit; index++) {
		
			U_16 flags = (NULL != parameters) ? parameters[index].flags : 0;
			jstring nameStr = NULL;
			jobject parameterObject = NULL;

			if (parameterListOkay) {
				if ((NULL != parametersData) && (index < parametersData->parameterCount)) {
					/*Get the name of the parameter */
					J9UTF8 * nameUTF = SRP_GET(parameters[index].name, J9UTF8 *);
					if (NULL != nameUTF) {
						char* nameCharArray = buffer;
						UDATA utf8Length = J9UTF8_LENGTH(nameUTF);
						if ((utf8Length + 1) > sizeof(buffer)) {
							nameCharArray = (char*)j9mem_allocate_memory(utf8Length + 1, OMRMEM_CATEGORY_VM);
							if (NULL == nameCharArray) {
								vmFuncs->internalEnterVMFromJNI(vmThread);
								vmFuncs->setNativeOutOfMemoryError(vmThread, 0, 0);
								vmFuncs->internalExitVMToJNI(vmThread);
								Trc_JCL_getMethodParametersAsArray_Failed_To_Allocate_Memory_For_NameCharArray(env, utf8Length + 1);
								goto finished;
							}
						}
						memcpy(nameCharArray, J9UTF8_DATA(nameUTF), utf8Length);
						nameCharArray[utf8Length] = 0;
						Trc_JCL_getMethodParametersAsArray_Event6(env, nameCharArray);

						nameStr = (*env)->NewStringUTF(env, nameCharArray);
						if (nameCharArray != buffer) {
							j9mem_free_memory(nameCharArray);
						}
						if ((*env)->ExceptionCheck(env)) {
							Trc_JCL_getMethodParametersAsArray_Failed_To_Create_NewStringUTF8(env, nameCharArray);
							goto finished;
						}
					} /* else anonymous parameter */
				}

			} else {
				flags = -1; /* force an error if the parameter list is corrupt */
			}
			Trc_JCL_getMethodParametersAsArray_Event7(env, nameStr, flags, index);
			parameterObject = (*env)->NewObject(env, jlrParameterClass, initMethodID, nameStr, flags, jlrExecutable, index );

			if ((*env)->ExceptionCheck(env)) {
				Trc_JCL_getMethodParametersAsArray_Failed_To_Create_java_lang_reflect_Parameter_Object(env, nameStr, flags, index);
				goto finished;
			}
			(*env)->SetObjectArrayElement(env, parametersArray, index, parameterObject);
			(*env)->DeleteLocalRef(env, nameStr);
			(*env)->DeleteLocalRef(env, parameterObject);
		}
	}

finished:
	Trc_JCL_getMethodParametersAsArray_Exit(env, parametersArray);
	return parametersArray;
}


j9object_t
getMethodAnnotationData(struct J9VMThread *vmThread, struct J9Class *declaringClass, J9Method *ramMethod)
{
	return getAnnotationDataFromROMMethodHelper(vmThread, ramMethod, getMethodAnnotationsDataFromROMMethod);
}

j9object_t
getMethodParametersAnnotationData(struct J9VMThread *vmThread, struct J9Class *declaringClass, J9Method *ramMethod)
{
	return getAnnotationDataFromROMMethodHelper(vmThread, ramMethod, getParameterAnnotationsDataFromROMMethod);
}

j9object_t
getMethodDefaultAnnotationData(struct J9VMThread *vmThread, struct J9Class *declaringClass, J9Method *ramMethod)
{
	return getAnnotationDataFromROMMethodHelper(vmThread, ramMethod, getDefaultAnnotationDataFromROMMethod);
}

/*
 * Must be called with VM access
 */
j9object_t
getMethodTypeAnnotationData(struct J9VMThread *vmThread, struct J9Class *declaringClass, J9Method *ramMethod)
{
	return getAnnotationDataFromROMMethodHelper(vmThread, ramMethod, getMethodTypeAnnotationsDataFromROMMethod);
}

J9Class *
fetchArrayClass(struct J9VMThread *vmThread, J9Class *elementTypeClass)
{
	/* Quick check before grabbing the mutex */
	J9Class *resultClass = elementTypeClass->arrayClass;
	if (NULL == resultClass) {
		/* Allocate an array class */
		J9ROMArrayClass* arrayOfObjectsROMClass = (J9ROMArrayClass*)J9ROMIMAGEHEADER_FIRSTCLASS(vmThread->javaVM->arrayROMClasses);

		resultClass = vmThread->javaVM->internalVMFunctions->internalCreateArrayClass(vmThread, arrayOfObjectsROMClass, elementTypeClass);
	}
	return resultClass;
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

static U_32
getArgCountFromSignature(J9UTF8* signature)
{
	U_32 argCount = 0;
	UDATA i = 0;
	U_8 *sigData = J9UTF8_DATA(signature);

	for (i = 1; ')' != sigData[i]; i++, argCount++) { /* 1 to skip the opening '(' */
		/* skip all '['s */
		while ('[' == sigData[i]) {
			i++;
		}
		/* skip class name */
		if ('L' == sigData[i]) {
			while (';' != sigData[i]) {
				i++;
			}
		}
	}

	return argCount;
}

static j9object_t   
parameterTypesForMethod(struct J9VMThread *vmThread, struct J9Method *ramMethod, struct J9Class **returnType)
{
	j9object_t params = NULL;
	J9UTF8 *sigUTF = J9ROMMETHOD_GET_SIGNATURE(methodClass->romClass, J9_ROM_METHOD_FROM_RAM_METHOD(ramMethod));
	J9ClassLoader* classLoader = J9_CLASS_FROM_METHOD(ramMethod)->classLoader;
	U_8 *sigData = J9UTF8_DATA(sigUTF);
	U_32 argCount = getArgCountFromSignature(sigUTF);
	U_32 i = 0;

	/* create the array of classes */
	params = vmThread->javaVM->memoryManagerFunctions->J9AllocateIndexableObject(
		vmThread, fetchArrayClass(vmThread, J9VMJAVALANGCLASS_OR_NULL(vmThread->javaVM)), argCount, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
	if (NULL == params) {
		vmThread->javaVM->internalVMFunctions->setHeapOutOfMemoryError(vmThread);
		return NULL;
	}

	PUSH_OBJECT_IN_SPECIAL_FRAME(vmThread, params);

	/* loop over the characters of the signature */
	sigData++; /* skip '(' */
	for (i = 0; (')' != *sigData) && (NULL != params); i++) {
		J9Class *clazz = classForSignature(vmThread, &sigData, classLoader);
		if (NULL == clazz) {
			DROP_OBJECT_IN_SPECIAL_FRAME(vmThread); /* params */
			return NULL;
		}

		/*
		 * Store the class into the params array.
		 * The class is immortal and the array is newly allocated, so no exception can occur.
		 */
		params = PEEK_OBJECT_IN_SPECIAL_FRAME(vmThread, 0);
		J9JAVAARRAYOFOBJECT_STORE(vmThread, params, i, J9VM_J9CLASS_TO_HEAPCLASS(clazz));
	}

	if (NULL != returnType) {
		J9Class *clazz = NULL;
		sigData++; /* skip ')' */
		clazz = classForSignature(vmThread, &sigData, classLoader);
		if (NULL == clazz) {
			DROP_OBJECT_IN_SPECIAL_FRAME(vmThread); /* params */
			return NULL;
		}

		*returnType = clazz;
	}

	/* Retrieve params from the stack, in case it was moved by the GC */
	params = POP_OBJECT_IN_SPECIAL_FRAME(vmThread);
	return params;
}

static j9object_t   
exceptionTypesForMethod(struct J9VMThread *vmThread, struct J9Method *ramMethod)
{
	j9object_t exceptions = NULL;
	J9ClassLoader* classLoader = J9_CLASS_FROM_METHOD(ramMethod)->classLoader;
	J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(ramMethod);
	J9ExceptionInfo *exceptionData = J9_EXCEPTION_DATA_FROM_ROM_METHOD(romMethod);
	U_32 count = 0;

	if (J9ROMMETHOD_HAS_EXCEPTION_INFO(romMethod)) {
		count = exceptionData->throwCount;
	}

	/* create the array of classes */
	exceptions = vmThread->javaVM->memoryManagerFunctions->J9AllocateIndexableObject(
		vmThread, fetchArrayClass(vmThread, J9VMJAVALANGCLASS_OR_NULL(vmThread->javaVM)), count, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
	if (NULL == exceptions) {
		vmThread->javaVM->internalVMFunctions->setHeapOutOfMemoryError(vmThread);
		return NULL;
	}

	if (0 != count) {
		U_32 i = 0;
		J9SRP *throwNames = J9EXCEPTIONINFO_THROWNAMES(exceptionData);

		PUSH_OBJECT_IN_SPECIAL_FRAME(vmThread, exceptions);

		/* loop over the names */
		for (i = 0; (i < count) && (NULL != exceptions); i++, throwNames++) {
			/* get the name */
			J9UTF8 *throwUTF = NNSRP_PTR_GET(throwNames, J9UTF8 *);

			/* find the class from the name */
			J9Class *clazz = vmThread->javaVM->internalVMFunctions->internalFindClassUTF8(vmThread, J9UTF8_DATA(throwUTF), J9UTF8_LENGTH(throwUTF), classLoader, J9_FINDCLASS_FLAG_THROW_ON_FAIL);
			if (NULL == clazz) {
				DROP_OBJECT_IN_SPECIAL_FRAME(vmThread); /* exceptions */
				return NULL;
			}

			/*
			 * Store the class into the exceptions array.
			 * The class is immortal and the array is newly allocated, so no exception can occur.
			 */
			exceptions = PEEK_OBJECT_IN_SPECIAL_FRAME(vmThread, 0);
			J9JAVAARRAYOFOBJECT_STORE(vmThread, exceptions, i, J9VM_J9CLASS_TO_HEAPCLASS(clazz));
		}

		exceptions = POP_OBJECT_IN_SPECIAL_FRAME(vmThread);
	}

	return exceptions;
}

static void   
fillInReflectMethod(j9object_t methodObject, struct J9Class *declaringClass, jmethodID methodID, struct J9VMThread *vmThread)
{
	J9Class *returnType = NULL;
	j9object_t parameterTypes = NULL;
	j9object_t exceptionTypes = NULL;
	J9Method *ramMethod = ((J9JNIMethodID *) methodID)->method;
	J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(ramMethod);
	J9MemoryManagerFunctions *gcFunctions = vmThread->javaVM->memoryManagerFunctions;
	J9UTF8 *nameUTF = NULL;
	j9object_t nameString = NULL;

	PUSH_OBJECT_IN_SPECIAL_FRAME(vmThread, methodObject);

	exceptionTypes = exceptionTypesForMethod(vmThread, ramMethod);
	if (NULL == exceptionTypes) {
		DROP_OBJECT_IN_SPECIAL_FRAME(vmThread); /* methodObject */
		return;
	}

	methodObject = PEEK_OBJECT_IN_SPECIAL_FRAME(vmThread, 0);
	J9VMJAVALANGREFLECTMETHOD_SET_EXCEPTIONTYPES(vmThread, methodObject, exceptionTypes);

	parameterTypes = parameterTypesForMethod(vmThread, ramMethod, &returnType);
	if (NULL == parameterTypes) {
		DROP_OBJECT_IN_SPECIAL_FRAME(vmThread); /* methodObject */
		return;
	}

	methodObject = PEEK_OBJECT_IN_SPECIAL_FRAME(vmThread, 0);
	J9VMJAVALANGREFLECTMETHOD_SET_PARAMETERTYPES(vmThread, methodObject, parameterTypes);
	J9VMJAVALANGREFLECTMETHOD_SET_RETURNTYPE(vmThread, methodObject, J9VM_J9CLASS_TO_HEAPCLASS(returnType));

	nameUTF = J9ROMMETHOD_GET_NAME(methodClass->romClass, romMethod);
	nameString = gcFunctions->j9gc_createJavaLangString(vmThread, J9UTF8_DATA(nameUTF), (U_32) J9UTF8_LENGTH(nameUTF), J9_STR_INTERN);
	if (NULL == nameString) {
		DROP_OBJECT_IN_SPECIAL_FRAME(vmThread); /* methodObject */
		return;
	}

	methodObject = PEEK_OBJECT_IN_SPECIAL_FRAME(vmThread, 0);
	J9VMJAVALANGREFLECTMETHOD_SET_NAME(vmThread, methodObject, nameString);

	if (J9ROMMETHOD_HAS_GENERIC_SIGNATURE(romMethod)) {
		J9UTF8 *sigUTF = J9_GENERIC_SIGNATURE_FROM_ROM_METHOD(romMethod);
		j9object_t sigString = gcFunctions->j9gc_createJavaLangString(vmThread, J9UTF8_DATA(sigUTF), (U_32) J9UTF8_LENGTH(sigUTF), 0);
		if (NULL == sigString) {
			DROP_OBJECT_IN_SPECIAL_FRAME(vmThread); /* methodObject */
			return;
		}

		methodObject = PEEK_OBJECT_IN_SPECIAL_FRAME(vmThread, 0);

#if defined(USE_SUN_REFLECT)
		J9VMJAVALANGREFLECTMETHOD_SET_SIGNATURE(vmThread, methodObject, sigString);
#endif
	}

	{
		j9object_t byteArray = getMethodAnnotationData(vmThread, declaringClass, ramMethod);
		if (NULL != vmThread->currentException) {
			DROP_OBJECT_IN_SPECIAL_FRAME(vmThread); /* methodObject */
			return;
		}
		if (NULL != byteArray) {
			methodObject = PEEK_OBJECT_IN_SPECIAL_FRAME(vmThread, 0);
			J9VMJAVALANGREFLECTMETHOD_SET_ANNOTATIONS(vmThread, methodObject, byteArray);
		}

		byteArray = getMethodParametersAnnotationData(vmThread, declaringClass, ramMethod);
		if (NULL != vmThread->currentException) {
			DROP_OBJECT_IN_SPECIAL_FRAME(vmThread); /* methodObject */
			return;
		}
		if (NULL != byteArray) {
			methodObject = PEEK_OBJECT_IN_SPECIAL_FRAME(vmThread, 0);
			J9VMJAVALANGREFLECTMETHOD_SET_PARAMETERANNOTATIONS(vmThread, methodObject, byteArray);
		}

		byteArray = getMethodDefaultAnnotationData(vmThread, declaringClass, ramMethod);
		if (NULL != vmThread->currentException) {
			DROP_OBJECT_IN_SPECIAL_FRAME(vmThread); /* methodObject */
			return;
		}
		if (NULL != byteArray) {
			methodObject = PEEK_OBJECT_IN_SPECIAL_FRAME(vmThread, 0);
			J9VMJAVALANGREFLECTMETHOD_SET_ANNOTATIONDEFAULT(vmThread, methodObject, byteArray);
		}
	}

	methodObject = POP_OBJECT_IN_SPECIAL_FRAME(vmThread);

	J9VMJAVALANGREFLECTMETHOD_SET_DECLARINGCLASS(vmThread, methodObject, J9VM_J9CLASS_TO_HEAPCLASS(declaringClass));

	/* Java 7 uses int field and method ids to avoid modifying Sun JCL code */
	J9VMJAVALANGREFLECTMETHOD_SET_INTMETHODID(vmThread, methodObject, (U_32)getMethodIndex(ramMethod));

#if defined(USE_SUN_REFLECT) 
	J9VMJAVALANGREFLECTMETHOD_SET_MODIFIERS(vmThread, methodObject, romMethod->modifiers & CFR_METHOD_ACCESS_MASK);
#endif

}


static j9object_t   
createField(struct J9VMThread *vmThread, jfieldID fieldID)
{
	J9JNIFieldID *j9FieldID = (J9JNIFieldID *)fieldID;
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

static j9object_t   
createMethod(struct J9VMThread *vmThread, J9JNIMethodID *methodID, j9object_t parameterTypes)
{
	J9Class *jlrMethodClass = NULL;
	j9object_t methodObject = NULL;
	J9Class *declaringClass = J9_CLASS_FROM_METHOD(methodID->method);
	UDATA initStatus;

	PUSH_OBJECT_IN_SPECIAL_FRAME(vmThread, parameterTypes);
	jlrMethodClass = J9VMJAVALANGREFLECTMETHOD(vmThread->javaVM);

	if (NULL == jlrMethodClass) {
		DROP_OBJECT_IN_SPECIAL_FRAME(vmThread); /* parameterTypes */
		return NULL;
	}

	initStatus = jlrMethodClass->initializeStatus;
	if (initStatus != J9ClassInitSucceeded && initStatus != (UDATA) vmThread) {
		vmThread->javaVM->internalVMFunctions->initializeClass(vmThread, jlrMethodClass);
		if (vmThread->currentException != NULL) {
			DROP_OBJECT_IN_SPECIAL_FRAME(vmThread); /* parameterTypes */
			return NULL;
		}
	}

	/* allocate a method object */
	methodObject = vmThread->javaVM->memoryManagerFunctions->J9AllocateObject(vmThread, jlrMethodClass, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
	if (NULL == methodObject) {
		vmThread->javaVM->internalVMFunctions->setHeapOutOfMemoryError(vmThread);
		DROP_OBJECT_IN_SPECIAL_FRAME(vmThread); /* parameterTypes */
		return NULL;
	}

	PUSH_OBJECT_IN_SPECIAL_FRAME(vmThread, methodObject);
	fillInReflectMethod(methodObject, declaringClass, (jmethodID) methodID, vmThread);
	methodObject = POP_OBJECT_IN_SPECIAL_FRAME(vmThread);

	/* Forget the methodObject if fillInReflectMethod raised an exception */
	if (NULL != vmThread->currentException) {
		methodObject = NULL;
	}

	DROP_OBJECT_IN_SPECIAL_FRAME(vmThread); /* parameterTypes */
	return methodObject;
}

static j9object_t   
createConstructor(struct J9VMThread *vmThread, J9JNIMethodID *methodID, j9object_t parameterTypes)
{
	j9object_t exceptionTypes = NULL;
	J9Method *ramMethod = methodID->method;
	J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(ramMethod);
	J9Class *jlrConstructorClass = J9VMJAVALANGREFLECTCONSTRUCTOR(vmThread->javaVM);
	j9object_t constructorObject = NULL;
	J9Class *declaringClass = J9_CLASS_FROM_METHOD(ramMethod);
	UDATA initStatus;

	if (NULL == jlrConstructorClass) {
		return NULL;
	}

	initStatus = jlrConstructorClass->initializeStatus;
	if (initStatus != J9ClassInitSucceeded && initStatus != (UDATA) vmThread) {
		vmThread->javaVM->internalVMFunctions->initializeClass(vmThread, jlrConstructorClass);
		if (vmThread->currentException != NULL) {
			return NULL;
		}
	}

	constructorObject = vmThread->javaVM->memoryManagerFunctions->J9AllocateObject(vmThread, jlrConstructorClass, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
	if (NULL == constructorObject) {
		vmThread->javaVM->internalVMFunctions->setHeapOutOfMemoryError(vmThread);
		return NULL;
	}

	PUSH_OBJECT_IN_SPECIAL_FRAME(vmThread, constructorObject);
	parameterTypes = parameterTypesForMethod(vmThread, ramMethod, NULL);
	if (NULL == parameterTypes) {
		DROP_OBJECT_IN_SPECIAL_FRAME(vmThread); /* constructorObject */
		return NULL;
	}
	constructorObject = PEEK_OBJECT_IN_SPECIAL_FRAME(vmThread, 0);
	J9VMJAVALANGREFLECTCONSTRUCTOR_SET_PARAMETERTYPES(vmThread, constructorObject, parameterTypes);

	exceptionTypes = exceptionTypesForMethod(vmThread, ramMethod);
	if (NULL == exceptionTypes) {
		DROP_OBJECT_IN_SPECIAL_FRAME(vmThread); /* constructorObject */
		return NULL;
	}

	constructorObject = PEEK_OBJECT_IN_SPECIAL_FRAME(vmThread, 0);
	J9VMJAVALANGREFLECTCONSTRUCTOR_SET_EXCEPTIONTYPES(vmThread, constructorObject, exceptionTypes);

	if (J9ROMMETHOD_HAS_GENERIC_SIGNATURE(romMethod)) {
		J9UTF8 *sigUTF = J9_GENERIC_SIGNATURE_FROM_ROM_METHOD(romMethod);
		j9object_t sigString = vmThread->javaVM->memoryManagerFunctions->j9gc_createJavaLangString(vmThread, J9UTF8_DATA(sigUTF), (U_32) J9UTF8_LENGTH(sigUTF), 0);
		if (NULL == sigString) {
			DROP_OBJECT_IN_SPECIAL_FRAME(vmThread); /* constructorObject */
			return NULL;
		}

		constructorObject = PEEK_OBJECT_IN_SPECIAL_FRAME(vmThread, 0);
#if defined(USE_SUN_REFLECT)
		J9VMJAVALANGREFLECTCONSTRUCTOR_SET_SIGNATURE(vmThread, constructorObject, sigString);
#endif
	}

	{
		j9object_t byteArray = getMethodAnnotationData(vmThread, declaringClass, ramMethod);
		if (NULL != vmThread->currentException) {
			DROP_OBJECT_IN_SPECIAL_FRAME(vmThread); /* constructorObject */
			return NULL;
		}
		if (NULL != byteArray) {
			constructorObject = PEEK_OBJECT_IN_SPECIAL_FRAME(vmThread, 0);
			J9VMJAVALANGREFLECTCONSTRUCTOR_SET_ANNOTATIONS(vmThread, constructorObject, byteArray);
		}

		byteArray = getMethodParametersAnnotationData(vmThread, declaringClass, ramMethod);
		if (NULL != vmThread->currentException) {
			DROP_OBJECT_IN_SPECIAL_FRAME(vmThread); /* constructorObject */
			return NULL;
		}
		if (NULL != byteArray) {
			constructorObject = PEEK_OBJECT_IN_SPECIAL_FRAME(vmThread, 0);
			J9VMJAVALANGREFLECTCONSTRUCTOR_SET_PARAMETERANNOTATIONS(vmThread, constructorObject, byteArray);
		}
	}

	constructorObject = POP_OBJECT_IN_SPECIAL_FRAME(vmThread);

	J9VMJAVALANGREFLECTCONSTRUCTOR_SET_DECLARINGCLASS(vmThread, constructorObject, J9VM_J9CLASS_TO_HEAPCLASS(declaringClass));

	J9VMJAVALANGREFLECTCONSTRUCTOR_SET_INTMETHODID(vmThread, constructorObject, (U_32)getMethodIndex(ramMethod));

#if defined(USE_SUN_REFLECT) 
	J9VMJAVALANGREFLECTCONSTRUCTOR_SET_MODIFIERS(vmThread, constructorObject, romMethod->modifiers & CFR_METHOD_ACCESS_MASK);
#endif

	return constructorObject;
}

static struct J9JNIFieldID *
idFromFieldObject(J9VMThread* vmThread, j9object_t declaringClassObject, j9object_t fieldObject)
{
	J9JNIFieldID *fieldID = NULL;
	J9Class *declaringClass = NULL;
	U_32 index = J9VMJAVALANGREFLECTFIELD_INTFIELDID(vmThread, fieldObject);
	if (NULL == declaringClassObject) {
		j9object_t declaringClassObjectTmp = J9VMJAVALANGREFLECTFIELD_DECLARINGCLASS(vmThread, fieldObject);
		declaringClass = J9VM_J9CLASS_FROM_HEAPCLASS(vmThread, declaringClassObjectTmp);
	} else {
		declaringClass = J9VM_J9CLASS_FROM_HEAPCLASS(vmThread, declaringClassObject);
	}
	fieldID = (J9JNIFieldID *)(declaringClass->jniIDs[index]);

	return fieldID;
}

static struct J9JNIMethodID *
idFromMethodObject(J9VMThread* vmThread, j9object_t methodObject)
{
	J9JNIMethodID *methodID = NULL;
	U_32 index = J9VMJAVALANGREFLECTMETHOD_INTMETHODID(vmThread, methodObject);
	j9object_t declaringClassObject = J9VMJAVALANGREFLECTMETHOD_DECLARINGCLASS(vmThread, methodObject);
	J9Class *declaringClass = J9VM_J9CLASS_FROM_HEAPCLASS(vmThread, declaringClassObject);
	methodID = (J9JNIMethodID *)(declaringClass->jniIDs[index]);

	return methodID;
}

static struct J9JNIMethodID *
idFromConstructorObject(J9VMThread* vmThread, j9object_t constructorObject)
{
	J9JNIMethodID *methodID = NULL;
	U_32 index = J9VMJAVALANGREFLECTCONSTRUCTOR_INTMETHODID(vmThread, constructorObject);
	j9object_t declaringClassObject = J9VMJAVALANGREFLECTCONSTRUCTOR_DECLARINGCLASS(vmThread, constructorObject);
	J9Class *declaringClass = J9VM_J9CLASS_FROM_HEAPCLASS(vmThread, declaringClassObject);
	methodID = (J9JNIMethodID *)(declaringClass->jniIDs[index]);

	return methodID;
}

static jobject
idToReflectField(J9VMThread* vmThread, jfieldID fieldID)
{
	jobject result = NULL;
	enterVMFromJNI(vmThread);
	if (NULL == fieldID) {
		vmThread->javaVM->internalVMFunctions->setHeapOutOfMemoryError(vmThread);
	} else {
		{
			j9object_t fieldObject = createField(vmThread, fieldID);
			if (NULL != fieldObject) {
				result = vmThread->javaVM->internalVMFunctions->j9jni_createLocalRef((JNIEnv *)vmThread, fieldObject);
				if (NULL == result) {
					vmThread->javaVM->internalVMFunctions->setNativeOutOfMemoryError(vmThread, 0, 0);
				}
			}
		}
	}
	exitVMToJNI(vmThread);
	return result;
}

static jobject
idToReflectMethod(J9VMThread* vmThread, jmethodID methodID)
{
	jobject result = NULL;
	enterVMFromJNI(vmThread);
	if (NULL == methodID) {
		vmThread->javaVM->internalVMFunctions->setHeapOutOfMemoryError(vmThread);
	} else {
		j9object_t methodObject = NULL;
		J9JNIMethodID *j9MethodID = (J9JNIMethodID *) methodID;

		if (isConstructor(j9MethodID->method)) {
			methodObject = createConstructor(vmThread, j9MethodID, NULL);
		} else {
			methodObject = createMethod(vmThread, j9MethodID, NULL);
		}
		if (NULL != methodObject) {
			result = vmThread->javaVM->internalVMFunctions->j9jni_createLocalRef((JNIEnv *)vmThread, methodObject);
			if (NULL == result) {
				vmThread->javaVM->internalVMFunctions->setNativeOutOfMemoryError(vmThread, 0, 0);
			}
		}
	}
	exitVMToJNI(vmThread);
	return result;
}

static j9object_t
createMethodObject(struct J9Method *ramMethod, struct J9Class *declaringClass, j9array_t parameterTypes, struct J9VMThread* vmThread)
{
	j9object_t method = NULL;
	J9JNIMethodID *methodID = vmThread->javaVM->internalVMFunctions->getJNIMethodID(vmThread, ramMethod);
	if (NULL != methodID) {
		method = createMethod(vmThread, methodID, (j9object_t) parameterTypes);
	}
	return method;
}

static j9object_t
createConstructorObject(struct J9Method *ramMethod, struct J9Class *declaringClass, j9array_t parameterTypes, struct J9VMThread* vmThread)
{
	j9object_t method = NULL;
	J9JNIMethodID *methodID = vmThread->javaVM->internalVMFunctions->getJNIMethodID(vmThread, ramMethod);
	if (NULL != methodID) {
		method = createConstructor(vmThread, methodID, (j9object_t) parameterTypes);
	}
	return method;
}

static j9object_t
createInstanceFieldObject(struct J9ROMFieldShape *romField, struct J9Class *declaringClass, struct J9Class *lookupClass, struct J9VMThread* vmThread, UDATA *inconsistentData)
{
	const J9InternalVMFunctions *vmFuncs = vmThread->javaVM->internalVMFunctions;
	J9UTF8 *nameUTF = J9ROMFIELDSHAPE_NAME(romField);
	J9UTF8 *sigUTF = J9ROMFIELDSHAPE_SIGNATURE(romField);
	IDATA offset = vmFuncs->instanceFieldOffset(vmThread, declaringClass, J9UTF8_DATA(nameUTF), J9UTF8_LENGTH(nameUTF), J9UTF8_DATA(sigUTF), J9UTF8_LENGTH(sigUTF), NULL, NULL, 0);
	jfieldID fieldID = (jfieldID) vmFuncs->getJNIFieldID(vmThread, declaringClass, romField, (UDATA) offset, inconsistentData);
	j9object_t field = NULL;
	if (NULL != fieldID) {
		field = createField(vmThread, fieldID);
	}
	return field;
}

static j9object_t  
createStaticFieldObject(struct J9ROMFieldShape *romField, struct J9Class *declaringClass, struct J9Class *lookupClass, struct J9VMThread* vmThread, UDATA *inconsistentData)
{
	const J9InternalVMFunctions *vmFuncs = vmThread->javaVM->internalVMFunctions;
	j9object_t field = NULL;
	jfieldID fieldID = NULL;
	J9UTF8 *nameUTF = J9ROMFIELDSHAPE_NAME(romField);
	J9UTF8 *sigUTF = J9ROMFIELDSHAPE_SIGNATURE(romField);
	void *offset = vmFuncs->staticFieldAddress(vmThread, declaringClass, J9UTF8_DATA(nameUTF), J9UTF8_LENGTH(nameUTF), J9UTF8_DATA(sigUTF), J9UTF8_LENGTH(sigUTF), NULL, NULL, 0, NULL);
	if (NULL != offset) {
		fieldID = (jfieldID) vmFuncs->getJNIFieldID(vmThread, declaringClass, romField, (UDATA) offset - (UDATA) declaringClass->ramStatics, inconsistentData);
	}
	if (NULL != fieldID) {
		field = createField(vmThread, fieldID);
	}
	return field;
}

static jfieldID
reflectFieldToID(J9VMThread *vmThread, jobject reflectField)
{
	/* Since reflect uses JNI IDs in its underlying representation, this call cannot fail unless the field is bogus. */
	jfieldID result = NULL;
	j9object_t field = NULL;

	field = J9_JNI_UNWRAP_REFERENCE(reflectField);
	if (NULL != field) {
		result = (jfieldID) idFromFieldObject(vmThread, NULL, field);
	}
	return result;
}

static jmethodID
reflectMethodToID(J9VMThread *vmThread, jobject reflectMethod)
{
	/* Since reflect uses JNI IDs in its underlying representation, this call cannot fail unless the method is bogus. */
	jmethodID result = NULL;
	j9object_t method = NULL;

	method = J9_JNI_UNWRAP_REFERENCE(reflectMethod);
	if (NULL != method) {
		if (J9OBJECT_CLAZZ(vmThread, method) == J9VMJAVALANGREFLECTCONSTRUCTOR_OR_NULL(vmThread->javaVM)) {
			result = (jmethodID) idFromConstructorObject(vmThread, method);
		} else {
			result = (jmethodID) idFromMethodObject(vmThread, method);
		}
	}
	return result;
}

void
preloadReflectWrapperClasses(J9JavaVM *javaVM)
{
	UDATA i = 0;
	enterVMFromJNI(javaVM->mainThread);
	/* TODO should the condition be <= ? i.e. should java.lang.Void also be initialized? */
	for (i = J9VMCONSTANTPOOL_JAVALANGBYTE; i < J9VMCONSTANTPOOL_JAVALANGVOID; i++) {
		(void) javaVM->internalVMFunctions->internalFindKnownClass(javaVM->mainThread, i, J9_FINDKNOWNCLASS_FLAG_INITIALIZE);
	}
	(void) javaVM->internalVMFunctions->internalFindKnownClass(javaVM->mainThread, J9VMCONSTANTPOOL_JAVALANGREFLECTINVOCATIONTARGETEXCEPTION, J9_FINDKNOWNCLASS_FLAG_INITIALIZE);
	exitVMToJNI(javaVM->mainThread);
}

void
initializeReflection(J9JavaVM *javaVM)
{
	J9ReflectFunctionTable *reflectFunctions = &(javaVM->reflectFunctions);
	reflectFunctions->idToReflectMethod = idToReflectMethod;
	reflectFunctions->idToReflectField = idToReflectField;
	reflectFunctions->reflectMethodToID = reflectMethodToID;
	reflectFunctions->reflectFieldToID = reflectFieldToID;
	reflectFunctions->createConstructorObject = createConstructorObject;
	reflectFunctions->createDeclaredConstructorObject = createConstructorObject;
	reflectFunctions->createDeclaredMethodObject = createMethodObject;
	reflectFunctions->createMethodObject = createMethodObject;
	reflectFunctions->fillInReflectMethod = fillInReflectMethod;
	reflectFunctions->idFromFieldObject = idFromFieldObject;
	reflectFunctions->idFromMethodObject = idFromMethodObject;
	reflectFunctions->idFromConstructorObject = idFromConstructorObject;
}

/**
 * (method was formerly in Smalltalk as:  J9VMJavaLangReflectArrayNatives>>multiNewArrayImpl)
 */
jobject JNICALL
Java_java_lang_reflect_Array_multiNewArrayImpl(JNIEnv *env, jclass unusedClass, jclass componentType, jint dimensions, jintArray dimensionsArray)
{
	jobject result = NULL;

	J9VMThread *vmThread = (J9VMThread *) env;
	J9Object *componentTypeClassObject = NULL;

	enterVMFromJNI(vmThread);
	componentTypeClassObject = J9_JNI_UNWRAP_REFERENCE(componentType);
	if (NULL != componentTypeClassObject) {
		J9Class *componentTypeClass = J9VM_J9CLASS_FROM_HEAPCLASS(vmThread, componentTypeClassObject);

		/* create an array class with one greater arity than desired */
		UDATA count = dimensions + 1;
		BOOLEAN exceptionIsPending = FALSE;

		if (J9ROMCLASS_IS_ARRAY(componentTypeClass->romClass) && ((((J9ArrayClass *)componentTypeClass)->arity + dimensions) > J9_ARRAY_DIMENSION_LIMIT)) {
			/* The spec says to throw IllegalArgumentException if the number of dimensions is greater than J9_ARRAY_DIMENSION_LIMIT */
			exitVMToJNI(vmThread);
			throwNewIllegalArgumentException(env, NULL);
			goto exit;
		}

		while ((count > 0) && (!exceptionIsPending)) {
			componentTypeClass = fetchArrayClass(vmThread, componentTypeClass);
			exceptionIsPending = (NULL != vmThread->currentException);
			count -= 1;
		}
		
		if (!exceptionIsPending) {
			/* make a copy of the dimensions array in non-object memory */
			I_32 onStackDimensions[J9_ARRAY_DIMENSION_LIMIT];
			j9object_t directObject = NULL;
			J9Object *dimensionsArrayObject = J9_JNI_UNWRAP_REFERENCE(dimensionsArray);
			UDATA i = 0;

			Assert_JCL_true(dimensions == J9INDEXABLEOBJECT_SIZE(vmThread, dimensionsArrayObject));
			memset(onStackDimensions, 0, sizeof(onStackDimensions));
			for (i = 0; i < (UDATA)dimensions; i++) {
				onStackDimensions[i] = J9JAVAARRAYOFINT_LOAD(vmThread, dimensionsArrayObject, i);
			}
			
			directObject = vmThread->javaVM->internalVMFunctions->helperMultiANewArray(vmThread, (J9ArrayClass *)componentTypeClass, (UDATA)dimensions, onStackDimensions, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
			if (NULL != directObject) {
				result = vmThread->javaVM->internalVMFunctions->j9jni_createLocalRef(env, directObject);
			}
		}
	}
	exitVMToJNI(vmThread);
exit:
	return result;
}

/* The caller must have VM access. */
j9object_t
getFieldObjHelper(J9VMThread *vmThread, jclass declaringClass, jstring fieldName)
{
	J9InternalVMFunctions *vmFuncs = vmThread->javaVM->internalVMFunctions;
	J9Class *clazz = NULL;
	J9ROMClass *romClass = NULL;
	J9ROMFieldShape *romField = NULL;
	j9object_t fieldNameObj = NULL;
	j9object_t fieldObj = NULL;
	J9ROMFieldWalkState state;

	Assert_JCL_mustHaveVMAccess(vmThread);

	if ((NULL == fieldName) || (NULL == declaringClass)) {
		goto nullpointer;
	}
	fieldNameObj = J9_JNI_UNWRAP_REFERENCE(fieldName);
	if (NULL == fieldNameObj) {
		goto nullpointer;
	}
	clazz = J9VM_J9CLASS_FROM_JCLASS(vmThread, declaringClass);
	Assert_JCL_notNull(clazz);
	romClass = clazz->romClass;

	/* primitives/arrays don't have fields */
	if (J9ROMCLASS_IS_PRIMITIVE_OR_ARRAY(romClass)) {
		goto nosuchfield;
	}

	/* loop over the fields */
	memset(&state, 0, sizeof(state));
	romField = romFieldsStartDo(romClass, &state);
	while (NULL != romField) {
		J9UTF8 *romFieldName = J9ROMFIELDSHAPE_NAME(romField);

		/* compare name with this field */
		if (0 != vmFuncs->compareStringToUTF8(vmThread, fieldNameObj, 0, J9UTF8_DATA(romFieldName), J9UTF8_LENGTH(romFieldName))) {
			UDATA inconsistentData = 0;

			if (romField->modifiers & J9AccStatic) {
				/* create static field object */
				fieldObj = createStaticFieldObject(romField, clazz, clazz, vmThread, &inconsistentData);
			} else {
				/* create instance field object */
				fieldObj = createInstanceFieldObject(romField, clazz, clazz, vmThread, &inconsistentData);
			}
			if (NULL != vmThread->currentException) {
				goto done;
			}
			/* There is no window here for redefinition to cause a mismatch
			 * between declaringClass->romClass and the romField in this code.
			 */
			Assert_JCL_true(0 == inconsistentData);

			if (NULL == fieldObj) {
				goto heapoutofmemory;
			}

			goto done;
		}

		romField = romFieldsNextDo(&state);
	}
	/* fall through to nosuchfield - no match found */
nosuchfield:
	vmFuncs->setCurrentException(vmThread, J9VMCONSTANTPOOL_JAVALANGNOSUCHFIELDEXCEPTION, (UDATA *)*(j9object_t *)fieldName);
	goto done;

nullpointer:
	vmFuncs->setCurrentException(vmThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	goto done;

heapoutofmemory:
	vmFuncs->setHeapOutOfMemoryError(vmThread);
	goto done;

done:
	return fieldObj;
}

jobject
getDeclaredFieldHelper(JNIEnv *env, jobject declaringClass, jstring fieldName)
{
	J9VMThread *vmThread = (J9VMThread *)env;
	J9InternalVMFunctions *vmFuncs = vmThread->javaVM->internalVMFunctions;
	jobject result = NULL;
	j9object_t fieldObj = NULL;

	vmFuncs->internalEnterVMFromJNI(vmThread);
	fieldObj = getFieldObjHelper(vmThread, (jclass)declaringClass, fieldName);
	if (NULL != fieldObj) {
		result = vmFuncs->j9jni_createLocalRef(env, fieldObj);
		if (NULL == result) {
			vmFuncs->setNativeOutOfMemoryError(vmThread, 0, 0);
		}
	}
	vmFuncs->internalExitVMToJNI(vmThread);
	return result;
}

jarray
getDeclaredFieldsHelper(JNIEnv *env, jobject declaringClass)
{
	J9VMThread *vmThread = (J9VMThread *)env;
	J9JavaVM *vm = vmThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	jarray result = NULL;
	J9Class *clazz = NULL;
	J9ROMClass *romClass = NULL;
	U_32 fieldCount = 0;
	j9array_t fieldArrayObject = NULL;
	J9Class *fieldArrayClass = NULL;
	J9Class *fieldClass = NULL;
	UDATA preCount = 0;

	vmFuncs->internalEnterVMFromJNI(vmThread);
	
	/* allocate an array of Field objects */
	fieldClass = J9VMJAVALANGREFLECTFIELD(vm);
	if (NULL != vmThread->currentException) {
		goto done;
	}

	fieldArrayClass = fetchArrayClass(vmThread, fieldClass);
	if (NULL != vmThread->currentException) {
		goto done;
	}
	
retry:
	preCount = vm->hotSwapCount;
	clazz = J9VM_J9CLASS_FROM_JCLASS(vmThread, (jclass)declaringClass);
	romClass = clazz->romClass;

	/* primitives/arrays don't have fields */
	if (FALSE == J9ROMCLASS_IS_PRIMITIVE_OR_ARRAY(romClass)) {
		fieldCount = romClass->romFieldCount;
	}

	fieldArrayObject = (j9array_t)vm->memoryManagerFunctions->J9AllocateIndexableObject(vmThread, fieldArrayClass,
			fieldCount, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
	if (vm->hotSwapCount != preCount) {
		goto retry;
	} else if (NULL == fieldArrayObject) {
		goto heapoutofmemory;
	}

	result = vmFuncs->j9jni_createLocalRef(env, (j9object_t)fieldArrayObject);
	if (NULL == result) {
		goto nativeoutofmemory;
	}

	if (fieldCount > 0) {
		J9ROMFieldShape *romField = NULL;
		J9ROMFieldWalkState state;
		U_32 fieldIndex = 0;

		/* loop over the fields */
		memset(&state, 0, sizeof(state));
		romField = romFieldsStartDo(romClass, &state);
		while (NULL != romField) {
			UDATA inconsistentData = 0;
			j9object_t fieldObject = NULL;

			if (romField->modifiers & J9AccStatic) {
				/* create static field object */
				fieldObject = createStaticFieldObject(romField, clazz, clazz, vmThread, &inconsistentData);
			} else {
				/* create instance field object */
				fieldObject = createInstanceFieldObject(romField, clazz, clazz, vmThread, &inconsistentData);
			}

			if (NULL != vmThread->currentException) {
				goto done;
			}

			/* Must check for inconsistentData due to hotswap before checking for null
			 * fieldObject as getJNIFieldID may return null if the romFieldShape and
			 * declaringClass are mismatched to redefinition.
			 */
			if (0 != inconsistentData) {
				vmFuncs->j9jni_deleteLocalRef(env, result);
				result = NULL;
				goto retry;
			}

			if (NULL == fieldObject) {
				goto heapoutofmemory;
			}

			/* refetch array object because vmaccess could have been released */
			fieldArrayObject = (j9array_t)J9_JNI_UNWRAP_REFERENCE(result);

			/* Storing newly allocated objects into newly allocated array. Both in same space, so no exception can occur. */
			J9JAVAARRAYOFOBJECT_STORE(vmThread, fieldArrayObject, fieldIndex, fieldObject);

			fieldIndex += 1;

			romField = romFieldsNextDo(&state);
		} /* while (NULL != romField) */
	} /* if (fieldCount > 0) */
	goto done;

nativeoutofmemory:
	vmFuncs->setNativeOutOfMemoryError(vmThread, 0, 0);
	goto done;

heapoutofmemory:
	vmFuncs->setHeapOutOfMemoryError(vmThread);
	goto done;

done:
	vmFuncs->internalExitVMToJNI(vmThread);
	return result;
}

static J9WalkFieldAction
findFieldIterator(J9ROMFieldShape *romField, J9Class *declaringClass, void *userData)
{
	if (romField->modifiers & J9AccPublic) {
		J9UTF8 *fieldName = J9ROMFIELDSHAPE_NAME(romField);
		FindFieldData *data = userData;
		J9JavaVM *vm = data->currentThread->javaVM;

		/* compare name with this field */
		if (0 != vm->internalVMFunctions->compareStringToUTF8(data->currentThread, data->fieldNameObj, 0, J9UTF8_DATA(fieldName), J9UTF8_LENGTH(fieldName))) {
			data->foundField = romField;
			data->declaringClass = declaringClass;
			return J9WalkFieldActionStop;
		}
	}

	return J9WalkFieldActionContinue;
}

jobject
getFieldHelper(JNIEnv *env, jobject cls, jstring fieldName)
{
	J9VMThread *vmThread = (J9VMThread *)env;
	J9JavaVM *vm = vmThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	jarray result = NULL;
	J9Class *clazz = NULL;
	J9ROMClass *romClass = NULL;
	j9object_t fieldNameObj = NULL;
	J9WalkFieldHierarchyState state;
	FindFieldData userData;

	vmFuncs->internalEnterVMFromJNI(vmThread);

	if (NULL == fieldName) {
		goto nullpointer;
	}

	fieldNameObj = J9_JNI_UNWRAP_REFERENCE(fieldName);
	if (NULL == fieldNameObj) {
		goto nullpointer;
	}

	clazz = J9VM_J9CLASS_FROM_JCLASS(vmThread, (jclass)cls);
	romClass = clazz->romClass;

	/* primitives/arrays don't have fields */
	if (J9ROMCLASS_IS_PRIMITIVE_OR_ARRAY(romClass)) {
		goto nosuchfield;
	}

	state.fieldCallback = findFieldIterator;
	state.userData = &userData;
	userData.currentThread = vmThread;
	userData.fieldNameObj = fieldNameObj;
	userData.foundField = NULL;
	userData.declaringClass = NULL;
	walkFieldHierarchyDo(clazz, &state);

	if (NULL != userData.foundField) {
		UDATA inconsistentData = 0;
		J9ROMFieldShape *romField = userData.foundField;
		j9object_t fieldObj = NULL;

		if (romField->modifiers & J9AccStatic) {
			/* create static field object */
			fieldObj = createStaticFieldObject(romField, userData.declaringClass, clazz, vmThread, &inconsistentData);
		} else {
			/* create instance field object */
			fieldObj = createInstanceFieldObject(romField, userData.declaringClass, clazz, vmThread, &inconsistentData);
		}

		if (NULL != vmThread->currentException) {
			goto done;
		}

		/* No opportunity for hotswap to cause declaringClass->romClass
		 * and romField to be from different romclasses here.
		 */
		Assert_JCL_true(0 == inconsistentData);

		if (NULL == fieldObj) {
			goto heapoutofmemory;
		}

		result = vmFuncs->j9jni_createLocalRef(env, fieldObj);
		if (NULL == result) {
			goto nativeoutofmemory;
		}

		goto done;
	}
	/* fall through to nosuchfield - no match found */
nosuchfield:
	vmFuncs->setCurrentException(vmThread, J9VMCONSTANTPOOL_JAVALANGNOSUCHFIELDEXCEPTION, (UDATA *)*(j9object_t *)fieldName);
	goto done;

nullpointer:
	vmFuncs->setCurrentException(vmThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	goto done;

nativeoutofmemory:
	vmFuncs->setNativeOutOfMemoryError(vmThread, 0, 0);
	goto done;

heapoutofmemory:
	vmFuncs->setHeapOutOfMemoryError(vmThread);
	goto done;

done:
	vmFuncs->internalExitVMToJNI(vmThread);
	return result;
}

static J9WalkFieldAction
countFieldIterator(J9ROMFieldShape *romField, J9Class *declaringClass, void *userData)
{
	if (romField->modifiers & J9AccPublic) {
		((CountFieldData *)userData)->fieldCount += 1;
	}
	return J9WalkFieldActionContinue;
}

static J9WalkFieldAction
allFieldIterator(J9ROMFieldShape *romField, J9Class *declaringClass, void *userData)
{
	if (romField->modifiers & J9AccPublic) {
		AllFieldData *data = userData;
		UDATA inconsistentData = 0;
		J9Class *lookupClass = data->lookupClass;
		J9VMThread *vmThread = data->currentThread;
		j9object_t fieldObject = NULL;
		j9array_t fieldArrayObject = NULL;

		if (romField->modifiers & J9AccStatic) {
			/* create static field object */
			fieldObject = createStaticFieldObject(romField, declaringClass, lookupClass, vmThread, &inconsistentData);
		} else {
			/* create instance field object */
			fieldObject = createInstanceFieldObject(romField, declaringClass, lookupClass, vmThread, &inconsistentData);
		}

		if (NULL != vmThread->currentException) {
			return J9WalkFieldActionStop;
		}

		if (0 != inconsistentData) {
			/* Class redefinition left this in an inconsistent state.
			 * Need to restart the entire operation.
			 */
			data->restartRequired = 1;
			return J9WalkFieldActionStop;
		}

		if (NULL == fieldObject) {
			vmThread->javaVM->internalVMFunctions->setHeapOutOfMemoryError(vmThread);
			return J9WalkFieldActionStop;
		}

		/* refetch array object because vmaccess could have been released */
		fieldArrayObject = (j9array_t)J9_JNI_UNWRAP_REFERENCE(data->fieldArray);

		/* Storing newly allocated objects into newly allocated array. Both in same space, so no exception can occur. */
		J9JAVAARRAYOFOBJECT_STORE(vmThread, fieldArrayObject, data->fieldIndex, fieldObject);

		data->fieldIndex += 1;
	}

	return J9WalkFieldActionContinue;
}

jarray
getFieldsHelper(JNIEnv *env, jobject cls)
{
	J9VMThread *vmThread = (J9VMThread *)env;
	J9JavaVM *vm = vmThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	jarray result = NULL;
	J9Class *clazz = NULL;
	J9ROMClass *romClass = NULL;
	U_32 fieldCount = 0;
	j9array_t fieldArrayObject = NULL;
	J9Class *fieldArrayClass = NULL;
	J9Class *fieldClass = NULL;

	vmFuncs->internalEnterVMFromJNI(vmThread);

retry:
	clazz = J9VM_J9CLASS_FROM_JCLASS(vmThread, (jclass)cls);
	romClass = clazz->romClass;

	/* primitives/arrays don't have fields */
	if (FALSE == J9ROMCLASS_IS_PRIMITIVE_OR_ARRAY(romClass)) {
		J9WalkFieldHierarchyState state;
		CountFieldData countData;

		countData.fieldCount = 0;
		state.fieldCallback = countFieldIterator;
		state.userData = &countData;
		walkFieldHierarchyDo(clazz, &state);
		fieldCount = countData.fieldCount;
	}

	/* allocate an array of Field objects */
	fieldClass = J9VMJAVALANGREFLECTFIELD(vm);
	if (NULL != vmThread->currentException) {
		goto done;
	}

	fieldArrayClass = fetchArrayClass(vmThread, fieldClass);
	if (NULL != vmThread->currentException) {
		goto done;
	}

	fieldArrayObject = (j9array_t)vm->memoryManagerFunctions->J9AllocateIndexableObject(vmThread, fieldArrayClass,
			fieldCount, J9_GC_ALLOCATE_OBJECT_NON_INSTRUMENTABLE);
	if (NULL == fieldArrayObject) {
		goto heapoutofmemory;
	}

	result = vmFuncs->j9jni_createLocalRef(env, (j9object_t)fieldArrayObject);
	if (NULL == result) {
		goto nativeoutofmemory;
	}

	if (fieldCount > 0) {
		J9WalkFieldHierarchyState state;
		AllFieldData data;

		data.lookupClass = clazz;
		data.currentThread = vmThread;
		data.fieldArray = result;
		data.fieldIndex = 0;
		data.restartRequired = 0;
		state.fieldCallback = allFieldIterator;
		state.userData = &data;
		walkFieldHierarchyDo(clazz, &state);

		if (0 != data.restartRequired) {
			/* Class redefinition resulted in an inconsistent state.
			 * Restart the operation
			 */
			if (NULL != result) {
				vmFuncs->j9jni_deleteLocalRef(env, result);
				result = NULL;
			}
			goto retry;
		}
	}
	goto done;

nativeoutofmemory:
	vmFuncs->setNativeOutOfMemoryError(vmThread, 0, 0);
	goto done;

heapoutofmemory:
	vmFuncs->setHeapOutOfMemoryError(vmThread);
	goto done;

done:
	vmFuncs->internalExitVMToJNI(vmThread);
	return result;
}
