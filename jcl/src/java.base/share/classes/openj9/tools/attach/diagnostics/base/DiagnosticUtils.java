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

package openj9.tools.attach.diagnostics.base;

import java.io.PrintWriter;
import java.io.StringWriter;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;
import java.util.function.Function;
import java.util.function.Predicate;

import com.ibm.oti.vm.VM;
import com.ibm.tools.attach.target.IPC;

import openj9.management.internal.LockInfoBase;
import openj9.management.internal.ThreadInfoBase;

/**
 * Common methods for the diagnostics tools
 *
 */
public class DiagnosticUtils {

	/**
	 * Command strings for executeDiagnosticCommand()
	 */

	/**
	 * Run Get the stack traces and other thread information.
	 */
	public static final String DIAGNOSTICS_THREAD_PRINT = "Thread.print"; //$NON-NLS-1$

	/**
	 * Run System.gc();
	 */
	public static final String DIAGNOSTICS_GC_RUN = "GC.run"; //$NON-NLS-1$

	/**
	 * Get the heap object statistics.
	 */
	public static final String DIAGNOSTICS_GC_CLASS_HISTOGRAM = "GC.class_histogram"; //$NON-NLS-1$

	/**
	 * Key for the command sent to executeDiagnosticCommand()
	 */
	public static final String COMMAND_STRING = "command_string"; //$NON-NLS-1$

	/**
	 * Use this to separate arguments in a diagnostic command string.
	 */
	public static final String DIAGNOSTICS_OPTION_SEPARATOR = ","; //$NON-NLS-1$

	/**
	 * Report only live heap objects.
	 */
	public static final String LIVE_OPTION = "live"; //$NON-NLS-1$
	private static final String THREAD_LOCKED_SYNCHRONIZERS_OPTION = "-l"; //$NON-NLS-1$

	private static final Map<String, Function<String, DiagnosticProperties>> commandTable;

	/**
	 * Create the command to run the heapHisto command
	 * 
	 * @param liveObjects add the option to run a GC before listing the objects
	 * @return formatted string
	 */
	public static String makeHeapHistoCommand(boolean liveObjects) {
		String cmd = DIAGNOSTICS_GC_CLASS_HISTOGRAM;
		if (liveObjects) {
			cmd = DIAGNOSTICS_GC_CLASS_HISTOGRAM + DIAGNOSTICS_OPTION_SEPARATOR + LIVE_OPTION;
		}
		return cmd;
	}

	/**
	 * Create the command to run the Thread.print command
	 * 
	 * @param lockedSynchronizers print the locked ownable synchronizers
	 * @return formatted string
	 */
	public static String makeThreadPrintCommand(boolean lockedSynchronizers) {
		String cmd = lockedSynchronizers
				? DIAGNOSTICS_THREAD_PRINT + DIAGNOSTICS_OPTION_SEPARATOR + THREAD_LOCKED_SYNCHRONIZERS_OPTION
				: DIAGNOSTICS_THREAD_PRINT;
		return cmd;
	}

	private static native String getHeapClassStatisticsImpl();

	/**
	 * Run a diagnostic command and return the result in a properties file
	 * 
	 * @param diagnosticCommand String containing the command and options
	 * @return command result or diagnostic information in case of error
	 */
	public static DiagnosticProperties executeDiagnosticCommand(String diagnosticCommand) {
		IPC.logMessage("executeDiagnosticCommand: ", diagnosticCommand); //$NON-NLS-1$

		DiagnosticProperties result;
		String[] commandRoot = diagnosticCommand.split(DiagnosticUtils.DIAGNOSTICS_OPTION_SEPARATOR);
		Function<String, DiagnosticProperties> cmd = commandTable.get(commandRoot[0]);
		if (null == cmd) {
			result = DiagnosticProperties.makeStatusProperties(true,
					"Command " + diagnosticCommand + " not recognized"); //$NON-NLS-1$ //$NON-NLS-2$
		} else {
			result = cmd.apply(diagnosticCommand);
			result.put(DiagnosticUtils.COMMAND_STRING, diagnosticCommand);
		}
		return result;
	}

