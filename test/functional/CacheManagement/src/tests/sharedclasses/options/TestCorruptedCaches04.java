package tests.sharedclasses.options;

import tests.sharedclasses.RunCommand;
import tests.sharedclasses.TestUtils;

/*
 * Complex corruption - CMVC defect 123221
 * 1. start a VM using a cache
 * 2. start/stop a second VM using the same cache
 * 3. ask the first VM to change the cache (load something new)
 * 4. start/stop a third VM using the same cache (will crash!)
 * 
 * To achieve this, this test program launches the VM described in step 1.  See the type
 * TestCorruptedCaches04Helper to see the other steps.
 */
public class TestCorruptedCaches04 extends TestUtils {
	public static void main(String[] args) {
		runDestroyAllCaches();
		if (isMVS()) {
			// disabling for ZOS ... it doesn't make sense to corrupt the file
			// (it only contains os id for shared mem chunk) ...
			return;
		}
		createPersistentCache("Foo");

		String cmd = getCommand(RunComplexJavaProgramWithPersistentCache,
				"Foo", System.getProperty("config.properties",
						"config.properties"));
		RunCommand.execute(cmd);
		showOutput();
	}
}
