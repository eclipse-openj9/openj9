/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

#include "j9.h"
#include "j9cfg.h"
#include "j9protos.h"
#include "util_internal.h"
#include "ut_j9util.h"

#define WILDCARD_CHARACTER '*'

#define EXACT_MATCH 0x00
#define LEADING_WILDCARD 0x01
#define TRAILING_WILDCARD 0x02
#define BOTH_WILDCARDS (LEADING_WILDCARD | TRAILING_WILDCARD)

/*
 * Parses a wildcard string, storing the result in needle, needleLength and matchFlag.
 * The result string, needle, is a pointer into the input string.
 * This function only supports leading or trailing wild card characters.
 * The only supported wild card character is '*'.
 *
 * Returns 0 on success, or non-zero if the input string contains wild card characters 
 * in an unsupported location.
 */
IDATA
parseWildcard(const char * pattern, UDATA patternLength, const char** needle, UDATA* needleLength, U_32 * matchFlag)
{
	IDATA rc = 0;
	const char *str;

	Trc_Util_parseWildcard_Entry(patternLength, pattern);

	*matchFlag = 0;
	if (patternLength > 0 && *pattern == WILDCARD_CHARACTER) {
		patternLength--;
		pattern++;
		*matchFlag |= LEADING_WILDCARD;
	}

	for (str = pattern; str < pattern + patternLength; str++) {
		if (*str == WILDCARD_CHARACTER) {
			if (str == pattern + patternLength - 1) {
				*matchFlag |= TRAILING_WILDCARD;
				patternLength--;
				break;
			} else {
				/* invalid wildcard */
				Trc_Util_parseWildcard_Error();
				return -1;
			}
		}
	}

	*needleLength = patternLength;
	*needle = pattern;

	Trc_Util_parseWildcard_Exit(patternLength, pattern, *matchFlag);

	return 0;
}



/*
 * Determines if needle appears in haystack, using the match rules
 * encoded in matchFlag.
 * matchFlag, needle and needleLength must have been initialized by calling parseWildcard.
 *
 * Return TRUE on match, FALSE if no match.
 */
IDATA
wildcardMatch(U_32 matchFlag, const char* needle, UDATA needleLength, const char* haystack, UDATA haystackLength)
{
	IDATA retval = FALSE;

	switch(matchFlag) {
	case EXACT_MATCH:
		if(haystackLength == needleLength && memcmp(haystack, needle, needleLength) == 0) {
			retval = TRUE;
		}
		break;
	case LEADING_WILDCARD:
		if(haystackLength >= needleLength) {
			if (memcmp(haystack + (haystackLength - needleLength), needle, needleLength) == 0) {
				retval = TRUE;
			}
		}
		break;
	case TRAILING_WILDCARD:
		if(haystackLength >= needleLength) {
			if (memcmp(haystack, needle, needleLength) == 0) {
				retval = TRUE;
			}
		}
		break;
	case BOTH_WILDCARDS:
		if(needleLength == 0) {
			retval = TRUE;
		} else if (haystackLength >= needleLength) {
			UDATA i;
			for (i = 0; i <= haystackLength - needleLength; i++) {
				if (memcmp(haystack + i, needle, needleLength) == 0) {
					retval = TRUE;
					break;
				}
			}
		}
	}

	return retval;
}





