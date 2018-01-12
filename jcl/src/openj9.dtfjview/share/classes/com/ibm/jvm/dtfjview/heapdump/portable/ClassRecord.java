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

/**
 * Object representing a PHD ClassRecord
 * 
 * @author andhall
 *
 */
public class ClassRecord extends PortableHeapDumpRecord
{
	private final String _className;
	private final long _superClassAddress;
	private final long _instanceSize;    
	private final int _hashCode;
	private final boolean _is64Bit;
	private final boolean _is32BitHash;
	
	protected ClassRecord(long address, long previousAddress, String className, long superClassAddress, long instanceSize, int hashCode, boolean is64Bit,
			ReferenceIterator references, boolean is32BitHash)
	{
		super(address, previousAddress, references);
		
		this._className = className;
		this._superClassAddress = superClassAddress;
		
		this._instanceSize = instanceSize;
		this._is64Bit = is64Bit;
		this._hashCode = hashCode;
		this._is32BitHash = is32BitHash;
	}

	protected void writeHeapDump(DataOutput out) throws IOException
	{
		out.writeByte(PortableHeapDumpFormatter.CLASS_RECORD_TAG);
		
		byte flag = 0;
		flag |= _gapSize << 6;
		flag |= _referenceFieldSize << 4;
		if (_is32BitHash && _hashCode != 0) { // JVM 2.6 and later, 32-bit optional hashcodes
			flag |= (byte) 0x08; // set 0x08 flag if the class record includes a hashcode
		}
		
		out.writeByte(flag);
		
		writeReference(out,_gapSize,_gapPreceding);
		out.writeInt((int)_instanceSize); // instance size became a long for defect 176753, when DTFJ interface changed to 1.6
		
		if (_is32BitHash) { // JVM 2.6 and later, write optional 32-bit hashcode
			if (_hashCode != 0) {
				out.writeInt(_hashCode);
			}
		} else { // JVM prior to 2.6, all objects have a 16-bit hashcode 
			out.writeShort(_hashCode);
		}
		
		if(_is64Bit) {
			out.writeLong(_superClassAddress);
		} else {
			out.writeInt((int)_superClassAddress);
		}
		
		out.writeUTF(_className);
		
		out.writeInt(_numberOfReferences);
		
		writeReferences(out);
	}
	
}
