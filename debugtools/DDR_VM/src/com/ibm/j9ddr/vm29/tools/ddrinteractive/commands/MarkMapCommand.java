/*******************************************************************************
 * Copyright (c) 2015, 2015 IBM Corp. and others
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
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.vm29.j9.gc.GCExtensions;
import com.ibm.j9ddr.vm29.j9.gc.GCHeapMap;
import com.ibm.j9ddr.vm29.j9.gc.GCHeapMap.MarkedObject;
import com.ibm.j9ddr.vm29.pointer.UDATAPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_GCExtensionsPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_HeapMapPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_IncrementalGenerationalGCPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_MarkMapPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_ParallelGlobalGCPointer;
import com.ibm.j9ddr.vm29.types.IDATA;
import com.ibm.j9ddr.vm29.types.UDATA;


public class MarkMapCommand extends Command
{
	protected static GCHeapMap markMap;
	static {
		try {
			markMap = GCHeapMap.from();
		} catch (CorruptDataException e) {
			markMap = null;
		}
	}

	public MarkMapCommand() 
	{
		addCommand("markmap", "<command> <object>", "query the markmap");
	}

	private void printHelp(PrintStream out)
	{
		out.println("Usage: !markmap <command> <object>");
		out.println("Supported commands:");
		out.println("   help                    Print the help text"); 
		out.println("   ismarked <object>       Query the markmap to see if <object> is marked");
		out.println("   near <object>           Print out marked objects near <object>");
		out.println("   scanRange <start> <end> Print out marked objects in the specified range");
		out.println("   bits <object>           Show the address of the mark slot corresponding to <object>");
		out.println("   fromBits <addr>         Show the heap range covered by a mark slot");
		out.println("   source <object>         Try to find out where <object> was relocated from");
		out.println("   show                    Display the current markmap in use");
		out.println("   set <markMap>           Change the current markmap");
		out.println();
		out.println("WARNING: markmap data is almost certainly out of date at any time. Interpret these results with extreme care!");
	}

	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException
	{
		String subcommand;
		
		if(command.equals("!markmap")) {
			if(args.length < 1) {
				out.println("No command specified");
				printHelp(out);
				return;
			}
			subcommand = args[0];
		} else {
			subcommand = command.substring("!markmap".length());
		}
		if(subcommand.equalsIgnoreCase("ismarked")) {
			isMarked(args, context, out);
		} else if(subcommand.equalsIgnoreCase("near")) {
			near(args, context, out);
		} else if(subcommand.equalsIgnoreCase("bits")) {
			markBits(args, context, out);
		} else if(subcommand.equalsIgnoreCase("fromBits")) {
			fromBits(args, context, out);
		} else if(subcommand.equalsIgnoreCase("source")) {
			findSource(args, context, out);
		} else if(subcommand.equalsIgnoreCase("scanRange")) {
			scanRange(args, context, out);
		} else if(subcommand.equalsIgnoreCase("show")) {
			showMarkMap(args, context, out);
		} else if(subcommand.equalsIgnoreCase("set")) {
			setMarkMap(args, context, out);
		} else if(subcommand.equals("help")) {
			printHelp(out);
		} else {
			out.println("Unrecognised command: " + subcommand);
			printHelp(out);
		}
		return;
	}
	
	protected void isMarked(String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException
	{
		try {
			long address = CommandUtils.parsePointer(args[1], J9BuildFlags.env_data64);
			J9ObjectPointer object = J9ObjectPointer.cast(address);
			
			MarkedObject result = markMap.queryObject(object);
			if (result != null) {
				if (result.wasRelocated()) {
					out.format("Object %s is marked and relocated to %s\n", result.object.getHexAddress(), result.relocatedObject.getHexAddress());
				} else {
					out.format("Object %s is marked\n",  result.object.getHexAddress());
				}
			} else {
				out.format("Object %s is not marked\n",  object.getHexAddress());
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}
	
	protected void near(String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException
	{
		try {
			long address = CommandUtils.parsePointer(args[1], J9BuildFlags.env_data64);
			J9ObjectPointer object = J9ObjectPointer.cast(address);
			J9ObjectPointer base = object.untag(markMap.getPageSize(object) - 1);
			J9ObjectPointer top = base.addOffset(markMap.getPageSize(object));
			
			MarkedObject[] results = markMap.queryRange(base, top);
			reportResults(base, top, results, out);
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}
	
	protected void scanRange(String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException
	{
		try {
			long baseAddress = CommandUtils.parsePointer(args[1], J9BuildFlags.env_data64);
			long topAddress = CommandUtils.parsePointer(args[2], J9BuildFlags.env_data64);
			J9ObjectPointer base = J9ObjectPointer.cast(baseAddress);
			J9ObjectPointer top = J9ObjectPointer.cast(topAddress);
			
			MarkedObject[] results = markMap.queryRange(base, top);
			reportResults(base, top, results, out);
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}
	
	protected void reportResults(J9ObjectPointer base, J9ObjectPointer top, MarkedObject[] results, PrintStream out)
	{
		if (results.length == 0) {
			out.format("No marked objects in the range %s -> %s\n", base.getHexAddress(), top.getHexAddress());
		} else {
			out.format("Marked objects in the range %s -> %s\n", base.getHexAddress(), top.getHexAddress());
			for (int i = 0; i < results.length; i++) {
				MarkedObject result = results[i];
				if (result.wasRelocated()) {
					out.format("\t!j9object %s -> !j9object %s\n", result.object.getHexAddress(), result.relocatedObject.getHexAddress());
				} else {
					out.format("\t!j9object %s\n",  result.object.getHexAddress());
				}
			}
		}
	}
	
	protected void markBits(String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException
	{
		try {
			long address = CommandUtils.parsePointer(args[1], J9BuildFlags.env_data64);
			J9ObjectPointer object = J9ObjectPointer.cast(address);
			
			J9ObjectPointer base = J9ObjectPointer.cast(address).untag(markMap.getPageSize(object) - 1);
			J9ObjectPointer top = base.addOffset(markMap.getPageSize(object));
			MarkedObject[] result = markMap.queryRange(base, top);
			if (result.length > 0) {
				if (result[0].wasRelocated()) {
					out.format("Mark bits for the compacted range %s -> %s: !j9x %s\n", 
							base.getHexAddress(),
							top.getHexAddress(),
							result[0].markBitsSlot.getHexAddress());
				} else {
					out.format("Mark bits for the range %s -> %s: !j9x %s\n", 
							base.getHexAddress(),
							top.getHexAddress(),
							result[0].markBitsSlot.getHexAddress());
				}
			} else {
				/* Either outside the heap, or just nothing there */
				try {
					UDATA[] indexAndMask = markMap.getSlotIndexAndMask(base);
					UDATAPointer markBitsSlot = markMap.getHeapMapBits().add(indexAndMask[0]);
					out.format("Mark bits for the range %s -> %s: !j9x %s\n", 
							base.getHexAddress(),
							top.getHexAddress(),
							markBitsSlot.getHexAddress());
				} catch (IllegalArgumentException ex) {
					out.format("Object %s is not in the heap\n",  object.getHexAddress());
				}
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}
	
	protected void fromBits(String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException
	{
		long address = CommandUtils.parsePointer(args[1], J9BuildFlags.env_data64);
		UDATAPointer markSlot = UDATAPointer.cast(address);

		UDATAPointer base = markMap.getHeapMapBits();
		UDATA maxOffset = markMap.getSlotIndexAndMask(J9ObjectPointer.cast(markMap.getHeapTop().subOffset(markMap.getObjectGrain())))[0];
		maxOffset = maxOffset.add(1);
		UDATAPointer top = base.add(maxOffset);
		
		if (markSlot.gte(base) && markSlot.lte(top)) {
			IDATA delta = markSlot.sub(base);
			int pageSize = markMap.getPageSize(null);
// obviously not right for metronome
			delta = delta.mult(pageSize);
			
			J9ObjectPointer rangeBase = J9ObjectPointer.cast(markMap.getHeapBase().addOffset(delta));
			J9ObjectPointer rangeTop = rangeBase.addOffset(pageSize);
			out.format("Mark bits at %s corresponds to heap range %s -> %s\n", 
					markSlot.getHexAddress(), 
					rangeBase.getHexAddress(), 
					rangeTop.getHexAddress());
			
		} else {
			out.format("Address %s is not in the mark map\n", markSlot.getHexAddress());
		}
	}
	
	protected void findSource(String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException
	{
		try {
			long address = CommandUtils.parsePointer(args[1], J9BuildFlags.env_data64);
			J9ObjectPointer object = J9ObjectPointer.cast(address);
			
			J9ObjectPointer scanPtr = J9ObjectPointer.cast(markMap.getHeapBase());
			J9ObjectPointer heapTop  = J9ObjectPointer.cast(markMap.getHeapTop());
			if (object.gte(scanPtr) && object.lt(heapTop)) {
				int count = 0;
				while (scanPtr.lt(heapTop)) {
					J9ObjectPointer base = scanPtr;
					J9ObjectPointer top = base.addOffset(markMap.getPageSize(scanPtr));
					MarkedObject[] results = markMap.queryRange(base, top);
					for (int i = 0; i < results.length; i++) {
						MarkedObject result = results[i];
						if (result.wasRelocated()) {
							if (result.relocatedObject.eq(object)) {
								// A match!
								out.format("Object %s was relocated to %s\n", result.object.getHexAddress(), result.relocatedObject.getHexAddress());
								count += 1;
							}
						}
					}
					scanPtr = top;
				}
				if (count > 0) {
					out.format("%1 relocation candidates found\n", count);
				} else {
					out.format("No relocation candidates found\n");
				}
			} else {
				out.format("Object %s is not in the heap\n",  object.getHexAddress());
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}
	
	protected void showMarkMap(String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException
	{
		showActiveMarkMap(out);
		
		try {
			out.format("\nKnown mark maps:\n");
			MM_GCExtensionsPointer extensions = GCExtensions.getGCExtensionsPointer();
			if (GCExtensions.isStandardGC()) {
				MM_ParallelGlobalGCPointer pgc = MM_ParallelGlobalGCPointer.cast(extensions._globalCollector());
				MM_MarkMapPointer markMap = pgc._markingScheme()._markMap();
				out.format("\tactive: %s\n", markMap.getHexAddress());
			} else if (GCExtensions.isVLHGC()) {
				// probably needs a proper subclass
				MM_IncrementalGenerationalGCPointer igc = MM_IncrementalGenerationalGCPointer.cast(extensions._globalCollector());
				out.format("\tprevious: %s\n", igc._markMapManager()._previousMarkMap().getHexAddress());
				out.format("\tnext: %s\n", igc._markMapManager()._nextMarkMap().getHexAddress());
			} else if (GCExtensions.isMetronomeGC()) {
				MM_HeapMapPointer markMap = extensions.realtimeGC()._markingScheme()._markMap();
				out.format("\tactive: %s\n", markMap.getHexAddress());
			} else {
				out.format("\tUnrecognized GC policy!\n");
			}
			try {
				if (extensions.referenceChainWalkerMarkMap().notNull()) {
					out.format("\treference chain walker: %s\n", extensions.referenceChainWalkerMarkMap().getHexAddress());
				}
			} catch (NoSuchFieldError nsf) {
				// guess we don't have one
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}

	protected void showActiveMarkMap(PrintStream out) 
	{
		out.format("Currently active mark map: !mm_heapmap %s\n", markMap.getHeapMap().getHexAddress());
		out.format("\tHeap: %s -> %s\n", markMap.getHeapBase().getHexAddress(), markMap.getHeapTop().getHexAddress());
		UDATA maxOffset = markMap.getSlotIndexAndMask(J9ObjectPointer.cast(markMap.getHeapTop().subOffset(markMap.getObjectGrain())))[0];
		maxOffset = maxOffset.add(1);
		out.format("\tBits: %s -> %s\n", markMap.getHeapMapBits().getHexAddress(), markMap.getHeapMapBits().add(maxOffset).getHexAddress());
	}
	
	protected void setMarkMap(String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException
	{
		GCHeapMap newMap = null;
		try {
			if (args[1].equalsIgnoreCase("active") || args[1].equalsIgnoreCase("default")) {
				newMap = GCHeapMap.from();
			} else if (args[1].equalsIgnoreCase("previous")) {
				if (GCExtensions.isVLHGC()) {
					MM_GCExtensionsPointer extensions = GCExtensions.getGCExtensionsPointer();
					MM_IncrementalGenerationalGCPointer igc = MM_IncrementalGenerationalGCPointer.cast(extensions._globalCollector());
					newMap = GCHeapMap.fromHeapMap(igc._markMapManager()._previousMarkMap());
				} else {
					throw new DDRInteractiveCommandException("Only valid when running balanced!");
				}
			} else if (args[1].equalsIgnoreCase("next")) {
				if (GCExtensions.isVLHGC()) {
					MM_GCExtensionsPointer extensions = GCExtensions.getGCExtensionsPointer();
					MM_IncrementalGenerationalGCPointer igc = MM_IncrementalGenerationalGCPointer.cast(extensions._globalCollector());
					newMap = GCHeapMap.fromHeapMap(igc._markMapManager()._nextMarkMap());
				} else {
					throw new DDRInteractiveCommandException("Only valid when running balanced!");
				}
			} else {
				long address = CommandUtils.parsePointer(args[1], J9BuildFlags.env_data64);
				MM_HeapMapPointer heapMap = MM_HeapMapPointer.cast(address);
				newMap = GCHeapMap.fromHeapMap(heapMap);
			}
			
			/* Quick test for validity */
			newMap.queryObject(J9ObjectPointer.cast(newMap.getHeapBase()));
			newMap.queryObject(J9ObjectPointer.cast(newMap.getHeapTop().subOffset(newMap.getObjectGrain())));
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
		
		/* If we got here, the new map must be ok */
		if (newMap != null) {
			markMap = newMap;
			showActiveMarkMap(out);
		}
	}
}
