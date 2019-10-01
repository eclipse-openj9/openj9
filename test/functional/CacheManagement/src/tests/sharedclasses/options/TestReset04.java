package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Create persistent cache, run something, check whats in there, then run it again with a reset option.
 */
public class TestReset04 extends TestUtils {
  public static void main(String[] args) {
    runDestroyAllCaches();    
    
	if (isMVS())
	{
		//this test checks persistent and non persistent cahces ... zos only has nonpersistent ... so we assume other tests cover this
		return;
	}
    
	createPersistentCache("Foo");
	runSimpleJavaProgramWithPersistentCache("Foo");
	
	runPrintAllStats("Foo",true);
	checkOutputContains("ROMCLASS:.*SimpleApp", "Did not find expected entry for SimpleApp in the printAllStats output");

	runSimpleJavaProgramWithPersistentCacheAndReset("Foo");
	checkForSuccessfulPersistentCacheDestroyMessage("Foo");
	checkForSuccessfulPersistentCacheCreationMessage("Foo");
	
	runPrintAllStats("Foo",true);
	checkOutputContains("ROMCLASS:.*SimpleApp", "Did not find expected entry for SimpleApp in the printAllStats output");

	runDestroyAllCaches();
  }
}
