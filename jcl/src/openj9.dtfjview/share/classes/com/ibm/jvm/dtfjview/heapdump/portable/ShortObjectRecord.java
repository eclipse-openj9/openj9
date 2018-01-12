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

public class ShortObjectRecord extends ObjectRecord
{   
	private final byte _classCacheIndex;
	
	public ShortObjectRecord(long address, long previousAddress,
			long classAddress, int hashCode,
			ReferenceIterator references, byte classCacheIndex, boolean is32BitHash)
	{
		super(address,previousAddress,classAddress,hashCode,references,is32BitHash);
		
		_classCacheIndex = classCacheIndex;
	}

	protected void writeHeapDump(DataOutput out) throws IOException
	{
		int tagAndFlag = PortableHeapDumpFormatter.SHORT_OBJECT_RECORD_TAG;
		tagAndFlag |= _classCacheIndex << 5;
		tagAndFlag |= _numberOfReferences << 3;
		tagAndFlag |= _gapSize << 2;
		tagAndFlag |= _referenceFieldSize;
		
		out.writeByte(tagAndFlag);
		writeReference(out,_gapSize,_gapPreceding);
		
		// JVMs prior to 2.6 have a 16-bit hashcode for all objects, which is added to all PHD records.
		if (!_is32BitHash) {
			out.writeShort(_hashCode);
		}
		// Note: JVM 2.6 and later have optional 32-bit hashcodes. We use a LongObjectRecord if the hashcode was set.
		
		writeReferences(out);
	}
	
}
