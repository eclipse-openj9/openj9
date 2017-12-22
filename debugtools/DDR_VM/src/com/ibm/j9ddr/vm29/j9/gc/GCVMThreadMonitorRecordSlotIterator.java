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
package com.ibm.j9ddr.vm29.j9.gc;

import static com.ibm.j9ddr.vm29.events.EventManager.raiseCorruptDataEvent;

import java.util.NoSuchElementException;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MonitorEnterRecordPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;

public class GCVMThreadMonitorRecordSlotIterator extends GCIterator
{
	protected J9MonitorEnterRecordPointer monitorRecord;
	protected J9MonitorEnterRecordPointer jniMonitorRecord;
	
	protected GCVMThreadMonitorRecordSlotIterator(J9VMThreadPointer vmThread) throws CorruptDataException
	{
		monitorRecord = vmThread.monitorEnterRecords();
		jniMonitorRecord = vmThread.jniMonitorEnterRecords();
	}

	public static GCVMThreadMonitorRecordSlotIterator fromJ9VMThread(J9VMThreadPointer vmThread) throws CorruptDataException
	{
		return new GCVMThreadMonitorRecordSlotIterator(vmThread);
	}
	
	public boolean hasNext()
	{
		return monitorRecord.notNull() || jniMonitorRecord.notNull();
	}

	public J9ObjectPointer next()
	{
		if(hasNext()) {
			try {
				J9ObjectPointer next;
				if(monitorRecord.notNull()) { 
					next = monitorRecord.object();
					monitorRecord = monitorRecord.next();
				} else {
					next = jniMonitorRecord.object();
					jniMonitorRecord = jniMonitorRecord.next();
				}
				return next;
			} catch (CorruptDataException e) {
				raiseCorruptDataEvent("Error getting next item", e, false);		//can try to recover from this
				return null;
			}
		} else {
			throw new NoSuchElementException("There are no more items available through this iterator");
		}
	}
	
	public VoidPointer nextAddress()
	{
		if(hasNext()) {
			try {
				VoidPointer next;
				if(monitorRecord.notNull()) {
					next = VoidPointer.cast(monitorRecord.objectEA());
					monitorRecord = monitorRecord.next();
				} else {
					next = VoidPointer.cast(jniMonitorRecord.objectEA());
					jniMonitorRecord = jniMonitorRecord.next();
				}
				return next;
			} catch (CorruptDataException e) {
				raiseCorruptDataEvent("Error getting next item", e, false);		//can try to recover from this
				return null;
			}
		} else {
			throw new NoSuchElementException("There are no more items available through this iterator");
		}
	}
}
