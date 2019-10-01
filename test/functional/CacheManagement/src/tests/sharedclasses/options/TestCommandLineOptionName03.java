package tests.sharedclasses.options;

import java.util.Iterator;
import java.util.List;

import tests.sharedclasses.TestUtils;

/*
 * Check the 'name=' option 
 * - the value '%u' should be the userid
 * - the value '%g' should be the group id
 */
public class TestCommandLineOptionName03 extends TestUtils {
  public static void main(String[] args) {	
	runDestroyAllCaches();

	// %u - should be fine
	runSimpleJavaProgramWithNonPersistentCache("%u");

	if (isWindows()) {
		// %g - not supported
		runSimpleJavaProgramWithNonPersistentCache("%g",false);
		checkOutputContains("JVMSHRC154E.*character g", "Did not get expected message about escape character G not being supported on windows");		
	} else {
		// %g - should be fine
		runSimpleJavaProgramWithNonPersistentCache("%g");
	}
	listAllCaches();

	List l = processOutputForCompatibleCacheList();
	for (Iterator iterator = l.iterator(); iterator.hasNext();) {
		String name = (String) iterator.next();
		if (name.equals("")) {
			listAllCaches();
			fail("Cache found with a blank name!!!!");
		}
	}

	runDestroyAllCaches();
  }
}
