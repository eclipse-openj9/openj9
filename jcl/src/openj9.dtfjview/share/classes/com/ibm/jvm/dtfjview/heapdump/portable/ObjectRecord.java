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

import com.ibm.jvm.dtfjview.heapdump.ReferenceIterator;

/**
 * Object record.
 * 
 * Base class for anything that looks like an object (has a classAddress, a size and a hashcode
 * in addition to the address, previousAddress and references required to be a PortableHeapDumpRecord).
 * 
 * @author andhall
 *
 */
public abstract class ObjectRecord extends PortableHeapDumpRecord
{
	protected final int _hashCode;
	protected final long _classAddress;
	protected final boolean _is32BitHash;
	
	protected ObjectRecord(long address, long previousAddress,
			long classAddress, int hashCode,
			ReferenceIterator references, boolean is32BitHash)
	{
		super(address, previousAddress, references);
		
		this._hashCode = hashCode;
		this._classAddress = classAddress;
		this._is32BitHash = is32BitHash;
	}

	protected static boolean isShortObjectEligible(long current, long previous) {
		long addressDifference = PortableHeapDumpRecord.getAddressDifference(current, previous);
		return ( addressDifference < Short.MAX_VALUE && addressDifference > Short.MIN_VALUE );
	}
	
	/**
	 * Static factory method to pick the appropriate
	 * factory method
	 * @return ObjectRecord for this object
	 */
	public static ObjectRecord getObjectRecord(long address, long previousAddress, long classAddress, int hashCode,
			ReferenceIterator references, PortableHeapDumpClassCache cache, boolean is64Bit, boolean is32BitHash)
	{
		byte classCacheIndex = cache.getClassCacheIndex(classAddress);
		
		if (classCacheIndex == -1 || previousAddress == 0) {
			cache.setClassCacheIndex(classAddress);
			return new LongObjectRecord(address,previousAddress,classAddress,hashCode,references,is64Bit,is32BitHash);
		} else if (is32BitHash && hashCode != 0) {
			// JVM 2.6 and later, 32-bit optional hashcodes, require LongObjectRecord if hashcode is set
			cache.setClassCacheIndex(classAddress);
			return new LongObjectRecord(address,previousAddress,classAddress,hashCode,references,is64Bit,is32BitHash);
		} else if (isShortObjectEligible(address,previousAddress)) {
			int numberOfReferences = countReferences(references);
			
			if (numberOfReferences <= 3) { 
				//this is a short object
				return new ShortObjectRecord(address,previousAddress,classAddress,hashCode,references,classCacheIndex,is32BitHash);
			} else if (numberOfReferences < 8) {
				//this is a medium object
				cache.setClassCacheIndex(classAddress);
				return  new MediumObjectRecord(address,previousAddress,classAddress,hashCode,references,is64Bit,is32BitHash);
			} else {
				cache.setClassCacheIndex(classAddress);
				return new LongObjectRecord(address,previousAddress,classAddress,hashCode,references,is64Bit,is32BitHash);
			}
		} else { 
			//address difference is bigger than Short.MAX_VALUE doublewords.
			cache.setClassCacheIndex(classAddress);
			return new LongObjectRecord(address,previousAddress,classAddress,hashCode,references,is64Bit,is32BitHash);
		}
	}

	private static int countReferences(ReferenceIterator references)
	{
		int count = 0;
		
		references.reset();
		
		while(references.hasNext()) {
			references.next();
			count++;
		}
		
		return count;
	}
}
