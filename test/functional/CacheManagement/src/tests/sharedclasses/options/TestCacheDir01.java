package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Check that when cacheDir is used, the files go to the right place.
 * If cacheDir is used, the persistent cache files go directly where it points whilst
 * the non-persistent cache control files go in the subdirectory javasharedresources 
 */
public class TestCacheDir01 extends TestUtils {
  public static void main(String[] args) {
	  runDestroyAllCaches();
	  if (isMVS())
	  {
		  //this test checks persistent and non persistent cahces ... zos only has nonpersistent ... so we assume other tests cover this
		  return;
	  }
	  String dir = createTemporaryDirectory("TestCacheDir01");
	  String currentCacheDir = getCacheDir();
	  setCacheDir(dir);
	  try {
		  createPersistentCache("Foo");	  
		  checkFileExistsForPersistentCache("Foo");
		  createNonPersistentCache("Foo2");	  
		  checkFileExistsForNonPersistentCache("Foo2");
	  } finally {
		  runDestroyAllCaches();
		  setCacheDir(currentCacheDir);
		  deleteTemporaryDirectory(dir);
	  }
  }
}
