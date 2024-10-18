/*[INCLUDE-IF Sidecar18-SE]*/
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
import java.util.function.Function;

/*[IF JFR_SUPPORT]*/
import java.lang.reflect.Constructor;
import java.lang.reflect.Method;
/*[ENDIF] JFR_SUPPORT */
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
public class DiagnosticUtils {

	private static final String FORMAT_PREFIX = " Format: "; //$NON-NLS-1$

/*[IF JFR_SUPPORT]*/
	private static final String JFR_FORMAT_PREFIX = "Syntax: ";
/*[ENDIF] JFR_SUPPORT */

	@SuppressWarnings("nls")
	private static final String HEAP_DUMP_OPTION_HELP = " [request=<options>] [opts=<options>] [<file path>]%n"
			+ " Set optional request= and opts= -Xdump options. The order of the parameters does not matter.%n";

	@SuppressWarnings("nls")
	private static final String OTHER_DUMP_OPTION_HELP = " [request=<options>] [<file path>]%n"
				+ " Set optional request= -Xdump options. The order of the parameters does not matter.%n";

	@SuppressWarnings("nls")
	private static final String HEAPSYSTEM_DUMP_OPTION_HELP =
			" system and heap dumps default to request=exclusive+prepwalk rather than the -Xdump:<type>:defaults setting.%n";

	@SuppressWarnings("nls")
	private static final String GENERIC_DUMP_OPTION_HELP =
			" <file path> is optional, otherwise a default path/name is used.%n"
			+ " Relative paths are resolved to the target's working directory.%n"
			+ " The dump agent may choose a different file path if the requested file exists.%n";

/*[IF JFR_SUPPORT]*/
	@SuppressWarnings("nls")
	private static final String JFR_START_OPTION_HELP =
			" [options]%n"
			+ "%n"
			+ "Options:%n"
			+ "%n"
			+ "delay        (Optional) Length of time to wait before starting to record%n"
			+ "             (INTEGER followed by 's' for seconds 'm' for minutes or h' for hours,0s)%n"
			+ "%n"
			+ "disk         (Optional) Flag for also writing the data to disk while recording%n"
			+ "             (BOOLEAN, true)%n"
			+ "%n"
			+ "duration     (Optional) Length of time to record. Note that 0s means forever%n"
			+ "             (INTEGER followed by 's' for seconds 'm' for minutes or 'h' for hours,0s)%n"
			+ "%n"
			+ "filename     (Optional) Name of the file to which the flight recording data is%n"
			+ "              written when the recording is stopped. If no filename is given, a filename%n"
			+ "              is generated from the PID and the current date and is placed in the directory%n"
			+ "              where the process was started. The  filename may also be a directory in which%n"
			+ "              case, the filename is generated from the PID and the current date in the specified%n"
			+ "              directory. (STRING, no default value)%n"
			+ "%n"
			+ "maxage       (Optional) Maximum time to keep the recorded data on disk. This parameter is valid%n"
			+ "              only when the disk parameter is set to true. Note 0s means forever.%n"
			+ "              (INTEGER followed by 's' for seconds 'm' for minutes or 'h' for hours, 0s)%n"
			+ "%n"
			+ "maxsize      (Optional) Maximum size of the data to keep on disk in bytes if one of the following%n"
			+ "              suffixes is not used: 'm' or 'M' for megabytes OR 'g' or 'G' for gigabytes. This%n"
			+ "              parameter is valid only when the disk parameter is set to 'true'. The value must not%n"
			+ "              be less than the value for the maxchunksize parameter set with the JFR.configure command.%n"
			+ "              (STRING, 0 (no max size))%n"
			+ "%n"
			+ "settings     (Optional) Name of the settings file that identifies which events to record.%n"
			+ "              To specify more than one file, use the settings parameter repeatedly. Include the%n"
			+ "              path if the file is not in JAVA-HOME/lib/jfr. The following profiles are included%n"
			+ "              with the JDK in the JAVA-HOME/lib/jfr directory: 'default.jfc': collects a predefined%n"
			+ "              set of information with low overhead, so it has minimal impact on performance%n"
			+ "              and can be used with recordings that run continuously; 'profile.jfc': Provides%n"
			+ "              more data than the  'default.jfc' profile, but with more overhead and impact on%n"
			+ "              performance. Use this configuration for short periods of time when more information%n"
			+ "              is needed. Use none to start a recording without a predefined configuration file.%n"
			+ "              (STRING,JAVA-HOME/lib/jfr/default.jfc)%n"
			+ "%n"
			+ "Options must be specified using the <key> or <key>=<value> syntax.%n"
			+ "%n"
			+ "Example usage:%n"
			+ "%n"
			+ "$ jcmd <pid> JFR.start%n"
			+ "$ jcmd <pid> JFR.start filename=dump.jfr%n"
			+ "$ jcmd <pid> JFR.start filename=/directory/recordings%n"
			+ "$ jcmd <pid> JFR.start maxage=1h,maxsize=1000M%n"
			+ "$ jcmd <pid> JFR.start settings=profile%n"
			+ "$ jcmd <pid> JFR.start delay=5m,settings=my.jfc%n"
			+ "%n"
			+ "Note, if the default event settings are modified, overhead may exceed 1%%.%n";

