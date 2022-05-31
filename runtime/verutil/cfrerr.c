/*******************************************************************************
 * Copyright (c) 1991, 2022 IBM Corp. and others
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
#include "cfrerrnls.h"
#include "j9bcvnls.h"
#include "cfreader.h"

#define MAX_INT_SIZE 10 /* max length of printed int */

const char*
getJ9CfrErrorDescription(J9PortLibrary* portLib, J9CfrError* error)
{
	PORT_ACCESS_FROM_PORT(portLib);
	return OMRPORT_FROM_J9PORT(PORTLIB)->nls_lookup_message(OMRPORT_FROM_J9PORT(PORTLIB), J9NLS_DO_NOT_APPEND_NEWLINE, error->errorCatalog, error->errorCode, NULL);
}

const char*
getJ9CfrErrorNormalMessage(J9PortLibrary* portLib, J9CfrError* error, const U_8* className, UDATA classNameLength)
{
	PORT_ACCESS_FROM_PORT(portLib);
	UDATA allocSize = 0;
	char *errorString = NULL;
	const char *errorDescription = NULL;
	const char *template = NULL;

	errorDescription = getJ9CfrErrorDescription(PORTLIB, error);

	/* J9NLS_CFR_ERROR_TEMPLATE_NO_METHOD=%1$s; class=%3$.*2$s, offset=%4$u */
	template = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_CFR_ERROR_TEMPLATE_NO_METHOD, "%s; class=%.*s, offset=%u");

	allocSize = strlen(template) + strlen(errorDescription) + classNameLength + MAX_INT_SIZE;
	errorString = j9mem_allocate_memory(allocSize, OMRMEM_CATEGORY_VM);
	if (NULL != errorString) {
		j9str_printf(PORTLIB, errorString, allocSize, template, errorDescription, classNameLength, className, error->errorOffset);
	}

	return errorString;
}

const char*
getJ9CfrErrorBsmMessage(J9PortLibrary* portLib, J9CfrError* error, const U_8* className, UDATA classNameLength)
{
	PORT_ACCESS_FROM_PORT(portLib);
	UDATA allocSize = 0;
	char *errorString = NULL;

	/* J9NLS_CFR_ERR_BAD_BOOTSTRAP_ARGUMENT_ENTRY=BootstrapMethod (%1$d) arguments contain invalid constantpool entry at index (#%2$u) of type (%3$u); class=%5$.*4$s, offset=%6$u */
	const char *template = j9nls_lookup_message(J9NLS_ERROR | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_CFR_ERR_BAD_BOOTSTRAP_ARGUMENT_ENTRY,
			"BootstrapMethod (%d) arguments contain invalid constantpool entry at index (#%u) of type (%u); class=%.*s, offset=%u");

	allocSize = strlen(template) + classNameLength + (MAX_INT_SIZE * 4);
	errorString = j9mem_allocate_memory(allocSize, OMRMEM_CATEGORY_VM);
	if (NULL != errorString) {
		j9str_printf(PORTLIB, errorString, allocSize, template,
			error->errorBsmIndex, error->errorBsmArgsIndex, error->errorCPType, classNameLength, className, error->errorOffset);
	}

	return errorString;
}

const char*
getJ9CfrErrorMajorVersionMessage(J9PortLibrary* portLib, J9CfrError* error, const U_8* className, UDATA classNameLength)
{
	PORT_ACCESS_FROM_PORT(portLib);
	const char *template = j9nls_lookup_message(J9NLS_ERROR | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_CFR_ERR_MAJOR_VERSION2, NULL);
	UDATA allocSize = strlen(template) + classNameLength + (MAX_INT_SIZE * 4) + 1;
	char *errorString = j9mem_allocate_memory(allocSize, OMRMEM_CATEGORY_VM);

	if (NULL != errorString) {
		j9str_printf(PORTLIB, errorString, allocSize, template,
			error->errorMajorVersion, error->errorMinorVersion, classNameLength, className, error->errorMaxMajorVersion, error->errorOffset);
	}

	return errorString;
}

const char*
getJ9CfrErrorMinorVersionMessage(J9PortLibrary* portLib, J9CfrError* error, const U_8* className, UDATA classNameLength)
{
	PORT_ACCESS_FROM_PORT(portLib);
	const char *template = j9nls_lookup_message(J9NLS_ERROR | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_CFR_ERR_MINOR_VERSION2, NULL);
	UDATA allocSize = strlen(template) + classNameLength + (MAX_INT_SIZE * 3) + 1;
	char *errorString = j9mem_allocate_memory(allocSize, OMRMEM_CATEGORY_VM);

	if (NULL != errorString) {
		j9str_printf(PORTLIB, errorString, allocSize, template,
			classNameLength, className, error->errorMinorVersion, error->errorMajorVersion, error->errorOffset);
	}

	return errorString;
}

