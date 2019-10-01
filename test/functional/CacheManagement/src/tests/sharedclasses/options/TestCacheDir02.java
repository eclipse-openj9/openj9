package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Set cacheDir to rubbish and run something
 */
public class TestCacheDir02 extends TestUtils {
  public static void main(String[] args) {
	  String currentCacheDir = getCacheDir();
	  try {
		  setCacheDir("!<><><><><><\\");
		  runSimpleJavaProgramWithPersistentCache("Foo",false);
		  showOutput();
		  fail("Did not expect the port layer error line did I?  (although I know the cache name is invalid)");		  
	  } finally {
		  runDestroyAllCaches();
		  deleteTemporaryDirectory("!<><><><><><\\");
		  setCacheDir(currentCacheDir);
	  }
	  fail("Did not expect the port layer error did I?  (I know the cache name is invalid)");
  }
}
