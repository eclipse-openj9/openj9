package tests.sharedclasses.options;

import tests.sharedclasses.TestUtils;
import com.ibm.oti.shared.*;
import java.util.*;
import java.util.regex.*;

/*
 * Tests to check functionality of -Xshareclasses:enableBCI sub-option.
 * To check the functionality, it needs an JVMTI agent that registers for JVMTI_EVENT_CLASS_FILE_LOAD_HOOK.
 * This is achieved by using JVMTI agent present in com.ibm.jvmti.tests package.
 * The JVMTI test used is ecflh001.c.
 */
public class TestSharedCacheEnableBCI extends TestUtils {
	private final static String cacheName = "testSharedCacheEnableBCI";
	private static final String BOOTSTRAP_CLASS = "openj9/internal/tools/attach/target/IPC";
	private static final String NON_BOOTSTRAP_CLASS = "SimpleApp";
	private static final String agentFailureMsg = "Invalid class file bytes for class";
	private static final String jvmLevel = System.getProperty("java.specification.version");
	
	/*
	 * Number of ROMClasses stored in cache during startup varies across VM
	 * versions. However, this number is always comfortably more than 250. So,
	 * it should be safe to assume atleast 250 classes are stored in the cache
	 * during startup. If ever number of ROMClasses stored in cache during
	 * startup goes below MIN_EXPECTED_ROMCLASSES, this constant would need to
	 * be updated accordingly.
	 */
	private static final int MIN_EXPECTED_ROMCLASSES = 250;
	private static int CLASSS_FILE_LOAD_HOOK_COUNT = 250;

	public static void main(String args[]) {
		String jvmLevel = System.getProperty("java.specification.version");
		/* CLASSS_FILE_LOAD_HOOK_COUNT is different in Java8 and Java9 */
		if (Double.parseDouble(jvmLevel) >= 9) {
			CLASSS_FILE_LOAD_HOOK_COUNT = 20;
		}
		runDestroyAllCaches();
		runTest1();
		runTest2();
		runTest3();
		runTest4();
		runTest5();
		runDestroyAllCaches();
	}

