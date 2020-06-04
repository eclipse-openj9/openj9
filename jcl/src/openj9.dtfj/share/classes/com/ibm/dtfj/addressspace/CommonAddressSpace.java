/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2020 IBM Corp. and others
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
package com.ibm.dtfj.addressspace;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Arrays;
import java.util.Comparator;
import java.util.Iterator;

import com.ibm.dtfj.corereaders.MemoryAccessException;
import com.ibm.dtfj.corereaders.MemoryRange;

/**
 * @author jmdisher
 * Contains the common functionality shared by the current address space implementations
 */
public abstract class CommonAddressSpace implements IAbstractAddressSpace {
	private static final int BUFFER_MAX = 4096;
	private static final int MEMORY_CHECK_THRESHOLD = 0x100000;
	
	private MemoryRange[] _translations;
	private Integer _lastTranslationUsed = Integer.valueOf(0);
	private boolean _isLittleEndian;
	private boolean _is64Bit;
	private int lastAsid;
	private int _is64BitAsid[];
	
	protected CommonAddressSpace(MemoryRange[] translations, boolean isLittleEndian, boolean is64Bit)
	{
		_translations = _sortRanges(translations);
		_isLittleEndian = isLittleEndian;
		_is64Bit = is64Bit;
		set64Bitness();
	}
	
	private static MemoryRange[] _sortRanges(MemoryRange[] ranges)
	{
		Arrays.sort(ranges, new Comparator<MemoryRange>() {
			@Override
			public int compare(MemoryRange one, MemoryRange two)
			{
				return compareAddress(one.getAsid(), one.getVirtualAddress(), two.getAsid(), two.getVirtualAddress()); 
			}
		});
		return ranges;
	}

