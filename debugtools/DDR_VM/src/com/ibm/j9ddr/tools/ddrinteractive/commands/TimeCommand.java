/*******************************************************************************
 * Copyright (c) 2014, 2014 IBM Corp. and others
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
import java.text.ParseException;

import com.ibm.j9ddr.command.CommandParser;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;

/**
 * Wrapper command to time the execution of another ddr command.
 * 
 * @author Howard Hellyer
 *
 */
public class TimeCommand extends Command {

	public TimeCommand()
	{
		addCommand("time", "<command>", "run another command and print out elapsed time");
	}

	public void run(String command, String[] args, Context context,
			PrintStream out) throws DDRInteractiveCommandException {
		String[] newArgs = null;
		if( args.length < 1 ) {
			out.println("The time command requires another command to run as an argument.");
			return;
		} else if( args.length > 1 ) {
			newArgs = new String[args.length - 1];
			System.arraycopy(args, 1, newArgs, 0, newArgs.length);
		} else {
			newArgs = new String[0];
		}
		CommandParser commandParser;
		try {
			commandParser = new CommandParser(args[0], newArgs);
			long startTime = System.currentTimeMillis();
			context.execute(commandParser, out);
			long endTime = System.currentTimeMillis();
			double secs = ((double)endTime - (double) startTime)/1000.0;
			out.printf("\n---\nCommand '%s' took %f seconds\n", commandParser.toString(), secs);
		} catch (ParseException e) {
			e.printStackTrace(out);
		}
	}

}
