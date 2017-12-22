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
package com.ibm.j9ddr.vm29.tools.ddrinteractive.commands;

import java.io.PrintStream;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.exceptions.UnknownArchitectureException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.VMRegMapHelper;

public class J9RegCommand extends Command 
{
	public J9RegCommand()
	{
		addCommand("j9reg", "<level>", "list VM named registers");
	}

	 /**
     * Prints the usage for the j9reg command.
     *
     * @param out PrintStream to print the output to the console.
     */
	private void printUsage (PrintStream out) {
		out.println("j9reg <level> - list VM named registers");
	}

	/**
	 * Run method for !j9reg extension.
	 * 
	 * @param command  !j9reg
	 * @param args	args passed by !j9reg extension.
	 * @param context Context of current core file.
	 * @param out PrintStream to print the output to the console.
	 * @throws DDRInteractiveCommandException
	 */	
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException 
	{
		int level = 0;
		if (0 == args.length) {
			level = 1;
		} else if (1 == args.length) {
			try {
				level = Integer.parseInt(args[0]);
			} catch (NumberFormatException e) {
				out.println(args[0] + " is not a valid integer. Please try again.");
				return;
			}
		} else {
			printUsage(out);
		}
		
		J9JavaVMPointer vm;
		
			try {
				vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			
				VMRegMapHelper.printRegisters(vm, level, out);
			} catch (CorruptDataException e) {
				throw new DDRInteractiveCommandException("Failed to get vm address from RAS");
			} catch (UnknownArchitectureException uae) {
				throw new DDRInteractiveCommandException(uae.getMessage(), uae);
			}
	}

}

