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

#define J9DYN_TEST_ALL                     ((UDATA)0xFFFFFFFF)
#define J9DYN_TEST_ROMCLASSCORRECTNESS     ((UDATA)0x00000001)
#define J9DYN_TEST_ROMCLASSCOMPARE         ((UDATA)0x00000002)
#define J9DYN_TEST_MISC                    ((UDATA)0x00000004)
#define J9DYN_TEST_INTERNING               ((UDATA)0x00000008)
#define J9DYN_TEST_LINENUMBERS             ((UDATA)0x00000010)
#define J9DYN_TEST_LOCALVARIABLETABLE      ((UDATA)0x00000020)

extern IDATA j9dyn_testROMClassCorrectness(J9PortLibrary *portLib);
extern IDATA j9dyn_testROMClassCompare(J9PortLibrary *portLib);
extern IDATA j9dyn_testMisc(J9PortLibrary *portLib);
extern IDATA j9dyn_testInterning(J9PortLibrary *portLib, int randomSeed);
extern IDATA j9dyn_lineNumber_tests(J9PortLibrary *portLib, int randomSeed);
extern IDATA j9dyn_localvariabletable_tests(J9PortLibrary *portLib, int randomSeed);

/*helpers*/
static int
startsWith(char *s, char *prefix)
{
	size_t sLen=strlen(s);
	size_t prefixLen=strlen(prefix);
	size_t i;

	if (sLen < prefixLen) {
		return 0;
	}
	for (i=0; i<prefixLen; i++){
		if (s[i] != prefix[i]) {
			return 0;
		}
	}
	return 1; /*might want to make sure s is not NULL or something*/
}

static int
consumeOption(char **s, char *prefix)
{
	size_t sLen=strlen(*s);
	size_t prefixLen=strlen(prefix);
	size_t i;
	char *option = *s;

	if (sLen < prefixLen) {
		return 0;
	}
	for (i=0; i<prefixLen; i++){
		if (option[i] != prefix[i]) {
			return 0;
		}
	}
	*s += prefixLen;
	if (sLen > prefixLen) {
		if (**s == ',') {
			*s += 1;
		}
	}
	return 1;
}

static UDATA
parseForAreasToTests(struct J9PortLibrary *portLibrary, char *options)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	UDATA userParm = 0;
	char *allOptions = options;

	while (strlen(allOptions) > 0) {
		if (consumeOption(&allOptions, "rccorrectness")) {
			userParm |= J9DYN_TEST_ROMCLASSCORRECTNESS;
		} else if (consumeOption(&allOptions, "rccompare")) {
			userParm |= J9DYN_TEST_ROMCLASSCOMPARE;
		} else if (consumeOption(&allOptions, "misc")) {
			userParm |= J9DYN_TEST_MISC;
		} else if (consumeOption(&allOptions, "intern")) {
			userParm |= J9DYN_TEST_INTERNING;
		} else if (consumeOption(&allOptions, "linenumbers")) {
			userParm |= J9DYN_TEST_LINENUMBERS;
		} else if (consumeOption(&allOptions, "localvariabletable")) {
			userParm |= J9DYN_TEST_LOCALVARIABLETABLE;
		} else {
			j9tty_printf(PORTLIB, "\n\nWarning: invalid option (%s) ignored\n\n", allOptions);
			break;
		}
	}
	return userParm;
}

UDATA
signalProtectedMain(struct J9PortLibrary *portLibrary, void *arg)
{
	struct j9cmdlineOptions * args = arg;
	int argc = args->argc;
	char **argv = args->argv;
	I_32 rc,i;
	BOOLEAN isClient = 0;
	BOOLEAN isHost = 0;
	BOOLEAN isChild = 0;
	BOOLEAN shsem_child = 0;
	char serverName[99]="";
	char testName[99]="";
	UDATA areasToTest = 0;
	PORT_ACCESS_FROM_PORT(args->portLibrary);
	char srand[99]="";
	int randomSeed = 0;
	const char *paths[1] = {"./"};

#ifdef J9VM_OPT_MEMORY_CHECK_SUPPORT
	/* This should happen before anybody allocates memory!  Otherwise, shutdown will not work properly. */
	memoryCheck_parseCmdLine(PORTLIB, argc-1, argv);
#endif /* J9VM_OPT_MEMORY_CHECK_SUPPORT */

#ifdef J9VM_OPT_REMOTE_CONSOLE_SUPPORT
	remoteConsole_parseCmdLine(PORTLIB, argc-1, argv);
#endif /* J9VM_OPT_REMOTE_CONSOLE_SUPPORT */

	/* set up nls */
	j9nls_set_catalog(paths, 1, "java", "properties");

	/* host, shsem_child and shmem_child preclude any other test running */
	for (i=1; i<argc; i++){
		if (startsWith(argv[i],"-include:")) {
			areasToTest = parseForAreasToTests(PORTLIB, &argv[i][9]);
		} else if (startsWith(argv[i],"-exclude:")) {
			areasToTest = J9DYN_TEST_ALL & ~parseForAreasToTests(PORTLIB, &argv[i][9]);
		} else if (startsWith(argv[i],"-srand:")) {
			strcpy(srand,&argv[i][7]);
			randomSeed = atoi(srand);
		}
	}

	rc = 0;

	/* If nothing specified, then run all tests */
	if (0 == areasToTest) {
		areasToTest = J9DYN_TEST_ALL;
	}

	if (J9DYN_TEST_ROMCLASSCORRECTNESS ==(areasToTest & J9DYN_TEST_ROMCLASSCORRECTNESS)) {
		rc |= j9dyn_testROMClassCorrectness(PORTLIB);
	}
	if (J9DYN_TEST_ROMCLASSCOMPARE ==(areasToTest & J9DYN_TEST_ROMCLASSCOMPARE)) {
		rc |= j9dyn_testROMClassCompare(PORTLIB);
	}
	if (J9DYN_TEST_MISC ==(areasToTest & J9DYN_TEST_MISC)) {
		rc |= j9dyn_testMisc(PORTLIB);
	}
	if (J9DYN_TEST_INTERNING ==(areasToTest & J9DYN_TEST_INTERNING)) {
		rc |= j9dyn_testInterning(PORTLIB, randomSeed);
	}
	if (J9DYN_TEST_LINENUMBERS ==(areasToTest & J9DYN_TEST_LINENUMBERS)) {
		rc |= j9dyn_lineNumber_tests(PORTLIB, randomSeed);
	}
	if (J9DYN_TEST_LOCALVARIABLETABLE ==(areasToTest & J9DYN_TEST_LOCALVARIABLETABLE)) {
		rc |= j9dyn_localvariabletable_tests(PORTLIB, randomSeed);
	}


	if (rc) {
		dumpTestFailuresToConsole(portLibrary);
	} else {
		j9tty_printf(PORTLIB,"\nALL TESTS COMPLETED AND PASSED\n");
	}

	return 0;
}



