/*[INCLUDE-IF JAVA_SPEC_VERSION >= 8]*/
/*
 * Copyright IBM Corp. and others 2019
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */

package openj9.internal.tools.attach.target;

import java.io.PrintWriter;
import java.io.StringWriter;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;
import java.util.Timer;
import java.util.TimerTask;
import java.util.concurrent.TimeUnit;
import java.util.function.Function;

import com.ibm.oti.vm.VM;

/*[IF CRAC_SUPPORT]*/
import openj9.internal.criu.InternalCRIUSupport;
/*[ENDIF] CRAC_SUPPORT */
import openj9.internal.management.ClassLoaderInfoBaseImpl;
import openj9.management.internal.IDCacheInitializer;
import openj9.management.internal.InvalidDumpOptionExceptionBase;
import openj9.management.internal.LockInfoBase;
import openj9.management.internal.ThreadInfoBase;

/**
 * Common methods for the diagnostics tools
 *
 */
@SuppressWarnings("nls")
public class DiagnosticUtils {

	private static final String FORMAT_PREFIX = " Format: ";
	private static final String SYNTAX_PREFIX = "Syntax : ";

	private static final String HEAP_DUMP_OPTION_HELP = " [request=<options>] [opts=<options>] [<file path>]%n"
			+ " Set optional request= and opts= -Xdump options. The order of the parameters does not matter.%n";

	private static final String OTHER_DUMP_OPTION_HELP = " [request=<options>] [<file path>]%n"
				+ " Set optional request= -Xdump options. The order of the parameters does not matter.%n";

	private static final String HEAPSYSTEM_DUMP_OPTION_HELP =
			" system and heap dumps default to request=exclusive+prepwalk rather than the -Xdump:<type>:defaults setting.%n";

	private static final String GENERIC_DUMP_OPTION_HELP =
			" <file path> is optional, otherwise a default path/name is used.%n"
			+ " Relative paths are resolved to the target's working directory.%n"
			+ " The dump agent may choose a different file path if the requested file exists.%n";

/*[IF JFR_SUPPORT]*/
	private static String jfrRecordingFileName = "defaultJ9recording.jfr";
	private static final String JFR_START_OPTION_HELP =
			" [options]%n"
			+ "%n"
			+ "Options:%n"
			+ "%n"
			+ "duration     (Optional) Length of time to record. Note that 0s means forever.%n"
			+ "             (INTEGER followed by 's' for seconds 'm' for minutes or 'h' for hours)%n"
			+ "%n"
			+ "filename     (Optional) Name of the file to which the flight recording data is%n"
			+ "              written when the recording is stopped.%n";

	private static final String JFR_STOP_OPTION_HELP =
			" [options]%n"
			+ "%n"
			+ "Options:%n"
			+ "%n"
			+ "filename     (Optional) Name of the file to which the recording is written%n"
			+ "              when the recording is stopped.%n";

	private static final String JFR_DUMP_OPTION_HELP =
			" [options]%n"
			+ "%n"
			+ "Options:%n"
			+ "%n"
			+ "filename        (Optional) Name of the file to which the flight recording data is written.%n";
/*[ENDIF] JFR_SUPPORT */

	/**
	 * Command strings for executeDiagnosticCommand()
	 */

	/**
	 * Print help text
	 */
	private static final String DIAGNOSTICS_HELP = "help";

	/**
	 * Run Get the stack traces and other thread information.
	 */
	private static final String DIAGNOSTICS_THREAD_PRINT = "Thread.print";

/*[IF CRAC_SUPPORT]*/
	/**
	 * Generate a checkpoint via CRIUSupport using a compatability name.
	 */
	private static final String DIAGNOSTICS_JDK_CHECKPOINT = "JDK.checkpoint";
/*[ENDIF] CRAC_SUPPORT */

	/**
	 * Run System.gc();
	 */
	private static final String DIAGNOSTICS_GC_RUN = "GC.run";

	/**
	 * Get the heap object statistics.
	 */
	private static final String DIAGNOSTICS_GC_CLASS_HISTOGRAM = "GC.class_histogram";

