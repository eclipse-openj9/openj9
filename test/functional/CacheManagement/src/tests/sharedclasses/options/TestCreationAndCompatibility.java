package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Create Java6 caches, list them all and check they are marked compatible
 */
public class TestCreationAndCompatibility extends TestUtils {
  public static void main(String[] args) {
    destroyPersistentCache("Foo");
	destroyPersistentCache("Bar");
	destroyNonPersistentCache("Foo");
	destroyNonPersistentCache("Bar");
	
	 if (isMVS()) {
			// this test checks persistent and non persistent cahces ... zos
			// only has nonpersistent ... so we assume other tests cover this
			return;
		}
	
	createNonPersistentCache("Bar");
	createNonPersistentCache("Foo");
	createPersistentCache("Bar");
	createPersistentCache("Foo");

	listAllCaches();

	checkOutputForCompatiblePersistentCache("Foo",true);
	checkOutputForCompatiblePersistentCache("Bar",true);

	checkOutputForCompatibleNonPersistentCache("Foo",true);
	checkOutputForCompatibleNonPersistentCache("Bar",true);

	
	runDestroyAllCaches();
  }
	
}
