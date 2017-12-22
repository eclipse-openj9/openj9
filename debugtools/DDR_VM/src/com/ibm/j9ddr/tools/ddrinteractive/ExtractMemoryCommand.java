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
package com.ibm.j9ddr.tools.ddrinteractive;

import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintStream;

import com.ibm.j9ddr.corereaders.memory.MemoryFault;

public class ExtractMemoryCommand extends Command 
{
	public ExtractMemoryCommand()
	{
		addCommand("extractmemory", "<hexAddress> <hexLength> <filename>", "Dump the specified memory range to a binary file");
	}
	

	public void run(String command, String arguments[], Context context, PrintStream out) throws DDRInteractiveCommandException 
	{
		if (arguments.length != 3) {
			out.println("Unexpected number of arguments.");
			printUsage(out);
			return;
		}
		
		long address = 0L;
		if (arguments[0].startsWith("0x")) {
			address = Long.parseLong(arguments[0].substring(2 /* 0x */), 16);
		} else {
			address = Long.parseLong(arguments[0], 16);
		}
		
		int length = 0;
		if (arguments[1].startsWith("0x")) {
			length = Integer.parseInt(arguments[1].substring(2 /* 0x */), 16);
		} else {
			length = Integer.parseInt(arguments[1], 16);
		}
		
		String filename = arguments[2];
		FileOutputStream fos;
		try {
			fos = new FileOutputStream(filename);
		} catch (FileNotFoundException e) {
			throw new DDRInteractiveCommandException("Failed to open output file " + filename, e);
		}
		
		try {
			byte[] data = new byte[length];
			context.process.getBytesAt(address, data);
			try {
				fos.write(data, 0, length);
			} catch (IOException e) {
				throw new DDRInteractiveCommandException("Failed to write data to file " + filename, e);
			} finally {
				try {
					fos.close();
				} catch (IOException e) {
					/* Whatcha gonna do? */
				}
			}
		} catch (MemoryFault e) {
			throw new DDRInteractiveCommandException("Unable to read memory range", e);
		}
	}

	private void printUsage(PrintStream out) 
	{
		out.println("Usage:\n  !extractmemory <hexAddress> <hexLength> <filename>");
	}

}