	/**
	 * 1. Create a cache without an agent and with disableBCI option.
	 * <p>
	 * 2. Create a cache without an agent and with enablBCI option.
	 * <p>
	 * 3. Create a cache with an agent that does not modify class file data and
	 * with enableBCI option.
	 * <p>
	 * Verify that number of ROMClasses are same in all three cases.
	 */
	static void runTest1() {
		String printStatsOutput[], output[];
		int romClasses = 0, romClassesWithEnableBCI = 0;

		/*
		 * CMVC 186357 : This is to provide some leeway for number of ROMClasses
		 * stored in shared cache by different invocations of JVM.
		 */
		int romClassNumberMargin = 10;

		if (isMVS() == false) {
			/* Create a cache without an agent and with disableBCI option. */
			runHanoiProgramWithCache(cacheName,"disableBCI",null);
			checkFileExistsForPersistentCache(cacheName);
			runPrintStats(cacheName, true);

			printStatsOutput = getLastCommandStderr();
			if (printStatsOutput != null) {
				for (int i = 0; i < printStatsOutput.length; i++) {
					if (printStatsOutput[i].matches("# ROMClasses.*$")) {
						String outputLine[] = printStatsOutput[i].split("=");
						romClasses = Integer.parseInt(outputLine[1].trim());
					}
				}
			}
			if (romClasses == 0) {
				fail("# ROMClasses statement not found in printStats output");
			}

			destroyPersistentCache(cacheName);
			checkFileDoesNotExistForPersistentCache(cacheName);

			if (romClasses < MIN_EXPECTED_ROMCLASSES) {
				fail("Number of ROMClasses found: " + romClasses
						+ ". Minimum expected ROMClasses: "
						+ MIN_EXPECTED_ROMCLASSES);
			}

			/* Create a cache without an agent and with enablBCI option. */
			runHanoiProgramWithCache(cacheName,"enableBCI",null);
			checkFileExistsForPersistentCache(cacheName);
			runPrintStats(cacheName, true);

			printStatsOutput = getLastCommandStderr();
			if (printStatsOutput != null) {
				for (int i = 0; i < printStatsOutput.length; i++) {
					if (printStatsOutput[i].matches("# ROMClasses.*$")) {
						String outputLine[] = printStatsOutput[i].split("=");
						romClassesWithEnableBCI = Integer
								.parseInt(outputLine[1].trim());
					}
				}
			}
			if (romClassesWithEnableBCI == 0) {
				fail("# ROMClasses statement not found in printStats output");
			}

			destroyPersistentCache(cacheName);
			checkFileDoesNotExistForPersistentCache(cacheName);

			if (romClassesWithEnableBCI < MIN_EXPECTED_ROMCLASSES) {
				fail("Number of ROMClasses found: " + romClassesWithEnableBCI
						+ ". Minimum expected ROMClasses: "
						+ MIN_EXPECTED_ROMCLASSES);
			}

			if (Math.abs(romClasses - romClassesWithEnableBCI) > romClassNumberMargin) {
				fail("Mismatch in number of ROMClasses. Without enableBCI, # ROMClasses: "
						+ romClasses
						+ "With enableBCI, # ROMClasses: "
						+ romClassesWithEnableBCI
						+ ".\nMaximum allowed difference is "
						+ romClassNumberMargin);
			}

			/*
			 * Create a cache with an agent that does not modify class file data
			 * and with enableBCI option.
			 */
			runHanoiProgramWithCache(cacheName,"enableBCI","-agentlib:jvmtitest=test:ecflh001,args:noModify");
			output = getLastCommandStderr();
			checkAgentOutputForFailureMsg(output);
			checkFileExistsForPersistentCache(cacheName);
			runPrintStats(cacheName, true);

			romClassesWithEnableBCI = 0;

			printStatsOutput = getLastCommandStderr();
			if (printStatsOutput != null) {
				for (int i = 0; i < printStatsOutput.length; i++) {
					if (printStatsOutput[i].matches("# ROMClasses.*$")) {
						String outputLine[] = printStatsOutput[i].split("=");
						romClassesWithEnableBCI = Integer
								.parseInt(outputLine[1].trim());
					}
				}
			}
			if (romClassesWithEnableBCI == 0) {
				fail("# ROMClasses statement not found in printStats output");
			}

			destroyPersistentCache(cacheName);
			checkFileDoesNotExistForPersistentCache(cacheName);

			if (romClassesWithEnableBCI < MIN_EXPECTED_ROMCLASSES) {
				fail("Number of ROMClasses found: " + romClassesWithEnableBCI
						+ ". Minimum expected ROMClasses: "
						+ MIN_EXPECTED_ROMCLASSES);
			}
			if (Math.abs(romClasses - romClassesWithEnableBCI) > romClassNumberMargin) {
				fail("Mismatch in number of ROMClasses. Without enableBCI, # ROMClasses: "
						+ romClasses
						+ "With enableBCI, # ROMClasses: "
						+ romClassesWithEnableBCI
						+ ".\nMaximum allowed difference is "
						+ romClassNumberMargin);
			}
		}
		
		/* Create a cache without an agent and with disableBCI option. */
		runHanoiProgramWithCache(cacheName,"nonpersistent,disableBCI",null);
		checkFileExistsForNonPersistentCache(cacheName);
		runPrintStats(cacheName, false);

		printStatsOutput = getLastCommandStderr();
		if (printStatsOutput != null) {
			for (int i = 0; i < printStatsOutput.length; i++) {
				if (printStatsOutput[i].matches("# ROMClasses.*$")) {
					String outputLine[] = printStatsOutput[i].split("=");
					romClasses = Integer.parseInt(outputLine[1].trim());
				}
			}
		}
		if (romClasses == 0) {
			fail("# ROMClasses statement not found in printStats output");
		}

		destroyNonPersistentCache(cacheName);
		checkFileDoesNotExistForNonPersistentCache(cacheName);

		if (romClasses < MIN_EXPECTED_ROMCLASSES) {
			fail("Number of ROMClasses found: " + romClasses
					+ ". Minimum expected ROMClasses: "
					+ MIN_EXPECTED_ROMCLASSES);
		}

		/* Create a cache without an agent and with enablBCI option. */
		runHanoiProgramWithCache(cacheName,"nonpersistent,enableBCI",null);
		checkFileExistsForNonPersistentCache(cacheName);
		runPrintStats(cacheName, false);

		romClassesWithEnableBCI = 0;

		printStatsOutput = getLastCommandStderr();
		if (printStatsOutput != null) {
			for (int i = 0; i < printStatsOutput.length; i++) {
				if (printStatsOutput[i].matches("# ROMClasses.*$")) {
					String outputLine[] = printStatsOutput[i].split("=");
					romClassesWithEnableBCI = Integer.parseInt(outputLine[1]
							.trim());
				}
			}
		}
		if (romClassesWithEnableBCI == 0) {
			fail("# ROMClasses statement not found in printStats output");
		}

		destroyNonPersistentCache(cacheName);
		checkFileDoesNotExistForNonPersistentCache(cacheName);

		if (romClassesWithEnableBCI < MIN_EXPECTED_ROMCLASSES) {
			fail("Number of ROMClasses found: " + romClassesWithEnableBCI
					+ ". Minimum expected ROMClasses: "
					+ MIN_EXPECTED_ROMCLASSES);
		}
		if (Math.abs(romClasses - romClassesWithEnableBCI) > romClassNumberMargin) {
			fail("Mismatch in number of ROMClasses. Without enableBCI, # ROMClasses: "
					+ romClasses
					+ "With enableBCI, # ROMClasses: "
					+ romClassesWithEnableBCI
					+ ".\nMaximum allowed difference is "
					+ romClassNumberMargin);
		}

		/*
		 * Create a cache wtih an agent that does not modify class file data and
		 * with enableBCI option.
		 */
		runHanoiProgramWithCache(cacheName,"nonpersistent,enableBCI","-agentlib:jvmtitest=test:ecflh001,args:noModify");
		output = getLastCommandStderr();
		checkAgentOutputForFailureMsg(output);
		checkFileExistsForNonPersistentCache(cacheName);
		runPrintStats(cacheName, false);

		romClassesWithEnableBCI = 0;

		printStatsOutput = getLastCommandStderr();
		if (printStatsOutput != null) {
			for (int i = 0; i < printStatsOutput.length; i++) {
				if (printStatsOutput[i].matches("# ROMClasses.*$")) {
					String outputLine[] = printStatsOutput[i].split("=");
					romClassesWithEnableBCI = Integer.parseInt(outputLine[1]
							.trim());
				}
			}
		}
		if (romClassesWithEnableBCI == 0) {
			fail("# ROMClasses statement not found in printStats output");
		}

		destroyNonPersistentCache(cacheName);
		checkFileDoesNotExistForNonPersistentCache(cacheName);

		if (romClassesWithEnableBCI < MIN_EXPECTED_ROMCLASSES) {
			fail("Number of ROMClasses found: " + romClassesWithEnableBCI
					+ ". Minimum expected ROMClasses: "
					+ MIN_EXPECTED_ROMCLASSES);
		}
		if (Math.abs(romClasses - romClassesWithEnableBCI) > romClassNumberMargin) {
			fail("Mismatch in number of ROMClasses. Without enableBCI, # ROMClasses: "
					+ romClasses
					+ "With enableBCI, # ROMClasses: "
					+ romClassesWithEnableBCI
					+ ".\nMaximum allowed difference is "
					+ romClassNumberMargin);
		}
	}

