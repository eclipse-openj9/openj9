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
import java.text.DecimalFormat;
import java.util.*;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.vm29.j9.walkers.MemoryCategoryIterator;
import com.ibm.j9ddr.vm29.pointer.generated.OMRMemCategoryPointer;
import com.ibm.j9ddr.vm29.pointer.helper.OMRMemCategoryHelper;
import com.ibm.j9ddr.vm29.types.UDATA;
import com.ibm.j9ddr.vm29.view.dtfj.DTFJContext;

public class NativeMemInfoCommand extends Command 
{
	private PrintStream out;
	private DecimalFormat myFormatter = new DecimalFormat("#,###");

	public NativeMemInfoCommand() {
		addCommand("nativememinfo", "", "Dump the native memory info");
	}
	
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException {
		this.out = out;
		
		try {
			Iterator<? extends OMRMemCategoryPointer> categories = MemoryCategoryIterator.iterateCategoryRootSet(DTFJContext.getVm().portLibrary());

			while (categories.hasNext()) {
				OMRMemCategoryPointer next = categories.next();
				printSections(next, 0);
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}
	
	/**
	 * 
	 * @param next
	 * @param level
	 * @return size in bytes
	 * @throws CorruptDataException
	 */
	void printSections(OMRMemCategoryPointer next, int level) throws CorruptDataException {
		ComponentSizeAllocation csa = computeSize(next);
		if (csa.size == 0) {
			return;
		}
		printLine(level, next.name().getCStringAtOffset(0), csa.size, csa.allocations);

		final int numberOfChildren = next.numberOfChildren().intValue();
		for (int i = 0; i < numberOfChildren; i++) {
			UDATA childCode = next.children().at(i);
			OMRMemCategoryPointer child = OMRMemCategoryHelper.getMemoryCategory(childCode);
			printSections(child, level + 1);
		}
		
		final long liveBytes = next.liveBytes().longValue();
		if (liveBytes < csa.size && liveBytes != 0) {
			printLine(level + 1, "Other", liveBytes, next.liveAllocations().longValue());
		}
	}

	private void printLine(int level, String name, long liveBytes, long liveAllocations) {
		for (int i=0; i<level; i++) out.print("|  ");
		out.println();
		for (int i=0; i<level; i++) {
			out.print((i==level-1) ? "+--" : "|  ");
		}
		CommandUtils.dbgPrint(out, "%s: %s bytes / %d allocation%s\n", name, myFormatter.format(liveBytes), liveAllocations, liveAllocations > 1?"s":"");
	}
	
	private ComponentSizeAllocation computeSize(OMRMemCategoryPointer mcp) throws CorruptDataException {
		ComponentSizeAllocation csa = new ComponentSizeAllocation();
		
		csa.size += mcp.liveBytes().longValue();
		csa.allocations += mcp.liveAllocations().longValue();
		
		final int numberOfChildren = mcp.numberOfChildren().intValue();
		for (int i = 0; i < numberOfChildren; i++) {
			UDATA childCode = mcp.children().at(i);
			OMRMemCategoryPointer child = OMRMemCategoryHelper.getMemoryCategory(childCode);
			csa.add(computeSize(child));
		}
		
		return csa;
	}
	
	private class ComponentSizeAllocation {
		protected long size;
		protected long allocations;
		public void add(ComponentSizeAllocation computeSize) {
			size += computeSize.size;
			allocations += computeSize.allocations;
		}
	}
}
