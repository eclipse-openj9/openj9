package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Create a new non-persistent cache using default options and check the file goes to the
 * default location as expected.
 */
public class TestCreatingNonpersistentCache extends TestUtils {
  public static void main(String[] args) {
	  
	 if (isMVS()) {
			// this test checks persistent and non persistent cahces ... zos
			// only has nonpersistent ... so we assume other tests cover this
			return;
		}
	  
	destroyPersistentCache("Bar");
	destroyNonPersistentCache("Bar");
	checkNoFileForPersistentCache("Bar");
	checkNoFileForNonPersistentCache("Bar");
	
	createNonPersistentCache("Bar");
	checkForSuccessfulNonPersistentCacheCreationMessage("Bar");
	
	checkFileExistsForNonPersistentCache("Bar");
	checkNoFileForPersistentCache("Bar");
	destroyCache("Bar");
	
	runDestroyAllCaches();
  }
	
}
