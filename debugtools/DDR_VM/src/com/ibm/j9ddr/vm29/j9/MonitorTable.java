/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

import java.util.HashMap;
import java.util.Iterator;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.HashTable.HashEqualFunction;
import com.ibm.j9ddr.vm29.j9.HashTable.HashFunction;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9HashTablePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MonitorTableListEntryPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectMonitorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ThreadAbstractMonitorPointer;
import com.ibm.j9ddr.vm29.types.UDATA;

public class MonitorTable implements IHashTable<J9ObjectMonitorPointer>
{	
	protected HashTable<PointerPointer> monitorTable;
	protected HashMap<J9ObjectPointer,J9ObjectMonitorPointer> cachedMonitorTable;
	protected J9HashTablePointer hashTable;
	protected J9MonitorTableListEntryPointer monitorTableListEntry;
	
	// Not intended for construction, use the factory
	protected MonitorTable(J9MonitorTableListEntryPointer entry)  throws CorruptDataException
	{
		HashTable.HashEqualFunction<PointerPointer> equalFunction = getEqualFunction();
		HashTable.HashFunction<PointerPointer> hashFunction = getHashFunction();
		monitorTableListEntry = entry;
		hashTable = entry.monitorTable();
		
		monitorTable = HashTable.fromJ9HashTable(hashTable, true, PointerPointer.class, equalFunction, hashFunction);
	}
	
	public J9HashTablePointer getJ9HashTablePointer()
	{
		return hashTable;
	}
	
	public String extraInfo() throws CorruptDataException
	{
		String tableName = "Default Monitor Table";
				
		return String.format("<%s>, \"%s\", itemCount=%d", monitorTableListEntry.formatShortInteractive(), tableName, getCount());
	}
	
	public J9MonitorTableListEntryPointer getMonitorTableListEntryPointer()
	{
		return monitorTableListEntry;
	}

	protected static class MonitorHashFunction_V1 implements HashTable.HashFunction<PointerPointer> 
	{
		protected MonitorHashFunction_V1() {
			super();
		}
		
		public UDATA hash(PointerPointer entry)
		{
			try {
				J9ObjectMonitorPointer key = J9ObjectMonitorPointer.cast(entry);
				J9ThreadAbstractMonitorPointer monitor = J9ThreadAbstractMonitorPointer.cast(key.monitor()); 
				J9ObjectPointer object = J9ObjectPointer.cast(monitor.userData());
				return new UDATA(ObjectAccessBarrier.getObjectHashCode(object));
			} catch (CorruptDataException e) {
				raiseCorruptDataEvent("Error calculating hash", e, false);
				return new UDATA(0);
			}
		}
	};

	protected static class MonitorEqualFunction_V1 implements HashTable.HashEqualFunction<PointerPointer>
	{
		protected MonitorEqualFunction_V1() {
			super();
		}
		
		public boolean equal(PointerPointer entry1, PointerPointer entry2)
		{
			// TODO : handle CDE
			try {
				J9ObjectMonitorPointer leftKey = J9ObjectMonitorPointer.cast(entry1);
				J9ObjectMonitorPointer rightKey = J9ObjectMonitorPointer.cast(entry2);
				J9ThreadAbstractMonitorPointer leftMonitor = J9ThreadAbstractMonitorPointer.cast(leftKey.monitor()); 
				J9ThreadAbstractMonitorPointer rightMonitor = J9ThreadAbstractMonitorPointer.cast(rightKey.monitor()); 
				return leftMonitor.userData().eq(rightMonitor.userData());
			} catch (CorruptDataException e) {
				raiseCorruptDataEvent("Error checking equality", e, true);
				return false;
			} 
		}
	};
	
	public static MonitorTable from(J9MonitorTableListEntryPointer entry) throws CorruptDataException
	{
		return new MonitorTable(entry);
	}
	
	private static HashFunction<PointerPointer> getHashFunction()
	{
		AlgorithmVersion version = AlgorithmVersion.getVersionOf(AlgorithmVersion.MONITOR_HASH_FUNCTION_VERSION);
		switch (version.getAlgorithmVersion()) {
			// Add case statements for new algorithm versions
			default:
				return new MonitorHashFunction_V1();
		}
	}

	private static HashEqualFunction<PointerPointer> getEqualFunction()
	{
		AlgorithmVersion version = AlgorithmVersion.getVersionOf(AlgorithmVersion.MONITOR_EQUAL_FUNCTION_VERSION);
		switch (version.getAlgorithmVersion()) {
			// Add case statements for new algorithm versions
			default:
				return new MonitorEqualFunction_V1();
		}
	}

	public SlotIterator<J9ObjectMonitorPointer> iterator()
	{	
		return new SlotIterator<J9ObjectMonitorPointer>() 
		{
			SlotIterator<PointerPointer> monitorTableIterator = monitorTable.iterator();

			public boolean hasNext()
			{
				return monitorTableIterator.hasNext();
			}

			public J9ObjectMonitorPointer next()
			{
				return J9ObjectMonitorPointer.cast(monitorTableIterator.next());
			}
			
			public VoidPointer nextAddress()
			{
				return monitorTableIterator.nextAddress();
			}

			public void remove()
			{
				monitorTableIterator.remove();
			}
		
		};
	}

	public long getCount()
	{
		return monitorTable.getCount();
	}


	public String getTableName()
	{
		return monitorTable.getTableName();
	}

	/**
	 * Search the MonitorTable for the inflated monitor corresponding to an object.
	 * 
	 * @param[in] object the object
	 * @returns a J9ObjectMonitor from the monitor hashtable
	 * @retval NULL There is no corresponding monitor in the MonitorTable.
	 * 
	 * @see util/thrinfo.c : monitorTablePeek
	 */
	public J9ObjectMonitorPointer peek(J9ObjectPointer object)
	{
		if(cachedMonitorTable == null) {
			initializedCachedMonitorTable();
		}
		return cachedMonitorTable.get(object);
	}

	private void initializedCachedMonitorTable()
	{
		cachedMonitorTable = new HashMap<J9ObjectPointer, J9ObjectMonitorPointer>();
		try {
			Iterator<J9ObjectMonitorPointer> iterator = iterator();
			while (iterator.hasNext()) {					
				J9ObjectMonitorPointer objectMonitor = iterator.next();
				J9ObjectPointer object = J9ObjectPointer.cast(objectMonitor.monitor().userData());
				cachedMonitorTable.put(object, objectMonitor);
			}
		} catch (CorruptDataException e) {
			raiseCorruptDataEvent("Error building cached monitor table", e, false);
		}
	}
}
