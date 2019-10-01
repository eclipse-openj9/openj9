package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;

/*
 * Check the 'name=' option 
 * - name longer than 64chars
 */
public class TestCommandLineOptionName04 extends TestUtils {
  public static void main(String[] args) {	
	runDestroyAllCaches();

	StringBuffer name= new StringBuffer();
	for (int i=0;i<10;i++) {
		name.append("thisIsAString");
	}

	runSimpleJavaProgramWithNonPersistentCache(name.toString(),false);
	checkOutputContains("JVMSHRC061E","Did not get message about name of length greater than 64 chars not allowed");

	runDestroyAllCaches();
  }
}
