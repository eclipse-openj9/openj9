package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Use the modification context command line option and check some entries in the cache are
 * stored against it.
 */
public class TestCommandLineOptionModified extends TestUtils {
  public static void main(String[] args) {
	
	runDestroyAllCaches();
	runSimpleJavaProgramWithNonPersistentCache("testCache");
	runPrintAllStats("testCache",false);
	checkOutputDoesNotContain("ModContext", "Did not expect to find modContext mentioned in the stats output");

	runSimpleJavaProgramWithNonPersistentCache("testCache,modified=mod1");
	runPrintAllStats("testCache",false);
	// example output:
    //3: 0x4343DC10 ROMCLASS: java/net/URI at 0x42589CC8.
    //	Index 1 in classpath 0x4349F6FC
    //	ModContext mod1
	checkOutputContains("ModContext.*mod1", "Expected the ModContext to be mentioned for some entries");
	runDestroyAllCaches();
  }
}
