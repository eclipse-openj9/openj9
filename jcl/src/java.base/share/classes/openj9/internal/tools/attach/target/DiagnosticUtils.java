/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
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

package openj9.internal.tools.attach.target;

import java.io.PrintWriter;
import java.io.StringWriter;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;
import java.util.function.Function;

import com.ibm.oti.vm.VM;

import openj9.internal.management.ClassLoaderInfoBaseImpl;
import openj9.management.internal.InvalidDumpOptionExceptionBase;
import openj9.management.internal.LockInfoBase;
import openj9.management.internal.ThreadInfoBase;

/**
 * Common methods for the diagnostics tools
 *
 */
public class DiagnosticUtils {

	private static final String FORMAT_PREFIX = " Format: "; //$NON-NLS-1$

	@SuppressWarnings("nls")
	private static final String FILE_PATH_HELP = " <file path>%n"
				+ " <file path> is optional, otherwise a default path/name is used."
				+ " Relative paths are resolved to the target's working directory.%n"
				+ " The dump agent may choose a different file path if the requested file exists.%n";

	/**
	 * Command strings for executeDiagnosticCommand()
	 */

	/**
	 * Print help text
	 */
	private static final String DIAGNOSTICS_HELP = "help"; //$NON-NLS-1$

	/**
	 * Run Get the stack traces and other thread information.
	 */
	private static final String DIAGNOSTICS_THREAD_PRINT = "Thread.print"; //$NON-NLS-1$

	/**
	 * Run System.gc();
	 */
	private static final String DIAGNOSTICS_GC_RUN = "GC.run"; //$NON-NLS-1$

	/**
	 * Get the heap object statistics.
	 */
	private static final String DIAGNOSTICS_GC_CLASS_HISTOGRAM = "GC.class_histogram"; //$NON-NLS-1$

	/**
	 * Commands to generate dumps of various types
	 */
	private static final String DIAGNOSTICS_DUMP_HEAP = "Dump.heap"; //$NON-NLS-1$
	private static final String DIAGNOSTICS_GC_HEAP_DUMP = "GC.heap_dump"; //$NON-NLS-1$
	private static final String DIAGNOSTICS_DUMP_JAVA = "Dump.java"; //$NON-NLS-1$
	private static final String DIAGNOSTICS_DUMP_SNAP = "Dump.snap"; //$NON-NLS-1$
	private static final String DIAGNOSTICS_DUMP_SYSTEM = "Dump.system"; //$NON-NLS-1$

	/**
	 * Get JVM statistics
	 */
	private static final String DIAGNOSTICS_STAT_CLASS = "jstat.class"; //$NON-NLS-1$
	
	/**
	 * Key for the command sent to executeDiagnosticCommand()
	 */
	public static final String COMMAND_STRING = "command_string"; //$NON-NLS-1$

	/**
	 * Use this to separate arguments in a diagnostic command string.
	 */
	public static final String DIAGNOSTICS_OPTION_SEPARATOR = ","; //$NON-NLS-1$

	/**
	 * Report live or all heap objects.
	 */
	private static final String ALL_OPTION = "all"; //$NON-NLS-1$
	private static final String LIVE_OPTION = "live"; //$NON-NLS-1$
	private static final String THREAD_LOCKED_SYNCHRONIZERS_OPTION = "-l"; //$NON-NLS-1$

	private static final Map<String, Function<String, DiagnosticProperties>> commandTable;
	private static final Map<String, String> helpTable;

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

	/**
	 * Convert the command line options into attach API diagnostic command format
	 * @param options List of command line arguments
	 * @param skip Number of options to omit from the beginning of the list
	 *
	 * @return formatted string
	 */
	public static String makeJcmdCommand(String[] options, int skip) {
		String cmd = String.join(DIAGNOSTICS_OPTION_SEPARATOR, Arrays.asList(options).subList(skip, options.length));
		return cmd;
	}

