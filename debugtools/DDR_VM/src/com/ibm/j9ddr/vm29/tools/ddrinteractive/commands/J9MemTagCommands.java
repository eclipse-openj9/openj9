/*******************************************************************************
 * Copyright (c) 2010, 2019 IBM Corp. and others
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
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.TreeMap;
import java.util.Map.Entry;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.corereaders.memory.Addresses;
import com.ibm.j9ddr.events.IEventListener;
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.vm29.events.EventManager;
import com.ibm.j9ddr.vm29.j9.walkers.J9MemTagIterator;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9MemTagPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9MemTagHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9MemTagHelper.J9MemTagCheckError;
import com.ibm.j9ddr.vm29.structure.J9MemTag;
import com.ibm.j9ddr.vm29.types.UDATA;

/**
 * 
 */
public class J9MemTagCommands extends Command implements IEventListener
{
	public static final long SEARCH_SLAB_SIZE = 1024;
	
	private final int SORT_TYPE_DEFAULT = 1;
	private final int SORT_TYPE_NAME = 2;
	private final int SORT_TYPE_ALLOCSIZE = 3;
	
	private final String SORT_TYPE_STRING_NAME = "name";
	private final String SORT_TYPE_STRING_ALLOCSIZE = "allocsize";
	
	private final String SORT_TYPE_PREFIX = "sort:";
	
	private static final String commands[][] = new String[][] { 
		new String[]{"!findcallsite <callsite>[,start[,end]] [sort:<name|allocsize>]", "list all allocations for the specified callsite."},
		new String[]{"!printallcallsites [sort:<name|allocsize>]", "list all blocks and bytes allocated by each callsite (same as !findcallsite *)."},
		new String[]{"!printfreedcallsites [sort:<name|allocsize>]", "list all freed blocks and bytes allocated by each callsite. (same as !findfreedcallsite *)"},
		new String[]{"!findheader", "locate the memory allocation header for the specified address."},
		new String[]{"!findallcallsites [sort:<name|allocsize>]", "list a summary of blocks and bytes allocated by each callsite."},
		new String[]{"!findfreedcallsites [sort:<name|allocsize>]", "list a summary of all freed blocks and bytes allocated by each callsite."},
		new String[]{"!findfreedcallsite <callsite>[,start[,end]] [sort:<name|allocsize>]", "list all freed blocks for the specified callsite."}
	};
	
	private PrintStream out;

	public J9MemTagCommands()
	{
		addCommand("findcallsite", "<callsite>[,start[,end]] [sort:<name|allocsize>]", "list all allocations for the specified callsite.");
		addCommand("printallcallsites", "[sort:<name|allocsize>]", "list all blocks and bytes allocated by each callsite (same as !findcallsite *).");
		addCommand("printfreedcallsites", "[sort:<name|allocsize>]", "list all freed blocks and bytes allocated by each callsite. (same as !findfreedcallsite *)");
		addCommand("findheader", "", "locate the memory allocation header for the specified address.");
		addCommand("findallcallsites", "[sort:<name|allocsize>]", "list a summary of blocks and bytes allocated by each callsite.");
		addCommand("findfreedcallsites", "[sort:<name|allocsize>]", "list a summary of all freed blocks and bytes allocated by each callsite.");
		addCommand("findfreedcallsite", "<callsite>[,start[,end]] [sort:<name|allocsize>]", "list all freed blocks for the specified callsite.");
	}
		
	/* (non-Javadoc)
	 * @see com.ibm.j9ddr.tools.ddrinteractive.ICommand#run(java.lang.String, java.lang.String[], com.ibm.j9ddr.tools.ddrinteractive.Context, java.io.PrintStream)
	 */
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException
	{
		command = command.toLowerCase();
		
		this.out = out;
		
		EventManager.register(this);
		try {
			if (command.endsWith("findallcallsites")) {
				runFindAllCallsites(command, args, context);
				return;
			}
			
			if (command.endsWith("findfreedcallsites")) {
				runFindAllFreedCallsites(command, args, context);
				return;
			}
			
			if (command.endsWith("findfreedcallsite")) {
				runFindFreedCallsite(command, args, context);
				return;
			}
			
			if (command.endsWith("printallcallsites")) {
				runPrintAllCallsites(command, args, context);
				return;
			}
			
			if (command.endsWith("printfreedcallsites")) {
				runPrintAllFreedCallsites(command, args, context);
				return;
			}
			
			if (command.endsWith("findheader")) {
				runFindHeader(command, args, context);
				return;
			}
			
			if (command.endsWith("findcallsite")) {
				runFindCallsite(command, args, context);
				return;
			}
			
			throw new DDRInteractiveCommandException("Unrecognized command: " + command);
		} finally {
			EventManager.unregister(this);
		}
	}
	
