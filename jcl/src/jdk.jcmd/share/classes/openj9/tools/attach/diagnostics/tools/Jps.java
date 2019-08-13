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

import java.util.List;

import com.sun.tools.attach.spi.AttachProvider;
import com.sun.tools.attach.VirtualMachineDescriptor;

/**
 * Java Process Status
 * A tool for listing Java processes and their information.
 *
 */
public class Jps {

	private static boolean printApplicationArguments;
	private static boolean printJvmArguments;
	private static boolean noPackageName;
	private static boolean vmidOnly;
	/**
	 * Print a list of Java processes and information about them.
	 * @param args Arguments to the application
	 */
	public static void main(String[] args) {
		int rc = 0;
		parseArguments(args);
		final List<AttachProvider> providers = AttachProvider.providers();
		AttachProvider theProvider = null;
		if (0 != providers.size()) {
			theProvider = providers.get(0);
		} 
		if (null == theProvider) {
			System.err.println("no attach providers available"); //$NON-NLS-1$
			rc = 1;
		} else {
			List<VirtualMachineDescriptor> vmds = theProvider.listVirtualMachines();
			for (VirtualMachineDescriptor vmd : vmds) {
				StringBuilder outputBuffer = new StringBuilder(vmd.id());
				if (!vmidOnly) {
					Util.getTargetInformation(theProvider, vmd, 
							printJvmArguments, noPackageName, printApplicationArguments, outputBuffer);
				}
				System.out.println(outputBuffer.toString());
			}
		}
		System.exit(rc);
	}
	
	@SuppressWarnings("nls")
	private static void parseArguments(String[] args) {
		printApplicationArguments = false;
		printJvmArguments = false;
		noPackageName = true;
		vmidOnly = false;
		final String HELPTEXT = "jps: Print a list of Java processes and information about them%n"
				+ "    -J: supply arguments to the Java VM running jps%n"
				+ "    -l: print the application package name%n"
				+ "    -q: print only the virtual machine identifiers%n"
				+ "    -m: print the application arguments%n"
				+ "    -v: print the Java VM arguments, including those produced automatically%n";
		for (String a : args) {
			switch (a) {
			case "-l":
				noPackageName = false;
				break;
			case "-m":
				printApplicationArguments = true;
				break;
			case "-q":
				vmidOnly = true;
				break;
			case "-v":
				printJvmArguments = true;
				break;
				/* implicitly handle -h and -help via the default case */
			default: 
				System.out.printf(HELPTEXT);
				System.exit(1);
				break;
			}
		}
	}
}
