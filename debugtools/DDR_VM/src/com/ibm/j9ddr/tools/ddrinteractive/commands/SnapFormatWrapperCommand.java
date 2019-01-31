/*******************************************************************************
 * Copyright (c) 2013, 2019 IBM Corp. and others
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
package com.ibm.j9ddr.tools.ddrinteractive.commands;

import java.io.PrintStream;

import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;

/* Wrapper class for SnapFormatCommand to prevent class not found exceptions if
 * the trace code isn't available.
 */
public class SnapFormatWrapperCommand extends Command {

	public SnapFormatWrapperCommand()
	{
		/* Check we can find trace API and dat files in here before adding the command. */
		addCommand("snapformat", "[<filename>]", "format trace buffers to a specified file or stdout");
		addCommand("snapformat", "-help", "print detailed help");
		addCommand("snapformat", "[-f <filename>] [-d <datfile_directory>] [-t <j9vmthread id>] [-s <filter>]", "format trace buffers for all threads or just the specified thread to a file or stdout using the specified .dat files");
	}

	public void run(String command, String[] args, Context context,
			PrintStream out) throws DDRInteractiveCommandException {
		if ((args.length > 0) && (args[0].equals("-help") || args[0].equals("-h") || args[0].equals("-?"))) {
			out.println("Format selected trace buffers.");
			out.println();
			out.println("   -f <filename>            Write output to the specified file (default is stdout)");
			out.println();
			out.println("   -d <datfile_directory>   Set the directory to search for the trace format database");
			out.println();
			out.println("   -t <thread_id>           Only output trace information for the specified J9VMThread");
			out.println();
			out.println("   -s <filter>              Only output tracepoints that match the filter expression.");
			out.println("                            A filter expression consists of one or more tracepoint IDs");
			out.println("                            combined with AND, OR, and NOT operators (&, |, !).");
			out.println("      Examples:");
			out.println("         -s j9vm                            Print all tracepoints in component 'j9vm'");
			out.println("         -s j9vm|j9mm                       Print all tracepoints in components 'j9vm' and 'j9mm'");
			out.println("         -s {entry}                         Only print 'Entry' tracepoints");
			out.println("         -s j9vm.0-99                       Only print 'j9vm' tracepoints with IDs below 100");
			out.println("         -s j9scar&!(j9scar.59|j9scar.60)   Print j9scar tracepoints except for 59 and 60");
			return;
		}
		try {
			Class<?> snapFormatCommandCls = Class.forName("com.ibm.j9ddr.tools.ddrinteractive.commands.SnapFormatCommand");
			Object snapFormatCommandInstance = snapFormatCommandCls.newInstance();
			if (snapFormatCommandInstance instanceof Command) {
				Command c = (Command) snapFormatCommandInstance;
				c.run(command, args, context, out);
			} else {
				throw new DDRInteractiveCommandException("Unable to format trace. Could not create formatter.");
			}
		} catch (ClassNotFoundException
				| IllegalAccessException
				| IllegalArgumentException
				| InstantiationException
				| SecurityException e) {
			throw new DDRInteractiveCommandException("Unable to format trace. " + e.getMessage(), e);
		}
	}

}
