/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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
#include "j9protos.h"
#include "argcount.h"
#include "cfreader.h"


/* The conversion table stores the CFR_STACKMAP_TYPE_XXX values (as the index of dataTypeNames[] in errormessage_internal.c)
 * so as to identify the data type string of argument in signature defined in dataTypeNames[].
 * dataTypeNames[] is arranged based on the value of CFR_STACKMAP_TYPE_XXX so as to locate the expected
 * name string of data type. Thus, there is no such kind of array in vrfyconvert.c intended for this special purpose.
 * (e.g. baseTypeCharConversion[] or argTypeCharConversion[]. They return BCV_BASE_TYPE_XXX values
 * which are huge to be used as indexs of a name string array to locate the name string of data type)
 */
static const U_32 argumentTypeConversion[] = {
0,						CFR_STACKMAP_TYPE_INT,		CFR_STACKMAP_TYPE_INT,	CFR_STACKMAP_TYPE_DOUBLE,
0,						CFR_STACKMAP_TYPE_FLOAT,	0,						0,
CFR_STACKMAP_TYPE_INT,	CFR_STACKMAP_TYPE_LONG,		0,						CFR_STACKMAP_TYPE_OBJECT,
0,						0,							0,						0,
0,						0,							CFR_STACKMAP_TYPE_INT,	0,
0,						0,							0,						0,
0,						CFR_STACKMAP_TYPE_INT,		0};


/**
 * 	Check the validity of a field signature.
 * 	@param signatureBytes pointer to first character of the signature
 * 	@param signatureLength Length of the signature in bytes
 * 	@param currentIndex starting point of the signature string, updated to the end of the scan
 *  @param argumentType - data type of the argument
 *	@return the slot count of argument in bytes on success (double/long type takes 2 slots, the others take 1 slot)
 *	        -2 if arity > 255
 *	        -1 on other failure
 */
static IDATA checkSignatureImpl (U_8* signatureBytes, UDATA signatureLength, UDATA *currentIndex);
static IDATA checkSignatureInlined (U_8* signatureBytes, UDATA signatureLength, UDATA *currentIndex, U_8* argumentType);
static IDATA verifyIdentifierUtf8Impl (U_8* identifierStart, U_8 *limit, BOOLEAN allowSlashes);

IDATA 
verifySignatureUtf8(U_8* signatureBytes, UDATA signatureLength) {
	UDATA currentIndex = 0;

	return checkSignatureImpl(signatureBytes, signatureLength, &currentIndex);
}

IDATA 
verifyFieldSignatureUtf8(U_8* signatureBytes, UDATA signatureLength, UDATA currentIndex) {
	IDATA argCount = checkSignatureImpl(signatureBytes, signatureLength, &currentIndex);
	if (argCount < 0) {
		return argCount;
	} else	if (currentIndex != signatureLength) {
		return -1;
	} else {
		return 0;
	}
}

IDATA 
verifyMethodSignatureUtf8(U_8* signatureBytes, UDATA signatureLength) {
	UDATA index = 1;
	BOOLEAN moreArgs = TRUE;
	IDATA argCount = 0;

	if (*signatureBytes != '(') {
		return -1;
	}

	while (moreArgs) {

		if (index > signatureLength) {
			return -1;
		}

		if (signatureBytes[index] != ')') {
			IDATA fieldArgs = 0;
			if ((fieldArgs = checkSignatureImpl(signatureBytes, signatureLength, &index)) < 0) {
				return -1;
			}
			argCount += fieldArgs;
		} else {
			moreArgs = FALSE;
			index++;
		}
	}

	/* V is valid for the return signature so only check if not V */
	if (signatureBytes[index] != 'V') {
		if (verifyFieldSignatureUtf8(signatureBytes, signatureLength, index) < 0) {
			return -1;
		}
	} else if ((index + 1) != signatureLength) {
		return -1;
	}
	return argCount;
}

/**
 * stop at the limit of the string, or when we see a semicolon.
 * @param identifierStart point to first character in the string
 * @param limit pointer to the byte following the last character to be scanned.
 * @param allowSlashes true if isolated slashes are allowed (i.e. for fully qualified class names)
 * @return number of characters scanned (not including terminating semicolon), or -1 if illegal sequence found.
 */