	@SuppressWarnings("nls")
	private static final String JFR_STOP_OPTION_HELP =
			" [options]%n"
			+ "%n"
			+ "Options:%n"
			+ "%n"
			+ "filename     (Optional) Name of the file to which the recording is written when the%n"
			+ "              written when the recording is stopped. If no filename is provided, the data%n"
			+ "              from the recording is discarded. (STRING, no default value)%n"
			+ "%n"
			+ "name         Name of the recording (STRING, no default value)%n"
			+ "%n"
			+ "Example usage:%n"
			+ "%n"
			+ "$ jcmd <pid> JFR.stop name=1%n"
			+ "$ jcmd <pid> JFR.stop name=benchmark filename=/directory/recordings%n"
			+ "$ jcmd <pid> JFR.stop name=5 filename=recording.jfr%n";

	@SuppressWarnings("nls")
	private static final String JFR_DUMP_OPTION_HELP =
			" [options]%n"
			+ "%n"
			+ "Options:%n"
			+ "%n"
			+ "filename        (Optional) Name of the file to which the flight recording data is%n"
			+ "                dumped. If no filename is given, a filename is generated from the PID%n"
			+ "                and the current date. The filename may also be a directory in which%n"
			+ "                case, the filename is generated from the PID and the current date in%n"
			+ "                the specified directory. (STRING, no default value)%n"
			+ "%n"
			+ "maxage          (Optional) Length of time for dumping the flight recording data to a%n"
			+ "                file. (INTEGER followed by 's' for seconds 'm' for minutes or 'h' for%n"
			+ "                hours, no default value)%n"
			+ "%n"
			+ "maxsize         (Optional) Maximum size for the amount of data to dump from a flight%n"
			+ "                recording in bytes if one of the following suffixes is not used:%n"
			+ "                'm' or 'M' for megabytes OR 'g' or 'G' for gigabytes.%n"
			+ "                (STRING, no default value)%n"
			+ "%n"
			+ "name            (Optional) Name of the recording. If no name is given, data from all%n"
			+ "                recordings is dumped. (STRING, no default value)%n"
			+ "%n"
			+ "Options must be specified using the <key> or <key>=<value> syntax.%n"
			+ "%n"
			+ "Example usage:%n"
			+ "%n"
			+ "$ jcmd <pid> JFR.dump%n"
			+ "$ jcmd <pid> JFR.dump filename=recording.jfr%n"
			+ "$ jcmd <pid> JFR.dump filename=/directory/recordings%n"
			+ "$ jcmd <pid> JFR.dump name=1 filename=/recordings/recording.jfr%n"
			+ "$ jcmd <pid> JFR.dump maxage=1h%n"
			+ "$ jcmd <pid> JFR.dump maxage=1h maxsize=50M%n";
/*[ENDIF] JFR_SUPPORT */

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

/*[IF CRAC_SUPPORT]*/
	/**
	 * Generate a checkpoint via CRIUSupport using a compatability name.
	 */
	private static final String DIAGNOSTICS_JDK_CHECKPOINT = "JDK.checkpoint"; //$NON-NLS-1$
/*[ENDIF] CRAC_SUPPORT */

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

/*[IF JFR_SUPPORT]*/
	/**
	 * Commands for JFR start, stop and dump
	 */
	private static final String DIAGNOSTICS_JFR_START = "JFR.start";
	private static final String DIAGNOSTICS_JFR_STOP = "JFR.stop";
	private static final String DIAGNOSTICS_JFR_DUMP = "JFR.dump";
/*[ENDIF] JFR_SUPPORT */

