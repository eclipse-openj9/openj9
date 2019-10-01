package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Check the verboseIO flag - should be no crash and some sensible output
 */
public class TestVerboseIO extends TestUtils {
  public static void main(String[] args) {
    runDestroyAllCaches();
    runSimpleJavaProgramWithPersistentCache("plums","verboseIO");
    checkOutputContains("Failed to find class SimpleApp ", "Did not see expected verboseIO text 'Failed to find class SimpleApp'");
    runSimpleJavaProgramWithNonPersistentCache("oranges","verboseIO");
    checkOutputContains("Failed to find class SimpleApp ", "Did not see expected verboseIO text 'Failed to find class SimpleApp'");
    runDestroyAllCaches();
  }
}
