/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2008, 2018 IBM Corp. and others
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
package com.ibm.jvm.dtfjview.heapdump.portable;

import java.io.DataOutput;
import java.io.IOException;

import com.ibm.jvm.dtfjview.heapdump.ReferenceIterator;

/**
 * Base class for all PHD records.
 * 
 * Contains logic common to relative-addressed entities with references
 * 
 * @author andhall
 * 
 */
public abstract class PortableHeapDumpRecord
{

	/* Constants for reference sizes */
	public static final byte ONE_BYTE_REF = 0;
	public static final byte TWO_BYTE_REF = 1;
	public static final byte FOUR_BYTE_REF = 2;
	public static final byte EIGHT_BYTE_REF = 3;
	public static final int ADDRESS_SHIFT_SIZE = 2; // pow(2,2) = 4 - objects
													// lies on 4 byte boundary

	/**
	 * Address diff-addresses are based from
	 */
	protected final long _baseAddress;
	protected final ReferenceIterator _references;
	protected final byte _referenceFieldSize;
	protected final long _gapPreceding;
	protected final byte _gapSize;
	protected int _numberOfReferences;

	/**
	 * Constructor
	 * 
	 * @param baseAddress
	 *            The address to base the relative reference addresses from
	 * @param references
	 *            Iterator of regular (absolute) references
	 */
	protected PortableHeapDumpRecord(long address, long previousAddress,
			ReferenceIterator references)
	{
		_baseAddress = address;
		
		if(references != null) {
			_references = getDifferenceReferences(references, address);
			_referenceFieldSize = calculateReferenceFieldSize(references);
		} else {
			_references = null;
			_referenceFieldSize = 0;
		}
		
		_gapPreceding = getAddressDifference(address,previousAddress);
		_gapSize = sizeofReference(_gapPreceding);
	}

	private byte calculateReferenceFieldSize(ReferenceIterator references)
	{
		byte toReturn = ONE_BYTE_REF;
		
		references.reset();
		
		_numberOfReferences = 0;
		
		while (references.hasNext()) {
			Long thisRef = references.next();

			byte thisSize = sizeofReference(thisRef.longValue());

			if (thisSize > toReturn) {
				toReturn = thisSize;
			}
			
			_numberOfReferences++;
		}

		return toReturn;
	}

	protected static byte sizeofReference(long reference)
	{
		if (reference > Integer.MAX_VALUE || reference < Integer.MIN_VALUE) {
			return EIGHT_BYTE_REF;
		}
		if (reference > Short.MAX_VALUE || reference < Short.MIN_VALUE) {
			return FOUR_BYTE_REF;
		}
		if (reference > Byte.MAX_VALUE || reference < Byte.MIN_VALUE) {
			return TWO_BYTE_REF;
		}
		return ONE_BYTE_REF;

	}

	protected ReferenceIterator getDifferenceReferences(
			final ReferenceIterator input, final long base)
	{
		return new ReferenceIterator()
		{

			public boolean hasNext()
			{
				return input.hasNext();
			}

			public Long next()
			{
				Long toConvert = input.next();

				if (toConvert == null) {
					return null;
				}
				else {
					return Long.valueOf(getAddressDifference(toConvert.longValue(),
							base));
				}
			}

			public void reset()
			{
				input.reset();
			}
		};
	}

	protected static long getAddressDifference(long address, long base)
	{
		return (address - base) >> ADDRESS_SHIFT_SIZE;
	}

	protected final void writeReferences(DataOutput out) throws IOException
	{
		_references.reset();
		if (!_references.hasNext()) {
			return;
		}

		switch (_referenceFieldSize) {
		case PortableHeapDumpRecord.ONE_BYTE_REF:
			while (_references.hasNext()) {
				Long thisRef = _references.next();
				out.writeByte(thisRef.byteValue());
			}
			break;
		case PortableHeapDumpRecord.TWO_BYTE_REF:
			while (_references.hasNext()) {
				Long thisRef = _references.next();
				out.writeShort(thisRef.shortValue());
			}
			break;
		case PortableHeapDumpRecord.FOUR_BYTE_REF:
			while (_references.hasNext()) {
				Long thisRef = _references.next();
				out.writeInt(thisRef.intValue());
			}
			break;
		case PortableHeapDumpRecord.EIGHT_BYTE_REF:
			while (_references.hasNext()) {
				Long thisRef = _references.next();
				out.writeLong(thisRef.longValue());
			}
			break;
		default:
			throw new IllegalArgumentException("Error size byte invalid");
		}
	}

	protected void writeReference(DataOutput dos, byte size, long reference)
			throws IOException
	{
		switch (size) {
		case PortableHeapDumpRecord.ONE_BYTE_REF:
			dos.writeByte((byte) reference);
			break;
		case PortableHeapDumpRecord.TWO_BYTE_REF:
			dos.writeShort((short) reference);
			break;
		case PortableHeapDumpRecord.FOUR_BYTE_REF:
			dos.writeInt((int) reference);
			break;
		case PortableHeapDumpRecord.EIGHT_BYTE_REF:
			dos.writeLong(reference);
			break;
		default:
			throw new IllegalArgumentException("Wrong size argument!");
		}
	}

	protected abstract void writeHeapDump(DataOutput out)
			throws IOException;
}
