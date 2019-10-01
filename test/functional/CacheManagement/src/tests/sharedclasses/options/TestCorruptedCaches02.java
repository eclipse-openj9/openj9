package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Create a persistent cache, simulate it being ftp'd in ascii mode - check we detect the problem when trying to use it
 */
public class TestCorruptedCaches02 extends TestUtils {
	public static void main(String[] args) {
		runDestroyAllCaches();
		if (isMVS()) {
			// disabling for ZOS ... it doesn't make sense to corrupt the file
			// (it only contains os id for shared mem chunk) ...
			return;
		}
		createPersistentCache("Foo");
		corruptCache("Foo", true, "ftpascii");
		runSimpleJavaProgramWithPersistentCache("Foo", "disablecorruptcachedumps");
		checkOutputContains("SimpleApp running",
				"Did not get expected message about the application running");
		checkOutputContains("JVMSHRC442E.*\"Foo\"",
				"Did not get expected message JVMSHRC442E about the Foo cache being corrupted");
	}
}
