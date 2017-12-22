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

import java.util.List;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectMonitorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ThreadAbstractMonitorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;

public abstract class ObjectMonitor implements Comparable<ObjectMonitor>
{

	/**
	 * Return an ObjectMonitor representing the monitor for the given object, or null if it does not have a lockword.
	 * 
	 * @param object the object to read
	 * 
	 * @return the ObjectMonitor corresponding to the object
	 */
	public static ObjectMonitor fromJ9Object(J9ObjectPointer object) throws CorruptDataException
	{
		AlgorithmVersion version = AlgorithmVersion.getVersionOf(AlgorithmVersion.OBJECT_MONITOR_VERSION);
		switch (version.getAlgorithmVersion()) {
			// Add case statements for new algorithm versions
			default:
				ObjectMonitor_V1 v1 = new ObjectMonitor_V1(object);
				if(v1.getLockword().isNull() && !v1.isInTable()) {
					return null;
				} else {
					return v1;
				}
		}
	}
	
	@Override
	public boolean equals(Object object) {
		return this.compareTo((ObjectMonitor)object) == 0 ? true : false;
	}

	public abstract J9ObjectPointer getObject();
	
	public abstract J9ObjectMonitorPointer getLockword();
	
	public abstract J9VMThreadPointer getOwner() throws CorruptDataException;

	public abstract long getCount() throws CorruptDataException;

	public abstract boolean isInflated();

	public abstract boolean isInTable();
	
	public abstract boolean isContended() throws CorruptDataException;
	
	public abstract List<J9VMThreadPointer> getWaitingThreads() throws CorruptDataException;

	public abstract List<J9VMThreadPointer> getBlockedThreads() throws CorruptDataException;
	
	public abstract J9ThreadAbstractMonitorPointer getInflatedMonitor();

	public abstract J9ObjectMonitorPointer getJ9ObjectMonitorPointer();
	
}
