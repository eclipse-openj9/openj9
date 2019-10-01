package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Create the four caches (2 persistent, 2 non-persistent) and check they list OK, and they destroy OK
 */
public class TestCacheCreationListingDestroying01 extends TestUtils {
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
	runDestroyAllCaches();
	checkNoFileForNonPersistentCache("Foo");
	checkNoFileForNonPersistentCache("Bar");
	checkNoFileForPersistentCache("Foo");
	checkNoFileForPersistentCache("Bar");
  }
}
