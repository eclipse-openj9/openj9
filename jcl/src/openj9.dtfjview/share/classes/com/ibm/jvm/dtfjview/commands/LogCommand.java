/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2011, 2019 IBM Corp. and others
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
package com.ibm.jvm.dtfjview.commands;

import java.io.PrintStream;
import java.util.Enumeration;
import java.util.TreeSet;
import java.util.logging.ConsoleHandler;
import java.util.logging.Handler;
import java.util.logging.Level;
import java.util.logging.LogManager;
import java.util.logging.Logger;

import com.ibm.java.diagnostics.utils.IContext;
import com.ibm.java.diagnostics.utils.commands.CommandException;
import com.ibm.java.diagnostics.utils.plugins.DTFJPlugin;

/**
 * Command to display all the currently installed Java util loggers.
 * Allows you to selectively enable or disable them as well as set their logging levels.
 *
 * @author adam
 */
@DTFJPlugin(version=".*", runtime=false, image=false)
public class LogCommand extends BaseJdmpviewCommand {

	{
		addCommand("log", "[name level]", "display and control instances of java.util.logging.Logger");
	}

	public void run(String command, String[] args, IContext context, PrintStream out) throws CommandException {
		if(initCommand(command, args, context, out)) {
			return;		//processing already handled by super class
		}
		switch(args.length) {
			case 0 :		//display all the available loggers
				displayLoggers();
				break;
			case 2:			//configure a logger
				configureLogger(args[0], args[1]);
				break;
			default:
				out.println("Incorrect number of parameters supplied. See 'help log' for more details");
				break;
		}
	}

	private void displayLoggers() {
		LogManager lm = LogManager.getLogManager();
		TreeSet<String> names = new TreeSet<String>();
		for(Enumeration<String> e = lm.getLoggerNames(); e.hasMoreElements(); ) {
			Logger logger = lm.getLogger(e.nextElement());
			StringBuilder output = new StringBuilder();
			if(logger.getName().length() == 0) {
				output.append("<<default logger>>");
			} else {
				output.append(logger.getName());
			}

			if(logger.getHandlers().length == 0) {
				output.append(" : disabled");
			} else {
				output.append(" : enabled");
				output.append(" (");
				Level level = logger.getLevel();
				if (level != null) {
					String name = level.getName();
					output.append(name);
				}
				output.append(") ");
			}
			names.add(output.toString());
		}
		out.println("Currently installed loggers :-");
		for(String name : names) {
			out.print("\t");
			out.println(name);
		}
	}

	private void configureLogger(String name, String level) {
		LogManager lm = LogManager.getLogManager();
		Logger logger = lm.getLogger(name);
		if (logger == null) {
			out.println("The logger name " + name + " was not recognised");
			return;
		}
		Level lvl = null;
		try {
			lvl = Level.parse(level);
		} catch (IllegalArgumentException e) {
			out.println("The level " + level + " was not recognised");
			return;
		}
		if(logger.getHandlers().length != 0) {	//need to check for an existing console handler
			for(Handler handler : logger.getHandlers()) {
				if(handler instanceof ConsoleHandler) {
					logger.setLevel(lvl);
					handler.setLevel(lvl);
					return;		//reconfigure the existing handler
				}
			}
		}
		//if get this far then need to install a new console handler as one was not previously detected
		ConsoleHandler handler = new ConsoleHandler();
		logger.setLevel(lvl);
		handler.setLevel(lvl);
		logger.addHandler(handler);
	}

	@Override
	public void printDetailedHelp(PrintStream out) {
		out.println("Command 'log' displays all the currently installed Java util loggers. " +
				"\n'log <logname> <log level> allows you to selectively enable a logger and set its level." +
				"\nValid log levels are SEVERE, WARNING, INFO, CONFIG, FINE, FINER, FINEST" +
				"\nUse 'log <logname> OFF' to disable a logger");
	}

}