const char*
getJ9CfrErrorPreviewVersionMessage(J9PortLibrary* portLib, J9CfrError* error, const U_8* className, UDATA classNameLength)
{
	PORT_ACCESS_FROM_PORT(portLib);
	const char *template = j9nls_lookup_message(J9NLS_ERROR | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_CFR_ERR_PREVIEW_VERSION2, NULL);
	UDATA allocSize = strlen(template) + classNameLength + (MAX_INT_SIZE * 4) + 1;
	char *errorString = j9mem_allocate_memory(allocSize, OMRMEM_CATEGORY_VM);

	if (NULL != errorString) {
		j9str_printf(PORTLIB, errorString, allocSize, template,
			error->errorMajorVersion, error->errorMinorVersion, classNameLength, className, error->errorMaxMajorVersion, error->errorOffset);
	}

	return errorString;
}

const char*
getJ9CfrErrorPreviewVersionNotEnabledMessage(J9PortLibrary* portLib, J9CfrError* error, const U_8* className, UDATA classNameLength)
{
	PORT_ACCESS_FROM_PORT(portLib);
	const char *template = j9nls_lookup_message(J9NLS_ERROR | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_CFR_ERR_PREVIEW_VERSION_NOT_ENABLED, NULL);
	UDATA allocSize = strlen(template) + classNameLength + (MAX_INT_SIZE * 3) + 1;
	char *errorString = j9mem_allocate_memory(allocSize, OMRMEM_CATEGORY_VM);

	if (NULL != errorString) {
		j9str_printf(PORTLIB, errorString, allocSize, template,
			error->errorMajorVersion, error->errorMinorVersion, classNameLength, className, error->errorOffset);
	}

	return errorString;
}

const char*
getJ9CfrErrorDetailMessageNoMethod(J9PortLibrary* portLib, J9CfrError* error, const U_8* className, UDATA classNameLength)
{
	const char *errorString = NULL;

	switch (error->errorCode) {
	case J9NLS_CFR_ERR_BAD_BOOTSTRAP_ARGUMENT_ENTRY__ID:
		errorString = getJ9CfrErrorBsmMessage(portLib, error, className, classNameLength);
		break;
	case J9NLS_CFR_ERR_MAJOR_VERSION2__ID:
		errorString = getJ9CfrErrorMajorVersionMessage(portLib, error, className, classNameLength);
		break;
	case J9NLS_CFR_ERR_MINOR_VERSION2__ID:
		errorString = getJ9CfrErrorMinorVersionMessage(portLib, error, className, classNameLength);
		break;
	case J9NLS_CFR_ERR_PREVIEW_VERSION2__ID:
		errorString = getJ9CfrErrorPreviewVersionMessage(portLib, error, className, classNameLength);
		break;
	case J9NLS_CFR_ERR_PREVIEW_VERSION_NOT_ENABLED__ID:
		errorString = getJ9CfrErrorPreviewVersionNotEnabledMessage(portLib, error, className, classNameLength);
		break;
	default:
		errorString = getJ9CfrErrorNormalMessage(portLib, error, className, classNameLength);
		break;
	}

	return errorString;
}

