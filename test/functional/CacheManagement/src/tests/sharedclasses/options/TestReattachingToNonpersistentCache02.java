package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Create a new non-persistent cache using default options and then attempt to reuse it on
 * a second run.
 */
public class TestReattachingToNonpersistentCache02 extends TestUtils {
  public static void main(String[] args) {
    runDestroyAllCaches();
	createNonPersistentCache("Bar");
	checkForSuccessfulNonPersistentCacheCreationMessage("Bar");
	
	runSimpleJavaProgramWithNonPersistentCache("Bar");
	checkOutputContains("JVMSHRC166I.*Bar","Expected message about successfully opening a shared class cache");

	runDestroyAllCaches();
  }
}
