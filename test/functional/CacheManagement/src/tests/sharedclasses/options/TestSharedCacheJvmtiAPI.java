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
    	int oldCacheCount = 0;
    	int newCacheCount = 0;
    	
        runDestroyAllCaches();
        if (false == isWindows()) {
        	runDestroyAllSnapshots();
        }
        try {
	    	dir = getCacheDir();
	    	if (dir == null) {
	    	}
	    	
	    	/* get cache count before creating any new cache */
	    	oldCacheCount = iterateSharedCache(dir, NO_FLAGS, false);
		    if (oldCacheCount == -1) {
		    	fail("iterateSharedCacheFunction failed");
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
		    if ((newCacheCount == -1) || (newCacheCount != oldCacheCount + cacheCount + snapshotCount)) {
		    	fail("iterateSharedCacheFunction failed");
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
			    	destroySharedCache(dir, "cache1_groupaccess", PERSISTENT, false);
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
			    	destroySharedCache(dir, "cache2_groupaccess", NONPERSISTENT, false);
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
			}
		}
	}
}