	/**
	 * 1. Create a cache with enableBCI sub-option using an agent that modifies
	 * java/lang/Class. Verify that it does not appear in printStats=romclass
	 * output.
	 * <p>
	 * 2. Use same cache as in 1, but without an agent. Verify that
	 * java/lang/Class is added in the cache.
	 */
	static void runTest2() {
		testHelper(BOOTSTRAP_CLASS, "modifyBootstrap=" + BOOTSTRAP_CLASS);
	}

	/**
	 * 1. Create a cache with enableBCI sub-option using an agent that modifies
	 * non-bootstrap class like SimpleApp. Verify that it does not appear in
	 * printStats=romclass output.
	 * <p>
	 * 2. Use same cache as in 1, but without an agent. Verify that SimpleApp is
	 * added in the cache.
	 */
	static void runTest3() {
		testHelper(NON_BOOTSTRAP_CLASS, "modifyNonBootstrap="
				+ NON_BOOTSTRAP_CLASS);
	}

	/**
	 * Create a cache with enableBCI sub-option using a JVMTI agent. JVMTI agent
	 * prints a message on ClassFileLoadHook event. Verify that number of times
	 * ClassFileLoadHook event message printed is close to number of classes
	 * stored in the cache.
	 */
	static void runTest4() {
		int romClassesWithEnableBCI = 0;
		int classFileLoadHookCount = 0;
		boolean found = false;
		String output[];
		int difference = 50;

		if (isMVS() == false) {
			runSimpleJavaProgramWithAgentWithPersistentCache(cacheName,
					"enableBCI", "ecflh001", "noModify+printClassFileLoadMsg");
			output = getLastCommandStderr();
			checkAgentOutputForFailureMsg(output);
			checkFileExistsForPersistentCache(cacheName);
			/* Count number of times ClassFileLoadHook event was triggered */
			if (output != null) {
				classFileLoadHookCount = 0;
				String expectedOutput = "ClassFileLoadHook event called for class:.*$";
				for (int i = 0; i < output.length; i++) {
					if (output[i].matches(expectedOutput)) {
						classFileLoadHookCount += 1;
					}
				}
			}

			runPrintStats(cacheName, true, "romclass");
			output = getLastCommandStderr();
			if (output != null) {
				found = false;
				for (int i = 0; i < output.length; i++) {
					if (output[i].matches("# ROMClasses.*$")) {
						String romClassLine[] = output[i].split("=");
						romClassesWithEnableBCI = Integer
								.parseInt(romClassLine[1].trim());
						found = true;
						break;
					}
				}
				if (false == found) {
					fail("# ROMClasses statement not found in printStats output");
				}
			}

			destroyPersistentCache(cacheName);
			checkFileDoesNotExistForPersistentCache(cacheName);

			if (romClassesWithEnableBCI < MIN_EXPECTED_ROMCLASSES) {
				fail("Number of ROMClasses found: " + romClassesWithEnableBCI
						+ ". Minimum expected ROMClasses: "
						+ MIN_EXPECTED_ROMCLASSES);
			}
			if (classFileLoadHookCount < CLASSS_FILE_LOAD_HOOK_COUNT) {
				fail("Number of times ClassFileLoadHook event triggered: "
						+ classFileLoadHookCount + ". Expected it to be >= "
						+ CLASSS_FILE_LOAD_HOOK_COUNT);
			}
			if (Double.parseDouble(jvmLevel) < 9) {
				/*
				 * For some classes like java.lang.J9VMInternals, ClassFileLoadHook
				 * is not triggered. Some difference in number of ROMClasses in cache
				 * and number of times ClassFileLoadHook gets triggered is expected.
				 */
				if (Math.abs(romClassesWithEnableBCI - classFileLoadHookCount) > difference) {
					fail("Difference between number of ROMClasses ("
							+ romClassesWithEnableBCI + ") in cache and "
							+ "number of times ClassFileLoadHook event triggered ("
							+ classFileLoadHookCount + ")is not as expected");
				}
			}
		}

		runSimpleJavaProgramWithAgentWithNonPersistentCache(cacheName,
				"enableBCI", "ecflh001", "noModify+printClassFileLoadMsg");
		output = getLastCommandStderr();
		checkAgentOutputForFailureMsg(output);
		checkFileExistsForNonPersistentCache(cacheName);
		/* Count number of times ClassFileLoadHook event was triggered */
		if (output != null) {
			classFileLoadHookCount = 0;
			String expectedOutput = "ClassFileLoadHook event called for class:.*$";
			for (int i = 0; i < output.length; i++) {
				if (output[i].matches(expectedOutput)) {
					classFileLoadHookCount += 1;
				}
			}
		}

		runPrintStats(cacheName, false, "romclass");
		output = getLastCommandStderr();
		if (output != null) {
			found = false;
			for (int i = 0; i < output.length; i++) {
				if (output[i].matches("# ROMClasses.*$")) {
					String romClassLine[] = output[i].split("=");
					romClassesWithEnableBCI = Integer.parseInt(romClassLine[1]
							.trim());
					found = true;
					break;
				}
			}
			if (false == found) {
				fail("# ROMClasses statement not found in printStats output");
			}
		}

		destroyNonPersistentCache(cacheName);
		checkFileDoesNotExistForNonPersistentCache(cacheName);

		if (romClassesWithEnableBCI < MIN_EXPECTED_ROMCLASSES) {
			fail("Number of ROMClasses found: " + romClassesWithEnableBCI
					+ ". Minimum expected ROMClasses: "
					+ MIN_EXPECTED_ROMCLASSES);
		}
		if (classFileLoadHookCount < CLASSS_FILE_LOAD_HOOK_COUNT) {
			fail("Number of times ClassFileLoadHook event triggered: "
					+ classFileLoadHookCount + ". Expected it to be >= "
					+ CLASSS_FILE_LOAD_HOOK_COUNT);
		}
		if (Double.parseDouble(jvmLevel) < 9) {
		/*
		 * For some classes like java.lang.J9VMInternals, ClassFileLoadHook is
		 * not triggered Some difference in number of ROMClasses in cache and
		 * number of times ClassFileLoadHook gets triggered is expected
		 */
			if (Math.abs(romClassesWithEnableBCI - classFileLoadHookCount) > difference) {
				fail("Difference between number of ROMClasses ("
						+ romClassesWithEnableBCI + ") in cache and "
						+ "number of times ClassFileLoadHook event triggered ("
						+ classFileLoadHookCount + ")is not as expected");
			}
		}
	}

