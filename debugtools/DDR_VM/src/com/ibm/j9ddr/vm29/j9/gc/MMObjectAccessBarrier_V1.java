/*
 * Copyright IBM Corp. and others 2001
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
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
import com.ibm.j9ddr.vm29.types.U32;
import com.ibm.j9ddr.vm29.types.UDATA;
import java.lang.reflect.InvocationTargetException;

class MMObjectAccessBarrier_V1 extends MMObjectAccessBarrier
{
	private int shift;

	protected MMObjectAccessBarrier_V1() throws CorruptDataException
	{
		if (J9ObjectHelper.compressObjectReferences) {
			try {
				J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
				shift = vm.compressedPointersShift().intValue();
			} catch (NoSuchFieldException re) {
				shift = 0;
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
		if (fieldOffset.eq(0)) {
			return J9ObjectPointer.NULL;
		}
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
		if (token == 0) {
			return J9ObjectPointer.NULL;
		}
		if (J9ObjectHelper.compressObjectReferences) {
			UDATA ref = new UDATA(token & U32.MASK);
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
		if (pointer.isNull()) {
			return 0L;
		}
		UDATA address = UDATA.cast(pointer);
		if (J9ObjectHelper.compressObjectReferences) {
			address = address.rightShift(shift);
		}
		return address.longValue();
	}
}
