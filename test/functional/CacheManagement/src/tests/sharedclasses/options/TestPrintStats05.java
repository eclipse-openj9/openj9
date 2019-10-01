package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Running printAllStats on non-existent cache
 */
public class TestPrintStats05 extends TestUtils {
  public static void main(String[] args) {
    runDestroyAllCaches();
	
	runPrintAllStats("Foo",true);
	checkOutputContains("JVMSHRC023E","Did not get expected message JVMSHRC023E about the cache not existing");
  }
}
