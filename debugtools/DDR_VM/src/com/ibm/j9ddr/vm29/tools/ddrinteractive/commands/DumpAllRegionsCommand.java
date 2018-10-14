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
import com.ibm.j9ddr.vm29.structure.MM_HeapRegionDescriptor$RegionType;
import com.ibm.j9ddr.vm29.j9.gc.GCHeapRegionDescriptor;
import com.ibm.j9ddr.vm29.j9.gc.GCHeapRegionIterator;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;

public class DumpAllRegionsCommand extends Command 
{
	private static final String nl = System.getProperty("line.separator");

	private static String[] regionTypesString = new String[(int)MM_HeapRegionDescriptor$RegionType.LAST_REGION_TYPE];

	public DumpAllRegionsCommand()
	{
		initializeRegionTypes();
		addCommand("dumpallregions", "cmd|help", "dump all regions in the GC");
	}

	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException 
	{
		boolean statsRequested = false;
		boolean skipReport = false;
		
		if (0 != args.length) {
			String argument = args[0];
			
			if(argument.equalsIgnoreCase("help")) {
				help(out);
				return;
			}
			statsRequested = argument.equalsIgnoreCase("stats");

			skipReport = statsRequested;
		}
		
		try {
			GCHeapRegionIterator gcHeapRegionIterator = GCHeapRegionIterator.from();
			int[] stats = new int[regionTypesString.length];
			int total = 0;
			
			String header1, header2, header3;
			String footer;
			String formatString;
			String regionType;

			initializeStats(stats);
			
			if(J9BuildFlags.env_data64) {
				header1 = "+----------------+----------------+----------------+----------------+--------+----------------+----------------------\n";
				header2 = "|    region      |     start      |      end       |    subspace    | flags  |      size      |      region type     \n";
				header3 = "+----------------+----------------+----------------+----------------+--------+----------------+----------------------\n";
				formatString = " %016x %016x %016x %016x %08x %16x %s\n";
				footer =  "+----------------+----------------+----------------+----------------+--------+----------------+----------------------\n";
			} else {
				header1 = "+--------+--------+--------+--------+--------+--------+----------------------\n";
				header2 = "| region | start  |  end   |subspace| flags  |  size  |      region type     \n";
				header3 = "+--------+--------+--------+--------+--------+--------+----------------------\n";
				formatString = " %08x %08x %08x %08x %08x %8x %s\n";
				footer =  "+--------+--------+--------+--------+--------+--------+----------------------\n";
			}
			
			if (!skipReport) {
				out.append(header1);
				out.append(header2);
				out.append(header3);
			}
		
			while(gcHeapRegionIterator.hasNext()) {
				GCHeapRegionDescriptor heapRegionDescriptor = gcHeapRegionIterator.next();
				int index = (int)heapRegionDescriptor.getRegionType();
				total += 1;
				if (index < regionTypesString.length) {
					regionType = regionTypesString[index];
					stats[index] += 1;
				} else {
					regionType = "Unknown";
				}
				if (!skipReport) {
					out.append(String.format(formatString, 
							heapRegionDescriptor.getHeapRegionDescriptorPointer().getAddress(), 
							heapRegionDescriptor.getLowAddress().getAddress(),
							heapRegionDescriptor.getHighAddress().getAddress(),
							heapRegionDescriptor.getSubSpace().getAddress(),
							heapRegionDescriptor.getTypeFlags().longValue(),
							heapRegionDescriptor.getSize().longValue(),
							regionType
						)
					);
				}
			}
		
			if (!skipReport) {
				out.append(footer);
			}
			
			if(statsRequested) {
				String formatStringStats = " \t%s: %d\n";
	
				for (int i = 0; i < regionTypesString.length; i++) {
					if(0 != stats[i]) {
						out.append(String.format(formatStringStats, regionTypesString[i], stats[i]));
					}
				}
				out.append(String.format(formatStringStats, "++++ TOTAL", total));
			}
			
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}

	}

	private void initializeStats(int[] stats) {
		for (int i = 0; i < stats.length; i++) {
			stats[i] = 0;
		}
	}
	
	private void initializeRegionTypes()
	{
		for(int i = 0; i < regionTypesString.length; i++) {
			regionTypesString[i] = "Unknown";
		}
	
		try {
			regionTypesString[(int)MM_HeapRegionDescriptor$RegionType.RESERVED] = "RESERVED";
		} catch (NoSuchFieldError e) {
		}
		
		try {
			regionTypesString[(int)MM_HeapRegionDescriptor$RegionType.FREE] = "FREE";
		} catch (NoSuchFieldError e) {
		}
		
		try {
			regionTypesString[(int)MM_HeapRegionDescriptor$RegionType.SEGREGATED_SMALL] = "SEGREGATED_SMALL";
		} catch (NoSuchFieldError e) {
		}
		
		try {
			regionTypesString[(int)MM_HeapRegionDescriptor$RegionType.SEGREGATED_LARGE] = "SEGREGATED_LARGE";
		} catch (NoSuchFieldError e) {
		}
		
		try {
			regionTypesString[(int)MM_HeapRegionDescriptor$RegionType.ARRAYLET_LEAF] = "ARRAYLET_LEAF";
		} catch (NoSuchFieldError e) {
		}
		
		try {
			regionTypesString[(int)MM_HeapRegionDescriptor$RegionType.ADDRESS_ORDERED] = "ADDRESS_ORDERED";
		} catch (NoSuchFieldError e) {
		}
		
		try {
			regionTypesString[(int)MM_HeapRegionDescriptor$RegionType.ADDRESS_ORDERED_IDLE] = "ADDRESS_ORDERED_IDLE";
		} catch (NoSuchFieldError e) {
		}
		
		try {
			regionTypesString[(int)MM_HeapRegionDescriptor$RegionType.ADDRESS_ORDERED_MARKED] = "ADDRESS_ORDERED_MARKED";
		} catch (NoSuchFieldError e) {
		}
		
		try {
			regionTypesString[(int)MM_HeapRegionDescriptor$RegionType.BUMP_ALLOCATED] = "BUMP_ALLOCATED";
		} catch (NoSuchFieldError e) {
		}
		
		try {
			regionTypesString[(int)MM_HeapRegionDescriptor$RegionType.BUMP_ALLOCATED_IDLE] = "BUMP_ALLOCATED_IDLE";
		} catch (NoSuchFieldError e) {
		}
		
		try {
			regionTypesString[(int)MM_HeapRegionDescriptor$RegionType.BUMP_ALLOCATED_MARKED] = "BUMP_ALLOCATED_MARKED";
		} catch (NoSuchFieldError e) {
		}
	}
	
	private void help(PrintStream out) {
		out.append("!dumpallregions       -- dump all regions");
		out.append(nl);
		out.append("!dumpallregions stats -- calculate regions stats");
		out.append(nl);
	}
}
