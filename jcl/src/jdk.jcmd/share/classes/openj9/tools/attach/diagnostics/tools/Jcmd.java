/*[INCLUDE-IF JAVA_SPEC_VERSION >= 8]*/
/*******************************************************************************
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
 *******************************************************************************/
package openj9.tools.attach.diagnostics.tools;

import java.io.IOException;
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
			System.out.printf(HELPTEXT);
			return;
		}

		List<AttachProvider> providers = AttachProvider.providers();
		if ((providers == null) || providers.isEmpty()) {
			System.err.println("no attach providers available"); //$NON-NLS-1$
			return;
		}

		AttachProvider openj9Provider = providers.get(0);
		List<VirtualMachineDescriptor> vmds = openj9Provider.listVirtualMachines();
		if ((vmds == null) || vmds.isEmpty()) {
			System.err.println("no VMs found"); //$NON-NLS-1$
			return;
		}

		/* An empty argument list is a request for a list of VMs. */
		final String firstArgument = (0 == args.length) ? "-l" : args[0]; //$NON-NLS-1$
		if ("-l".equals(firstArgument)) { //$NON-NLS-1$
			for (VirtualMachineDescriptor vmd : vmds) {
				StringBuilder outputBuffer = new StringBuilder(vmd.id());
				Util.getTargetInformation(openj9Provider, vmd, false, false, true, outputBuffer);
				System.out.println(outputBuffer.toString());
			}
		} else {
			String command = DiagnosticUtils.makeJcmdCommand(args, 1);
			if (command.isEmpty()) {
				System.err.printf("There is no jcmd command.%n"); //$NON-NLS-1$
				System.out.printf(HELPTEXT);
			} else {
				AttacherDiagnosticsProvider diagProvider = new AttacherDiagnosticsProvider();
				List<String> vmids = Util.findMatchVMIDs(vmds, firstArgument);
				boolean exceptionThrown = false;
				for (String vmid : vmids) {
					if (!vmid.equals(firstArgument)) {
						// skip following if firstArgument is a VMID
						// keep the output compatible with the old behavior (only one VMID accepted)
						System.out.println(vmid + ":"); //$NON-NLS-1$
					}
					try {
						IPC.logMessage("attaching vmid = " + vmid); //$NON-NLS-1$
						diagProvider.attach(vmid);
						try {
							Util.runCommandAndPrintResult(diagProvider, command, command);
						} finally {
							diagProvider.detach();
						}
					} catch (IOException e) {
						Util.handleCommandException(vmid, e);
						exceptionThrown = true;
						// keep iterating the rest of VMIDs
					}
				}
				if (exceptionThrown) {
					System.out.printf(HELPTEXT);
				}
			}
		}
	}
}
