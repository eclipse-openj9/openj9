package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Create some incompatible caches, check they appear in listAllCaches in the appropriate
 * section.
 */
public class TestIncompatibleCaches01 extends TestUtils {
  public static void main(String[] args) {
    runDestroyAllCaches();
	createIncompatibleCache("Foo5");
	createIncompatibleCache("Foo");
	createIncompatibleCache("Bar");

	listAllCaches();
	checkOutputForIncompatibleCache("Foo5",true);
	checkOutputForIncompatibleCache("Foo",true);
	checkOutputForIncompatibleCache("Bar",true);

	runDestroyAllCaches();
	checkOutputContains("\"Bar\".*is destroyed", "Did not see expected message about cache Bar being destroyed");
	checkOutputContains("\"Foo\".*is destroyed", "Did not see expected message about cache Foo being destroyed");
	checkOutputContains("\"Foo5\".*is destroyed", "Did not see expected message about cache Foo5 being destroyed");
	
  }
}
