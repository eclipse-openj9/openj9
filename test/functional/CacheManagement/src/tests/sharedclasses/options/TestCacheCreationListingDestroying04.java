package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Create two non-persistent caches, check they list OK
 * then destroy them one at a time, check the right files disappear as we go.
 */
public class TestCacheCreationListingDestroying04 extends TestUtils {
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
		
		destroyNonPersistentCache("Foo");
		checkNoFileForNonPersistentCache("Foo");
		checkFileExistsForNonPersistentCache("Bar");
		
		destroyNonPersistentCache("Bar");
		checkNoFileForNonPersistentCache("Foo");
		checkNoFileForNonPersistentCache("Bar");
  }
}
