package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Create compatible caches (persistent and non-persistent) and just run a simple
 * printAllStats on each.
 */
public class TestPrintStats02 extends TestUtils {
  public static void main(String[] args) {
    runDestroyAllCaches();
    
	createNonPersistentCache("Bar");
	createNonPersistentCache("Foo");

	createPersistentCache("Bar");
	createPersistentCache("Foo");
	
	runPrintAllStats("Foo",true);
	checkOutputContains("Current statistics for cache", "Did not find expected line of output 'Current statistics for cache'");
	checkOutputContains("1:.*CLASSPATH","Expected to find a line in the printAllStats output relating to CLASSPATH");
	
	runPrintAllStats("Foo",false);
	checkOutputContains("Current statistics for cache", "Did not find expected line of output 'Current statistics for cache'");
	checkOutputContains("1:.*CLASSPATH","Expected to find a line in the printAllStats output relating to CLASSPATH");
	
	runPrintAllStats("Bar",true);
	checkOutputContains("Current statistics for cache", "Did not find expected line of output 'Current statistics for cache'");
	checkOutputContains("1:.*CLASSPATH","Expected to find a line in the printAllStats output relating to CLASSPATH");

	runPrintAllStats("Bar",false);
	checkOutputContains("Current statistics for cache", "Did not find expected line of output 'Current statistics for cache'");
	checkOutputContains("1:.*CLASSPATH","Expected to find a line in the printAllStats output relating to CLASSPATH");
	
	runDestroyAllCaches();
  }
}
