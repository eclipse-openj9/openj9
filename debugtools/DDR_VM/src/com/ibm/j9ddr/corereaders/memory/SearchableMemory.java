/*******************************************************************************
 * Copyright (c) 2004, 2015 IBM Corp. and others
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
package com.ibm.j9ddr.corereaders.memory;

import static java.util.logging.Level.FINE;
import static java.util.logging.Level.FINER;
import static java.util.logging.Level.WARNING;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.logging.Logger;

/**
 * Abstract IMemory encapsulating findPattern() logic.
 * 
 * @author andhall
 *
 */
public abstract class SearchableMemory implements IMemory
{
	static final Logger logger = Logger.getLogger(com.ibm.j9ddr.corereaders.ICoreFileReader.J9DDR_CORE_READERS_LOGGER_NAME);
	
	static final int BUFFER_MAX = 4096;
	
	static final int MINIMUM_PAGE_SIZE = 4096;

	protected long[][] rangeTable = null;
	private int mergedRanges = -1;
	
	private static int match(byte[] whatBytes, int matchedSoFar, byte[] buffer,
			int index)
	{
		int matched = matchedSoFar;
		for (int i = index; i < buffer.length && matched < whatBytes.length; i++, matched++)
			if (buffer[i] != whatBytes[matched])
				return 0;
		return matched;
	}

	public long findPattern(byte[] whatBytes, int alignment, long startFrom)
	{
		int align = 0 == alignment ? 1 : alignment; // avoid divide-by-zero errors
	
		logger.logp(FINE,"AbstractMemory","findPattern","called with {0}, alignment {1}, startFrom, {2}",new Object[]{whatBytes,alignment,Long.toHexString(startFrom)});
		
		// Ensure buffer size is a multiple of the alignment
		int bufferMax = BUFFER_MAX;
		if (bufferMax < align)
			bufferMax = align;
		else if (0 != bufferMax % align)
			bufferMax -= bufferMax % align;
	
		long location = -1;
		
		if( rangeTable == null ) {
			rangeTable = buildRangeTable();
			logger.logp(FINER,"AbstractMemory","findPattern","rangeTable contains {0} elements",rangeTable.length);
			mergedRanges = mergeRangeTable(rangeTable);
			logger.logp(FINER,"AbstractMemory","findPattern","mergedRanges contains {0} elements",mergedRanges);
		}

		for (int i=0;i<mergedRanges;i++) {
			long base = rangeTable[i][0];
			long size = rangeTable[i][1];
			long top = base + size - 1;
			
			if (top > startFrom) {
				location = findPatternInRange(whatBytes, align, startFrom, base, size, bufferMax);
				if (-1 != location) {
					logger.logp(FINE,null,null,"Pattern matched at {0}",Long.toHexString(location));
					break;
				}
			}
		}
		if (-1 == location) {
			logger.logp(FINE,null,null,"Pattern didn't match");
		}
		return location;
	}

	/**
	 * Merges consecutive ranges to build a list of ranges to read
	 * @param rangeTable Range table to modify
	 * @return Number of merged fields
	 */
	protected int mergeRangeTable(long[][] rangeTable)
	{
		int readPointer;
		int writePointer = 0;
		
		if (rangeTable.length == 0) {
			// Empty range table is a special case not handled below. Nothing to merge so return zero. 
			return 0;
		}
		
		for (readPointer = 1;readPointer < rangeTable.length; readPointer++) {
			if (rangeTable[readPointer][0] == rangeTable[writePointer][0] + rangeTable[writePointer][1]) {
				rangeTable[writePointer][1] += rangeTable[readPointer][1];
			} else {
				writePointer++;
				rangeTable[writePointer][0] = rangeTable[readPointer][0];
				rangeTable[writePointer][1] = rangeTable[readPointer][1];
			}
		}
		
		return writePointer + 1;
	}

	/**
	 * 
	 * @return Memory ranges and sizes. Addressable as array[range index][0=base address, 1=size]
	 */
	protected long[][] buildRangeTable()
	{
		List<? extends IMemoryRange> ranges = new ArrayList<IMemoryRange>(getMemoryRanges());
		Collections.sort(ranges);
		
		long[][] toReturn = new long[ranges.size()][2];
		int index = 0;
		
		for (IMemoryRange range : ranges) {
			// We can't search unbacked ranges!
			if( range.isBacked() ) {
				toReturn[index][0] = range.getBaseAddress();
				toReturn[index][1] = range.getSize();
				index++;
			}
		}
		
		return toReturn;
	}

	public SearchableMemory()
	{
		super();
	}

	private long findPatternInRange(byte[] whatBytes, int alignment, long start,
			long searchRangeBase, long searchRangeSize, int bufferMax)
	{
			// Ensure the address is within the range and aligned
			long addr = start;
			if (addr < searchRangeBase) {
				addr = searchRangeBase;
			}
			
			if (0 != addr % alignment) {
				addr += alignment - (addr % alignment);
			}
			
			long edge = searchRangeBase + searchRangeSize;
				// FIXME this is somewhat inefficient - should use Boyer-Moore
			int matched = 0;
			long matchAddr = -1;
			while (addr < edge) {
				long bytesLeftInRange = edge - addr;
				long count = bufferMax < bytesLeftInRange ? bufferMax : bytesLeftInRange;
					
				byte[] buffer = new byte[(int)count];
				try {
					getBytesAt(addr, buffer);
				} catch (MemoryFault ex) {
					/* Couldn't read entire range. If there was memory prior to the fault, read that instead */
					if (Addresses.greaterThan(ex.getAddress(),addr)) {
						count = ex.getAddress() - addr;
						
						buffer = new byte[(int)count];
						
						try {
							getBytesAt(addr,buffer);
						} catch (MemoryFault e) {
							logger.log(WARNING,"J9DDR findPattern algorithm broken. Double memory fault in findPatternInRange. addr={0}, count={1}",new Object[]{addr,count});
							addr += count;
							continue;
						}
					} else {
						/* We need to find the next bit of good data */
						addr = findNextGoodAddress(addr,edge);
						continue;
					}
				}
				
				if (null != buffer) {
					// Handle partial match
					if (0 != matched) {
						matched = match(whatBytes, matched, buffer, 0);
					}
					
					// Handle no match
					for (int i = 0; i < buffer.length && 0 == matched; i += alignment) {
						matchAddr = addr + i;
						matched = match(whatBytes, 0, buffer, i);
					}
					
					// Check for a full match
					if (whatBytes.length == matched) {
						return matchAddr;
					}
				}
			
				addr += count;
			}
		return -1;
	}

	/**
	 * Used by findPatternInRange.
	 * 
	 * If there is a dead section of memory in the middle of the range, we need to walk over it to find the next viable section.
	 * 
	 * Touching every byte to trigger a MemoryFault would be painfully slow.  Instead, we'll assume that memory faults will be 
	 * determined on a per-page (4k) basis, and we'll try to find the next valid page.
	 * 
	 * @param addr Current bad address
	 * @param edge Top end of current range
	 * @return The first valid address above addr we find, or edge if we can't find any
	 */
	private long findNextGoodAddress(long addr, long edge)
	{
		long pageBoundary = addr - (addr % MINIMUM_PAGE_SIZE);
		
		long pointer = pageBoundary + MINIMUM_PAGE_SIZE;
		
		while (pointer < edge) {
			try {
				getByteAt(pointer);
				return pointer;
			} catch (MemoryFault ex) {
				/* Deliberately do nothing */
			}
			pointer += MINIMUM_PAGE_SIZE;
		}
		
		return edge;
	}

}
