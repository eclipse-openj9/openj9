/*******************************************************************************
 * Copyright (c) 2016, 2021 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
package com.ibm.jvm.ras.tests;

import java.util.Arrays;

import junit.framework.TestCase;

import com.ibm.jvm.DumpConfigurationUnavailableException;
import com.ibm.jvm.InvalidDumpOptionException;

@SuppressWarnings("nls")
public class DumpAPIQuerySetReset extends TestCase {

	@Override
	protected void setUp() throws Exception {
		super.setUp();
	}

	@Override
	protected void tearDown() throws Exception {
		super.tearDown();
	}

	private static void clearDumpOptions() {
		String initialDumpOptions[] = com.ibm.jvm.Dump.queryDumpOptions();

		try {
			com.ibm.jvm.Dump.setDumpOptions("none");
		} catch (InvalidDumpOptionException idoe) {
			idoe.printStackTrace();
		} catch (NullPointerException e) {
			e.printStackTrace();
		} catch (DumpConfigurationUnavailableException e) {
			e.printStackTrace();
		}

		String afterNoneOptions[] = com.ibm.jvm.Dump.queryDumpOptions();

		if (0 != afterNoneOptions.length) {
			fail("Expected an empty array of dump options but got: " + Arrays.toString(afterNoneOptions));
		}
	}

	public void testSetNone() {
		String initialDumpOptions[] = com.ibm.jvm.Dump.queryDumpOptions();

		try {
			com.ibm.jvm.Dump.setDumpOptions("none");
		} catch (InvalidDumpOptionException idoe) {

		} catch (DumpConfigurationUnavailableException e) {
			e.printStackTrace();
		}

		String afterNoneOptions[] = com.ibm.jvm.Dump.queryDumpOptions();

		if (0 != afterNoneOptions.length) {
			fail("Expected an empty array of dump options but got: " + Arrays.toString(afterNoneOptions));
		}
	}

	public void testSetWhat() {
		// clearDumpOptions();
		try {
			com.ibm.jvm.Dump.resetDumpOptions();
			try {
				com.ibm.jvm.Dump.setDumpOptions("what");
			} catch (InvalidDumpOptionException idoe) {
				fail("-Xdump:what was expected to be permitted");
			}
		} catch (DumpConfigurationUnavailableException e) {
			e.printStackTrace();
		}
		// -Xdump:what doesn't do anything but it would just be irritating if people couldn't
		// use it to quickly print all available options.
	}

	// testQuery - set loads of options to get a huge buffer, query, check we get all the options back as expected
	public void testQuery() {
		clearDumpOptions();
		String[] dumpTypes = { "java", "system", "heap", "tool", "console", "stack", "silent", "jit" };
		String[] dumpEvents = { "systhrow", "uncaught", "gpf", "traceassert", "user", "abort", "vmstop" };
		// The following events are omitted from the list above as they can affect the behaviour of the VM under test,
		// e.g. "thrstart" will cause a dump to be triggered if another thread starts, and hence lock the dump configuration
		// at an arbitrary point in time: "vmstart", "throw", "systhrow", "catch", "uncaught", "thrstart", "thrstop"
		String[] dumpFilters = { "java/lang/OutOfMemoryError", "java/lang/NullPointerException", "made/up/Exception1",
				"made/up/Exception2", "made/up/Exception3", "made/up/Exception4", "made/up/Exception5",
				"made/up/Exception6", "made/up/Exception7" };

		int count = populateDumpAgents(dumpTypes, dumpEvents, dumpFilters);

		String dumpAgents[] = com.ibm.jvm.Dump.queryDumpOptions();

		assertNotNull("Expected array of dump agents returned, not null", dumpAgents);
		assertEquals("Expected " + count + " dump agents configured.", count, dumpAgents.length);

		// Can probably have one function to validate two list of options match and re-use in lots of places.

		int foundCount = checkDumpAgents(dumpTypes, dumpEvents, dumpFilters, dumpAgents);
		assertEquals("Expected to find all the expected agents.", dumpAgents.length, foundCount);
		clearDumpOptions();
	}

	/**
	 * Use the settings we are passed to generate all those combinations of dump
	 * agents.
	 * The arrays must be the same size as the arrays passed to populateDumpAgents,
	 * use null for any items which have been removed.
	 * @param dumpTypes
	 * @param dumpEvents
	 * @param dumpFilters
	 * @param dumpAgents
	 * @return
	 */
	private static int checkDumpAgents(String[] dumpTypes, String[] dumpEvents, String[] dumpFilters, String[] dumpAgents) {
		int checkCount = 0;
		int foundCount = 0;
		// System.err.println("Running checkDumpAgents...");
		for (String type : dumpTypes) {
			for (String event : dumpEvents) {
				String eventPattern = "events=" + event;
				for (String filter : dumpFilters) {
					// Find a dump type agent for each line we added. Can't do an exact match as
					// the agents add paths and re-order arguments.
					String filterPattern = "filter=" + filter;
					// 'label' is still accepted in options, but no longer used in reports
					String labelPattern = ".*\\b(dsn|exec|file)=.*" + type + checkCount + ".*";
					checkCount++;
					if (type == null || event == null || filter == null) {
						// This permutation should have been removed, still
						// need to increment checkCount to kep the labels matching.
						continue;
					}
					// System.err.println("Checking: " + type);
					// System.err.println("Checking: " + eventPattern);
					// System.err.println("Checking: " + filterPattern);
					// System.err.println("Checking: " + labelPattern);
					for (String agentString : dumpAgents) {
						// System.err.println("Agent checked is: " + agentString);
						if (!agentString.startsWith(type)) {
							continue;
						}
						if (!agentString.contains(eventPattern)) {
							continue;
						}
						if (!agentString.contains(filterPattern)) {
							continue;
						}
						if (!agentString.matches(labelPattern)) {
							continue;
						}
						foundCount++;
						// System.err.println("found: " + agentString);
					}
				}
			}
		}
		// System.err.println("Checked: " + checkCount);
		return foundCount;
	}

	/**
	 * Use the settings we are passed to generate all combinations of dump agents.
	 */
	private static int populateDumpAgents(String[] dumpTypes, String[] dumpEvents, String[] dumpFilters) {
		int count = 0;

		for (String type : dumpTypes) {
			for (String event : dumpEvents) {
				for (String filter : dumpFilters) {
					// Create agents the dump system can't merge.
					String options = type + ":events=" + event + ",filter=" + filter + ",label=" + type + count++;
					try {
						com.ibm.jvm.Dump.setDumpOptions(options);
					} catch (InvalidDumpOptionException e) {
						e.printStackTrace();
						fail("Expected my dump options to work but got an InvalidDumpOptionException: " + e.getMessage());
					} catch (DumpConfigurationUnavailableException e) {
						e.printStackTrace();
						fail("Expected my dump options to work but got an DumpConfigurationUnavailableException: " + e.getMessage());
					}
				}
			}
		}

		return count;
	}

	// testSetNone<Type> - test all dumps of particular types can be removed.
	public void testSetNoneByType() {
		clearDumpOptions();

		String[] dumpTypes = { "java", "system", "heap", "tool", "console", "stack", "silent", "jit" };
		String[] dumpEvents = { "systhrow", "uncaught" }; // { "throw", "systhrow", "catch", "uncaught" };
		String[] dumpFilters = { "java/lang/OutOfMemoryError", "java/lang/NullPointerException", "made/up/Exception1",
				"made/up/Exception2", "made/up/Exception3", "made/up/Exception4", "made/up/Exception5",
				"made/up/Exception6", "made/up/Exception7" };

		int count = populateDumpAgents(dumpTypes, dumpEvents, dumpFilters);

		String dumpAgents[] = com.ibm.jvm.Dump.queryDumpOptions();

		assertNotNull("Expected array of dump agents returned, not null", dumpAgents);
		assertEquals("Expected " + count + " dump agents configured.", count, dumpAgents.length);

		String removalType = "heap";
		try {
			com.ibm.jvm.Dump.setDumpOptions(removalType + ":none");
		} catch (InvalidDumpOptionException e) {
			fail("Expected to be able to remove all dump agents of type: " + removalType);
		} catch (DumpConfigurationUnavailableException e) {
			fail("Expected to be able to remove all dump agents of type: " + removalType);
		}

		String[] afterAgents = com.ibm.jvm.Dump.queryDumpOptions();
		// System.out.println("Found " + afterAgents.length + " agents");
		// for (String agent : afterAgents) {
		//     System.out.println("After: " + agent);
		// }

		// Validate the list of agents.
		int foundCount = checkDumpAgents(
				new String[] { "java", "system", null, "tool", "console", "stack", "silent", "jit" },
				dumpEvents, dumpFilters, afterAgents);
		assertEquals("Expected to find all the expected agents.", afterAgents.length, foundCount);
	}

	// testSetNone<Options> - test all dumps with certain options can be disabled
	public void testSetNoneByOptions() {
		clearDumpOptions();
		String[] dumpTypes = { "java", "system", "heap", "console", "stack", "silent", "jit" };
		String[] dumpEvents = { "systhrow", "uncaught" }; // { "throw", "systhrow", "catch", "uncaught" };
		String[] dumpFilters = { "java/lang/OutOfMemoryError", "java/lang/NullPointerException", "made/up/Exception1",
				"made/up/Exception2", "made/up/Exception3", "made/up/Exception4", "made/up/Exception5",
				"made/up/Exception6", "made/up/Exception7" };

		int count = populateDumpAgents(dumpTypes, dumpEvents, dumpFilters);

		String dumpAgents[] = com.ibm.jvm.Dump.queryDumpOptions();

		assertNotNull("Expected array of dump agents returned, not null", dumpAgents);
		assertTrue("Expected " + count + " dump agents configured.", dumpAgents.length == count);

		try {
			com.ibm.jvm.Dump.setDumpOptions("java+system+heap+console+stack+silent+jit:none:events=uncaught");
			com.ibm.jvm.Dump.setDumpOptions("java+system+heap+console+stack+silent+jit:none:filter=made/up/Exception3");
			com.ibm.jvm.Dump.setDumpOptions("java+system+heap+console+stack+silent+jit:none:filter=made/up/Exception5");
		} catch (InvalidDumpOptionException e) {
			fail("Expected to be able to remove all dump agents by filter");
		} catch (DumpConfigurationUnavailableException e) {
			fail("Expected to be able to remove all dump agents by filter. Dump configuration was unavailable.");
		}

		String[] afterAgents = com.ibm.jvm.Dump.queryDumpOptions();
		// System.out.println("Found " + afterAgents.length + " agents");
		// for (String agent : afterAgents) {
		//     System.out.println("After: " + agent);
		// }

		// Validate the list of agents.
		int foundCount = checkDumpAgents(dumpTypes,
				new String[] { "systhrow", null },
				new String[] {
						"java/lang/OutOfMemoryError",
						"java/lang/NullPointerException", "made/up/Exception1",
						"made/up/Exception2", null, "made/up/Exception4", null,
						"made/up/Exception6", "made/up/Exception7" },
				afterAgents);
		assertEquals("Expected to find all the expected agents.", afterAgents.length, foundCount);
	}

	// testSetNone<Type><Options> - test all dumps of particular types with certain options can be disabled.
	//  public void testSetNoneByTypeAndOptions() {
	//      clearDumpOptions();
	//      String[] dumpTypes = {"java", "system", "heap", "console", "stack", "silent", "jit"};
	//      String[] dumpEvents = {"systhrow", "uncaught"};//{"throw", "systhrow", "catch", "uncaught"};
	//      String[] dumpFilters = {"java/lang/OutOfMemoryError", "java/lang/NullPointerException", "made/up/Exception1", "made/up/Exception2",
	//              "made/up/Exception3", "made/up/Exception4", "made/up/Exception5", "made/up/Exception6", "made/up/Exception7"};
	//
	//      int count = populateDumpAgents(dumpTypes, dumpEvents, dumpFilters );
	//
	//      String dumpAgents[] = com.ibm.jvm.Dump.queryDumpOptions();
	//      assertNotNull("Expected array of dump agents returned, not null", dumpAgents);
	//      assertTrue("Expected " + count + " dump agents configured.", dumpAgents.length == count);
	//
	//      try {
	//          com.ibm.jvm.Dump.setDumpOptions("console:none:events=systhrow");
	//          com.ibm.jvm.Dump.setDumpOptions("console:none:filter=made/up/Exception2");
	//          com.ibm.jvm.Dump.setDumpOptions("console:none:filter=made/up/Exception7");
	//      } catch (InvalidDumpOptionException e) {
	//          fail("Expected to be able to remove all dump agents by filter");
	//      }
	//
	//      String[] afterAgents = com.ibm.jvm.Dump.queryDumpOptions();
	//      System.out.println("Found " + afterAgents.length + " agents");
	//      for( String agent: afterAgents ) {
	//          System.out.println("After: " + agent);
	//      }
	//
	//      // Validate the list of agents.
	//      int foundCount = checkDumpAgents(new String[] {"java", "system", "heap", null, "stack", "silent", "jit"},
	//              new String[] { null, "uncaught" }, new String[] {
	//                      "java/lang/OutOfMemoryError",
	//                      "java/lang/NullPointerException", "made/up/Exception1",
	//                      null, "made/up/Exception3", "made/up/Exception4", "made/up/Exception5",
	//                      "made/up/Exception6", null },
	//              afterAgents);
	//      assertEquals("Expected to find all the expected agents.", afterAgents.length, foundCount );
	//  }

	// testReset - reset, query, clear with -Xdump:none (check cleared), reset again and query to see if we have the original settings
	public void testReset() {
		// Set the options back to the defaults.
		clearDumpOptions();

		try {
			com.ibm.jvm.Dump.resetDumpOptions();
		} catch (DumpConfigurationUnavailableException e) {
			fail("Dump configuration was unavailable.");
		}

		String originalOptions[] = com.ibm.jvm.Dump.queryDumpOptions();

		// Check there were some default dump options, we don't really need to
		// know what they were just that there were some.
		assertTrue("Expected some dump options after reset.", originalOptions.length > 0);

		clearDumpOptions(); // This asserts that the options are empty.

		try {
			com.ibm.jvm.Dump.resetDumpOptions();
		} catch (DumpConfigurationUnavailableException e) {
			fail("Dump configuration was unavailable.");
		}

		String afterResetOptions[] = com.ibm.jvm.Dump.queryDumpOptions();

		assertEquals("Expected the same number of options setup before and after reset.",
				originalOptions.length, afterResetOptions.length);
		assertTrue("Expected contents of options to be equal before and after reset.",
				Arrays.equals(originalOptions, afterResetOptions));
	}

	// testRoundTrip - reset options, query them, clear with -Xdump:none, pass back original strings to set, query and check we get back what we started with, reset and check again.
	public void testRoundTrip() {
		// Set the options back to the defaults.
		clearDumpOptions();

		try {
			com.ibm.jvm.Dump.resetDumpOptions();
		} catch (DumpConfigurationUnavailableException e) {
			fail("Dump configuration was unavailable.");
		}

		String originalOptions[] = com.ibm.jvm.Dump.queryDumpOptions();

		// Check there were some default dump options, we don't really need to
		// know what they were just that there were some.
		assertTrue("Expected some dump options after reset.", originalOptions.length > 0);

		clearDumpOptions(); // This asserts that the options are empty.

		// Reset the options by resubmitting the result of queryDumpOptions
		for (String agent : originalOptions) {
			try {
				com.ibm.jvm.Dump.setDumpOptions(agent);
			} catch (InvalidDumpOptionException e) {
				fail("Expected to be able to set dump options to the strings returned by query dump options.");
			} catch (DumpConfigurationUnavailableException e) {
				fail("Dump configuration was unavailable.");
			}
		}

		String afterResetOptions[] = com.ibm.jvm.Dump.queryDumpOptions();

		assertEquals("Expected the same number of options setup before and after reset.",
				originalOptions.length, afterResetOptions.length);
		assertTrue("Expected contents of options to be equal before and after reset.",
				Arrays.equals(originalOptions, afterResetOptions));
	}

	// testSet<Type> - set up a dump to trigger on some event, cause the event and check we have a dump
	// testSetToolDump - confirm tool dumps are blocked, in several ways.
	//  public void testSetToolDump() {
	//      clearDumpOptions();
	//
	//      String toolAgents[] = {"tool", "tool:events=gpf", "java+tool", "tool+java", "java+tool+heap",
	//              "tool+java:events=gpf", "java+tool:events=systhrow", "java+tool+heap:events=traceassert",
	//              " tool", "\ntool", "TOOL"};
	//
	//      for( String agent: toolAgents ) {
	//          try {
	//              com.ibm.jvm.Dump.setDumpOptions(agent);
	//              fail("Did not expect to be able to set a tool dump agent, found: " + Arrays.toString(com.ibm.jvm.Dump.queryDumpOptions()));
	//          } catch (InvalidDumpOptionException e) {
	//              // Pass.
	//              // Check exception message.
	//          }
	//          // Check we didn't set things anyway.
	//          String newOptions[] = com.ibm.jvm.Dump.queryDumpOptions();
	//          assertTrue("Expected no options to be set.", newOptions.length == 0);
	//      }
	//  }

	// testSetBadOptions - pass in garbage options.
	public void testSetBadOptions() {
		clearDumpOptions();

		String badAgents[] = {
			//  "",                                         // Empty String
				"hippo",                                    // Bad dump type
				"java+hippo",                               // Bad second dump type
				"java:hippo=gpf",                           // Bad dump option
				"java:events=gpf,request=hippo",            // Bad sub option
				"java:events=gpf\nheap:events=systhrow",    // Bad specification of two dumps
				"java:events=hippo",                        // Bad event
				"java:request=hippo",                       // Bad request
				"java:priority=hippo",                      // Bad priority, should be a number
				"defaults",                                 // Setting defaults, not valid?
			//  "java:defaults:priority=500",               // Setting defaults, not valid?
				"help",                                     // Not valid at runtime
				"events",                                   // Not valid at runtime
				"request",                                  // Not valid at runtime
				"tokens",                                   // Not valid at runtime
				"nofailover",                               // Not valid at runtime - possibly should be!

// Not valid, but not parsed yet.               "java:events=vmstop,filter=hippo",          // Bad filter (vmstop takes a number)
// Not valid, not handled right.
				"java:events=throw,filter=not/a/real/Exception",        // Bad option, uses event that needs -Xdump:dynamic on the command line
				"java:events=vmstart+throw,filter=not/a/real/Exception" // Bad option, uses event that needs -Xdump:dynamic on the command line

// Valid        "java:events=gpf,options=CLASSIC+PHD", // Options belonging to another dump type.
// Valid        "heap+java:events=gpf,opts=CLASSIC+PHD", // Options belonging to only one dump type.
		};

		try {
			// System.err.println("Trying: null");
			com.ibm.jvm.Dump.setDumpOptions(null);
			fail("Did not expect to be able to set a bad dump agent, found: "
					+ Arrays.toString(com.ibm.jvm.Dump.queryDumpOptions()));
		} catch (NullPointerException e) {
			// Pass.
			// Check exception message.
		} catch (InvalidDumpOptionException e) {
			fail("Expected a null pointer exception");
		} catch (DumpConfigurationUnavailableException e) {
			fail("Dump configuration was unavailable.");
		}

		for (String agent : badAgents) {
			try {
				// System.err.println("Trying: " + agent);
				com.ibm.jvm.Dump.setDumpOptions(agent);
				com.ibm.jvm.Dump.setDumpOptions("what"); // We should not reach this so the output will be useful for debugging if we do.
				fail("Did not expect to be able to set a bad dump option: " + agent + ", queryDumpOptions() found: "
						+ Arrays.toString(com.ibm.jvm.Dump.queryDumpOptions()));
			} catch (InvalidDumpOptionException e) {
				// Pass.
				// Check exception message.
			} catch (DumpConfigurationUnavailableException e) {
				fail("Dump configuration was unavailable.");
			}
			// Check we didn't set things anyway.
			String newOptions[] = com.ibm.jvm.Dump.queryDumpOptions();
			if (newOptions.length > 0) {
				// System.err.println(Arrays.toString(newOptions));
				try {
					com.ibm.jvm.Dump.setDumpOptions("what");
				} catch (InvalidDumpOptionException e) {
					// TODO Auto-generated catch block
					e.printStackTrace();
				} catch (DumpConfigurationUnavailableException e) {
					fail("Dump configuration was unavailable.");
				}
			}
			assertEquals("Expected no options to be set for agent string - " + agent, 0, newOptions.length);
		}
	}

	// testSetDynamic - I don't think this should work at runtime.
	public void testSetDynamic() {
		try {
			com.ibm.jvm.Dump.setDumpOptions("dynamic");
			fail("Did not expect to be able to set -Xdump:dynamic at runtime.");
		} catch (InvalidDumpOptionException e) {
			// Pass.
			// Check exception message.
		} catch (DumpConfigurationUnavailableException e) {
			fail("Dump configuration was unavailable.");
		}
	}

	// testSetNullOptions - I don't think this should work at runtime.
	public void testSetNullOptions() {
		try {
			com.ibm.jvm.Dump.setDumpOptions(null);
			fail("Did not expect to be able to do setDumpOptions(null)");
		} catch (NullPointerException e) {
			// Pass.
		} catch (InvalidDumpOptionException e) {
			fail("Expected NPE to be thrown not InvalidDumpOptionException");
		} catch (DumpConfigurationUnavailableException e) {
			fail("Dump configuration was unavailable.");
		}
	}

	// testSetDynamicOnly - This shouldn't work at runtime unless -Xdynamic was specified at startup.
	public void testSetDynamicOnlyOptions() {
		clearDumpOptions();

		String[] dumpTypes = { "java", "system", "heap", "console", "stack", "silent", "jit" };
		String[] dumpEvents = { "catch", "throw", "vmstart" }; // vmstart is not fixed with -Xdump:dynamic but it can't be set at runtime.
		String[] dumpFilters = { "java/lang/OutOfMemoryError", "java/lang/NullPointerException", "made/up/Exception1",
				"made/up/Exception2", "made/up/Exception3", "made/up/Exception4", "made/up/Exception5",
				"made/up/Exception6", "made/up/Exception7" };

		// None of these should work since uncaught and throw cannot be set at runtime unless -Xdump:dynamic was on the
		// command line.
		int count = 0;
		for (String type : dumpTypes) {
			for (String event : dumpEvents) {
				for (String filter : dumpFilters) {
					// Create agents the dump system can't merge.
					String options = type + ":events=" + event + ",filter=" + filter + ",label=" + type + count++;
					try {
						com.ibm.jvm.Dump.setDumpOptions(options);
						fail("Expected not to be ablel to set these dump options: " + options);
					} catch (InvalidDumpOptionException e) {
						// TODO Auto-generated catch block
					} catch (DumpConfigurationUnavailableException e) {
						fail("Dump configuration was unavailable.");
					}
				}
			}
		}

		String dumpAgents[] = com.ibm.jvm.Dump.queryDumpOptions();

		assertNotNull("Expected array of dump agents returned, not null", dumpAgents);
		assertEquals("Expected 0 dump agents configured found " + dumpAgents.length, 0, dumpAgents.length);

		// Can probably have one function to validate two list of options match and re-use in lots of places.

		int foundCount = checkDumpAgents(dumpTypes, dumpEvents, dumpFilters, dumpAgents);

		assertEquals("Expected to find all the expected agents.", dumpAgents.length, foundCount);

		clearDumpOptions();
	}

	/**
	 * Set the default options.
	 */
	public void testSetDefaults() {
		// Set the options back to the defaults.
		clearDumpOptions(); // This asserts that the options are empty.

		try {
			com.ibm.jvm.Dump.setDumpOptions("java:defaults:opts=HIPPO");
			com.ibm.jvm.Dump.setDumpOptions("java");
		} catch (DumpConfigurationUnavailableException e) {
			fail("Dump configuration was unavailable.");
		} catch (InvalidDumpOptionException e) {
			fail("Expected setting the defaults to be a valid option.");
		}

		String afterDefaultsSetOptions[] = com.ibm.jvm.Dump.queryDumpOptions();

		assertEquals("Expected only one option to be setup.", 1, afterDefaultsSetOptions.length);

		String option = afterDefaultsSetOptions[0];

		assertTrue("Expected the option string to contain opts=HIPPO and java:",
				option.startsWith("java:") && option.contains("opts=HIPPO"));
	}

	/**
	 * Set the default options and check that setting them hasn't affected
	 * the options we return to after reset.
	 */
	public void testSetDefaultsAndReset() {
		// Set the options back to the defaults.
		clearDumpOptions();

		try {
			com.ibm.jvm.Dump.resetDumpOptions();
		} catch (DumpConfigurationUnavailableException e) {
			fail("Dump configuration was unavailable.");
		}

		String originalOptions[] = com.ibm.jvm.Dump.queryDumpOptions();

		// Check there were some default dump options, we don't really need to
		// know what they were just that there were some.
		assertTrue("Expected some dump options after reset.", originalOptions.length > 0);

		clearDumpOptions(); // This asserts that the options are empty.

		try {
			com.ibm.jvm.Dump.setDumpOptions("java:defaults:opts=HIPPO");
			com.ibm.jvm.Dump.setDumpOptions("java");
		} catch (DumpConfigurationUnavailableException e) {
			fail("Dump configuration was unavailable.");
		} catch (InvalidDumpOptionException e) {
			fail("Expected setting the defaults to be a valid option.");
		}

		String afterDefaultsSetOptions[] = com.ibm.jvm.Dump.queryDumpOptions();

		assertEquals("Expected only one option to be setup.", 1, afterDefaultsSetOptions.length);

		String option = afterDefaultsSetOptions[0];

		assertTrue("Expected the option string to contain opts=HIPPO and java:",
				option.startsWith("java:") && option.contains("opts=HIPPO"));

		/* Now check reseting takes us back where it should do. */
		try {
			com.ibm.jvm.Dump.resetDumpOptions();
		} catch (DumpConfigurationUnavailableException e) {
			fail("Dump configuration was unavailable.");
		}

		String afterResetOptions[] = com.ibm.jvm.Dump.queryDumpOptions();

		assertEquals("Expected the same number of options setup before and after reset.",
				originalOptions.length, afterResetOptions.length);
		assertTrue("Expected contents of options to be equal before and after reset.",
				Arrays.equals(originalOptions, afterResetOptions));
	}

}
