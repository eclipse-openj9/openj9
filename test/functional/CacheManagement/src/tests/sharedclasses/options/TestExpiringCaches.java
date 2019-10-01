package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Create the four caches (2 persistent, 2 non-persistent) and check they list OK
 * then expire them, check they all go.
 */
public class TestExpiringCaches extends TestUtils {
  public static void main(String[] args) {
	  
		if (isMVS()) {

			destroyNonPersistentCache("Foo");
			destroyNonPersistentCache("Bar");
			checkNoFileForNonPersistentCache("Foo");
			checkNoFileForNonPersistentCache("Bar");
			
			createNonPersistentCache("Bar");
			createNonPersistentCache("Foo");
			checkFileExistsForNonPersistentCache("Foo");
			checkFileExistsForNonPersistentCache("Bar");
			
			listAllCaches();
			checkOutputContains("Foo.*(no|non-persistent)",
					"Did not find a non-persistent cache called Foo");
			checkOutputContains("Bar.*(no|non-persistent)",
					"Did not find a non-persistent cache called Bar");
			try {
				Thread.sleep(1000);
			} catch (Exception e) {
			} // required???!?!?
			expireAllCaches();
			
			checkForSuccessfulNonPersistentCacheDestoryMessage("Bar");
			checkForSuccessfulNonPersistentCacheDestoryMessage("Foo");
			checkNoFileForNonPersistentCache("Foo");
			checkNoFileForNonPersistentCache("Bar");
		} else {
			destroyPersistentCache("Foo");
			destroyPersistentCache("Bar");
			destroyNonPersistentCache("Foo");
			destroyNonPersistentCache("Bar");
			checkNoFileForNonPersistentCache("Foo");
			checkNoFileForNonPersistentCache("Bar");
			checkNoFileForPersistentCache("Foo");
			checkNoFileForPersistentCache("Bar");

			createNonPersistentCache("Bar");
			createNonPersistentCache("Foo");
			createPersistentCache("Bar");
			createPersistentCache("Foo");
			checkFileExistsForNonPersistentCache("Foo");
			checkFileExistsForNonPersistentCache("Bar");
			checkFileExistsForPersistentCache("Foo");
			checkFileExistsForPersistentCache("Bar");

			listAllCaches();
			checkOutputContains("Foo.*(yes|persistent)",
					"Did not find a persistent cache called Foo");
			checkOutputContains("Foo.*(no|non-persistent)",
					"Did not find a non-persistent cache called Foo");
			checkOutputContains("Bar.*(yes|persistent)",
					"Did not find a persistent cache called Bar");
			checkOutputContains("Bar.*(no|non-persistent)",
					"Did not find a non-persistent cache called Bar");

			try {
				Thread.sleep(1000);
			} catch (Exception e) {
			} // required???!?!?
			expireAllCaches();

			checkForSuccessfulPersistentCacheDestroyMessage("Bar");
			checkForSuccessfulPersistentCacheDestroyMessage("Foo");
			checkForSuccessfulNonPersistentCacheDestoryMessage("Bar");
			checkForSuccessfulNonPersistentCacheDestoryMessage("Foo");

			checkNoFileForNonPersistentCache("Foo");
			checkNoFileForNonPersistentCache("Bar");
			checkNoFileForPersistentCache("Foo");
			checkNoFileForPersistentCache("Bar");
		}
  }
}