	/**
	 * Get JVM statistics
	 */
	private static final String DIAGNOSTICS_STAT_CLASS = "jstat.class"; //$NON-NLS-1$

	// load JVMTI agent
	private static final String DIAGNOSTICS_LOAD_JVMTI_AGENT = "JVMTI.agent_load"; //$NON-NLS-1$

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
		if (parts.length == 0 || parts.length > 4) {
			// The argument could be just Dump command which is going to use default configurations like -Xdump,
			// or there is an optional path/name argument for the dump file generated, plus `request=` and `opts=` options.
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
				StringBuilder request = new StringBuilder();
				request.append(dumpType);
				String filePath = null;
				boolean foundRequest = false;
				boolean heapDump = "heap".equals(dumpType); //$NON-NLS-1$
				boolean systemDump = "system".equals(dumpType); //$NON-NLS-1$
				String separator = ":"; //$NON-NLS-1$
				for (int i = 1; i < parts.length; i++) {
					String option = parts[i];
					boolean isRequest = option.startsWith("request="); //$NON-NLS-1$
					boolean isOpts = option.startsWith("opts="); //$NON-NLS-1$
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
							result = DiagnosticProperties.makeErrorProperties("Error: second <file path> found, \"" //$NON-NLS-1$
									+ option + "\" after \"" + filePath + "\""); //$NON-NLS-1$ //$NON-NLS-2$
							break;
						}
						String fileDirective = (systemDump && IPC.isZOS) ? "dsn=" : "file="; //$NON-NLS-1$ //$NON-NLS-2$
						request.append(separator).append(fileDirective).append(option);
						filePath = option;
					}
					separator = ","; //$NON-NLS-1$
				}
				if (result == null) {
					if (!foundRequest && (systemDump || heapDump)) {
						// set default options if the user didn't specify
						request.append(separator).append("request=exclusive+prepwalk"); //$NON-NLS-1$
					}
					try {
						String actualDumpFile = triggerDumpsImpl(request.toString(), dumpType + "DumpToFile"); //$NON-NLS-1$
						result = DiagnosticProperties.makeStringResult("Dump written to " + actualDumpFile); //$NON-NLS-1$
					} catch (InvalidDumpOptionExceptionBase e) {
						IPC.logMessage("doDump exception: ", e.getMessage()); //$NON-NLS-1$
						result = DiagnosticProperties.makeExceptionProperties(e);
					}
				}
			}
		}
		return result;
	}

/*[IF JFR_SUPPORT]*/
/*[IF JAVA_SPEC_VERSION == 11]*/
	private static Long convertToBytes(String sizeValue) {
		long sizeInBytes = 0L;

		/* Extract the numeric part and the size unit */
		String numericPart = sizeValue.replaceAll("[^0-9]", "");

		/* Convert the unit to lowercase for consistency */
		String sizeUnit = sizeValue.replaceAll("[0-9]", "").toLowerCase();

		/* Parse the numeric part */
		long size = Long.parseLong(numericPart);

		/* Convert to bytes based on the unit */
		switch (sizeUnit) {
		case "k":
			sizeInBytes = size * 1_024L;
			break;
		case "m":
			sizeInBytes = size * 1_024L * 1_024L;
			break;
		case "g":
			sizeInBytes = size * 1_024L * 1_024L * 1_024L;
			break;
		default:
			/* No unit or unrecognized unit, assume bytes */
			sizeInBytes = size;
			break;
		}
		return sizeInBytes;
	}
