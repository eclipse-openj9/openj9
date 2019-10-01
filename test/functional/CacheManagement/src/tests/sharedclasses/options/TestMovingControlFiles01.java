package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Create non-persistent cache, use it then move the control file, use it again (new one created), delete than one
 * and return the original control file, use it then delete it.
 */
public class TestMovingControlFiles01 extends TestUtils {
  public static void main(String[] args) {
	  runDestroyAllCaches();
	  createNonPersistentCache("Foo");	  
	  runSimpleJavaProgramWithNonPersistentCache("Foo");

	  String fromdir = getCacheFileDirectory(false);
	  String todir = createTemporaryDirectory("TestListAllWhenNoCaches");
	  try {
		  
		  // move the control file to a temp directory
		  moveControlFilesForNonPersistentCache("Foo",fromdir,todir);
		  
		  // shouldnt be able to find it now...
		  listAllCaches();
		  checkOutputForCompatibleCache("Foo",false,false);
		  
		  // recreated by this run
		  runSimpleJavaProgramWithNonPersistentCache("Foo");
		  checkForSuccessfulNonPersistentCacheCreationMessage("Foo");
	
		  destroyNonPersistentCache("Foo");
		  
		  // Copy the old control file back

		  moveControlFilesForNonPersistentCache("Foo",todir,fromdir);
		  // use it
		  if (isWindows()) {
			  runSimpleJavaProgramWithNonPersistentCache("Foo");
			  checkForSuccessfulNonPersistentCacheOpenMessage("Foo");
		  } else {
			  // this may fail if our new file reuses inodes ...
			  runSimpleJavaProgramWithNonPersistentCacheMayFail("Foo");
		  }
		  
		  destroyNonPersistentCache("Foo");
		  
		  if (isWindows()) {
			  checkForSuccessfulNonPersistentCacheDestoryMessage("Foo");
		  }

	  } finally {
		  deleteTemporaryDirectory(todir);
	  }
  }
}
