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
import com.ibm.j9ddr.vm29.j9.stackwalker.WalkState;
import com.ibm.j9ddr.vm29.pointer.ObjectMonitorReferencePointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassLoaderPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectMonitorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;

public abstract class AddressAwareRootScanner extends RootScanner
{
	protected AddressAwareRootScanner() throws CorruptDataException 
	{
		super();
	}
	
	protected abstract void doClassSlot(J9ClassPointer slot, VoidPointer address);
	protected abstract void doClass(J9ClassPointer clazz, VoidPointer address);
	protected abstract void doClassLoader(J9ClassLoaderPointer slot, VoidPointer address);
	protected abstract void doWeakReferenceSlot(J9ObjectPointer slot, VoidPointer address);
	protected abstract void doSoftReferenceSlot(J9ObjectPointer slot, VoidPointer address);
	protected abstract void doPhantomReferenceSlot(J9ObjectPointer slot, VoidPointer address);
	protected abstract void doFinalizableObject(J9ObjectPointer slot, VoidPointer address);
	protected abstract void doUnfinalizedObject(J9ObjectPointer slot, VoidPointer address);
	protected abstract void doMonitorReference(J9ObjectMonitorPointer objectMonitor, VoidPointer address);
	protected abstract void doMonitorLookupCacheSlot(J9ObjectMonitorPointer objectMonitor, ObjectMonitorReferencePointer address);
	protected abstract void doOwnableSynchronizerObject(J9ObjectPointer slot, VoidPointer address);
	protected abstract void doJNIWeakGlobalReference(J9ObjectPointer slot, VoidPointer address);
	protected abstract void doJNIGlobalReferenceSlot(J9ObjectPointer slot, VoidPointer address);
	protected abstract void doRememberedSlot(J9ObjectPointer slot, VoidPointer address);
	protected abstract void doJVMTIObjectTagSlot(J9ObjectPointer slot, VoidPointer address);
	protected abstract void doStringTableSlot(J9ObjectPointer slot, VoidPointer address);
	protected abstract void doStringCacheTableSlot(J9ObjectPointer slot, VoidPointer address);
	protected abstract void doVMClassSlot(J9ClassPointer slot, VoidPointer address);
	protected abstract void doVMThreadSlot(J9ObjectPointer slot, VoidPointer address);
	protected abstract void doVMThreadJNISlot(J9ObjectPointer slot, VoidPointer address);
	protected abstract void doVMThreadMonitorRecordSlot(J9ObjectPointer slot, VoidPointer address);
	protected abstract void doNonCollectableObjectSlot(J9ObjectPointer slot, VoidPointer address);
	protected abstract void doMemoryAreaSlot(J9ObjectPointer slot, VoidPointer address);
	protected abstract void doStackSlot(J9ObjectPointer slot, WalkState walkState, VoidPointer stackLocation);
	
	@Override
	protected void doClass(J9ClassPointer slot)
	{
		doClass(slot, VoidPointer.NULL);
	}
	
	@Override
	protected void doClassSlot(J9ClassPointer slot)
	{
		doClassSlot(slot, VoidPointer.NULL);
	}

	@Override
	protected void doClassLoader(J9ClassLoaderPointer slot)
	{
		doClassLoader(slot, VoidPointer.NULL);
	}

	@Override
	protected void doFinalizableObject(J9ObjectPointer slot)
	{
		doFinalizableObject(slot, VoidPointer.NULL);
	}

	@Override
	protected void doJNIGlobalReferenceSlot(J9ObjectPointer slot)
	{
		doJNIGlobalReferenceSlot(slot, VoidPointer.NULL);
	}

	@Override
	protected void doJNIWeakGlobalReference(J9ObjectPointer slot)
	{
		doJNIWeakGlobalReference(slot, VoidPointer.NULL);
	}
	
	@Override
	protected void doJVMTIObjectTagSlot(J9ObjectPointer slot)
	{
		doJVMTIObjectTagSlot(slot, VoidPointer.NULL);
	}

	@Override
	protected void doMonitorReference(J9ObjectMonitorPointer slot)
	{
		doMonitorReference(slot, VoidPointer.NULL);
	}
	
	@Override
	protected void doMonitorLookupCacheSlot(J9ObjectMonitorPointer slot)
	{
		doMonitorLookupCacheSlot(slot, ObjectMonitorReferencePointer.NULL);
	}

	@Override
	protected void doPhantomReferenceSlot(J9ObjectPointer slot)
	{
		doPhantomReferenceSlot(slot, VoidPointer.NULL);
	}

	@Override
	protected void doRememberedSlot(J9ObjectPointer slot)
	{
		doRememberedSlot(slot, VoidPointer.NULL);
	}

	@Override
	protected void doSoftReferenceSlot(J9ObjectPointer slot)
	{
		doSoftReferenceSlot(slot, VoidPointer.NULL);
	}

	@Override
	protected void doStringCacheTableSlot(J9ObjectPointer slot)
	{
		doStringCacheTableSlot(slot, VoidPointer.NULL);
	}
	
	@Override
	protected void doStringTableSlot(J9ObjectPointer slot)
	{
		doStringTableSlot(slot, VoidPointer.NULL);
	}

	@Override
	protected void doUnfinalizedObject(J9ObjectPointer slot)
	{
		doUnfinalizedObject(slot, VoidPointer.NULL);
	}
	
	@Override
	protected void doOwnableSynchronizerObject(J9ObjectPointer slot)
	{
		doOwnableSynchronizerObject(slot, VoidPointer.NULL);
	}

	@Override
	protected void doVMClassSlot(J9ClassPointer slot)
	{
		doVMClassSlot(slot, VoidPointer.NULL);
	}

	@Override
	protected void doVMThreadJNISlot(J9ObjectPointer slot)
	{
		doVMThreadJNISlot(slot, VoidPointer.NULL);
	}


	@Override
	protected void doVMThreadMonitorRecordSlot(J9ObjectPointer slot)
	{
		doVMThreadMonitorRecordSlot(slot, VoidPointer.NULL);
	}
	
	@Override
	protected void doNonCollectableObjectSlot(J9ObjectPointer slot)
	{
		doNonCollectableObjectSlot(slot, VoidPointer.NULL);
	}


	@Override
	protected void doMemorySpaceSlot(J9ObjectPointer slot)
	{
		doMemoryAreaSlot(slot, VoidPointer.NULL);
	}

	@Override
	protected void doVMThreadSlot(J9ObjectPointer slot)
	{
		doVMThreadSlot(slot, VoidPointer.NULL);
	}
	
	@Override
	protected void doWeakReferenceSlot(J9ObjectPointer slot)
	{
		doWeakReferenceSlot(slot, VoidPointer.NULL);
	}
	
	@Override
	protected void doStackSlot(J9ObjectPointer slot)
	{
		doStackSlot(slot, new WalkState(), VoidPointer.NULL);
	}
}
