package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Create compatible and incompatible caches, check they are reported in the right
 * section on listAllCaches - then destroyAll and check they *all* disappear.
 */
public class TestIncompatibleCaches03 extends TestUtils {
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
	
	runDestroyAllCaches();
	
	listAllCaches();
	checkOutputForIncompatibleCache("Foo5",false);
	checkOutputForIncompatibleCache("Foo",false);
	checkOutputForIncompatibleCache("Bar",false);
	checkOutputForCompatiblePersistentCache("Foo",false);
	checkOutputForCompatiblePersistentCache("Bar",false);
	checkOutputForCompatiblePersistentCache("Foo",false);
	checkOutputForCompatiblePersistentCache("Bar",false);
	
  }
}
