package tests.sharedclasses.options.junit;

import tests.sharedclasses.TestUtils;

/**
 * For every testcase inherited from the superclass this will set the cacheDir.
 */
public class TestOptionsCacheDir extends TestOptionsBase {

	String currentCacheDir;
	String tmpdir;
	
	protected void setUp() throws Exception {
		super.setUp();
		currentCacheDir = TestUtils.getCacheDir();
		tmpdir= TestUtils.createTemporaryDirectory("TestOptionsCacheDir");
		TestUtils.setCacheDir(tmpdir);
		System.out.println("Running  :" + this.getName() + "  (Test suite : " + this.getClass().getSimpleName() + ")");
	}
	
	protected void tearDown() throws Exception {
		super.tearDown();
		TestUtils.runDestroyAllCaches();
		TestUtils.setCacheDir(currentCacheDir);
		TestUtils.deleteTemporaryDirectory(tmpdir);
	}
		
}
