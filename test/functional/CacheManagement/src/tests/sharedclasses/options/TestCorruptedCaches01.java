package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Create a persistent cache, chop it (truncate it) then try and use it - check we detect the problem.
 */
public class TestCorruptedCaches01 extends TestUtils {
	public static void main(String[] args) {
		runDestroyAllCaches();
		if (isMVS()) {
			// disabling for ZOS ... it doesn't make sense to corrupt the file
			// (it only contains os id for shared mem chunk) ...
			return;
		}
		createPersistentCache("Foo");
		corruptCache("Foo", true, "truncate");
		runSimpleJavaProgramWithPersistentCache("Foo", "disablecorruptcachedumps");
		checkOutputContains("SimpleApp running",
				"Did not get expected message about the application running");
		checkOutputContains("JVMSHRC442E.*\"Foo\"",
				"Did not get expected message JVMSHRC442E about the Foo cache being corrupted");
	}
}
