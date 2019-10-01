package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Check the verboseData flag - just check for no crash.
 */
public class TestVerboseData extends TestUtils {
  public static void main(String[] args) {
    runDestroyAllCaches();
    runSimpleJavaProgramWithPersistentCache("plums","verboseData");
    // actual message I saw in the output:
    // "[-Xshareclasses byte data verbose output enabled]"
    // may need to change test if that changes
    checkOutputContains("-Xshareclasses byte data verbose output enabled", "Did not see expected verboseData activation message '-Xshareclasses byte data verbose output enabledd'");
    runSimpleJavaProgramWithNonPersistentCache("oranges","verboseData");
    checkOutputContains("-Xshareclasses byte data verbose output enabled", "Did not see expected verboseData activation message '-Xshareclasses byte data verbose output enabledd'");
    runDestroyAllCaches();
  }
}
