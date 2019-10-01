package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Check the 'name=' option 
 * - the value '%d' should not be ok
 */
public class TestCommandLineOptionName02 extends TestUtils {
  public static void main(String[] args) {	
	runDestroyAllCaches();

	// %d - should not be fine
	runSimpleJavaProgramWithNonPersistentCache("%d",false);
	checkOutputContains("JVMSHRC154E.*character d", 
			            "Did not get expected message about 'escape character d' being invalid");

	runDestroyAllCaches();
  }
}
