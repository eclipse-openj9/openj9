/*[INCLUDE-IF JFR_SUPPORT]*/
/*
 * Copyright IBM Corp. and others 2026
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

import static openj9.internal.tools.attach.target.IPC.LOGGING_ENABLED;
import static openj9.internal.tools.attach.target.IPC.loggingStatus;

import com.ibm.oti.vm.VM;
import com.ibm.oti.vm.VMLangAccess;

import java.util.Arrays;
import java.util.concurrent.TimeUnit;
import java.util.stream.Collectors;
import java.util.stream.Stream;
import java.util.Timer;
import java.util.TimerTask;

/**
 * Common methods for the JFR diagnostics tools.
 *
 */
@SuppressWarnings("nls")
public class JFR {
	/**
	 * Commands for JFR start, stop, dump and configure.
	 */
	static final String DIAGNOSTICS_JFR_START = "JFR.start";
	static final String DIAGNOSTICS_JFR_DUMP = "JFR.dump";
	static final String DIAGNOSTICS_JFR_STOP = "JFR.stop";
	static final String DIAGNOSTICS_JFR_CONFIGURE = "JFR.configure";

	private static final int ERROR_NO_TIME_UNIT = -1;
	private static final int ERROR_NO_TIME_DURATION = -2;

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

	private static final String JFR_CONFIGURE_OPTION_HELP =
			" [options]%n"
			+ "%n"
			+ "Options:%n"
			+ "%n"
			+ "samplethreads      (Optional) Flag for activating thread sampling. (BOOLEAN, true)%n";

	static final String DIAGNOSTICS_JFR_START_HELP = "Start a new Recording%n%n"
			+ DiagnosticUtils.SYNTAX_PREFIX + DIAGNOSTICS_JFR_START + JFR_START_OPTION_HELP;

	static final String DIAGNOSTICS_JFR_DUMP_HELP = "Dump a JFR recording to file%n%n"
			+ DiagnosticUtils.SYNTAX_PREFIX + DIAGNOSTICS_JFR_DUMP + JFR_DUMP_OPTION_HELP;

	static final String DIAGNOSTICS_JFR_CONFIGURE_HELP = "Modify JFR configuration files%n%n"
			+ DiagnosticUtils.SYNTAX_PREFIX + DIAGNOSTICS_JFR_CONFIGURE + JFR_CONFIGURE_OPTION_HELP;

	static final String DIAGNOSTICS_JFR_STOP_HELP = "Stop a JFR recording%n%n"
			+ DiagnosticUtils.SYNTAX_PREFIX + DiagnosticUtils.FORMAT_PREFIX + DIAGNOSTICS_JFR_STOP + JFR_STOP_OPTION_HELP;

