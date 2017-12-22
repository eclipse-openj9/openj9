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
package com.ibm.j9ddr.vm29.j9.walkers;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Iterator;
import java.util.NoSuchElementException;

import com.ibm.j9ddr.corereaders.memory.Addresses;
import com.ibm.j9ddr.vm29.events.EventManager;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MemTagPointer;
import com.ibm.j9ddr.vm29.types.IDATA;
import com.ibm.j9ddr.vm29.types.UDATA;

import static com.ibm.j9ddr.vm29.structure.J9PortLibrary.*;
import static com.ibm.j9ddr.vm29.pointer.helper.J9MemTagHelper.*;

/**
 * Iterator encapsulating the knowledge of how to search for J9-malloc'd sections in 
 * memory.
 * 
 * Iterates over header tags. Use J9MemTagHelper to get from headers to footers and vice-versa.
 *
 */
public class J9MemTagIterator implements Iterator<J9MemTagPointer>
{
	private final long topAddress;
	private final byte[] headerEyecatcherBytes;
	private final long headerEyecatcher;
	private final long footerEyecatcher;
	private J9MemTagPointer current;
	private long currentSearchAddress;
	private boolean isFooterCorrupted;
	private boolean lookingForFreedCallSites;
	
	private J9MemTagIterator(long baseAddress, long topAddress,
			long headerEyecatcher, long footerEyecatcher)
	{
		this(baseAddress, topAddress, headerEyecatcher, footerEyecatcher, false);
	}


	private J9MemTagIterator(long baseAddress, long topAddress,
			long headerEyecatcher, long footerEyecatcher, boolean lookingForFreedCallSites)
	{
		this.topAddress = topAddress;
		this.headerEyecatcherBytes = longToByteString(headerEyecatcher);
		this.headerEyecatcher = headerEyecatcher;
		this.footerEyecatcher = footerEyecatcher;
		this.currentSearchAddress = baseAddress;
		this.isFooterCorrupted = false;
		this.lookingForFreedCallSites = lookingForFreedCallSites;
	}

	private byte[] longToByteString(long input)
	{
		/* Write long out in appropriate byte order */
		ByteBuffer b = ByteBuffer.allocate(8);
		b.order(DataType.getProcess().getByteOrder());
		b.putLong(input);
		b.flip();
		
		/* Eyecatcher is 4 bytes - need to copy it out of the appropriate end of the bytebuffer buffer */
		byte[] toReturn = new byte[4];
		if (b.order() == ByteOrder.BIG_ENDIAN) {
			b.position(4);
		}
		b.get(toReturn, 0, 4);
		
		return toReturn;
	}

	public boolean hasNext()
	{
		if (current != null) {
			return true;
		} else {
			current = internalNext();
			return current != null;
		}
	}
	
	public void moveCurrentSearchAddress(long jumpSize) {
		/*
		 * By default, currentSearchAddress is moved beyond the current address by internalNext() once it is called.
		 * UDATA.SIZEOF needs to be subtracted from currentSearchAddress to be at current searched address which next() call returned. 
		 * 
		 * If jump size is not bigger than UDATA.SIZEOF, 
		 * skip this step since it moves current search address backward, where it should always go forward
		 */
		if (jumpSize > UDATA.SIZEOF) {
			this.currentSearchAddress += jumpSize - UDATA.SIZEOF;
		}
	}
	
	public boolean isFooterCorrupted() {
		return this.isFooterCorrupted;
	}

	private J9MemTagPointer internalNext()
	{
		long result;
		this.isFooterCorrupted = false;
		
		/* Hunt for the header eyecatcher */
		do {
			result = DataType.getProcess().findPattern(headerEyecatcherBytes, UDATA.SIZEOF, currentSearchAddress);
			
			if (result == -1) {
				return null;
			}
			
			if (Addresses.greaterThan(result,topAddress)) {
				return null;
			}
			
			/* Move the current search address beyond the current result */
			currentSearchAddress = result + UDATA.SIZEOF;
			
			/* Address is in range - does it point to a valid block? */
			VoidPointer memoryBase = j9mem_get_memory_base(J9MemTagPointer.cast(result));
			
			try {
				IDATA corruption = j9mem_check_tags(VoidPointer.cast(memoryBase), headerEyecatcher, footerEyecatcher);
				
				if (corruption.allBitsIn(J9PORT_MEMTAG_NOT_A_TAG.longValue()) ) {
					/* This is not a memory tag */
					continue;
				}
				if (corruption.allBitsIn(J9PORT_MEMTAG_FOOTER_TAG_CORRUPTED.longValue())
						|| corruption.allBitsIn(J9PORT_MEMTAG_FOOTER_PADDING_CORRUPTED.longValue())) {
					/*
					 * A block with corrupted footer is accepted only and only if 
					 * we are dealing with freed memory. Therefore, following check is double check.
					 * If we are here, then it is freed memory is being looked for with the current algorithm. 
					 *	
					 */
					if (headerEyecatcher == J9MEMTAG_EYECATCHER_FREED_HEADER 
						&& footerEyecatcher == J9MEMTAG_EYECATCHER_FREED_FOOTER) {
						this.isFooterCorrupted = true;
						return J9MemTagPointer.cast(result);
					}
				}						
			} catch (J9MemTagCheckError e) {
				/* Tag is readable, but corrupt */
				if (!lookingForFreedCallSites) {
					EventManager.raiseCorruptDataEvent("Corrupt memory section found", e, false);
				}
				/* Find the next section */
				continue;
			}
			
			return J9MemTagPointer.cast(result);
		} while (true);
	}

	public J9MemTagPointer next()
	{
		if (hasNext()) {
			J9MemTagPointer toReturn = current;
			current = null;
			return toReturn;
		} else {
			throw new NoSuchElementException();
		}
	}

	public void remove()
	{
		throw new UnsupportedOperationException();
	}

	public static J9MemTagIterator iterateHeaders(long baseAddress, long topAddress, long headerEyecatcher, long footerEyecatcher)
	{
		return new J9MemTagIterator(baseAddress, topAddress, headerEyecatcher, footerEyecatcher);
	}
	
	public static J9MemTagIterator iterateHeaders(long baseAddress, long topAddress, long headerEyecatcher, long footerEyecatcher, boolean lookingForFreedCallSites)
	{
		return new J9MemTagIterator(baseAddress, topAddress, headerEyecatcher, footerEyecatcher, lookingForFreedCallSites);
	}

	public static J9MemTagIterator iterateAllocatedHeaders(long baseAddress, long topAddress)
	{
		return iterateHeaders(baseAddress, topAddress, J9MEMTAG_EYECATCHER_ALLOC_HEADER, J9MEMTAG_EYECATCHER_ALLOC_FOOTER);
	}
	
	public static J9MemTagIterator iterateAllocatedHeaders()
	{
		return iterateAllocatedHeaders(0, UDATA.MASK);
	}
	
	public static J9MemTagIterator iterateFreedHeaders()
	{
		return iterateFreedHeaders(0, UDATA.MASK);
	}

	public static J9MemTagIterator iterateFreedHeaders(long baseAddress)
	{
		return iterateFreedHeaders(baseAddress, UDATA.MASK);
	}

	public static J9MemTagIterator iterateFreedHeaders(long baseAddress, long topAddress)
	{
		return iterateHeaders(baseAddress, topAddress, J9MEMTAG_EYECATCHER_FREED_HEADER, J9MEMTAG_EYECATCHER_FREED_FOOTER, true);
	}

}
