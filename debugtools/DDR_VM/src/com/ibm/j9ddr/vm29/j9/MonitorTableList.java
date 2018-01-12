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

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MonitorTableListEntryPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectMonitorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.types.U32;

/**
 * Represents the list of monitors anchored on J9JavaVM->monitorTableList
 */
public class MonitorTableList {
	
	protected static MonitorTable[] monitorTables = null;
	protected static boolean initialized = false;

	/**
	 * Returns a new MonitorTableList that represents the list of monitors anchored on J9JavaVM->monitorTableList
	 * 
	 * @throws CorruptDataException
	 * 
	 * @return 
	 */
	public static MonitorTableList from() throws CorruptDataException
	{
		return new MonitorTableList();
	}
	
	/**
	 * Returns an iterator that can be used to iterate over the J9ObjectMonitorPointers in the MonitorTableList
	 * 
	 * @throws CorruptDataException
	 * 
	 * @return an iterator that can be used to iterate over the J9ObjectMonitorPointers in the MonitorTableList
	 */
	public MonitorTableListIterator iterator() throws CorruptDataException
	{
		return new MonitorTableListIterator();
	}
	
	/**
	 * Search all the monitor tables in the J9JavaVM->monitorTables for the inflated monitor corresponding to the specified object
	 * 
	 * @throws CorruptDataException 
	 * 
	 * @param object the object
	 * 
	 * @returns the J9ObjectMonitorPointer found in the table, or J9ObjectMonitorPointer.NULL if no entry was found 
	 * 
	 * @see util/thrinfo.c : monitorTablePeek
	 */
	public static J9ObjectMonitorPointer peek(J9ObjectPointer object) throws CorruptDataException {
				
		if ((null == object) || (object.isNull())){
			return J9ObjectMonitorPointer.NULL;
		}
		MonitorTable table = null;
		
		if (!initialized) {
			initializeCaches();
		}
		
		J9ObjectMonitorPointer objectMonitor = J9ObjectMonitorPointer.NULL;
		
		long hashcode = new U32(ObjectModel.getObjectHashCode(object)).longValue();
		int index = (int)(hashcode % monitorTables.length);
		table = monitorTables[index];
		
		if (table == null) {
			return objectMonitor;
		}
		
		if (null != table) {
			objectMonitor = table.peek(object);
			if (null == objectMonitor) {
				return J9ObjectMonitorPointer.NULL;
			}
		}
		
		return objectMonitor;
	}
	
	private static synchronized void initializeCaches() throws CorruptDataException {
		if (!initialized) {
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			PointerPointer cursor = vm.monitorTables();
			
			int count = vm.monitorTableCount().intValue();
			monitorTables = new MonitorTable[count];
			
			for (int i = 0; i < count; i++) {
				J9MonitorTableListEntryPointer entries = vm.monitorTableList();
				while (entries.notNull()) {
					if (entries.monitorTable().eq(cursor.at(i))) {
						monitorTables[i] = MonitorTable.from(entries);
						break;
					}
					entries = entries.next();
				}
			}			
			initialized = true;
		}
	}
}