	private static long convertToMilliseconds(String timeValue) {
		long timeInMilli = 0L;

		// Extract the numeric part and the time unit.
		String numericPart = timeValue.replaceAll("[^0-9]", "");

		// Convert the unit to lower case for consistency.
		String timeUnit = timeValue.replaceAll("[0-9]", "").toLowerCase();

		// Parse the numeric part.
		long time = Long.parseLong(numericPart);

		// Convert to milliseconds based on the unit.
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
			// No unit or unrecognized unit, return ERROR_NO_TIME_UNIT.
			timeInMilli = ERROR_NO_TIME_UNIT;
			break;
		}
		return timeInMilli;
	}

	/**
	 * Parse a time parameter, and return the duration in milliseconds.
	 * If the time unit is missing, ERROR_NO_TIME_UNIT is returned.
	 * If the paramName is not in parameters, ERROR_NO_TIME_DURATION is returned.
	 *
	 * @param paramName the parameter name
	 * @param parameters the parameter array
	 *
	 * @return the duration in milliseconds,
	 *         ERROR_NO_TIME_UNIT if no time unit,
	 *         ERROR_NO_TIME_DURATION if paramName wasn't found.
	 */
	private static long parseTimeParameter(String paramName, String[] parameters) {
		String prefix = paramName + "=";
		for (String param : parameters) {
			if (param.startsWith(prefix)) {
				return convertToMilliseconds(param.substring(prefix.length()));
			}
		}
		return ERROR_NO_TIME_DURATION;
	}

	private static String parseStringParameter(String paramName, String[] parameters, String defaultValue) {
		String prefix = paramName + "=";
		for (String param : parameters) {
			if (param.startsWith(prefix)) {
				return param.substring(prefix.length());
			}
		}
		return defaultValue;
	}

	static DiagnosticProperties doJFR(String diagnosticCommand) {
		if (VM.isStartFlightRecordingSpecified()) {
			return DiagnosticProperties.makeStringResult("Cannot use jcmd JFR options at the same time as -XX:startFlightRecording.");
		}

		DiagnosticProperties result = null;
		// Split the command and arguments.
		String[] parts = diagnosticCommand.split(DiagnosticUtils.DIAGNOSTICS_OPTION_SEPARATOR);
		IPC.logMessage("doJFR() diagnosticCommand = " + diagnosticCommand);
		// Ensure there's at least one part for the command.
		if (parts.length == 0) {
			return DiagnosticProperties.makeErrorProperties("Error: No JFR command specified");
		}
		String command = parts[0].trim();
		String[] parameters = Arrays.copyOfRange(parts, 1, parts.length);
		String fileName = parseStringParameter("filename", parameters, null);
		IPC.logMessage("doJFR(): filename = ", fileName);

		IPC.logMessage("doJFR(): JFR support is enabled with V1 implementation");
		try {
			switch (command) {
			case DIAGNOSTICS_JFR_START:
				if (VM.isJFRRecordingStarted()) {
					result = DiagnosticProperties.makeErrorProperties("One JFR recording is in progress ["
							+ jfrRecordingFileName + "], only one recording is allowed at a time.");
				} else {
					// Only JFR.start command is allowed to change the recording filename.
					boolean setFileName = (fileName != null) && !fileName.isEmpty();
					if (setFileName) {
						// The recording filename should be set before VM.startJFR() which invokes JFRWriter:openJFRFile().
						if (!VM.setJFRRecordingFileName(fileName)) {
							return DiagnosticProperties.makeErrorProperties("setJFRRecordingFileName() failed");
						} else {
							jfrRecordingFileName = fileName;
						}
					}
					long duration = parseTimeParameter("duration", parameters);
					IPC.logMessage("doJFR(): duration = " + duration);
					if (duration == ERROR_NO_TIME_UNIT) {
						return DiagnosticProperties.makeErrorProperties("The duration doesn't have a time unit.");
					}
					VM.startJFR();
					if (duration > 0) {
						Timer timer = new Timer(true);
						TimerTask jfrDumpTask = new TimerTask() {
							public void run() {
								if (VM.isJFRRecordingStarted()) {
									VM.stopJFR();
								}
							}
						};
						timer.schedule(jfrDumpTask, duration);
					} else {
						// The recording is on until JFR.stop.
					}
					result = DiagnosticProperties.makeStringResult("Start JFR recording to " + jfrRecordingFileName);
				}
				break;
			case DIAGNOSTICS_JFR_STOP:
				if (!VM.isJFRRecordingStarted()) {
					result = DiagnosticProperties.makeErrorProperties("Could not stop recording ["
							+ jfrRecordingFileName + "], run JFR.start first.");
				} else {
					VM.stopJFR();
					result = DiagnosticProperties.makeStringResult("Stop JFR recording, and dump all Java threads to "
							+ jfrRecordingFileName);
				}
				break;
			case DIAGNOSTICS_JFR_DUMP:
				if (!VM.isJFRRecordingStarted()) {
					result = DiagnosticProperties.makeErrorProperties("Could not create a JFR recording ["
							+ jfrRecordingFileName + "], run JFR.start first.");
				} else {
					VM.jfrDump();
					result = DiagnosticProperties.makeStringResult("Dump all Java threads to " + jfrRecordingFileName);
				}
				break;
			default:
				result = DiagnosticProperties.makeErrorProperties("Command not recognized: " + command);
				break;
			}
		} catch (Exception e) {
			result = DiagnosticProperties.makeErrorProperties("Error in JFR: " + e.getMessage());
		}

		return result;
	}

