package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * listAllCaches when there are no caches - expect sensible output
 */
public class TestListAllWhenNoCaches extends TestUtils {
  public static void main(String[] args) {
    runDestroyAllCaches();   
    String tdir = createTemporaryDirectory("TestListAllWhenNoCaches");
    String currentCacheDir = getCacheDir();
    
    setCacheDir(tdir);
    try {
    	listAllCaches();
	    checkOutputContains("JVMSHRC005I", "Did not get expected message about there being no caches available");
	    checkOutputDoesNotContain("JVMSHRC...E", "unexpected error received");
    } finally {
    	setCacheDir(currentCacheDir);
    	deleteTemporaryDirectory(tdir);
    }
  }
}
