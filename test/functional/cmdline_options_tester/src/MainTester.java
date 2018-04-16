/*******************************************************************************
 * Copyright (c) 2004, 2018 IBM Corp. and others
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

import com.oti.j9.exclude.ExcludeList;

/**
 * This is the entry point for the command-line testing application. It is not meant to be
 * initialized; simply run.
 */
public class MainTester {
	/**
	 * The main method parses the arguments and checks them for validity. If they are not valid,
	 * it displays usage information and exits. If they are valid, the command-line testing
	 * application is initialized with the given configuration settings and run.
	 * @param args Command-line arguments
	 */
	public static void main( String[] args ) {
		// variables to read arguments into
		String configFile = null;
		String excludeFile = null;
		String excludes = null;
		boolean explain = false;
		boolean debugCmdOnTimeout = true;
		String platforms = null;
		boolean verbose = false;
		boolean nonZeroExitCodeWhenError = false;
		String outputLimitString = null;
		String modeHints = null;
		int outputLimit = -1;
		
		for (int i = 0; i < args.length; i++) {
			if (args[i].equalsIgnoreCase("-explainExcludes")) {
				explain = true;
			} else if (args[i].equalsIgnoreCase("-verbose")) {
				verbose = true;
			} else if (args[i].equalsIgnoreCase("-nonZeroExitWhenError")) {
				nonZeroExitCodeWhenError = true;
			} else if (i < args.length - 1) {
				if (args[i].equalsIgnoreCase("-config")) {
					configFile = args[++i];
				} else if (args[i].equalsIgnoreCase("-xlist")) {
					excludeFile = args[++i];
				} else if (args[i].equalsIgnoreCase("-xids")) {
					excludes = args[++i];
				} else if (args[i].equalsIgnoreCase("-plats")) {
					platforms = args[++i];
				} else if (args[i].equalsIgnoreCase("-outputLimit")) {
					outputLimitString = args[++i];
				} else if (args[i].equalsIgnoreCase("-disableDebugOnTimeout")) {
					debugCmdOnTimeout = false;
				}
			}
		}
		
		if (configFile == null) {
			// error
			System.out.println( "ERROR: The required -config parameter was not found\n" );
			displayHelp();
			return;
		}
		
		if (null != outputLimitString) {
			try {
				outputLimit = Integer.parseInt(outputLimitString);
			} catch(NumberFormatException e) {
				System.out.println("ERROR: Invalid -outputLimit value : " + outputLimitString +", integer is expected");
				displayHelp();
				return;
			}
		}

		// all good - set up and run the suite
		if (excludes == null) {
			excludes = "";
		}
		ExcludeList excludeList = null;
		if (excludeFile != null) {
			excludeList = ExcludeList.readFrom( excludeFile, excludes );
			if (explain) {
				excludeList.explain( System.out );
			}
		}
		modeHints = (String)System.getProperties().get("MODE_HINTS");
		TestConfigParser parser = new TestConfigParser();
		parser.setVerbose( verbose );
		parser.setOutputLimit(outputLimit);
		TestSuite suite = parser.runTests( configFile, excludeList, platforms, debugCmdOnTimeout, modeHints);
		suite.printResults();
		/* If there were test failures exit with a non-zero return code to force an error in the makefiles. 
		 * Otherwise just die naturally. 
		 */
		if ((true == nonZeroExitCodeWhenError) && (0 < suite.getFailureCount())) {
			System.exit(2);
		}
	}
	
	/**
	 * Display usage information.
	 */
	private static void displayHelp() {
		System.out.println( "Usage: MainTester -config xxx [options]" );
		System.out.println( "   xxx: The configuration file from which to read test cases" );
		System.out.println( "[options]:" );
		System.out.println( "   -xlist yyy         The exclude file from which to read which test cases" );
		System.out.println( "                      are excluded" );
		System.out.println( "   -xids zzz          The comma-delimited list of platforms whose excludes" );
		System.out.println( "                      should be taken into account (if -xlist is specified)" );
		System.out.println( "   -explainExcludes   Print out the details of excluded test cases" );
		System.out.println( "                      Default: disabled" );
		System.out.println( "   -plats ppp         The platforms to enable conditional tests on. Comma-");
		System.out.println( "                      separated values are allowed (e.g. -plats j9win,j9lnx)");
		System.out.println( "                      Default: none; enable only tests with no platform");
		System.out.println( "                      dependency" );
		System.out.println( "   -outputLimit n Max number of lines to be printed for each test case.");
		System.out.println( "                      By default, all of the output from test case is printed,");
		System.out.println( "                      unless max number of lines specified by this option.");
		System.out.println( "Examples:" );
		System.out.println( "   MainTester -config j2jcmdlineopts.xml -xlist j2jexcludes.xml -xids all,j9lnx" );
		System.out.println( "   MainTester -config j9.xml" );
		System.out.println( "   MainTester -config a.xml -xlist b.xml -xids all -plats a,b,c\n" );
		System.out.println( "   MainTester -config a.xml -xlist b.xml -xids all -plats a,b,c\n -outputLimit 300" );
	}
}
