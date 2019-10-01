package tests.sharedclasses.options;

import java.io.File;

import tests.sharedclasses.TestUtils;

/*
 * Check that when cacheDir is used and a non-persistent cache is used, we create the directory if it does not exist
 */
public class TestCacheDir07 extends TestUtils {
  public static void main(String[] args) {
	  runDestroyAllCaches();
	  String dir = createTemporaryDirectory("TestCacheDir07");
	  String currentCacheDir = getCacheDir();
	  String subdir = dir+File.separator+"subdir";
	  setCacheDir(subdir);
	  try {
		  runSimpleJavaProgramWithNonPersistentCache("Foo",false);
		  checkOutputDoesNotContain("JVMSHRC022E", 
				  "Did not expect message creating shared memory region, did we build the cacheDir ok? "+
				  subdir);		  
		  runDestroyAllCaches();
	  } finally {
		  runDestroyAllCaches();
		  setCacheDir(currentCacheDir);
		  deleteTemporaryDirectory(dir);
	  }
  }
}
