package tests.sharedclasses.options;

import tests.sharedclasses.RunCommand;
import tests.sharedclasses.TestUtils;

/*
 * Check -Xscmcx
 */
public class TestCommandLineOptionXscmx01 extends TestUtils {
  public static void main(String[] args) {	
	runDestroyAllCaches();
	
	/* Note: AOT is turned off for the below tests. In some cases the JIT has 
	 * enough time to store information in the already small cache. During this 
	 * test this may cause the alredy to small cache to be marked as full 
	 * (when this is not expected to occur).
	 */
	
	// set 4k cache, too small
	RunCommand.execute(getCommand("runSimpleJavaProgramWithPersistentCachePlusOptions","Foo","-Xscmx4k -Xnoaot"));
	checkOutputContains("JVMSHRC096I.*is full", "Did not see expected message about the cache being full");
	
	runDestroyAllCaches();
	
	// set 5000k cache, ok
	RunCommand.execute(getCommand("runSimpleJavaProgramWithPersistentCachePlusOptions","Foo","-Xscmx5000k -Xnoaot"));
	checkOutputDoesNotContain("JVMSHRC096I.*is full", "Did not expect message about the cache being full");

	runDestroyAllCaches();
	
	// set 8M cache, ok
	RunCommand.execute(getCommand("runSimpleJavaProgramWithPersistentCachePlusOptions","Foo","-Xscmx8M -Xnoaot"));
	checkOutputDoesNotContain("JVMSHRC096I.*is full", "Did not expect message about the cache being full");
	
	runDestroyAllCaches();
  }
}
