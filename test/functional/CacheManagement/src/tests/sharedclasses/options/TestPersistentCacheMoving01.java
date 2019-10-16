package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;
import java.io.*;

/*
 * Create a persistent cache, move the cache to another location, use cachedir to pick it up from the other 
 * location and check no new classes are loaded.
 */
public class TestPersistentCacheMoving01 extends TestUtils {
  public static void main(String[] args) {
	  runDestroyAllCaches();

	  if (isMVS())
	  {
		  // no support on ZOS for persistent caches ...
		  return;
	  }
	  
	  runSimpleJavaProgramWithPersistentCache("Foo,verboseIO");
	  checkOutputContains("Stored class SimpleApp2", "Did not find expected message about the test class being stored in the cache");
	  String currentCacheDir = getCacheDir();
	  String tmpdir = createTemporaryDirectory("TestPersistentCacheMoving01");
	  try {
		  
		  // move the cache file to the temp directory
		  String loc = getCacheFileLocationForPersistentCache("Foo");
		  File f = new File(loc);
		  File fHidden = new File(tmpdir+File.separator+getCacheFileName("Foo", true));
		//  System.out.println(loc);
		//  System.out.println(fHidden);
		  if (!f.renameTo(fHidden)) fail("Could not rename the control file to be hidden");
		  setCacheDir(tmpdir);
		  listAllCaches();
		  checkOutputForCompatibleCache("Foo",true,true);
		  
		  runSimpleJavaProgramWithPersistentCache("Foo,verboseIO");
		  checkOutputDoesNotContain("Stored class SimpleApp2", "Unexpectedly found message about storing the SimpleApp2 class in the cache, it should already be there!");
		  checkOutputContains("Found class SimpleApp2", "Did not get expected message about looking for SimpleApp2 in the cache");

		  runDestroyAllCaches();
	  } finally {
		  deleteTemporaryDirectory(tmpdir);
		  setCacheDir(currentCacheDir);
	  }
  }
}
