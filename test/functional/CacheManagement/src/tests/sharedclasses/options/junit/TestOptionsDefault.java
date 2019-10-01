package tests.sharedclasses.options.junit;

import java.util.HashSet;
import java.util.Set;

import tests.sharedclasses.TestUtils;

/**
 * For every testcase inherited from the superclass this will just let the cache file location default.  It will
 * also check for leaking memory/semaphores on unix platforms.
 */
public class TestOptionsDefault extends TestOptionsBase {

	Set sharedMemorySegments;
	Set sharedSemaphores;
	
	protected void setUp() throws Exception {
		super.setUp();
		if (System.getProperty("check.shared.memory","no").toLowerCase().equals("yes")) {
			if (!TestUtils.isWindows()  && !TestUtils.isMVS()) {
				sharedMemorySegments = TestUtils.getSharedMemorySegments();
				sharedSemaphores = TestUtils.getSharedSemaphores();	
			}
		}
		System.out.println("Running  :" + this.getName() + "  (Test suite : " + this.getClass().getSimpleName() + ")");
	}
	
	protected void tearDown() throws Exception {
		super.tearDown();
		TestUtils.runDestroyAllCaches();
		if (System.getProperty("check.shared.memory","no").toLowerCase().equals("yes")) {
			if (!TestUtils.isWindows() && !TestUtils.isMVS()) {
				Set after = TestUtils.getSharedMemorySegments();
				Set difference = new HashSet();
				difference.addAll(after);
				difference.removeAll(sharedMemorySegments);
				if (difference.size()!=0) {
					throw new RuntimeException("Shared memory leaked: "+difference.size());
				}
				after = TestUtils.getSharedSemaphores();
				difference = new HashSet();
				difference.addAll(after);
				difference.removeAll(sharedSemaphores);
				if (difference.size()!=0) {
					throw new RuntimeException("Shared semaphores leaked: "+difference.size());
				}
			}
		}
		TestUtils.runDestroyAllCaches();
	}
}
