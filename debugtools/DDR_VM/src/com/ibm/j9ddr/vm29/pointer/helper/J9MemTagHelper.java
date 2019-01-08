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
package com.ibm.j9ddr.vm29.pointer.helper;

import static com.ibm.j9ddr.vm29.structure.J9PortLibrary.J9MEMTAG_EYECATCHER_FREED_FOOTER;
import static com.ibm.j9ddr.vm29.structure.J9PortLibrary.J9MEMTAG_EYECATCHER_FREED_HEADER;

import com.ibm.j9ddr.AddressedCorruptDataException;
import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.U32Pointer;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9MemTagPointer;
import com.ibm.j9ddr.vm29.structure.J9MemTag;
import com.ibm.j9ddr.vm29.structure.J9PortLibrary;
import com.ibm.j9ddr.vm29.types.IDATA;
import com.ibm.j9ddr.vm29.types.U32;
import com.ibm.j9ddr.vm29.types.UDATA;

/**
 *
 */
public class J9MemTagHelper
{
	
	public static final long ROUNDING_GRANULARITY = 8;
	
	public static IDATA J9PORT_MEMTAG_HEADER_TAG_CORRUPTED = new IDATA(0x00000001);
	public static IDATA J9PORT_MEMTAG_FOOTER_TAG_CORRUPTED = new IDATA(0x00000002);
	public static IDATA J9PORT_MEMTAG_FOOTER_PADDING_CORRUPTED = new IDATA(0x00000004);
	public static IDATA J9PORT_MEMTAG_NOT_A_TAG = new IDATA(0x00000008);
	public static IDATA J9PORT_MEMTAG_VALID_TAG = new IDATA(0x00000010);

	/**
	 * Performs validation checks on the memory block starting at memoryPointer.
	 * 
	 * The memory passed in could fall into one of three categories:
	 * 1) It could be a valid J9MemTag region with valid headers and footers
	 * 2) It could be not be a J9MemTag region
	 * 3) It could be a J9MemTag region with some corruption in either the header, footer or padding
	 * 
	 * @param[in] portLibrary The port library
	 * @param[in] memoryPointer address returned by @ref j9mem_allocate_memory()
	 * 
	 * @throws CorruptDataException if the tags are corrupted, or the memory is inaccessible. (Option 3)
	 * @return J9PORT_MEMTAG_NOT_A_TAG if memoryPointer address is not a tag
	 * 		   J9PORT_MEMTAG_HEADER_TAG_CORRUPTED if memoryPointer address is valid tag region but header tag is corrupted.
	 * 		   J9PORT_MEMTAG_FOOTER_TAG_CORRUPTED if memoryPointer address is valid tag region but footer tag is corrupted.
	 * 		   J9PORT_MEMTAG_FOOTER_PADDING_CORRUPTED if memoryPointer address is valid tag region but footer padding is corrupted.
	 * 		   J9PORT_MEMTAG_VALID_TAG if memoryPointer address is valid tag and header/footer are not corrupted.
	 * @note memoryPointer may not be NULL.
	 */
	public static IDATA j9mem_check_tags(VoidPointer memoryPointer, long headerEyecatcher, long footerEyecatcher) throws J9MemTagCheckError 
	{
		J9MemTagPointer headerTagAddress = j9mem_get_header_tag(memoryPointer);
		J9MemTagPointer footerTagAddress = J9MemTagPointer.NULL;

		try {
			footerTagAddress = j9mem_get_footer_tag(headerTagAddress);
			checkTagSumCheck(headerTagAddress, headerEyecatcher);
		} catch (J9MemTagCheckError e) {
			/* Corruption here either means the header tag is mangled, or the header eyecatcher isn't actually the start of a 
			 * J9MemTag block.
			 * 
			 * If we can find a valid footerTag, then we'll believe this is a mangled header.
			 */
			try {
				if (checkEyecatcher(footerTagAddress, footerEyecatcher)) {
					/* Footer eyecatcher is valid - header tag is corrupt */
					throw e;
				} else {
					/* Footer eyecatcher is invalid - this isn't a memory tag */
					return J9PORT_MEMTAG_NOT_A_TAG;
				}
			} catch (CorruptDataException e1) {
				/* Can't read the footer tag - assume memoryPointer isn't a tag */
				return J9PORT_MEMTAG_NOT_A_TAG;
			}
		} catch (CorruptDataException e) {
			/* CorruptDataException here will mean MemoryFault. Which means we can't read the 
			 * entire header tag - so we assume this isn't a valid J9MemTag
			 */
			return J9PORT_MEMTAG_NOT_A_TAG;
		}
	
		try {
			checkTagSumCheck(footerTagAddress, footerEyecatcher);
		} catch (J9MemTagCheckError e) {
			if (headerEyecatcher == J9MEMTAG_EYECATCHER_FREED_HEADER && footerEyecatcher == J9MEMTAG_EYECATCHER_FREED_FOOTER) {
				/* When dealing with already freed memory, it is reasonable to accept
				 * a block that is missing the footer instead of throwing an exception.
				 */
				return J9PORT_MEMTAG_FOOTER_TAG_CORRUPTED;
			} else {
				throw e;
			}
		} catch (CorruptDataException e) {
			/* If we got here, the header was valid. A memory fault here suggests
			 * we're missing the storage for the footer - which is corruption
			 */
			throw new J9MemTagCheckError(headerTagAddress, e);
		}
	
		try {
			checkPadding(headerTagAddress);
		} catch (J9MemTagCheckError e) {
			if (headerEyecatcher == J9MEMTAG_EYECATCHER_FREED_HEADER && footerEyecatcher == J9MEMTAG_EYECATCHER_FREED_FOOTER) {
				/* When dealing with already freed memory, it is reasonable to accept
				 * a block that is missing the footer instead of throwing an exception.
				 */
				return J9PORT_MEMTAG_FOOTER_PADDING_CORRUPTED;
			} else {
				throw e;
			}
		} catch (CorruptDataException e) {
			/* If we got here, the header was valid. A memory fault here suggests
			 * we're missing the storage for the padding - which is corruption
			 */
			throw new J9MemTagCheckError(headerTagAddress, e);
		}
		
		return J9PORT_MEMTAG_VALID_TAG;
	}

