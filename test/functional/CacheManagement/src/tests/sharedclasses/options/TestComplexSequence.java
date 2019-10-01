package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Create persistent cache Foo - check file is in right place
 * Create non-persistent cache Bar - check cache control file is in right place
 * Try to attach to persistent cache Foo - check attach to existing
 * Try to attach to non-persistent cache Bar - check attach to existing
 * Create non-persistent cache Foo - check new cache is created (and we dont connect to existing)
 * Create persistent cache Bar - check new cache is created (and we dont connect to existing)
 * Try to attach to non-persistent cache Foo - check attach to correct cache
 * Try to attach to persistent cache Bar - check attach to correct cache
 */
public class TestComplexSequence extends TestUtils {
  public static void main(String[] args) {
	  runDestroyAllCaches();
	  
	  if (isMVS())
	  {
		  //this test checks persistent and non persistent cahces ... zos only has nonpersistent ... so we assume other tests cover this
		  return;
	  }
	  
	  createPersistentCache("Foo");
	  checkFileExistsForPersistentCache("Foo");
	  checkNoFileForNonPersistentCache("Foo");
	  
	  createNonPersistentCache("Bar");
	  checkFileExistsForNonPersistentCache("Bar");
	  checkNoFileForPersistentCache("Bar");
	  
	  runSimpleJavaProgramWithPersistentCache("Foo");
	  checkOutputContains("JVMSHRC237I.*Foo", "Did not find expected message 'JVMSHRC237I Opened shared classes persistent cache Foo'");
	  runSimpleJavaProgramWithNonPersistentCache("Bar");
	  checkOutputContains("JVMSHRC159I.*Bar", "Did not find expected message 'JVMSHRC159I Successfully opened shared class cache \"Bar\"'");
	  
	  createNonPersistentCache("Foo");
	  checkOutputContains("JVMSHRC158I.*Foo", "Did not find expected message 'JVMSHRC158I Successfully created shared class cache \"Foo\"'");
	  checkFileExistsForNonPersistentCache("Foo");

	  createPersistentCache("Bar");
	  checkForSuccessfulPersistentCacheCreationMessage("Bar");
	  checkFileExistsForPersistentCache("Bar");
	  
	  runSimpleJavaProgramWithNonPersistentCache("Foo");
	  checkForSuccessfulNonPersistentCacheOpenMessage("Foo");
	  
	  runSimpleJavaProgramWithPersistentCache("Bar");
	  checkForSuccessfulPersistentCacheOpenMessage("Bar");
	  
	  runDestroyAllCaches();
  }
}