	/**
	 * Create a cache with 'enableBCI' sub-option but without using any jvmti
	 * agent. Re-use the cache with jvmti agent that modifies the class files
	 * and verifies that class file bytes passed to ClassFileLoadHoook event
	 * callback represent valid class file.
	 */
	static void runTest5() {
		String output[];

		if (isMVS() == false) {
			runSimpleJavaProgramWithPersistentCache(cacheName, "enableBCI");
			checkFileExistsForPersistentCache(cacheName);
			
			runSimpleJavaProgramWithAgentWithPersistentCache(cacheName,
					"enableBCI", "ecflh001", "modifyAll");
			output = getLastCommandStderr();
			checkAgentOutputForFailureMsg(output);
			destroyPersistentCache(cacheName);
			checkFileDoesNotExistForPersistentCache(cacheName);
		}

		runSimpleJavaProgramWithNonPersistentCache(cacheName, "enableBCI");
		checkFileExistsForNonPersistentCache(cacheName);
		
		runSimpleJavaProgramWithAgentWithNonPersistentCache(cacheName,
				"enableBCI", "ecflh001", "modifyAll");
		output = getLastCommandStderr();
		checkAgentOutputForFailureMsg(output);
		destroyNonPersistentCache(cacheName);
		checkFileDoesNotExistForNonPersistentCache(cacheName);
	}
	
