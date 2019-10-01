package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Destroy caches that don't exist, checking for sensible message
 */
public class TestDestroyNonExistentCaches extends TestUtils {
  public static void main(String[] args) {
    runDestroyAllCaches();   
    destroyPersistentCache("DoesNotExist");
    checkOutputContains("JVMSHRC023E", "Did not get expected message JVMSHRC023E saying the cache did not exist");
  }
}
