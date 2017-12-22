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
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.walkers.ROMClassesIterator;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9UTF8Helper;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.ClassWalker;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.LinearDumper;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.RomClassWalker;

public class DumpAllRomClassLinearCommand extends Command 
{

	public DumpAllRomClassLinearCommand()
	{
		addCommand("dumpallromclasslinear", "[nestingThreshold]", "cfdump all J9ROMClass using the Linear ROM Class Dumper");
	}
		
	public void run(String command, String[] args, Context context,	PrintStream out) throws DDRInteractiveCommandException 
	{
		long nestingThreashold;
		if (args.length > 1) {
			throw new DDRInteractiveCommandException("This debug extension accepts none or one argument!");
		} else if (args.length == 1) {
			nestingThreashold = Long.valueOf(args[0]);
		} else {
			nestingThreashold = 1;
		}
		
		try {
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			if (null != vm) {
				out.println();
				out.println("!j9javavm " +  vm.getHexAddress());
			} else {
				throw new DDRInteractiveCommandException("Unable to find the VM in core dump!");
			}
			out.println();
			
			ROMClassesIterator iterator = new ROMClassesIterator(out, vm.classMemorySegments());
			while (iterator.hasNext()) {
				J9ROMClassPointer classPointer = iterator.next();
				out.println("!dumpromclasslinear " +  classPointer.getHexAddress());
				//ROM Class 'org/apache/tomcat/util/buf/MessageBytes' at 0x0DCF9008:
				out.println(String.format("ROM Class '%s' at %s", J9UTF8Helper.stringValue(classPointer.className()), classPointer.getHexAddress()));
				out.println();
				ClassWalker classWalker = new RomClassWalker(classPointer, context);
				new LinearDumper().gatherLayoutInfo(out, classWalker, nestingThreashold);
				out.println();
			}
			
		} catch (CorruptDataException e1) {
			// TODO Auto-generated catch block
			e1.printStackTrace();
		}	
	}
}
