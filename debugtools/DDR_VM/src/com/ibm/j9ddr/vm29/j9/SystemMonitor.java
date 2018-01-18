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
package com.ibm.j9ddr.vm29.j9;

import java.util.List;
import com.ibm.j9ddr.CorruptDataException;

import com.ibm.j9ddr.vm29.pointer.generated.J9ThreadMonitorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ThreadPointer;
import com.ibm.j9ddr.vm29.pointer.generated.OmrBuildFlags;

public abstract class SystemMonitor implements Comparable<SystemMonitor>
{
	protected J9ThreadMonitorPointer monitor;
	
	public static SystemMonitor fromJ9ThreadMonitor(J9ThreadMonitorPointer monitor) throws CorruptDataException
	{
		AlgorithmVersion version = AlgorithmVersion.getVersionOf(AlgorithmVersion.OBJECT_MONITOR_VERSION);
		switch (version.getAlgorithmVersion()) {
			// Add case statements for new algorithm versions
			default:
				// N.B. The two types are functionally equivalent
				// but the SingleTier logic is more complicated
				// due to missing fields in the monitor objects.
				if (OmrBuildFlags.OMR_THR_THREE_TIER_LOCKING) {
					return new SystemMonitorThreeTier_V1(monitor);
				} else {
					return new SystemMonitorSingleTier_V1(monitor);
				}
		}
	}
	
	public J9ThreadMonitorPointer getRawMonitor()
	{
		return monitor;
	}
	
	public int compareTo(SystemMonitor other)
	{
		return monitor.compare(other.monitor);
	}
	
	@Override
	public boolean equals(Object object) {
		return this.compareTo((SystemMonitor)object) == 0 ? true : false;
	}
	
	@Override
	public int hashCode()
	{
		return monitor.hashCode();
	}
	
	public J9ThreadPointer getOwner() throws CorruptDataException
	{
		return monitor.owner();
	}
	
	public String getName()
	{
		String monitorName = "<unnamed monitor>";
		try {
			if (monitor.name().notNull()) {
				monitorName = String.format("\"%s\"", monitor.name().getCStringAtOffset(0));
			}
		} catch (CorruptDataException e) {
			monitorName = "<FAULT>";
		}
		return monitorName;
	}

	public long getCount() throws CorruptDataException
	{
		return monitor.count().longValue();
	}
	
	public abstract boolean isContended() throws CorruptDataException;

	public abstract List<J9ThreadPointer> getWaitingThreads() throws CorruptDataException;

	public abstract List<J9ThreadPointer> getBlockedThreads() throws CorruptDataException;
	
}
