package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Create compatible and incompatible caches and just run a simple
 * printStats on each. Should get errors for incompatible caches.
 */
public class TestPrintStats03 extends TestUtils {
  public static void main(String[] args) {
	//String cachenamefoo = TestUtils.getCacheFileName("Foo",false,false);
    runDestroyAllCaches();
    
	createNonPersistentCache("Foo");

	createPersistentCache("Foo");
	
	createIncompatibleCache("Foo5");
	createIncompatibleCache("Foo");
	
	runPrintStats("Foo",true);
	checkOutputContains("Current statistics for cache", "Did not find expected line of output 'Current statistics for cache'");
	
	runPrintStats("Foo",false);
	checkOutputContains("Current statistics for cache", "Did not find expected line of output 'Current statistics for cache'");
	
	runPrintStats("Foo5",true);
	checkOutputContains("incompatible", "Did not get expected message JVMSHRC278I about printStats cannot operate on incompatible cache Foo5");
	
	runDestroyAllCaches();
  }
}
