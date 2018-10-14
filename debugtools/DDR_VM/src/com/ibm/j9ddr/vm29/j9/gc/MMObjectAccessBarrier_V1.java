/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.ObjectModel;
import com.ibm.j9ddr.vm29.j9.ObjectMonitor;
import com.ibm.j9ddr.vm29.pointer.ObjectReferencePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectMonitorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.types.I32;
import com.ibm.j9ddr.vm29.types.UDATA;
import java.lang.reflect.InvocationTargetException;

class MMObjectAccessBarrier_V1 extends MMObjectAccessBarrier
{
	private int shift;
	
	protected MMObjectAccessBarrier_V1() throws CorruptDataException 

	{
		shift = 0;
		
		if(J9BuildFlags.gc_compressedPointers) {
			try {
				J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());

				/* use reflection to access compressedPointersShift which will not exist if build is default */
				UDATA shiftUdata = (UDATA)J9JavaVMPointer.class.getMethod("compressedPointersShift").invoke(vm); //$NON-NLS-1$
				shift = shiftUdata.intValue();
			} catch (CorruptDataException cde) {
				raiseCorruptDataEvent("Error initializing the object access barrier", cde, true);
			} catch (IllegalAccessException re) {
				/* error caused by reflection */
				CorruptDataException rcde = new CorruptDataException(re.toString(), re);
				raiseCorruptDataEvent("Error retrieving compressedPointersShift", rcde, true);
			} catch (InvocationTargetException re) {
				/* error caused by reflection */
				CorruptDataException rcde = new CorruptDataException(re.toString(), re);
				raiseCorruptDataEvent("Error retrieving compressedPointersShift", rcde, true);
			} catch (NoSuchMethodException re) {
				/* error caused by reflection */
				CorruptDataException rcde = new CorruptDataException(re.toString(), re);
				raiseCorruptDataEvent("Error retrieving compressedPointersShift", rcde, true);
			} catch (SecurityException re) {
				/* error caused by reflection */
				CorruptDataException rcde = new CorruptDataException(re.toString(), re);
				raiseCorruptDataEvent("Error retrieving compressedPointersShift", rcde, true);
			}
		}
	}
	
	/**
	 * @see MMObjectAccessBarrier
	 */
	@Override
	public J9ObjectPointer getFinalizeLink(J9ObjectPointer object) throws CorruptDataException
	{
		UDATA fieldOffset = J9ObjectHelper.clazz(object).finalizeLinkOffset();
		if(fieldOffset.eq(0)) {
			return J9ObjectPointer.NULL;
		}
		return ObjectReferencePointer.cast(object.addOffset(fieldOffset)).at(0);
	}
	
	@Override
	public J9ObjectPointer getOwnableSynchronizerLink(J9ObjectPointer object) throws CorruptDataException
	{
		UDATA fieldOffset = getExtensions().accessBarrier()._ownableSynchronizerLinkOffset();
		J9ObjectPointer next = ObjectReferencePointer.cast(object.addOffset(fieldOffset)).at(0);
		if (object.equals(next)) {
			return J9ObjectPointer.NULL;
		}
		return next;
	}

	@Override
	public J9ObjectPointer  isObjectInOwnableSynchronizerList(J9ObjectPointer object) throws CorruptDataException
	{
		UDATA fieldOffset = getExtensions().accessBarrier()._ownableSynchronizerLinkOffset();
		return ObjectReferencePointer.cast(object.addOffset(fieldOffset)).at(0);
	}
	
	/**
	 * @see MMObjectAccessBarrier
	 */
	@Override
	public J9ObjectPointer getReferenceLink(J9ObjectPointer object) throws CorruptDataException
	{
		UDATA linkOffset = getExtensions().accessBarrier()._referenceLinkOffset();
		return ObjectReferencePointer.cast(object.addOffset(linkOffset)).at(0); 
	}

	/**
	 * @see MMObjectAccessBarrier
	 */
	@Override
	public I32 getObjectHashCode(J9ObjectPointer object) throws CorruptDataException
	{
		return ObjectModel.getObjectHashCode(object);
	}

	/**
	 * @see MMObjectAccessBarrier
	 */
	@Override
	public J9ObjectMonitorPointer getLockword(J9ObjectPointer object) throws CorruptDataException
	{
		return getMonitor(object).getLockword();
	}
	
	/**
	 * @see MMObjectAccessBarrier
	 */
	@Override
	public ObjectMonitor getMonitor(J9ObjectPointer object) throws CorruptDataException
	{
		return ObjectMonitor.fromJ9Object(object);
	}

	/**
	 * @see MMObjectAccessBarrier
	 */
	@Override
	public J9ObjectPointer convertPointerFromToken(long token)
	{
		if(token == 0) {
			return J9ObjectPointer.NULL;
		}
		if(J9BuildFlags.gc_compressedPointers) {
			UDATA ref = new UDATA(token);
			ref = ref.leftShift(shift);
			return J9ObjectPointer.cast(ref);
		} else {
			return J9ObjectPointer.cast(token);
		}
	}

	/**
	 * @see MMObjectAccessBarrier
	 */
	@Override
	public long convertTokenFromPointer(J9ObjectPointer pointer)
	{
		if(pointer.isNull()) {
			return 0L;
		}
		UDATA address = UDATA.cast(pointer);
		if(J9BuildFlags.gc_compressedPointers) {
			address = address.rightShift(shift);
		}
		return address.longValue();
	}
}
