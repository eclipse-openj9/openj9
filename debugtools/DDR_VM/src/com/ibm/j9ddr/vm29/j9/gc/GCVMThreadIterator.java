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

import java.util.NoSuchElementException;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;

public class GCVMThreadIterator extends GCIterator
{
	public final static int state_start = 0;
	public final static int state_slots = 1;
	public final static int state_jni_slots = 2;
	public final static int state_monitor_records = 3;
	public static final int state_end = 4;
	
	protected int state = state_start;
	
	protected GCVMThreadSlotIterator threadSlotIterator;
	protected GCVMThreadJNISlotIterator threadJNISlotIterator;
	protected GCVMThreadMonitorRecordSlotIterator threadMonitorRecordSlotIterator;
	
	protected J9VMThreadPointer vmThread;
	
	protected GCVMThreadIterator(J9VMThreadPointer vmThread) throws CorruptDataException
	{
		this.vmThread = vmThread;
		threadSlotIterator = GCVMThreadSlotIterator.fromJ9VMThread(vmThread);
		threadJNISlotIterator = GCVMThreadJNISlotIterator.fromJ9VMThread(vmThread);
		threadMonitorRecordSlotIterator = GCVMThreadMonitorRecordSlotIterator.fromJ9VMThread(vmThread);
	}

	public static GCVMThreadIterator fromJ9VMThread(J9VMThreadPointer vmThread) throws CorruptDataException
	{
		return new GCVMThreadIterator(vmThread);
	}

	public boolean hasNext()
	{
		if(state == state_end) {
			return false;
		}
		/* this switch has an intentional fall through logic */
		switch(state) {
		
		case state_start:
			state++;
		
		case state_slots:
			if(threadSlotIterator.hasNext()) {
				return true;
			}
			state++;
			
		case state_jni_slots:
			if(threadJNISlotIterator.hasNext()) {
				return true;
			}
			state++;
			
		case state_monitor_records:
			if(threadMonitorRecordSlotIterator.hasNext()) {
				return true;
			}
			state++;
		}
		return false;
	}

	public J9ObjectPointer next()
	{
		if(hasNext()) {
			switch(state) {
			
			case state_slots:
				return threadSlotIterator.next();
				
			case state_jni_slots:
				return threadJNISlotIterator.next();
				
			case state_monitor_records:
				return threadMonitorRecordSlotIterator.next();
			}
		}
		throw new NoSuchElementException("There are no more items available through this iterator"); 
	}

	public VoidPointer nextAddress()
	{
		if(hasNext()) {
			switch(state) {
			
			case state_slots:
				return threadSlotIterator.nextAddress();
				
			case state_jni_slots:
				return threadJNISlotIterator.nextAddress();
				
			case state_monitor_records:
				return threadMonitorRecordSlotIterator.nextAddress();
			}
		}
		throw new NoSuchElementException("There are no more items available through this iterator"); 
	}

	public int getState()
	{
		return state;
	}

	public J9VMThreadPointer getThread()
	{
		return vmThread;
	}
}
