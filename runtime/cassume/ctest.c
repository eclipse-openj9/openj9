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

#include "ctest.h"
#include "basesize.h"
#include "exelib_api.h"
#include "vatest.h"
#include "cassume_internal.h"

J9PortLibrary *cTestPortLib = NULL;
UDATA passCount = 0, failCount = 0;

UDATA signalProtectedMain(struct J9PortLibrary *portLibrary, void *arg)
{
	struct j9cmdlineOptions * args = arg;
	int argc = args->argc;
	char **argv = args->argv;
	PORT_ACCESS_FROM_PORT(args->portLibrary);
	cTestPortLib = args->portLibrary;

#ifdef J9VM_OPT_REMOTE_CONSOLE_SUPPORT
	remoteConsole_parseCmdLine( cTestPortLib, argc - 1, argv );
#endif

#ifdef J9VM_OPT_MEMORY_CHECK_SUPPORT
	/* This should happen before anybody allocates memory!  Otherwise, shutdown will not work properly. */
	memoryCheck_parseCmdLine( cTestPortLib, argc-1, argv );
#endif /* J9VM_OPT_MEMORY_CHECK_SUPPORT */

	j9tty_printf(PORTLIB, "C Assumptions Test Start\n");

	verifyJNISizes();
	verifyVAList();
	verifyUDATASizes();

	/* Ensure floatTemp1 is 8-aligned */
	j9_assume(offsetof(J9VMThread, floatTemp1) % 8, 0);
#if defined(J9VM_JIT_TRANSACTION_DIAGNOSTIC_THREAD_BLOCK)
	/* Ensure transactionDiagBlock is 8-aligned */
	j9_assume(offsetof(J9VMThread, transactionDiagBlock) % 8, 0);
#endif /* J9VM_JIT_TRANSACTION_DIAGNOSTIC_THREAD_BLOCK */

	/* Ensure J9ROMClass, J9ROMArrayClass, and J9ROMReflectClass are 64-bit aligned. 64 bits = 8 bytes */
	j9_assume(sizeof(J9ROMClass) % 8, 0);
	j9_assume(sizeof(J9ROMArrayClass) % 8, 0);
	j9_assume(sizeof(J9ROMReflectClass) % 8, 0);

	j9tty_printf(PORTLIB, "C Assumptions Test Finished\n");
	j9tty_printf(PORTLIB, "total tests: %d\n", passCount + failCount);
	j9tty_printf(PORTLIB, "total passes: %d\n", passCount);
	j9tty_printf(PORTLIB, "total failures: %d\n", failCount);

	return 0;
}