	private void printUsageForFindFreedCallsite() 
	{

		out.println("Usage:");
		out.println("  !findfreedcallsite <callsite> [sort:<name|allocsize>]");
		out.println("  !findfreedcallsite <callsite>,<start> [sort:<name:allocsize>]");
		out.println("  !findfreedcallsite <callsite>,<start>,<end> [sort:<name|allocsize>]");
		
	}
	
	private void runFindFreedCallsite(String command, String[] args, Context context) 
		throws DDRInteractiveCommandException
	{
		final long matchFlags;
		long startAddress = 0;
		long endAddress = UDATA.MASK;
		
		final String needle;
		String callsiteEnd;
		
		int sortType = SORT_TYPE_DEFAULT;

		if (0 == args.length) {
			printUsageForFindFreedCallsite();
			return;
		}
		
		String[] realArgs = args[0].split(",");
		callsiteEnd = realArgs[0];

		if (realArgs.length == 2) {
			startAddress = Long.decode(realArgs[1]);
		} else if (realArgs.length == 3) {
			startAddress = Long.decode(realArgs[1]);
			endAddress = Long.decode(realArgs[2]);
		} else if (realArgs.length > 3 || args.length > 2){
			out.print("Too many args : ");
			for (int i = 0; i < args.length - 1; i++) {
				out.print(args[i]);
			}
			out.println(args[args.length - 1]);
			printUsageForFindFreedCallsite();
			return;			
		}		
		
		
		if (args.length == 2) {
			sortType = parseSortType(args[1]);
			if (sortType == -1) {
				printUsageForFindFreedCallsite();
				return;
			}
		}
		
		if (Addresses.greaterThan(startAddress,endAddress)) {
			out.println("Error: start address cannot be greater than end address");
			return;
		}

		StringBuffer needleBuffer = new StringBuffer();
		matchFlags = WildCard.parseWildcard(callsiteEnd, needleBuffer);
		
		if (matchFlags < 0) {
			out.println("Error: Invalid wildcard(s) in callsite");
			return;
		} else {
			needle = needleBuffer.toString();
		}

		J9MemTagIterator it = J9MemTagIterator.iterateFreedHeaders(startAddress, endAddress);
		
		
		if (J9BuildFlags.env_data64) {
			out.println("+------------------------------------------+------------------+-------------------+");
			out.println("|          address      |      size        |    org size      | callsite          |");
			out.println("+------------------------------------------+------------------+-------------------+");
		/*	         " !j9x 0x000000000751F180,0x000001000 (0x000003390)   jsrinliner.c:567 */
		} else {
			out.println("+--------------------------+----------+-------------------+");
			out.println("|      address  |   size   | org size | callsite          |");
			out.println("+--------------------------+----------+-------------------+");
		/*	         " !j9x 0x0751F180,0x00001000 (0x00003390) jsrinliner.c:567 */
		}		
		
		
		ArrayList<MemTagEntry> freedMemTagEntries = new ArrayList<MemTagEntry>();
		
		int freedCallSiteCount = 0;
		int corruptedFreedCallSiteCount = 0;
		
		while (it.hasNext()) {
			J9MemTagPointer header = it.next();

			if (!regexMatches(header, matchFlags, needle)) {
				continue;
			}
			freedCallSiteCount++;
			
			try {
				long allocSize = header.allocSize().longValue();	
				if (it.isFooterCorrupted()) {
					/* 
					 * Corrupted footer is accepted only if freed call sites are being searched. 					 
					 * 
					 * If any part of current freed memory block re-allocated, 
					 * then just assume the memory between freed header and alloc header as freed memory.
					 *   
					 */
					corruptedFreedCallSiteCount++;
					J9MemTagIterator allocIte = J9MemTagIterator.iterateAllocatedHeaders(header.longValue() + J9MemTag.SIZEOF, header.add(allocSize).longValue() + + J9MemTag.SIZEOF);
					if (allocIte.hasNext()) {
						J9MemTagPointer nextAllocHeader = allocIte.next();
						allocSize = nextAllocHeader.longValue() - header.longValue();
					}
				}
				
				if (SORT_TYPE_DEFAULT != sortType) {
					freedMemTagEntries.add(new MemTagEntry(header, allocSize));
				} else {
					printMemTagForFindFreedCallSite(new MemTagEntry(header, allocSize));
				}
				
			} catch (CorruptDataException e) {
				//Unlikely - we've already checksummed the block
				e.printStackTrace(out);
			}
		}

		if (SORT_TYPE_NAME == sortType || SORT_TYPE_ALLOCSIZE == sortType) {
			if (SORT_TYPE_NAME == sortType) {
				Collections.sort(freedMemTagEntries, new NameComparator());
			} else {
				Collections.sort(freedMemTagEntries, new AllocSizeComparator());
			}
		
			for (int i = 0; i < freedMemTagEntries.size(); i++) {
				MemTagEntry freedMemTagEntry = freedMemTagEntries.get(i);
				printMemTagForFindFreedCallSite(freedMemTagEntry);
			}		
		}
		
		out.println("Freed call site count = " + freedCallSiteCount);
		out.println("Corrupted freed call site count = " + corruptedFreedCallSiteCount);
	}
	
