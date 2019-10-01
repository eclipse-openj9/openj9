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
	  