/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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

#include "j9cel4ro64.h"
#include <assert.h> /* Required for __gtca() */

/* Function descriptor of CEL4RO64 runtime call from GTCA control block */
typedef void cel4ro64_cwi_func(void*);
#pragma linkage (cel4ro64_cwi_func,OS_UPSTACK)
#define CEL4RO64_FNPTR ((cel4ro64_cwi_func *)(*(int*)((char*)(*(int*)(((char*)__gtca())+1024))+8)))

#pragma linkage (J9VM_STE,OS)
float J9VM_STE(void);
#pragma linkage (J9VM_STD,OS)
double J9VM_STD(void);
#pragma linkage (J9VM_LE,OS)
void J9VM_LE(float);
#pragma linkage (J9VM_LD,OS)
void J9VM_LD(double);

/**
 * Populate the arguments from argValues in outgoing 64-bit representation based on the types provided.
 * This routine will zero extend upper 32-bits as necessary.
 *
 * @param[in] outgoingArgs The outgoing argument storage area with first 4-bytes as length.
 * @param[in] argTypes An array of argument types for call.
 * @param[in] argsValues An uint64_t array of argument values.
 * @param[in] numArgs Number of arguments.
 */
static void
populateArguments(uint64_t *outgoingArgs, J9_CEL4RO64_ArgType *argTypes, uint64_t *argValues, uint32_t numArgs)
{
	/* First field of outgoingArgs is length member - each argument on 64-bit side is
	 * in an 8-byte slot. Bump outgoingArgs by 4-bytes afterwards.
	 */
	uint32_t *lengthPtr = (uint32_t*)outgoingArgs;
	*lengthPtr = sizeof(uint64_t) * numArgs;
	outgoingArgs = (uint64_t *)&(lengthPtr[1]);

	for (uint32_t i = 0; i < numArgs; i++) {
		J9_CEL4RO64_ArgType argType = argTypes[i];
		jfloat floatArg;
		jdouble doubleArg;

		switch(argType) {
		case CEL4RO64_type_jboolean:
		case CEL4RO64_type_jbyte:
		case CEL4RO64_type_jchar:
		case CEL4RO64_type_jshort:
		case CEL4RO64_type_jint:
		case CEL4RO64_type_jsize:
		case CEL4RO64_type_uint32_ptr:
			outgoingArgs[i] = argValues[i] & 0xFFFFFFFF;
			break;
		case CEL4RO64_type_jfloat:
			/* Note: Float argument requires special handling as the value needs to be loaded into FPR0.
			 * Only set*FloatField JNI functions have a jfloat parameter, so only need to handle
			 * one instance.
			 */
			floatArg = *(jfloat *)(&(argValues[i]));
			/* Ideally, we could have used the following inline asm
			 * but V2R1 xlc does not support that yet.
			 * __asm(" LE 0,%0" : : "m"(floatArg));
			 */
			J9VM_LE(floatArg);
			break;
		case CEL4RO64_type_jdouble:
			/* Note: Double argument requires special handling as the value needs to be loaded into FPR0.
			 * Only set*DoubleField JNI functions have a jdouble parameter, so only need to handle
			 * one instance.
			 */
			doubleArg = *(jdouble *)(&(argValues[i]));
			/* Ideally, we could have used the following inline asm
			 * but V2R1 xlc does not support that yet.
			 * __asm(" LD 0,%0" : : "m"(doubleArg));
			 */
			J9VM_LD(doubleArg);
			break;
		case CEL4RO64_type_jlong:
		case CEL4RO64_type_jobject:
		case CEL4RO64_type_jweak:
		case CEL4RO64_type_jclass:
		case CEL4RO64_type_jstring:
		case CEL4RO64_type_jthrowable:
		case CEL4RO64_type_jarray:
		case CEL4RO64_type_jmethodID:
		case CEL4RO64_type_jfieldID:
		case CEL4RO64_type_JNIEnv64:
		case CEL4RO64_type_JavaVM64:
			outgoingArgs[i] = argValues[i];
			break;
		default:
			break;
		}
	}
}

