/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2019 IBM Corp. and others
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

import com.ibm.java.diagnostics.utils.IContext;
import com.ibm.java.diagnostics.utils.commands.CommandException;
import com.ibm.java.diagnostics.utils.plugins.DTFJPlugin;
import com.ibm.jvm.dtfjview.commands.helpers.Utils;

@DTFJPlugin(version="1.*", runtime=false)
public class ScrollCommand extends BaseJdmpviewCommand{
	{
		addCommand("+", "", "displays the next section of memory in hexdump-like format");	
		addCommand("-", "", "displays the previous section of memory in hexdump-like format");
	}
	
	public void run(String command, String[] args, IContext context, PrintStream out) throws CommandException {
		if(initCommand(command, args, context, out)) {
			return;		//processing already handled by super class
		}
		Long currentMemAddress = (Long)ctx.getProperties().get(Utils.CURRENT_MEM_ADDRESS);
		Integer currentNumBytesToPrint = (Integer)ctx.getProperties().get(Utils.CURRENT_NUM_BYTES_TO_PRINT);
		if (null == currentMemAddress || null == currentNumBytesToPrint){
			out.println("ERROR - starting mem address or the number of bytes to print needs to be set " +
					"by running hexdump command");
			return;
		}
		
		long newMemAddress = 0;
		if(command.equals("+")) {
			newMemAddress = currentMemAddress.longValue() + currentNumBytesToPrint.intValue();
		}
		if(command.equals("-")) {
			newMemAddress = currentMemAddress.longValue() - currentNumBytesToPrint.intValue();
		}
		ctx.getProperties().put(Utils.CURRENT_MEM_ADDRESS, Long.valueOf(newMemAddress));
		ctx.execute("hexdump", new String[]{Long.toHexString(newMemAddress),currentNumBytesToPrint.toString()}, out);
	}

	@Override
	public void printDetailedHelp(PrintStream out) {
		out.println("displays the next section of memory in hexdump-like format\n\n" + 
				"parameters: none\n\n" +
				"The + command is used in conjunction with the hexdump command \n" +
				"to allow easy scrolling forwards through memory. It repeats the \n" +
				"previous hexdump command starting from the end of the previous one.\n" +
				"The - command is used in conjunction with the hexdump command \n" +
				"to allow easy scrolling backwards through memory. It repeats \n" +
				"the previous hexdump command starting from a position \n" +
				"before the previous one."
				);
	}
	
}
