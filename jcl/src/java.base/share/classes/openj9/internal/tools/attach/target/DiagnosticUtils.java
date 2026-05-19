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

import java.io.File;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.nio.file.InvalidPathException;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;
import java.util.function.Function;

import com.ibm.oti.vm.VM;

/*[IF CRIU_SUPPORT]*/
import java.util.Properties;
/*[ENDIF] CRIU_SUPPORT */
/*[IF CRAC_SUPPORT | CRIU_SUPPORT]*/
import openj9.internal.criu.InternalCRIUSupport;
/*[ENDIF] CRAC_SUPPORT | CRIU_SUPPORT */
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

	static final String FORMAT_PREFIX = " Format: ";
	static final String SYNTAX_PREFIX = "Syntax : ";

	private static final String HEAP_DUMP_OPTION_HELP = " [request=<options>] [opts=<options>] [<file path>]%n"
			+ " Set optional request= and opts= -Xdump options. The order of the parameters does not matter.%n";

	private static final String OTHER_DUMP_OPTION_HELP = " [request=<options>] [<file path>]%n"
				+ " Set optional request= -Xdump options. The order of the parameters does not matter.%n";

	private static final String HEAPSYSTEM_DUMP_OPTION_HELP =
			" heap dumps default to request=exclusive+compact+prepwalk and system dumps default to request=exclusive+prepwalk"
			+ " rather than the -Xdump:<type>:defaults setting.%n";

	private static final String GENERIC_DUMP_OPTION_HELP =
			" <file path> is optional, otherwise a default path/name is used.%n"
			+ " Relative paths are resolved to the target's working directory.%n"
			+ " The dump agent may choose a different file path if the requested file exists.%n";

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

/*[IF CRIU_SUPPORT]*/
	/**
	 * The system property prefix for the key/value pairs specified via CRIU.checkpoint.
	 */
	private static final String CRIU_SYSTEM_PROPERTY_PREFIX = "openj9.internal.criu.";
/*[ENDIF] CRIU_SUPPORT */

/*[IF CRAC_SUPPORT]*/
	/**
	 * Generate a checkpoint via CRIUSupport using a compatible name for CRaC enabled JVM.
	 */
	private static final String DIAGNOSTICS_JDK_CHECKPOINT = "JDK.checkpoint";
/*[ENDIF] CRAC_SUPPORT */

/*[IF CRIU_SUPPORT]*/
	/**
	 * Generate a checkpoint via CRIUSupport using a compatible name for CRIU enabled JVM.
	 */
	private static final String DIAGNOSTICS_CRIU_CHECKPOINT = "CRIU.checkpoint";
/*[ENDIF] CRIU_SUPPORT */

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
	 * Use these to separate arguments in a diagnostic command string.
	 */
	public static final String DIAGNOSTICS_OPTION_SEPARATOR = ",";
	public static final String DIAGNOSTICS_PROPERTY_SEPARATOR = "=";

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
		int optionsLength = options.length;
/*[IF JFR_SUPPORT]*/
		if (optionsLength >= 2) {
			// there is a jcmd command
			if (JFR.DIAGNOSTICS_JFR_START.equalsIgnoreCase(options[1])) {
				// search JFR.start options
				for (int i = 2; i < optionsLength; i++) {
					String option = options[i];
					IPC.logMessage("makeJcmdCommand: option = ", option);
					if (option.startsWith("filename=")) {
						String fileName = option.substring(option.indexOf("=") + 1);
						try {
							Path filePath = Paths.get(fileName);
							if (!filePath.isAbsolute()) {
								// default recording file path is jcmd current working directory
								String jcmdPWD = Paths.get("").toAbsolutePath().toString();
								fileName = jcmdPWD + File.separator + fileName;
								IPC.logMessage("makeJcmdCommand: absolute filename = ", fileName);
								// replace existing entry with an absolute jcmd pwd path for the target VM
								options[i] = "filename=" + fileName;
							}
						} catch (InvalidPathException ipe) {
							// ignore this exception and keep the original entry
							IPC.logMessage("makeJcmdCommand: ipe = ", ipe.getMessage());
						}
						// only one filename is allowed
						break;
					}
				}
			}
		}
