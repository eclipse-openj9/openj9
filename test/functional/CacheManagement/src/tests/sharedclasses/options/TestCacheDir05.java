package tests.sharedclasses.options;

import java.io.File;

import tests.sharedclasses.TestUtils;

/*
 * Check that when cacheDir is used, we never get a javasharedresources sub folder
 */
public class TestCacheDir05 extends TestUtils {
  public static void main(String[] args) {
	  runDestroyAllCaches();
	  String dir = createTemporaryDirectory("TestCacheDir05");
	  String currentCacheDir = getCacheDir();
	  File javaSharedResourcesFolder = new File(dir+File.separator+"javasharedresources");
	  setCacheDir(dir);
	  try {
		  createPersistentCache("Foo");	  
		  if (javaSharedResourcesFolder.exists()) 
			  fail("Did not expect a javasharedresources folder to be created after creating a persistent cache when cacheDir is used");
		  createNonPersistentCache("Bar");	  
		  if (javaSharedResourcesFolder.exists()) 
			  fail("Did not expect a javasharedresources folder to be created after creating a non-persistent cache when cacheDir is used");
		  runDestroyAllCaches();
	  } finally {
		  runDestroyAllCaches();
		  setCacheDir(currentCacheDir);
		  deleteTemporaryDirectory(dir);
	  }
  }
}
