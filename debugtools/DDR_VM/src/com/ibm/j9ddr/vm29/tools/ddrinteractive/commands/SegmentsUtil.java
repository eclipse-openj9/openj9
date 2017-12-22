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
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.walkers.MemorySegmentIterator;
import com.ibm.j9ddr.vm29.pointer.UDATAPointer;
import com.ibm.j9ddr.vm29.pointer.generated.*;
import com.ibm.j9ddr.vm29.structure.J9MemorySegment;
import com.ibm.j9ddr.vm29.types.UDATA;

public class SegmentsUtil {
	private static final String nl = System.getProperty("line.separator");
	
	public static void dbgDumpSegmentList(PrintStream out, J9MemorySegmentListPointer segmentListPointer) throws CorruptDataException {
		String fmt = null;
		if (J9BuildFlags.env_data64) {
			out.append("+----------------+----------------+----------------+----------------+--------+--------+\n");
			out.append("|    segment     |     start      |     alloc      |      end       |  type  |  size  |\n");
			out.append("+----------------+----------------+----------------+----------------+--------+--------+\n");
			fmt = " %016x %016x %016x %016x %08x %8x";
		} else {
			out.append("+--------+--------+--------+--------+--------+--------+\n");
			out.append("|segment | start  | alloc  |  end   |  type  |  size  |\n");
			out.append("+--------+--------+--------+--------+--------+--------+\n");
			fmt = " %08x %08x %08x %08x %08x %8x";
		}
		MemorySegmentIterator segmentIterator = new MemorySegmentIterator(segmentListPointer, MemorySegmentIterator.MEMORY_ALL_TYPES, false);

		long totalMemory = 0L, totalMemoryInUse = 0L;
		while (segmentIterator.hasNext()) {
			J9MemorySegmentPointer seg = (J9MemorySegmentPointer) segmentIterator.next();

			totalMemory += seg.size().longValue();
			totalMemoryInUse += seg.heapAlloc().sub(seg.heapBase()).longValue();
			
			String msg = String.format(fmt, seg.getAddress(), seg.heapBase().getAddress(), seg.heapAlloc().getAddress(), seg.heapTop().getAddress()
					, seg.type().longValue(), seg.size().longValue());
			out.append(msg);
			out.append(nl);
			seg = segmentListPointer.nextSegment();
		}
		
		if (J9BuildFlags.env_data64) {
			out.append("+----------------+----------------+----------------+----------------+--------+--------+\n");
		} else {
			out.append("+--------+--------+--------+--------+--------+--------+\n");
		}
		printMemoryUsed(out, totalMemory, totalMemoryInUse);
		out.append(nl);
	}
	
	public static void dbgDumpJITCodeSegmentList(PrintStream out, J9MemorySegmentListPointer segmentListPointer) throws CorruptDataException {
		String fmt = null;
		if (J9BuildFlags.env_data64) {
			out.append("+----------------+----------------+----------------+----------------+----------------+--------+\n");
			out.append("|    segment     |     start      |    warmAlloc   |    coldAlloc   |      end       |  size  |\n");
			out.append("+----------------+----------------+----------------+----------------+----------------+--------+\n");
			fmt = " %016x %016x %016x %016x %016x %8x";
		} else {
			out.append("+--------+--------+--------+--------+--------+--------+\n");
			out.append("|segment | start  |  warm  |  cold  |  end   |  size  |\n");
			out.append("+--------+--------+--------+--------+--------+--------+\n");
			fmt = " %08x %08x %08x %08x %08x %8x";
		}
		MemorySegmentIterator segmentIterator = new MemorySegmentIterator(segmentListPointer, MemorySegmentIterator.MEMORY_ALL_TYPES, false);

		long totalMemory = 0L, totalMemoryInUse = 0L;
		while (segmentIterator.hasNext()) {
			J9MemorySegmentPointer seg = (J9MemorySegmentPointer) segmentIterator.next();

			final UDATA heapBase = UDATAPointer.cast(seg.heapBase()).at(0);
			long warmAlloc = UDATAPointer.cast(heapBase).at(0).longValue();
			long coldAlloc = UDATAPointer.cast(heapBase.add(UDATA.SIZEOF)).at(0).longValue();

			totalMemory += seg.size().longValue();
			totalMemoryInUse += (warmAlloc - seg.heapBase().longValue()) + 
								(seg.heapTop().longValue() - coldAlloc);
			
			String msg = String.format(fmt, seg.getAddress(), seg.heapBase().getAddress(), warmAlloc, coldAlloc, seg.heapTop().getAddress()
					, seg.size().longValue());
			out.append(msg);
			out.append(nl);
			seg = segmentListPointer.nextSegment();
		}

		if (J9BuildFlags.env_data64) {
			out.append("+----------------+----------------+----------------+----------------+----------------+--------+\n");
		} else {
			out.append("+--------+--------+--------+--------+--------+--------+\n");
		}
		printMemoryUsed(out, totalMemory, totalMemoryInUse);
		out.append(nl);
	}
	
