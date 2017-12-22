/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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

import java.io.BufferedReader;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.PrintStream;

import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractive;

public class ConsoleCommandReader extends CommandReader {
	private BufferedReader reader;
	
	public ConsoleCommandReader(PrintStream out) {
		super(out);
		//default to stdin for input
		setInputStream(System.in);
	}

	@Override
	public void setInputStream(InputStream in) {
		if(reader != null) {
			//close the current input stream
			try {
				reader.close();
			} catch (Exception e) {
				//ignore exceptions
			}
		}
		reader = new BufferedReader(new InputStreamReader(in));
	}


	@Override
	public void processInput(DDRInteractive engine) throws Exception {		
		while (true) {
			String line;

			prompt();
			while ((line = reader.readLine()) != null) {
				if (line.length() == 0) {
					continue;
				}

				try {
					processLine(engine, line);
					prompt();
				} catch (ExitException e) {
					return;
				}
			}
		}
	}

	private void prompt() {
		out.print("> ");
	}
}