	private void printMemTagForFindFreedCallSite(MemTagEntry freedMemTagEntry)
	{
		J9MemTagPointer freedHeader =  freedMemTagEntry.getHeader();
		long allocSize = freedMemTagEntry.getAllocSize();
		
		try {
			String callsite = freedHeader.callSite().getCStringAtOffset(0);
			if (J9BuildFlags.env_data64) {
				out.append(String.format(" !j9x 0x%s ", padWith('0', J9MemTagHelper.j9mem_get_memory_base(freedHeader).getHexAddress().substring(2), 16)));
				out.append(String.format("0x%s ", padWith('0', Long.toHexString(allocSize), 16)));
				if (allocSize == freedHeader.allocSize().longValue()) {
					out.append(String.format("%19s", ""));
				} else {
					out.append(String.format("%s ", freedHeader.allocSize().getHexValue()));
				}
				out.append(String.format("%s", callsite));
			} else {
				out.append(String.format(" !j9x 0x%s ", padWith('0', J9MemTagHelper.j9mem_get_memory_base(freedHeader).getHexAddress().substring(2), 8)));
				out.append(String.format("0x%s ", padWith('0', Long.toHexString(allocSize), 8)));
				if (allocSize != freedHeader.allocSize().longValue()) {
					out.append(String.format("%13s", ""));
				} else {
					out.append(String.format("%s ", freedHeader.allocSize().getHexValue()));
				}
				out.append(String.format("%s", callsite));
			}
			out.append('\n');
		
		} catch (CorruptDataException e) {
			//Unlikely - we've already checksummed the block
			e.printStackTrace(out);
		}			
	}
	
	private void printUsageForFindCallsite() 
	{

		out.println("Usage:");
		out.println("  !findcallsite <callsite> [sort:<name|allocsize>]");
		out.println("  !findcallsite <callsite>,<start> [sort:<name|allocsize>]");
		out.println("  !findcallsite <callsite>,<start>,<end> [sort:<name|allocsize>]");
		
	}