	private static native String getHeapClassStatisticsImpl();
	private static native String triggerDumpsImpl(String dumpOptions, String event) throws InvalidDumpOptionExceptionBase;

	/**
	 * Run a diagnostic command and return the result in a properties file
	 *
	 * @param diagnosticCommand String containing the command and options
	 * @return command result or diagnostic information in case of error
	 */
	static DiagnosticProperties executeDiagnosticCommand(String diagnosticCommand) {
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
		DiagnosticProperties result = null;
		boolean invalidArg = false;
		boolean doLive = false;
		String[] parts = diagnosticCommand.split(DIAGNOSTICS_OPTION_SEPARATOR);
		if (parts.length > 2) {
			invalidArg = true;
		} else if (parts.length == 2) {
			if (LIVE_OPTION.equalsIgnoreCase(parts[1])) {
				doLive = true;
			} else if (!ALL_OPTION.equalsIgnoreCase(parts[1])) {
				invalidArg = true;
			}
		}
		if (invalidArg) {
			result = DiagnosticProperties.makeErrorProperties("Command not recognized: " + diagnosticCommand); //$NON-NLS-1$
		} else {
			if (doLive) {
				runGC();
			}
			String hcsi = getHeapClassStatisticsImpl();
			String lineSeparator = System.lineSeparator();
			final String unixLineSeparator = "\n"; //$NON-NLS-1$
			if (!unixLineSeparator.equals(lineSeparator)) {
				hcsi = hcsi.replace(unixLineSeparator, lineSeparator);
			}
			result = DiagnosticProperties.makeStringResult(hcsi);
		}
		return result;
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

	private static DiagnosticProperties doDump(String diagnosticCommand) {
		DiagnosticProperties result = null;
		String[] parts = diagnosticCommand.split(DIAGNOSTICS_OPTION_SEPARATOR);
		IPC.logMessage("doDump: ", diagnosticCommand); //$NON-NLS-1$
		if (parts.length == 0 || parts.length > 2) {
			// The argument could be just Dump command which is going to use default configurations like -Xdump,
			// or there is an optional path/name argument for the dump file generated.
			result = DiagnosticProperties.makeErrorProperties("Error: wrong number of arguments"); //$NON-NLS-1$
		} else {
			String dumpType = ""; //$NON-NLS-1$
			/* handle legacy form of Dump.heap for compatibility with reference implementation */
			if (DIAGNOSTICS_GC_HEAP_DUMP.equals(parts[0])) {
				dumpType = "heap"; //$NON-NLS-1$
			} else {
				String[] dumpCommandAndType = parts[0].split("\\."); //$NON-NLS-1$
				if (dumpCommandAndType.length != 2) {
					result = DiagnosticProperties.makeErrorProperties(String.format("Error: invalid command %s", parts[0])); //$NON-NLS-1$
				} else {
					dumpType = dumpCommandAndType[1];
				}
			}
			if (!dumpType.isEmpty()) {
				String request = dumpType;
				if (parts.length == 2) {
					String fileDirective = ("system".equals(dumpType) && IPC.isZOS) ? ":dsn=" : ":file="; //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$
					request += fileDirective + parts[1];
				}
				try {
					String actualDumpFile = triggerDumpsImpl(request, dumpType + "DumpToFile"); //$NON-NLS-1$
					result = DiagnosticProperties.makeStringResult("Dump written to " + actualDumpFile); //$NON-NLS-1$
				} catch (InvalidDumpOptionExceptionBase e) {
					IPC.logMessage("doDump exception: ", e.getMessage()); //$NON-NLS-1$
					DiagnosticProperties.makeExceptionProperties(e);
				}
			}
		}
		return result;
	}

	private static native ThreadInfoBase[] dumpAllThreadsImpl(boolean lockedMonitors,
			boolean lockedSynchronizers, int maxDepth);

	private static DiagnosticProperties runGC() {
		VM.globalGC();
		return DiagnosticProperties.makeCommandSucceeded();
	}

	private static DiagnosticProperties getJstatClass(String diagnosticCommand) {
		IPC.logMessage("jstat command : ", diagnosticCommand); //$NON-NLS-1$
		StringWriter buffer = new StringWriter(100);
		PrintWriter bufferPrinter = new PrintWriter(buffer);
		bufferPrinter.println("Class Loaded    Class Unloaded"); //$NON-NLS-1$
		// "Class Loaded".length = 12, "Class Unloaded".length = 14
		bufferPrinter.printf("%12d    %14d%n", //$NON-NLS-1$
				Long.valueOf(ClassLoaderInfoBaseImpl.getLoadedClassCountImpl()),
				Long.valueOf(ClassLoaderInfoBaseImpl.getUnloadedClassCountImpl()));
		bufferPrinter.flush();
		return DiagnosticProperties.makeStringResult(buffer.toString());
	}
	
	private static DiagnosticProperties doHelp(String diagnosticCommand) {
		String[] parts = diagnosticCommand.split(DIAGNOSTICS_OPTION_SEPARATOR);
		/* print a list of the available commands */
		StringWriter buffer = new StringWriter(500);
		PrintWriter bufferPrinter = new PrintWriter(buffer);
		if (DIAGNOSTICS_HELP.equals(parts[0])) {
			if (parts.length == 1) {
				/* print a list of commands */
				commandTable.keySet().stream().sorted().forEach(s -> bufferPrinter.println(s));
			} else if (parts.length == 2) {
				String helpText = helpTable.getOrDefault(parts[1], "No help available"); //$NON-NLS-1$
				bufferPrinter.printf("%s: ", parts[1]); //$NON-NLS-1$
				bufferPrinter.printf(helpText);
			}
		} else {
			bufferPrinter.print("Invalid command: " + diagnosticCommand); //$NON-NLS-1$
		}
		return DiagnosticProperties.makeStringResult(buffer.toString());

	}

	/* Help strings for the jcmd utilities */
	@SuppressWarnings("nls")
	private static final String DIAGNOSTICS_HELP_HELP = "Show help for a command%n"
			+ FORMAT_PREFIX + " help <command>%n"
			+ " If no command is supplied, print the list of available commands on the target JVM.%n";

	@SuppressWarnings("nls")
	private static final String DIAGNOSTICS_GC_CLASS_HISTOGRAM_HELP = "Obtain heap information about a Java process%n"
			+ FORMAT_PREFIX + DIAGNOSTICS_GC_CLASS_HISTOGRAM + " [options]%n"
			+ " Options:%n"
			+ "          all : include all objects, including dead objects (this is the default option)%n"
			+ "         live : include all objects after a global GC collection%n"
			+ "NOTE: this utility might significantly affect the performance of the target VM.%n";

	@SuppressWarnings("nls")
	private static final String DIAGNOSTICS_GC_RUN_HELP = "Run the garbage collector.%n"
			+ FORMAT_PREFIX + DIAGNOSTICS_GC_RUN + "%n"
			+ "NOTE: this utility might significantly affect the performance of the target VM.%n";

	@SuppressWarnings("nls")
	private static final String DIAGNOSTICS_THREAD_PRINT_HELP = "List thread information.%n"
			+ FORMAT_PREFIX + DIAGNOSTICS_THREAD_PRINT + " [options]%n"
			+ " Options: -l : print information about ownable synchronizers%n";

	private static final String DIAGNOSTICS_DUMP_HEAP_HELP = "Create a heap dump.%n" //$NON-NLS-1$
			+ FORMAT_PREFIX + DIAGNOSTICS_DUMP_HEAP + FILE_PATH_HELP
			+ DIAGNOSTICS_GC_HEAP_DUMP + " is an alias for " + DIAGNOSTICS_DUMP_HEAP; //$NON-NLS-1$

	private static final String DIAGNOSTICS_DUMP_JAVA_HELP = "Create a javacore file.%n" //$NON-NLS-1$
			+ FORMAT_PREFIX + DIAGNOSTICS_DUMP_JAVA + FILE_PATH_HELP;

	private static final String DIAGNOSTICS_DUMP_SNAP_HELP = "Dump the snap trace buffer.%n" //$NON-NLS-1$
			+ FORMAT_PREFIX + DIAGNOSTICS_DUMP_SNAP + FILE_PATH_HELP;

	private static final String DIAGNOSTICS_DUMP_SYSTEM_HELP = "Create a native core file.%n" //$NON-NLS-1$
			+ FORMAT_PREFIX + DIAGNOSTICS_DUMP_SYSTEM + FILE_PATH_HELP;

	private static final String DIAGNOSTICS_JSTAT_CLASS_HELP = "Show JVM classloader statistics.%n" //$NON-NLS-1$
			+ FORMAT_PREFIX + DIAGNOSTICS_STAT_CLASS + "%n" //$NON-NLS-1$
			+ "NOTE: this utility might significantly affect the performance of the target VM.%n"; //$NON-NLS-1$

	/* Initialize the command and help text tables */
	static {
		commandTable = new HashMap<>();
		helpTable = new HashMap<>();

		commandTable.put(DIAGNOSTICS_HELP, DiagnosticUtils::doHelp);
		helpTable.put(DIAGNOSTICS_HELP, DIAGNOSTICS_HELP_HELP);

		commandTable.put(DIAGNOSTICS_GC_CLASS_HISTOGRAM, DiagnosticUtils::getHeapStatistics);
		helpTable.put(DIAGNOSTICS_GC_CLASS_HISTOGRAM, DIAGNOSTICS_GC_CLASS_HISTOGRAM_HELP);

		commandTable.put(DIAGNOSTICS_GC_RUN, s -> runGC());
		helpTable.put(DIAGNOSTICS_GC_RUN, DIAGNOSTICS_GC_RUN_HELP);

		commandTable.put(DIAGNOSTICS_THREAD_PRINT, DiagnosticUtils::getThreadInfo);
		helpTable.put(DIAGNOSTICS_THREAD_PRINT, DIAGNOSTICS_THREAD_PRINT_HELP);

		commandTable.put(DIAGNOSTICS_DUMP_HEAP, DiagnosticUtils::doDump);
		helpTable.put(DIAGNOSTICS_DUMP_HEAP, DIAGNOSTICS_DUMP_HEAP_HELP);

		commandTable.put(DIAGNOSTICS_GC_HEAP_DUMP, DiagnosticUtils::doDump);
		helpTable.put(DIAGNOSTICS_GC_HEAP_DUMP, DIAGNOSTICS_DUMP_HEAP_HELP);

		commandTable.put(DIAGNOSTICS_DUMP_JAVA, DiagnosticUtils::doDump);
		helpTable.put(DIAGNOSTICS_DUMP_JAVA, DIAGNOSTICS_DUMP_JAVA_HELP);

		commandTable.put(DIAGNOSTICS_DUMP_SNAP, DiagnosticUtils::doDump);
		helpTable.put(DIAGNOSTICS_DUMP_SNAP, DIAGNOSTICS_DUMP_SNAP_HELP);

		commandTable.put(DIAGNOSTICS_DUMP_SYSTEM, DiagnosticUtils::doDump);
		helpTable.put(DIAGNOSTICS_DUMP_SYSTEM, DIAGNOSTICS_DUMP_SYSTEM_HELP);
		
		commandTable.put(DIAGNOSTICS_STAT_CLASS, DiagnosticUtils::getJstatClass);
		helpTable.put(DIAGNOSTICS_STAT_CLASS, DIAGNOSTICS_JSTAT_CLASS_HELP);
	}
}