const char*
getJ9CfrErrorDetailMessageForMethod(J9PortLibrary* portLib, J9CfrError* error, const U_8* className, UDATA classNameLength, const U_8* methodName, UDATA methodNameLength, const U_8* methodSignature, UDATA methodSignatureLength, const U_8* detailedException, UDATA detailedExceptionLength)
{
	PORT_ACCESS_FROM_PORT( portLib );
	UDATA allocSize = 0;
	char *errorString;
	const char* errorDescription;
	const char *template;

	errorDescription = getJ9CfrErrorDescription(PORTLIB, error);

	/* J9NLS_CFR_ERROR_TEMPLATE_METHOD=%1$s; class=%3$.*2$s, method=%5$.*4$s%7$.*6$s, pc=%8$u */
	template = j9nls_lookup_message(J9NLS_DO_NOT_PRINT_MESSAGE_TAG | J9NLS_DO_NOT_APPEND_NEWLINE, J9NLS_CFR_ERROR_TEMPLATE_METHOD, "%s; class=%.*s, method=%.*s%.*s, pc=%u");

	allocSize = strlen(template) + strlen(errorDescription) + MAX_INT_SIZE + classNameLength + methodNameLength + methodSignatureLength + detailedExceptionLength;

	errorString = j9mem_allocate_memory(allocSize, OMRMEM_CATEGORY_VM);
	if (errorString != NULL) {
		UDATA cursor = j9str_printf(PORTLIB,
			errorString, 
			allocSize,
			template, 
			errorDescription, 
			classNameLength, 
			className, 
			methodNameLength,
			methodName,
			methodSignatureLength,
			methodSignature,
			error->errorPC);

		/* Jazz 82615: Print the detailed exception info to the error message buffer if not empty */
		if ((NULL != detailedException) && (detailedExceptionLength > 0)) {
			j9str_printf(PORTLIB, &errorString[cursor], allocSize - cursor, "%.*s", detailedExceptionLength, detailedException);
		}
	}

	return errorString;
}

void
buildError(J9CfrError * errorStruct, UDATA code, UDATA action, UDATA offset)
{
	errorStruct->errorCode = (U_16) code;
	errorStruct->errorAction = (U_16) action;
	errorStruct->errorCatalog = (U_32) J9NLS_CFR_ERR_NO_ERROR__MODULE;
	errorStruct->errorOffset = (U_32) offset;
	errorStruct->errorMethod = -1;
	errorStruct->errorPC = 0;
	errorStruct->errorMember = NULL;
	errorStruct->constantPool = NULL;

	/* Jazz 82615: Initialize with default values if detailed error message is not required */
	errorStruct->verboseErrorType = 0;
	errorStruct->errorDataIndex = 0;
	errorStruct->errorFrameIndex = -1;
	errorStruct->errorFrameBCI = 0;

	errorStruct->errorBsmIndex = -1;
	errorStruct->errorBsmArgsIndex = 0;
	errorStruct->errorCPType = 0;

	errorStruct->errorMaxMajorVersion = 0;
	errorStruct->errorMajorVersion = 0;
	errorStruct->errorMinorVersion = 0;
}

void
buildBootstrapMethodError(J9CfrError * errorStruct, UDATA code, UDATA action, UDATA offset, I_32 bsmIndex, U_32 bsmArgsIndex, U_32 cpType)
{
	buildError(errorStruct, code, action, offset);
	errorStruct->errorBsmIndex = bsmIndex;
	errorStruct->errorBsmArgsIndex = bsmArgsIndex;
	errorStruct->errorCPType = cpType;
}

void
buildMethodError(J9CfrError * errorStruct, UDATA code, UDATA action, I_32 methodIndex, U_32 pc, J9CfrMethod* method, J9CfrConstantPoolInfo* constantPoolPointer)
{
	errorStruct->constantPool = constantPoolPointer;
	errorStruct->errorCode = (U_16) code;
	errorStruct->errorAction = (U_16) action;
	errorStruct->errorCatalog = (U_32) J9NLS_CFR_ERR_NO_ERROR__MODULE;
	errorStruct->errorOffset = 0;
	errorStruct->errorMethod = methodIndex;
	errorStruct->errorPC = pc;
	errorStruct->errorMember = method;
	
	/* Jazz 82615: Initialize with default values if detailed error message is not required */
	errorStruct->verboseErrorType = 0;
	errorStruct->errorDataIndex = 0;
	errorStruct->errorFrameIndex = -1;
	errorStruct->errorFrameBCI = 0;

	errorStruct->errorBsmIndex = -1;
	errorStruct->errorBsmArgsIndex = 0;
	errorStruct->errorCPType = 0;
}

void
buildMethodErrorWithExceptionDetails(J9CfrError * errorStruct, UDATA code, I_32 verboseErrorType, UDATA action, I_32 methodIndex, U_32 pc, J9CfrMethod* method, J9CfrConstantPoolInfo* constantPoolPointer, U_32 errorDataIndex, I_32 stackmapFrameIndex, U_32 stackmapFrameBCI)
{
	buildMethodError(errorStruct, code, action, methodIndex, pc, method, constantPoolPointer);
	errorStruct->verboseErrorType = verboseErrorType;
	errorStruct->errorDataIndex = errorDataIndex;
	errorStruct->errorFrameIndex = stackmapFrameIndex;
	errorStruct->errorFrameBCI = stackmapFrameBCI;
}

