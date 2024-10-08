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
package openj9.tools.attach.diagnostics.tools;

import com.sun.tools.attach.AttachNotSupportedException;
import com.sun.tools.attach.VirtualMachine;
import com.sun.tools.attach.VirtualMachineDescriptor;
import com.sun.tools.attach.spi.AttachProvider;

import java.io.BufferedReader;
import java.io.File;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.IOException;
import java.io.PrintStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Map.Entry;
import java.util.Properties;
import java.util.stream.Collectors;

import openj9.internal.tools.attach.target.AttachHandler;
import openj9.internal.tools.attach.target.DiagnosticProperties;
import openj9.internal.tools.attach.target.IPC;
import openj9.tools.attach.diagnostics.attacher.AttacherDiagnosticsProvider;

/**
 * Common functions for diagnostic tools
 *
 */
public class Util {
	private static final String SUN_JAVA_COMMAND = "sun.java.command"; //$NON-NLS-1$
	private static final String SUN_JVM_ARGS = "sun.jvm.args"; //$NON-NLS-1$
	// retry 3 times by default in case of SocketException
	static final int retry = Integer.getInteger("com.ibm.tools.attach.retry", 3).intValue(); //$NON-NLS-1$

	/**
	 * Read the text from an input stream, split it into separate strings at line breaks,
	 * remove blank lines, and strip leading and trailing whitespace.
	 * @param inStream text input stream
	 * @return contents of inStream as a list of strings
	 */
	public static List<String> inStreamToStringList(InputStream inStream) {
		List<String> stdinList;
		try (BufferedReader jpsOutReader = new BufferedReader(new InputStreamReader(inStream))) {
			stdinList = jpsOutReader
					.lines()
					.map(s -> s.trim())
					.filter(s -> !s.isEmpty())
					.collect(Collectors.toList());
		} catch (IOException e) {
			stdinList = Collections.emptyList();
		}
		return stdinList;
	}

	static void runCommandAndPrintResult(AttacherDiagnosticsProvider diagProvider, String cmd, String commandName)
			throws IOException {
		Properties props = diagProvider.executeDiagnosticCommand(cmd);
		DiagnosticProperties.dumpPropertiesIfDebug(commandName + " result:", props); //$NON-NLS-1$
		String responseString = new DiagnosticProperties(props).printStringResult();
		IPC.logMessage("Util.runCommandAndPrintResult(): " + responseString); //$NON-NLS-1$
		System.out.print(responseString);
	}

	static void handleCommandException(String vmid, Exception e) {
		String format = "Error getting data from %s"; //$NON-NLS-1$
		final String msg = e.getMessage();
		if (null != msg) {
			if (msg.matches(IPC.INCOMPATIBLE_JAVA_VERSION)) {
				format += ": incompatible target JVM%n"; //$NON-NLS-1$
			} else {
				format += ": %s%n"; //$NON-NLS-1$
			}
		} else {
			format += "%n"; //$NON-NLS-1$
		}
		System.err.printf(format, vmid, msg);
		if (DiagnosticProperties.isDebug) {
			e.printStackTrace();
		}
	}

	static void getTargetInformation(AttachProvider theProvider, VirtualMachineDescriptor vmd,
			boolean printJvmArguments, boolean noPackageName, boolean printApplicationArguments, StringBuilder outputBuffer) {
		try {
			VirtualMachine theVm = theProvider.attachVirtualMachine(vmd);
			try {
				Properties vmProperties = theVm.getSystemProperties();
				String theCommand = vmProperties.getProperty(SUN_JAVA_COMMAND, ""); //$NON-NLS-1$
				String parts[] = theCommand.split("\\s+", 2); /* split into at most 2 parts: command and argument string */  //$NON-NLS-1$
				if (noPackageName) {
					String commandString = parts[0];
					int finalSeparatorPosition = -1;
					if (commandString.toLowerCase().endsWith(".jar")) { //$NON-NLS-1$
						/* the application was launched via '-jar'.  Get the file name, without directory path. */
						finalSeparatorPosition = commandString.lastIndexOf(File.pathSeparatorChar);
					} else {
						/* the application was launched using a class name */
						finalSeparatorPosition = commandString.lastIndexOf('.');
					}
					parts[0] = commandString.substring(finalSeparatorPosition + 1);
				}
				if (printApplicationArguments) {
					for (String p : parts) {
						outputBuffer.append(' ');
						outputBuffer.append(p);
					}
				} else if (parts.length > 0) { /* some Java processes do not use the Java launcher */
					outputBuffer.append(' ');
					outputBuffer.append(parts[0]);
				}
				if (printJvmArguments) {
					String jvmArguments = vmProperties.getProperty(SUN_JVM_ARGS);
					if ((null != jvmArguments) && !jvmArguments.isEmpty()) {
						outputBuffer.append(' ').append(jvmArguments);
					}
				}
			} finally {
				theVm.detach();
			}
		} catch (AttachNotSupportedException | IOException e) {
			outputBuffer.append(" <no information available>"); //$NON-NLS-1$
		}
	}

