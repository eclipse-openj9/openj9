package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;
import com.ibm.oti.shared.*;
import java.util.*;

public class TestSharedCacheJavaAPI extends TestUtils {
	private final static int INVALID_CACHE_TYPE = 3;
	/* Define SOFT_MAX_BYTES_VALUE to be equal to 16m. Caches used in this test are created with soft max size = 16m, 
	 * see cmd.runSimpleJavaProgramWithPersistentCache/cmd.runSimpleJavaProgramWithNonPersistentCache in configs/config.defaults.
	 */
	private final static int SOFT_MAX_BYTES_VALUE = 16 * 1024 * 1024;
	private final static int TEMP_JAVA10_JVMLEVEL = 6;
	private static int persistentCount, nonpersistentCount, snapshotCount;
	private static ArrayList<String> persistentList, nonpersistentList, snapshotList;
		
	static {
		if (isMVS() == false) {
			persistentList = new ArrayList<String>();
			persistentList.add("cache1");
			if (isWindows() == false) {
				persistentList.add("cache1_groupaccess");
			}
		}
    	if (realtimeTestsSelected() == false) {
			nonpersistentList = new ArrayList<String>();
			nonpersistentList.add("cache2");
			if (isWindows() == false) {
				nonpersistentList.add("cache2_groupaccess");
				snapshotList = new ArrayList<String>();
				snapshotList.add("cache2");
			}
    	}
	}
    public static void main(String args[]) {
    	String dir = null;
    	int oldCacheCount = 0;
    	int newCacheCount = 0;
    	String addrMode, jvmLevel;
    	List<SharedClassCacheInfo> cacheList;
    	int compressedRefMode = 0;
    	
    	if (TestUtils.isCompressedRefEnabled()) {
    		compressedRefMode = 1;
    	}
    	runDestroyAllCaches();
    	if (false == isWindows()) {
    		runDestroyAllSnapshots();
    	}
    	addrMode = System.getProperty("com.ibm.vm.bitmode");
		jvmLevel = System.getProperty("java.specification.version");
		
        try {
        	dir = getCacheDir();
	    	if (dir == null) {
	    		dir = getControlDir();
	    	}
	    	
	    	/* get cache count before creating any new cache */
	    	cacheList = SharedClassUtilities.getSharedCacheInfo(dir, SharedClassUtilities.NO_FLAGS, false);
	    	if (cacheList == null) {
	    		oldCacheCount = 0;
	    	} else {
	    		oldCacheCount = cacheList.size();
	    	}
		    if (oldCacheCount == -1) {
		    	fail("iterateSharedCacheFunction failed");
		    }
		    persistentCount = nonpersistentCount = snapshotCount = 0;
	       	if (isMVS() == false) {
	       		for(String cacheName: persistentList) {
	       			if (cacheName.indexOf("groupaccess") != -1) {
	       				runSimpleJavaProgramWithPersistentCache(cacheName, "groupAccess");
	       			} else {
	       				runSimpleJavaProgramWithPersistentCache(cacheName, null);
	       			}
	       			checkFileExistsForPersistentCache(cacheName);
	       		}
	       		persistentCount = persistentList.size();
	    	}
	       	if (realtimeTestsSelected() == false) {
		       	for(String cacheName: nonpersistentList) {
	       			if (cacheName.indexOf("groupaccess") != -1) {
	       				runSimpleJavaProgramWithNonPersistentCache(cacheName, "groupAccess");
	       			} else {
	       				runSimpleJavaProgramWithNonPersistentCache(cacheName, null);
	       			}
	       			checkFileExistsForNonPersistentCache(cacheName);
		    	}
			    nonpersistentCount = nonpersistentList.size();
			    
			    if ((false == isWindows()) 
			    	&& (nonpersistentCount > 0)
			    ) {
			    	for(String cacheName: snapshotList) {	
		       			checkFileExistsForNonPersistentCache(cacheName);
		       			createCacheSnapshot(cacheName);
		       			checkFileExistsForCacheSnapshot(cacheName);
			       	}
			       	snapshotCount = snapshotList.size();
			    }
	       	}
	       	
		    cacheList = SharedClassUtilities.getSharedCacheInfo(dir, SharedClassUtilities.NO_FLAGS, false);
		    if (cacheList == null) {
		    	fail("SharedClassUtilities.getSharedCacheInfo failed: no cache found");
		    }
		    newCacheCount = cacheList.size();
		    if ((newCacheCount == -1) || (newCacheCount != (oldCacheCount + persistentCount + nonpersistentCount + snapshotCount))) {
		    	fail("SharedClassUtilities.getSharedCacheInfo failed: Invalid number of cache found\t" +
		    			"expected: " + (oldCacheCount + persistentCount + nonpersistentCount + snapshotCount) + "\tfound: " + newCacheCount);
		    }
		    if (isMVS() == false) {
			    for(String cacheName: persistentList) {
			    	for(SharedClassCacheInfo cacheInfo: cacheList) {
			    		if (cacheInfo.getCacheName().equals(cacheName)) {
			    			if ((cacheInfo.getCacheSize() <= 0) ||
			    				(cacheInfo.getCacheSoftMaxBytes() != SOFT_MAX_BYTES_VALUE) ||
			    				(cacheInfo.getCacheFreeBytes() <= 0) ||
			    				(cacheInfo.isCacheCompatible() == false) ||
			    				(cacheInfo.isCacheCorrupt() == true) ||
			    				(cacheInfo.getCacheType() != SharedClassUtilities.PERSISTENT) ||
			    				(cacheInfo.getOSshmid() != -1) ||
			    				(cacheInfo.getOSsemid() != -1) ||
			    				(cacheInfo.getLastDetach() == null) ||
			    				(cacheInfo.getCacheCompressedRefsMode() != compressedRefMode) ||
			    				(cacheInfo.getCacheLayer() != 0) ||
			    				(System.currentTimeMillis() - cacheInfo.getLastDetach().getTime() < 0) ||			/* if lastDetach is in future */
			    				(System.currentTimeMillis() - cacheInfo.getLastDetach().getTime() > (65*60*1000))	/* difference should not be more than 1 hr 5 mins */
			    			) {
			    				fail("SharedClassUtilities.getSharedCacheInfo failed for persistent cache: Cache information is not proper");
	  		    			}	
			    			if ((addrMode.equals("32") && (cacheInfo.getCacheAddressMode() != SharedClassCacheInfo.ADDRESS_MODE_32)) ||
			    				(addrMode.equals("64") && (cacheInfo.getCacheAddressMode() != SharedClassCacheInfo.ADDRESS_MODE_64))
			    			) {
			    				fail("SharedClassUtilities.getSharedCacheInfo failed for persistent cache: Address mode is not proper");
			    			}
			    			if ((jvmLevel.equals("1.6") && (cacheInfo.getCacheJVMLevel() != SharedClassCacheInfo.JVMLEVEL_JAVA6)) ||
			    				(jvmLevel.equals("1.7") && (cacheInfo.getCacheJVMLevel() != SharedClassCacheInfo.JVMLEVEL_JAVA7)) ||
			    				(jvmLevel.equals("1.8") && (cacheInfo.getCacheJVMLevel() != SharedClassCacheInfo.JVMLEVEL_JAVA8)) ||
			    				((Double.parseDouble(jvmLevel) >= 10) && (cacheInfo.getCacheJVMLevel() != Integer.parseInt(jvmLevel)))
			    			) {
			    				fail("SharedClassUtilities.getSharedCacheInfo failed for persistent cache: JVM level is not proper");
			    			}
			    		}
			    	}
			    }
		    }
		    
		    if (realtimeTestsSelected() == false) {
		    	for(String cacheName: nonpersistentList) {
			    	for(SharedClassCacheInfo cacheInfo: cacheList) {
			    		if ((cacheInfo.getCacheName().equals(cacheName))
			    			&& (cacheInfo.getCacheType() == SharedClassUtilities.NONPERSISTENT)
			    		) {
			    			if ((cacheInfo.getCacheSize() <= 0) ||
			    				(cacheInfo.getCacheFreeBytes() <= 0) ||
			    				(cacheInfo.isCacheCompatible() == false) ||
			    				(cacheInfo.isCacheCorrupt() == true) ||
			    				(cacheInfo.getLastDetach() == null) ||
			    				(cacheInfo.getCacheSoftMaxBytes() != SOFT_MAX_BYTES_VALUE) ||
			    				(cacheInfo.getCacheCompressedRefsMode() != compressedRefMode) ||
			    				(cacheInfo.getCacheLayer() != 0) ||
			    				(System.currentTimeMillis() - cacheInfo.getLastDetach().getTime() < 0) ||			/* if lastDetach is in future */
			    				(System.currentTimeMillis() - cacheInfo.getLastDetach().getTime() > (65*60*1000))	/* difference should not be more than 1 hr 5 mins */
			    			) {
			    				fail("SharedClassUtilities.getSharedCacheInfo failed for non-persistent cache: Cache information is not proper");
			    			}	
			    			if ((addrMode.equals("32") && (cacheInfo.getCacheAddressMode() != SharedClassCacheInfo.ADDRESS_MODE_32)) ||
			    				(addrMode.equals("64") && (cacheInfo.getCacheAddressMode() != SharedClassCacheInfo.ADDRESS_MODE_64))
			    			) {
			    				fail("SharedClassUtilities.getSharedCacheInfo failed for non-persistent cache: Address mode is not proper");
			    			}
			    			if ((jvmLevel.equals("1.6") && (cacheInfo.getCacheJVMLevel() != SharedClassCacheInfo.JVMLEVEL_JAVA6)) ||
			    				(jvmLevel.equals("1.7") && (cacheInfo.getCacheJVMLevel() != SharedClassCacheInfo.JVMLEVEL_JAVA7)) ||
			    				(jvmLevel.equals("1.8") && (cacheInfo.getCacheJVMLevel() != SharedClassCacheInfo.JVMLEVEL_JAVA8)) ||
			    				((Double.parseDouble(jvmLevel) >= 10) && (cacheInfo.getCacheJVMLevel() != Integer.parseInt(jvmLevel)))
			    			) {
			    				fail("SharedClassUtilities.getSharedCacheInfo failed for non-persistent cache: JVM level is not proper");
			    			}
	
		    				if ((isWindows()) && ((cacheInfo.getOSshmid() > 0) || (cacheInfo.getOSsemid() > 0))) {
		    					fail("SharedClassUtilities.getSharedCacheInfo failed: Cache information is not proper\t" +
		    							"os_shmid : " + cacheInfo.getOSshmid() + " os_semid : " + cacheInfo.getOSsemid());
		    				} 
		    				if ((isWindows() == false) && ((cacheInfo.getOSshmid() <= 0) || (cacheInfo.getOSsemid() <= 0))) {
		    					fail("SharedClassUtilities.getSharedCacheInfo failed: Cache information is not proper\t" + 
		    							"os_shmid : " + cacheInfo.getOSshmid() + " os_semid : " + cacheInfo.getOSsemid());
		    				}
			    		}
			    	}
		    	}
		    	
		    	if (false == isWindows()) {
			    	for(String cacheName: snapshotList) {
				    	for(SharedClassCacheInfo cacheInfo: cacheList) {
				    		if ((cacheInfo.getCacheName().equals(cacheName))
				    			&& (cacheInfo.getCacheType() == SharedClassUtilities.SNAPSHOT)
				    		) {
				    			if ((-1 != cacheInfo.getCacheSize()) ||
				    				(-1 != cacheInfo.getCacheFreeBytes()) ||
				    				(-1 != cacheInfo.getCacheSoftMaxBytes()) ||
				    				(false == cacheInfo.isCacheCompatible()) ||
				    				(true == cacheInfo.isCacheCorrupt()) ||
				    				(null != cacheInfo.getLastDetach()) ||
				    				(cacheInfo.getCacheCompressedRefsMode() != compressedRefMode) ||
				    				(cacheInfo.getCacheLayer() != 0) ||
				    				(-1 != cacheInfo.getOSshmid()) ||
				    				(-1 != cacheInfo.getOSsemid())
				    			) {
				    				fail("SharedClassUtilities.getSharedCacheInfo failed for cache snapshot: Cache information is not proper");
				    			}	
				    			if ((addrMode.equals("32") && (cacheInfo.getCacheAddressMode() != SharedClassCacheInfo.ADDRESS_MODE_32)) ||
				    				(addrMode.equals("64") && (cacheInfo.getCacheAddressMode() != SharedClassCacheInfo.ADDRESS_MODE_64))
				    			) {
				    				fail("SharedClassUtilities.getSharedCacheInfo failed for non-persistent cache: Address mode is not proper");
				    			}
				    			if ((jvmLevel.equals("1.8") && (cacheInfo.getCacheJVMLevel() != SharedClassCacheInfo.JVMLEVEL_JAVA8)) ||
				    				((Double.parseDouble(jvmLevel) >= 10) && (cacheInfo.getCacheJVMLevel() != Integer.parseInt(jvmLevel)))
				    			) {
				    				fail("SharedClassUtilities.getSharedCacheInfo failed for cache snapshot: JVM level is not proper");
				    			}
				    		}
				    	}
			    	}
		    	}
		    	
		    }
			    
		    if (isMVS() == false) {
		    	for(String cacheName: persistentList) {
		    		int ret;
		    		try {
			        	SharedClassUtilities.destroySharedCache(dir, INVALID_CACHE_TYPE, cacheName, false);
			        	fail("SharedClassUtilities.destroySharedCache (persistent)failed: should have thrown IllegalArgumentException");
		    		} catch (IllegalArgumentException e) {
		    			/* expected to reach here */
		    		}
			    	checkFileExistsForPersistentCache(cacheName);
			    	ret = SharedClassUtilities.destroySharedCache(dir, SharedClassUtilities.NONPERSISTENT, cacheName, false);
			    	if ((-1 != ret) && (SharedClassUtilities.DESTROYED_ALL_CACHE != ret)) {
			    		fail("SharedClassUtilities.destroySharedCache failed: trying to destroy persistent cache as a non-persistent cache");
			    	}
			    	checkFileExistsForPersistentCache(cacheName);
			    	ret = SharedClassUtilities.destroySharedCache(dir, SharedClassUtilities.SNAPSHOT, cacheName, false);
			    	if ((-1 != ret) && (SharedClassUtilities.DESTROYED_ALL_CACHE != ret)) {
			    		fail("SharedClassUtilities.destroySharedCache failed: trying to destroy persistent cache as a cache snapshot");
			    	}
			    	checkFileExistsForPersistentCache(cacheName);
			    	ret = SharedClassUtilities.destroySharedCache(dir, SharedClassUtilities.PERSISTENT, cacheName, false);
			    	if ((-1 == ret) || (SharedClassUtilities.DESTROYED_ALL_CACHE != ret)) {
			    		fail("SharedClassUtilities.destroySharedCache failed (persistent)");
			    	}
			    	checkFileDoesNotExistForPersistentCache(cacheName);
		    	}
		    }
		    if (realtimeTestsSelected() == false) {
			    for(String cacheName: nonpersistentList) {
			    	int ret;
			    	try {
			    		SharedClassUtilities.destroySharedCache(dir, INVALID_CACHE_TYPE, cacheName, false);
			    		fail("SharedClassUtilities.destroySharedCache (non-persistent) failed: should have thrown IllegalArgumentException");
			      	} catch (IllegalArgumentException e) {
			      		/* expected to reach here */
			      	}
			    	checkFileExistsForNonPersistentCache(cacheName);
			    	ret = SharedClassUtilities.destroySharedCache(dir, SharedClassUtilities.PERSISTENT, cacheName, false);
			    	if ((-1 != ret) && (SharedClassUtilities.DESTROYED_ALL_CACHE != ret)) {
			    		fail("SharedClassUtilities.destroySharedCache failed: trying to destroy non-persistent cache as a persistent cache");
			    	}
			    	checkFileExistsForNonPersistentCache(cacheName);
			    	ret = SharedClassUtilities.destroySharedCache(dir, SharedClassUtilities.NONPERSISTENT, cacheName, false);
			    	if ((-1 == ret) || (SharedClassUtilities.DESTROYED_ALL_CACHE != ret)) {
			    		fail("SharedClassUtilities.destroySharedCache failed (non-persistent)");
			    	}
				    checkFileDoesNotExistForNonPersistentCache(cacheName);
				    
				    if (false == isWindows() && snapshotList.contains(cacheName)) {
				    	checkFileExistsForCacheSnapshot(cacheName);
				    	ret = SharedClassUtilities.destroySharedCache(dir, SharedClassUtilities.SNAPSHOT, cacheName, false);
				    	if ((-1 == ret) || (SharedClassUtilities.DESTROYED_ALL_CACHE != ret)) {
				    		fail("SharedClassUtilities.destroySharedCache failed (snapshot)");
				    	}
				    	checkFileDoesNotExistForCacheSnapshot(cacheName);
				    }
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
