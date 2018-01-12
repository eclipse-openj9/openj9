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

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.ObjectMonitorReferencePointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassLoaderPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectMonitorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ThreadAbstractMonitorPointer;
import com.ibm.j9ddr.vm29.events.EventManager;
import com.ibm.j9ddr.vm29.j9.AddressAwareRootScanner;
import com.ibm.j9ddr.vm29.j9.stackwalker.WalkState;

abstract class SimpleRootScanner extends AddressAwareRootScanner
{
	protected SimpleRootScanner() throws CorruptDataException 
	{
		super();
	}
	protected abstract void doSlot(J9ObjectPointer slot, VoidPointer address);
	protected abstract void doClass(J9ClassPointer slot, VoidPointer address);
	protected abstract void doClassSlot(J9ClassPointer slot, VoidPointer address);
	
	@Override
	protected void doClassLoader(J9ClassLoaderPointer slot, VoidPointer address)
	{
		try {
			doSlot(slot.classLoaderObject(), address);
		} catch (CorruptDataException e) {}
	}

	@Override
	protected void doFinalizableObject(J9ObjectPointer slot, VoidPointer address)
	{
		doSlot(slot, address);
	}

	@Override
	protected void doJNIGlobalReferenceSlot(J9ObjectPointer slot, VoidPointer address)
	{
		doSlot(slot, address);
	}

	@Override
	protected void doJNIWeakGlobalReference(J9ObjectPointer slot, VoidPointer address)
	{
		doSlot(slot, address);
	}
	
	@Override
	protected void doJVMTIObjectTagSlot(J9ObjectPointer slot, VoidPointer address)
	{
		doSlot(slot, address);
	}

	@Override
	protected void doMonitorReference(J9ObjectMonitorPointer slot, VoidPointer address)
	{
		try {
			J9ThreadAbstractMonitorPointer monitor = J9ThreadAbstractMonitorPointer.cast(slot.monitor());
			doSlot(J9ObjectPointer.cast(monitor.userData()), VoidPointer.cast(monitor.userDataEA()));
		} catch (CorruptDataException e) {
			EventManager.raiseCorruptDataEvent("Errror accessing object slot from J9ObjectMonitorPointer: " + slot.getHexAddress(), e, false);
		}
	}
	
	@Override
	protected void doMonitorLookupCacheSlot(J9ObjectMonitorPointer slot, ObjectMonitorReferencePointer address)
	{
		try {
			J9ThreadAbstractMonitorPointer monitor = J9ThreadAbstractMonitorPointer.cast(slot.monitor());
			doSlot(J9ObjectPointer.cast(monitor.userData()), VoidPointer.cast(monitor.userDataEA()));
		} catch (CorruptDataException e) {
			EventManager.raiseCorruptDataEvent("Errror accessing object slot from J9ObjectMonitorPointer: " + slot.getHexAddress(), e, false);
		}
	}

	@Override
	protected void doPhantomReferenceSlot(J9ObjectPointer slot, VoidPointer address)
	{
		doSlot(slot, address);
	}

	@Override
	protected void doRememberedSlot(J9ObjectPointer slot, VoidPointer address)
	{
		doSlot(slot, address);
	}

	@Override
	protected void doSoftReferenceSlot(J9ObjectPointer slot, VoidPointer address)
	{
		doSlot(slot, address);
	}

	@Override
	protected void doStringCacheTableSlot(J9ObjectPointer slot, VoidPointer address)
	{
		doSlot(slot, address);
	}
	
	@Override
	protected void doStringTableSlot(J9ObjectPointer slot, VoidPointer address)
	{
		doSlot(slot, address);
	}

	@Override
	protected void doUnfinalizedObject(J9ObjectPointer slot, VoidPointer address)
	{
		doSlot(slot, address);
	}
	
	@Override
	protected void doOwnableSynchronizerObject(J9ObjectPointer slot, VoidPointer address)
	{
		doSlot(slot, address);
	}

	@Override
	protected void doVMClassSlot(J9ClassPointer slot, VoidPointer address)
	{
		doClassSlot(slot, address);
	}
	
	@Override
	protected void doVMThreadJNISlot(J9ObjectPointer slot, VoidPointer address)
	{
		doSlot(slot, address);
	}


	@Override
	protected void doVMThreadMonitorRecordSlot(J9ObjectPointer slot, VoidPointer address)
	{
		doSlot(slot, address);
	}
	
	@Override
	protected void doNonCollectableObjectSlot(J9ObjectPointer slot, VoidPointer address)
	{
		doSlot(slot, address);
	}
	
	@Override
	protected void doMemoryAreaSlot(J9ObjectPointer slot, VoidPointer address)
	{
		doSlot(slot, address);
	}

	@Override
	protected void doVMThreadSlot(J9ObjectPointer slot, VoidPointer address)
	{
		doSlot(slot, address);
	}
	
	@Override
	protected void doWeakReferenceSlot(J9ObjectPointer slot, VoidPointer address)
	{
		doSlot(slot, address);
	}
	
	@Override
	protected void doStackSlot(J9ObjectPointer slot, WalkState walkState, VoidPointer stackLocation)
	{
		doSlot(slot, stackLocation);
	}

}
