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

import java.io.IOException;
import java.net.SocketException;
import java.util.Arrays;
import java.util.List;

import com.sun.tools.attach.VirtualMachineDescriptor;
import com.sun.tools.attach.spi.AttachProvider;

import openj9.internal.tools.attach.target.DiagnosticUtils;
import openj9.internal.tools.attach.target.IPC;
import openj9.tools.attach.diagnostics.attacher.AttacherDiagnosticsProvider;

/**
 * Jcmd A tool for running diagnostic commands on another Java process
 *
 */
public class Jcmd {

	@SuppressWarnings("nls")
	private static final String HELPTEXT = "Usage : jcmd <vmid | display name | 0> <arguments>%n"
			+ "%n"
			+ "   -J : supply arguments to the Java VM running jcmd%n"
			+ "   -l : list JVM processes on the local machine%n"
			+ "   -h : print this help message%n"
			+ "%n"
			+ "   <vmid> : Attach API VM ID as shown in jcmd or other Attach API-based tools%n"
			+ "   <display name> : this argument is used to match (either fully or partially) the display name as shown in jcmd or other Attach API-based tools%n"
			+ "   <0> : the jcmd command will be sent to all Java processes detected by this utility%n"
			+ "%n"
			+ "   arguments:%n"
			+ "      help : print the list of diagnostic commands%n"
			+ "      help <command> : print help for the specific command%n"
			+ "      <command> [command arguments] : command from the list returned by \"help\"%n"
			+ "%n"
			+ "list JVM processes on the local machine. Default behavior when no options are specified.%n"
			+ "%n"
			+ "NOTE: this utility might significantly affect the performance of the target JVM.%n"
			+ "    The available diagnostic commands are determined by%n"
			+ "    the target VM and may vary between VMs.%n";
	@SuppressWarnings("nls")
	private static final String[] HELP_OPTIONS = {"-h", "help", "-help", "--help"};

	/**
	 * @param args Application arguments.  See help text for details.
	 */
	public static void main(String[] args) {
		if ((args.length == 1) && Arrays.stream(HELP_OPTIONS).anyMatch(args[0]::equals)) {
			IPC.logMessage("Jcmd exits with HELPTEXT and args[0] = " + args[0]); //$NON-NLS-1$
			System.out.printf(HELPTEXT);
			return;
		}

		List<AttachProvider> providers = AttachProvider.providers();
		if ((providers == null) || providers.isEmpty()) {
			IPC.logMessage("Jcmd exits when no attach providers available"); //$NON-NLS-1$
			System.err.println("no attach providers available"); //$NON-NLS-1$
			return;
		}

		AttachProvider openj9Provider = providers.get(0);
		List<VirtualMachineDescriptor> vmds = openj9Provider.listVirtualMachines();
		if ((vmds == null) || vmds.isEmpty()) {
			IPC.logMessage("Jcmd exits when no VMs found"); //$NON-NLS-1$
			System.err.println("no VMs found"); //$NON-NLS-1$
			return;
		}

		/* An empty argument list is a request for a list of VMs. */
		final String firstArgument = (0 == args.length) ? "-l" : args[0]; //$NON-NLS-1$
		if ("-l".equals(firstArgument)) { //$NON-NLS-1$
			IPC.logMessage("Jcmd -l run"); //$NON-NLS-1$
			for (VirtualMachineDescriptor vmd : vmds) {
				StringBuilder outputBuffer = new StringBuilder(vmd.id());
				Util.getTargetInformation(openj9Provider, vmd, false, false, true, outputBuffer);
				System.out.println(outputBuffer.toString());
			}
		} else {
			String command = DiagnosticUtils.makeJcmdCommand(args, 1);
			if (command.isEmpty()) {
				IPC.logMessage("Jcmd exits when there is no jcmd command"); //$NON-NLS-1$
				System.err.printf("There is no jcmd command.%n"); //$NON-NLS-1$
				System.out.printf(HELPTEXT);
			} else {
				AttacherDiagnosticsProvider diagProvider = new AttacherDiagnosticsProvider();
				List<String> vmids = Util.findMatchVMIDs(vmds, firstArgument);
				boolean exceptionThrown = false;
				int retry = 0;
				for (String vmid : vmids) {
					if (!vmid.equals(firstArgument)) {
						// skip following if firstArgument is a VMID
						// keep the output compatible with the old behavior (only one VMID accepted)
						System.out.println(vmid + ":"); //$NON-NLS-1$
					}
					do {
						try {
							IPC.logMessage("attaching vmid = " + vmid + ", retry = " + retry); //$NON-NLS-1$ //$NON-NLS-2$
							diagProvider.attach(vmid);
							try {
								Util.runCommandAndPrintResult(diagProvider, command, command);
							} finally {
								try {
									diagProvider.detach();
								} catch (SocketException se) {
									IPC.logMessage("SocketException thrown at diagProvider.detach() vmid = " + vmid, se); //$NON-NLS-1$
									break;
									// ignore this SocketException since the attaching finished anyway
								}
							}
							// attaching succeeded
							break;
						} catch (SocketException se) {
							/* Following SocketException is seen occationally at Windows
							 * https://learn.microsoft.com/en-us/previous-versions/ms832256(v=msdn.10)?redirectedfrom=MSDN
							 * java.net.SocketException: Software caused connection abort: socket write error
							 *     at java.base/java.net.SocketOutputStream.socketWrite0(Native Method)
							 *     at java.base/java.net.SocketOutputStream.socketWrite(SocketOutputStream.java:110)
							 *     at java.base/java.net.SocketOutputStream.write(SocketOutputStream.java:129)
							 *     at java.base/openj9.internal.tools.attach.target.AttachmentConnection.streamSend(AttachmentConnection.java:105)
							 *     at jdk.attach/com.ibm.tools.attach.attacher.OpenJ9VirtualMachine.executeDiagnosticCommand(OpenJ9VirtualMachine.java:318)
							 *     at jdk.jcmd/openj9.tools.attach.diagnostics.attacher.AttacherDiagnosticsProvider.executeDiagnosticCommand(AttacherDiagnosticsProvider.java:55)
							 *     at jdk.jcmd/openj9.tools.attach.diagnostics.tools.Util.runCommandAndPrintResult(Util.java:78)
							 *     at jdk.jcmd/openj9.tools.attach.diagnostics.tools.Jcmd.main(Jcmd.java:120)
							 */
							IPC.logMessage("SocketException thrown while attaching vmid = " + vmid, se); //$NON-NLS-1$
						} catch (IOException e) {
							IPC.logMessage("IOException thrown while attaching vmid = " + vmid, e); //$NON-NLS-1$
							Util.handleCommandException(vmid, e);
							exceptionThrown = true;
							break;
							// keep iterating the rest of VMIDs
						}
						if (!IPC.isWindows) {
							IPC.logMessage("Only retry on Windows platform, vmid = " + vmid); //$NON-NLS-1$
							break;
						}
						retry += 1;
						try {
							Thread.sleep(10);
						} catch (InterruptedException e) {
							IPC.logMessage("Ignore Thread.sleep() InterruptedException, vmid = " + vmid); //$NON-NLS-1$
						}
					} while (retry < Util.retry);
				}
				if (exceptionThrown) {
					System.out.printf(HELPTEXT);
				}
			}
		}
	}
}