static VMINLINE IDATA
verifyIdentifierUtf8Impl (U_8* identifierStart, U_8 *limit, BOOLEAN allowSlashes)
{

	U_8 *cursor = identifierStart;
	BOOLEAN slash = FALSE;

	while ((';' != *cursor) && (cursor < limit)) {
		/* check for subset of illegal characters:  46(.) 47(/) 91([) in JVMS 4.2.2*/
		switch (*cursor) {
		case '/':
			/* can't have two slashes next to each other */
			if (!allowSlashes || slash) {
				return -1;
			}
			slash = TRUE;
			break;
		case '.': /* FALLTHROUGH */
		case '[':
			return -1;
			break;
		default:
			/* Do Nothing */
			slash = FALSE;
			break;
		}
		++cursor;
	}
	/* Identifier must not end in / (i.e. must have specified a type name after any package name) */
	if (slash) {
		return -1;
	}
	return cursor - identifierStart;
}

IDATA
fetchArgumentOfSignature (U_8* signatureBytes, UDATA signatureLength, UDATA *currentIndex, U_8* argumentType)
{
	return checkSignatureInlined(signatureBytes, signatureLength, currentIndex, argumentType);
}

static IDATA
checkSignatureImpl (U_8* signatureBytes, UDATA signatureLength, UDATA *currentIndex)
{
	return checkSignatureInlined(signatureBytes, signatureLength, currentIndex, NULL);
}

static VMINLINE IDATA
checkSignatureInlined (U_8* signatureBytes, UDATA signatureLength, UDATA *currentIndex, U_8* argumentType)
{
	IDATA argCount = 1;
	IDATA arity = 0;
	U_8 *cursor = &(signatureBytes[*currentIndex]);
	U_8 *signatureEnd = &(signatureBytes[signatureLength]);

	/* Jazz 82615: Initialize the data type of argument */
	if (NULL != argumentType) {
		*argumentType = CFR_STACKMAP_TYPE_OBJECT;
	}

	if (*cursor == '[') {
		while (*cursor == '[') {
			cursor++;
			arity++;
			if (cursor >= signatureEnd) {
				return -1;
			}
		}
		if (arity > 255) {
			return -2;
		}
	}

	if (*cursor == 'L') {
		IDATA bytesConsumed = 0;

		cursor += 1; /* skip the 'L' */
		bytesConsumed = verifyIdentifierUtf8Impl(cursor, signatureEnd, TRUE);

		/* Identifier must end in a semicolon */
		if ((bytesConsumed <= 0) || (';' != cursor[bytesConsumed])) {
			return -1;
		} else {
			cursor += bytesConsumed + 1; /* skip scanned characters and the terminating ';'  */
		}
	} else {
		/* Jazz 82615: Define an index to track the location of argument in signature */
		IDATA index = *cursor - 'A';
		if ((*cursor < 'A') || (*cursor > 'Z')) {
			return -1;
		}
		argCount = argCountCharConversion[index];
		++cursor;
		if (0 == argCount) {
			return -1;
		}
		if (0 != arity) {
			argCount = 1;
		} else {
			/* Jazz 82615: Identify the data type of argument at the specified location in signature against the conversion table */
			if (NULL != argumentType) {
				*argumentType = argumentTypeConversion[index];
			}
		}
	}

	*currentIndex = (UDATA) (cursor - signatureBytes);
	return argCount;
}

BOOLEAN
verifyIdentifierUtf8(U_8* identifierStart, UDATA identifierLength) {
	IDATA charactersParsed = verifyIdentifierUtf8Impl(identifierStart, identifierStart+identifierLength, FALSE);

	return (identifierLength > 0) && (charactersParsed == identifierLength); /* we should have consumed all the characters */
}

BOOLEAN
verifyClassnameUtf8(U_8* identifierStart, UDATA identifierLength) {
	IDATA charactersParsed = verifyIdentifierUtf8Impl(identifierStart, identifierStart+identifierLength, TRUE);

	return (identifierLength > 0) && (charactersParsed == identifierLength); /* we should have consumed all the characters */
}

IDATA
j9bcv_checkFieldSignature (J9CfrConstantPoolInfo * info, UDATA currentIndex)
{
	IDATA index;
	if ((info->flags1 & CFR_FLAGS1_ValidFieldSignature) != 0) {
		return 0;
	}
	index = verifyFieldSignatureUtf8(info->bytes, info->slot1, currentIndex);
	if (index < 0) {
		return index;
	}
	info->flags1 |= CFR_FLAGS1_ValidFieldSignature;
	return index;
}

IDATA
j9bcv_checkMethodSignature (J9CfrConstantPoolInfo * info, BOOLEAN getSlots)
{
	IDATA argCount = 0;

	if ((info->flags1 & CFR_FLAGS1_ValidMethodSignature) != 0) {
		return getSlots? getSendSlotsFromSignature(info->bytes): 0;
	}

	argCount = verifyMethodSignatureUtf8(info->bytes, info->slot1);
	if (argCount >= 0) {
		info->flags1 |= CFR_FLAGS1_ValidMethodSignature;
	}
	return argCount;
}
