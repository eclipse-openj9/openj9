package tests.sharedclasses.options.junit;

import tests.sharedclasses.options.*;
import junit.framework.TestCase;

/**
 * A subset of tests that don't rely on any persistent cache support
 */
public class TestOptionsNonpersistentRealtime extends TestCase {

	public void testCommandLineOptionName02() { TestCommandLineOptionName02.main(null); }

	public void testCommandLineOptionName04() { TestCommandLineOptionName04.main(null); }

	public void testCommandLineOptionSilent01() { TestCommandLineOptionSilent01.main(null); }

	public void testCommandLineOptionSilent02() { TestCommandLineOptionSilent02.main(null); }

	public void testDestroyAllWhenNoCaches() { TestDestroyAllWhenNoCaches.main(null);}

	public void testPrintStats04() { TestPrintStats04.main(null); }

	public void testPrintStats05() { TestPrintStats05.main(null); }

}