/*[IF JAVA_SPEC_VERSION == 17]*/
	static DiagnosticProperties doJFRv2(String diagnosticCommand) {
		// Split the command and arguments.
		String[] parts = diagnosticCommand.split(DiagnosticUtils.DIAGNOSTICS_OPTION_SEPARATOR);
		IPC.logMessage("doJFRv2() diagnosticCommand = " + diagnosticCommand);
		String command = parts[0];
		String execArgs = "";
		if (parts.length > 1) {
			execArgs = diagnosticCommand.substring(command.length() + 1);
		}
		command = command.trim();

		DiagnosticProperties result;
		VMLangAccess access = VM.getVMLangAccess();
		String[] jfrDcmdResult;
		try {
			switch (command) {
			case DIAGNOSTICS_JFR_START:
				jfrDcmdResult = access.doJFRDCmdStartExecute(execArgs);
				if (LOGGING_ENABLED == loggingStatus) {
					for (String str : jfrDcmdResult) {
						IPC.logMessage("JFR.start Result string: " + str);
					}
				}
				result = DiagnosticProperties.makeStringResult(Stream.of(jfrDcmdResult).collect(Collectors.joining("\n")));
				break;
			case DIAGNOSTICS_JFR_STOP:
				jfrDcmdResult = access.doJFRDCmdStopExecute(execArgs);
				if (LOGGING_ENABLED == loggingStatus) {
					for (String str : jfrDcmdResult) {
						IPC.logMessage("JFR.stop Result string = " + str);
					}
				}
				result = DiagnosticProperties.makeStringResult(Stream.of(jfrDcmdResult).collect(Collectors.joining("\n")));
				break;
			case DIAGNOSTICS_JFR_DUMP:
				jfrDcmdResult = access.doJFRDCmdDumpExecute(execArgs);
				if (LOGGING_ENABLED == loggingStatus) {
					for (String str : jfrDcmdResult) {
						IPC.logMessage("JFR.dump Result string = " + str);
					}
				}
				result = DiagnosticProperties.makeStringResult(Stream.of(jfrDcmdResult).collect(Collectors.joining("\n")));
				break;
			case DIAGNOSTICS_JFR_CONFIGURE:
				String paramRepositoryPath = null;
				String paramDumpPath = null;
				Integer paramStackDepth = null;
				Long paramGlobalBufferCount = null;
				Long paramGlobalBufferSize = null;
				Long paramThreadBufferSize = null;
				Long paramMemorySize = null;
				Long paramMaxChunkSize = null;
				Boolean paramSampleThreads = null;
				for (int index = 1; index < parts.length; index++) {
					String part = parts[index];
					int valueStart = part.indexOf('=');
					if (valueStart == -1) {
						return DiagnosticProperties.makeErrorProperties(
								"unrecognized JFR.configure option: " + part);
					}
					String partName = part.substring(0, valueStart);
					String partValue = part.substring(valueStart + 1);
					IPC.logMessage("DIAGNOSTICS_JFR_CONFIGURE partName = " + partName + ", found partValue = " + partValue);
					// The partName has to match those JFR.configure options at
					// https://github.com/ibmruntimes/openj9-openjdk-jdk17/blob/openj9/src/jdk.jcmd/share/man/jcmd.1
					switch (partName) {
					case "repositorypath":
						paramRepositoryPath = partValue;
						break;
					case "dumppath":
						paramDumpPath = partValue;
						break;
					case "stackdepth":
						paramStackDepth = Integer.valueOf(partValue);
						if (paramStackDepth < 0) {
							return result = DiagnosticProperties.makeErrorProperties(
									"java.lang.IllegalArgumentException: Parsing error stackdepth value: negative values not allowed");
						}
						break;
					case "globalbuffercount":
						paramGlobalBufferCount = Long.valueOf(partValue);
						if (paramGlobalBufferCount < 0) {
							return result = DiagnosticProperties.makeErrorProperties(
									"java.lang.IllegalArgumentException: Parsing error globalbuffercount value: negative values not allowed");
						}
						break;
					case "globalbuffersize":
						paramGlobalBufferSize = Long.valueOf(partValue);
						if (paramGlobalBufferSize < 0) {
							return result = DiagnosticProperties.makeErrorProperties(
									"java.lang.IllegalArgumentException: Parsing error globalbuffersize value: negative values not allowed");
						}
						break;
					case "thread_buffer_size":
						paramThreadBufferSize = Long.valueOf(partValue);
						if (paramThreadBufferSize < 0) {
							return result = DiagnosticProperties.makeErrorProperties(
									"java.lang.IllegalArgumentException: Parsing error thread_buffer_size value: negative values not allowed");
						}
						break;
					case "memorysize":
						paramMemorySize = Long.valueOf(partValue);
						if (paramMemorySize < 0) {
							return result = DiagnosticProperties.makeErrorProperties(
									"java.lang.IllegalArgumentException: Parsing error memory size value: negative values not allowed");
						}
						break;
					case "maxchunksize":
						paramMaxChunkSize = Long.valueOf(partValue);
						if (paramMaxChunkSize < 0) {
							return result = DiagnosticProperties.makeErrorProperties(
									"java.lang.IllegalArgumentException: Parsing error maxchunksize size value: negative values not allowed");
						}
						break;
					case "samplethreads":
						paramSampleThreads = Boolean.valueOf(partValue);
						break;
					default:
						return DiagnosticProperties.makeErrorProperties(
								"unrecognized JFR.configure option name: " + partName);
					}
				}
				IPC.logMessage("JFRHelpers:doJFRDCmdConfigureExecute() verbose = true"
						+ ", paramRepositoryPath = " + paramRepositoryPath
						+ ", paramDumpPath = " + paramDumpPath
						+ ", paramStackDepth = " + paramStackDepth
						+ ", paramGlobalBufferCount = " + paramGlobalBufferCount
						+ ", paramGlobalBufferSize = " + paramGlobalBufferSize
						+ ", paramThreadBufferSize = " + paramThreadBufferSize
						+ ", paramMemorySize = " + paramMemorySize
						+ ", paramMaxChunkSize = " + paramMaxChunkSize
						+ ", paramSampleThreads = " + paramSampleThreads);
				jfrDcmdResult = access.doJFRDCmdConfigureExecute(
						true, // verbose on
						paramRepositoryPath,
						paramDumpPath,
						paramStackDepth,
						paramGlobalBufferCount,
						paramGlobalBufferSize,
						paramThreadBufferSize,
						paramMemorySize,
						paramMaxChunkSize,
						paramSampleThreads);
				if (LOGGING_ENABLED == loggingStatus) {
					for (String str : jfrDcmdResult) {
						IPC.logMessage("JFR.configure Result string = " + str);
					}
				}
				result = DiagnosticProperties.makeStringResult(Stream.of(jfrDcmdResult).collect(Collectors.joining("\n")));
				break;
			default:
				result = DiagnosticProperties.makeErrorProperties("JFR command not recognized: " + command);
				break;
			}
		} catch (Exception e) {
			result = DiagnosticProperties.makeErrorProperties("Error in JFR: " + e.getMessage());
		}

		return result;
	}

	/**
	 * Handle a JFR.* command when Flight Recorder is unavailable, reporting the reason.
	 *
	 * @param diagnosticCommand the full command string (ignored)
	 * @return a result carrying the unavailability message
	 */
	static DiagnosticProperties getJFRModuleUnavailableMessage(String diagnosticCommand) {
		String[] message = VM.getVMLangAccess().getJFRModuleUnavailableMessage();
		if (null == message) {
			return DiagnosticProperties.makeStringResult("Flight Recorder can not be enabled.");
		}
		return DiagnosticProperties.makeStringResult(Stream.of(message).collect(Collectors.joining("\n")));
	}
/*[ENDIF] JAVA_SPEC_VERSION == 17 */
}
