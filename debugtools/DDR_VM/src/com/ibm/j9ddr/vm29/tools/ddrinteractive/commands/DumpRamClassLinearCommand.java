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
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9UTF8Helper;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.ClassWalker;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.LinearDumper;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.RamClassWalker;

public class DumpRamClassLinearCommand extends Command 
{
	public DumpRamClassLinearCommand()
	{
		addCommand("dumpramclasslinear", "<addr>[,n]", "cfdump the specified J9RAMClass using Linear RAM Class Dumper");
	}

	public void run(String command, String[] args, Context context,
			PrintStream out) throws DDRInteractiveCommandException {
		
		if (args.length == 0)
			throw new DDRInteractiveCommandException("This debug extension needs an address argument !dumpramclasslinear <addr>[,n]");
		
		String[] arguments = args[0].split(",");
		
		long addr = Long.decode(arguments[0]);
		long nestingThreshold;
		if (arguments.length > 1) {
			nestingThreshold = Long.decode(arguments[1]);
		} else {
			nestingThreshold = 1;
		}
		J9ClassPointer clazz = J9ClassPointer.cast(addr);
		try {
			out.println(String.format("RAM Class '%s' at %s", J9UTF8Helper.stringValue(clazz.romClass().className()), clazz.getHexAddress()));
			ClassWalker classWalker = new RamClassWalker(clazz, context);
			new LinearDumper().gatherLayoutInfo(out, classWalker, nestingThreshold);
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}

}
