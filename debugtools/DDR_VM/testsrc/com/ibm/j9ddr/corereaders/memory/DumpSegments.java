/*******************************************************************************
 * Copyright (c) 2009, 2018 IBM Corp. and others
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

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;

import com.ibm.j9ddr.corereaders.CoreReader;
import com.ibm.j9ddr.corereaders.ICore;
import com.ibm.j9ddr.corereaders.memory.IAddressSpace;
import com.ibm.j9ddr.corereaders.memory.IMemoryRange;

/**
 * Test class that dumps out all the segments in a dump and some integers read
 * from each segment.
 * 
 * Useful for comparing behaviour of the core readers with previous
 * implementations.
 * 
 * @author andhall
 * 
 */
public class DumpSegments
{

	private static class Range
	{
		public long base;
		public long size;
	}

	/**
	 * @param args
	 */
	public static void main(String[] args) throws Exception
	{
		if (args.length != 1) {
			System.err.println("Expected 1 argument, got " + args.length);
		}

		String filename = args[0];

		ICore core = CoreReader.readCoreFile(filename);

		Collection<? extends IAddressSpace> addressSpaces = core.getAddressSpaces();

		for (IAddressSpace as : addressSpaces) {
			System.err.println("Address space: " + as.getAddressSpaceId());

			List<? extends IMemoryRange> ranges = new ArrayList<IMemoryRange>(as.getMemoryRanges());

			Collections.sort(ranges);

			// System.err.println("Raw mappings");
			// for(IMemoryRange thisRange : ranges) {
			// System.err.println("Base: " +
			// Long.toHexString(thisRange.getBaseAddress()) + " - size: " +
			// Long.toHexString(thisRange.getSize()));
			// if (thisRange.getBaseAddress() == 0xf95f000) {
			// System.err.println("Found!");
			// }
			// }

			List<Range> localRanges = new LinkedList<Range>();

			for (IMemoryRange range : ranges) {
				Range newRange = new Range();
				newRange.base = range.getBaseAddress();
				newRange.size = range.getSize();
				localRanges.add(newRange);
			}

			localRanges = mergeRanges(localRanges);

			for (Range range : localRanges) {
				System.err.println("Base: " + Long.toHexString(range.base)
						+ " - size: " + Long.toHexString(range.size));

				try {
					if (range.size > 7) {
						/*
						 * Pick three addresses and read integers from them -
						 * proves that endian conversion is working
						 */
						long address1 = range.base;
						long address2 = range.base + range.size - 5;
						long address3 = (address1 + address2) / 2;

						System.err.print(Long.toHexString(address1) + " - ");
						System.err.println(Integer.toHexString(as
								.getIntAt(address1)));
						System.err.print(Long.toHexString(address2) + " - ");
						System.err.println(Integer.toHexString(as
								.getIntAt(address2)));
						System.err.print(Long.toHexString(address3) + " - ");

						if (0xf971ffd == address3) {
							System.err.println("Found!");
						}

						System.err.println(Integer.toHexString(as
								.getIntAt(address3)));
					}
				} catch (MemoryFault ex) {
					ex.printStackTrace();
				}
			}
		}
	}

	private static List<Range> mergeRanges(List<Range> localRanges)
	{
		List<Range> newRanges = new LinkedList<Range>();

		Iterator<Range> it = localRanges.iterator();

		Range current = it.next();

		while (it.hasNext()) {
			Range thisRange = it.next();

			if (current.base + current.size == thisRange.base) {
				// Amalgamate
				current.size += thisRange.size;
			} else {
				newRanges.add(current);
				current = thisRange;
			}
		}

		if (current != null) {
			newRanges.add(current);
		}

		return newRanges;
	}

}
