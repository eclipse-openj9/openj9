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
 * Create a cache, corrput it by zero-ing out its pages, expire it using -Xshareclasses:expire=0 and -Xshareclasses:expire=n.
 * Create a cache, corrupt it by zero-ing out its pages, destroy it using -Xshareclasses:destroy.
 * Create a cache, corrupt it by zero-ing out its pages, destroy it using -Xshareclasses:destroyAll.
 */
public class TestExpireDestroyOnCorruptCache extends TestUtils {
	public static void main(String[] args) {
		if (isMVS()) {
			/* we don't yet have a mechanism to corrupt a non-persistent cache */
			return;
		}
		
		destroyPersistentCache("Foo");
		createPersistentCache("Foo");
		checkFileExistsForPersistentCache("Foo");
		corruptCache("Foo", true, "zeroFirstPage");
		try {
			Thread.sleep(1000);
		} catch (Exception e) {
		}
		expireAllCaches();
		/* expire does not delete caches detected as corrupt during oscache->startup() */
		checkFileExistsForPersistentCache("Foo");

		try {
			/* sleep for 2 mins */
			Thread.sleep(2000*60);
		} catch (Exception e) {
		}

		/* expire all caches not used for past 1 min */
		expireAllCachesWithTime(new Integer(1).toString());
		/* expire does not delete caches detected as corrupt during oscache->startup() */
		checkFileExistsForPersistentCache("Foo");
		
		destroyPersistentCache("Foo");
		checkForSuccessfulPersistentCacheDestroyMessage("Foo");
		checkNoFileForPersistentCache("Foo");
		
		createPersistentCache("Foo");
		checkFileExistsForPersistentCache("Foo");
		corruptCache("Foo", true, "zeroFirstPage");
		runDestroyAllCaches();
		checkForSuccessfulPersistentCacheDestroyMessage("Foo");
		checkNoFileForPersistentCache("Foo");
	}
}
	  