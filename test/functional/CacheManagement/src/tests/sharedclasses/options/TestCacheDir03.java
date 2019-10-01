package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Set cacheDir to rubbish and run something
 */
public class TestCacheDir03 extends TestUtils {
  public static void main(String[] args) {
	  String currentCacheDir = getCacheDir();
	  deleteTemporaryDirectory("!%#&^%#");
	  try {
		  setCacheDir("!%#&^%#"); // this is horrible, but valid - at least on windows...
		  runSimpleJavaProgramWithPersistentCache("Foo");
	  } finally {
		  runDestroyAllCaches();
		  setCacheDir(currentCacheDir);
		  // try and tidy up a bit?
		  deleteTemporaryDirectory("!%#&^%#");
	  }
  }
}
