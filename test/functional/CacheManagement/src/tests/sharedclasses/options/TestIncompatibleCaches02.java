package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Create compatible and incompatible caches, check they are reported in the right
 * section on listAllCaches.
 */
public class TestIncompatibleCaches02 extends TestUtils {
  public static void main(String[] args) {
    runDestroyAllCaches();
    
	createNonPersistentCache("Bar");
	createNonPersistentCache("Foo");

	createPersistentCache("Bar");
	createPersistentCache("Foo");

	createIncompatibleCache("Foo5");
	createIncompatibleCache("Foo");
	createIncompatibleCache("Bar");

	listAllCaches();
	checkOutputForIncompatibleCache("Foo5",true);
	checkOutputForIncompatibleCache("Foo",true);
	checkOutputForIncompatibleCache("Bar",true);
	checkOutputForCompatiblePersistentCache("Foo",true);
	checkOutputForCompatiblePersistentCache("Bar",true);
	checkOutputForCompatiblePersistentCache("Foo",true);
	checkOutputForCompatiblePersistentCache("Bar",true);
	
	destroyNonPersistentCache("Foo");
	checkNoFileForNonPersistentCache("Foo");

	listAllCaches();
	checkOutputForIncompatibleCache("Foo5",true);
	checkOutputForIncompatibleCache("Foo",true);
	checkOutputForIncompatibleCache("Bar",true);

	destroyNonPersistentCache("Bar");
	checkNoFileForNonPersistentCache("Bar");

	listAllCaches();
	checkOutputForIncompatibleCache("Foo5",true);
	checkOutputForIncompatibleCache("Foo",true);
	checkOutputForIncompatibleCache("Bar",true);

	destroyPersistentCache("Foo");
	checkNoFileForPersistentCache("Foo");

	listAllCaches();
	checkOutputForIncompatibleCache("Foo5",true);
	checkOutputForIncompatibleCache("Foo",true);
	checkOutputForIncompatibleCache("Bar",true);

	destroyPersistentCache("Bar");
	checkNoFileForPersistentCache("Bar");

	listAllCaches();
	checkOutputForIncompatibleCache("Foo5",true);
	checkOutputForIncompatibleCache("Foo",true);
	checkOutputForIncompatibleCache("Bar",true);

	checkOutputForCompatiblePersistentCache("Foo",false);
	checkOutputForCompatiblePersistentCache("Bar",false);
	checkOutputForCompatibleNonPersistentCache("Foo",false);
	checkOutputForCompatibleNonPersistentCache("Bar",false);
	
	runDestroyAllCaches();
  }
}
