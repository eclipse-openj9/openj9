package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Create a cache with testFakeCorruption, then read that cache with the option
 * disablecorruptcachedumps and checks that no dumps are created
 */
public class TestDisableCorruptCacheDumps extends TestUtils {
	public static void main(String[] args) {
		runDestroyAllCaches();    

		if (isMVS()) {
			// disabling for ZOS ... it doesn't make sense to corrupt the file
			return;
		}
    
		createPersistentCache("Foo");
		corruptCache("Foo", true, "truncate");
		runSimpleJavaProgramWithPersistentCache("Foo", "disablecorruptcachedumps");
		
		checkOutputContains("JVMSHRC442E.*\"Foo\"",
		"Did not get expected message JVMSHRC442E about the Foo cache being corrupted");
		checkOutputForDump(false);
		runDestroyAllCaches();
	}
}