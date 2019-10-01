package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Create compatible caches (persistent and non-persistent) and just run a simple
 * printStats on each.
 */
public class TestPrintStats01 extends TestUtils {
  public static void main(String[] args) {
    runDestroyAllCaches();
    
	createNonPersistentCache("Bar");
	createNonPersistentCache("Foo");

	createPersistentCache("Bar");
	createPersistentCache("Foo");
	
	runPrintStats("Foo",true);
	checkOutputContains("Current statistics for cache", "Did not find expected line of output 'Current statistics for cache'");
	
	runPrintStats("Foo",false);
	checkOutputContains("Current statistics for cache", "Did not find expected line of output 'Current statistics for cache'");
	
	runPrintStats("Bar",true);
	checkOutputContains("Current statistics for cache", "Did not find expected line of output 'Current statistics for cache'");

	runPrintStats("Bar",false);
	checkOutputContains("Current statistics for cache", "Did not find expected line of output 'Current statistics for cache'");
	
	runDestroyAllCaches();
  }
}