/*[ENDIF] JAVA_SPEC_VERSION == 11 */
/*[ENDIF] JFR_SUPPORT */

/*[IF JFR_SUPPORT]*/
/*[IF JAVA_SPEC_VERSION == 11]*/
	private static Long convertToNanoseconds(String timeValue) {
		long timeInNano = 0L;

		/* Extract the numeric part and the time unit */
		String numericPart = timeValue.replaceAll("[^0-9]", "");

		/* Convert the unit to lowercase for consistency */
		String timeUnit = timeValue.replaceAll("[0-9]", "").toLowerCase();

		/* Parse the numeric part */
		long time = Long.parseLong(numericPart);

		/* Convert to nanoseconds based on the unit */
		switch (timeUnit) {
		case "s":
			timeInNano = time * 1_000_000_000L;
			break;
		case "m":
			timeInNano = time * 60 * 1_000_000_000L;
			break;
		case "h":
			timeInNano = time * 60 * 60 * 1_000_000_000L;
			break;
		case "d":
			timeInNano = time * 24 * 60 * 60 * 1_000_000_000L;
			break;
		default:
			/* No unit or unrecognized unit, assume nanoseconds */
			timeInNano = time;
			break;
		}
		return timeInNano;
	}
/*[ENDIF] JAVA_SPEC_VERSION == 11 */
/*[ENDIF] JFR_SUPPORT */

/*[IF JFR_SUPPORT]*/
/*[IF JAVA_SPEC_VERSION == 11]*/
	private static Boolean parseBooleanParameter(String paramName, String[] parameters) {
		for (String param : parameters) {
			int index = param.indexOf('=');
			if (index != -1 && param.substring(0, index).equals(paramName)) {
				return Boolean.parseBoolean(param.substring(index + 1));
			}
		}
		return null;
	}
/*[ENDIF] JAVA_SPEC_VERSION == 11 */
/*[ENDIF] JFR_SUPPORT */

/*[IF JFR_SUPPORT]*/
/*[IF JAVA_SPEC_VERSION == 11]*/
	private static Long parseMemorySizeParameter(String paramName, String[] parameters) {
		for (String param : parameters) {
			int index = param.indexOf(paramName + "=");
			if (index == 0) {
				String value = param.substring(paramName.length() + 1);
				return convertToBytes(value);
			}
		}
		return null;
	}
/*[ENDIF] JAVA_SPEC_VERSION == 11 */
/*[ENDIF] JFR_SUPPORT */

/*[IF JFR_SUPPORT]*/
/*[IF JAVA_SPEC_VERSION == 11]*/
	private static Long parseTimeParameter(String paramName, String[] parameters) {
		for (String param : parameters) {
			int index = param.indexOf(paramName + "=");
			if (index == 0) {
				String value = param.substring(paramName.length() + 1);
				return convertToNanoseconds(value);
			}
		}
		return null;
	}
/*[ENDIF] JAVA_SPEC_VERSION == 11 */
/*[ENDIF] JFR_SUPPORT */

/*[IF JFR_SUPPORT]*/
/*[IF JAVA_SPEC_VERSION == 11]*/
	private static String parseStringParameter(String paramName, String[] parameters, String defaultValue) {
		for (String param : parameters) {
			int index = param.indexOf(paramName + "=");
			if (index == 0) {
				return param.substring(index + paramName.length() + 1);
			}
		}
		return defaultValue;
	}
/*[ENDIF] JAVA_SPEC_VERSION == 11 */
/*[ENDIF] JFR_SUPPORT */

/*[IF JFR_SUPPORT]*/
/*[IF JAVA_SPEC_VERSION >= 17]*/
	private static String invokeJavaJFR(Method executeMethod, Object dcmdInstance, String[] parameters) throws Exception {
		String jcmdarg = "attach";
		char delimiter = (parameters.length > 0) ? ',' : ' ';
		String[] returnedStringArray = (String[]) executeMethod.invoke(dcmdInstance, jcmdarg, String.join(",", parameters), delimiter);

		/* Convert the String[] to a single String */
		String messageString = String.join("\n", returnedStringArray);
		return messageString;
	}
