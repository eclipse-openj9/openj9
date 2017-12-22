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
#include "j9protos.h"
#include "j9port.h"

static I_32 verifyWildcard (J9PortLibrary *portLib, const char* pattern, const char* expectedMatch, const char* expectedMismatch, UDATA *passCount, UDATA *failCount);


I_32 
verifyWildcards(J9PortLibrary *portLib, UDATA *passCount, UDATA *failCount) 
{
	const char* pattern;
	const char* needleString;
	UDATA needleLength;
	U_32 matchFlags;
	I_32 rc = 0;
	PORT_ACCESS_FROM_PORT(portLib);

	j9tty_printf(PORTLIB, "Testing wildcard functions...\n");

	/* test that wildcards are permitted in the appropriate locations */
	rc |= verifyWildcard(portLib, "*START", "START", "STARTMISMATCH", passCount, failCount);
	rc |= verifyWildcard(portLib, "*START", "MATCHSTART", "ANOTHERSTARTMISMATCH", passCount, failCount);
	rc |= verifyWildcard(portLib, "*START", "MATCHSTART", "", passCount, failCount);

	rc |= verifyWildcard(portLib, "END*", "END", "MISMATCHEND", passCount, failCount);
	rc |= verifyWildcard(portLib, "END*", "ENDMATCH", "ANOTHERENDMISMATCH", passCount, failCount);
	rc |= verifyWildcard(portLib, "END*", "ENDMATCH", "", passCount, failCount);

	rc |= verifyWildcard(portLib, "*BOTH*", "BOTH", "BOT", passCount, failCount);
	rc |= verifyWildcard(portLib, "*BOTH*", "MATCHBOTH", "OTH", passCount, failCount);
	rc |= verifyWildcard(portLib, "*BOTH*", "MATCHBOTHANYWHERE", "", passCount, failCount);

	rc |= verifyWildcard(portLib, "NONE", "NONE", "_NONE", passCount, failCount);
	rc |= verifyWildcard(portLib, "NONE", "NONE", "NONE_", passCount, failCount);
	rc |= verifyWildcard(portLib, "NONE", "NONE", "", passCount, failCount);

	rc |= verifyWildcard(portLib, "*", "ANYTHING", NULL, passCount, failCount);
	rc |= verifyWildcard(portLib, "*", "", NULL, passCount, failCount);

	rc |= verifyWildcard(portLib, "**", "ANYTHING", NULL, passCount, failCount);
	rc |= verifyWildcard(portLib, "**", "", NULL, passCount, failCount);

	rc |= verifyWildcard(portLib, "", "", "X", passCount, failCount);

	/* test NULL and 0 length */
	if (parseWildcard(NULL, 0, &needleString, &needleLength, &matchFlags)) {
		j9tty_err_printf(PORTLIB, "Unexpected error parsing NULL pattern\n");
		(*failCount)++;
	} else {
		(*passCount)++;
	}

	/* test expected error cases */
	pattern = "X*X";
	if (!parseWildcard(pattern, strlen(pattern), &needleString, &needleLength, &matchFlags)) {
		j9tty_err_printf(PORTLIB, "Unexpected success parsing \"%s\"\n", pattern);
		(*failCount)++;
	} else {
		(*passCount)++;
	}

	j9tty_printf(PORTLIB, "Finished testing wildcard functions.\n");

	return rc;
}



static I_32 
verifyWildcard(J9PortLibrary *portLib, const char* pattern, const char* expectedMatch, const char* expectedMismatch, UDATA *passCount, UDATA *failCount) 
{
	const char* needleString;
	UDATA needleLength;
	U_32 matchFlags;
	PORT_ACCESS_FROM_PORT(portLib);

	if (parseWildcard(pattern, strlen(pattern), &needleString, &needleLength, &matchFlags)) {
		j9tty_err_printf(PORTLIB, "Unexpected error parsing \"%s\"\n", pattern);
		return -1;
	}

	if (expectedMatch) {
		if (!wildcardMatch(matchFlags, needleString, needleLength, expectedMatch, strlen(expectedMatch))) {
			j9tty_err_printf(PORTLIB, "Failed to match \"%s\" with \"%s\"\n", expectedMatch, pattern);
			(*failCount)++;
			return 0;
		}
	}

	if (expectedMismatch) {
		if (wildcardMatch(matchFlags, needleString, needleLength, expectedMismatch, strlen(expectedMismatch))) {
			j9tty_err_printf(PORTLIB, "Unexpectedly matched \"%s\" with \"%s\"\n", expectedMismatch, pattern);
			(*failCount)++;
			return 0;
		}
	}

	(*passCount)++;
	return 0;
}



