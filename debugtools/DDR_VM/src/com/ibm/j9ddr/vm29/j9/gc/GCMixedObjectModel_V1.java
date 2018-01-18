/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.structure.J9Object;
import com.ibm.j9ddr.vm29.types.UDATA;

class GCMixedObjectModel_V1 extends GCMixedObjectModel 
{
	public GCMixedObjectModel_V1() throws CorruptDataException 
	{
	}	

	@Override
	public UDATA getSizeInBytesWithoutHeader(J9ObjectPointer object) throws CorruptDataException 
	{
		return getSizeInBytesWithoutHeader(J9ObjectHelper.clazz(object));
	}

	@Override
	public UDATA getSizeInBytesWithoutHeader(J9ClassPointer clazz) throws CorruptDataException 
	{		
		return clazz.totalInstanceSize();
	}

	@Override
	public UDATA getHashcodeOffset(J9ObjectPointer object) throws CorruptDataException 
	{
		return getHashcodeOffset(J9ObjectHelper.clazz(object));
	}
	
	@Override
	public UDATA getHashcodeOffset(J9ClassPointer clazz) throws CorruptDataException
	{
		return new UDATA(clazz.backfillOffset());
	}

	@Override
	public UDATA getHeaderSize(J9ObjectPointer object) throws CorruptDataException
	{		
		return new UDATA(J9Object.SIZEOF);
	}
}
