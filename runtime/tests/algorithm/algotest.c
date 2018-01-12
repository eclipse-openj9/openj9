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
#include "algotest.h"
#include "exelib_api.h"
#include "algorithm_test_internal.h"

J9PortLibrary *algoTestPortLib = NULL;

UDATA signalProtectedMain(struct J9PortLibrary *portLibrary, void *arg)
{
	struct j9cmdlineOptions * args = arg;
#if (defined(J9VM_OPT_REMOTE_CONSOLE_SUPPORT) || defined(J9VM_OPT_MEMORY_CHECK_SUPPORT))
	int argc = args->argc;
	char **argv = args->argv;
#endif /* (defined(J9VM_OPT_REMOTE_CONSOLE_SUPPORT) || defined(J9VM_OPT_MEMORY_CHECK_SUPPORT)) */
	I_32 numSuitesNotRun = 0;
	UDATA passCount = 0;
	UDATA failCount = 0;

	PORT_ACCESS_FROM_PORT(args->portLibrary);

	algoTestPortLib = args->portLibrary;

#ifdef J9VM_OPT_REMOTE_CONSOLE_SUPPORT
	remoteConsole_parseCmdLine( algoTestPortLib, argc - 1, argv );
#endif

#ifdef J9VM_OPT_MEMORY_CHECK_SUPPORT
	/* This should happen before anybody allocates memory!  Otherwise, shutdown will not work properly. */
	memoryCheck_parseCmdLine( algoTestPortLib, argc-1, argv );
#endif /* J9VM_OPT_MEMORY_CHECK_SUPPORT */

	j9tty_printf( PORTLIB, "Algorithm Test Start\n");


	if (verifyWildcards(PORTLIB, &passCount, &failCount)) {
		numSuitesNotRun++;
	}

	if (verifyArgScan(PORTLIB, &passCount, &failCount)) {
		numSuitesNotRun++;
	}

	if (verifySimplePools(PORTLIB, &passCount, &failCount)) {
		numSuitesNotRun++;
	}

	if (verifySendSlots(PORTLIB, &passCount, &failCount)) {
		numSuitesNotRun++;
	}

	if (verifySRPHashtable(PORTLIB, &passCount, &failCount)) {
		numSuitesNotRun++;
	}

	if (verifyPrimeNumberHelper(PORTLIB, &passCount, &failCount)) {
		numSuitesNotRun++;
	}

	j9tty_printf( PORTLIB, "Algorithm Test Finished\n");
	j9tty_printf( PORTLIB, "total tests: %d\n", passCount + failCount);
	j9tty_printf( PORTLIB, "total passes: %d\n", passCount);
	j9tty_printf( PORTLIB, "total failures: %d\n", failCount);

	if (numSuitesNotRun) {
		j9tty_printf( PORTLIB, "%i SUITE%s NOT RUN!!!\n", numSuitesNotRun, numSuitesNotRun != 1 ? "S" : "");
	} else if ((failCount == 0) && (passCount)) {
		j9tty_printf( PORTLIB, "ALL TESTS COMPLETED AND PASSED.\n");
		return 0;
	}

	/* want to report failures even if suites skipped or failed */
	if (failCount) {
		j9tty_printf( PORTLIB, "THERE WERE FAILURES!!!\n");
	}

	return -1;
}




