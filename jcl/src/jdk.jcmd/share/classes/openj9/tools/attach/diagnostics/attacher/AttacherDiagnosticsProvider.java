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

package openj9.tools.attach.diagnostics.attacher;

import static com.ibm.oti.util.Msg.getString;
import static openj9.tools.attach.diagnostics.base.DiagnosticUtils.COMMAND_STRING;
import static openj9.tools.attach.diagnostics.base.DiagnosticUtils.DIAGNOSTICS_GC_CLASS_HISTOGRAM;
import static openj9.tools.attach.diagnostics.base.DiagnosticUtils.DIAGNOSTICS_GC_CONFIG;
import static openj9.tools.attach.diagnostics.base.DiagnosticUtils.DIAGNOSTICS_OPTION_SEPARATOR;
import static openj9.tools.attach.diagnostics.base.DiagnosticUtils.DIAGNOSTICS_THREAD_PRINT;
import static openj9.tools.attach.diagnostics.base.DiagnosticUtils.LOCKS_OPTION;

import java.io.IOException;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;
import java.util.Properties;
import java.util.function.BiFunction;
import java.util.function.Predicate;

import com.ibm.tools.attach.attacher.OpenJ9AttachProvider;
import com.ibm.tools.attach.attacher.OpenJ9VirtualMachine;
import com.ibm.tools.attach.target.IPC;
import com.sun.tools.attach.AttachNotSupportedException;

import openj9.tools.attach.diagnostics.base.DiagnosticProperties;
import openj9.tools.attach.diagnostics.base.DiagnosticUtils;
import openj9.tools.attach.diagnostics.base.DiagnosticsInfo;
import openj9.tools.attach.diagnostics.info.HeapClassInfo;
import openj9.tools.attach.diagnostics.info.HeapConfigInfo;
import openj9.tools.attach.diagnostics.info.ThreadGroupInfo;

/**
 * This class allows a Attach API attacher to query a target JVM about
 * diagnostic information such as JIT compilation, threads, classes, etc.
 * Instances must be created using the getDiagnostics() factory method.
 *
 */
public class AttacherDiagnosticsProvider {

	private OpenJ9VirtualMachine vm;
	static final Map<String, BiFunction<String, DiagnosticProperties, String>> toStringTable;
	static {
		toStringTable = new HashMap<>();
		toStringTable.put(DIAGNOSTICS_GC_CLASS_HISTOGRAM, (s, p) -> formatHeapStatistics(s, p));
		toStringTable.put(DIAGNOSTICS_GC_CONFIG, (s, p) -> formatHeapConfig(s, p));
		toStringTable.put(DIAGNOSTICS_THREAD_PRINT, (s, p) -> formatThreadInfo(s, p));
		toStringTable.put(DIAGNOSTICS_THREAD_PRINT, (s, p) -> printStatus(s, p));
	}


	/**
	 * Acquire the stacks of all running threads in the current VM and encode them
	 * into a set of properties.
	 * 
	 * @param printSynchronizers indicate if ownable synchronizers should be printed
	 * @return properties object
	 * @throws IOException on communication error
	 */
	public DiagnosticsInfo getThreadGroupInfo(boolean printSynchronizers) throws IOException {
		IPC.logMessage("enter getRemoteThreadGroupInfo"); //$NON-NLS-1$
		checkAttached();
		Properties threadInfo = vm.getThreadInfo();
		DiagnosticProperties.dumpPropertiesIfDebug("Properties from target:", threadInfo); //$NON-NLS-1$
		ThreadGroupInfo info = new ThreadGroupInfo(threadInfo, printSynchronizers);
		IPC.logMessage("exit getRemoteThreadGroupInfo"); //$NON-NLS-1$
		return info;
	}

	private static String formatHeapStatistics(@SuppressWarnings("unused") String unused, DiagnosticProperties props) {
		String result;
		try {
			HeapClassInfo info = new HeapClassInfo(props.toProperties());
			result = info.toString();
		} catch (IOException e) {
			DiagnosticProperties.dumpPropertiesIfDebug("HeapClassInfo properties", props); //$NON-NLS-1$
			result = "error in heap information: " + e.getMessage(); //$NON-NLS-1$
		}
		return result;
	}

	private static String formatHeapConfig(@SuppressWarnings("unused") String unused, DiagnosticProperties props) {
		String result;
		try {
			HeapConfigInfo info = new HeapConfigInfo(props.toProperties());
			result = info.toString();
		} catch (IOException e) {
			DiagnosticProperties.dumpPropertiesIfDebug("HeapConfigInfo properties", props); //$NON-NLS-1$
			result = "error in heap information: " + e.getMessage(); //$NON-NLS-1$
		}
		return result;
	}

