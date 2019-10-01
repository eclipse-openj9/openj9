package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Check the 'name=' option 
 * - does it work at all?
 * - the value '*' should give us a sensible error message
 * - the value '$' should give us a sensible error message
 */
public class TestCommandLineOptionName01 extends TestUtils {
  public static void main(String[] args) {	
	runDestroyAllCaches();

	// the basics
	runSimpleJavaProgramWithNonPersistentCache("elephant");
	listAllCaches();
	checkOutputForCompatibleCache("elephant",false,true);
	
	// rogue character
	runPrintStats("*",false);
	checkOutputContains("JVMSHRC147E", "expected message about '*' not being valid for cache name");

	// rogue character
	runSimpleJavaProgramWithNonPersistentCache("$%u",false);
	checkOutputContains("JVMSHRC147E", "expected message about '$' not being valid for cache name");

	// rogue character
	runSimpleJavaProgramWithNonPersistentCache("&%u",false);
	checkOutputContains("JVMSHRC147E", "expected message about '&' not being valid for cache name");

	runDestroyAllCaches();
  }
}