	/**
	 * Print the Properties object in key-value format.
	 * This differs from Properties.list() in that it does not truncate long values.
	 * @param out
	 * @param props
	 */
	static void printProperties(PrintStream out, Properties props) {
		for ( Entry<Object, Object> theEntry : props.entrySet()) {
			out.printf("%s=%s%n", theEntry.getKey(), theEntry.getValue()); //$NON-NLS-1$
		}
	}

	/**
	 * Print an error message if it is not null or empty.
	 * Print the help content.
	 * Terminates JVM.
	 *
	 * @param error
	 *            an error message to indicate the cause of the error
	 * @param help
	 *            the help showing the utility usages
	 */
	static void exitJVMWithReasonAndHelp(String error, String help) {
		if ((error != null) && (!error.isEmpty())) {
			System.out.println(error);
		}
		System.out.printf(help);
		System.exit(1);
	}

	@SuppressWarnings("nls")
	private static final String[] HELP_OPTIONS = { "-h", "help", "-help", "--help" };

	/**
	 * Check if the option matches one of HELP_OPTIONS
	 *
	 * @param option
	 *            the option to be checked
	 * @return true if found a match, otherwise false
	 */
	static boolean checkHelpOption(String option) {
		return Arrays.stream(HELP_OPTIONS).anyMatch(option::equalsIgnoreCase);
	}

	static List<String> findMatchVMIDs(List<VirtualMachineDescriptor> vmds, String firstArg) {
		IPC.logMessage("findMatchVMIDs firstArg = " + firstArg); //$NON-NLS-1$
		ArrayList<String> vmids = new ArrayList<>();
		String currentVMID = AttachHandler.getVmId();
		IPC.logMessage("findMatchVMIDs currentVMID = " + currentVMID); //$NON-NLS-1$
		try {
			long pid = Long.parseLong(firstArg);
			boolean includeAllVMIDs = (pid == 0);
			// this is the virtual machine identifier which is usually an OS process ID
			for (VirtualMachineDescriptor vmd : vmds) {
				String vmid = vmd.id();
				if (includeAllVMIDs || firstArg.equals(vmid)) {
					IPC.logMessage("add vmid(firstArg) = " + vmid); //$NON-NLS-1$
					if (vmid.equals(currentVMID) && !AttachHandler.selfAttachAllowed) {
						IPC.logMessage("skip self, vmid(firstArg) = " + vmid); //$NON-NLS-1$
					} else {
						vmids.add(vmid);
					}
					if (!includeAllVMIDs) {
						// exit if not include all VMIDs
						break;
					}
				} else {
					IPC.logMessage("skip vmid(firstArg) != " + vmid); //$NON-NLS-1$
				}
			}
		} catch (NumberFormatException nfe) {
			// the firstArg is not 0 or a unique VMID, search for matching
			for (VirtualMachineDescriptor vmd : vmds) {
				String displayName = vmd.displayName();
				if ((displayName != null) && displayName.contains(firstArg)) {
					String vmid = vmd.id();
					if (vmid.equals(currentVMID) && !AttachHandler.selfAttachAllowed) {
						IPC.logMessage("skip self, vmid = " + vmid //$NON-NLS-1$
								+ " with displayName = " + displayName); //$NON-NLS-1$
					} else {
						IPC.logMessage("find a match: vmid = " + vmid //$NON-NLS-1$
								+ " in displayName = " + displayName); //$NON-NLS-1$
						vmids.add(vmid);
					}
				} else {
					IPC.logMessage("skip displayName = " + displayName); //$NON-NLS-1$
				}
			}
		}
		if (vmids.isEmpty()) {
			IPC.logMessage("Util.findMatchVMIDs() returns empty vmids"); //$NON-NLS-1$
		}
		return vmids;
	}
}
