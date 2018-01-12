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
package com.ibm.j9ddr.vm29.tools.ddrinteractive.gccheck;

import static com.ibm.j9ddr.vm29.tools.ddrinteractive.gccheck.CheckBase.J9MODRON_SLOT_ITERATOR_OK;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.MonitorTable;
import com.ibm.j9ddr.vm29.j9.gc.GCMonitorReferenceIterator;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectMonitorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ThreadAbstractMonitorPointer;

class CheckMonitorTable extends Check
{
	@Override
	public void check()
	{
		try {
			GCMonitorReferenceIterator monitorReferenceIterator = GCMonitorReferenceIterator.from();
			while(monitorReferenceIterator.hasNext()) {
				J9ObjectMonitorPointer objectMonitor = monitorReferenceIterator.next();
				J9ThreadAbstractMonitorPointer monitor = J9ThreadAbstractMonitorPointer.cast(objectMonitor.monitor());
				PointerPointer slot = PointerPointer.cast(monitor.userDataEA());
				
				if(_engine.checkSlotPool(slot, VoidPointer.cast(monitorReferenceIterator.currentMonitorTable().getJ9HashTablePointer())) != J9MODRON_SLOT_ITERATOR_OK ) {
					return;
				}
			}
		} catch (CorruptDataException e) {
			// TODO: handle exception
		}
	}

	@Override
	public String getCheckName()
	{
		return "MONITOR TABLE";
	}

	@Override
	public void print()
	{
		try {
			VoidPointer monitorTableList = VoidPointer.cast(getJavaVM().monitorTableList());
			GCMonitorReferenceIterator monitorReferenceIterator = GCMonitorReferenceIterator.from();
			MonitorTable previousMonitorTable = null;
						
			ScanFormatter formatter = new ScanFormatter(this, "MonitorTableList", monitorTableList);
			while(monitorReferenceIterator.hasNext()) {
				J9ObjectMonitorPointer objectMonitor = monitorReferenceIterator.next();
				
				MonitorTable currentMonitorTable = monitorReferenceIterator.currentMonitorTable();
								
				if (!currentMonitorTable.equals(previousMonitorTable)) {
					if (null != previousMonitorTable) {
						formatter.endSection();
					}
					formatter.section("MonitorTable", currentMonitorTable.getMonitorTableListEntryPointer());
				}
				
				J9ThreadAbstractMonitorPointer monitor = J9ThreadAbstractMonitorPointer.cast(objectMonitor.monitor());
				formatter.entry(VoidPointer.cast(monitor.userData()));
			
				previousMonitorTable = currentMonitorTable;
			}
			if (null != previousMonitorTable) {
				formatter.endSection();
			}
			formatter.end("MonitorTableList", monitorTableList);
		} catch (CorruptDataException e) {
			// TODO: handle exception
		}
	}

}