/**
 * Populate the return storage buffer with the appropriate return value from CEL4RO64 call.
 *
 * @param[in] controlBlock The outgoing argument storage area.
 * @param[in] returnType The return type of target function.
 * @param[in] returnStorage The storage location to store return value.
 */
static void
populateReturnValue(J9_CEL4RO64_controlBlock *controlBlock, J9_CEL4RO64_ArgType returnType, void *returnStorage)
{
	/* Integral and pointer data types <= 64 bits in length are widened to 64 bits and returned in GPR3. */
	uint64_t GPR3returnValue = controlBlock->gpr3ReturnValue;

	/* Cannot initialize these floating point variables, as it might kill FPR0 which contains the real
	 * return value.
	 */
	jfloat floatReturnValue;
	jdouble doubleReturnValue;

	switch(returnType) {
	case CEL4RO64_type_jboolean:
		*((jboolean *)returnStorage) = (jboolean)GPR3returnValue;
		break;
	case CEL4RO64_type_jbyte:
		*((jbyte *)returnStorage) = (jbyte)GPR3returnValue;
		break;
	case CEL4RO64_type_jchar:
		*((jchar *)returnStorage) = (jchar)GPR3returnValue;
		break;
	case CEL4RO64_type_jshort:
		*((jshort *)returnStorage) = (jshort)GPR3returnValue;
		break;
	case CEL4RO64_type_jint:
	case CEL4RO64_type_jsize:
		*((jint *)returnStorage) = (jint)GPR3returnValue;
		break;
	case CEL4RO64_type_jfloat:
		/* Float return values are returned via FPR0.
		 * Ideally, we could have used the following inline asm
		 * but V2R1 xlc does not support that yet.
		 * __asm(" STE 0,%0" : "=m"(floatReturnValue));
		 */
		floatReturnValue = J9VM_STE();
		*((jfloat *)returnStorage) = floatReturnValue;
		break;
	case CEL4RO64_type_jdouble:
		/* Double return values are returned via FPR0.
		 * Ideally, we could have used the following inline asm
		 * but V2R1 xlc does not support that yet.
		 * __asm(" STD 0,%0" : "=m"(doubleReturnValue));
		 */
		doubleReturnValue = J9VM_STD();
		*((jdouble *)returnStorage) = doubleReturnValue;
		break;
	case CEL4RO64_type_jlong:
	case CEL4RO64_type_jobject:
	case CEL4RO64_type_jweak:
	case CEL4RO64_type_jclass:
	case CEL4RO64_type_jstring:
	case CEL4RO64_type_jthrowable:
	case CEL4RO64_type_jarray:
	case CEL4RO64_type_jmethodID:
	case CEL4RO64_type_jfieldID:
	case CEL4RO64_type_JNIEnv64:
	case CEL4RO64_type_JavaVM64:
		/* These are all 64-bit types. */
		*((uint64_t*)returnStorage) = GPR3returnValue;
		break;
	case CEL4RO64_type_uint32_ptr:
		*((uint32_t *)returnStorage) = (uint32_t)GPR3returnValue;
		break;
	default:
		break;
	}
}

