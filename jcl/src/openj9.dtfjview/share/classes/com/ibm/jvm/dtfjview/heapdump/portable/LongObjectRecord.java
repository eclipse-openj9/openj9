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

import com.ibm.jvm.dtfjview.heapdump.ReferenceIterator;

public class LongObjectRecord extends ObjectRecord
{    
	private final byte _tag;
	private final boolean _is64Bit;
	
	public LongObjectRecord(long address, long previousAddress,
			long classAddress, int hashCode,
			ReferenceIterator references, boolean is64Bit, boolean is32BitHash)
	{
		this(PortableHeapDumpFormatter.LONG_OBJECT_RECORD_TAG,address,previousAddress,classAddress,hashCode,references,is64Bit,is32BitHash);
	}
	
	/**
	 * Alternative constructor for use by object array record - the structure of that record is
	 * essentially identical with a different tag. 
	 */
	protected LongObjectRecord(byte tag,long address, long previousAddress,
			long classAddress, int hashCode,
			ReferenceIterator references, boolean is64Bit, boolean is32BitHash)
	{
		super(address,previousAddress,classAddress,hashCode,references,is32BitHash);
		
		_tag = tag;
		
		_is64Bit = is64Bit;
	}

	protected void writeHeapDump(DataOutput out) throws IOException
	{
		out.writeByte(_tag);
		
		byte flag = 0;
		flag |= _gapSize << 6;
		flag |= _referenceFieldSize << 4;
		if (_is32BitHash) { // JVM 2.6 and later, optional 32-bit hashcode
			if (_hashCode != 0) {
				flag |= (byte) 0x03; // set 0x01 and 0x02 bits if the record includes a hashcode
			}
		} else { // JVM prior to 2.6, all objects have a 16-bit hashcode
			flag |= (byte) 0x01;
		}
		
		out.writeByte(flag);
		
		writeReference(out,_gapSize,_gapPreceding);
		
		if(_is64Bit){
			out.writeLong(_classAddress);
		} else {
			out.writeInt((int)_classAddress);
		}
		
		if (_is32BitHash) { // JVM 2.6 and later, optional 32-bit hashcode
			if (_hashCode != 0) {
				out.writeInt(_hashCode);
			}
		} else { // JVM prior to 2.6 VM, all objects have a 16-bit hashcode
			out.writeShort(_hashCode);
		}
		
		out.writeInt(_numberOfReferences);
		
		writeReferences(out);
	}
   
}
