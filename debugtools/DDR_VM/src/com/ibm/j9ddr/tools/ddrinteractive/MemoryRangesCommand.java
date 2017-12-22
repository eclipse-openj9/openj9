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

package com.ibm.j9ddr.tools.ddrinteractive;

import java.io.PrintStream;
import java.util.Collection;

import com.ibm.j9ddr.corereaders.memory.IMemoryRange;
import com.ibm.j9ddr.corereaders.memory.IProcess;

public class MemoryRangesCommand extends Command 
{
	public MemoryRangesCommand()
	{
		addCommand("ranges", "", "Prints list of available memory ranges");
	}

	public void run(String command, String[] args, Context context, PrintStream out)	throws DDRInteractiveCommandException 
	{
		IProcess process = context.process;
		
		String format = "0x%0" + (process.bytesPerPointer() * 2) + "x%" + (16 - process.bytesPerPointer()) + "s";
		Collection<? extends IMemoryRange> ranges = process.getMemoryRanges();
		if( ranges != null && !ranges.isEmpty() ) {
			out.println("Base                  Top                   Size");
			for(IMemoryRange range : ranges) {
				out.print(String.format(format, range.getBaseAddress(), " "));
				out.print(String.format(format, range.getTopAddress(), " "));
				out.print(String.format(format, range.getSize(), " "));
				out.print("\n");
			}
		} else {
			out.println("No memory ranges found (Note: this has not yet been implemented for execution from a native debugger such as gdb)");
		}
	}

}
