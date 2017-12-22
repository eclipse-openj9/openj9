/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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

import java.util.NoSuchElementException;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MonitorTableListEntryPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectMonitorPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;

/** 
 * Iterates over all the J9ObjectMonitor entries in all of the monitor tables in the monitor table list
 */
public class MonitorTableListIterator implements SlotIterator<J9ObjectMonitorPointer> {

	private MonitorTable currentMonitorTable;
	SlotIterator<J9ObjectMonitorPointer> localIterator;
	
	/**
	 * @see SlotIterator
	 */
	public MonitorTableListIterator() throws CorruptDataException {
		currentMonitorTable = MonitorTable.from(J9RASHelper.getVM(DataType.getJ9RASPointer()).monitorTableList());
		localIterator = currentMonitorTable.iterator();
	}
	
	/**
	 * @see SlotIterator
	 */
	public boolean hasNext() {
		while (true) {
			if (localIterator.hasNext()) {
				return true;
			} else {
				J9MonitorTableListEntryPointer entry = currentMonitorTable.getMonitorTableListEntryPointer();
				try {
					entry = entry.next();
					if (entry.notNull()) {
						currentMonitorTable = MonitorTable.from(entry);
						localIterator = currentMonitorTable.iterator();
					} else {
						return false;
					}
				} catch (CorruptDataException e) {
					e.printStackTrace();
					return false;
				}
			}
		}
	}
	
	/**
	 * @see SlotIterator
	 */
	public J9ObjectMonitorPointer next() {
		if (hasNext()) {
			return localIterator.next();
		} else {
			throw new NoSuchElementException("There are no more items available through this iterator");
		}
	}

	/**
	 * Not implemented
	 * 
	 * @throws UnsupportedOperationException
	 * 
	 * @see SlotIterator
	 */
	public void remove() {
		throw new java.lang.UnsupportedOperationException();
	}

	/**
	 * @see SlotIterator
	 */
	public MonitorTable currentMonitorTable() {
		return currentMonitorTable;
	}
	
	/**
	 * @see SlotIterator
	 */
	public VoidPointer nextAddress()
	{
		if(hasNext()) {
			return localIterator.nextAddress();
		} else {
			throw new NoSuchElementException("There are no more items available through this iterator");
		}
	}
}

