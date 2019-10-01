package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Create a persistent cache, zero some of it - check we detect the problem when trying to use it
 */
public class TestCorruptedCaches03 extends TestUtils {
	public static void main(String[] args) {
		runDestroyAllCaches();
		if (isMVS()) {
			// disabling for ZOS ... it doesn't make sense to corrupt the file
			// (it only contains os id for shared mem chunk) ...
			return;
		}
		createPersistentCache("Foo");
		corruptCache("Foo", true, "zero");
		
		runSimpleJavaProgramWithPersistentCache("Foo", "disablecorruptcachedumps");
		
		if (! realtimeTestsSelected() ) {
			checkOutputContains("SimpleApp running",
			"Did not get expected message about the application running");
		}
		
		checkOutputContains("JVMSHRC442E.*\"Foo\"",
				"Did not get expected message JVMSHRC442E about the Foo cache being corrupted");
	}
}