	private void runFindCallsite(String command, String[] args, Context context) throws DDRInteractiveCommandException
	{
		final long matchFlags;
		long startAddress = 0;
		long endAddress = UDATA.MASK;

		final String needle;
		String callsiteEnd;
		
		int sortType = SORT_TYPE_DEFAULT;

		if (args.length == 0) {
			printUsageForFindCallsite();
			return;
		}

		String[] realArgs = args[0].split(",");
		callsiteEnd = realArgs[0];
		
		if (args.length == 2) {
			sortType = parseSortType(args[1]);
			if (sortType == -1) {
				printUsageForFindCallsite();
				return;
			}
		}

		if (realArgs.length == 2) {
			startAddress = Long.decode(realArgs[1]);
		} else if (realArgs.length == 3) {
			startAddress = Long.decode(realArgs[1]);
			endAddress = Long.decode(realArgs[2]);
		} else if (realArgs.length > 3 || args.length > 2){
			out.print("Too many args : ");
			for (int i = 0; i < args.length - 1; i++) {
				out.print(args[i]);
			}
			out.println(args[args.length - 1]);
			printUsageForFindCallsite();
			return;			
		}		

		if (Addresses.greaterThan(startAddress,endAddress)) {
			out.println("Error: start address cannot be greater than end address");
			return;
		}

		StringBuffer needleBuffer = new StringBuffer();
		matchFlags = WildCard.parseWildcard(callsiteEnd, needleBuffer);
		if (matchFlags < 0) {
			out.println("Error: Invalid wildcard(s) in callsite");
			return;
		} else {
			needle = needleBuffer.toString();
		}
		
		J9MemTagIterator it = J9MemTagIterator.iterateAllocatedHeaders(startAddress, endAddress);
		
		ArrayList<MemTagEntry> memTagEntries = new ArrayList<MemTagEntry>();

		int callSiteCount = 0;
	
		while (it.hasNext()) {
			J9MemTagPointer header = it.next();

			if (!regexMatches(header, matchFlags, needle)) {
				continue;
			}
			callSiteCount++;
			try {
				long allocSize = header.allocSize().longValue();
				if (SORT_TYPE_DEFAULT != sortType) {
					memTagEntries.add(new MemTagEntry(header, allocSize));
				} else {
					printMemTagForFindCallSite(new MemTagEntry(header, allocSize));
				}
			} catch (CorruptDataException e) {
				//Unlikely - we've already checksummed the block
				e.printStackTrace(out);
			}
		}
		
		if (SORT_TYPE_NAME == sortType || SORT_TYPE_ALLOCSIZE == sortType){
			if (SORT_TYPE_NAME == sortType)	{
				Collections.sort(memTagEntries, new NameComparator());
			}
			else {
				Collections.sort(memTagEntries, new AllocSizeComparator());
			}
			
			for (int i = 0; i < memTagEntries.size(); i++) {
				MemTagEntry memTagEntry = memTagEntries.get(i);
				printMemTagForFindCallSite(memTagEntry);
			}
		}
		out.println("Call site count = " + callSiteCount);
	}
	
	private void printMemTagForFindCallSite(MemTagEntry memTagEntry)
	{
		J9MemTagPointer header =  memTagEntry.getHeader();
		try {
			String callsite = header.callSite().getCStringAtOffset(0);
			out.println(String.format(" !j9x %s,%s\t%s", J9MemTagHelper.j9mem_get_memory_base(header).getHexAddress(), header.allocSize().getHexValue(), callsite));
		} catch (CorruptDataException e) {
			//Unlikely - we've already checksummed the block
			e.printStackTrace(out);
		} 
	}

	private void runFindHeader(String command, String[] args, Context context) throws DDRInteractiveCommandException
	{
		long address = 0;

		J9MemTagPointer header = null;

		if (args.length != 1) {
			out.println("Usage: ");
			out.println("  !findheader <address> (e.g. !findheader 0xa2b4c6d8)");
			return;
		}
		
		address = CommandUtils.parsePointer(args[0], J9BuildFlags.env_data64);

		out.println(String.format("Searching memory allocation header for %s", U8Pointer.cast(address).getHexAddress()));

		/*
		 * Search for an eyecatcher on or before the specified address. Start
		 * the search on the specified address since an eyecatcher may start on
		 * the address. Ensure that the eyecatcher that is found is on or before
		 * the address. If this condition is not satisfied, start searching for
		 * an eyecatcher 1K before the previous start address. Continue until an
		 * eyecatcher is found, or the start address of 0 is reached.
		 */
		
		long searchBase = address - SEARCH_SLAB_SIZE;
		
SEARCH_LOOP:do {
			/* Add 3 (length of eyecatcher - 1) to the top address, to catch eyecatchers written between search slabs */
			J9MemTagIterator it = J9MemTagIterator.iterateAllocatedHeaders(searchBase, searchBase + SEARCH_SLAB_SIZE + 3);
			
			while (it.hasNext()) {
				J9MemTagPointer potential = it.next();
				
				if (Addresses.greaterThan(potential.getAddress(), address)) {
					/* Walked past address */
					break;
				}
				
				VoidPointer start = J9MemTagHelper.j9mem_get_memory_base(potential);
				VoidPointer end;
				try {
					end = start.addOffset(potential.allocSize().longValue());
				} catch (CorruptDataException e) {
					continue;
				}
				
				if (Addresses.greaterThanOrEqual(address, start.getAddress()) && Addresses.lessThan(address, end.getAddress())) {
					/* Match */
					header = potential;
					break SEARCH_LOOP;
				}
			}
			
			if (Addresses.lessThan(searchBase,SEARCH_SLAB_SIZE)) {
				searchBase = 0;
			} else {
				searchBase = searchBase - SEARCH_SLAB_SIZE;
			}
		} while (searchBase != 0);
		
		if (header == null) {
			out.println("No memory allocation header found");
		} else {
			
			String callsite;
			
			try {
				callsite = header.callSite().getCStringAtOffset(0);
			} catch (CorruptDataException ex) {
				callsite = "<FAULT> reading callsite string: " + ex.getMessage();
			}
			
			String categoryName;
			try {
				categoryName = header.category().name().getCStringAtOffset(0);
			} catch (CorruptDataException ex) {
				categoryName = "<FAULT> reading category name string: " + ex.getMessage();
			}
			
			try {
				out.println(String.format("Found memory allocation header, !j9x %s,0x%#x", J9MemTagHelper.j9mem_get_memory_base(header).getHexAddress(), header.allocSize().longValue()));
				out.println(String.format("J9MemTag at %s {", header.getHexAddress()));
				out.println(String.format("    U_32 eyeCatcher = 0x%x;", header.eyeCatcher().longValue()));
				out.println(String.format("    U_32 sumCheck = 0x%x;", header.sumCheck().longValue()));
				out.println(String.format("    UDATA allocSize = 0x%x;", header.allocSize().longValue()));
				out.println(String.format("    char* callSite = %s;", callsite));
				out.println(String.format("    struct OMRMemCategory* category = !omrmemcategory 0x%x (%s);", header.category().longValue(), categoryName));
				out.println("}");
			} catch (CorruptDataException ex) {
				out.println("CDE formatting J9MemTag at " + header.getHexAddress());
			}
		}
	}
	