/*[ENDIF] JAVA_SPEC_VERSION >= 17 */
/*[ENDIF] JFR_SUPPORT */

/*[IF JFR_SUPPORT]*/
	private static Object createInstance(Class<?> dcmdClass) throws Exception {
		Constructor<?> constructor = dcmdClass.getDeclaredConstructor();
		constructor.setAccessible(true);
		return constructor.newInstance();
	}
/*[ENDIF] JFR_SUPPORT */

/*[IF JFR_SUPPORT]*/
	private static DiagnosticProperties doJFR(String diagnosticCommand) {
		DiagnosticProperties result = null;

		/* Split the command and arguments */
		String[] parts = diagnosticCommand.split(DIAGNOSTICS_OPTION_SEPARATOR);
		IPC.logMessage("doJFR: ", diagnosticCommand);

		/* Ensure there's at least one part for the command */
		if (parts.length == 0) {
			return DiagnosticProperties.makeErrorProperties("Error: No JFR command specified");
		}

		String command = parts[0].trim();
		String[] parameters = Arrays.copyOfRange(parts, 1, parts.length);

/*[IF JAVA_SPEC_VERSION == 11]*/
		/* Parse common JFR parameters for Java 11 */
		String name = null; /* Not supported, may be added in a future release */
		String path = parseStringParameter("filename", parameters, null);
		String settings = parseStringParameter("settings", parameters, "default");
		Long delay = parseTimeParameter("delay", parameters);
		Long duration = parseTimeParameter("duration", parameters);
		Long maxAge = parseTimeParameter("maxage", parameters);
		Long maxSize = parseMemorySizeParameter("maxsize", parameters);
		Boolean disk = parseBooleanParameter("disk", parameters);
		Boolean dumpOnExit = null; /* Not supported, may be added in a future release */
		Boolean pathToGcRoots = null; /* Not supported, may be added in a future release */
		String begin = null; /* Not supported, may be added in a future release */
		String end = null; /* Not supported, may be added in a future release */
/*[ENDIF] JAVA_SPEC_VERSION == 11 */

		try {
			Class<?> dcmdClass;
			Method executeMethod;
			Object dcmdInstance;
			String returnedString;

			if (command.equals(DIAGNOSTICS_JFR_START)) {
				dcmdClass = Class.forName("jdk.jfr.internal.dcmd.DCmdStart");
				dcmdInstance = createInstance(dcmdClass);

/*[IF JAVA_SPEC_VERSION == 11]*/
				/* DCmdStart execute method for Java 11 JFR.start */
				executeMethod = dcmdClass.getDeclaredMethod(
					"execute", String.class, String[].class,
					Long.class, Long.class, Boolean.class, String.class,
					Long.class, Long.class, Boolean.class, Boolean.class);
				executeMethod.setAccessible(true);
				returnedString = (String) executeMethod.invoke(
					dcmdInstance, name, new String[]{settings},
					delay, duration, disk, path, maxAge, maxSize, dumpOnExit, pathToGcRoots);
/*[ENDIF] JAVA_SPEC_VERSION == 11 */

/*[IF JAVA_SPEC_VERSION >= 17]*/
				/* DCmdStart execute method for Java 17 and later, handling the JFR.start */
				executeMethod = dcmdClass.getSuperclass().getDeclaredMethod("execute", String.class, String.class, char.class);
				executeMethod.setAccessible(true);
				returnedString = invokeJavaJFR(executeMethod, dcmdInstance, parameters);
/*[ENDIF] JAVA_SPEC_VERSION >= 17 */

				result = DiagnosticProperties.makeStringResult(returnedString);
			} else if (command.equals(DIAGNOSTICS_JFR_STOP)) {
				dcmdClass = Class.forName("jdk.jfr.internal.dcmd.DCmdStop");
				dcmdInstance = createInstance(dcmdClass);

/*[IF JAVA_SPEC_VERSION == 11]*/
				/* DCmdStop execute method for Java 11 JFR.stop */
				executeMethod = dcmdClass.getDeclaredMethod("execute", String.class, String.class);
				executeMethod.setAccessible(true);
				returnedString = (String) executeMethod.invoke(dcmdInstance, name, path);
/*[ENDIF] JAVA_SPEC_VERSION == 11 */

/*[IF JAVA_SPEC_VERSION >= 17]*/
				/* DCmdStop execute method for Java 17 and later , handling the JFR.stop */
				executeMethod = dcmdClass.getSuperclass().getDeclaredMethod("execute", String.class, String.class, char.class);
				executeMethod.setAccessible(true);
				returnedString = invokeJavaJFR(executeMethod, dcmdInstance, parameters);
/*[ENDIF] JAVA_SPEC_VERSION >= 17 */

				result = DiagnosticProperties.makeStringResult(returnedString);
			} else if (command.equals(DIAGNOSTICS_JFR_DUMP)) {
				dcmdClass = Class.forName("jdk.jfr.internal.dcmd.DCmdDump");
				dcmdInstance = createInstance(dcmdClass);

/*[IF JAVA_SPEC_VERSION == 11]*/
				/* DCmdDump execute method for Java 11 JFR.dump */
				executeMethod = dcmdClass.getDeclaredMethod(
					"execute", String.class, String.class,
					Long.class, Long.class, String.class, String.class, Boolean.class);
				executeMethod.setAccessible(true);
				returnedString = (String) executeMethod.invoke(dcmdInstance, name, path, maxAge, maxSize, begin, end, pathToGcRoots);
/*[ENDIF] JAVA_SPEC_VERSION == 11 */

/*[IF JAVA_SPEC_VERSION >= 17]*/
				/* DCmdDump execute method for Java 17 and later, handling the JFR.dump */
				executeMethod = dcmdClass.getSuperclass().getDeclaredMethod("execute", String.class, String.class, char.class);
				executeMethod.setAccessible(true);
				returnedString = invokeJavaJFR(executeMethod, dcmdInstance, parameters);
/*[ENDIF] JAVA_SPEC_VERSION >= 17 */

				result = DiagnosticProperties.makeStringResult(returnedString);
			} else {
				return DiagnosticProperties.makeErrorProperties("Error: Unknown JFR command");
			}
		} catch (Exception e) {
			return DiagnosticProperties.makeErrorProperties("Error in JFR command: " + e.getMessage());
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

	@SuppressWarnings("nls")
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
				String helpText = helpTable.getOrDefault(parts[1], "No help available"); //$NON-NLS-1$
				bufferPrinter.printf("%s: ", parts[1]); //$NON-NLS-1$
				bufferPrinter.printf(helpText);
			}
		} else {
			bufferPrinter.print("Invalid command: " + diagnosticCommand); //$NON-NLS-1$
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

		return DiagnosticProperties.makeStringResult("JVM checkpoint requested"); //$NON-NLS-1$
	}
