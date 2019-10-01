package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Create non-persistent cache, run something, check whats in there, then run it again with
 * reset sub-option.
 */
public class TestReset05 extends TestUtils {
  public static void main(String[] args) {
    runDestroyAllCaches();    
	createNonPersistentCache("Foo");
	runSimpleJavaProgramWithNonPersistentCache("Foo");
	
	runPrintAllStats("Foo",false);
	checkOutputContains("ROMCLASS:.*SimpleApp", "Did not find expected entry for SimpleApp in the printAllStats output");

	runSimpleJavaProgramWithNonPersistentCacheAndReset("Foo");
	checkOutputContains("JVMSHRC159I","Expected message about the cache being opened: JVMSHRC159I");
	checkOutputContains("is destroyed","Expected message about the cache being destroyed: is destroyed");
	checkOutputContains("JVMSHRC158I","Expected message about the cache being created: JVMSHRC158I");
	
	runPrintAllStats("Foo",false);
	checkOutputContains("ROMCLASS:.*SimpleApp", "Did not find expected entry for SimpleApp in the printAllStats output");

	runDestroyAllCaches();
  }
}
