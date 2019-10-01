package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Call destroyAll when there are no caches - expect sensible message
 */
public class TestDestroyAllWhenNoCaches extends TestUtils {
  public static void main(String[] args) {
    runDestroyAllCaches();   
    String tdir = createTemporaryDirectory("TestDestroyAllWhenNoCaches");
    String currentCacheDir = getCacheDir();
    setCacheDir(tdir);
    try {
    	runDestroyAllCaches();
	    checkOutputContains("JVMSHRC005I", "Did not get expected message about there being no caches available");
	    checkOutputDoesNotContain("JVMSHRC...E", "unexpected error received");
    } finally {
    	setCacheDir(currentCacheDir);
    	deleteTemporaryDirectory(tdir);
    }
  }
}
