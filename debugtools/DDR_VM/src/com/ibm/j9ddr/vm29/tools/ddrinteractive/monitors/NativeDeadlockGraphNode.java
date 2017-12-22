/*******************************************************************************
 * Copyright (c) 2014, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.tools.ddrinteractive.monitors;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.generated.J9ThreadMonitorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ThreadPointer;

public class NativeDeadlockGraphNode
{
	public J9ThreadPointer nativeThread = null;
	public NativeDeadlockGraphNode next = null;
	public J9ThreadMonitorPointer nativeLock = null;
	public int cycle = 0;
	
	public int hashCode()
	{
		return nativeThread.hashCode();
	}
	
	public boolean equals(Object obj)
	{
		if (null == obj)
			return false;
		if (this == obj)
			return true;
		if (false == (obj instanceof NativeDeadlockGraphNode))
			return false;
		
		NativeDeadlockGraphNode other = (NativeDeadlockGraphNode) obj;
		return other.nativeThread.equals(this.nativeThread);
	}
	
	public String toString()
	{
		String retVal = "OS Thread:\t";
		try {
			retVal += nativeThread.formatShortInteractive() + 
				String.format("\ttid 0x%x (%d)",
						nativeThread.tid().longValue(),
						nativeThread.tid().longValue());
		} catch (CorruptDataException e) {
			retVal += "<Corrupted>";
		}
		return retVal;
	}
	
}
