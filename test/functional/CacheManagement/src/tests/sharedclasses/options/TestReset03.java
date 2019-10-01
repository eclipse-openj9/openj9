package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Attempt to reset an incompatible cache.
 */
public class TestReset03 extends TestUtils {
  public static void main(String[] args) {
    runDestroyAllCaches();  
	createIncompatibleCache("Foo");
	runResetPersistentCache("Foo");
	checkOutputContains("JVMSHRC278I.*\"reset\".*\"Foo\"",
			"Did not get expected message JVMSHRC278I about reset not working on incompatible cache Foo");

	runDestroyAllCaches();
  }
}
