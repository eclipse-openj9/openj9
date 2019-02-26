/*******************************************************************************
 * Copyright (c) 2017, 2019 IBM Corp. and others
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
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ITablePointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.types.UDATA;
import com.ibm.j9ddr.vm29.structure.J9ITable;
import static com.ibm.j9ddr.vm29.structure.J9Consts.*;
import static com.ibm.j9ddr.vm29.structure.J9JavaAccessFlags.*;

public class ITableSizeCommand extends Command 
{
	private static final String nl = System.getProperty("line.separator");
	
	public ITableSizeCommand()
	{
		addCommand("itablesize", "", "Dump experimental iTable sizing");
	}		
	
	public long iTableChainSize(J9ITablePointer startTable, J9ITablePointer superTable) throws CorruptDataException
	{
		long size = 0;
		J9ITablePointer iTable = startTable;
		while (!iTable.eq(superTable)) {
			size += J9ITable.SIZEOF;
			J9ClassPointer interfaceClass = iTable.interfaceClass();
			J9ROMClassPointer romClass = interfaceClass.romClass();
			size += (UDATA.SIZEOF * romClass.romMethodCount().intValue());
			iTable = iTable.next();
		}
		return size;
	}
	
	public long iTableExtendedSize(J9ITablePointer startTable, J9ITablePointer superTable) throws CorruptDataException
	{
		long size = 0;
		J9ITablePointer iTable = startTable;
		while (!iTable.eq(superTable)) {
			size += J9ITable.SIZEOF;
			J9ClassPointer interfaceClass = iTable.interfaceClass();
			J9ROMClassPointer romClass = interfaceClass.romClass();
			J9ITablePointer allInterfaces = J9ITablePointer.cast(interfaceClass.iTable());
			do {
				size += (UDATA.SIZEOF * allInterfaces.interfaceClass().romClass().romMethodCount().intValue());
				allInterfaces = allInterfaces.next();
			} while (!allInterfaces.eq(J9ITablePointer.NULL));
			iTable = iTable.next();
		}
		return size;
	}
	
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException 
	{
		long currentSize = 0;
		long duplicatedSize = 0;
		long extendedSize = 0;
		try {
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			ClassSegmentIterator classSegmentIterator = new ClassSegmentIterator(vm.classMemorySegments());
			while (classSegmentIterator.hasNext()) {
				J9ClassPointer clazz = (J9ClassPointer) classSegmentIterator.next();
				int classDepth = clazz.classDepthAndFlags().bitAnd(J9AccClassDepthMask).intValue();
				J9ITablePointer superITable = J9ITablePointer.NULL;
				J9ITablePointer startITable = J9ITablePointer.cast(clazz.iTable());
				if (0 != classDepth) {
					PointerPointer superclasses = clazz.superclasses();
					J9ClassPointer superclazz = J9ClassPointer.cast(superclasses.at(classDepth - 1));
					superITable =  J9ITablePointer.cast(superclazz.iTable());
				}
				currentSize += iTableChainSize(startITable, superITable);
				duplicatedSize += iTableChainSize(superITable, J9ITablePointer.NULL);
				extendedSize += iTableExtendedSize(startITable, superITable);
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}

		long totalSize = duplicatedSize + currentSize;
		double percent = (double)totalSize / (double)currentSize;
		out.append("iTable duplication" + nl);
		out.append("------------------" + nl);
		out.append("current    iTable size : " + currentSize + nl);
		out.append("additional iTable size : " + duplicatedSize + nl);
		out.append("total      iTable size : " + totalSize + nl);
		out.append("growth factor          : " + percent + nl);
		out.append(nl);

		percent = (double)extendedSize / (double)currentSize;
		out.append("iTable contains extends" + nl);
		out.append("-----------------------" + nl);
		out.append("current    iTable size : " + currentSize + nl);
		out.append("new        iTable size : " + extendedSize + nl);
		out.append("growth factor          : " + percent + nl);
		out.append(nl);
	}
}