/*[ENDIF] CRAC_SUPPORT */

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
			+ FORMAT_PREFIX + DIAGNOSTICS_DUMP_HEAP + HEAP_DUMP_OPTION_HELP + HEAPSYSTEM_DUMP_OPTION_HELP + GENERIC_DUMP_OPTION_HELP
			+ DIAGNOSTICS_GC_HEAP_DUMP + " is an alias for " + DIAGNOSTICS_DUMP_HEAP + "%n"; //$NON-NLS-1$ //$NON-NLS-2$

	private static final String DIAGNOSTICS_DUMP_JAVA_HELP = "Create a javacore file.%n" //$NON-NLS-1$
			+ FORMAT_PREFIX + DIAGNOSTICS_DUMP_JAVA + OTHER_DUMP_OPTION_HELP + GENERIC_DUMP_OPTION_HELP;

	private static final String DIAGNOSTICS_DUMP_SNAP_HELP = "Dump the snap trace buffer.%n" //$NON-NLS-1$
			+ FORMAT_PREFIX + DIAGNOSTICS_DUMP_SNAP + OTHER_DUMP_OPTION_HELP + GENERIC_DUMP_OPTION_HELP;

	private static final String DIAGNOSTICS_DUMP_SYSTEM_HELP = "Create a native core file.%n" //$NON-NLS-1$
			+ FORMAT_PREFIX + DIAGNOSTICS_DUMP_SYSTEM + OTHER_DUMP_OPTION_HELP + HEAPSYSTEM_DUMP_OPTION_HELP + GENERIC_DUMP_OPTION_HELP;

	private static final String DIAGNOSTICS_JSTAT_CLASS_HELP = "Show JVM classloader statistics.%n" //$NON-NLS-1$
			+ FORMAT_PREFIX + DIAGNOSTICS_STAT_CLASS + "%n" //$NON-NLS-1$
			+ "NOTE: this utility might significantly affect the performance of the target VM.%n"; //$NON-NLS-1$

	@SuppressWarnings("nls")
	private static final String DIAGNOSTICS_LOAD_JVMTI_AGENT_HELP = "Load JVMTI agent.%n"
			+ FORMAT_PREFIX + DIAGNOSTICS_LOAD_JVMTI_AGENT + " <agentLibrary> [<agent option>]%n"
			+ "          agentLibrary: the absolute path of the agent%n"
			+ "          agent option: (Optional) the agent option string%n";