	/**
	 * Parses the sort arg : [sort<name|allocsize>]
	 * @param sortArg
	 * @return
	 */
	private int parseSortType (String sortArg) 
		throws DDRInteractiveCommandException 
	{
		if (sortArg.length() > SORT_TYPE_PREFIX.length() && sortArg.substring(0, SORT_TYPE_PREFIX.length()).equalsIgnoreCase(SORT_TYPE_PREFIX)) {
			String tmp = sortArg.substring(SORT_TYPE_PREFIX.length());
			if (tmp.equalsIgnoreCase(SORT_TYPE_STRING_NAME)) {
				return SORT_TYPE_NAME;
			} else if (tmp.equalsIgnoreCase(SORT_TYPE_STRING_ALLOCSIZE)) {
				return SORT_TYPE_ALLOCSIZE;
			} else {
				throw new DDRInteractiveCommandException("Error: Unknown sort type: " + sortArg);
			}
		} else {
			throw new DDRInteractiveCommandException("Error: Unknown argument: " + sortArg);
		}
	}

	private void runPrintAllCallsites(String command, String[] args, Context context)
		throws DDRInteractiveCommandException
	{
		out.println("Searching for all memory block callsites...");
		String [] newArgs = new String[args.length + 1];
		newArgs[0] = "*";
		if (args.length == 1) {
			newArgs[1] = args[0];
		}
		runFindCallsite("!findcallsite", newArgs, context);
	}
	
	private void runPrintAllFreedCallsites(String command, String[] args, Context context)
		throws DDRInteractiveCommandException
	{
		out.println("Searching for all freed memory block callsites...");
		String [] newArgs = new String[args.length + 1];
		newArgs[0] = "*";
		if (args.length == 1) {
			newArgs[1] = args[0];
		}
		runFindFreedCallsite("!findfreedcallsite", newArgs, context);
	}

	private void runFindAllCallsites(String command, String[] args,
			Context context) throws DDRInteractiveCommandException
	{
		int sortType = SORT_TYPE_ALLOCSIZE;
		
		J9MemTagIterator it = J9MemTagIterator.iterateAllocatedHeaders();
		
		if (args.length == 1) {
			sortType = parseSortType(args[0]);
		}
		printCallsitesTable(buildCallsitesTable(it, true), sortType);
	}
	
