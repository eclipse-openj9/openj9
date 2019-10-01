package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Create a persistent cache, zero some of it - check we detect the problem when trying to use it
 */
public class TestCorruptedCaches05 extends TestUtils {
	public static void main(String[] args) {
		runDestroyAllCaches();
		if (isMVS()) {
			// disabling for ZOS ... it doesn't make sense to corrupt the file
			// (it only contains os id for shared mem chunk) ...
			return;
		}
		createPersistentCache("Foo");
		corruptCache("Foo", true, "zeroStringCache");
		
		runSimpleJavaProgramWithPersistentCacheCheckStringTable("Foo");
		
		checkOutputContains("Resetting shared string table",
				"Did not get expected message Resetting shared string table");
	}
}