	/**
	 * Checks that the memory tag is not corrupt.
	 * 
	 * @param[in] tagAddress the in-process or out-of-process address of the
	 *            header/footer memory tag
	 * @param[in] eyeCatcher the eyecatcher corresponding to the memory tag
	 * 
	 * @return 0 if the sum check is successful, non-zero otherwise
	 * @throws CorruptDataException
	 */
	public static void checkTagSumCheck(J9MemTagPointer tag, long eyeCatcher) throws CorruptDataException 
	{
		int sum = 0;
		U32Pointer slots;
		
		if (! checkEyecatcher(tag, eyeCatcher)) {
			throw new J9MemTagCheckError(tag, "Wrong eyecatcher. Expected 0x" + Long.toHexString(eyeCatcher) + " but was " + UDATA.cast(tag).getHexValue());
		}

		slots = U32Pointer.cast(tag);
		/*
		 * Could be unrolled into chained xors with a J9VM_ENV_DATA64
		 * conditional on the extra 2 U_32s
		 */
		for (int i = 0; i < (J9MemTag.SIZEOF / U32.SIZEOF); i++) {
			sum ^= slots.at(i).longValue();
		}
		
		if (J9BuildFlags.env_data64) {
			U32 a = new U32(UDATA.cast(tag).rightShift(32));
			U32 b = new U32(UDATA.cast(tag).bitAnd(U32.MAX));
			sum ^= a.longValue() ^ b.longValue();
		} else {
			sum ^= tag.longValue();
		}

		if (sum != 0) {
			throw new J9MemTagCheckError(tag,"J9MemTag sumcheck failed: " + sum);
		}
	}

