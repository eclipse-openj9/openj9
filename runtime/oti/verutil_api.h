/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

#ifndef verutil_api_h
#define verutil_api_h

/**
* @file verutil_api.h
* @brief Public API for the VERUTIL module.
*
* This file contains public function prototypes and
* type definitions for the VERUTIL module.
*
*/

#include "j9.h"
#include "j9comp.h"
#include "cfr.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------- sigverify.c ---------------- */

/**
* Checks that the string is a well formed signature
* @param signatureBytes Signature string
* @param signatureLength length of the string
* @param currentIndex starting position of the check
* @return negative on failure, non-negative on success
*/
IDATA verifyFieldSignatureUtf8(U_8* signatureBytes, UDATA signatureLength, UDATA currentIndex);

/**
 * Check if a string is a well-formed identifier per JVMS java 8 version, section 4.2.
 * String is well-formed if it comprises one or more Unicode characters, with the following exceptions:
 * '/', ';' , '[' and '.' (semicolon, open square bracket, and period) are not allowed
 * 	@param identifierStart pointer to first character of the signature
 * 	@param identifierLength Length of the signature in bytes
 *	@return TRUE if the string is well formed
 */

BOOLEAN verifyIdentifierUtf8(U_8* identifierStart, UDATA identifierLength);

/**
 * Check if a string is a well-formed fully qualified class or interface name per JVMS java 8 version, section 4.2.
 * String is well-formed if it comprises one or more Unicode characters, with the following exceptions:
 * ';' , '[' and '.' (semicolon, open square bracket, and period) are not allowed
 * '/' is allowed but may not be contiguous with another '/'
 * 	@param identifierStart pointer to first character of the signature
 * 	@param identifierLength Length of the signature in bytes
 *	@return TRUE if the string is well formed
 */

BOOLEAN verifyClassnameUtf8(U_8* identifierStart, UDATA identifierLength);

/**
* Verify that a string is a well-formed method signature
* @param signatureBytes character string
* @param signatureLength length of the string
* @return argument count on success, -2 on arity failure, -1 on other failure.
*/
IDATA verifyMethodSignatureUtf8(U_8* signatureBytes, UDATA signatureLength);

/**
* Verify that a string is a well-formed signature
* @param signatureBytes signature
* @param signatureLength length of the string
* @param currentIndex starting point fpr the check
* @return negative on failure, non-negative on success
*/
IDATA verifySignatureUtf8(U_8* signatureBytes, UDATA signatureLength);

/**
 * 	Fetch the argument of signature to obtain the slot count of argument plus its data type if requested.
 * 	@param signatureBytes pointer to first character of the signature
 * 	@param signatureLength Length of the signature in bytes
 * 	@param currentIndex starting point of the signature string, updated to the end of the scan
 *  @param argumentType - data type of the argument
 *	@return the slot count of argument in bytes on success (double/long type takes 2 slots, the others take 1 slot)
 *	        -2 if arity > 255
 *	        -1 on other failure
 */
IDATA fetchArgumentOfSignature (U_8* signatureBytes, UDATA signatureLength, UDATA *currentIndex, U_8* argumentType);


/* ---------------- cfrerr.c ---------------- */

/**
 * Fetch an error message for the specified error.
 *
 * This function will never return NULL.
 * The pointer returned by this function does not need to be freed.
 *
 * @param[in] portLib - the portLibrary
 * @param[in] error - the class loading error
 * @return const char* - a localized description of the error
 */
const char*
getJ9CfrErrorDescription(J9PortLibrary* portLib, J9CfrError* error);

/**
 * Generate a detail message for the given J9CfrError. The error is not associated with a method.
 *
 * The pointer returned by this function must be freed.
 *
 * @param[in] portLib - the portLibrary
 * @param[in] error - the class loading error
 * @param[in] className - the name of the class where the error occurred
 * @param[in] classNameLength - the number of bytes in className
 * @return const char* - a localized description of the error, or NULL if out of memory
 */
