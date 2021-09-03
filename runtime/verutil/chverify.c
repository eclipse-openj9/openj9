/*******************************************************************************
 * Copyright (c) 1991, 2021 IBM Corp. and others
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

#include <string.h>
#include "cfreader.h"

#include "bcverify_api.h"

/**
 * Method responsible for checking name validity according to the following rules.
 * Methods, fields, local variables can not contain: '.', ';', '[' '/'.
 * Methods, other than <init> and <clinit> cannot contain '<' or '>'.
 * Classes can contain '[' only at the front if they are array classes.
 * Classes can end with ';' only if they are array classes or valuetypes.
 * Classes can contain '/'
 * 		if not the first character,
 * 		if not the last character,
 * 		if not back to back (ie: //),
 *
 *  @return -1 if invalid.  0 or class arity if valid.
 */
static VMINLINE I_32
checkNameImpl (J9CfrConstantPoolInfo * info, BOOLEAN isClass, BOOLEAN isMethod, BOOLEAN isLoading)
{
	/* The separator is '.' in loading classes.
	 * The separator is '/' in verifying classes.
	 */
	BOOLEAN separator = FALSE;
	I_32 arity = 0;
	U_32 length = info->slot1;
	U_8 *c = info->bytes;
	U_8 *end = c + info->slot1;
	
	if (isClass) {
		/* strip leading ['s for array classes */
		for (; ('[' == *c) && (c < end); c++) {
			arity++;
		}
		length -= arity;
	}
	if (c >= end){
		/* Name must be more than just [s.  Also catches empty string case */
		return -1;
	}
	
	while (c < end) {
		/* check for illegal characters:  46(.) 47(/) 59(;) 60(<) 70(>) 91([) 123({) 125(}) */
		switch(*c) {
		case '.':
			/* Only valid for the loading classes */
			if (!isClass || !isLoading) {
				return -1;
			}
			/* FALLTHRU */
		case '/':
			if (isClass) {
				/* Only valid between identifiers and not at end if not in loading classes */
				if ((isLoading && ('/' == *c)) || separator) {
					return -1;
				}
				separator = TRUE;
				break;
			}
			return -1;
		case ';':
			if (isClass) {
				/* Valid at the end of array classes or for valuetypes */
				if ((arity || IS_QTYPE(*info->bytes))
					&& ((c + 1) == end)) {
					break;
				}
			}
			return -1;
		case '<': /* Fall through */
		case '>':
			if (isMethod) {
				return -1;
			}
			separator = FALSE; /* allow /<>/ as a pattern, per test suites */
			break;
		case '{': /* Fall through */
		case '}':
			if (isClass) {
				return -1;
			}
			break;
		case '[': return -1;
		default:
			/* Do Nothing */
			separator = FALSE;
		}
		if (length < 1) return -1;
		length--;
		c++;
	}
	
	/* the separator shouldn't exist at the end of name, regardless of class and method */
	if (separator) {
		return -1;
	}

	return arity;
}


static VMINLINE I_32
isInitOrClinitImpl (J9CfrConstantPoolInfo * info)
{
	U_8 *name = info->bytes;

	/* Handle <init>/<clinit> cases */
	if (*name == '<') {
		if (J9UTF8_DATA_EQUALS("<init>", 6, name, info->slot1)) {
			return CFR_METHOD_NAME_INIT;
		}
		if (J9UTF8_DATA_EQUALS("<clinit>", 8, name, info->slot1)) {
			return CFR_METHOD_NAME_CLINIT;
		}
		return CFR_METHOD_NAME_INVALID;
	}
	return 0; /* not <init> or <clinit> */
}

/**
 * Determine if this name is either "<init>" or "<clinit>".
 *
 * @returns 0 if name is a normal name, 1 if '<init>' and 2 if '<clinit>' , -1 if it starts with '<' but is not a valid class name.
 * @note result is positive if the name is "<init>" or "<clinit>", result is negative if the name is illegal
 */
I_32
bcvIsInitOrClinit (J9CfrConstantPoolInfo * info)
{
	return isInitOrClinitImpl(info);
}

/**
 * Determine if this a valid name for Methods.
 *
 * @returns 1 if '<init>' and 2 if '<clinit>', otherwise 0 or positive if a valid name; negative value if class name is invalid
 */
I_32
bcvCheckMethodName (J9CfrConstantPoolInfo * info)
{
	U_8 *c = info->bytes;
	I_32 nameStatus = isInitOrClinitImpl(info);
	if (0 == nameStatus) {
		nameStatus = checkNameImpl(info, FALSE, TRUE, FALSE);
	}
	return nameStatus;
}

/**
 * Determine if this a valid name for Classes.
 *
 * @returns The arity of the class if valid, -1 otherwise
 */
I_32
bcvCheckClassName (J9CfrConstantPoolInfo * info)
{
	/* Class checks, not method checks */
	return checkNameImpl(info, TRUE, FALSE, FALSE);
}

/**
 * Determine if this a valid name for fields, local variables, etc.  Can be used
 * to check anything except method names and class names.
 *
 * @returns 0 if valid, -1 otherwise.
 */
I_32
bcvCheckName (J9CfrConstantPoolInfo * info) {
	return checkNameImpl(info, FALSE, FALSE, FALSE);
}
