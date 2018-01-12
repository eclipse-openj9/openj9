/*******************************************************************************
 * Copyright (c) 2015, 2015 IBM Corp. and others
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

#include "cfreader.h"
#include "j9protos.h"

static I_32
skipAnnotationElement(J9ROMConstantPoolItem const * constantPool, U_8 const *data, U_8 const **pIndex, U_8 const * dataEnd);

/**
 * @param (in) constantPool pointer to ROM class constant pool
 * @param (in) searchString name of the annotation class
 * @param (in) numAnnotations number of annotations
 * @param (in) data pointer to start of an attribute.
 * @param (inout) pIndex pointer to pointer to the current position in the attribute
 * @param (in) dataEnd pointer to the end of the data.
 * @return number of element/value pairs, -1 if searchString not found, -2 if an error occurred
 */
I_32
getAnnotationByType(J9ROMConstantPoolItem const *constantPool, J9UTF8 const *searchString,
	U_32 const numAnnotations, U_8 const *data, U_8 const **pIndex,  U_8 const *dataEnd) {
	I_32 result = -1;
	U_32 errorCode = 0; /* used by CHECK_EOF */
	U_32 offset = 0; /* used by CHECK_EOF */
	U_8 const *index = *pIndex; /* name used by CHECK_EOF */
	U_32 annCount = 0;

	for (annCount = 0; (annCount < numAnnotations) && (-1 == result); ++annCount) {
		J9UTF8 *className = NULL;
		U_32 numElementValuePairs = 0;
		U_16 annTypeIndex = 0;

		CHECK_EOF(2);
		NEXT_U16(annTypeIndex, index);

		className = J9ROMCLASSREF_NAME((J9ROMClassRef*)constantPool+annTypeIndex);
		CHECK_EOF(2);
		NEXT_U16(numElementValuePairs, index);
		if (J9UTF8_EQUALS(className, searchString)) {
			result = numElementValuePairs;
		} else {
			while (numElementValuePairs > 0) {
				if (0 != skipAnnotationElement(constantPool, data, &index, dataEnd)) {
					result = -2; /* bad annotation */
					break;
				}
				numElementValuePairs--;
			}
		}
	}
	*pIndex = index;
	return result;
_errorFound:
	return -2;
}

static I_32
skipAnnotationElement(J9ROMConstantPoolItem const * constantPool, U_8 const *data, U_8 const **pIndex, U_8 const * dataEnd)
{
	U_8 tag = 0;
	I_32 result = 0;
	U_32 errorCode = 0; /* used by CHECK_EOF */
	U_32 offset = 0; /* used by CHECK_EOF */
	U_8 const *index = *pIndex; /* used by CHECK_EOF */

	CHECK_EOF(1);
	NEXT_U8(tag, index);

	switch (tag) {
	case 'B':
	case 'C':
	case 'D':
	case 'F':
	case 'I':
	case 'J':
	case 'S':
	case 'Z':
	case 's':
	case 'c':
		CHECK_EOF(2);
		data += 2;
		break;

	case 'e':
		CHECK_EOF(4);
		data += 4;
		break;

	case '@':{
		U_32 numValues = 0;
		U_32 j = 0;
		CHECK_EOF(4);
		data += 2; /* skip type_index */
		NEXT_U16(numValues, index);
		for (j = 0; (j < numValues) && (0 == result); j++) {
			result = skipAnnotationElement(constantPool, data, &index, dataEnd);
		}
		break;
	}

	case '[': {
		U_32 numValues = 0;
		U_32 j = 0;
		CHECK_EOF(2);
		NEXT_U16(numValues, index);
		for (j = 0; (j < numValues) && (0 == result); j++) {
			result = skipAnnotationElement(constantPool, data, &index, dataEnd);
		}
		break;
	}

	default:
		result = -1;
		break;
	}
	*pIndex = index;
	return result;

_errorFound:
	*pIndex = index;
	return -1;
}
