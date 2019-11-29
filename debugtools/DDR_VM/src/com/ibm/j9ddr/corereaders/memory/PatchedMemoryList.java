/*******************************************************************************
 * Copyright (c) 2004, 2019 IBM Corp. and others
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

import java.io.PrintStream;
import java.util.ArrayList;
import java.util.List;

/**
 * This class statically maintains a list of memory records are in ascending order, meaning the end address of the previous record is 
 * always less than the start address of the next address.  If the end address of the previous record is equal to the start
 * address of the next address, then two records will be merged to one single record.
 * </P>
 * This class provides two major static functions:
 * (1) Add a memory record as a patch.  Note, the memory content in the new memory record will overwrite the memory content in the existing list
 * if they are for the same address.
 * (2) Patch the memory from a specified area. Note, the result is a list of memory records.  If a memory record has only the start address and the length,
 * but the content of the memory record is null, then the memory record is not patched.  Otherwise, the memory record has been patched.
 * <p>
 * @author Manqing LI, IBM Canada
 *
 */
public class PatchedMemoryList {
	public static boolean verbose = new Boolean(System.getProperty("com.ibm.j9ddr.corereaders.memory.PatchedMemoryList.verbose"));
	public static List<MemoryRecord> list = null;  // a list with start address in ascending order; no memory content between two elements overlaps.

	/**
	 * Record a new memory record.
	 * <p>
	 * @param important	The new memory record whose content will be used to overwrite the existing content in the list.
	 */
	public static void record(MemoryRecord important) {
		addAndMergeIfNecessary(important);
		if (verbose) {
			PatchedMemoryList.print(System.out);
		}
	}

	/**
	 * Add the new record to the list and merge if necessary.
	 * <p>
	 * @param important	The new memory record whose content will be used to overwrite the existing content in the list.
	 */
	private static void addAndMergeIfNecessary(MemoryRecord important) {
		if (list == null) {
			list = new ArrayList<MemoryRecord>();
		}
		for (int index = 0; index < list.size(); index++) {
			if (important.getStart() > list.get(index).getEnd()) {
				continue; // the new one is separately higher than the current one, go to the next one.
			} else if (important.getEnd() < list.get(index).getStart()){
				list.add(index, important); // the new one is separately lower than the current one, add ahead of it.
				return;
			} else if (important.getEnd() == list.get(index).getStart()) { // the new one is right before the current one, prefix it. 
				prefixRecordAtIndexWithNewRecord(index, important);
				return;
			} else if (important.getStart() == list.get(index).getEnd()) { // the new one is right after the current one, suffix it.
				suffixRecordAtIndexWithNewRecord(index, important);
				mergeRecordAtIndexWithTheLessImportantNextOneIfNecessary(index);
				return;
			} else {	// Memory overlapped.
				mergeRecordAtIndexWithOverlappedAndMoreImportantNewRecord(index, important);
				return;
			}
		}
		list.add(important);
	}
	
	/**
	 * Assumes the new record shares memory with the record at the specific index, we need overwrite the content in the list
	 * with the ones in the new record.
	 * <p>
	 * @param index			The index of the memory record in the list.
	 * @param important		The new memory record.
	 */
	private static void mergeRecordAtIndexWithOverlappedAndMoreImportantNewRecord(int index, MemoryRecord important) {
		long start = Math.min(list.get(index).getStart(), important.getStart());
		long end = Math.max(list.get(index).getEnd(), important.getEnd());

		byte [] content = new byte [(int)(end - start)];
		
		int cursor = 0;
		if (list.get(index).getStart() < important.getStart()) {	// If the record at index starts earlier than the new one.
																	// copy the leading content over.
			
			cursor = (int)(important.getStart() - list.get(index).getStart());
			System.arraycopy(list.get(index).getContent(), 0, content, 0, cursor);
		}
		
		// Copy the content in the new record over.
		System.arraycopy(important.getContent(), 0, content, cursor, important.getLength());
		cursor = cursor + important.getLength();
		
		if (list.get(index).getEnd() > important.getEnd()) {		// If the record at index ends later than the new one.
			System.arraycopy(list.get(index).getContent(), 			// copy the tail of the record at index over.
					(int)(important.getEnd() - list.get(index).getStart()), 
					content, 
					cursor, 
					(int)(list.get(index).getEnd() - important.getEnd()));
		}
		
		list.get(index).setStart(start);
		list.get(index).setContent(content);
		
		mergeRecordAtIndexWithTheLessImportantNextOneIfNecessary(index);
		return;		
	}
	
