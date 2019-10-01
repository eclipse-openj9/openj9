package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Check the 'none' option - should switch off any class sharing
 */
public class TestOptionNone extends TestUtils {
  public static void main(String[] args) {
    runDestroyAllCaches();
    runSimpleJavaProgramWithPersistentCache("plums","none");   
    checkOutputDoesNotContain("JVMSHRC236I", "Did not expect a message about a cache being created");
    
    runSimpleJavaProgramWithPersistentCache("plums");
    
    if (isMVS())
    {
        checkForSuccessfulNonPersistentCacheCreationMessage("plums");
    }
    else
    {
    	checkForSuccessfulPersistentCacheCreationMessage("plums");
    }
    runDestroyAllCaches();
  }
}