	/**
	 * Commands to generate dumps of various types
	 */
	private static final String DIAGNOSTICS_DUMP_HEAP = "Dump.heap";
	private static final String DIAGNOSTICS_GC_HEAP_DUMP = "GC.heap_dump";
	private static final String DIAGNOSTICS_DUMP_JAVA = "Dump.java";
	private static final String DIAGNOSTICS_DUMP_SNAP = "Dump.snap";
	private static final String DIAGNOSTICS_DUMP_SYSTEM = "Dump.system";

/*[IF JFR_SUPPORT]*/
	/**
	 * Commands for JFR start, stop and dump
	 */
	private static final String DIAGNOSTICS_JFR_START = "JFR.start";
	private static final String DIAGNOSTICS_JFR_DUMP = "JFR.dump";
	private static final String DIAGNOSTICS_JFR_STOP = "JFR.stop";
/*[ENDIF] JFR_SUPPORT */

	/**
	 * Get JVM statistics
	 */
	private static final String DIAGNOSTICS_STAT_CLASS = "jstat.class";

	// load JVMTI agent
	private static final String DIAGNOSTICS_LOAD_JVMTI_AGENT = "JVMTI.agent_load";

	/**
	 * Key for the command sent to executeDiagnosticCommand()
	 */
	public static final String COMMAND_STRING = "command_string";

	/**
	 * Use this to separate arguments in a diagnostic command string.
	 */
	public static final String DIAGNOSTICS_OPTION_SEPARATOR = ",";

	/**
	 * Report live or all heap objects.
	 */
	private static final String ALL_OPTION = "all";
	private static final String LIVE_OPTION = "live";
	private static final String THREAD_LOCKED_SYNCHRONIZERS_OPTION = "-l";

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
		IPC.logMessage("executeDiagnosticCommand: ", diagnosticCommand);

