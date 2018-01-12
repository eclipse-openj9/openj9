/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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
package com.ibm.jvm.dtfjview;

import java.io.PrintStream;

import com.ibm.jvm.dtfjview.commands.helpers.Utils;
import com.ibm.jvm.dtfjview.spi.IOutputChannel;

public class ConsoleOutputChannel implements IOutputChannel {
	private PrintStream out = System.out;		//grab the print stream in case it gets redirected later on
	
	private boolean noPrint = false;

	public void print(String outputString) {
		if (!noPrint)
			out.print(outputString);
	}

	public void printPrompt(String prompt) {
		out.print(prompt);
	}

	public void println(String outputString) {
		if (!noPrint)
			out.println(outputString);
	}

	public void error(String outputString) {
		System.err.print("\n");
		System.err.print("ERROR: " + outputString + "\n");
	}
	
	//logs an error to the specified output channel
	public void error(String msg, Exception e) {
		System.err.println(Utils.toString(msg));
		e.printStackTrace(System.err);
	}

	public void printInput(long timestamp, String prompt, String outputString) {
		// we don't need to output anything, but we could output the time the command was started
		//System.out.println("<started at: " + (new Date(timestamp)).toString() + ">");
	}
	
	public void close() {
		// do nothing, because we don't need to close System.out or System.err
	}
	
	public void setNoPrint(boolean b) {
		noPrint = b;
	}

	public void flush() {
		// do nothing for console output
	}
}
