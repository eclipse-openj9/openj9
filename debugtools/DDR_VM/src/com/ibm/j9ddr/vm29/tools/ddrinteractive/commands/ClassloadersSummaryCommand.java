/*******************************************************************************
 * Copyright (c) 2001, 2015 IBM Corp. and others
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
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.TreeMap;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.gc.GCClassLoaderIterator;
import com.ibm.j9ddr.vm29.j9.walkers.ClassSegmentIterator;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassLoaderPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MemorySegmentPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.structure.J9MemorySegment;
import com.ibm.j9ddr.vm29.types.UDATA;

public class ClassloadersSummaryCommand extends Command 
{

	public ClassloadersSummaryCommand()
	{
		addCommand("classloaderssummary", "[segs]", "Display classloaders summary, optionally including the RAM and ROM segment breakdown");
	}

	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException 
	{
		try {
			Collection<ClassloadersSummaryNode> stats = getStat();
			printStat(stats, out, args.length > 0 && args[0].equalsIgnoreCase("segs"));
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}

	public void printStat(Collection<ClassloadersSummaryNode> stats, PrintStream out, boolean printSegments) throws CorruptDataException 
	{
		int longestName = 0;
		int numClassloaders = 0;
		long numROMSegment = 0;
		long numRAMSegment = 0;
		long totalROMSegmentAllocatedSize = 0;
		long totalRAMSegmentAllocatedSize = 0;
		for (ClassloadersSummaryNode csc : stats) {
			numClassloaders += csc.numClassloaders;
			longestName = Math.max(longestName, csc.name.length());
		}
				
		out.println();
		out.println("[Classloaders Summary]");
		CommandUtils.dbgPrint(out, "<num, %-" + longestName + "s, # Classloaders, # Loaded Classes", "Classloader name");
		if (printSegments) {
			CommandUtils.dbgPrint(out, ", # RAM Segments, RAM Segment memory, # ROM Segments, ROM Segment memory, Total Segments, Segments memory>" + nl);
		} else {
			CommandUtils.dbgPrint(out, ", Total Segments, Segments memory>" + nl);			
		}
		TreeMap<Long, Long> countmap = new TreeMap<Long, Long>(Collections.reverseOrder());
		HashMap<Long, String> cllist = new LinkedHashMap<Long, String>();
		int count = 0;
		for (ClassloadersSummaryNode csc : stats) {
			long totalSegmentCount = csc.romSegmentCounter + csc.ramSegmentCounter;
			long totalAllocatedMemoryForclassloader = csc.totalROMSegmentAllocatedMemory + csc.totalRAMSegmentAllocatedMemory;
			CommandUtils.dbgPrint(out, "%3d)  %-" + longestName + "s, %-14d, %-16d", count, csc.name, csc.numClassloaders, csc.numLoadedClasses);
			if (printSegments) {
				CommandUtils.dbgPrint(out, ", %-14d, %-18d, %-14d, %-18d, %-14d, %d" + nl, csc.ramSegmentCounter, csc.totalRAMSegmentAllocatedMemory,
						csc.romSegmentCounter, csc.totalROMSegmentAllocatedMemory, totalSegmentCount, totalAllocatedMemoryForclassloader);
			} else {
				CommandUtils.dbgPrint(out, ", %-14d, %d" + nl, totalSegmentCount, totalAllocatedMemoryForclassloader);
			}
			numROMSegment += csc.romSegmentCounter;
			numRAMSegment += csc.ramSegmentCounter;
			totalROMSegmentAllocatedSize += csc.totalROMSegmentAllocatedMemory;
			totalRAMSegmentAllocatedSize += csc.totalRAMSegmentAllocatedMemory;
			Long cpval = countmap.get(csc.numLoadedClasses);
			if (cpval != null){
				Long clcount = new Long (cpval + csc.numClassloaders);
				countmap.put(csc.numLoadedClasses, clcount);
				String list = cllist.get(csc.numLoadedClasses);
				cllist.put(csc.numLoadedClasses, list + ", " + csc.name);
			} else {
				countmap.put(csc.numLoadedClasses, csc.numClassloaders);
				cllist.put(csc.numLoadedClasses, csc.name);
			}
			count += 1;
		}

		if (!countmap.isEmpty()) {
			out.println("\n<# Loaded Classes, # Classloaders, Classloader Names>");
			for(Map.Entry<Long, Long> entry : countmap.entrySet()) {
				CommandUtils.dbgPrint(out, "%-17d, %-14d, %s" + nl, entry.getKey(), entry.getValue(), cllist.get(entry.getKey()));
			}
		}

		out.println();
		out.println("Number of Classloaders: " + numClassloaders);
		out.println();
		
		out.println("Segment Totals: ");
		out.println("\n<# RAM Segments, RAM Segments memory, # ROM Segments, ROM Segments memory, Total Segments, Segments memory>");
		CommandUtils.dbgPrint(out, " %-14d, %-19d, %-14d, %-19d, %-14d, %d", numRAMSegment, totalRAMSegmentAllocatedSize, numROMSegment, totalROMSegmentAllocatedSize, 
				numRAMSegment + numROMSegment, totalRAMSegmentAllocatedSize + totalROMSegmentAllocatedSize);
		out.println();
	}
	
	static final class Counter {
		long count;
		Counter(long initialValue) {
			count = initialValue;
		}
		Counter addOne() {
			count += 1;
			return this;
		}
		long getCount() {
			return count;
		}
	}
	
	public Collection<ClassloadersSummaryNode> getStat() throws CorruptDataException 
	{
		Map<J9ClassLoaderPointer, Counter> classloadersCount = new HashMap<J9ClassLoaderPointer, Counter>();
		J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
		GCClassLoaderIterator iterator = GCClassLoaderIterator.from();
		// The longestName variable is used to format the output
		
		while (iterator.hasNext()) {
			J9ClassLoaderPointer classLoader = iterator.next();
			classloadersCount.put(classLoader, new Counter(0));
		}

		/* Iterate through all classes and count how many were loaded by each classLoader */
		ClassSegmentIterator classSegmentIterator = new ClassSegmentIterator(vm.classMemorySegments());
		while (classSegmentIterator.hasNext()) {
			J9ClassPointer classPointer = (J9ClassPointer) classSegmentIterator.next();
			Counter counter = classloadersCount.get(classPointer.classLoader());
			if (counter != null) {
				counter.addOne();
			} else {
				classloadersCount.put(classPointer.classLoader(), new Counter(1));
			}
		}

		J9ObjectPointer sys = vm.systemClassLoader().classLoaderObject();
		final UDATA valueROM = new UDATA(J9MemorySegment.MEMORY_TYPE_ROM_CLASS);
		final UDATA valueRAM = new UDATA(J9MemorySegment.MEMORY_TYPE_RAM_CLASS);
		Map<String, ClassloadersSummaryNode> classloaders = new LinkedHashMap<String, ClassloadersSummaryNode>();
		for (Map.Entry<J9ClassLoaderPointer, Counter> entry : classloadersCount.entrySet()) {
			J9ObjectPointer classLoaderObject = entry.getKey().classLoaderObject();
			boolean isSystem = classLoaderObject.equals(sys);
			String loader = isSystem ? "*System*" : J9ObjectHelper.getClassName(classLoaderObject);
			
			/*	For each classloader, iterate through each associated memory segment and identify whether it is  
			 * 	a ROM or a RAM type memory segment  */
			long romSegmentCount = 0;
			long ramSegmentCount = 0;
			long romSegmentAllocatedMem = 0;
			long ramSegmentAllocatedMem = 0;
			J9MemorySegmentPointer segment = entry.getKey().classSegments();
			while (segment.notNull()) {
				if ((segment.type().bitAnd(valueROM)).equals(valueROM)) {
					romSegmentCount += 1;
					romSegmentAllocatedMem += segment.size().longValue();							
				} else if ((segment.type().bitAnd(valueRAM)).equals(valueRAM)) {
					ramSegmentCount += 1;
					ramSegmentAllocatedMem += segment.size().longValue();
				}
				segment = segment.nextSegmentInClassLoader();
			}
			
			ClassloadersSummaryNode cpentry = classloaders.get(loader);
			if (cpentry == null) {
				// If the classLoader is not in the list, add it with "# Classloaders = 1" and "# Loaded Classes = classesLoaded"
				classloaders.put(loader, new ClassloadersSummaryNode(loader, 1L, entry.getValue().getCount(), ramSegmentCount, romSegmentCount, romSegmentAllocatedMem, ramSegmentAllocatedMem));
			} else {
				// If the classLoader is in the list, increment "# Classloaders" by 1 and increment "# Loaded Classes" by classesLoaded
				cpentry.numClassloaders += 1;
				cpentry.numLoadedClasses += entry.getValue().getCount();
				cpentry.ramSegmentCounter += ramSegmentCount;
				cpentry.romSegmentCounter += romSegmentCount;
				cpentry.totalROMSegmentAllocatedMemory += romSegmentAllocatedMem;
				cpentry.totalRAMSegmentAllocatedMemory += ramSegmentAllocatedMem;
			}
		}
		
		return classloaders.values();
	}
	
	public class ClassloadersSummaryNode 
	{
		public ClassloadersSummaryNode(String name, Long numClassloaders, long numLoadedClasses, long ramSegmentCounter, long romSegmentCounter, 
				long totalROMSegmentAllocatedMemory, long totalRAMSegmentAllocatedMemory) {
			this.name = name;
			this.numClassloaders = numClassloaders;
			this.numLoadedClasses = numLoadedClasses;
			this.romSegmentCounter = romSegmentCounter; 
			this.ramSegmentCounter = ramSegmentCounter; 
			this.totalROMSegmentAllocatedMemory = totalROMSegmentAllocatedMemory;
			this.totalRAMSegmentAllocatedMemory = totalRAMSegmentAllocatedMemory;
		}
		public String name;
		public Long numClassloaders;
		public long numLoadedClasses;
		public long romSegmentCounter;
		public long ramSegmentCounter;
		public long totalROMSegmentAllocatedMemory;
		public long totalRAMSegmentAllocatedMemory;
		
	}
}
