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

import java.util.Iterator;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.Pool;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JVMTIDataPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JVMTIEnvPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9PoolPointer;

public class GCJVMTIObjectTagTableListIterator extends GCIterator
{
	protected Iterator<J9JVMTIEnvPointer> poolIterator;
	
	protected GCJVMTIObjectTagTableListIterator(J9PoolPointer environments) throws CorruptDataException
	{
		poolIterator = Pool.fromJ9Pool(environments, J9JVMTIEnvPointer.class).iterator();
	}

	public static GCJVMTIObjectTagTableListIterator fromJ9JVMTIData(J9JVMTIDataPointer jvmtiData) throws CorruptDataException
	{
		return new GCJVMTIObjectTagTableListIterator(jvmtiData.environments());
	}

	public boolean hasNext()
	{
		return poolIterator.hasNext();
	}

	public J9JVMTIEnvPointer next()
	{
		return poolIterator.next();
	}
	
	public VoidPointer nextAddress() 
	{
		// Does not make sense in this case
		throw new UnsupportedOperationException("This iterator cannot return addresses");
	}
}