	private static boolean checkEyecatcher(J9MemTagPointer tag, long eyeCatcher)
			throws CorruptDataException
	{
		return tag.eyeCatcher().eq(eyeCatcher);
	}

	
	/**
	 * Given the address of the headerEyecatcher for the memory block, return
	 * the memory pointer that was returned by j9mem_allocate_memory() when the
	 * block was allocated.
	 */
	public static VoidPointer j9mem_get_memory_base(J9MemTagPointer headerEyeCatcherAddress)
	{
		return VoidPointer.cast(U8Pointer.cast(headerEyeCatcherAddress).add(J9MemTag.SIZEOF));
	}

	/**
	 * Given the address of the headerEyecatcher for the memory block, return
	 * the address of the corresponding footer tag.
	 * 
	 * @throws CorruptDataException
	 */
	public static J9MemTagPointer j9mem_get_footer_tag(J9MemTagPointer headerEyeCatcherAddress) throws CorruptDataException 
	{
		UDATA footerOffset = ROUNDED_FOOTER_OFFSET(headerEyeCatcherAddress.allocSize());
		return J9MemTagPointer.cast(U8Pointer.cast(headerEyeCatcherAddress).add(footerOffset));
	}
	

	/**
	 * Given the address returned by @ref j9mem_allocate_memory(), return
	 * address of the header tag for the memory block
	 * 
	 */
	public static J9MemTagPointer j9mem_get_header_tag(VoidPointer memoryPointer) {
		return J9MemTagPointer.cast(U8Pointer.cast(memoryPointer).sub(J9MemTag.SIZEOF));
	}

	/**
	 * Checks that the padding associated with the memory block has not been
	 * corrupted
	 * 
	 * @param[in] headerTagAddress the address of the header memory tag for the
	 *            memory block.
	 * 
	 * @return 0 if no corruption was detected, otherwise non-zero.
	 * @throws CorruptDataException
	 */
	public static void checkPadding(J9MemTagPointer headerTagAddress) throws CorruptDataException 
	{

		U8Pointer padding = U8Pointer.cast(j9mem_get_footer_padding(headerTagAddress));

		while (!(UDATA.cast(padding).bitAnd(ROUNDING_GRANULARITY - 1)).eq(0)) {
			if (padding.at(0).eq(J9PortLibrary.J9MEMTAG_PADDING_BYTE)) {
				padding = padding.add(1);
			} else {
				throw new J9MemTagCheckError(headerTagAddress, "J9MemTag Footer Padding Corrupted at " + padding.getHexAddress() + ". Value: " + padding.at(0));
			}
		}
	}

	/**
	 * Given the address of the headerEyecatcher for the memory block, return
	 * the address of the footer padding.
	 * 
	 * Note that there not be any padding, in which case this returns the same
	 * as @ref j9mem_get_footer_tag(), the address of the footer tag.
	 * 
	 * @throws CorruptDataException
	 */
	public static VoidPointer j9mem_get_footer_padding(J9MemTagPointer headerEyeCatcherAddress) throws CorruptDataException 
	{
		UDATA cursor = UDATA.cast(U8Pointer.cast(headerEyeCatcherAddress).add(J9MemTag.SIZEOF));
		U8Pointer padding = U8Pointer.cast(cursor.add(headerEyeCatcherAddress.allocSize()));

		return VoidPointer.cast(padding);
	}
	
	public static UDATA ROUNDED_FOOTER_OFFSET(UDATA number) 
	{
		UDATA mask = new UDATA(ROUNDING_GRANULARITY - 1);
		UDATA a = mask.add(number).add(J9MemTag.SIZEOF);
		UDATA b = mask.bitNot();
		return a.bitAnd(b);
	}
	
	public static class J9MemTagCheckError extends AddressedCorruptDataException
	{
		/**
		 * 
		 */
		private static final long serialVersionUID = -8326638947722902353L;
		private final J9MemTagPointer memTag;
		
		J9MemTagCheckError(J9MemTagPointer memTag, String message)
		{
			super(memTag.getAddress(), message);
			this.memTag = memTag;
		}
		
		public J9MemTagCheckError(J9MemTagPointer memTag,
				CorruptDataException e)
		{
			super(memTag.getAddress(), e);
			this.memTag = memTag;
		}

		public J9MemTagPointer getMemTag()
		{
			return memTag;
		}
	}
}