	/**
	 * Put the content of the new record to the beginning of the record at index.
	 * <p>
	 * @param index		The index of the record in the list.
	 * @param record	The new record.
	 */
	private static void prefixRecordAtIndexWithNewRecord(int index, MemoryRecord record) {
		byte [] content = new byte [record.getLength() + list.get(index).getLength()];
		System.arraycopy(record.getContent(),0, content, 0, record.getLength());
		System.arraycopy(list.get(index).getContent(), 0, content, record.getLength(), list.get(index).getLength());
		list.get(index).setStart(record.getStart());
		list.get(index).setContent(content);
	}
	
	/**
	 * Put the content of the new record to the end of the record at index.
	 * Note, this method does not check the next record in the list.
	 * <p>
	 * @param index		The index of the record in the list.
	 * @param record	The new record.
	 */
	private static void suffixRecordAtIndexWithNewRecord(int index, MemoryRecord record) {
		byte [] content = new byte [list.get(index).getLength() + record.getLength()];
		System.arraycopy(list.get(index).getContent(), 0, content, 0, list.get(index).getLength());
		System.arraycopy(record.getContent(), 0, content, list.get(index).getLength(), record.getLength());
		list.get(index).setContent(content);
	}
	
	/**
	 * Merge the record at index with the next one.  Note, if the record at index overlaps with the content of the next one,
	 * the one at the index would be used for the overlapped area because the record at the index comes from the new patch.
	 * Note, the method will merge the next one if necessary.
	 * <p>
	 * @param index		The index of the record in the list.
	 */
	private static void mergeRecordAtIndexWithTheLessImportantNextOneIfNecessary(int index) {
		if (index + 1 == list.size()) {
			return; // The one at index is the last one. No more element to merge.
		}

		if (list.get(index).getEnd() < list.get(index + 1).getStart()) { // disconnected, no more merge required.
			return;
		} 
		
		if (list.get(index).getEnd() == list.get(index + 1).getStart()) { // connected.
			suffixRecordAtIndexWithNewRecord(index, list.get(index + 1));
			list.remove(index + 1);
			mergeRecordAtIndexWithTheLessImportantNextOneIfNecessary(index);
			return;
		}
		
		// Overlapped.
		mergeRecordAtIndexWithTheOverlappedAndLessImportantNextOne(index);
		return;
	}
	
	/**
	 * Assuming the record at index overlaps the next one, we want merge them.
	 * Note, this method will check if further merging is necessary.
	 * <p>
	 * @param index		The index of the current record in the list.
	 */
	private static void mergeRecordAtIndexWithTheOverlappedAndLessImportantNextOne(int index) {

		// Note we know the start address of the current record is lower than the next one.
		// So we do not need to reset the start address of the record.
		
		if (list.get(index).getEnd() > list.get(index + 1).getEnd()) { 	// If the next one is completely covered by the current one.
			list.remove(index + 1);										// simply discard the next one
			mergeRecordAtIndexWithTheOverlappedAndLessImportantNextOne(index);	// and continue merging.
			return;
		}
		
		// Now the next one has a tail not covered by the current one.
		byte [] content = new byte [(int)(list.get(index + 1).getEnd() - list.get(index).getEnd())];
		System.arraycopy(list.get(index), 0, content, 0, list.get(index).getLength());
		
		if (list.get(index + 1).getEnd() > list.get(index).getEnd()) { // copy the tail content of the next one over.
			System.arraycopy(
					list.get(index + 1), 
					(int)(list.get(index).getEnd() - list.get(index + 1).getStart()), 
					content, 
					list.get(index).getLength(), 
					(int)(list.get(index + 1).getEnd() - list.get(index).getEnd()));
		}
		
		list.get(index).setContent(content);
		list.remove(index + 1);
	}

	/**
	 * Patches the memory at address
	 * <p>
	 * @param address	The starting address of the memory to be apply patch on.
	 * @param length	How many bytes to be patched.
	 * <p>
	 * @return	A list of memory records. Some memory records might not be initialized
	 * 			because those memory contents are not available in the patch.
	 */
	public static List<MemoryRecord> getPatched(long address, int length) {
		List<MemoryRecord> container = new ArrayList<>();
		
		if (list == null || length <= 0) {
			return container;
		}
		
		getPatchedFromIndex(0, address, length, container);
		
		if (verbose) {
			System.out.print("Patched address 0x" + Long.toHexString(address) + " length " + length);
			for (int i = 0; i < container.size(); i++) {
				container.get(i).print(System.out, i + " : ");
			}
		}
		
		return container;
	}

