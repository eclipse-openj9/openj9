package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Check that controlDir and cacheDir work the same.  Set the cacheDir, create a persistent cache and use it
 * then reset cacheDir and point controlDir at the same place and check we can see the same cache.
 */
public class TestControlDirCacheDir01 extends TestUtils {
  public static void main(String[] args) {
	  runDestroyAllCaches();
	  if (isMVS())
	  {
		  //this test checks persistent and non persistent cahces ... zos only has nonpersistent ... so we assume other tests cover this
		  return;
	  }
	  String dir = createTemporaryDirectory("TestControlDirCacheDir01");
	  String currentCacheDir = getCacheDir();
	  String currentControlDir = getControlDir();
	  try {
		  //tidy up default area
		  setCacheDir(null);setControlDir(null);
		  runDestroyAllCaches();
		  setCacheDir(currentCacheDir);setControlDir(currentControlDir);

		  setCacheDir(dir);

		  runDestroyAllCaches();
		  createPersistentCache("Foo");
		  runSimpleJavaProgramWithPersistentCache("Foo");
		  checkForSuccessfulPersistentCacheOpenMessage("Foo");
		  checkFileExistsForPersistentCache("Foo");
		  
		  setCacheDir(null);
		  checkFileDoesNotExistForPersistentCache("Foo");
		  
		  setControlDir(dir);
		  checkFileExistsForPersistentCache("Foo");
		  
		  runSimpleJavaProgramWithPersistentCache("Foo");
		  checkForSuccessfulPersistentCacheOpenMessage("Foo");
		  
		  // Delete the cache and check it has vanished when viewed via cacheDir
		  destroyPersistentCache("Foo");
		  setControlDir(null);
		  setCacheDir(dir);
		  checkFileDoesNotExistForPersistentCache("Foo");		  
	  } finally {
		  runDestroyAllCaches();
		  setCacheDir(currentCacheDir);
		  setControlDir(currentControlDir);
		  deleteTemporaryDirectory(dir);
	  }
  }
//  
//  public static String createTemporaryDirectory() {
//	try {
//	  File f = File.createTempFile("testSC","dir");
//	  if (!f.delete()) fail("Couldn't create the temp dir");
//	  if (!f.mkdir()) fail("Couldnt create the temp dir");
//	  return f.getAbsolutePath();  
//	} catch (IOException e) {
//		fail("Couldnt create temp dir: "+e);
//		return null;
//	}
//  }
}
