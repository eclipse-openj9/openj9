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
import java.util.ArrayList;
import java.util.List;
import java.util.Properties;

import com.ibm.tools.attach.target.AttachHandler;
import com.ibm.tools.attach.target.IPC;

import openj9.tools.attach.diagnostics.attacher.AttacherDiagnosticsProvider;
import openj9.tools.attach.diagnostics.base.DiagnosticProperties;
import openj9.tools.attach.diagnostics.base.DiagnosticUtils;

/**
 * JMap 
 * A tool for listing heap information about another Java process
 *
 */
public class Jmap {

	private static List<String> vmids;
	private static boolean config;
	private static boolean histo;
	private static boolean live;
	@SuppressWarnings("nls")
	private static String HELPTEXT = "jstack: listing thread information about another Java process%n"
			+ " Usage:%n"
			+ "    jmap <option>* <vmid>%n"
			+ "        <vmid>: Attach API VM ID as shown in jps or other Attach API-based tools%n"
			+ "        <vmid>s are read from stdin if none are supplied as arguments%n"
			+ "    -heap: print the heap configuration%n"
			+ "    -histo: print statistics about classes on the heap, including number of objects and aggregate size%n"
			+ "    -histo:live : Print only live objects%n"
			+ "    -J: supply arguments to the Java VM running jps%n"
			+ " NOTE: this utility may significantly affect the performance of the target JVM.%n"
			+ "At least one option must be selected.";
	/**
	 * Print a list of Java processes and information about them.
	 * @param args Arguments to the application
	 */
	public static void main(String[] args) {

		if (!parseArguments(args)) {
			System.exit(1);
		}
		AttacherDiagnosticsProvider diagProvider = new AttacherDiagnosticsProvider();

		String myId = AttachHandler.getVmId();
		for (String vmid: vmids) {
			if (vmid.equals(myId)) {
				continue;
			}
			/* if the ID looks like a process ID, check if it is running */
			if (vmid.matches("\\d+")) { //$NON-NLS-1$
				long pid = Long.parseLong(vmid);
				if (!IPC.processExists(pid)) {
					continue;
				}
			}
			try {
				diagProvider.attach(vmid);
				if (histo) {
					String cmd = DiagnosticUtils.makeHeapHistoCommand(live);
					Properties props = diagProvider.executeDiagnosticCommand(cmd);
					DiagnosticProperties.dumpPropertiesIfDebug("jmap result:", props); //$NON-NLS-1$
					System.out.printf("Heap class statistics for %s\n", vmid); //$NON-NLS-1$
					System.out.print(AttacherDiagnosticsProvider.resultToString(props));
				} else if (config) {
					String cmd = DiagnosticUtils.DIAGNOSTICS_GC_CONFIG;
					Properties props = diagProvider.executeDiagnosticCommand(cmd);
					DiagnosticProperties.dumpPropertiesIfDebug("jmap result:", props); //$NON-NLS-1$
					System.out.printf("Heap configuration for %s\n", vmid); //$NON-NLS-1$
					System.out.print(AttacherDiagnosticsProvider.resultToString(props));
					
				}
			} catch (Exception e) {
				System.err.printf("Error getting data from %s", vmid); //$NON-NLS-1$
				final String msg = e.getMessage();
				if (null == msg) {
					System.err.println();
				} else {
					if (msg.matches(IPC.INCOMPATIBLE_JAVA_VERSION)) {
						System.err.println(": incompatible target JVM"); //$NON-NLS-1$
					} else {
						System.err.printf(": %s%n", msg); //$NON-NLS-1$
					}
				}
				if (DiagnosticProperties.isDebug) {
					e.printStackTrace();
				}
			} finally {
				try {
					diagProvider.detach();
				} catch (IOException e) {
					/* ignore */
				}
			}
		}
	}

	@SuppressWarnings("nls")
	private static boolean parseArguments(String[] args) {
		boolean okay = true;
		vmids = new ArrayList<>();
		config = false;
		histo = false;
		live = false;
		int optionsSelected = 0;
		String errorMessage = "";
		vmids = new ArrayList<>();
		for (String a: args) {
			if (!a.startsWith("-")) {
				vmids.add(a);
			} else if (a.startsWith("-histo")) {
				histo = true;
				optionsSelected +=1;
				if (a.endsWith(":live")) {
					live = true;
				}
			} else if (a.startsWith("-heap")) {
				config = true;
				optionsSelected +=1;
				if (a.endsWith(":live")) {
					live = true;
				}
			} else {
				okay = false;
				errorMessage = "unrecognized option " + a;
				break;
			}
		}
		if (1 != optionsSelected) {
			okay = false;
			errorMessage = "exactly one option must be selected";
		}
		if (!okay) {
			System.err.printf("%s.%n", errorMessage); //$NON-NLS-1$
			System.err.printf(HELPTEXT);
			okay = false;
		}
		if (okay && vmids.isEmpty()) {
			/* grab the VMIDs from stdin. */
			vmids = DiagnosticUtils.stdinToStringList();
			okay = !vmids.isEmpty();
		}
		return okay;
	}
}

