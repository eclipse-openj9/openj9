package tests.sharedclasses.options;


import java.io.File;
import java.io.IOException;


import tests.sharedclasses.TestUtils;

/*
 * Check that controlDir and cacheDir work the same.  Set the cacheDir, create a non-persistent cache and use it
 * then reset cacheDir and point controlDir at the same place and check we can see the same cache.
 */
public class TestControlDirCacheDir02 extends TestUtils {
  public static void main(String[] args) {
	  runDestroyAllCaches();
	  
	  String dir = createTemporaryDirectory("TestControlDirCacheDir02");
	  String currentCacheDir = getCacheDir();
	  String currentControlDir = getControlDir();
	  try {
		  // tidyup before we go...
		  setCacheDir(null);setControlDir(null);
		  runDestroyAllCaches();
		  setCacheDir(currentCacheDir);setControlDir(currentControlDir);

		  setCacheDir(dir);

		  runDestroyAllCaches();
		  createNonPersistentCache("Foo");
		  runSimpleJavaProgramWithNonPersistentCache("Foo");
		  checkForSuccessfulNonPersistentCacheOpenMessage("Foo");
		  checkFileExistsForNonPersistentCache("Foo");
		  
		  setCacheDir(null);
		  checkFileDoesNotExistForNonPersistentCache("Foo");
		  
		  setControlDir(dir);
		  checkFileExistsForNonPersistentCache("Foo");
		  
		  runSimpleJavaProgramWithNonPersistentCache("Foo");
		  checkForSuccessfulNonPersistentCacheOpenMessage("Foo");
		  
		  // Delete the cache and check it has vanished when viewed via cacheDir
		  destroyNonPersistentCache("Foo");
		  setControlDir(null);
		  setCacheDir(dir);
		  checkFileDoesNotExistForNonPersistentCache("Foo");		  
	  } finally {
		  setCacheDir(currentCacheDir);
		  setControlDir(currentControlDir);
		  deleteTemporaryDirectory(dir);
	  }
  }  
}
