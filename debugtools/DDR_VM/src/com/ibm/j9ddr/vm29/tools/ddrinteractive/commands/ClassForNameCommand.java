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
import com.ibm.j9ddr.vm29.j9.walkers.ClassSegmentIterator;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;

public class ClassForNameCommand extends Command 
{

	public ClassForNameCommand()
	{
		addCommand("classforname", "<name>", "find the class corresponding to name (with wildcards)");
	}

	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException 
	{
		if (args.length == 0) {
			printUsage(out);
			return;
		}
		try {
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			ClassSegmentIterator iterator = new ClassSegmentIterator(vm.classMemorySegments());

			int hitCount = 0;
			String searchClassName = args[0];
			PatternString pattern = new PatternString (searchClassName);
			
			out.println(String.format(
					"Searching for classes named '%1$s' in VM=%2$s",
					searchClassName, Long.toHexString(vm.getAddress())));
			while (iterator.hasNext()) {
				J9ClassPointer classPointer = (J9ClassPointer) iterator.next();
				String javaName = J9ClassHelper.getJavaName(classPointer);
				if (pattern.isMatch(javaName)) {
					hitCount++;
					
					String hexString = classPointer.getHexAddress();
					out.println(String.format("!j9class %1$s named %2$s", hexString, javaName));					
				}
			}
			out.println(String.format("Found %1$d class(es) named %2$s",
					hitCount, searchClassName));
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}

	}
	
	 /**
     * Prints the usage for the classforname command.
     *
     * @param out  the PrintStream the usage statement prints to
     */
	private void printUsage (PrintStream out) {
		out.println("classforname <name> - find the class corresponding to name (with wildcards)");
	}
}
