/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
#include "OSCacheTest.hpp"

extern "C" {
#include "j9port.h"
}

#include "main.h"
#include "OSCacheTestMmap.hpp"
#include "OSCacheTestSysv.hpp"
#include "OSCacheTestMisc.hpp"

extern "C" {

IDATA
testOSCache(J9JavaVM* vm, struct j9cmdlineOptions *arg, const char *cmdline)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	IDATA rc = PASS;

	/* Detect children */
	if(cmdline != NULL) {
		/* Search cmdline for the right prefix so it can be forwarded appropriately */
		const char *result;
		if(NULL != (result = strstr(cmdline, OSCACHETESTSYSV_CMDLINE_PREFIX))) {
			return SH_OSCacheTestSysv::runTests(vm, arg, result);
		} else if(NULL != (result = strstr(cmdline, OSCACHETESTMMAP_CMDLINE_PREFIX))) {
			return SH_OSCacheTestMmap::runTests(vm, arg, result);
		} else if(NULL != (result = strstr(cmdline, OSCACHETESTMISC_CMDLINE_PREFIX))) {
			return SH_OSCacheTestMisc::runTests(vm, arg, result);
		}
		return -1;
	}

	j9tty_printf(PORTLIB,"OSCacheTests started\n");

	/* Run the sysv tests */
	rc |= SH_OSCacheTestSysv::runTests(vm, arg, cmdline);

#if !defined(J9ZOS390)
	/* Run the mmap tests */
	rc |= SH_OSCacheTestMmap::runTests(vm, arg, cmdline);
#endif

	if (rc == PASS) {
		j9tty_printf(PORTLIB, "OSCacheTest: PASSED\n");
	} else {
		j9tty_printf(PORTLIB, "OSCacheTest: FAILURE(S) DETECTED - rc = %zi\n", rc);
	}
	return rc;
}

IDATA
testOSCacheMisc(J9JavaVM *vm, struct j9cmdlineOptions *arg, const char *cmdline)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	IDATA rc = PASS;
	j9tty_printf(PORTLIB,"testOSCacheMisc started\n");
	/* Run the misc tests */
	rc |= SH_OSCacheTestMisc::runTests(vm, arg, cmdline);
	if (rc == PASS) {
		j9tty_printf(PORTLIB, "testOSCacheMisc: PASSED\n");
	} else {
		j9tty_printf(PORTLIB, "testOSCacheMisc: FAILURE(S) DETECTED - rc = %zi\n", rc);
	}
	return rc;
}

} /* extern "C" */