uint32_t
j9_cel4ro64_call_function(
	uint64_t functionDescriptor, J9_CEL4RO64_ArgType *argTypes, uint64_t *argValues, uint32_t numArgs,
	J9_CEL4RO64_ArgType returnType, void *returnStorage)
{
	uint32_t retcode = J9_CEL4RO64_RETCODE_OK;
	uint32_t argumentAreaInBytes = (0 == numArgs) ? 0 : (sizeof(uint64_t) * numArgs + sizeof(uint32_t));  /* 8 byte slots + 4 byte length member. */
	J9_CEL4RO64_controlBlock *controlBlock = NULL;

	/* The CEL4RO64 function may not be available if the LE ++APAR is not installed. */
	if (!j9_cel4ro64_isSupported()) {
		fprintf(stderr, "CEL4RO64: %s\n", j9_cel4ro64_get_error_message(J9_CEL4RO64_RETCODE_ERROR_LE_RUNTIME_SUPPORT_NOT_FOUND));
		return J9_CEL4RO64_RETCODE_ERROR_LE_RUNTIME_SUPPORT_NOT_FOUND;
	}

	controlBlock = (J9_CEL4RO64_controlBlock *)malloc(sizeof(J9_CEL4RO64_controlBlock) + argumentAreaInBytes);

	if (NULL == controlBlock) {
		retcode = J9_CEL4RO64_RETCODE_ERROR_STORAGE_ISSUE;
		fprintf(stderr, "CEL4RO64: malloc failed: %s\n", j9_cel4ro64_get_error_message(retcode));
		return retcode;
	}

	/* Initialize the control block members and calculate the various offsets. */
	controlBlock->version = 1;
	controlBlock->flags = J9_CEL4RO64_FLAG_EXECUTE_TARGET_FUNC;
	controlBlock->moduleOffset = sizeof(J9_CEL4RO64_controlBlock);
	controlBlock->functionOffset = sizeof(J9_CEL4RO64_controlBlock);
	controlBlock->dllHandle = 0;
	controlBlock->functionDescriptor = functionDescriptor;

	/* If no arguments, the argumentOffset must be zero. */
	if (0 == numArgs) {
		controlBlock->argumentsOffset = 0;
	} else {
		uint64_t *argumentsArea = (uint64_t *)((char *)(controlBlock) + sizeof(J9_CEL4RO64_controlBlock));
		controlBlock->argumentsOffset = sizeof(J9_CEL4RO64_controlBlock);
		populateArguments(argumentsArea, argTypes, argValues, numArgs);
	}

	CEL4RO64_FNPTR(controlBlock);
	retcode = controlBlock->retcode;
	if ((J9_CEL4RO64_RETCODE_OK == retcode) && (CEL4RO64_type_void != returnType)) {
		populateReturnValue(controlBlock, returnType, returnStorage);
	}

	free(controlBlock);
	return retcode;
}