	private void runFindAllFreedCallsites(String command, String[] args, Context context) 
		throws DDRInteractiveCommandException
	{
		out.println("Searching for all freed memory block callsites...");
		int sortType = SORT_TYPE_ALLOCSIZE;
		
		J9MemTagIterator it = J9MemTagIterator.iterateFreedHeaders();
		
		if (args.length == 1) {
			sortType = parseSortType(args[0]);
		}
		
		printCallsitesTable(buildCallsitesTable(it, true), sortType);
	}

	
	/**
	 * This method pads strings upto desired length. 
	 * I does padding at the beginning of the string. 
	 * 
	 * @param padding	padding char to be used for padding
	 * @param strToPad	string that needs to be pad
	 * @param lengthToPad	final length that string needs to be padded to
	 * @return	padded string
	 */
	public String padWith(char padding, String strToPad, int lengthToPad) 
	{
		String paddingStr = "";
		int strLength = strToPad.length();
		if (strLength >= lengthToPad) {
			return strToPad;
		} else {
			int padLength = lengthToPad - strLength;
			for (int i = 0; i < padLength; i++) {
				paddingStr += padding;
			}
			return paddingStr + strToPad;
		}
	}
	
	private Map<String, J9DbgExtMemStats>  buildCallsitesTable(J9MemTagIterator it, boolean lookingForFreedCallSites)
	{
		Map<String, J9DbgExtMemStats> callSites = new TreeMap<String, J9DbgExtMemStats>();
		
		while (it.hasNext()) {
			J9MemTagPointer header = it.next();
			long allocSize;
			
			String callsite;
			try {
				allocSize = header.allocSize().longValue();
				if (lookingForFreedCallSites && it.isFooterCorrupted()) {
					/* 
					 * Corrupted footer is accepted only if freed call sites are being searched. 					 
					 * 
					 * If any part of current freed memory block re-allocated, 
					 * then just assume the memory between freed header and alloc header as freed memory.
					 *   
					 */
					J9MemTagIterator allocIte = J9MemTagIterator.iterateAllocatedHeaders(header.longValue() + J9MemTag.SIZEOF, header.add(allocSize).longValue() + + J9MemTag.SIZEOF);
					if (allocIte.hasNext()) {
						J9MemTagPointer nextAllocHeader = allocIte.next();
						allocSize = nextAllocHeader.longValue() - header.longValue();
					}
				}
				
				callsite = header.callSite().getCStringAtOffset(0);
				
				/*
				 * Since the type of next alloc size memory is known, iterator can safely jump to the end of it.  
				 */				
				it.moveCurrentSearchAddress(allocSize);
				
				if (!callSites.containsKey(callsite)) {
					callSites.put(callsite, new J9DbgExtMemStats(header.allocSize().longValue()));
				} else {
					J9DbgExtMemStats memStats = callSites.get(callsite);
					memStats.incrementTotalBlocksAllocated();
					memStats.addTotalBytesAllocated(header.allocSize().longValue());
				}
			} catch (CorruptDataException e) {
				//Unlikely - J9MemTagIterator already checksummed the header
				out.println("Unexpected CDE reading contents of " + Long.toHexString(header.getAddress()));
				e.printStackTrace(out);
				continue;
			}
		}		
		return callSites;
	}

	/**
	 * CorruptData handler used with J9MemTagIterator
	 */
	public void corruptData(String message, CorruptDataException e,
			boolean fatal)
	{
		if (e instanceof J9MemTagCheckError) {
			J9MemTagCheckError casted = (J9MemTagCheckError) e;
			out.println("J9MemTag check failed at " + Long.toHexString(casted.getAddress()) + ": " + message + " :" + e.getMessage());
		} else {
			e.printStackTrace(out);
		}
	}

