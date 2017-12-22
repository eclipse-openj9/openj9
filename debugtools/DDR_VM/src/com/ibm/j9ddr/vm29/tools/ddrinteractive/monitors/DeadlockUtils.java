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

import static com.ibm.j9ddr.vm29.structure.J9VMThread.*;
import static com.ibm.j9ddr.vm29.structure.J9AbstractThread.*;

import java.io.PrintStream;
import java.util.HashMap;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.J9VMThreadPointerUtil;
import com.ibm.j9ddr.vm29.j9.J9VMThreadPointerUtil.ThreadInfo;
import com.ibm.j9ddr.vm29.j9.ObjectMonitor;
import com.ibm.j9ddr.vm29.j9.walkers.MonitorIterator;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectMonitorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ThreadAbstractMonitorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ThreadMonitorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ThreadPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ThreadHelper;
import com.ibm.j9ddr.vm29.types.UDATA;

public class DeadlockUtils
{
	/**
	 * 
	 * @param vmThread
	 * @param map
	 * @throws CorruptDataException 
	 */
	public static void findThreadCycle(J9ThreadPointer aThread, HashMap<Integer,NativeDeadlockGraphNode> deadlocks, 
									HashMap<J9ObjectPointer, Object> objectMonitorsMap) throws CorruptDataException
	{
		// Based on JavaCoreDumpWriter::findThreadCycle()
		
		// Can't stack allocate or (safely) re-purpose an object in Java,
		// so we create a new one on each iteration, this is slightly 
		// different from what is done in javadump.cpp
		NativeDeadlockGraphNode node = null;
		NativeDeadlockGraphNode prev = null;
		
		do {
			// Is there an associated J9VMThread?
			J9VMThreadPointer vmThread = J9ThreadHelper.getVMThread(aThread);
			boolean isJavaThread = (null != vmThread) && vmThread.notNull();
			boolean isJavaWaiter = false; // Enter waiter (blocked) or notify waiter.
			
			if (isJavaThread) {
				// There is, so grab all the J9VMThread related information.
				JavaDeadlockGraphNode javaNode = new JavaDeadlockGraphNode();
				javaNode.javaThread = vmThread;
				
				J9VMThreadPointer owner = null;
				J9ObjectPointer lockObject = null;
				J9ThreadAbstractMonitorPointer lock = null;
				
				// Functionally equivalent to getVMThreadStateHelper(...) in thrinfo.c
				lockObject = vmThread.blockingEnterObject();
				
				if ((null != lockObject) && lockObject.notNull()) {
					
					// This condition hitting means we are in one of the desired states:
					// J9VMTHREAD_STATE_BLOCKED
					// J9VMTHREAD_STATE_WAITING, J9VMTHREAD_STATE_WAITING_TIMED
					// J9VMTHREAD_STATE_PARKED, J9VMTHREAD_STATE_PARKED_TIMED
					
					isJavaWaiter = true;
					
					// The original native relies on getVMThreadStateHelper(...) 
					// from thrinfo.c to obtain this information, which is analogous 
					// to J9VMThreadPointerUtil.getJ9State(...), but we don't actually
					// need all that information.
					Object current = objectMonitorsMap.get(lockObject);
					if (current instanceof ObjectMonitor) {
						ObjectMonitor mon = (ObjectMonitor) current;
						owner = mon.getOwner();
						lock = mon.getInflatedMonitor();
					} else if (current instanceof J9ThreadMonitorPointer) { // System Monitor
						J9ThreadMonitorPointer mon = (J9ThreadMonitorPointer) current;
						J9ThreadPointer sysThread = mon.owner();
						lock = J9ThreadAbstractMonitorPointer.cast(mon);
						if (sysThread.notNull()) {
							owner = J9ThreadHelper.getVMThread(sysThread);
						}
					}
					// Finished obtaining required thread info.
					
					if ((null == owner) || owner.isNull() || (owner.equals(vmThread))) {
						// See JAZZ103 63676: There are cases where even if we know there is no 
						// Java deadlock, we should still check for a system deadlock.
						// We simply deal with this node as a native one from this point on.
						isJavaWaiter = false;
					} else {
						// N.B. In the native there are comparisons with J9VMTHREAD_STATE_*
						// constants as returned by the helper, but here we don't actually need them.
						javaNode.javaLock = lock;
						javaNode.lockObject = lockObject;
						javaNode.cycle = 0;
						
						/* Record current thread and update last node */
						javaNode.nativeThread = vmThread.osThread();
						deadlocks.put(javaNode.hashCode(), javaNode);
						if (null != prev) {
							prev.next = javaNode;
						}
						
						/* Move around the graph */
						prev = javaNode;
						
						/* Peek ahead to see if we're in a possible cycle */
						prev.next = deadlocks.get(owner.osThread().hashCode());
						aThread = owner.osThread();
					}
				}

				// Have to keep going for the case of Java thread blocking on system monitor.
				node = javaNode; // We have to write the J9Thread info as well.
				
			} 
			
			if (false == isJavaThread) {
				node = new NativeDeadlockGraphNode();
			}
			
			// Now get all of the J9Thread fields (even when we are working with a Java thread).	
			node.nativeThread = aThread;
			J9ThreadMonitorPointer nativeLock = aThread.monitor();
			
			J9ThreadPointer owner = null;
			
			if ((null == nativeLock) || nativeLock.isNull()) {
				if (false == isJavaThread) {
					return;
				}
			} else {
				node.nativeLock = nativeLock;
				owner = nativeLock.owner();
			}
			
			// If this was a Java (J9VMThread) then we don't want
			// to change the pointers, otherwise we have to set them up.
			if (false == isJavaWaiter) {
				
				deadlocks.put(node.hashCode(), node);
				if (null != prev) {
					prev.next = node;
				}
				
				if ((null == owner) || owner.isNull() || (owner.equals(aThread))) {
					return;
				} else {
					
					/* Move around the graph */
					prev = node;
					
					/* Peek ahead to see if we're in a possible cycle */
					prev.next = deadlocks.get(owner.hashCode());
					aThread = owner;
				}
				
			}
			
		} while (null == prev.next); // Quit as soon as possible (first node we've already visited).
		// The rest of the cycles will be found by multiple calls to findThreadCycle(...)
	}
	
	
	/**
	 * '
	 * @param node
	 * @param isCycleHead
	 * @param objectMonitorsMap
	 * @param out
	 * @throws CorruptDataException
	 */
	public static void writeDeadlockNode(NativeDeadlockGraphNode node, boolean isCycleHead, 
			HashMap<J9ObjectPointer, Object> objectMonitorsMap, PrintStream out) throws CorruptDataException
	{
		// Based on JavaCoreDumpWriter::writeDeadlockNode(...)
		out.println(node.toString());
		
		String stateMsg = getThreadStateDescription(node);
		
		if (isCycleHead) {
			out.println("\tis " + stateMsg);
		} else {
			out.println("\twhich is " + stateMsg);
		}
		// A bit of ugliness since Java resolves the methods by parameter type at compile time.
		if (node instanceof JavaDeadlockGraphNode) {
			DeadlockUtils.writeJavaDeadlockNode( (JavaDeadlockGraphNode) node, isCycleHead, objectMonitorsMap, out);
		} else {
			DeadlockUtils.writeNativeDeadlockNode( node, isCycleHead, objectMonitorsMap, out);
		}	
	}
	
