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

#ifndef j9cel4ro64_h
#define j9cel4ro64_h

#include <stdint.h>
#include <string.h>
#include "jni.h" /* Needed for return argument type handling */

#ifdef __cplusplus
extern "C" {
#endif

/* Flags to control the behaviour of CEL4RO64. Used by controlBlock->flags */
#define J9_CEL4RO64_FLAG_LOAD_DLL               0x80000000
#define J9_CEL4RO64_FLAG_QUERY_TARGET_FUNC      0x40000000
#define J9_CEL4RO64_FLAG_EXECUTE_TARGET_FUNC    0x20000000
#define J9_CEL4RO64_FLAG_ALL_LOAD_QUERY_EXECUTE (J9_CEL4RO64_FLAG_LOAD_DLL | J9_CEL4RO64_FLAG_QUERY_TARGET_FUNC | J9_CEL4RO64_FLAG_EXECUTE_TARGET_FUNC)

/* Return codes for CEL4RO64 */
#define J9_CEL4RO64_RETCODE_OK                                   0x0
#define J9_CEL4RO64_RETCODE_ERROR_MULTITHREAD_INVOCATION         0x1
#define J9_CEL4RO64_RETCODE_ERROR_STORAGE_ISSUE                  0x2
#define J9_CEL4RO64_RETCODE_ERROR_FAILED_AMODE64_ENV             0x3
#define J9_CEL4RO64_RETCODE_ERROR_FAILED_LOAD_TARGET_DLL         0x4
#define J9_CEL4RO64_RETCODE_ERROR_FAILED_QUERY_TARGET_FUNC       0x5
#define J9_CEL4RO64_RETCODE_ERROR_LE_RUNTIME_SUPPORT_NOT_FOUND   0x6

/* Fixed length Control Block structure RO64_CB */
typedef struct J9_CEL4RO64_controlBlock {
	uint32_t version;            /**< (Input) A integer which contains the version of RO64_INFO. */
	uint32_t length;             /**< (Input) A integer which contains the total length of RO64_INFO. */
	uint32_t flags;              /**< (Input) Flags to control the behavior of CEL4RO64. */
	uint32_t moduleOffset;       /**< (Input) Offset to RO64_module part from start of RO64_CB. Req'd for DLL load flag. */
	uint32_t functionOffset;     /**< (Input) Offset to RO64_function part from start of RO64_CB. Req'd for DLL query flag. */
	uint32_t argumentsOffset;    /**< (Input) Offset to RO64_args part from start of RO64_CB. Req'd for function execution flag. */
	uint64_t dllHandle;          /**< DLL handle of target program (Input) if DLL query flag. (Output) if DLL load flag. */
	uint64_t functionDescriptor; /**< Function descriptor of target function (Input) if function exection flag. (Output) if DLL query flag. */
	uint64_t gpr1ReturnValue;    /**< (Output) Return buffer containing 64-bit GPR1 value of target program after execution. */
	uint64_t gpr2ReturnValue;    /**< (Output) Return buffer containing 64-bit GPR2 value of target program after execution. */
	uint64_t gpr3ReturnValue;    /**< (Output) Return buffer containing 64-bit GPR3 value of target program after execution. */
	int32_t retcode;             /**< (Output) Return code of CEL4RO64. */
} J9_CEL4RO64_controlBlock;

/* Module name to load */
typedef struct J9_CEL4RO64_module {
	int32_t length;         /**< Length of the module name. */
	char moduleName[1];     /**< Pointer to variable length module name. */
} J9_CEL4RO64_module;

/* Internal struct to map function name definition in control block. */
typedef struct J9_CEL4RO64_function {
	int32_t length;          /**< Length of the function name. */
	char functionName[1];    /**< Pointer to variable length function name. */
} J9_CEL4RO64_function;

/* Internal struct to track the RO64 control block and related variable length structures. */
typedef struct J9_CEL4RO64_infoBlock {
	J9_CEL4RO64_module *ro64Module;             /**< Pointer to the start of the module section of the control block. */
	J9_CEL4RO64_function *ro64Function;         /**< Pointer to the start of the function section of the control block. */
	uint32_t *ro64Arguments;                    /**< Pointer to the start of the outgoing arguments section of the control block. */
	J9_CEL4RO64_controlBlock ro64ControlBlock;  /**< Pointer to the control block for CEL4RO64. */
} J9_CEL4RO64_infoBlock;

/**
 * Types to facilitate argument construction and return values for calls to 64-bit.
 */
typedef enum J9_CEL4RO64_ArgType {
	CEL4RO64_type_void,
	CEL4RO64_type_jboolean,
	CEL4RO64_type_jbyte,
	CEL4RO64_type_jchar,
	CEL4RO64_type_jshort,
	CEL4RO64_type_jint,
	CEL4RO64_type_jlong,
	CEL4RO64_type_jfloat,
	CEL4RO64_type_jdouble,
	CEL4RO64_type_jsize,
	CEL4RO64_type_jobject,
	CEL4RO64_type_jweak,
	CEL4RO64_type_jclass,
	CEL4RO64_type_jstring,
	CEL4RO64_type_jthrowable,
	CEL4RO64_type_jarray,
	CEL4RO64_type_uint32_ptr,
	CEL4RO64_type_jmethodID,
	CEL4RO64_type_jfieldID,
	CEL4RO64_type_JNIEnv64,
	CEL4RO64_type_JavaVM64,
	CEL4RO64_type_EnsureWideEnum = 0x1000000 /* force 4-byte enum */
} J9_CEL4RO64_ArgType;

/**
 * A helper routine to invoke a target function with outgoing arguments and return values.
 * @param[in] functionDescriptor The 64-bit function descriptor of target function to invoke.
 * @param[in] argTypes An array of argument types for call.
 * @param[in] argsValues An uint64_t array of argument values.
 * @param[in] numArgs Number of arguments.
 * @param[in] returnType The return type of target function.
 * @param[in] returnStorage The storage location to store return value.
 *
 * @return The return code of the CEL4RO64 call - 0 implies success.
 */
uint32_t
j9_cel4ro64_call_function(
	uint64_t functionDescriptor, J9_CEL4RO64_ArgType *argTypes, uint64_t *argValues, uint32_t numArgs,
	J9_CEL4RO64_ArgType returnType, void *returnStorage);

/**
 * A helper routine to allocate the CEL4RO64 control blocks and related structures
 * for module, function and arguments.
 * @param[in] moduleName The name of the target library to load.
 * @param[in] functionName The name of the target function to lookup.
 * @param[in] argTypes An array of argument types for call.
 * @param[in] argsValues An uint64_t array of argument values.
 * @param[in] numArgs Number of arguments.
 * @param[in] returnType The return type of target function.
 * @param[in] returnStorage The storage location to store return value.
 *
 * @return The return code of the CEL4RO64 call - 0 implies success.
 */
uint32_t
j9_cel4ro64_load_query_call_function(
	const char* moduleName, const char* functionName, J9_CEL4RO64_ArgType *argTypes,
	uint64_t *argValues, uint32_t numArgs, J9_CEL4RO64_ArgType returnType, void *returnStorage);

/**
 * A helper routine to confirm LE support of CEL4RO64
 * @return whether CEL4RO64 runtime support is available.
 */
jboolean
j9_cel4ro64_isSupported(void);

/**
 * A helper routine to return an error message associated with the CEL4RO64 return code.
 * @param[in] retCode A CEL4RO64 return code.
 *
 * @return A message describing the conditions of the given error.
 */
const char *
j9_cel4ro64_get_error_message(int32_t retCode);

#ifdef __cplusplus
}
#endif

#endif /* j9cel4ro64_h */
