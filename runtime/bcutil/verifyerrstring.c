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

#include "j9.h"
#include "j9port.h"
#include "j9protos.h"

/* 
 * Construct a string describing the specified J9CfrError.
 * The caller is responsible for freeing the returned string.
 * Returns NULL if unable to allocate memory for the string.
 */
const char * 
buildVerifyErrorString( J9JavaVM *javaVM, J9CfrError *error, U_8* className, UDATA classNameLength)
{
	PORT_ACCESS_FROM_JAVAVM( javaVM );
	const char* errorString;

	if(error->errorMethod == -1) {
		errorString = getJ9CfrErrorDetailMessageNoMethod(
			PORTLIB,
			error, 
			className,
			classNameLength);
	} else {
		J9CfrConstantPoolInfo *name, *sig;
		/* Jazz 82615: Statistics data indicates the buffer size of 97% JCK cases is less than 1K */
		U_8 byteArray[1024];
		U_8* detailedErrMsg = NULL;
		UDATA detailedErrMsgLength = 0;

		/* Jazz 82615: Generate the error message framework by default. 
		 * The error message framework is not required when the -XX:-VerifyErrorDetails option is specified.
		 */
		if (J9_ARE_ALL_BITS_SET(javaVM->bytecodeVerificationData->verificationFlags, J9_VERIFY_ERROR_DETAILS)) {
			/* Jazz 82615: The value of detailedErrMsg may change if the initial byteArray is insufficient to contain the error message framework.
			 * Under such circumstances, it points to the address of newly allocated memory.
			 */
			detailedErrMsg = byteArray;
			detailedErrMsgLength = sizeof(byteArray);
			/* In the case of failure to allocating memory in building the error message framework,
			 * detailedErrMsg is set to NULL and detailedErrMsgLength is set to 0 on return from getCfrExceptionDetail().
			 * Therefore, it ends up with a simple error message as the buffer for the error message framework has been cleaned up.
			 */
			detailedErrMsg = javaVM->verboseStruct->getCfrExceptionDetails(javaVM, error, className, classNameLength, detailedErrMsg, &detailedErrMsgLength);
		}

		name = &(error->constantPool)[error->errorMember->nameIndex];
		sig = &(error->constantPool)[error->errorMember->descriptorIndex];

		/* Jazz 82615: Append the error message framework to the existing error string */
		errorString = getJ9CfrErrorDetailMessageForMethod(
			PORTLIB, 
			error, 
			className, 
			classNameLength, 
			name->bytes, 
			name->slot1,
			sig->bytes,
			sig->slot1,
			detailedErrMsg,
			detailedErrMsgLength
		);

		/* Jazz 82615: Release the memory pointed by detailedErrMsg if allocated with new memory in getCfrExceptionDetails */
		if (detailedErrMsg != byteArray) {
			j9mem_free_memory(detailedErrMsg);
		}
	}

	return errorString;
}