	private static void printMemoryUsed(PrintStream out, long totalMemory, long totalMemoryInUse) {
		long totalMemoryFree = totalMemory - totalMemoryInUse;
		out.println(String.format("Total memory:           %016d (%016x)", totalMemory, totalMemory));
		out.println(String.format("Total memory in use:    %016d (%016x)", totalMemoryInUse, totalMemoryInUse));
		out.println(String.format("Total memory free:      %016d (%016x)", totalMemoryFree, totalMemoryFree));
	}

	private static class SegmentSortException extends RuntimeException {
		SegmentSortException(String msg, CorruptDataException cause) {
			super(msg, cause);
		}

		public CorruptDataException getCause() {
			return (CorruptDataException)getCause();
		}
	}

	private static class SegmentComparator implements Comparator<J9MemorySegmentPointer> {
		/**
		 * Sort memory segments by ascending heapBase(). If heapBase() are equal, sort by ascending heapTop().
		 * If both heapBase() and heapTop() are equal, the segments are equal.
		 * @param s1 a J9MemorySegment
		 * @param s2 a J9MemorySegment
		 * @return -1 if s1 < s2, 0 if s1 == s2, 1 if s1 > s2.
		 */
		public int compare(J9MemorySegmentPointer s1, J9MemorySegmentPointer s2) {
			try {
				if (s1.getAddress() == s2.getAddress()) {
					/* duplicate segment entries */
					return 0;
				} else if (s1.heapBase().getAddress() < s2.heapBase().getAddress()) {
					return -1;
				} else if (s1.heapBase().getAddress() > s2.heapBase().getAddress()) {
					return 1;
				} else {
					/* heapBases are equal */
					if (s1.heapTop().getAddress() < s2.heapTop().getAddress()) {
						return -1;
					} else if (s1.heapTop().getAddress() > s2.heapTop().getAddress()) {
						return 1;
					} else {
						return 0;
					}
				}
			} catch (CorruptDataException e) {
				throw new SegmentSortException("Failed to sort memory segments", e);
			}
		}
	}

	private static void addSegmentsToArrayList(J9MemorySegmentListPointer segmentListPointer, ArrayList<J9MemorySegmentPointer> segmentArray, int segmentType) 
			throws CorruptDataException
	{
		MemorySegmentIterator segmentIterator = new MemorySegmentIterator(segmentListPointer, segmentType, false);

		while (segmentIterator.hasNext()) {
			J9MemorySegmentPointer seg = (J9MemorySegmentPointer)segmentIterator.next();
			segmentArray.add(seg);
		}
	}

	private static J9MemorySegmentPointer[] getSortedSegments(J9JavaVMPointer vm, int segmentType) throws CorruptDataException {
		ArrayList<J9MemorySegmentPointer> segmentArray = new ArrayList<J9MemorySegmentPointer>();

		try {
			addSegmentsToArrayList(vm.memorySegments(), segmentArray, segmentType);
			addSegmentsToArrayList(vm.classMemorySegments(), segmentArray, segmentType);

			if (J9BuildFlags.interp_nativeSupport) {
				/* readJavaVM also reads converts the JITConfig pointer. */
				if (!vm.jitConfig().isNull()) {
					addSegmentsToArrayList(vm.jitConfig().codeCacheList(), segmentArray, segmentType);
					addSegmentsToArrayList(vm.jitConfig().dataCacheList(), segmentArray, segmentType);
				}
			}
		} catch (SegmentSortException e) {
			throw (CorruptDataException)e.getCause();
		}

		J9MemorySegmentPointer[] sortedSegmentArray = segmentArray.toArray(new J9MemorySegmentPointer[segmentArray.size()]);
		Arrays.sort(sortedSegmentArray, new SegmentComparator());
		return sortedSegmentArray;
	}

