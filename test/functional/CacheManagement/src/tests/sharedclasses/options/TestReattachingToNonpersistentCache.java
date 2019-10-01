package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Create a new non-persistent cache using default options and then attempt to reuse it on
 * a second run.
 */
public class TestReattachingToNonpersistentCache extends TestUtils {
  public static void main(String[] args) {
	destroyNonPersistentCache("Bar");
	destroyPersistentCache("Bar");
	checkNoFileForNonPersistentCache("Bar");
	checkNoFileForPersistentCache("Bar");
	
	createNonPersistentCache("Bar");
	checkForSuccessfulNonPersistentCacheCreationMessage("Bar");
	
	runSimpleJavaProgramWithNonPersistentCache("Bar");
	checkOutputContains("JVMSHRC166I.*Bar","Expected message about successfully opening a shared class cache");

	runDestroyAllCaches();
  }
}