	private static String printStatus(String commandString, DiagnosticProperties props) {
		String result = null;
			try {
				if (props.containsField(IPC.PROPERTY_DIAGNOSTICS_ERROR)
						&& props.getBoolean(IPC.PROPERTY_DIAGNOSTICS_ERROR)) {
					String targetMsg = props.getPropertyOrNull(IPC.PROPERTY_DIAGNOSTICS_ERRORMSG);
					String msg = props.getPropertyOrNull(IPC.PROPERTY_DIAGNOSTICS_ERRORTYPE);
					if ((null != targetMsg) && !targetMsg.isEmpty()) {
						msg = msg + ": " + targetMsg; //$NON-NLS-1$
					}
					result = msg;
				}
			} catch (IOException e) {
				/* ignore */
			}
			if (null == result) {
				result = commandString + ": command failed"; //$NON-NLS-1$
			}
		return result;
	}

	private static String formatThreadInfo(String commandString, DiagnosticProperties props) {
		String result;
		try {
			boolean doLocks = Arrays.stream(commandString
					.split(DIAGNOSTICS_OPTION_SEPARATOR))
					.anyMatch(Predicate.isEqual(LOCKS_OPTION));
			result = new ThreadGroupInfo(props.toProperties(), doLocks).toString();
		} catch (IOException e) {
			DiagnosticProperties.dumpPropertiesIfDebug("ThreadGroupInfo properties", props); //$NON-NLS-1$
			result = "error in thread information: " + e.getMessage(); //$NON-NLS-1$
		}
		return result;
	}

	/**
	 * Request thread information, including stack traces, from a target VM.
	 * 
	 * @param diagnosticCommand name of command to execute
	 * @return properties object containing serialized thread information
	 * @throws IOException in case of a communication error
	 */
	public Properties executeDiagnosticCommand(String diagnosticCommand) throws IOException {
		IPC.logMessage("enter executeDiagnosticCommand", diagnosticCommand); //$NON-NLS-1$
		checkAttached();
		Properties info = vm.executeDiagnosticCommand(diagnosticCommand);
		DiagnosticProperties.dumpPropertiesIfDebug("Properties from target:", info); //$NON-NLS-1$
		IPC.logMessage("exit getRemoteThreadGroupInfo"); //$NON-NLS-1$
		return info;
	}

	/**
	 * Produce a formatted copy of the results produced by executeDiagnosticCommand.
	 * 
	 * @param props            properties object containing the results
	 * @return formatted version of the results
	 */
	public static String resultToString(Properties props) {
		String result;
		String diagnosticCommand = props.getProperty(COMMAND_STRING, DiagnosticUtils.EMPTY_STRING); //$NON-NLS-1$

		String[] commandRoot = diagnosticCommand.split(DIAGNOSTICS_OPTION_SEPARATOR);
		BiFunction<String, DiagnosticProperties, String> cmd = toStringTable.get(commandRoot[0]);
		if (null == cmd) {
			result = "Command " + cmd + "not recognized"; //$NON-NLS-1$ //$NON-NLS-2$
		} else {
			result = cmd.apply(diagnosticCommand, new DiagnosticProperties(props));
		}
		return result;
	}

	/**
	 * Call equivalent com.sun.tools.attach.VirtualMachine method.
	 * 
	 * @param vmid ID of target
	 * @throws IOException on communication error
	 */
	public void attach(String vmid) throws IOException {
		OpenJ9AttachProvider attachProv = new OpenJ9AttachProvider();
		IPC.logMessage("DiagnosticsProviderImpl attaching to ", vmid); //$NON-NLS-1$
		try {
			vm = attachProv.attachVirtualMachine(vmid);
		} catch (AttachNotSupportedException e) {
			/*[MSG "K0809", "Exception connecting to {0}"] */
			throw new IOException(getString("K0809", vmid)); //$NON-NLS-1$
		}
		IPC.logMessage("DiagnosticsProviderImpl attached to ", vmid); //$NON-NLS-1$
	}

	/**
	 * Call equivalent com.sun.tools.attach.VirtualMachine method.
	 * 
	 * @throws IOException on communication error
	 */
	public void detach() throws IOException {
		if (null != vm) {
			vm.detach();
			vm = null;
		}
	}

	/**
	 * Call equivalent com.sun.tools.attach.VirtualMachine method.
	 * 
	 * @return properties from target VM
	 * @throws IOException on communication error
	 */
	public Properties getSystemProperties() throws IOException {
		checkAttached();
		return vm.getSystemProperties();
	}

	/**
	 * Call equivalent com.sun.tools.attach.VirtualMachine method.
	 * 
	 * @return properties from target VM
	 * @throws IOException on communication error
	 */
	public Properties getAgentProperties() throws IOException {
		checkAttached();
		return vm.getAgentProperties();
	}

	private void checkAttached() throws IOException {
		if (null == vm) {
			/*[MSG "K0544", "Target not attached"] */
			throw new IOException(getString("K0554")); //$NON-NLS-1$
		}
	}

}
