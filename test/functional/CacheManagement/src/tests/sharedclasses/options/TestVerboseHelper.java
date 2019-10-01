package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Check the verboseHelper flag - should be no crash and some sensible output
 */
public class TestVerboseHelper extends TestUtils {
  public static void main(String[] args) {
    runDestroyAllCaches();
    runSimpleJavaProgramWithPersistentCache("plums","verboseHelper");
    // actual line I saw:
    // "Info for SharedClassURLClasspathHelper id 2: Created SharedClassURLClasspathHelper with id 2"
    // may need to change test if that changes
    checkOutputContains("Created SharedClassURLClasspathHelper", "Did not see expected verboseHelper text 'Created SharedClassURLClasspathHelper'");
    
    runSimpleJavaProgramWithNonPersistentCache("oranges","verboseHelper");
    checkOutputContains("Created SharedClassURLClasspathHelper", "Did not see expected verboseHelper text 'Created SharedClassURLClasspathHelper'");
    runDestroyAllCaches();
  }
}
