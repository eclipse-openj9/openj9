package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Check the verboseAOT flag - just check for no crash.  AOT messages are intermittent so cannot rely on looking for them.
 */
public class TestVerboseAOT extends TestUtils {
  public static void main(String[] args) {
    runDestroyAllCaches();
    runSimpleJavaProgramWithPersistentCache("plums","verboseAOT");
    // actual message I saw in the output:
    // "[-Xshareclasses AOT verbose output enabled]"
    // may need to change test if that changes
    checkOutputContains("-Xshareclasses AOT verbose output enabled", "Did not see expected verboseAOT activation message '-Xshareclasses AOT verbose output enabled'");
    runSimpleJavaProgramWithNonPersistentCache("oranges","verboseAOT");
    checkOutputContains("-Xshareclasses AOT verbose output enabled", "Did not see expected verboseAOT activation message '-Xshareclasses AOT verbose output enabled'");
    runDestroyAllCaches();
  }
}