/*[ENDIF] JFR_SUPPORT */
		String cmd = String.join(DIAGNOSTICS_OPTION_SEPARATOR, Arrays.asList(options).subList(skip, optionsLength));
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
					if (!foundRequest) {
						// set default options if the user didn't specify
						if (heapDump) {
							request.append(separator).append("request=exclusive+compact+prepwalk");
						} else if (systemDump) {
							request.append(separator).append("request=exclusive+prepwalk");
						}
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

/*[IF CRAC_SUPPORT | CRIU_SUPPORT]*/
	private static DiagnosticProperties parseCheckpointCommands(String diagnosticCommand) {
		DiagnosticProperties result = null;
		String[] parts = diagnosticCommand.split(DIAGNOSTICS_OPTION_SEPARATOR);
		if (parts.length > 1) {
/*[IF CRAC_SUPPORT]*/
			if (DIAGNOSTICS_JDK_CHECKPOINT.equalsIgnoreCase(parts[0])) {
				// parts[0] is DIAGNOSTICS_JDK_CHECKPOINT
				result = DiagnosticProperties.makeStringResult(DIAGNOSTICS_JDK_CHECKPOINT + " doesn't take any parameters");
			} else
/*[ENDIF] CRAC_SUPPORT */
			{
/*[IF CRIU_SUPPORT]*/
				// parts[0] is DIAGNOSTICS_CRIU_CHECKPOINT
				// the command format: CRIU.checkpoint,imageDir=/path/to/cpData,logLevel=4
				for (int index = 1; index < parts.length; index++) {
					IPC.logMessage("parseCheckpointCommands: ", "parts[" + index + "] : " + parts[index]);
					String[] sysPropValue = parts[index].split(DIAGNOSTICS_PROPERTY_SEPARATOR);
					if (sysPropValue.length != 2) {
						result = DiagnosticProperties.makeErrorProperties("Expected Jcmd format: " + parts[0]
								+ ",imageDir=/path/to/cpData,logLevel=4"
								+ " that creates system properties <openj9.internal.criu.imageDir> with the value </path/to/cpData>"
								+ " and <openj9.internal.criu.logLevel> with the value <4>, but got: " + diagnosticCommand);
						break;
					}
					Properties props = VM.internalGetProperties();
					props.setProperty(CRIU_SYSTEM_PROPERTY_PREFIX + sysPropValue[0], sysPropValue[1]);
				}
/*[ENDIF] CRIU_SUPPORT */
			}
		}
		return result;
	}
/*[ENDIF] CRAC_SUPPORT | CRIU_SUPPORT */

/*[IF CRAC_SUPPORT]*/
	private static DiagnosticProperties doCRaCCheckpointJVM(String diagnosticCommand) {
		DiagnosticProperties result;
		if (InternalCRIUSupport.isCRaCSupportEnabled()) {
			result = parseCheckpointCommands(diagnosticCommand);
			if (result == null) {
				Thread checkpointThread = new Thread(() -> {
					try {
						jdk.crac.Core.checkpointRestore();
					} catch (Throwable t) {
						t.printStackTrace();
					}
				});
				checkpointThread.start();
				result = DiagnosticProperties.makeStringResult("JVM checkpoint requested");
			}
		} else {
			result = DiagnosticProperties.makeStringResult("CRaC support is not enabled, JVM can't perform a checkpoint");
		}
		return result;
	}
/*[ENDIF] CRAC_SUPPORT */

/*[IF CRIU_SUPPORT]*/
	private static DiagnosticProperties doCRIUCheckpointJVM(String diagnosticCommand) {
		DiagnosticProperties result;
		if (InternalCRIUSupport.isCRIUSupportEnabled()) {
			result = parseCheckpointCommands(diagnosticCommand);
			if (result == null) {
				Thread checkpointThread = new Thread(() -> {
					try {
						InternalCRIUSupport.getInternalCRIUSupport().setCheckpointDefaultParams().checkpointJVM();
					} catch (Throwable t) {
						t.printStackTrace();
					}
				});
				checkpointThread.start();
				result = DiagnosticProperties.makeStringResult("JVM checkpoint requested");
			}
		} else {
			result = DiagnosticProperties.makeStringResult("CRIU support is not enabled, JVM can't perform a checkpoint");
		}
		return result;
	}
/*[ENDIF] CRIU_SUPPORT */

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

/*[IF CRIU_SUPPORT]*/
	private static final String DIAGNOSTICS_CRIU_CHECKPOINT_HELP = "Produce a JVM checkpoint via CRIUSupport, optionally set system properties.%n"
			+ FORMAT_PREFIX + DIAGNOSTICS_CRIU_CHECKPOINT + "[,imageDir=/path/to/cpData][,logLevel=4]" + "%n"
			+ "          A prefix <" + CRIU_SYSTEM_PROPERTY_PREFIX + "> is added to the each key specified.%n"
			+ "          The sample options above create following system properties:%n"
			+ "          - a system property <openj9.internal.criu.imageDir> with the value </path/to/cpData>%n"
			+ "          - a system property <openj9.internal.criu.logLevel> with the value <4>%n"
			+ "NOTE: this utility might significantly affect the performance of the target VM.%n";
/*[ENDIF] CRIU_SUPPORT */

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
			commandTable.put(DIAGNOSTICS_JDK_CHECKPOINT, DiagnosticUtils::doCRaCCheckpointJVM);
			helpTable.put(DIAGNOSTICS_JDK_CHECKPOINT, DIAGNOSTICS_JDK_CHECKPOINT_HELP);
		}
