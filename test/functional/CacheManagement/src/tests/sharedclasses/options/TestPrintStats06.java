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
