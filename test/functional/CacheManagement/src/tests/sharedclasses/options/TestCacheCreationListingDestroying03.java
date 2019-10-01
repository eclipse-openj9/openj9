package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Create two non-persistent caches and check they list OK, and they destroy OK
 */
public class TestCacheCreationListingDestroying03 extends TestUtils {
  public static void main(String[] args) {
    runDestroyAllCaches();
	checkNoFileForNonPersistentCache("Foo");
	checkNoFileForNonPersistentCache("Bar");
	
	createNonPersistentCache("Bar");
	createNonPersistentCache("Foo");
	checkFileExistsForNonPersistentCache("Foo");
	checkFileExistsForNonPersistentCache("Bar");

	listAllCaches();
	checkOutputContains("Foo.*no","Did not find a non-persistent cache called Foo");
	checkOutputContains("Bar.*no","Did not find a non-persistent cache called Bar");
	
	runDestroyAllCaches();
	checkNoFileForNonPersistentCache("Foo");
	checkNoFileForNonPersistentCache("Bar");
  }
}