/*[ENDIF] CRAC_SUPPORT */

/*[IF CRIU_SUPPORT]*/
		if (InternalCRIUSupport.isCRIUSupportEnabled()) {
			commandTable.put(DIAGNOSTICS_CRIU_CHECKPOINT, DiagnosticUtils::doCRIUCheckpointJVM);
			helpTable.put(DIAGNOSTICS_CRIU_CHECKPOINT, DIAGNOSTICS_CRIU_CHECKPOINT_HELP);
		}
/*[ENDIF] CRIU_SUPPORT */

/*[IF JFR_SUPPORT]*/
		if (VM.isJFREnabled()) {
/*[IF JAVA_SPEC_VERSION == 17]*/
			if (VM.isJFRV2SupportEnabled()) {
				commandTable.put(JFR.DIAGNOSTICS_JFR_START, JFR::doJFRv2);
				helpTable.put(JFR.DIAGNOSTICS_JFR_START, JFR.DIAGNOSTICS_JFR_START_HELP);

				commandTable.put(JFR.DIAGNOSTICS_JFR_DUMP, JFR::doJFRv2);
				helpTable.put(JFR.DIAGNOSTICS_JFR_DUMP, JFR.DIAGNOSTICS_JFR_DUMP_HELP);

				commandTable.put(JFR.DIAGNOSTICS_JFR_STOP, JFR::doJFRv2);
				helpTable.put(JFR.DIAGNOSTICS_JFR_STOP, JFR.DIAGNOSTICS_JFR_STOP_HELP);

				commandTable.put(JFR.DIAGNOSTICS_JFR_CONFIGURE, JFR::doJFRv2);
				helpTable.put(JFR.DIAGNOSTICS_JFR_CONFIGURE, JFR.DIAGNOSTICS_JFR_CONFIGURE_HELP);
			} else
/*[ENDIF] JAVA_SPEC_VERSION == 17 */
			{
				commandTable.put(JFR.DIAGNOSTICS_JFR_START, JFR::doJFR);
				helpTable.put(JFR.DIAGNOSTICS_JFR_START, JFR.DIAGNOSTICS_JFR_START_HELP);

				commandTable.put(JFR.DIAGNOSTICS_JFR_DUMP, JFR::doJFR);
				helpTable.put(JFR.DIAGNOSTICS_JFR_DUMP, JFR.DIAGNOSTICS_JFR_DUMP_HELP);

				commandTable.put(JFR.DIAGNOSTICS_JFR_STOP, JFR::doJFR);
				helpTable.put(JFR.DIAGNOSTICS_JFR_STOP, JFR.DIAGNOSTICS_JFR_STOP_HELP);
			}
		}
/*[ENDIF] JFR_SUPPORT */
	}
}
