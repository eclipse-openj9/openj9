package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * expire=0 when there are no caches - expect sensible message
 */
public class TestExpireAllWhenNoCaches extends TestUtils {
  public static void main(String[] args) {
    runDestroyAllCaches();   
    String tdir = createTemporaryDirectory("TestExpireAllWhenNoCaches");
    String currentCacheDir = getCacheDir();
    setCacheDir(tdir);
    try {
    	expireAllCaches();
	    checkOutputContains("JVMSHRC005I", "Did not get expected message about there being no caches available");
	    checkOutputDoesNotContain("JVMSHRC...E", "unexpected error received");
	    if (isMVS())
	    {
	    	checkForSuccessfulNonPersistentCacheCreationMessage("");
	    }
	    else
	    {
	    	checkForSuccessfulPersistentCacheCreationMessage("");
	    }
    } finally {
    	runDestroyAllCaches();
    	setCacheDir(currentCacheDir);
    	deleteTemporaryDirectory(tdir);
    }
  }
}