	/**
	 * Walk through all the address ranges (presumed sorted by asid, address)
	 * Add ASID to array if any address >= 0x100000000 i.e. 64-bit
	 */
	private void set64Bitness() {
		// Promise of no 64-bit addresses
		if (!_is64Bit) return;
		int count = 0;
		for (int i = 0; i < _translations.length; ++i) {
			if (_translations[i].getVirtualAddress()+_translations[i].getSize() >= 0x100000000L) {
				// Found 64-bit entry
				int asid = _translations[i].getAsid();
				
				// Remember the ASID
				int newa[] = new int[count+1];
				for (int j = 0; j < count; ++j) {
					newa[j] = _is64BitAsid[j];
				}
				newa[count] = asid;
				_is64BitAsid = newa;
				
				++count;
				
				// Skip to the end of the ASID range
				while (i < _translations.length && _translations[i].getAsid() == asid) ++i;
			}
		}
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.addressspace.IAbstractAddressSpace#getMemoryRanges()
	 */
	public Iterator getMemoryRanges()
	{
		return Arrays.asList(_translations).iterator();
	}

	protected MemoryRange _residentRange(int asid, long address) throws MemoryAccessException
	{
		int range = findWhichMemoryRange(asid, address, _translations, _lastTranslationUsed, true);
		
		if (range >= 0) {
			return _translations[range];
		} else {
			throw new MemoryAccessException(asid, address);
		}
	}
	
	/**
	 * 
	 * This searches memory ranges for an addr using a binary chop and returns an int indicating the memory range
	 *  or -1 if address is not within any memory range...... 
	 * @param asid TODO
	 */
	protected static int findWhichMemoryRange(int asid, long addr, MemoryRange[] ranges, Integer lastRange, boolean doLinearIfNotFound)
	{
		int retI=-1; //assume we won't find it
		int last = lastRange.intValue();
		int lastPlusOne = last+1;
		
		if ((last < ranges.length) && 
			(ranges[last].contains(asid, addr)) && 
			(ranges[last].isInCoreFile() || (!ranges[last].isInCoreFile() && ranges[last].getLibraryReader() != null))) { //TLB hit
			retI = last;
		} else if ((lastPlusOne < ranges.length) && 
				   (ranges[lastPlusOne].contains(asid, addr)) && 
				   (ranges[last].isInCoreFile() || (!ranges[last].isInCoreFile() && ranges[last].getLibraryReader() != null))) { //TLB hit
			retI = lastPlusOne;
		} else {
			// TLB failed to do the search
			
			// Now effectively do a binary search find the right range....
			// note: on some platforms we can have overlapping memory ranges!!
			//   (ie. AIX, zOs and linux) so we always give back the first

			int highpos = ranges.length-1;
			int lowpos  = 0;
			
			while (lowpos <= highpos) {
				int currentPos = lowpos + ((highpos-lowpos) >>> 1);
				long mraStartAddr = ranges[currentPos].getVirtualAddress();
				int mraAsid = ranges[currentPos].getAsid();

				if (compareAddress(asid, addr, mraAsid, mraStartAddr) >= 0) { 	//addr above base of range
					if(compareAddress(asid, addr, mraAsid, mraStartAddr + ranges[currentPos].getSize()) < 0 && 
					  (ranges[currentPos].isInCoreFile() || !ranges[currentPos].isInCoreFile() && ranges[currentPos].getLibraryReader() != null)) {
						retI = currentPos;
						break;
					}
					lowpos = currentPos + 1;
				} else {
					highpos = currentPos-1;
				}					
			}

			// No match using binary chop, probably due to overlapping ranges => revert to a linear search
			//  [stop at lowpos because this will either be currentPos or currentPos+1, when retI is -1]

			if (-1 == retI && doLinearIfNotFound) {
				for (int i = 0; i < lowpos; i++) {
					long mraStartAddr = ranges[i].getVirtualAddress();
					int mraAsid = ranges[i].getAsid();
					if ((compareAddress(asid, addr, mraAsid, mraStartAddr) >= 0) && 
						(compareAddress(asid, addr, mraAsid, mraStartAddr + ranges[i].getSize()) < 0) && 
						(ranges[i].isInCoreFile() || (!ranges[i].isInCoreFile() && ranges[i].getLibraryReader() != null))) {
						retI = i;
						break;
					}
				}
			}
		}
		if (-1 != retI) {
			lastRange = Integer.valueOf(retI);
		}
		return retI;
	}
	
	protected static int compareAddress(int lasid, long lhs, int rasid, long rhs) {
		if (lasid < rasid) return -1;
		if (lasid > rasid) return 1;
		if (lhs == rhs) {
			return 0;
		} else if ((lhs >= 0 && rhs >= 0) || (lhs < 0 && rhs < 0)) {
			return lhs < rhs ? -1 : 1;
		} else {
			return lhs < rhs ? 1 : -1;
		}
	}
	
	private boolean isMemoryAccessible(int asid, long address, int size)
	{
		// CMVC 125071 - jextract failing with OOM error
		// Scan the memory ranges to determine whether the memory from address
		// to address+size is available to read. If so, return true else return false.
		boolean rc = false;
		long cumulativeSize = 0;
		int startRange;
		long hi;
		long memhi;
		
		startRange = findWhichMemoryRange(asid, address, _translations, _lastTranslationUsed, true);
		if (startRange == -1) {
			// Range not found, so return false
			return rc;
		}
		
		// Now we have a start range, check if the size fits
		hi = _translations[startRange].getVirtualAddress() + _translations[startRange].getSize();
		memhi = address + size;
		if (hi >= memhi) {
			// found a fit, so return true
			rc = true;
		} else {
			// size goes beyond the end of this range, so walk the contiguous
			// ranges to find if it will still fit
			cumulativeSize = hi - address;
			
			for (int i = startRange+1; i < _translations.length; i++) {
				long rangeBaseAddress = _translations[i].getVirtualAddress();
				long rangeSize = _translations[i].getSize();
				long rangeTopAddress = rangeBaseAddress + rangeSize;
				
				if (rangeBaseAddress == hi) {
					// contiguous range
					cumulativeSize += rangeSize;
					if (cumulativeSize >= size) {
						// found a fit, so return true
						rc = true;
						break;
					}
				} else {
					if (rangeBaseAddress < hi) {
						if (rangeTopAddress < hi) {
							// found a region contained within the range of the previous region
							// so just ignore it
							continue;
						} else {
							// found a region that overlaps with the previous region, so accumulate
							// the difference in size.
							cumulativeSize += rangeTopAddress - hi;
							if (cumulativeSize >= size) {
								// found a fit, so return true
								rc = true;
								break;
							}
						}
					} else {
						// non-contiguous, so abort
						break;
					}
				}
				// get the hi address of this range
				hi = rangeTopAddress;
			}
		}
		return rc;
	}

	@Override
	public ByteOrder getByteOrder() {
		return _isLittleEndian ? ByteOrder.LITTLE_ENDIAN : ByteOrder.BIG_ENDIAN;
	}

	@Override
	public byte[] getMemoryBytes(long vaddr, int size)
	{
		return getMemoryBytes(lastAsid, vaddr, size);
	}

	public byte[] getMemoryBytes(int asid, long vaddr, int size)
	{
		// CMVC 125071 - jextract failing with OOM error
		// This check is to help protect from OOM situations where a damaged core
		// has given us a ridiculously large memory size to allocate. If size is 
		// sufficiently large, then check if the memory really exists before trying
		// to allocate it. Obviously, this does not stop OOM occurring, if the memory
		// exists in the core file we may pass the test and still OOM on the allocation.
		boolean getMem = true;
		lastAsid = asid;
		byte[] buffer = null;
		
		if (size < 0) {
			return null;
		}
		
		if (0 != vaddr) {
			if (size > MEMORY_CHECK_THRESHOLD) {
				getMem = isMemoryAccessible(0, vaddr, size);
			}
		
			if (getMem) {
				buffer = new byte[size];

				try {
					getBytesAt(asid, vaddr, buffer);
				} catch (MemoryAccessException e) {
					buffer = null;
				}
			}
		}
		return buffer;
	}

	public long findPattern(byte[] whatBytes, int alignment, long startFrom) {
		int align = 0 == alignment ? 1 : alignment; // avoid divide-by-zero errors

		// Ensure buffer size is a multiple of the alignment
		int bufferMax = BUFFER_MAX;
		if (bufferMax < align)
			bufferMax = align;
		else if (0 != bufferMax % align)
			bufferMax -= bufferMax % align;

		long location = -1;
		Iterator ranges = getMemoryRanges();
		
		while ((-1 == location) && ranges.hasNext()) {
			MemoryRange range = (MemoryRange) ranges.next();
			// Skip over previous asids
			if (range.getAsid() < lastAsid) continue;
			// Reset start location for new asid
			if (range.getAsid() > lastAsid) startFrom = 0;
			location = findPatternInRange(whatBytes, align, startFrom, range, bufferMax);
		}
		// Reset asid as end of search
		if (location == -1) lastAsid = 0;
		return location;
	}
	
	private static int match(byte[] whatBytes, int matchedSoFar, byte[] buffer, int index) {
		int matched = matchedSoFar;
		for (int i = index; i < buffer.length && matched < whatBytes.length; i++, matched++)
			if (buffer[i] != whatBytes[matched])
				return 0;
		return matched;
	}
	
	private long findPatternInRange(byte[] whatBytes, int alignment, long start, MemoryRange range, int bufferMax) {
		if (range.getVirtualAddress() >= start || range.contains(start)) {
			// Ensure the address is within the range and aligned
			long addr = start;
			if (addr < range.getVirtualAddress())
				addr = range.getVirtualAddress();
			if (0 != addr % alignment)
				addr += alignment - (addr % alignment);

			long edge = range.getVirtualAddress() + range.getSize();
			
			int asid = range.getAsid();

			// FIXME this is somewhat inefficient - should use Boyer-Moore
			int matched = 0;
			long matchAddr = -1;
			while (addr < edge) {
				long bytesLeftInRange = edge - addr;
				long count = bufferMax < bytesLeftInRange ? bufferMax : bytesLeftInRange;
				
				if (0 != addr) {
					byte[] buffer = getMemoryBytes(asid, addr, (int)count);
					
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
				} else {
					System.err.println("Looking for address 0");
				}

				addr += count;
			}
		}
		return -1;
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.dtfj.addressspace.IAbstractAddressSpace#getLongAt(int, long)
	 */
	@Override
	public long getLongAt(int asid, long address) throws MemoryAccessException
	{
		byte buffer[] = new byte[8];
		getBytesAt(asid, address, buffer);
		return ByteBuffer.wrap(buffer).order(getByteOrder()).getLong();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.addressspace.IAbstractAddressSpace#getIntAt(int, long)
	 */
	@Override
	public int getIntAt(int asid, long address) throws MemoryAccessException
	{
		byte buffer[] = new byte[4];
		getBytesAt(asid, address, buffer);
		return ByteBuffer.wrap(buffer).order(getByteOrder()).getInt();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.addressspace.IAbstractAddressSpace#getShortAt(int, long)
	 */
	@Override
	public short getShortAt(int asid, long address) throws MemoryAccessException
	{
		byte buffer[] = new byte[2];
		getBytesAt(asid, address, buffer);
		return ByteBuffer.wrap(buffer).order(getByteOrder()).getShort();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.addressspace.IAbstractAddressSpace#getByteAt(int, long)
	 */
	@Override
	public byte getByteAt(int asid, long address) throws MemoryAccessException
	{
		byte buffer[] = new byte[1];
		getBytesAt(asid, address, buffer);
		return buffer[0];
	}

	@Override
	public long getPointerAt(int asid, long address) throws MemoryAccessException
	{
		long ptr;
		if (bytesPerPointer(asid) == 8) {
			ptr = getLongAt(asid, address);
		} else {
			ptr = getIntAt(asid, address) & 0xFFFFFFFFL;
		}
		return ptr;
	}

	@Override
	public abstract int getBytesAt(int asid, long address, byte[] buffer) throws MemoryAccessException;

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.addressspace.IAbstractAddressSpace#bytesPerPointer()
	 */
	@Override
	public int bytesPerPointer(int asid)
	{
		if (_is64Bit && _is64BitAsid != null) {
			// Search the list of 64-bit ASIDs
			for (int i = 0; i < _is64BitAsid.length; ++i) {
				if (_is64BitAsid[i] == asid) {
					return 8;
				}
			}
		}
		return 4;
	}

}
