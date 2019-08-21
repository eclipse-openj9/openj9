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
import java.util.Arrays;
import java.util.List;

import com.sun.tools.attach.VirtualMachineDescriptor;
import com.sun.tools.attach.spi.AttachProvider;

import openj9.tools.attach.diagnostics.attacher.AttacherDiagnosticsProvider;
import openj9.tools.attach.diagnostics.base.DiagnosticUtils;

/**
 * Jcmd A tool for running diagnostic commands on another Java process
 *
 */
public class Jcmd {

	@SuppressWarnings("nls")
	private static final String HELPTEXT = "Usage : jcmd <vmid> <arguments>%n"
			+ "%n"
			+ "   -J : supply arguments to the Java VM running jcmd%n" 
			+ "   -l : list JVM processes on the local machine%n"
			+ "   -h : print this help message%n"
			+ "%n"
			+ "   <vmid> : Attach API VM ID as shown in jps or other Attach API-based tools%n"
			+ "%n"
			+ "   arguments:%n" 
			+ "      help : print the list of diagnostic commands%n"
			+ "      help <command> : print help for the specific command%n"
			+ "      <command> [command arguments] : command from the list returned by \"help\"%n"
			+ "%n"
			+ "list JVM processes on the local machine. Default behavior when no options are specified.%n"
			+ "%n"
			+ "NOTE: this utility may significantly affect the performance of the target JVM.%n"
			+ "    The available diagnostic commands are determined by%n"
			+ "    the target VM and may vary between VMs.%n";
	@SuppressWarnings("nls")
	private static final String[] HELP_OPTIONS = {"-h", "help", "-help", "--help"};

	/**
	 * @param args Application arguments.  See help text for details.
	 */
	public static void main(String[] args) {
		String command = null;
		/* An empty argument list is a request for a list of VMs */
		final String firstArgument = (0 == args.length) ? "-l" : args[0]; //$NON-NLS-1$
		if ("-l".equals(firstArgument)) { //$NON-NLS-1$
			List<AttachProvider> providers = AttachProvider.providers();
			AttachProvider myProvider = null;
			if (!providers.isEmpty()) {
				myProvider = providers.get(0);
			}
			if (null == myProvider) {
				System.err.println("no attach providers available"); //$NON-NLS-1$
			} else {
				for (VirtualMachineDescriptor vmd : myProvider.listVirtualMachines()) {
					StringBuilder outputBuffer = new StringBuilder(vmd.id());
					Util.getTargetInformation(myProvider, vmd, false, false, true, outputBuffer);
					System.out.println(outputBuffer.toString());
				}
			}
		} else if ((args.length == 1) && (null != firstArgument) && Arrays.stream(HELP_OPTIONS).anyMatch(firstArgument::equals)) {	
			System.out.printf(HELPTEXT);
		} else {
			command = DiagnosticUtils.makeJcmdCommand(args, 1);
			if (command.isEmpty()) {
				System.err.printf("There is no jcmd command.%n"); //$NON-NLS-1$
				System.out.printf(HELPTEXT);
			} else {
				String vmid = firstArgument;
				AttacherDiagnosticsProvider diagProvider = new AttacherDiagnosticsProvider();
				try {
					diagProvider.attach(vmid);
					try {
						Util.runCommandAndPrintResult(diagProvider, command, command); // $NON-NLS-1$
					} finally {
						diagProvider.detach();
					}
				} catch (IOException e) {
					Util.handleCommandException(vmid, e);
					System.out.printf(HELPTEXT);
				}
			}
		}
	}

}