		DiagnosticProperties result;
		String[] commandRoot = diagnosticCommand.split(DiagnosticUtils.DIAGNOSTICS_OPTION_SEPARATOR);
		Function<String, DiagnosticProperties> cmd = commandTable.get(commandRoot[0]);
		if (null == cmd) {
			result = DiagnosticProperties.makeStatusProperties(true,
					"Command " + diagnosticCommand + " not recognized");
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
			result = DiagnosticProperties.makeErrorProperties("Command not recognized: " + diagnosticCommand);
		} else {
			if (doLive) {
				runGC();
			}
			String hcsi = getHeapClassStatisticsImpl();
			String lineSeparator = System.lineSeparator();
			final String unixLineSeparator = "\n";
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
						|| option.toLowerCase().equals(THREAD_LOCKED_SYNCHRONIZERS_OPTION + "=true")) {
					addSynchronizers = true;
				}
			} else {
				okay = false;
			}
		}
		if (!okay) {
			result = DiagnosticProperties.makeErrorProperties("Command not recognized: " + diagnosticCommand);
		} else {
			StringWriter buffer = new StringWriter(2000);
			PrintWriter bufferPrinter = new PrintWriter(buffer);
			bufferPrinter.println(System.getProperty("java.vm.info"));
			bufferPrinter.println();
			ThreadInfoBase[] threadInfoBases = dumpAllThreadsImpl(true, addSynchronizers, Integer.MAX_VALUE);
			for (ThreadInfoBase currentThreadInfoBase : threadInfoBases) {
				bufferPrinter.print(currentThreadInfoBase.toString());
				if (addSynchronizers) {
					LockInfoBase[] lockedSynchronizers = currentThreadInfoBase.getLockedSynchronizers();
					bufferPrinter.printf("%n\tLocked ownable synchronizers: %d%n",
							Integer.valueOf(lockedSynchronizers.length));
					for (LockInfoBase currentLockedSynchronizer : lockedSynchronizers) {
						bufferPrinter.printf("\t- %s%n", currentLockedSynchronizer.toString());
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
		IPC.logMessage("doDump: ", diagnosticCommand);
		if (parts.length == 0 || parts.length > 4) {
			// The argument could be just Dump command which is going to use default configurations like -Xdump,
			// or there is an optional path/name argument for the dump file generated, plus `request=` and `opts=` options.
			result = DiagnosticProperties.makeErrorProperties("Error: wrong number of arguments");
		} else {
			String dumpType = "";
			/* handle legacy form of Dump.heap for compatibility with reference implementation */
			if (DIAGNOSTICS_GC_HEAP_DUMP.equals(parts[0])) {
				dumpType = "heap";
			} else {
				String[] dumpCommandAndType = parts[0].split("\\.");
				if (dumpCommandAndType.length != 2) {
					result = DiagnosticProperties.makeErrorProperties(String.format("Error: invalid command %s", parts[0]));
				} else {
					dumpType = dumpCommandAndType[1];
				}
			}
			if (!dumpType.isEmpty()) {
				StringBuilder request = new StringBuilder();
				request.append(dumpType);
				String filePath = null;
				boolean foundRequest = false;
				boolean heapDump = "heap".equals(dumpType);
				boolean systemDump = "system".equals(dumpType);
				String separator = ":";
				for (int i = 1; i < parts.length; i++) {
					String option = parts[i];
					boolean isRequest = option.startsWith("request=");
					boolean isOpts = option.startsWith("opts=");
					if (isRequest || isOpts) {
						if (!heapDump && isOpts) {
							// opts= are only valid for heap dumps
							continue;
						}
						request.append(separator).append(option);
						if (isRequest) {
							foundRequest = true;
						}
					} else {
						if (filePath != null) {
							result = DiagnosticProperties.makeErrorProperties("Error: second <file path> found, \""
									+ option + "\" after \"" + filePath + "\"");
							break;
						}
						String fileDirective = (systemDump && IPC.isZOS) ? "dsn=" : "file=";
						request.append(separator).append(fileDirective).append(option);
						filePath = option;
					}
					separator = ",";
				}
				if (result == null) {
					if (!foundRequest && (systemDump || heapDump)) {
						// set default options if the user didn't specify
						request.append(separator).append("request=exclusive+prepwalk");
					}
					try {
						String actualDumpFile = triggerDumpsImpl(request.toString(), dumpType + "DumpToFile");
						result = DiagnosticProperties.makeStringResult("Dump written to " + actualDumpFile);
					} catch (InvalidDumpOptionExceptionBase e) {
						IPC.logMessage("doDump exception: ", e.getMessage());
						result = DiagnosticProperties.makeExceptionProperties(e);
					}
				}
			}
		}
		return result;
	}

/*[IF JFR_SUPPORT]*/
	private static long convertToMilliseconds(String timeValue) {
		long timeInMilli = 0L;

		// extract the numeric part and the time unit
		String numericPart = timeValue.replaceAll("[^0-9]", "");

		// convert the unit to lowercase for consistency
		String timeUnit = timeValue.replaceAll("[0-9]", "").toLowerCase();

		// parse the numeric part
		long time = Long.parseLong(numericPart);

		// convert to milliseconds based on the unit
		switch (timeUnit) {
		case "s":
			timeInMilli = TimeUnit.SECONDS.toMillis(time);
			break;
		case "m":
			timeInMilli = TimeUnit.MINUTES.toMillis(time);
			break;
		case "h":
			timeInMilli = TimeUnit.HOURS.toMillis(time);
			break;
		case "d":
			timeInMilli = TimeUnit.DAYS.toMillis(time);
			break;
		default:
			// no unit or unrecognized unit, assume milliseconds
			timeInMilli = time;
			break;
		}
		return timeInMilli;
	}

	private static long parseTimeParameter(String paramName, String[] parameters) {
		for (String param : parameters) {
			if (param.startsWith(paramName + "=")) {
				int valueStart = param.indexOf("=");
				if (valueStart != -1) {
					return convertToMilliseconds(param.substring(valueStart + 1));
				}
			}
		}
		return -1;
	}

	private static String parseStringParameter(String paramName, String[] parameters, String defaultValue) {
		for (String param : parameters) {
			if (param.startsWith(paramName + "=")) {
				int valueStart = param.indexOf("=");
				if (valueStart != -1) {
					return param.substring(valueStart + 1);
				}
			}
		}
		return defaultValue;
	}

	private static DiagnosticProperties doJFR(String diagnosticCommand) {
		DiagnosticProperties result = null;

		// split the command and arguments
		String[] parts = diagnosticCommand.split(DIAGNOSTICS_OPTION_SEPARATOR);
		IPC.logMessage("doJFR: ", diagnosticCommand);

		// ensure there's at least one part for the command
		if (parts.length == 0) {
			return DiagnosticProperties.makeErrorProperties("Error: No JFR command specified");
		}

		String command = parts[0].trim();
		String[] parameters = Arrays.copyOfRange(parts, 1, parts.length);

		String fileName = parseStringParameter("filename", parameters, null);
		IPC.logMessage("doJFR: filename = ", fileName);
		boolean setFileName = (fileName != null) && !fileName.isEmpty();
		if (setFileName) {
			if (!VM.setJFRRecordingFileName(fileName)) {
				return DiagnosticProperties.makeErrorProperties("setJFRRecordingFileName failed");
			} else {
				jfrRecordingFileName = fileName;
			}
		}

		try {
			if (command.equalsIgnoreCase(DIAGNOSTICS_JFR_START)) {
				if (VM.isJFRRecordingStarted()) {
					result = DiagnosticProperties.makeErrorProperties("One JFR recording is in progress [" + jfrRecordingFileName + "], only one recording is allowed at a time.");
				} else {
					VM.startJFR();
					long duration = parseTimeParameter("duration", parameters);
					IPC.logMessage("doJFR: duration = " + duration);
					if (duration > 0) {
						Timer timer = new Timer();
						TimerTask jfrDumpTask = new TimerTask() {
							public void run() {
								VM.stopJFR();
							}
						};
						timer.schedule(jfrDumpTask, duration);
					} else {
						// the recording is on until JFR.stop
					}
					result = DiagnosticProperties.makeStringResult("Start JFR recording to " + jfrRecordingFileName);
				}
			} else if (command.equalsIgnoreCase(DIAGNOSTICS_JFR_STOP)) {
				if (VM.isJFRRecordingStarted()) {
					VM.stopJFR();
					result = DiagnosticProperties.makeStringResult("Stop JFR recording, and dump all Java threads to " + jfrRecordingFileName);
				} else {
					result = DiagnosticProperties.makeErrorProperties("Could not stop recording [" + jfrRecordingFileName + "], run JFR.start first.");
				}
			} else if (command.equalsIgnoreCase(DIAGNOSTICS_JFR_DUMP)) {
				if (VM.isJFRRecordingStarted()) {
					VM.jfrDump();
					result = DiagnosticProperties.makeStringResult("Dump all Java threads to " + jfrRecordingFileName);
				} else {
					result = DiagnosticProperties.makeErrorProperties("Could not create a JFR recording [" + jfrRecordingFileName + "], run JFR.start first.");
				}
			} else {
				result = DiagnosticProperties.makeErrorProperties("Command not recognized: " + command);
			}
		} catch (Exception e) {
			result = DiagnosticProperties.makeErrorProperties("Error in JFR: " + e.getMessage());
		}

		return result;
	}
/*[ENDIF] JFR_SUPPORT */

	private static native ThreadInfoBase[] dumpAllThreadsImpl(boolean lockedMonitors,
			boolean lockedSynchronizers, int maxDepth);

	private static DiagnosticProperties runGC() {
		VM.globalGC();
		return DiagnosticProperties.makeCommandSucceeded();
	}

	private static DiagnosticProperties getJstatClass(String diagnosticCommand) {
		IPC.logMessage("jstat command : ", diagnosticCommand);
		StringWriter buffer = new StringWriter(100);
		PrintWriter bufferPrinter = new PrintWriter(buffer);
		bufferPrinter.println("Class Loaded    Class Unloaded");
		// "Class Loaded".length = 12, "Class Unloaded".length = 14
		bufferPrinter.printf("%12d    %14d%n",
				Long.valueOf(ClassLoaderInfoBaseImpl.getLoadedClassCountImpl()),
				Long.valueOf(ClassLoaderInfoBaseImpl.getUnloadedClassCountImpl()));
		bufferPrinter.flush();
		return DiagnosticProperties.makeStringResult(buffer.toString());
	}

	private static DiagnosticProperties loadJVMTIAgent(String diagnosticCommand) {
		DiagnosticProperties result;
		String[] parts = diagnosticCommand.split(DIAGNOSTICS_OPTION_SEPARATOR);
		// parts[0] is already verified as DIAGNOSTICS_LOAD_JVMTI_AGENT since we are here
		if (parts.length < 2) {
			result = DiagnosticProperties.makeErrorProperties("Too few arguments, the absolute path of the agent is required: " + diagnosticCommand);
		} else if (parts.length > 3) {
			result = DiagnosticProperties.makeErrorProperties("Command not recognized due to more than 3 arguments: " + diagnosticCommand);
		} else {
			String attachError = Attachment.loadAgentLibrary(parts[1], (parts.length == 3) ? parts[2] : "", false);
			if (attachError == null) {
				result = DiagnosticProperties.makeStringResult(DIAGNOSTICS_LOAD_JVMTI_AGENT + " succeeded");
			} else {
				result = DiagnosticProperties.makeStatusProperties(true, attachError);
			}
		}
		return result;
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
				String helpText = helpTable.getOrDefault(parts[1], "No help available");
				bufferPrinter.printf("%s: ", parts[1]);
				bufferPrinter.printf(helpText);
			}
		} else {
			bufferPrinter.print("Invalid command: " + diagnosticCommand);
		}
		return DiagnosticProperties.makeStringResult(buffer.toString());

	}

/*[IF CRAC_SUPPORT]*/
	private static DiagnosticProperties doCheckpointJVM(String diagnosticCommand) {
		Thread checkpointThread = new Thread(() -> {
			try {
				jdk.crac.Core.checkpointRestore();
			} catch (Throwable t) {
				t.printStackTrace();
			}
		});
		checkpointThread.start();

		return DiagnosticProperties.makeStringResult("JVM checkpoint requested");
	}
/*[ENDIF] CRAC_SUPPORT */

	/* Help strings for the jcmd utilities */
	private static final String DIAGNOSTICS_HELP_HELP = "Show help for a command%n"
			+ FORMAT_PREFIX + " help <command>%n"
			+ " If no command is supplied, print the list of available commands on the target JVM.%n";

	private static final String DIAGNOSTICS_GC_CLASS_HISTOGRAM_HELP = "Obtain heap information about a Java process%n"
			+ FORMAT_PREFIX + DIAGNOSTICS_GC_CLASS_HISTOGRAM + " [options]%n"
			+ " Options:%n"
			+ "          all : include all objects, including dead objects (this is the default option)%n"
			+ "         live : include all objects after a global GC collection%n"
			+ "NOTE: this utility might significantly affect the performance of the target VM.%n";

	private static final String DIAGNOSTICS_GC_RUN_HELP = "Run the garbage collector.%n"
			+ FORMAT_PREFIX + DIAGNOSTICS_GC_RUN + "%n"
			+ "NOTE: this utility might significantly affect the performance of the target VM.%n";

	private static final String DIAGNOSTICS_THREAD_PRINT_HELP = "List thread information.%n"
			+ FORMAT_PREFIX + DIAGNOSTICS_THREAD_PRINT + " [options]%n"
			+ " Options: -l : print information about ownable synchronizers%n";

	private static final String DIAGNOSTICS_DUMP_HEAP_HELP = "Create a heap dump.%n"
			+ FORMAT_PREFIX + DIAGNOSTICS_DUMP_HEAP + HEAP_DUMP_OPTION_HELP + HEAPSYSTEM_DUMP_OPTION_HELP + GENERIC_DUMP_OPTION_HELP
			+ DIAGNOSTICS_GC_HEAP_DUMP + " is an alias for " + DIAGNOSTICS_DUMP_HEAP + "%n";

	private static final String DIAGNOSTICS_DUMP_JAVA_HELP = "Create a javacore file.%n"
			+ FORMAT_PREFIX + DIAGNOSTICS_DUMP_JAVA + OTHER_DUMP_OPTION_HELP + GENERIC_DUMP_OPTION_HELP;

	private static final String DIAGNOSTICS_DUMP_SNAP_HELP = "Dump the snap trace buffer.%n"
			+ FORMAT_PREFIX + DIAGNOSTICS_DUMP_SNAP + OTHER_DUMP_OPTION_HELP + GENERIC_DUMP_OPTION_HELP;

	private static final String DIAGNOSTICS_DUMP_SYSTEM_HELP = "Create a native core file.%n"
			+ FORMAT_PREFIX + DIAGNOSTICS_DUMP_SYSTEM + OTHER_DUMP_OPTION_HELP + HEAPSYSTEM_DUMP_OPTION_HELP + GENERIC_DUMP_OPTION_HELP;

	private static final String DIAGNOSTICS_JSTAT_CLASS_HELP = "Show JVM classloader statistics.%n"
			+ FORMAT_PREFIX + DIAGNOSTICS_STAT_CLASS + "%n"
			+ "NOTE: this utility might significantly affect the performance of the target VM.%n";

	private static final String DIAGNOSTICS_LOAD_JVMTI_AGENT_HELP = "Load JVMTI agent.%n"
			+ FORMAT_PREFIX + DIAGNOSTICS_LOAD_JVMTI_AGENT + " <agentLibrary> [<agent option>]%n"
			+ "          agentLibrary: the absolute path of the agent%n"
			+ "          agent option: (Optional) the agent option string%n";

/*[IF CRAC_SUPPORT]*/
	private static final String DIAGNOSTICS_JDK_CHECKPOINT_HELP = "Produce a JVM checkpoint via CRIUSupport.%n"
			+ FORMAT_PREFIX + DIAGNOSTICS_JDK_CHECKPOINT + "%n"
			+ "NOTE: this utility might significantly affect the performance of the target VM.%n";
/*[ENDIF] CRAC_SUPPORT */

/*[IF JFR_SUPPORT]*/
	private static final String DIAGNOSTICS_JFR_START_HELP = "Start a new Recording%n%n"
			+ SYNTAX_PREFIX + DIAGNOSTICS_JFR_START + JFR_START_OPTION_HELP;

	private static final String DIAGNOSTICS_JFR_DUMP_HELP = "Dump a JFR recording to file%n%n"
			+ SYNTAX_PREFIX + DIAGNOSTICS_JFR_DUMP + JFR_DUMP_OPTION_HELP;

	private static final String DIAGNOSTICS_JFR_STOP_HELP = "Stop a JFR recording%n%n"
			+ SYNTAX_PREFIX + FORMAT_PREFIX + DIAGNOSTICS_JFR_STOP + JFR_STOP_OPTION_HELP;
/*[ENDIF] JFR_SUPPORT */

	/* Initialize the command and help text tables */
	static {
		IDCacheInitializer.init();
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

		commandTable.put(DIAGNOSTICS_LOAD_JVMTI_AGENT, DiagnosticUtils::loadJVMTIAgent);
		helpTable.put(DIAGNOSTICS_LOAD_JVMTI_AGENT, DIAGNOSTICS_LOAD_JVMTI_AGENT_HELP);

/*[IF CRAC_SUPPORT]*/
		if (InternalCRIUSupport.isCRaCSupportEnabled()) {
			commandTable.put(DIAGNOSTICS_JDK_CHECKPOINT, DiagnosticUtils::doCheckpointJVM);
			helpTable.put(DIAGNOSTICS_JDK_CHECKPOINT, DIAGNOSTICS_JDK_CHECKPOINT_HELP);
		}
/*[ENDIF] CRAC_SUPPORT */

/*[IF JFR_SUPPORT]*/
		commandTable.put(DIAGNOSTICS_JFR_START, DiagnosticUtils::doJFR);
		helpTable.put(DIAGNOSTICS_JFR_START, DIAGNOSTICS_JFR_START_HELP);

		commandTable.put(DIAGNOSTICS_JFR_DUMP, DiagnosticUtils::doJFR);
		helpTable.put(DIAGNOSTICS_JFR_DUMP, DIAGNOSTICS_JFR_DUMP_HELP);

		commandTable.put(DIAGNOSTICS_JFR_STOP, DiagnosticUtils::doJFR);
		helpTable.put(DIAGNOSTICS_JFR_STOP, DIAGNOSTICS_JFR_STOP_HELP);
/*[ENDIF] JFR_SUPPORT */
	}
}
