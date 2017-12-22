/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.j9;

import static com.ibm.j9ddr.vm29.events.EventManager.raiseCorruptDataEvent;

import java.util.Iterator;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.generated.J9JVMTIEnvPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JVMTIObjectTagPointer;
import com.ibm.j9ddr.vm29.types.UDATA;

public class JVMTIObjectTagTable implements IHashTable<J9JVMTIObjectTagPointer>
{	
	protected HashTable<J9JVMTIObjectTagPointer> objectTagTable;

	// Not intended for construction, use the factory
	protected JVMTIObjectTagTable(HashTable<J9JVMTIObjectTagPointer> hashTable) throws CorruptDataException
	{
		objectTagTable = hashTable;
	}

	protected static class ObjectTagHashFunction implements HashTable.HashFunction<J9JVMTIObjectTagPointer> 
	{
		public UDATA hash(J9JVMTIObjectTagPointer entry)
		{
			try {				
				// return ((UDATA) ((J9JVMTIObjectTag *) entry)->ref) / sizeof(UDATA);
				UDATA hash = UDATA.cast(entry.ref());
				if(4 == UDATA.SIZEOF) {
					return hash.rightShift(2);
				} else {
					return hash.rightShift(3);
				}
			} catch (CorruptDataException e) {
				raiseCorruptDataEvent("Error getting hash", e, false);		//can try to recover from this
				return new UDATA(0);
			}
		}
	};

	protected static class ObjectTagEqualFunction implements HashTable.HashEqualFunction<J9JVMTIObjectTagPointer>
	{
		public boolean equal(J9JVMTIObjectTagPointer entry1, J9JVMTIObjectTagPointer entry2)
		{
			try {
				return entry1.ref().equals(entry2.ref());
			} catch (CorruptDataException e) {
				raiseCorruptDataEvent("Error checking equality", e, true);
				return false;
			} 
		}
	};
	
	public static JVMTIObjectTagTable fromJ9JVMTIEnv(J9JVMTIEnvPointer jvmtiEnv) throws CorruptDataException
	{
		HashTable<J9JVMTIObjectTagPointer> hashTable = HashTable.fromJ9HashTable(
				jvmtiEnv.objectTagTable(),
				true, 
				J9JVMTIObjectTagPointer.class,
				new ObjectTagEqualFunction(),
				new ObjectTagHashFunction());
		return new JVMTIObjectTagTable(hashTable);
	}
	
	public Iterator<J9JVMTIObjectTagPointer> iterator()
	{
		return objectTagTable.iterator();
	}

	public long getCount()
	{
		return objectTagTable.getCount();
	}

	public String getTableName()
	{
		return objectTagTable.getTableName();
	}
}
