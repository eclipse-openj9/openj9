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

import java.util.LinkedList;
import java.util.List;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.generated.J9ThreadMonitorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ThreadPointer;

public class SystemMonitorThreeTier_V1 extends SystemMonitor
{
	/* Do not instantiate. Use the factory */
	protected SystemMonitorThreeTier_V1(J9ThreadMonitorPointer monitor)
	{
		this.monitor = monitor;
	}

	@Override
	public boolean isContended() throws CorruptDataException
	{
		J9ThreadPointer blocking = J9ThreadMonitorHelper.getBlockingField(monitor);
		return blocking.notNull();
	}

	private static List<J9ThreadPointer> threadListHelper(J9ThreadPointer thread) throws CorruptDataException
	{
		LinkedList<J9ThreadPointer> threadList = new LinkedList<J9ThreadPointer>();
		while (thread.notNull()) {
			threadList.add(thread);
			thread = thread.next();
		}
		return threadList;
	}
	
	@Override
	public List<J9ThreadPointer> getWaitingThreads() throws CorruptDataException
	{
		return threadListHelper(monitor.waiting());
	}

	@Override
	public List<J9ThreadPointer> getBlockedThreads() throws CorruptDataException
	{
		J9ThreadPointer blocking = J9ThreadMonitorHelper.getBlockingField(monitor);
		return threadListHelper(blocking);
	}
}
