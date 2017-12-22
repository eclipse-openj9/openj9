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
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.walkers.ClassSegmentIterator;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.ClassSummaryHelper;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.ClassWalker;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.LinearDumper;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.RamClassWalker;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.LinearDumper.J9ClassRegionNode;

public class RamClassSummaryCommand extends Command
{
	private long numberOfClasses;
	private final String[] preferredOrder = 
	{
			"jitVTables",
			"ramHeader",
			"vTable",
			"Extended method block",
			"RAM methods",
			"Constant Pool",
			"Ram static",
			"Superclasses",
			"InstanceDescription",
			"iTable"
	};
	
	public RamClassSummaryCommand()
	{
		addCommand("ramclasssummary", "", "Display ram class summary");
	}
		
	public void run(String command, String[] args, Context context,	PrintStream out) throws DDRInteractiveCommandException 
	{
		try {
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			ClassSummaryHelper classSummaryHelper = new ClassSummaryHelper(preferredOrder);

			ClassSegmentIterator classSegmentIterator = new ClassSegmentIterator(vm.classMemorySegments());

			while (classSegmentIterator.hasNext()) {
				J9ClassPointer classPointer = (J9ClassPointer) classSegmentIterator.next();
				numberOfClasses++;
				
				ClassWalker classWalker = new RamClassWalker(classPointer, context);
				LinearDumper linearDumper = new LinearDumper();
				J9ClassRegionNode allRegionsNode = linearDumper.getAllRegions(classWalker);

				classSummaryHelper.addRegionsForClass(allRegionsNode);
			}
			classSummaryHelper.printStatistics(out);


		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}	
	}
	public long getNumberOfClasses() {
		return numberOfClasses;
	}



}