	private static DiagnosticProperties getHeapStatistics(String diagnosticCommand) {
		boolean doLive = Arrays.stream(diagnosticCommand.split(DiagnosticUtils.DIAGNOSTICS_OPTION_SEPARATOR))
				.anyMatch(Predicate.isEqual(DiagnosticUtils.LIVE_OPTION));
		if (doLive) {
			runGC();
		}
		String result = getHeapClassStatisticsImpl();
		String lineSeparator = System.lineSeparator();
		final String unixLineSeparator = "\n"; //$NON-NLS-1$
		if (!unixLineSeparator.equals(lineSeparator)) {
			result = result.replace(unixLineSeparator, lineSeparator);
		}
		return DiagnosticProperties.makeStringResult(result);
	}
	
	private static DiagnosticProperties getThreadInfo(String diagnosticCommand) {
		DiagnosticProperties result = null;
		boolean okay = true;
		boolean addSynchronizers = false;
		String[] parts = diagnosticCommand.split(DIAGNOSTICS_OPTION_SEPARATOR);
		/* only one option ("-l[=<BOOLEAN>]") is allowed, so the diagnosticCommand can comprise at most
		 * the base command and one option, with an optional value */
		if (parts.length > 2) {
			okay = false;
		} else if (parts.length == 2) {
			String option = parts[1];
			if (option.startsWith(THREAD_LOCKED_SYNCHRONIZERS_OPTION)) {
				if ((THREAD_LOCKED_SYNCHRONIZERS_OPTION.length() == option.length()) /* exact match */
						|| option.toLowerCase().equals(THREAD_LOCKED_SYNCHRONIZERS_OPTION + "=true")) { //$NON-NLS-1$
					addSynchronizers = true;
				}
			} else {
				okay = false;
			}
		}
		if (!okay) {
			result = DiagnosticProperties.makeErrorProperties("Command not recognized: " + diagnosticCommand); //$NON-NLS-1$
		} else {
			StringWriter buffer = new StringWriter(2000);
			PrintWriter bufferPrinter = new PrintWriter(buffer);
			bufferPrinter.println(System.getProperty("java.vm.info")); //$NON-NLS-1$
			bufferPrinter.println();
			ThreadInfoBase[] threadInfoBases = dumpAllThreadsImpl(true, addSynchronizers, Integer.MAX_VALUE);
			for (ThreadInfoBase currentThreadInfoBase : threadInfoBases) {
				bufferPrinter.print(currentThreadInfoBase.toString());
				if (addSynchronizers) {
					LockInfoBase[] lockedSynchronizers = currentThreadInfoBase.getLockedSynchronizers();
					bufferPrinter.printf("%n\tLocked ownable synchronizers: %d%n", //$NON-NLS-1$
							Integer.valueOf(lockedSynchronizers.length));
					for (LockInfoBase currentLockedSynchronizer : lockedSynchronizers) {
						bufferPrinter.printf("\t- %s%n", currentLockedSynchronizer.toString()); //$NON-NLS-1$
					}
				}
				bufferPrinter.println();
			}
			bufferPrinter.flush();
			result = DiagnosticProperties.makeStringResult(buffer.toString());
		}
		return result;
	}

	private static native ThreadInfoBase[] dumpAllThreadsImpl(boolean lockedMonitors,
			boolean lockedSynchronizers, int maxDepth);

	private static DiagnosticProperties runGC() {
		VM.globalGC();
		return DiagnosticProperties.makeCommandSucceeded();
	}

	static {
		commandTable = new HashMap<>();
		commandTable.put(DIAGNOSTICS_GC_CLASS_HISTOGRAM, DiagnosticUtils::getHeapStatistics);
		commandTable.put(DIAGNOSTICS_GC_RUN, s -> runGC());
		commandTable.put(DIAGNOSTICS_THREAD_PRINT, DiagnosticUtils::getThreadInfo);
	}

}
