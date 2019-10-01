package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Attempt to reset an non existent cache
 */
public class TestReset06 extends TestUtils {
  public static void main(String[] args) {
    runDestroyAllCaches();    
    runResetPersistentCache("Foo");
    checkOutputContains("JVMSHRC023E","Did not get expected message JVMSHRC023E about the cache not existing");
	
    runDestroyAllCaches();
  }
}
