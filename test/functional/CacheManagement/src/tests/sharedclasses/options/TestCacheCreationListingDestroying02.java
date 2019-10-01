package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Create the four caches (2 persistent, 2 non-persistent) and check they list OK
 * then destroy them one at a time, check the right files disappear as we go.
 */
public class TestCacheCreationListingDestroying02 extends TestUtils {
  public static void main(String[] args) {
	  runDestroyAllCaches();
	  
	  if (isMVS())
	  {
		  //this test checks persistent and non persistent cahces ... zos only has nonpersistent ... so we assume other tests cover this
		  return;
	  }
	  
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
		if (isMVS())
		{
			checkOutputContains("Foo.*(no|non-persistent)","Did not find a non-persistent cache called Foo");
			checkOutputContains("Bar.*(no|non-persistent)","Did not find a non-persistent cache called Bar");
		}
		else
		{
			checkOutputContains("Foo.*(yes|persistent)","Did not find a persistent cache called Foo");
			checkOutputContains("Foo.*(no|non-persistent)","Did not find a non-persistent cache called Foo");
			checkOutputContains("Bar.*(yes|persistent)","Did not find a persistent cache called Bar");
			checkOutputContains("Bar.*(no|non-persistent)","Did not find a non-persistent cache called Bar");
		}
		if (isMVS())
		{
			destroyNonPersistentCache("Foo");
			checkNoFileForNonPersistentCache("Foo");
			checkFileExistsForNonPersistentCache("Bar");
			
			destroyNonPersistentCache("Bar");
			checkNoFileForNonPersistentCache("Foo");
			checkNoFileForNonPersistentCache("Bar");
		}
		else
		{
			destroyNonPersistentCache("Foo");
			checkNoFileForNonPersistentCache("Foo");
			checkFileExistsForNonPersistentCache("Bar");
			checkFileExistsForPersistentCache("Foo");
			checkFileExistsForPersistentCache("Bar");
	
			destroyPersistentCache("Foo");
			checkNoFileForNonPersistentCache("Foo");
			checkFileExistsForNonPersistentCache("Bar");
			checkNoFileForPersistentCache("Foo");
			checkFileExistsForPersistentCache("Bar");
			
			destroyPersistentCache("Bar");
			checkNoFileForNonPersistentCache("Foo");
			checkFileExistsForNonPersistentCache("Bar");
			checkNoFileForPersistentCache("Foo");
			checkNoFileForPersistentCache("Bar");
			
			destroyNonPersistentCache("Bar");
			checkNoFileForNonPersistentCache("Foo");
			checkNoFileForNonPersistentCache("Bar");
			checkNoFileForPersistentCache("Foo");
			checkNoFileForPersistentCache("Bar");
		}
  }
}
