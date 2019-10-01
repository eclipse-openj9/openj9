package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Running printStats on non-existent cache
 */
public class TestPrintStats04 extends TestUtils {
  public static void main(String[] args) {
    runDestroyAllCaches();	
	runPrintStats("Foo",true);
	checkOutputContains("JVMSHRC023E","Did not get expected message JVMSHRC023E about the cache not existing");

	runDestroyAllCaches();
  }
}
