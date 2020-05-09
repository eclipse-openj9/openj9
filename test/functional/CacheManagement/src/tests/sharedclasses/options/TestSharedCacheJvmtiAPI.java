/*******************************************************************************
 * Copyright (c) 2010, 2020 IBM Corp. and others
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

public class TestSharedCacheJvmtiAPI extends TestUtils {
	private final static int NO_FLAGS = 0;
	private final static int PERSISTENT = 1;
	private final static int NONPERSISTENT = 2;
	private final static int SNAPSHOT = 5;
	private final static int INVALID_CACHE_TYPE = 3;
	private static int cacheCount;
	private static int snapshotCount = 0;
		
    public native static int iterateSharedCache(String cacheDir, int flags, boolean useCommandLineValues);
    public native static boolean destroySharedCache(String cacheDir, String cacheName, int cacheType, boolean useCommandLineValues);

    public static void main(String args[]) {
    	boolean ret = true;
    	String dir = null;
    	String dirGroupAccess = null;
    	String dirRemoveJavaSharedResources = null;
    	int oldCacheCount = 0;
    	int oldCacheGroupAccessCount = 0;
    	int newCacheCount = 0;
    	int newCacheGroupAccessCount = 0;
    	
        runDestroyAllCaches();
        if (false == isWindows()) {
        	runDestroyAllSnapshots();
        	if (isOpenJ9()) {
        		runDestroyAllGroupAccessCaches();
        	}
        }
        try {
	    	dir = getCacheDir();
	    	if (dir == null) {
	    		dir = getControlDir();
	    	}
	    	/* get cache count before creating any new cache */
	    	oldCacheCount = iterateSharedCache(dir, NO_FLAGS, false);
		    if (oldCacheCount == -1) {
		    	fail("iterateSharedCacheFunction failed with dir " + dir);
		    }
	    	
	    	if (dir == null && false == isWindows() && isOpenJ9()) {
	    		dirGroupAccess = getCacheDir("Foo_groupaccess", false);
	    		dirRemoveJavaSharedResources = removeJavaSharedResourcesDir(dirGroupAccess);
	    		oldCacheGroupAccessCount = iterateSharedCache(dirGroupAccess, NO_FLAGS, false) + iterateSharedCache(dirRemoveJavaSharedResources, NO_FLAGS, false);
			    if (oldCacheGroupAccessCount == -1) {
			    	fail("iterateSharedCacheFunction failed with dir " + dirGroupAccess);
			    }
	    	}
	    	cacheCount = 0;
	    	if (isMVS() == false) {
	    		createPersistentCache("cache1");
	    		checkFileExistsForPersistentCache("cache1");
	    		cacheCount++;
	    		if (isWindows() == false) {
			    	runSimpleJavaProgramWithPersistentCache("cache1_groupaccess", "groupAccess");
					checkFileExistsForPersistentCache("cache1_groupaccess");
					cacheCount++;
				}
	    	}
	    	snapshotCount = 0;
	    	if (realtimeTestsSelected() == false) {
			    createNonPersistentCache("cache2");	  
			    checkFileExistsForNonPersistentCache("cache2");
			    cacheCount++;
				if (isWindows() == false) {
					runSimpleJavaProgramWithNonPersistentCache("cache2_groupaccess", "groupAccess");
					checkFileExistsForNonPersistentCache("cache2_groupaccess");
					cacheCount++;
					
					createCacheSnapshot("cache2");
					checkFileExistsForCacheSnapshot("cache2");
					snapshotCount++;
				}
	    	}
	    	newCacheCount = iterateSharedCache(dir, NO_FLAGS, false);
		    if ( dirGroupAccess == null ) {
			    if ((newCacheCount == -1) || (newCacheCount != oldCacheCount + cacheCount + snapshotCount)) {
			    	fail("iterateSharedCacheFunction failed, Invalid number of cache found\t" +
			    			"expected: " + (oldCacheCount + cacheCount + snapshotCount) + "\tfound: " + newCacheCount);
			    }
		    } else {
		    	newCacheGroupAccessCount = iterateSharedCache(dirGroupAccess, NO_FLAGS, false);
		    	newCacheGroupAccessCount += iterateSharedCache(dirRemoveJavaSharedResources, NO_FLAGS, false);
			    if ((newCacheCount == -1) || (newCacheGroupAccessCount == -1) ||
			    		((newCacheCount + newCacheGroupAccessCount) != (oldCacheCount +oldCacheGroupAccessCount + cacheCount + snapshotCount))) {
			    	fail("iterateSharedCacheFunction ffailed, Invalid number of cache found\t" +
			    			"expected: " + (oldCacheCount +oldCacheGroupAccessCount + cacheCount + snapshotCount) + "\tfound: " + (newCacheCount + newCacheGroupAccessCount));
			    }
		    }
		    
		    if (isMVS() == false) {
		    	destroySharedCache(dir, "cache1", INVALID_CACHE_TYPE, false);
		    	checkFileExistsForPersistentCache("cache1");
		    	destroySharedCache(dir, "cache1", NONPERSISTENT, false);
		    	checkFileExistsForPersistentCache("cache1");
		    	destroySharedCache(dir, "cache1", SNAPSHOT, false);
		    	checkFileExistsForPersistentCache("cache1");
		    	destroySharedCache(dir, "cache1", PERSISTENT, false);
		    	checkFileDoesNotExistForPersistentCache("cache1");
				if (isWindows() == false) {
					if (dirGroupAccess == null) {
						destroySharedCache(dir, "cache1_groupaccess", PERSISTENT, false);
					} else {
						destroySharedCache(dirGroupAccess, "cache1_groupaccess", PERSISTENT, false);
					}	
			    	checkFileDoesNotExistForPersistentCache("cache1_groupaccess");
				}
		    }
		    if (realtimeTestsSelected() == false) {
		    	destroySharedCache(dir, "cache2", INVALID_CACHE_TYPE, false);
			    checkFileExistsForNonPersistentCache("cache2");
		    	destroySharedCache(dir, "cache2", PERSISTENT, false);
		    	checkFileExistsForNonPersistentCache("cache2");
		    	destroySharedCache(dir, "cache2", NONPERSISTENT, false);
			    checkFileDoesNotExistForNonPersistentCache("cache2");
			    if (isWindows() == false) {
					if (dirGroupAccess == null) {
						destroySharedCache(dir, "cache2_groupaccess", NONPERSISTENT, false);
					} else {
						destroySharedCache(dirRemoveJavaSharedResources, "cache2_groupaccess", NONPERSISTENT, false);
					}
			    	checkFileDoesNotExistForNonPersistentCache("cache2_groupaccess");
			    	
			    	checkFileExistsForCacheSnapshot("cache2");
			    	destroySharedCache(dir, "cache2", SNAPSHOT, false);
			    	checkFileDoesNotExistForCacheSnapshot("cache2");
				}
		    }

		} finally {
			runDestroyAllCaches();
			if (false == isWindows()) {
				runDestroyAllSnapshots();
				if (isOpenJ9()) {
					runDestroyAllGroupAccessCaches();
				}
			}
		}
	}
}
