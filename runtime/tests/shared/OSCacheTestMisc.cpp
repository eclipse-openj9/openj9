/*******************************************************************************
 * Copyright (c) 2009, 2018 IBM Corp. and others
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
#include "OSCacheTestMisc.hpp"

extern "C" {
#include "j9port.h"
}

#include "main.h"
#include "OSCache.hpp"
#include "OSCacheTest.hpp"
#include "ProcessHelper.h"
#include "j2sever.h"
#include <string.h>
#include "CompositeCacheImpl.hpp"

#define OSCACHETEST_GET_CACHE_DIR_NAME "testGetCacheDir"


IDATA
SH_OSCacheTestMisc::testGetCacheDir(J9JavaVM* vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	IDATA rc = FAIL;
	const char *testDirPaths[] = {
		"c:\\Documents and Settings\\LocalService\\Local Settings\\Application Data\\javasharedresources",
		"c:\\javasharedresources\\foo\\bar"
	};
	char cacheDir[J9SH_MAXPATH];

	J9SharedClassConfig* sharedConfig = (J9SharedClassConfig*)j9mem_allocate_memory(sizeof(J9SharedClassConfig) + sizeof(J9SharedClassCacheDescriptor), J9MEM_CATEGORY_CLASSES);
	if (sharedConfig == 0) {
		return 1;
	}

	j9shmem_shutdown();

	for (UDATA i = 0; i < sizeof(testDirPaths)/sizeof(testDirPaths[0]); i++) {
		j9shmem_startup();

		/* Important to clean the sharedConfig for each tests */
		memset(sharedConfig, 0, sizeof(J9SharedClassConfig));

		sharedConfig->ctrlDirName = testDirPaths[i];

		if (0 != SH_OSCache::getCacheDir(vm, sharedConfig->ctrlDirName, cacheDir, sizeof(cacheDir), J9PORT_SHR_CACHE_TYPE_PERSISTENT)) {
			j9tty_printf(PORTLIB, "testGetCacheDir: failed: SH_OSCache::getCacheDir() did not return 0\n");
			goto done;
		}

		/* Strip any trailing dir separators. */
		for (UDATA j = strlen(cacheDir) - 1; (j != (UDATA)-1) && (cacheDir[j] == DIR_SEPARATOR); j--) {
			cacheDir[j] = '\0';
		}

		if (strcmp(cacheDir, testDirPaths[i])) {
			j9tty_printf(PORTLIB, "testGetCacheDir: failed: '%s' did not match '%s'\n", cacheDir, testDirPaths[i]);
			goto done;
		}

		j9shmem_shutdown();
	}

	j9shmem_startup();

	rc = PASS;

done:
	j9tty_printf(PORTLIB, "testGetCacheDir: %s\n", PASS==rc?"PASS":"FAIL");
	return rc;
}

IDATA
SH_OSCacheTestMisc::runTests(J9JavaVM* vm, struct j9cmdlineOptions *arg, const char *cmdline)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	IDATA rc = PASS;

	/* Detect children */
	if (NULL != cmdline) {
		/* We are using strstr, so we need to put the longer string first */
		if (NULL != strstr(cmdline, OSCACHETEST_GET_CACHE_DIR_NAME)) {
			return testGetCacheDir(vm);
		}
	}

	j9tty_printf(PORTLIB, "OSCacheTestMisc begin\n");

	j9tty_printf(PORTLIB, "testGetCacheDir begin\n");
	rc |= testGetCacheDir(vm);

	return rc;
}