	/**
	 * Find overlapping segments
	 * @param sortedSegmentArray sorted array of J9MemorySegments
	 * @param overlaps sorted array of overlap indicators, corresponding to sortedSegmentArray entries
	 * @return true if overlap(s) were found, false if not
	 */
	private static boolean findOverlappingSegments(J9MemorySegmentPointer[] sortedSegmentArray, boolean[] overlaps)
			throws CorruptDataException
	{
		if (sortedSegmentArray.length > 1) {
			boolean foundOverlaps = false;
			long heapTop = sortedSegmentArray[0].heapTop().getAddress();
			overlaps[0] = false;

			for (int i = 1; i < sortedSegmentArray.length; i++) {
				if (sortedSegmentArray[i].heapBase().getAddress() >= heapTop) {
					/* new disjoint range */
					heapTop = sortedSegmentArray[i].heapTop().getAddress();
					overlaps[i] = false;
				} else {
					/* overlaps existing range */
					foundOverlaps = true;
					overlaps[i - 1] = true;
					overlaps[i] = true;

					if (sortedSegmentArray[i].heapTop().getAddress() > heapTop) {
						/* extend range to cover this segment */
						heapTop = sortedSegmentArray[i].heapTop().getAddress();
					}
				}
			}
			return foundOverlaps;
		}
		return false;
	}

	private static String fmtSegment(String fmt, J9MemorySegmentPointer seg) throws CorruptDataException {
		String msg = String.format(fmt, seg.getAddress(), seg.heapBase().getAddress(), seg.heapAlloc().getAddress(),
				seg.heapTop().getAddress(), seg.type().longValue(), seg.size().longValue());
		return msg;
	}

	/**
	 * Print out overlapping J9MemorySegments
	 * @param out output stream
	 * @param sortedSegmentArray sorted array of J9MemorySegments
	 * @param overlaps sorted array of overlap indicators
	 */
	private static void printOverlappingSegments(PrintStream out, J9MemorySegmentPointer[] sortedSegmentArray,
			boolean[] overlaps) throws CorruptDataException
	{
		out.append("Overlapping segments:");
		out.append(nl);
		String fmt;
		if (J9BuildFlags.env_data64) {
			out.append("+----------------+----------------+----------------+----------------+--------+--------+\n");
			out.append("|    segment     |     start      |     alloc      |      end       |  type  |  size  |\n");
			out.append("+----------------+----------------+----------------+----------------+--------+--------+\n");
			fmt = " %016x %016x %016x %016x %08x %8x";
		} else {
			out.append("+--------+--------+--------+--------+--------+--------+\n");
			out.append("|segment | start  | alloc  |  end   |  type  |  size  |\n");
			out.append("+--------+--------+--------+--------+--------+--------+\n");
			fmt = " %08x %08x %08x %08x %08x %8x";
		}
		out.append(nl);

		for (int i = 0; i < overlaps.length; i++) {
			if (overlaps[i]) {
				out.append(fmtSegment(fmt, sortedSegmentArray[i]));
				out.append(nl);
			}
		}
	}

	/**
	 * Find and print overlapping memory segments
	 * @param out Output stream
	 * @param vm a J9JavaVMPointer
	 * @param segmentType Limit search to these types of segments
	 */
	public static void checkSegmentsForOverlap(PrintStream out, J9JavaVMPointer vm, int segmentType) throws CorruptDataException {
		boolean foundOverlaps = false;
		J9MemorySegmentPointer[] sortedSegmentArray = getSortedSegments(vm, segmentType);
		if ((sortedSegmentArray != null) && (sortedSegmentArray.length > 1)) {
			boolean[] overlaps = new boolean[sortedSegmentArray.length];
			if (findOverlappingSegments(sortedSegmentArray, overlaps)) {
				printOverlappingSegments(out, sortedSegmentArray, overlaps);
				if (0 != (segmentType & J9MemorySegment.MEMORY_TYPE_SHARED_META)) {
					out.append(nl);
					String msg = String.format(
							"**NOTE** If -Xshareclasses is enabled, then the shared cache's metadata segment (type %08x)"
							+ nl
							+ "is expected to overlap with ROM class segments (type %08x) in the cache.",
							J9MemorySegment.MEMORY_TYPE_SHARED_META,
							J9MemorySegment.MEMORY_TYPE_ROM_CLASS);
					out.append(msg);
					out.append(nl);
				}
				foundOverlaps = true;
			}
		}
		if (!foundOverlaps) {
			out.append("No overlaps found" + nl);
		}
	}
}
