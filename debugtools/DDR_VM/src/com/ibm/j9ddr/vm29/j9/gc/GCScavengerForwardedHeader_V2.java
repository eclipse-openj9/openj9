/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.pointer.U32Pointer;
import com.ibm.j9ddr.vm29.pointer.UDATAPointer;
import com.ibm.j9ddr.vm29.structure.MM_ForwardedHeader;
import com.ibm.j9ddr.vm29.types.U32;
import com.ibm.j9ddr.vm29.types.UDATA;

class GCScavengerForwardedHeader_V2 extends GCScavengerForwardedHeader_V1
{
	/* combine the flags into one mask which should be stripped from the pointer in order to remove all tags */
	private static final int ALL_TAGS = (int)MM_ForwardedHeader.OMR_FORWARDED_TAG_MASK;

	/* Do not instantiate. Use the factory */
	protected GCScavengerForwardedHeader_V2(J9ObjectPointer object)
	{
		super(object);
	}

	@Override
	protected J9ObjectPointer getForwardedObjectNoCheck() throws CorruptDataException
	{
		if(J9ObjectHelper.compressObjectReferences && !J9BuildFlags.env_littleEndian) {
			/* compressed big endian - read two halves separately */
			U32 low = U32Pointer.cast(objectPointer.clazzEA()).at(0).bitAnd(~ALL_TAGS);
			U32 high = U32Pointer.cast(objectPointer.clazzEA()).at(1);
			J9ObjectPointer forwardedObject = J9ObjectPointer.cast(new UDATA(low).bitOr(new UDATA(high).leftShift(32)));
			return forwardedObject;
		} else {
			/* Little endian or not compressed - read all UDATA bytes at once */
			J9ObjectPointer forwardedObject = J9ObjectPointer.cast(UDATAPointer.cast(objectPointer.clazzEA()).at(0));
			return forwardedObject.untag(ALL_TAGS);
		}
	}

	@Override
	public boolean isForwardedPointer() throws CorruptDataException
	{
		return J9ObjectHelper.rawClazz(objectPointer).allBitsIn(MM_ForwardedHeader.OMR_FORWARDED_TAG);
	}
}