/*[IF JFR_SUPPORT]*/
	private static final String DIAGNOSTICS_JFR_START_HELP = "Starts a new Recording%n%n"
			+ JFR_FORMAT_PREFIX + DIAGNOSTICS_JFR_START + JFR_START_OPTION_HELP;

	private static final String DIAGNOSTICS_JFR_STOP_HELP = "Stops a JFR recording%n%n"
			+ JFR_FORMAT_PREFIX + DIAGNOSTICS_JFR_STOP + JFR_STOP_OPTION_HELP;

	private static final String DIAGNOSTICS_JFR_DUMP_HELP = "Copies contents of a JFR recording to file. Either the name or the recording id must be specified.%n%n"
			+ JFR_FORMAT_PREFIX + DIAGNOSTICS_JFR_DUMP + JFR_DUMP_OPTION_HELP;
/*[ENDIF] JFR_SUPPORT */

/*[IF CRAC_SUPPORT]*/
	private static final String DIAGNOSTICS_JDK_CHECKPOINT_HELP = "Produce a JVM checkpoint via CRIUSupport.%n" //$NON-NLS-1$
			+ FORMAT_PREFIX + DIAGNOSTICS_JDK_CHECKPOINT + "%n" //$NON-NLS-1$
			+ "NOTE: this utility might significantly affect the performance of the target VM.%n"; //$NON-NLS-1$
/*[ENDIF] CRAC_SUPPORT */

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

/*[IF JFR_SUPPORT]*/
		commandTable.put(DIAGNOSTICS_JFR_START, DiagnosticUtils::doJFR);
		helpTable.put(DIAGNOSTICS_JFR_START, DIAGNOSTICS_JFR_START_HELP);

		commandTable.put(DIAGNOSTICS_JFR_STOP, DiagnosticUtils::doJFR);
		helpTable.put(DIAGNOSTICS_JFR_STOP, DIAGNOSTICS_JFR_STOP_HELP);

		commandTable.put(DIAGNOSTICS_JFR_DUMP, DiagnosticUtils::doJFR);
		helpTable.put(DIAGNOSTICS_JFR_DUMP, DIAGNOSTICS_JFR_DUMP_HELP);
/*[ENDIF] JFR_SUPPORT */

/*[IF CRAC_SUPPORT]*/
		if (InternalCRIUSupport.isCRaCSupportEnabled()) {
			commandTable.put(DIAGNOSTICS_JDK_CHECKPOINT, DiagnosticUtils::doCheckpointJVM);
			helpTable.put(DIAGNOSTICS_JDK_CHECKPOINT, DIAGNOSTICS_JDK_CHECKPOINT_HELP);
		}
/*[ENDIF] CRAC_SUPPORT */
	}
}
