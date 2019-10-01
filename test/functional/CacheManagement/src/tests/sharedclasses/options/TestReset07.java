package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Create an incompatible cache and a non-persistent cache, check reset warns us that we need to
 * specify nonpersistent and not that it reports the incompatible one.
 */
public class TestReset07 extends TestUtils {
  public static void main(String[] args) {
	  try {
	    runDestroyAllCaches();    
	    createIncompatibleCache("Foo");
	    createNonPersistentCache("Foo");
	    runResetPersistentCache("Foo");
	   // checkOutputContains("JVMSHRC278I.*Foo","Did not get expected message JVMSHRC273E about the incompatible form of Foo");
	    
	    showOutput();
	    fail("What is the expected message here?  I think I expect the message 'did you mean the non-persistent cache Foo because you forgot to specify nonpersistent'");
	  } finally {
		  runDestroyAllCaches();
	  }
  }
}
