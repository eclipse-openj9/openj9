/*******************************************************************************
 * Copyright (c) 2010, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.tools.ddrinteractive.commands;

import java.io.PrintStream;

import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.vm29.j9.ObjectAccessBarrier;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;

/**
 * Handles converting !fj9object into !j9object
 */
public class CompressedRefMappingCommand extends Command 
{

	public CompressedRefMappingCommand()
	{
		addCommand("fj9object", "<address>", "Display the j9object given a compressed refs address.");
		addCommand("fj9objecttoj9object", "<address>", "Convert the compressed refs address to a j9object address.");
		addCommand("j9objecttofj9object", "<address>", "Convert the j9object address to a compressed refs address.");
	}
	
	private void printHelp(PrintStream out) {
		out.append("Usage: \n");
		out.append("  !fj9object <address>\n");
		out.append("  !fj9objecttoj9object <address>\n");
		out.append("  !j9objecttofj9object <address>\n");
	}
	
	public void run(String command, String[] args, Context context,	PrintStream out) throws DDRInteractiveCommandException 
	{
		if (args.length == 0) {
			printHelp(out);
			return;
		}
		
		long address = CommandUtils.parsePointer(args[0], J9BuildFlags.env_data64);

		VoidPointer ptr = VoidPointer.cast(address);
		
		
		if (command.startsWith("!fj9object")) {
			J9ObjectPointer mappedValue;
			if (J9BuildFlags.gc_compressedPointers) {
				mappedValue = ObjectAccessBarrier.convertPointerFromToken(ptr.getAddress());
			} else {
				mappedValue = J9ObjectPointer.cast(ptr);
			}
		
			if (command.startsWith("!fj9objecttoj9object")) {
				out.println(String.format("!fj9object %s -> !j9object %s", ptr.getHexAddress(), mappedValue.getHexAddress()));
			} else {
				context.execute("!j9object", new String[]{mappedValue.getHexAddress()}, out);
			}
		} else {
			long tokenValue;
			if (J9BuildFlags.gc_compressedPointers) {
				tokenValue = ObjectAccessBarrier.convertTokenFromPointer(J9ObjectPointer.cast(ptr));
			} else {
				tokenValue = ptr.getAddress();
			}
			out.println(String.format("!j9object %s -> !fj9object 0x%s\n", ptr.getHexAddress(), Long.toHexString(tokenValue)));
		}
	}

}
