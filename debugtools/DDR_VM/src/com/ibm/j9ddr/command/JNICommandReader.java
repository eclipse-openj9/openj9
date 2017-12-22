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

import java.io.InputStream;
import java.io.PrintStream;
import java.util.concurrent.BlockingQueue;
import java.util.concurrent.Semaphore;
import java.util.concurrent.SynchronousQueue;

import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractive;


/**
 * Read DDR commands from attached interactive debugger.
 */
public class JNICommandReader extends CommandReader {

	private final static BlockingQueue<String> commandQueue = new SynchronousQueue<String>(true);
	public static String newline = System.getProperty("line.separator");
	private final static Semaphore lock = new Semaphore(0, true);
	
	public JNICommandReader(PrintStream out) {
		super(out);
	}

	public static void enqueueCommand(final String command, final String args) {
		/*
		 * When user issues !ddr command args in debugger
		 * function dbgext_ddr is called. It will will enqueue command args in command argument.
		 * 
		 */		
		try {
			commandQueue.put("!" + command + " " + args + newline);
			// wait for command completion
			// i.e (do not display 'new command' prompt)
			lock.acquire();
		} catch (InterruptedException e) {
			e.printStackTrace();
		}
	}

	private String getCommand() {
		try {
			return commandQueue.take();
		} catch (InterruptedException e) {
			e.printStackTrace();
			return "";
		}
	}

	@Override
	public void processInput(DDRInteractive engine) throws Exception {
		while (true) {
			String line = getCommand();

			if (line.length() == 0) {
				continue;
			}

			try {
				processLine(engine, line);
			} catch (ExitException e) {
				return;
			} finally {
				// command completed
				// release debugger prompt				
				lock.release();
			}
		}
	}

	@Override
	public void setInputStream(InputStream in) {
		// ignore as this is not applicable for this type of reader
		
	}
	
	
}