	/**
	 * Helper to obtain a textual description for a given thread state.
	 * @param node
	 * @return
	 * @throws CorruptDataException
	 */
	private static String getThreadStateDescription(NativeDeadlockGraphNode node) throws CorruptDataException 
	{
		
		String retVal = "unknown state for:";

		/*
		 * Blocked and Waiting are meaningful terms for Java
		 * Threads and should not be used interchangeably.
		 */
		
		if (node instanceof JavaDeadlockGraphNode) {
			
			JavaDeadlockGraphNode javaNode = (JavaDeadlockGraphNode) node;
			ThreadInfo thrInfo = J9VMThreadPointerUtil.getJ9State(javaNode.javaThread);
			UDATA state = new UDATA(thrInfo.getState());
			
			//TODO What about J9VMTHREAD_STATE_PARKED | J9VMTHREAD_STATE_PARKED_TIMED ? 
			
			if (state.allBitsIn(J9VMTHREAD_STATE_WAITING) || state.allBitsIn(J9VMTHREAD_STATE_WAITING_TIMED)) {
				retVal = "waiting for:";
			} 
			else if (state.allBitsIn(J9VMTHREAD_STATE_BLOCKED)) {
				retVal = "blocking on:";
			}

		} else { // Native thread
			
			UDATA flags = node.nativeThread.flags();
			
			if (flags.allBitsIn(J9THREAD_FLAG_BLOCKED)) {
				retVal = "blocking on:";
			}
			else if (flags.allBitsIn(J9THREAD_FLAG_WAITING)) {
				retVal = "waiting for";
			}
		}
		
		return retVal;
	}
	