	static void testHelper(String classToModify, String modType) {
		if (isMVS() == false) {
			runSimpleJavaProgramWithAgentWithPersistentCache(cacheName,
					"enableBCI", "ecflh001", modType);
			checkFileExistsForPersistentCache(cacheName);
			runPrintStats(cacheName, true, "romclass");
			checkOutputDoesNotContain("^.*ROMCLASS: " + classToModify
					+ " at.*$", "Did not expect " + classToModify
					+ " in the cache.");

			runSimpleJavaProgramWithPersistentCache(cacheName, "enableBCI");
			checkFileExistsForPersistentCache(cacheName);
			runPrintStats(cacheName, true, "romclass");
			checkOutputContains("^.*ROMCLASS: " + classToModify + " at.*$",
					classToModify + " not found in the cache");

			destroyPersistentCache(cacheName);
			checkFileDoesNotExistForPersistentCache(cacheName);
		}

		runSimpleJavaProgramWithAgentWithNonPersistentCache(cacheName,
				"enableBCI", "ecflh001", modType);
		checkFileExistsForNonPersistentCache(cacheName);
		runPrintStats(cacheName, false, "romclass");
		checkOutputDoesNotContain("^.*ROMCLASS: " + classToModify + " at.*$",
				"Did not expect " + classToModify + " in the cache.");

		runSimpleJavaProgramWithNonPersistentCache(cacheName, "enableBCI");
		checkFileExistsForNonPersistentCache(cacheName);
		runPrintStats(cacheName, false, "romclass");
		checkOutputContains("^.*ROMCLASS: " + classToModify + " at.*$",
				classToModify + " not found in the cache");

		destroyNonPersistentCache(cacheName);
		checkFileDoesNotExistForNonPersistentCache(cacheName);
	}
	
	static void checkAgentOutputForFailureMsg(String output[]) {
		if (output != null) {
			for (int i = 0; i < output.length; i++) {
				if (output[i].indexOf(agentFailureMsg) != -1) {
					fail("ClassFileLoadHoook event callback: " + output[i]);
				}
			}
		}
	}
}