uint32_t
j9_cel4ro64_load_query_call_function(
	const char* moduleName, const char* functionName, J9_CEL4RO64_ArgType *argTypes,
	uint64_t *argValues, uint32_t numArgs, J9_CEL4RO64_ArgType returnType, void *returnStorage)
{
	uint32_t retcode = J9_CEL4RO64_RETCODE_OK;
	uint32_t IPBLength = 0;
	uint32_t moduleNameLength = strlen(moduleName);
	uint32_t functionNameLength = strlen(functionName);
	uint32_t argsLength = sizeof(uint64_t) * numArgs + sizeof(uint32_t); /* 8 byte slots + 4 byte length member. */
	J9_CEL4RO64_infoBlock *infoBlock = NULL;
	J9_CEL4RO64_controlBlock *controlBlock = NULL;
	J9_CEL4RO64_module *moduleBlock = NULL;
	J9_CEL4RO64_function *functionBlock = NULL;

	/* The CEL4RO64 function may not be available if the LE ++APAR is not installed. */
	if (!j9_cel4ro64_isSupported()) {
		fprintf(stderr, "CEL4RO64: %s\n", j9_cel4ro64_get_error_message(J9_CEL4RO64_RETCODE_ERROR_LE_RUNTIME_SUPPORT_NOT_FOUND));
		return J9_CEL4RO64_RETCODE_ERROR_LE_RUNTIME_SUPPORT_NOT_FOUND;
	}

	IPBLength = sizeof(J9_CEL4RO64_infoBlock) + moduleNameLength + functionNameLength + argsLength +
	            sizeof(moduleBlock->length) + sizeof(functionBlock->length);
	infoBlock = (J9_CEL4RO64_infoBlock *) malloc(IPBLength);
	if (NULL == infoBlock) {
		retcode = J9_CEL4RO64_RETCODE_ERROR_STORAGE_ISSUE;
		fprintf(stderr, "CEL4RO64: malloc failed: %s\n", j9_cel4ro64_get_error_message(retcode));
		return retcode;
	}

	controlBlock = &(infoBlock->ro64ControlBlock);

	/* Initialize the control block members and calculate the various offsets. */
	controlBlock->version = 1;
	controlBlock->flags = J9_CEL4RO64_FLAG_ALL_LOAD_QUERY_EXECUTE;
	controlBlock->moduleOffset = sizeof(J9_CEL4RO64_controlBlock);
	if (0 == moduleNameLength) {
		controlBlock->functionOffset = controlBlock->moduleOffset;
	} else {
		controlBlock->functionOffset = controlBlock->moduleOffset + moduleNameLength + sizeof(moduleBlock->length);
	}

	/* For execute target function operations, we need to ensure args reference is set
	 * up appropriately. If no arguments, argumentsOffset needs to be 0.
	 */
	if (0 == argsLength) {
		controlBlock->argumentsOffset = 0;
	} else if (0 == functionNameLength) {
		controlBlock->argumentsOffset = controlBlock->functionOffset;
	} else {
		controlBlock->argumentsOffset = controlBlock->functionOffset + functionNameLength + sizeof(functionBlock->length);
	}

	/* For DLL load operations, we need a module name specified. */
	moduleBlock = (J9_CEL4RO64_module *)((char *)(controlBlock) + controlBlock->moduleOffset);
	infoBlock->ro64Module = moduleBlock;
	moduleBlock->length = moduleNameLength;
	strncpy(moduleBlock->moduleName, moduleName, moduleNameLength);

	/* For DLL query operations, we need a function name specified. */
	functionBlock = (J9_CEL4RO64_function *)((char *)(controlBlock) + controlBlock->functionOffset);
	infoBlock->ro64Function = functionBlock;
	functionBlock->length = functionNameLength;
	strncpy(functionBlock->functionName, functionName, functionNameLength);

	/* For execute target function operations, we need to ensure args is set
	 * up appropriately. If no arguments, argumentsOffset needs to be 0.
	 */
	if (0 == numArgs) {
		controlBlock->argumentsOffset = 0;
	} else {
		uint64_t *argumentsArea = (uint64_t *)((char *)(controlBlock) + controlBlock->argumentsOffset);
		populateArguments(argumentsArea, argTypes, argValues, numArgs);
	}

	CEL4RO64_FNPTR(controlBlock);
	retcode = controlBlock->retcode;
	if ((J9_CEL4RO64_RETCODE_OK == retcode) && (CEL4RO64_type_void != returnType)) {
		populateReturnValue(controlBlock, returnType, returnStorage);
	}

	free(infoBlock);
	return retcode;
}

jboolean
j9_cel4ro64_isSupported(void)
{
	return (NULL != CEL4RO64_FNPTR);
}

const char*
j9_cel4ro64_get_error_message(int retCode)
{
	const char* errorMessage;
	switch(retCode) {
	case J9_CEL4RO64_RETCODE_OK:
		errorMessage = "No errors detected.";
		break;
	case J9_CEL4RO64_RETCODE_ERROR_MULTITHREAD_INVOCATION:
		errorMessage = "Multi-threaded invocation not supported.";
		break;
	case J9_CEL4RO64_RETCODE_ERROR_STORAGE_ISSUE:
		errorMessage = "Storage issue detected.";
		break;
	case J9_CEL4RO64_RETCODE_ERROR_FAILED_AMODE64_ENV:
		errorMessage = "Failed to initialize AMODE64 environment.";
		break;
	case J9_CEL4RO64_RETCODE_ERROR_FAILED_LOAD_TARGET_DLL:
		errorMessage = "DLL module not found.";
		break;
	case J9_CEL4RO64_RETCODE_ERROR_FAILED_QUERY_TARGET_FUNC:
		errorMessage = "Target function not found.";
		break;
	case J9_CEL4RO64_RETCODE_ERROR_LE_RUNTIME_SUPPORT_NOT_FOUND:
		errorMessage = "CEL4RO64 runtime support not found.";
		break;
	default:
		errorMessage = "Unexpected return code.";
		break;
	}
	return errorMessage;
}
