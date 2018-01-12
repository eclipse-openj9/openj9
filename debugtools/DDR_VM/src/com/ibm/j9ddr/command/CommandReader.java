/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

package com.ibm.j9ddr.command;

import java.io.InputStream;
import java.io.PrintStream;
import java.text.ParseException;

import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractive;

public abstract class CommandReader {

	protected final PrintStream out;

	public CommandReader(PrintStream out) {
		this.out = out;
	}

	/**
	 * Execute next command, and execute it in DDRInteractive
	 * 
	 * @throws Exception
	 */
	public abstract void processInput(DDRInteractive engine) throws Exception;

	/**
	 * Set the input stream on which commands will be read from. This may be
	 * ignored by the underlying implementation if this redirection is not
	 * supported.
	 * 
	 * @param in
	 *            InputStream to read commands from
	 */
	public abstract void setInputStream(InputStream in);

	public void processLine(DDRInteractive engine, String line) throws Exception {
		line = line.trim();

		if (line.toLowerCase().equals("quit")) {
			out.println("Quitting...");

			throw new ExitException();
		}
		if (line.toLowerCase().equals("exit")) {
			out.println("Exiting...");
			
			throw new ExitException();
		}

		CommandParser parser;
		try {
			parser = new CommandParser(line);
		} catch (ParseException e) {
			out.println("Error running command: " + e.getMessage());
			return;
		}


		engine.execute(parser);
	}
}
