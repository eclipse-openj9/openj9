/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2008, 2017 IBM Corp. and others
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

public class PrimitiveArrayRecord extends PortableHeapDumpRecord
{
	private final int _type;
	private final int _numberOfElements;
	private final int _hashCode;
	private final boolean _is64Bit;
	private final long _instanceSize;
	private final boolean _is32BitHash;
	
	public PrimitiveArrayRecord(long address, long previousAddress, int type, int numberOfElements,
			int hashCode, boolean is64Bit, long instanceSize, boolean is32BitHash)
	{
		super(address,previousAddress,null);
		
		this._type = type;
		this._numberOfElements = numberOfElements;
		this._hashCode = hashCode;
		this._is64Bit = is64Bit;
		this._instanceSize = instanceSize;
		this._is32BitHash = is32BitHash;
		
		if(type < 0 || type > 7) {
			throw new IllegalArgumentException("Unrecognised type code: " + type);
		}
	}

	protected void writeHeapDump(DataOutput out) throws IOException
	{
		byte tagAndFlag = PortableHeapDumpFormatter.PRIMITIVE_ARRAY_RECORD_TAG;
		tagAndFlag |= _type << 2;
		
		byte arrayLengthSize = PortableHeapDumpRecord.sizeofReference(_numberOfElements);
		// Array length size and gap size in this record have to be the same, so use the larger
		byte fieldSize = arrayLengthSize > _gapSize ? arrayLengthSize : _gapSize;
		tagAndFlag |= fieldSize;
		
		out.writeByte(tagAndFlag);
		
		// Write the address delta (gap)
		writeReference(out, fieldSize,_gapPreceding);
		// Write the array length
		writeReference(out, fieldSize, arrayLengthSize);
		
		// JVMs prior to 2.6 have a 16-bit hashcode for all objects, which is added to all PHD records
		if (!_is32BitHash) {
			out.writeShort(_hashCode);
		}
		// Note: JVM 2.6 and later have optional 32-bit hashcodes. We use a LongPrimitiveArrayRecord if the hashcode was set
		
		// Instance size is held in the PHD since PHD version 6     
		// divide the size by 4 and write into the datastream as an 
		// unsigned int to make it possible to encode up to 16GB
		out.writeInt((int)(_instanceSize / 4));
	}

}
