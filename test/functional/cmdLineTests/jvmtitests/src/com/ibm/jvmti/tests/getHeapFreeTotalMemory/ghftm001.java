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
package com.ibm.jvmti.tests.getHeapFreeTotalMemory;

public class ghftm001 
{
	native static long getHeapFreeMemory();
	native static long getHeapTotalMemory();
	native static int getCycleStartCount();
	native static int getCycleEndCount();
	
	private final static int MAX_ALLOC_MBYTES = 2 * 1024; /* 2 GB */
	private final static int MAX_GC_CYCLE_SECONDS = 30;
	
	public boolean testGetHeapFreeTotalMemory()
	{
		long heapTotal = getHeapTotalMemory();
		long heapFree = getHeapFreeMemory();
		if (heapTotal == -1 || heapFree == -1) {
			System.out.println("Error: getHeapTotalMemory() returned " + heapTotal + " getHeapFreeMemory() returned " + heapFree);
			return false;
		}
		
		if (heapFree > heapTotal) {
			System.out.println("Error: heapFree > heapTotal: " + heapFree + " > " + heapTotal);
			return false;
		}
		
		return true;
	}
	
	public boolean testGarbageCollectionCycleEvent()
	{
		int allocatedMbytes = 0;
		int sleptSeconds = 0;
		while (true) {
			if (getCycleStartCount() != 0) {
				/* Cycle start event was received */
				if (getCycleEndCount() != 0) {
					/* Cycle end event was received... we're good */
					return true;
				} else if (sleptSeconds < MAX_GC_CYCLE_SECONDS) {
					/* GC Cycle is ongoing, let's sleep */
					try {
						Thread.sleep(1000);
						sleptSeconds += 1;
					} catch (InterruptedException e) {
						/* Do nothing */
					}
				} else {
					System.out.println("Error: GC Cycle end event not received within " + MAX_GC_CYCLE_SECONDS + "s");
					return false;
				}
			} else if (allocatedMbytes < MAX_ALLOC_MBYTES) {
				/* GC Cycle hasn't started, let's generate some garbage */
				char myArray[] = new char[1024*1024]; /* 1 MB */
				myArray[0] = 'A'; /* Just to suppress a warning */
				myArray = null;
				allocatedMbytes += 1;
			} else {
				System.out.println("Error: GC Cycle start event not received after allocating " + MAX_ALLOC_MBYTES +"MB");
				return false;
			}
		}
	}
	
	public String helpGetHeapFreeTotalMemory()
	{
		return "Tests calling the getHeapFree/TotalMemory through the JVMTI extensions";
	}
	
	public String helpGarbageCollectionCycleEvent()
	{
		return "Generates garbage until a cycle event has been sent to JVMTI, or until we give up";
	}
}
