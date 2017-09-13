/*******************************************************************************
 * Copyright (c) 2014, 2015 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
package com.ibm.j9ddr.tools.ddrinteractive.commands;

import java.io.PrintStream;
import java.text.ParseException;

import com.ibm.j9ddr.command.CommandParser;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;

public class LimitCommand extends Command {

	public LimitCommand()
	{
		addCommand("limit", "<timeout> <command> [<args>]", "run another command; terminate if it exceeds the timeout");
	}

	@SuppressWarnings("deprecation")
	public void run(String command, final String[] args, final Context context, final PrintStream out) throws DDRInteractiveCommandException {
		final String[] newArgs;
		if (args.length < 2) {
			out.println("The limit command requires a timeout and another command to run as an argument.");
			return;
		}
		
		long timeout = Integer.parseInt(args[0]) * 1000;
		
		if (args.length > 2) {
			newArgs = new String[args.length - 2];
			System.arraycopy(args, 2, newArgs, 0, newArgs.length);
		} else {
			newArgs = new String[0];
		}
		
		Thread runner = new Thread("LimitCommand: " + args[1]) 
		{
			public void run() 
			{
				try {
					CommandParser commandParser = new CommandParser(args[1], newArgs);
					context.execute(commandParser, out);
				} catch (ParseException e) {
					e.printStackTrace(out);
				}
			}
		};
		runner.start();
		try {
			runner.join(timeout);
		} catch (InterruptedException e) {
			e.printStackTrace();
		}
		if (runner.isAlive()) {
			runner.stop(new DDRInteractiveCommandException("Timeout exceeded!"));
		}
		
	}

}
