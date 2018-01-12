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

public class LongPrimitiveArrayRecord extends PortableHeapDumpRecord
{    
	private final int _type;
	private final int _numberOfElements;
	private final int _hashCode;
	private final boolean _is64Bit;
	private final long _instanceSize;
	private final boolean _is32BitHash;
	
	public LongPrimitiveArrayRecord(long address, long previousAddress, int type, int numberOfElements,
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
		out.writeByte(PortableHeapDumpFormatter.LONG_PRIMITIVE_ARRAY_RECORD_TAG);
		
		byte flag = 0;
		flag |= _type << 5;
		
		byte arrayLengthSize = PortableHeapDumpRecord.sizeofReference(_numberOfElements);
		
		if(arrayLengthSize < _gapSize) {
			arrayLengthSize = _gapSize;
		}
		
		if(arrayLengthSize != ONE_BYTE_REF) {
			flag |= 1 << 4;
		}
		
		//Set flag for moved & hashed
		flag |= 1 << 1; 
		
		out.writeByte(flag);
		
		if(arrayLengthSize > ONE_BYTE_REF) {
			if(_is64Bit){ 
				out.writeLong(_gapPreceding);
				out.writeLong(_numberOfElements);
			}
			else { 
				out.writeInt((int)_gapPreceding);
				out.writeInt(_numberOfElements);
			}
		} else {
			out.writeByte((byte)_gapPreceding);
			out.writeByte(_numberOfElements);
		}
		
		if (_is32BitHash) { // JVM 2.6 and later, 32-bit hashcode
			out.writeInt(_hashCode);
		} else { // JVM prior to 2.6, 16-bit hashcode
			out.writeShort(_hashCode);
		}
		
		// instance size is held in the PHD since PHD version 6     
		// divide the size by 4 and write into the datastream as an 
		// unsigned int to make it possible to encode up to 16GB
		out.writeInt((int)(_instanceSize / 4));
	}

}
