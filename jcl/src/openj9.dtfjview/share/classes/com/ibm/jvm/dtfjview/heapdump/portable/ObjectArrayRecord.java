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
 * Record representing an object array.
 * 
 * As of PHD v5 this contains a number of elements record on the
 * end of the LongObjectRecord structure
 * @author andhall
 *
 */
public class ObjectArrayRecord extends LongObjectRecord
{    
	private final int _numberOfElements;
	private final long _instanceSize;

	public ObjectArrayRecord(long address, long previousAddress,
			long elementClassAddress, int hashCode,
			int numberOfElements, ReferenceIterator references, boolean is64bit, long instanceSize, boolean is32BitHash)
	{
		super(PortableHeapDumpFormatter.OBJECT_ARRAY_RECORD_TAG,address,previousAddress,elementClassAddress,hashCode,references,is64bit,is32BitHash);

		this._numberOfElements = numberOfElements;
		this._instanceSize = instanceSize;
	}

	protected void writeHeapDump(DataOutput out) throws IOException
	{
		super.writeHeapDump(out);

		out.writeInt(_numberOfElements);
		// instance size is held in the PHD since PHD version 6     
		// divide the size by 4 and write into the datastream as an 
		// unsigned int to make it possible to encode up to 16GB
		out.writeInt((int)(_instanceSize / 4));
	}

}