	private void printCallsitesTable(Map<String, J9DbgExtMemStats> callSites, final int sortType)
			throws DDRInteractiveCommandException
	{
		out.println(" total alloc   | largest");
		out.println(" blocks| bytes | bytes | callsite");
		out.println("-------+-------+-------+-------+-------+-------+-------+-------+-------+-------");

		if (SORT_TYPE_NAME == sortType || SORT_TYPE_ALLOCSIZE == sortType) {
			List<Map.Entry<String, J9DbgExtMemStats>> entries = new LinkedList(callSites.entrySet());
			Collections.sort(entries, new Comparator<Map.Entry<String, J9DbgExtMemStats>>() {
				public int compare(Entry<String, J9DbgExtMemStats> o1,
						Entry<String, J9DbgExtMemStats> o2) {
						if (SORT_TYPE_NAME == sortType) {
							return o1.getKey().compareTo(o2.getKey());
						} else {
							if (o1.getValue().getTotalBytesAllocated() < o2.getValue().getTotalBytesAllocated())
								return 1;
							else if (o1.getValue().getTotalBytesAllocated() == o2.getValue().getTotalBytesAllocated()) {
								return 0;
							} else {
								return -1;
							}
						}
					}
			});
			for (int i = 0; i < entries.size(); i++) {
				Entry<String, J9DbgExtMemStats> entry = entries.get(i);
				String callSite = entry.getKey();
				J9DbgExtMemStats memStat = entry.getValue();
				out.println(String.format("%7d %7d %7d %s", memStat.getTotalBlocksAllocated(), memStat.getTotalBytesAllocated(), memStat.getLargestBlockAllocated(), callSite));
			}
		} else if (SORT_TYPE_DEFAULT == sortType){
			for (Entry<String, J9DbgExtMemStats> entry : callSites.entrySet()) {
				String callSite = entry.getKey();
				J9DbgExtMemStats memStat = entry.getValue();
				out.println(String.format("%7d %7d %7d %s", memStat.getTotalBlocksAllocated(), memStat.getTotalBytesAllocated(), memStat.getLargestBlockAllocated(), callSite));
			}
		} else {
			/**
			 * 
			 */
			throw  new DDRInteractiveCommandException("Unknown sort type: " + sortType);
		}
		out.println("-------+-------+-------+-------+-------+-------+-------+-------+-------+-------");
	}
	
	static class J9DbgExtMemStats {
		private long totalBlocksAllocated;
		private long totalBytesAllocated;
		private long largestBlockAllocated;

		public J9DbgExtMemStats(long totalBytesAllocated) {
			this.totalBytesAllocated = totalBytesAllocated;
			totalBlocksAllocated = 1;
			largestBlockAllocated = totalBytesAllocated;
		}

		public void incrementTotalBlocksAllocated() {
			totalBlocksAllocated++;
		}

		public void addTotalBytesAllocated(long byteAmount) {
			totalBytesAllocated += byteAmount;
			if (byteAmount > largestBlockAllocated) {
				largestBlockAllocated = byteAmount;
			}
		}

		public long getTotalBlocksAllocated() {
			return totalBlocksAllocated;
		}

		public long getTotalBytesAllocated() {
			return totalBytesAllocated;
		}

		public long getLargestBlockAllocated() {
			return largestBlockAllocated;
		}
	}
	
	/**
	 * Custom class to keep the headers and their allocated sizes.
	 * In the case of freed J9MemTags, calculated allocated size might be different than the one in the header.
	 * This happens if the memory block is freed and reallocation happens in the freed memory space.  
	 *
	 */
	private class MemTagEntry {
		private J9MemTagPointer header;
		private long allocSize;
		public MemTagEntry(J9MemTagPointer header, long allocSize){
			this.header = header;
			this.allocSize = allocSize;
		}
		
		public J9MemTagPointer getHeader() {
			return header;
		}
		
		public long getAllocSize () {
			return allocSize;
		}
	}

	/**
	 * Custom Comparator is used to sort J9MemTags depending on their allocated sizes. 
	 * It is used to sort the list of MemTagEntries
	 * 
	 *
	 */
	private class NameComparator implements Comparator<MemTagEntry> {
		public int compare(MemTagEntry e1, MemTagEntry e2) {
			String callsite1 = null;
			String callsite2 = null;
			
			try {
				callsite1 = e1.getHeader().callSite().getCStringAtOffset(0);
				callsite2 = e2.getHeader().callSite().getCStringAtOffset(0);
			} catch (CorruptDataException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
			return callsite1.compareTo(callsite2);
		}
	}
	/**
	 * Custom Comparator is used to sort J9MemTags depending on their allocated sizes. 
	 * It is used to sort the list of MemTagEntries
	 * 
	 *
	 */

	private class AllocSizeComparator implements Comparator<MemTagEntry> {
		public int compare(MemTagEntry e1, MemTagEntry e2) {
			if (e1.getAllocSize() < e2.getAllocSize()) {
				return 1;
			} else if (e1.getAllocSize() == e2.getAllocSize()) {
				return 0;
			}
			return -1;
		}
	}
	
	private boolean regexMatches(J9MemTagPointer obj, long matchFlags, String needle)
	{
		String callsite;
		try {
			callsite = obj.callSite().getCStringAtOffset(0);
		} catch (CorruptDataException e) {
			return false;
		}
		
		return WildCard.wildcardMatch(matchFlags, needle, callsite);
	}
}
