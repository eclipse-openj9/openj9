package tests.sharedclasses.options;

import tests.sharedclasses.RunCommand;
import tests.sharedclasses.TestUtils;

/*
 * Check 'silent' overrides verbose
 */
public class TestCommandLineOptionSilent02 extends TestUtils {
  public static void main(String[] args) {	
	runDestroyAllCaches();

	String cachename="$%u"; // invalid
	
    // Need to do a bit of a hack-job to ensure this particular command isnt run with verbose on
	String command = getCommand(RunSimpleJavaProgramWithNonPersistentCache,cachename);
	
	// this should not give the error
	RunCommand.execute(command,false);
	checkOutputContains("JVMSHRC147E", "expected message about '$' not being valid for cache name");
	
	command = getCommand(RunSimpleJavaProgramWithNonPersistentCache,cachename);
	// this should not give the error
	command = command.replaceAll("verbose", "verbose,silent");
	RunCommand.execute(command,false);
	checkOutputDoesNotContain("JVMSHRC147E", "did not expect message about '$' being valid for cache name");
	
	runDestroyAllCaches();
  }
}
