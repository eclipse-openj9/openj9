package tests.sharedclasses.options;

import tests.sharedclasses.RunCommand;
import tests.sharedclasses.TestUtils;

/*
 * Check 'silent' hides output
 */
public class TestCommandLineOptionSilent01 extends TestUtils {
  public static void main(String[] args) {	
	runDestroyAllCaches();

	String cachename="$%u"; // invalid
	
    // Need to do a bit of a hack-job to ensure this particular command isnt run with verbose on
	String command = getCommand(RunSimpleJavaProgramWithNonPersistentCache,cachename);

	// this should give the error
	RunCommand.execute(command,false);
	checkOutputContains("JVMSHRC147E", "expected message about '$' not being valid for cache name");

	command = command.replaceAll("verbose", "silent");
	
	// this should not give the error
	RunCommand.execute(command,false);
	checkOutputDoesNotContain("JVMSHRC147E", "expected message about '$' not being valid for cache name");
	
	runDestroyAllCaches();
  }
}