	/**
	 * 
	 * @param node
	 * @param isCycleHead
	 * @throws CorruptDataException 
	 */
	private static void writeJavaDeadlockNode(JavaDeadlockGraphNode node, boolean isCycleHead, 
					HashMap<J9ObjectPointer, Object> objectMonitorsMap, PrintStream out) throws CorruptDataException
	{
		// Based on JavaCoreDumpWriter::writeSystemMonitor()
		//			JavaCoreDumpWriter::writeMonitor()
		//			JavaCoreDumpWriter::writeMonitorObject()
		
		if (null != node.javaLock) {
			String monitorName = "[system]";
			if (node.javaLock.name().notNull()) {
				monitorName = node.javaLock.name().getCStringAtOffset(0);
			}
			out.print("\t\t" + monitorName);
			out.println(" lock (" + node.javaLock.formatShortInteractive() + ")");
		}
		else if ((null == node.javaLock) && (null != node.lockObject)) {
			
			String className = J9ObjectHelper.getClassName(node.lockObject);
			out.format("\t\t%s\t\"%s\"\n",node.lockObject.formatShortInteractive(), className);
			ObjectMonitor objMon = (ObjectMonitor) objectMonitorsMap.get(node.lockObject);
			J9ObjectMonitorPointer j9objMonPtr = objMon.getJ9ObjectMonitorPointer();
			
			if ((null != j9objMonPtr) && (j9objMonPtr.notNull())) {
				out.print(String.format("\t\t%s\n", j9objMonPtr.formatShortInteractive()));
				out.print(String.format("\t\t%s\n", j9objMonPtr.monitor().formatShortInteractive()));
				out.print(String.format("\t\t%s\n", objMon.getOwner().osThread().formatShortInteractive()));
			}
		}
		else if (null != node.nativeLock) { // Case of a Java thread blocking on a system monitor		
			printNativeLockHelper(node.nativeLock, out);
		}
		
		out.println("\twhich is owned by:");
	}
	
	/**
	 * 
	 * @param node
	 * @param isCycleHead
	 * @param objectMonitorsMap
	 * @param out
	 * @throws CorruptDataException 
	 */
	private static void writeNativeDeadlockNode(NativeDeadlockGraphNode node, boolean isCycleHead, 
			HashMap<J9ObjectPointer, Object> objectMonitorsMap, PrintStream out) throws CorruptDataException
	{
		printNativeLockHelper(node.nativeLock, out);
		out.println("\twhich is owned by:");
	}
	
	/**
	 * Helper to print out native monitor information.
	 * @param nativeLock
	 * @param out
	 * @throws CorruptDataException
	 */
	private static void printNativeLockHelper(J9ThreadMonitorPointer nativeLock, PrintStream out) throws CorruptDataException {
		String monitorName = "[system]";
		if (nativeLock.name().notNull()) {
			monitorName = nativeLock.name().getCStringAtOffset(0);
		}
		out.print("\t\t" + monitorName);
		out.println(" lock (" + nativeLock.formatShortInteractive() + ")");
	}
	
	/**
	 * Returns a hash map of Object Pointers to their respective mutex (Object Monitor or System Monitor)
	 * @param vm
	 * @return
	 * @throws CorruptDataException
	 */
	public static HashMap<J9ObjectPointer, Object> readObjectMonitors(J9JavaVMPointer vm) throws CorruptDataException
	{
		HashMap<J9ObjectPointer, Object> objectMonitorsMap = new HashMap<J9ObjectPointer, Object>();
		MonitorIterator iterator = new MonitorIterator(vm);
		
		while (iterator.hasNext()) {
			Object current = iterator.next();
			if (current instanceof ObjectMonitor) {
				ObjectMonitor mon = (ObjectMonitor) current;
				J9ObjectPointer object = mon.getObject();
				objectMonitorsMap.put(object, mon);
			} else if (current instanceof J9ThreadMonitorPointer) { // System Monitor
				J9ThreadMonitorPointer mon = (J9ThreadMonitorPointer) current;
				J9ThreadAbstractMonitorPointer lock = J9ThreadAbstractMonitorPointer.cast(mon);
				if (false == lock.userData().eq(0)) {// this is an object monitor in the system monitor table
					J9ObjectPointer object = J9ObjectPointer.cast(lock.userData());
					objectMonitorsMap.put(object, mon);
				}
			}
		}
		return objectMonitorsMap;
	}


}