const char*
getJ9CfrErrorDetailMessageNoMethod(J9PortLibrary* portLib, J9CfrError* error, const U_8* className, UDATA classNameLength);

/**
 * Generate a detail message for the given J9CfrError. The error is associated with a particular method.
 *
 * The pointer returned by this function must be freed.
 *
 * @param[in] portLib - the portLibrary
 * @param[in] error - the class loading error
 * @param[in] className - the name of the class where the error occurred
 * @param[in] classNameLength - the number of bytes in className
 * @param[in] methodName - the name of the method where the error occurred
 * @param[in] methodNameLength - the number of bytes in methodName
 * @param[in] methodSignature - the signature of the method where the error occurred
 * @param[in] methodSignatureLength - the number of bytes in methodSignature
 * @param[in] detailedException - pointer to the detailed exception data
 * @param[in] detailedExceptionLength - the length of detailed exception data
 * @return const char* - a localized description of the error, or NULL if out of memory
 */
const char*
getJ9CfrErrorDetailMessageForMethod(J9PortLibrary* portLib, J9CfrError* error, const U_8* className, UDATA classNameLength, const U_8* methodName, UDATA methodNameLength, const U_8* methodSignature, UDATA methodSignatureLength, const U_8* detailedException, UDATA detailedExceptionLength);

void
buildError(J9CfrError * errorStruct, UDATA code, UDATA action, UDATA offset);

/**
 * Set up bootstrap method errors if the verification error occurs.
 * @param[in] errorStruct - pointer to J9CfrError
 * @param[in] code - the error code
 * @param[in] action - the errorAction
 * @param[in] offset - the errorOffset
 * @param[in] bsmIndex - the index of the bootstrap method array
 * @param[in] bsmArgsIndex - the constant pool index stored in the bootstrap method arguments.
 * @param[in] cpType - the constant pool type value
 */
void
buildBootstrapMethodError(J9CfrError * errorStruct, UDATA code, UDATA action, UDATA offset, I_32 bsmIndex, U_32 bsmArgsIndex, U_32 cpType);

/**
 * Set up method errors if the verification error occurs.
 * @param[in] errorStruct - pointer to J9CfrError
 * @param[in] code - the error code
 * @param[in] action - the errorAction
 * @param[in] methodIndex - index to the specified method in the constant pool
 * @param[in] pc - the pc value
 * @param[in] method - pointer to J9CfrMethod
 * @param[in] constantPoolPointer - pointer to the constant pool
 */
void
buildMethodError(J9CfrError * errorStruct, UDATA code, UDATA action, I_32 methodIndex, U_32 pc, J9CfrMethod* method, J9CfrConstantPoolInfo* constantPoolPointer);

/**
 * Set up method errors for the error message framework if verification error occurs.
 * @param[in] errorStruct - pointer to J9CfrError
 * @param[in] code - the error code
 * @param[in] verboseErrorType - the verbose error type used in the error message framework
 * @param[in] action - the errorAction
 * @param[in] methodIndex - index to the specified method in the constant pool
 * @param[in] pc - the pc value
 * @param[in] method - pointer to J9CfrMethod
 * @param[in] constantPoolPointer - pointer to the constant pool
 * @param[in] errorDataIndex - the index value when verification error occurs (e.g. constant pool index, local variable index, etc)
 * @param[in] stackmapFrameIndex - index to stack frame when error occurs
 * @param[in] stackmapFrameBCI - the bci value of the stackmap frame when error occurs
 */
void
buildMethodErrorWithExceptionDetails(J9CfrError * errorStruct, UDATA code, I_32 verboseErrorType, UDATA action, I_32 methodIndex, U_32 pc, J9CfrMethod* method, J9CfrConstantPoolInfo* constantPoolPointer, U_32 errorDataIndex, I_32 stackmapFrameIndex, U_32 stackmapFrameBCI);

#ifdef __cplusplus
}
#endif
#endif
