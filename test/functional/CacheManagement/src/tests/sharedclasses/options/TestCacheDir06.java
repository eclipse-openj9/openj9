package tests.sharedclasses.options;

import java.io.File;

import tests.sharedclasses.TestUtils;

/*
 * Check that when cacheDir is used and a persistent cache created, we create the directory if it does not exist
 */
public class TestCacheDir06 extends TestUtils {
  public static void main(String[] args) {
	  runDestroyAllCaches();
	  String dir = createTemporaryDirectory("TestCacheDir06");
	  String currentCacheDir = getCacheDir();
	  String subdir = dir+File.separator+"subdir";
	  setCacheDir(subdir);
	  try {
		  runSimpleJavaProgramWithPersistentCache("Foo",false);
		  checkOutputDoesNotContain("JVMSHRC215E", 
				  "Did not expect message about the path not being found, it should have been constructed: "+
				  subdir);		  
		  runDestroyAllCaches();
	  } finally {
		  runDestroyAllCaches();
		  setCacheDir(currentCacheDir);
		  deleteTemporaryDirectory(dir);
	  }
  }
}
