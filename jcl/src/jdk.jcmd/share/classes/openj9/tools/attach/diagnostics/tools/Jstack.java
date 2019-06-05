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

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintStream;
import java.time.LocalDateTime;
import java.util.ArrayList;
import java.util.List;
import java.util.stream.Collectors;

import com.ibm.tools.attach.target.AttachHandler;
import com.ibm.tools.attach.target.IPC;

import openj9.tools.attach.diagnostics.attacher.AttacherDiagnosticsProvider;
import openj9.tools.attach.diagnostics.base.DiagnosticProperties;
import openj9.tools.attach.diagnostics.base.DiagnosticsInfo;

/**
 * JStack 
 * A tool for listing thread information about another Java process
 *
 */
public class Jstack {

	private static List<String> vmids;
	private static boolean printProperties;
	private static boolean printSynchronizers;
	/**
	 * Print a list of Java processes and information about them.
	 * @param args Arguments to the application
	 */
	public static void main(String[] args) {
		PrintStream out = System.out;

		if (!parseArguments(args)) {
			System.exit(1);
		}
		AttacherDiagnosticsProvider diagProvider = new AttacherDiagnosticsProvider();

		String myId = AttachHandler.getVmId();
		out.println(LocalDateTime.now());
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
				DiagnosticsInfo groupInfo = diagProvider.getThreadGroupInfo(printSynchronizers);
				out.printf("Virtual machine: %s JVM information %s%n", vmid, groupInfo.getJavaInfo()); //$NON-NLS-1$
				out.println(groupInfo.toString());
				if (printProperties) {
					out.println("System properties:"); //$NON-NLS-1$
					out.println(diagProvider.getSystemProperties());
					out.println("Agent properties:"); //$NON-NLS-1$
					out.println(diagProvider.getAgentProperties());
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
		printProperties = DiagnosticProperties.isDebug;
		printSynchronizers = false;
		final String HELPTEXT = "jstack: listing thread information about another Java process%n"
				+ " Usage:%n"
				+ "    jstack <vmid>*%n"
				+ "        <vmid>: Attach API VM ID as shown in jps or other Attach API-based tools%n"
				+ "        <vmid>s are read from stdin if none are supplied as arguments%n"
				+ "    -p: print the target's system and agent properties%n"
				+ "    -l: Long format. Print the thread's ownable synchronizers%n"
				+ "    -J: supply arguments to the Java VM running jps%n"
				;
		vmids = new ArrayList<>();
		for (String a: args) {
			if (!a.startsWith("-")) {
				vmids.add(a);
			} else if (a.equals("-p")) {
				printProperties = true;
			} else if (a.equals("-l")) {
				printSynchronizers = true;
			} else {
				System.err.printf(HELPTEXT);
				okay = false;
				break;
			}
		}

		if (okay && vmids.isEmpty()) {
			/* grab the VMIDs from stdin. */
			try (BufferedReader jpsOutReader = new BufferedReader(new InputStreamReader(System.in))) {
				vmids = jpsOutReader.lines()
						.map(s -> s.trim())
						.filter(s -> !s.isEmpty())
						.collect(Collectors.toList());
				okay = true;
			} catch (IOException e) {
				/* ignore */
			}
		}
		return okay;
	}
}

