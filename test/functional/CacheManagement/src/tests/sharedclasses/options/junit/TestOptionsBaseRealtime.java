package tests.sharedclasses.options.junit;

import tests.sharedclasses.options.*;

/**
 * Gather together all the tests into one testcase - see the subclasses for more info.
 */
public abstract class TestOptionsBaseRealtime extends TestOptionsNonpersistentRealtime {

	public void testCacheDir06() { TestCacheDir06.main(null);}

	public void testControlDirCacheDir01() { TestControlDirCacheDir01.main(null);}

	public void testCorruptedCaches03() { TestCorruptedCaches03.main(null);}

	public void testDestroyNonExistentCaches() { TestDestroyNonExistentCaches.main(null);}

	public void testListAllWhenNoCaches() { TestListAllWhenNoCaches.main(null);}
	
	public void testOptionNone() { TestOptionNone.main(null); }

	public void testPersistentCacheMoving01() { TestPersistentCacheMoving01.main(null);}

	public void testPersistentCacheMoving02() { TestPersistentCacheMoving02.main(null);}

	public void testPersistentCacheMoving03() { TestPersistentCacheMoving03.main(null);}

	public void testReset01() { TestReset01.main(null); }

	public void testReset04() { TestReset04.main(null); }

	public void testReset06() { TestReset06.main(null); }
	
	public void testVerboseIO() { TestVerboseIO.main(null); }
	public void testVerboseData() { TestVerboseData.main(null); }
	public void testVerboseHelper() { TestVerboseHelper.main(null); }
	public void testVerboseAOT() { TestVerboseAOT.main(null); }
	public void testSharedCacheJvmtiAPI() { TestSharedCacheJvmtiAPI.main(null); }
	public void testSharedCacheJavaAPI() { TestSharedCacheJavaAPI.main(null); }
	public void testDestroyCache() { TestDestroyCache.main(null); }
}
