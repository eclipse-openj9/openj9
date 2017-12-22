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

#include <string.h>
#include <stdlib.h>
#include "exelib_api.h"
#include "testHelpers.h"

#define VMTEST_ALL                     ((UDATA)0xFFFFFFFF)
#define VMTEST_RESOLVEFIELD            ((UDATA)0x00000001)

extern IDATA testResolveField(J9PortLibrary *portLib);


static BOOLEAN
startsWith(const char *s, const char *prefix)
{
	size_t sLen = strlen(s);
	size_t prefixLen = strlen(prefix);
	size_t i;

	if (sLen < prefixLen) {
		return 0;
	}
	for (i = 0; i < prefixLen; i++) {
		if (s[i] != prefix[i]) {
			return FALSE;
		}
	}
	return TRUE;
}

static BOOLEAN
consumeOption(const char **s, const char *prefix)
{
	size_t sLen = strlen(*s);
	size_t prefixLen = strlen(prefix);
	size_t i;
	const char *option = *s;

	if (sLen < prefixLen) {
		return FALSE;
	}
	for (i = 0; i < prefixLen; i++){
		if (option[i] != prefix[i]) {
			return FALSE;
		}
	}
	*s += prefixLen;
	if (sLen > prefixLen) {
		if (**s == ',') {
			*s += 1;
		}
	}
	return TRUE;
}

static UDATA
parseForAreasToTests(struct J9PortLibrary *portLibrary, const char *options)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	UDATA tests = 0;
	const char *allOptions = options;

	while ('\0' != *allOptions) {
		if (consumeOption(&allOptions, "resolvefield")) {
			tests |= VMTEST_RESOLVEFIELD;
		} else {
			j9tty_printf(portLibrary, "\n\nWarning: invalid option (%s) ignored\n\n", allOptions);
			break;
		}
	}

	return tests;
}

UDATA
signalProtectedMain(struct J9PortLibrary *portLibrary, void *arg)
{
	struct j9cmdlineOptions *args = arg;
	int argc = args->argc;
	char **argv = args->argv;
	IDATA rc, i;
	UDATA areasToTest;
	int randomSeed;
	const char *paths[1] = {"./"};

	PORT_ACCESS_FROM_PORT(args->portLibrary);

#ifdef J9VM_OPT_MEMORY_CHECK_SUPPORT
	/* This should happen before anybody allocates memory!  Otherwise, shutdown will not work properly. */
	memoryCheck_parseCmdLine(PORTLIB, argc-1, argv);
#endif /* J9VM_OPT_MEMORY_CHECK_SUPPORT */

#ifdef J9VM_OPT_REMOTE_CONSOLE_SUPPORT
	remoteConsole_parseCmdLine(PORTLIB, argc-1, argv);
#endif /* J9VM_OPT_REMOTE_CONSOLE_SUPPORT */

	/* set up nls */
	j9nls_set_catalog(paths, 1, "java", "properties");

	randomSeed = 0;
	areasToTest = 0;
	for (i = 1; i < argc; i++) {
		if (startsWith(argv[i], "-include:")) {
			areasToTest = parseForAreasToTests(PORTLIB, &argv[i][9]);
		} else if (startsWith(argv[i], "-exclude:")) {
			areasToTest = VMTEST_ALL & ~parseForAreasToTests(PORTLIB, &argv[i][9]);
		} else if (startsWith(argv[i], "-srand:")) {
			randomSeed = atoi(&argv[i][7]);
		}
	}

	rc = 0;

	/* If nothing specified, then run all tests */
	if (0 == areasToTest) {
		areasToTest = VMTEST_ALL;
	}

	if (VMTEST_RESOLVEFIELD == (areasToTest & VMTEST_RESOLVEFIELD)) {
		rc |= testResolveField(PORTLIB);
	}

	if (rc) {
		dumpTestFailuresToConsole(portLibrary);
	} else {
		j9tty_printf(PORTLIB,"\nALL TESTS COMPLETED AND PASSED\n");
	}

	return 0;
}
