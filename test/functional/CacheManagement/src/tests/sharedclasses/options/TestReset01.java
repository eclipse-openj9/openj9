package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Create persistent cache, run something, check whats in there, then reset (as a standalone action) and check
 * reset works.
 */
public class TestReset01 extends TestUtils {
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

	runResetPersistentCache("Foo");
	checkForSuccessfulPersistentCacheCreationMessage("Foo");
	checkForSuccessfulPersistentCacheDestroyMessage("Foo");
	
	runPrintAllStats("Foo",true);
	checkOutputDoesNotContain("ROMCLASS:.*SimpleApp", "Did not expect to see SimpleApp entries in the output now the cache has been reset");

	runDestroyAllCaches();
  }
}
