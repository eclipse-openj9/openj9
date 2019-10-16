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

	private static int persistentCount, nonpersistentCount, snapshotCount, persistentGroupAccessCount, nonpersistentGroupAccessCount;
	private static ArrayList<String> persistentList, nonpersistentList, snapshotList, persistentGroupAccessList, nonpersistentGroupAccessList;
		
	static {
		if (isMVS() == false) {
			persistentList = new ArrayList<String>();
			persistentList.add("cache1");
			if (isWindows() == false) {
				if (isOpenJ9()) {
					persistentGroupAccessList = new ArrayList<String>();
					persistentGroupAccessList.add("cache1_groupaccess");
				} else {
					persistentList.add("cache1_groupaccess");
				}
			}
		}
    	if (realtimeTestsSelected() == false) {
			nonpersistentList = new ArrayList<String>();
			nonpersistentList.add("cache2");
			if (isWindows() == false) {
				if (isOpenJ9()) {
					nonpersistentGroupAccessList = new ArrayList<String>();
					nonpersistentGroupAccessList.add("cache2_groupaccess");
				} else {
					nonpersistentList.add("cache2_groupaccess");
				}
				snapshotList = new ArrayList<String>();
				snapshotList.add("cache2");
			}
    	}
	}
    public static void main(String args[]) {
    	String dir = null;
    	String dirGroupAccess = null;
    	String dirRemoveJavaSharedResources = null;
    	int oldCacheCount = 0;
    	int oldCacheGroupAccessCount = 0;
    	int oldCacheGroupAccessNonpersistentCount = 0;
    	int newCacheCount = 0;
    	int newCacheGroupAccessCount = 0;
    	String addrMode, jvmLevel;
    	List<SharedClassCacheInfo> cacheList;
    	List<SharedClassCacheInfo> cacheGroupAccessList = null;
    	List<SharedClassCacheInfo> cacheGroupAccessNonPersistentList = null;
    	int compressedRefMode = 0;

    	if (TestUtils.isCompressedRefEnabled()) {
    		compressedRefMode = 1;
    	}
    	runDestroyAllCaches();
    	if (false == isWindows()) {
    		runDestroyAllSnapshots();
        	if (isOpenJ9()) {
        		runDestroyAllGroupAccessCaches();
            }
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
	    	
	    	if (dir == null && false == isWindows() && false == isMVS() && isOpenJ9()) {
	    		dirGroupAccess = getCacheDir("Foo_groupaccess", false);
	    		dirRemoveJavaSharedResources = removeJavaSharedResourcesDir(dirGroupAccess);
	    		cacheGroupAccessList = SharedClassUtilities.getSharedCacheInfo(dirGroupAccess, SharedClassUtilities.NO_FLAGS, false);
		    	if (cacheGroupAccessList == null) {
		    		oldCacheGroupAccessCount = 0;
		    	} else {
		    		oldCacheGroupAccessCount = cacheGroupAccessList.size();
		    	}
		    	cacheGroupAccessNonPersistentList = SharedClassUtilities.getSharedCacheInfo(dirRemoveJavaSharedResources, SharedClassUtilities.NO_FLAGS, false);
		    	if (cacheGroupAccessNonPersistentList == null) {
		    		oldCacheGroupAccessNonpersistentCount = 0;
		    	} else {
		    		oldCacheGroupAccessNonpersistentCount = cacheGroupAccessNonPersistentList.size();
		    	}
			    oldCacheCount = oldCacheCount + oldCacheGroupAccessCount + oldCacheGroupAccessNonpersistentCount;
	    	}

		    persistentCount = nonpersistentCount = snapshotCount = persistentGroupAccessCount = nonpersistentGroupAccessCount= 0;
	       	if (isMVS() == false) {
	       		if (dirGroupAccess != null) {
		       		for(String cacheName: persistentGroupAccessList) {
		       			runSimpleJavaProgramWithPersistentCache(cacheName, "groupAccess");
		       			checkFileExistsForPersistentCache(cacheName);
		       		}
		       		persistentGroupAccessCount = persistentGroupAccessList.size();
	       		}
	       		
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
	       		if (dirGroupAccess != null) {
		       		for(String cacheName: nonpersistentGroupAccessList) {
		       			runSimpleJavaProgramWithNonPersistentCache(cacheName, "groupAccess");
		       			checkFileExistsForNonPersistentCache(cacheName);
		       		}
		       		nonpersistentGroupAccessCount = nonpersistentGroupAccessList.size();
	       		}
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
		    	fail("SharedClassUtilities.getSharedCacheInfo failed: no cache found with dir is " + dir);
		    }
		    newCacheCount = cacheList.size();
		    
		    if (dirGroupAccess != null) {
		    	int tempGroupAccessCount = 0;
		    	cacheGroupAccessList = SharedClassUtilities.getSharedCacheInfo(dirGroupAccess, SharedClassUtilities.NO_FLAGS, false);
		    	if (cacheGroupAccessList == null) {
		    		fail("SharedClassUtilities.getSharedCacheInfo failed: no cache found with dir is " + dirGroupAccess);
		    	}
		    	tempGroupAccessCount = cacheGroupAccessList.size();
		    	if (tempGroupAccessCount == -1) {
		    		fail("SharedClassUtilities.getSharedCacheInfo failed: Invalid number of cache found with dir is " + dirGroupAccess);
		    	}
		    	cacheGroupAccessNonPersistentList = SharedClassUtilities.getSharedCacheInfo(dirRemoveJavaSharedResources, SharedClassUtilities.NO_FLAGS, false);
		    	if (cacheGroupAccessNonPersistentList == null) {
			    	fail("SharedClassUtilities.getSharedCacheInfo failed: no cache found with dir is " + dirRemoveJavaSharedResources);
			    }
		    	if (tempGroupAccessCount == -1) {
		    		fail("SharedClassUtilities.getSharedCacheInfo failed: Invalid number of cache found with dir is " + dirRemoveJavaSharedResources);
		    	}
		    	newCacheCount += cacheGroupAccessList.size() + cacheGroupAccessNonPersistentList.size();
		    	if ((newCacheCount == -1) || (newCacheCount != 
		    			(oldCacheCount + persistentGroupAccessCount + nonpersistentGroupAccessCount +
		    					persistentCount + nonpersistentCount + snapshotCount))) {
			    	fail("SharedClassUtilities.getSharedCacheInfo failed: Invalid number of cache found\t" +
			    		"expected: " + (oldCacheCount + persistentCount + nonpersistentCount + snapshotCount) + "\tfound: " + newCacheCount);
			    }
		    } else {
			    if ((newCacheCount == -1) || (newCacheCount != (oldCacheCount + persistentCount + nonpersistentCount + snapshotCount))) {
			    	fail("SharedClassUtilities.getSharedCacheInfo failed: Invalid number of cache found\t" +
			    			"expected: " + (oldCacheCount + persistentCount + nonpersistentCount + snapshotCount) + "\tfound: " + newCacheCount);
			    }
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
			    				(System.currentTimeMillis() - cacheInfo.getLastDetach().getTime() < 0) ||			/* if lastDetach is in future */
			    				(System.currentTimeMillis() - cacheInfo.getLastDetach().getTime() > (65*60*1000))	/* difference should not be more than 1 hr 5 mins */
			    			) {
			    				fail("SharedClassUtilities.getSharedCacheInfo failed for persistent cache: Cache information is not proper" + 
			    						"\n cacheName is " + cacheName + 
			    						"\n cacheInfo.getCacheSize() is " +  cacheInfo.getCacheSize() + 
			    						"\n cacheInfo.getCacheSoftMaxBytes() is " + cacheInfo.getCacheSoftMaxBytes() +
			    						"\n cacheInfo.getCacheFreeBytes() is " + cacheInfo.getCacheFreeBytes() +
			    						"\n cacheInfo.isCacheCompatible() is " + cacheInfo.isCacheCompatible() +
			    						"\n cacheInfo.isCacheCorrupt() is " + cacheInfo.isCacheCorrupt() +
			    						"\n cacheInfo.getOSshmid() is " + cacheInfo.getOSshmid() + 
			    						"\n cacheInfo.getOSsemid() is " + cacheInfo.getLastDetach() + 
			    						"\n cacheInfo.getCacheCompressedRefsMode() is " + cacheInfo.getCacheCompressedRefsMode() +
			    						"\n cacheInfo.getCacheLayer() is " + cacheInfo.getCacheLayer() +
			    						"\n System.currentTimeMillis() - cacheInfo.getLastDetach().getTime() is " + ( System.currentTimeMillis() - cacheInfo.getLastDetach().getTime())) ;
	  		    			}	
			    			if ((addrMode.equals("32") && (cacheInfo.getCacheAddressMode() != SharedClassCacheInfo.ADDRESS_MODE_32)) ||
			    				(addrMode.equals("64") && (cacheInfo.getCacheAddressMode() != SharedClassCacheInfo.ADDRESS_MODE_64))
			    			) {
			    				fail("SharedClassUtilities.getSharedCacheInfo failed for persistent cache: Address mode is not proper " + 
			    						"\n cacheInfo.getCacheAddressMode() is " + cacheInfo.getCacheAddressMode());
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
			    if (dirGroupAccess != null){
			    	for(String cacheName: persistentGroupAccessList) {
				    	for(SharedClassCacheInfo cacheInfo: cacheGroupAccessList) {
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
				    				fail("SharedClassUtilities.getSharedCacheInfo failed for persistent groupAccess cache: Cache information is not proper" + 
				    						"\n cacheName is " + cacheName +
				    						"\n cacheInfo.getCacheSize() is " +  cacheInfo.getCacheSize() + 
				    						"\n cacheInfo.getCacheSoftMaxBytes() is " + cacheInfo.getCacheSoftMaxBytes() +
				    						"\n cacheInfo.getCacheFreeBytes() is " + cacheInfo.getCacheFreeBytes() +
				    						"\n cacheInfo.isCacheCompatible() is " + cacheInfo.isCacheCompatible() +
				    						"\n cacheInfo.isCacheCorrupt() is " + cacheInfo.isCacheCorrupt() +
				    						"\n cacheInfo.getOSshmid() is " + cacheInfo.getOSshmid() + 
				    						"\n cacheInfo.getOSsemid() is " + cacheInfo.getLastDetach() + 
				    						"\n cacheInfo.getCacheCompressedRefsMode() is " + cacheInfo.getCacheCompressedRefsMode() +
				    						"\n cacheInfo.getCacheLayer() is " + cacheInfo.getCacheLayer() +
				    						"\n System.currentTimeMillis() - cacheInfo.getLastDetach().getTime() is " + ( System.currentTimeMillis() - cacheInfo.getLastDetach().getTime())) ;
		  		    			}	
				    			if ((addrMode.equals("32") && (cacheInfo.getCacheAddressMode() != SharedClassCacheInfo.ADDRESS_MODE_32)) ||
				    				(addrMode.equals("64") && (cacheInfo.getCacheAddressMode() != SharedClassCacheInfo.ADDRESS_MODE_64))
				    			) {
				    				fail("SharedClassUtilities.getSharedCacheInfo failed for persistent groupAccess cache: Address mode is not proper");
				    			}
				    			if ((jvmLevel.equals("1.6") && (cacheInfo.getCacheJVMLevel() != SharedClassCacheInfo.JVMLEVEL_JAVA6)) ||
				    				(jvmLevel.equals("1.7") && (cacheInfo.getCacheJVMLevel() != SharedClassCacheInfo.JVMLEVEL_JAVA7)) ||
				    				(jvmLevel.equals("1.8") && (cacheInfo.getCacheJVMLevel() != SharedClassCacheInfo.JVMLEVEL_JAVA8)) ||
				    				((Double.parseDouble(jvmLevel) >= 10) && (cacheInfo.getCacheJVMLevel() != Integer.parseInt(jvmLevel)))
				    			) {
				    				fail("SharedClassUtilities.getSharedCacheInfo failed for persistent groupAccess cache: JVM level is not proper");
				    			}
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
			    				fail("SharedClassUtilities.getSharedCacheInfo failed for non-persistent cache: Cache information is not proper"+ 
			    						"\n cacheName is " + cacheName + 
			    						"\n cacheInfo.getCacheSize() is " +  cacheInfo.getCacheSize() + 
			    						"\n cacheInfo.getCacheSoftMaxBytes() is " + cacheInfo.getCacheSoftMaxBytes() +
			    						"\n cacheInfo.getCacheFreeBytes() is " + cacheInfo.getCacheFreeBytes() +
			    						"\n cacheInfo.isCacheCompatible() is " + cacheInfo.isCacheCompatible() +
			    						"\n cacheInfo.isCacheCorrupt() is " + cacheInfo.isCacheCorrupt() +
			    						"\n cacheInfo.getCacheCompressedRefsMode() is " + cacheInfo.getCacheCompressedRefsMode() +
			    						"\n cacheInfo.getCacheLayer() is " + cacheInfo.getCacheLayer() +
			    						"\n System.currentTimeMillis() - cacheInfo.getLastDetach().getTime() is " + ( System.currentTimeMillis() - cacheInfo.getLastDetach().getTime())) ;
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
		    	
		    	if ( dirGroupAccess != null ) {
			    	for(String cacheName: nonpersistentGroupAccessList) {
				    	for(SharedClassCacheInfo cacheInfo: cacheGroupAccessNonPersistentList) {
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
				    				fail("SharedClassUtilities.getSharedCacheInfo failed for non-persistent groupAccess cache: Cache information is not proper Cache information is not proper " + 
				    					"\n cacheName is " + cacheName + 
				    					"\n cacheInfo.getCacheSize() is " +  cacheInfo.getCacheSize() + 
			    						"\n cacheInfo.getCacheSoftMaxBytes() is " + cacheInfo.getCacheSoftMaxBytes() +
			    						"\n cacheInfo.getCacheFreeBytes() is " + cacheInfo.getCacheFreeBytes() +
			    						"\n cacheInfo.isCacheCompatible() is " + cacheInfo.isCacheCompatible() +
			    						"\n cacheInfo.isCacheCorrupt() is " + cacheInfo.isCacheCorrupt() +
			    						"\n cacheInfo.getCacheCompressedRefsMode() is " + cacheInfo.getCacheCompressedRefsMode() +
			    						"\n cacheInfo.getCacheLayer() is " + cacheInfo.getCacheLayer() +
			    						"\n System.currentTimeMillis() - cacheInfo.getLastDetach().getTime() is " + ( System.currentTimeMillis() - cacheInfo.getLastDetach().getTime())) ;
				    			}	
				    			if ((addrMode.equals("32") && (cacheInfo.getCacheAddressMode() != SharedClassCacheInfo.ADDRESS_MODE_32)) ||
				    				(addrMode.equals("64") && (cacheInfo.getCacheAddressMode() != SharedClassCacheInfo.ADDRESS_MODE_64))
				    			) {
				    				fail("SharedClassUtilities.getSharedCacheInfo failed for non-persistent groupAccess cache: Address mode is not proper");
				    			}
				    			if ((jvmLevel.equals("1.6") && (cacheInfo.getCacheJVMLevel() != SharedClassCacheInfo.JVMLEVEL_JAVA6)) ||
				    				(jvmLevel.equals("1.7") && (cacheInfo.getCacheJVMLevel() != SharedClassCacheInfo.JVMLEVEL_JAVA7)) ||
				    				(jvmLevel.equals("1.8") && (cacheInfo.getCacheJVMLevel() != SharedClassCacheInfo.JVMLEVEL_JAVA8)) ||
				    				((Double.parseDouble(jvmLevel) >= 10) && (cacheInfo.getCacheJVMLevel() != Integer.parseInt(jvmLevel)))
				    			) {
				    				fail("SharedClassUtilities.getSharedCacheInfo failed for non-persistent groupAccess cache: JVM level is not proper");
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
				    				fail("SharedClassUtilities.getSharedCacheInfo failed for cache snapshot: Cache information is not proper Cache information is not proper " + 
				    					"\n cacheName is " + cacheName + 
				    					"\n cacheInfo.getCacheSize() is " +  cacheInfo.getCacheSize() + 
			    						"\n cacheInfo.getCacheSoftMaxBytes() is " + cacheInfo.getCacheSoftMaxBytes() +
			    						"\n cacheInfo.getCacheFreeBytes() is " + cacheInfo.getCacheFreeBytes() +
			    						"\n cacheInfo.isCacheCompatible() is " + cacheInfo.isCacheCompatible() +
			    						"\n cacheInfo.isCacheCorrupt() is " + cacheInfo.isCacheCorrupt() +
			    						"\n cacheInfo.getCacheCompressedRefsMode() is " + cacheInfo.getCacheCompressedRefsMode() +
			    						"\n cacheInfo.getCacheLayer() is " + cacheInfo.getCacheLayer() +
			    						"\n System.currentTimeMillis() - cacheInfo.getLastDetach().getTime() is " + ( System.currentTimeMillis() - cacheInfo.getLastDetach().getTime())) ;
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
		    		System.out.println("javaapi destroy cache dir is " + dir);
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
			    		fail("SharedClassUtilities.destroySharedCache failed (persistent), Cache Name: " + cacheName);
			    	}
			    	checkFileDoesNotExistForPersistentCache(cacheName);
		    	}
		    	if (dirGroupAccess != null) {
			    	for(String cacheName: persistentGroupAccessList) {
			    		int ret;
			    		try {
				        	SharedClassUtilities.destroySharedCache(dirGroupAccess, INVALID_CACHE_TYPE, cacheName, false);
				        	fail("SharedClassUtilities.destroySharedCache (persistent)failed: should have thrown IllegalArgumentException");
			    		} catch (IllegalArgumentException e) {
			    			/* expected to reach here */
			    		}
				    	checkFileExistsForPersistentCache(cacheName);
				    	ret = SharedClassUtilities.destroySharedCache(dirGroupAccess, SharedClassUtilities.NONPERSISTENT, cacheName, false);
				    	if ((-1 != ret) && (SharedClassUtilities.DESTROYED_ALL_CACHE != ret)) {
				    		fail("SharedClassUtilities.destroySharedCache failed: trying to destroy persistent cache as a non-persistent cache");
				    	}
				    	checkFileExistsForPersistentCache(cacheName);
				    	ret = SharedClassUtilities.destroySharedCache(dirGroupAccess, SharedClassUtilities.SNAPSHOT, cacheName, false);
				    	if ((-1 != ret) && (SharedClassUtilities.DESTROYED_ALL_CACHE != ret)) {
				    		fail("SharedClassUtilities.destroySharedCache failed: trying to destroy persistent cache as a cache snapshot");
				    	}
				    	checkFileExistsForPersistentCache(cacheName);
				    	ret = SharedClassUtilities.destroySharedCache(dirGroupAccess, SharedClassUtilities.PERSISTENT, cacheName, false);
				    	if ((-1 == ret) || (SharedClassUtilities.DESTROYED_ALL_CACHE != ret)) {
				    		fail("SharedClassUtilities.destroySharedCache failed (persistent)");
				    	}
				    	checkFileDoesNotExistForPersistentCache(cacheName);
			    	}
		    	}
		    }
		    if (realtimeTestsSelected() == false) {
			    for(String cacheName: nonpersistentList) {
			    	int ret;
			    	System.out.println("javaapi destroy nonpersistent cache dir is " + dir);
		    		
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
			    		fail("SharedClassUtilities.destroySharedCache failed (non-persistent), Cache Name: " + cacheName);
			    	}
				    checkFileDoesNotExistForNonPersistentCache(cacheName);
				    
				    if (false == isWindows() && snapshotList.contains(cacheName)) {
				    	checkFileExistsForCacheSnapshot(cacheName);
				    	ret = SharedClassUtilities.destroySharedCache(dir, SharedClassUtilities.SNAPSHOT, cacheName, false);
				    	if ((-1 == ret) || (SharedClassUtilities.DESTROYED_ALL_CACHE != ret)) {
				    		fail("SharedClassUtilities.destroySharedCache failed (snapshot), Cache Name: " + cacheName);
				    	}
				    	checkFileDoesNotExistForCacheSnapshot(cacheName);
				    }
			    }
			    if (dirGroupAccess != null) {
				    for(String cacheName: nonpersistentGroupAccessList) {
				    	int ret;
				    	try {
				    		SharedClassUtilities.destroySharedCache(dirRemoveJavaSharedResources, INVALID_CACHE_TYPE, cacheName, false);
				    		fail("SharedClassUtilities.destroySharedCache (non-persistent) failed: should have thrown IllegalArgumentException");
				      	} catch (IllegalArgumentException e) {
				      		/* expected to reach here */
				      	}
				    	checkFileExistsForNonPersistentCache(cacheName);
				    	ret = SharedClassUtilities.destroySharedCache(dirRemoveJavaSharedResources, SharedClassUtilities.PERSISTENT, cacheName, false);
				    	if ((-1 != ret) && (SharedClassUtilities.DESTROYED_ALL_CACHE != ret)) {
				    		fail("SharedClassUtilities.destroySharedCache failed: trying to destroy non-persistent cache as a persistent cache");
				    	}
				    	checkFileExistsForNonPersistentCache(cacheName);
				    	ret = SharedClassUtilities.destroySharedCache(dirRemoveJavaSharedResources, SharedClassUtilities.NONPERSISTENT, cacheName, false);
				    	if ((-1 == ret) || (SharedClassUtilities.DESTROYED_ALL_CACHE != ret)) {
				    		fail("SharedClassUtilities.destroySharedCache failed (non-persistent)");
				    	}
					    checkFileDoesNotExistForNonPersistentCache(cacheName);
				    }
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
