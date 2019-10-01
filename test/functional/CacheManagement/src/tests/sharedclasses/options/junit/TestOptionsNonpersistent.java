package tests.sharedclasses.options.junit;

import tests.sharedclasses.options.*;
import junit.framework.TestCase;

/**
 * A subset of tests that don't rely on any persistent cache support
 */
public class TestOptionsNonpersistent extends TestCase {
	public void testCacheCreationListingDestroying03() {TestCacheCreationListingDestroying03.main(null);}

	public void testCacheCreationListingDestroying04() {TestCacheCreationListingDestroying04.main(null);}

	public void testCacheDir04() {TestCacheDir04.main(null);}

	public void testCacheDir07() {TestCacheDir07.main(null);}

	public void testCommandLineOptionModified() { TestCommandLineOptionModified.main(null); }

	public void testCommandLineOptionName01() { TestCommandLineOptionName01.main(null); }

	public void testCommandLineOptionName02() { TestCommandLineOptionName02.main(null); }

	public void testCommandLineOptionName03() { TestCommandLineOptionName03.main(null); }

	public void testCommandLineOptionName04() { TestCommandLineOptionName04.main(null); }

	public void testCommandLineOptionSilent01() { TestCommandLineOptionSilent01.main(null); }

	public void testCommandLineOptionSilent02() { TestCommandLineOptionSilent02.main(null); }

	public void testDestroyAllWhenNoCaches() { TestDestroyAllWhenNoCaches.main(null);}

	//public void testIncompatibleCaches01() { TestIncompatibleCaches01.main(null);}

	//CMVC 146158: these tests leave artifacts on the system so i am removing them
	//public void testMovingControlFiles01() { TestMovingControlFiles01.main(null);}
	//public void testMovingControlFiles02() { TestMovingControlFiles02.main(null);}

	public void testPrintStats04() { TestPrintStats04.main(null); }
	
	public void testReattachingToNonpersistentCache02() { TestReattachingToNonpersistentCache02.main(null);}

	public void testPrintStats05() { TestPrintStats05.main(null); }

	public void testReset05() { TestReset05.main(null); }

	public void testPrintStats06() {TestPrintStats06.main(null); }
	
	// little too unreliable - need to be more selective on what platform they run or
	// support all unix flavours
//	public void testSemaphoreMeddling01() throws Exception { TestSemaphoreMeddling01.main(null);}
//
//	public void testSharedMemoryMeddling01() throws Exception { TestSharedMemoryMeddling01.main(null);}

}
