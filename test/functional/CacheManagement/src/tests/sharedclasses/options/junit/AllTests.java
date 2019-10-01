package tests.sharedclasses.options.junit;

import tests.sharedclasses.TestUtils;
import junit.framework.Test;
import junit.framework.TestSuite;

public class AllTests {

	public static Test suite() {
		TestSuite suite = new TestSuite(
				"Testing design 467 changes to shared classes command line options");
		//$JUnit-BEGIN$
		if (TestUtils.realtimeTestsSelected()){
			suite.addTestSuite(TestOptionsDefaultRealtime.class);
			suite.addTestSuite(TestOptionsControlDirRealtime.class);
			suite.addTestSuite(TestOptionsCacheDirRealtime.class);
		} else {
			suite.addTestSuite(TestOptionsDefault.class);
			suite.addTestSuite(TestOptionsControlDir.class);
			suite.addTestSuite(TestOptionsCacheDir.class);
		}
		//$JUnit-END$
		return suite;
	}

}