	/**
	 * Starting at patched memory at index, patch the memory starts at the specific address for the specific length.
	 * <p>
	 * @param index		The index of the patched memory at the list.
	 * @param address	The starting address of the memory to be apply patch on.
	 * @param length	How many bytes to be patched.
	 * @param container	A list of memory records. Some memory records might not be initialized
	 * 			because those memory contents are not available in the patch.
	 */
	private static void getPatchedFromIndex(int index, long address, int length, List<MemoryRecord> container) {
		
		if (list == null) {
			return;
		}
		
		if (index == list.size()) {
			if (length > 0) {
				container.add(new MemoryRecord(address, length)); // no more patches in the list.
			}
			return;
		}
		
		MemoryRecord record = list.get(index);

		if (address + length <= record.getStart()) { // The end address is before the start of the current one.
			container.add(new MemoryRecord(address, length)); // add this one and no more patches to match.
			return;
		}

		if (address >= record.getEnd()) { // The start address is higher than the end of the current patch, move to the next one.
			getPatchedFromIndex(index + 1, address, length, container);
			return;
		}				
		
		// Now, we have overlap and at least some memory needs to be patched.

		if (address < record.getStart()) {
			container.add(new MemoryRecord(address, (int)(record.getStart() - address)));
			length = length - (int)(record.getStart() - address);
			address = record.getStart();
		}

		byte [] buffer = new byte [(int)((Math.min(address + length, record.getEnd())) - address)];
		System.arraycopy(record.getContent(), (int)(address - record.getStart()), buffer, 0, buffer.length);
		container.add(new MemoryRecord(address, buffer));
		length = length - buffer.length;
		address = Math.min(address + length, record.getEnd());
		
		if (address + length > record.getEnd()) {
			getPatchedFromIndex(index + 1, address, length, container);
		}
	}
	
	/**
	 * Check if an address range is patched.
	 * <p>
	 * @param address	The start address of the range.
	 * @param length	The length of the range.
	 * <p>
	 * @return		Returns true if the address range is patched; false otherwise.
	 */
	public static boolean isPatched(long address, int length) {

		if (list == null || list.size() == 0 || length <= 0) {
			return false;
		}
		for (MemoryRecord mr : list) {
			if(mr.getEnd() <= address || mr.getStart() >= address + length ) {
				continue;
			} else {
				return true;
			}
		}		
		return false;
	}
	
	/**
	 * Clear all patched memory records.
	 */
	public static void clearAll() {
		list = null;
	}
	
	/**
	 * Clear memory area from the patch.
	 * <p>
	 * @param start		The start address of the range.
	 * @param length	The end of the range.
	 */
	public static void clear(long start, int length) {
		if (list == null || length <= 0) {
			return;
		}
		
		for (int index = list.size() - 1; index >= 0; index--) {
			
			if (start >= list.get(index).getEnd()) {				// If the memory to be cleared falls after the current entry
				break;												// no more actions
			}

			if (start + length <= list.get(index).getStart()) {  	// If the memory to be cleared falls before the current entry
				continue;											// continue to the previous one.												
			}
			
			MemoryRecord record = list.remove(index);	// Now the memory record at index overlaps with the specific memory range.

			if (record.getEnd() > start + length) {	// keep the tailing patches if there any. 
				long newStart = start + length;
				byte [] content = new byte [(int)(record.getEnd() - (start + length))];
				System.arraycopy(record.getContent(), (int)(start + length - record.getStart()), content, 0, content.length);
				list.add(index, new MemoryRecord(newStart, content));
			} 
			
			if (record.getStart() < start) {	// keep the leading patch if there is any.
				byte [] content = new byte [(int)(start - record.getStart())];
				System.arraycopy(record.getContent(), 0, content, 0, content.length);
				list.add(index, new MemoryRecord(record.getStart(), content));
			}
		}
		
		if (list.isEmpty()) {
			list = null;
		}
	}
	
	/**
	 * Print the patch memory contents.
	 * <p>
	 * @param out	The output stream object.
	 */
	public static void print(PrintStream out) {
		if (list == null || list.size() == 0) {
			out.println("No memory is currently patched.");
			return;
		}
		
		if (list.size() == 1) {
			out.println("The following memory area is currently patched:");
		} else {
			out.println("The following memory areas are currently patched:");
		}
		for (int i = 0; i < list.size(); i++) {
			list.get(i).print(out, i + ": ");
		}
		out.println();
	}
}

