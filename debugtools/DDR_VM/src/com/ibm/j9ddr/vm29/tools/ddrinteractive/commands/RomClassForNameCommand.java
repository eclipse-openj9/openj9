/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.util.PatternString;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.walkers.ROMClassesIterator;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9UTF8Helper;

public class RomClassForNameCommand extends Command 
{

	public RomClassForNameCommand()
	{
		addCommand("romclassforname", "<name>", "find all the romclasses corresponding to name (with wildcards)");
	}

	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException 
	{
		if (args.length == 0) {
			printUsage(out);
			return;
		}
		try {
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			ROMClassesIterator iterator = new ROMClassesIterator(out, vm.classMemorySegments());

			int hitCount = 0;
			String searchClassName = args[0];
			PatternString pattern = new PatternString (searchClassName);
			
			out.println(String.format(
					"Searching for ROMClasses named '%1$s' in VM=%2$s",
					searchClassName, Long.toHexString(vm.getAddress())));
			while (iterator.hasNext()) {
				J9ROMClassPointer romClassPointer = iterator.next();
				String javaName = J9UTF8Helper.stringValue(romClassPointer.className());
				if (pattern.isMatch(javaName)) {
					hitCount++;
					
					String hexString = romClassPointer.getHexAddress();
					out.println(String.format("!j9romclass %1$s named %2$s", hexString, javaName));					
				}
			}
			out.println(String.format("Found %1$d ROMClass(es) named %2$s",
					hitCount, searchClassName));
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}

	}
	
	 /**
     * Prints the usage for the romclassforname command.
     *
     * @param out  the PrintStream the usage statement prints to
     */
	private void printUsage (PrintStream out) {
		out.println("romclassforname <name> - find the class corresponding to name (with wildcards)");
	}
}
