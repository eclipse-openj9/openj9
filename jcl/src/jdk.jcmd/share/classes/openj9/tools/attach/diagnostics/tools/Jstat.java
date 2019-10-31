/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

package openj9.tools.attach.diagnostics.tools;

import java.io.IOException;

import openj9.internal.tools.attach.target.AttachHandler;
import openj9.internal.tools.attach.target.DiagnosticProperties;
import openj9.internal.tools.attach.target.IPC;
import openj9.tools.attach.diagnostics.attacher.AttacherDiagnosticsProvider;

/**
 * JStat A tool for statistics monitoring of another Java process
 *
 */
@SuppressWarnings("nls")
public class Jstat {

	private static String vmid;
	private static String statOption;

	private static final String OPTION_CLASS = "-class";
	private static final String[] OPTIONS = { OPTION_CLASS };

	private static final String ERROR_AN_ARG_REQUIRED = "An argument is required";
	private static final String ERROR_INVALID_ARG = "An invalid argument";
	private static final String ERROR_INVALID_OPTION = "An invalid option";
	private static final String ERROR_INVALID_VMID = "Can't monitor this utility JVM itself: ";
	private static final String ERROR_NOT_EXIST_VMID = "No such process for vmid: ";
	private static final String ERROR_OPTION_REQUIRED = "An <option> is required";
	private static final String ERROR_VMID_REQUIRED = "A <vmid> is required";

	private static String HELPTEXT = "jstat: obtain statistics information about a Java process%n"
			+ " Usage:%n"
			+ "    jstat [<option>] [<vmid>]%n"
			+ "%n"
			+ "  option:%n"
			+ "   -J : supply arguments to the Java VM running jstat%n"
			+ "   -h : print this help message%n"
			+ "   -options : list the available command options%n"
			+ "   -class : Classloading statistics%n"
			+ "  <vmid>: Attach API VM ID as shown in jps or other Attach API-based tools%n"
			+ "NOTE: this utility might significantly affect the performance of the target VM.%n"
			+ "At least one option must be selected.%n";

	/**
	 * Print a list of Java processes and information about them.
	 * 
	 * @param args
	 *            Arguments to the application
	 */
	public static void main(String[] args) {
		if (parseArguments(args)) {
			if (vmid == null || vmid.isEmpty()) {
				// no valid vmid supplied, print error message and help text, and exit
				Util.exitJVMWithReasonAndHelp(ERROR_VMID_REQUIRED, HELPTEXT);
			}

			AttacherDiagnosticsProvider diagProvider = new AttacherDiagnosticsProvider();
			String myId = AttachHandler.getVmId();

			if (vmid.equals(myId)) {
				// this utility doesn't monitor itself, print error message and help text, and exit
				Util.exitJVMWithReasonAndHelp(ERROR_INVALID_VMID + vmid, HELPTEXT);
			}

			// if the ID looks like a process ID, check if it is running
			if (vmid.matches("\\d+")) {
				if (!IPC.processExists(Long.parseLong(vmid))) {
					Util.exitJVMWithReasonAndHelp(ERROR_NOT_EXIST_VMID + vmid, HELPTEXT);
				}
			}

			try {
				diagProvider.attach(vmid);
				Util.runCommandAndPrintResult(diagProvider, statOption, "jstat");
			} catch (Exception e) {
				System.err.printf("Error getting data from %s", vmid);
				final String msg = e.getMessage();
				if (msg == null) {
					System.err.println();
				} else {
					if (msg.matches(IPC.INCOMPATIBLE_JAVA_VERSION)) {
						System.err.println(": incompatible target JVM");
					} else {
						System.err.printf(": %s%n", msg);
					}
				}
				if (DiagnosticProperties.isDebug) {
					e.printStackTrace();
				}
			} finally {
				try {
					diagProvider.detach();
				} catch (IOException e) {
					// ignore
				}
			}
		}
	}

	private static boolean parseArguments(String[] args) {
		boolean foundStatOption = false;

		if ((args == null) || (args.length == 0)) {
			// no argument supplied, print error message and help text, and exit
			Util.exitJVMWithReasonAndHelp(ERROR_AN_ARG_REQUIRED, HELPTEXT);
		} else if (Util.checkHelpOption(args[0])) {
			// Help argument is supplied, print help text
			System.out.printf(HELPTEXT);
		} else if ("-options".equals(args[0])) {
			if (args.length > 1) {
				// there are more arguments after -options, print error message and help text, and exit
				Util.exitJVMWithReasonAndHelp(ERROR_INVALID_ARG, HELPTEXT);
			}
			// print options available
			for (String option : OPTIONS) {
				System.out.println(option);
			}
		} else {
			for (String arg : args) {
				if (arg.startsWith("-")) {
					if (statOption != null) {
						// one option has already been set, print error message and help text, and exit
						Util.exitJVMWithReasonAndHelp(ERROR_INVALID_ARG, HELPTEXT);
					} else {
						foundStatOption = true;
						switch (arg) {
						case OPTION_CLASS:
							statOption = "jstat.class";
							break;
						default:
							// invalid option was specified, print error message and help text, and exit
							Util.exitJVMWithReasonAndHelp(ERROR_INVALID_OPTION, HELPTEXT);
						}
					}
				} else {
					if (statOption == null) {
						// no option was specified, print error message and help text, and exit
						Util.exitJVMWithReasonAndHelp(ERROR_OPTION_REQUIRED, HELPTEXT);
					} else if (vmid != null) {
						// one vmid has already been set, print error message and help text, and exit
						Util.exitJVMWithReasonAndHelp(ERROR_INVALID_ARG, HELPTEXT);
					} else {
						vmid = arg;
					}
				}
			}
		}

		return foundStatOption;
	}
}
