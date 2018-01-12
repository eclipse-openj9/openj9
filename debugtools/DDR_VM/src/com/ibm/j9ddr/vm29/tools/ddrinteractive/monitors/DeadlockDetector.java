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
package com.ibm.j9ddr.vm29.tools.ddrinteractive.monitors;

import java.io.PrintStream;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.Pool;
import com.ibm.j9ddr.vm29.j9.SlotIterator;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9PoolPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ThreadLibraryPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ThreadPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ThreadHelper;
import com.ibm.j9ddr.vm29.structure.J9Consts;

public class DeadlockDetector
{
	/**
	 * 
	 * @param out
	 * @throws DDRInteractiveCommandException
	 */
	public static void findDeadlocks(PrintStream out) throws DDRInteractiveCommandException
	{
		
		// Based on JavaCoreDumpWriter::writeDeadLocks()
		// Modified to work for both J9VMThreads and J9Threads.
		
		HashMap<Integer, NativeDeadlockGraphNode> map = new HashMap<Integer, NativeDeadlockGraphNode>();
		try {
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			
			HashMap<J9ObjectPointer, Object> objectMonitorsMap = DeadlockUtils.readObjectMonitors(vm);
			
			J9ThreadLibraryPointer lib = vm.mainThread().osThread().library();
			J9PoolPointer pool = lib.thread_pool();
			Pool<J9ThreadPointer> threadPool = Pool.fromJ9Pool(pool, J9ThreadPointer.class);
			SlotIterator<J9ThreadPointer> poolIterator = threadPool.iterator();
			J9ThreadPointer osThreadPtr = null;
			
			while (poolIterator.hasNext()) {
				osThreadPtr = poolIterator.next();
				DeadlockUtils.findThreadCycle(osThreadPtr, map, objectMonitorsMap);
				
				// Is there an associated J9VMThread?
				J9VMThreadPointer vmThread = J9ThreadHelper.getVMThread(osThreadPtr);
				if ((null != vmThread) && (vmThread.notNull()) && vmThread.publicFlags().allBitsIn(J9Consts.J9_PUBLIC_FLAGS_HALT_THREAD_INSPECTION)) {
					break;
				}
			}
			
			int cycle = 0; // Identifier for each (independent) cycle.
			
			Iterator<Map.Entry<Integer, NativeDeadlockGraphNode>> iterator = map.entrySet().iterator();
			while (iterator.hasNext()) {
				
				NativeDeadlockGraphNode node = iterator.next().getValue();
				cycle++;
				
				while (null != node) {
					
					if (node.cycle > 0) {
						// Found a deadlock!
						// i.e. we have already visited this node in *this* cycle, so
						// it means we've looped around!
						if (node.cycle == cycle) {
							// Output header for each deadlock:
							out.println("Deadlock Detected !!!");
							out.println("---------------------");
							out.println();
							
							NativeDeadlockGraphNode head = node;
							boolean isCycleRoot = true;
							
							do {					
								DeadlockUtils.writeDeadlockNode(node, isCycleRoot, objectMonitorsMap, out);
								node = node.next;
								isCycleRoot = false;
							} while (node != head);
							
							out.println(node.toString());
						}
						
						// Skip already visited nodes
						break;	
						
					} else {
						node.cycle = cycle;
					}
					node = node.next;
				}
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}
	
}
