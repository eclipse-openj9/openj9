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

import static com.ibm.j9ddr.vm29.structure.J9AbstractThread.*;

import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.types.UDATA;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9PoolPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ThreadLibraryPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ThreadMonitorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ThreadPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;

public class SystemMonitorSingleTier_V1 extends SystemMonitor
{
	/* 
	 * Certain platforms (e.g. AIX PPC) have thr_threeTierLocking disabled,
	 * so we cannot use the following fields of J9ThreadAbstractMonitor
	 * 		lockingWord
	 * 		spinCount1, spinCount2, spinCount3
	 * 		(See J9ThreadAbstractMonitor.st)
	 * 
	 * In this case we obtain the information by iterating over all threads.
	 * 
	 * */
	
	private final static HashMap<J9ThreadMonitorPointer, List<J9ThreadPointer>> blockedThreads = 
			new HashMap<J9ThreadMonitorPointer, List<J9ThreadPointer>>();
	
	private final static HashMap<J9ThreadMonitorPointer, List<J9ThreadPointer>> waitingThreads = 
			new HashMap<J9ThreadMonitorPointer, List<J9ThreadPointer>>();
	
	static
	{
		// Absorb this costly operation as a one time deal and cache the results.
		
		try {
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			J9VMThreadPointer mainThread = vm.mainThread();
			J9ThreadLibraryPointer lib = mainThread.osThread().library();
			J9PoolPointer pool = lib.thread_pool();
			Pool<J9ThreadPointer> threadPool = Pool.fromJ9Pool(pool, J9ThreadPointer.class);
			SlotIterator<J9ThreadPointer> poolIterator = threadPool.iterator();
			
			while (poolIterator.hasNext()) {
				J9ThreadPointer osThreadPtr = poolIterator.next();	
				J9ThreadMonitorPointer threadMon = osThreadPtr.monitor();
				UDATA flags = osThreadPtr.flags();
				
				if (threadMon.notNull()) {	
					if (flags.allBitsIn(J9THREAD_FLAG_BLOCKED)) {
						addToMap(blockedThreads, threadMon, osThreadPtr);
					}
					if (flags.allBitsIn(J9THREAD_FLAG_WAITING)) {
						addToMap(waitingThreads, threadMon, osThreadPtr);
					}
				}
			}
						
		} catch (CorruptDataException e) {
			// No sense in continuing, it will simply crash later making it more difficult to debug.
			throw new RuntimeException(e);
		}
	}
	
	private static void addToMap(HashMap<J9ThreadMonitorPointer, List<J9ThreadPointer>> map, 
		J9ThreadMonitorPointer monitor, J9ThreadPointer thread)
	{
		if(!map.containsKey(monitor)) {
			map.put(monitor, new LinkedList<J9ThreadPointer>());
		}
		map.get(monitor).add(thread);
	}
	
	/* Do not instantiate. Use the factory */
	protected SystemMonitorSingleTier_V1(J9ThreadMonitorPointer monitor)
	{
		this.monitor = monitor;
	}

	@Override
	public boolean isContended() throws CorruptDataException
	{
		return blockedThreads.containsKey(monitor);
	}

	private static List<J9ThreadPointer> threadListHelper(HashMap<J9ThreadMonitorPointer, List<J9ThreadPointer>> map, 
		J9ThreadMonitorPointer monitor) throws CorruptDataException
	{
		if(map.containsKey(monitor)) {
			return map.get(monitor);
		}
		
		return new LinkedList<J9ThreadPointer>();
	}
	
	@Override
	public List<J9ThreadPointer> getWaitingThreads() throws CorruptDataException
	{
		return threadListHelper(waitingThreads, monitor);
	}

	@Override
	public List<J9ThreadPointer> getBlockedThreads() throws CorruptDataException
	{
		return threadListHelper(blockedThreads, monitor);
	}

}
