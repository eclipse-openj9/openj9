/*******************************************************************************
 * Copyright (c) 2010, 2019 IBM Corp. and others
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

package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Create compatible non-persistent cache.
 * Remove semaphore, run printStats, verify that it prints cache stats. It also creates a new semaphore.
 * Using this cache again results in reporting cache corruption due to semaphore mismatch.
 * 
 * Remove shared memory, run printStats, verify that it reports cache does not exist.
 */
public class TestPrintStats06 extends TestUtils {
	public static void main(String[] args) {
		boolean rc;
		
		if (isWindows()) {
			return;
		}
		runDestroyAllCaches();
		
		createNonPersistentCache("Foo");
		checkFileExistsForNonPersistentCache("Foo");
	
		rc = removeSemaphoreForCache("Foo");
		if (rc == false) {
			fail("Failed to remove semaphore for cache Foo");
		}
		/* running printStats opens cache in read-only (since semaphore does not exist) and retrieves cache stats */
		runPrintStats("Foo", false);
		checkOutputContains("Current statistics for cache", "Did not find expected line of output 'Current statistics for cache'");
		
		/* using nonpersistent cache now should result in cache corruption due to semaphore mismatch */
		runSimpleJavaProgramWithNonPersistentCache("Foo", "disablecorruptcachedumps");
		checkOutputContains("JVMSHRC508E", "Did not find expected message about mismatch semaphore");
		checkOutputContains("JVMSHRC442E", "Did not find expected message about corrupt cache");
		
		rc = removeSharedMemoryForCache("Foo");
		if (rc == false) {
			fail("Failed to remove shared memory for cache Foo");
		}
		runPrintStats("Foo", false);
		/* JVMSHRC023E Cache does not exist */
		checkOutputContains("JVMSHRC023E", "Did not find expected line of output 'Cache does not exist'");
		/* destroy control files */
		destroyNonPersistentCache("Foo");
		runDestroyAllCaches();
	}
}
