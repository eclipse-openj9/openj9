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

import static com.ibm.j9ddr.vm29.structure.J9Object.*;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.pointer.helper.J9ObjectHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ThreadHelper;
import com.ibm.j9ddr.vm29.j9.gc.GCVMThreadListIterator;
import com.ibm.j9ddr.vm29.pointer.ObjectMonitorReferencePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectMonitorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ThreadAbstractMonitorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ThreadPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;
import com.ibm.j9ddr.vm29.pointer.generated.OmrBuildFlags;
import com.ibm.j9ddr.vm29.structure.J9Consts;
import com.ibm.j9ddr.vm29.types.IDATA;
import com.ibm.j9ddr.vm29.types.UDATA;

class ObjectMonitor_V1 extends ObjectMonitor
{	
	private static HashMap<J9ObjectPointer, List<J9VMThreadPointer>> blockedThreadsCache;
	
	private J9VMThreadPointer owner;
	private long count;
	private boolean isInflated;
	private J9ObjectMonitorPointer lockword;
	private J9ThreadAbstractMonitorPointer monitor;
	private ArrayList<J9VMThreadPointer> waitingThreads;
	private ArrayList<J9VMThreadPointer> blockedThreads;
	private boolean ownerAndCountInitialized = false;
	private J9ObjectMonitorPointer j9objectMonitor;	// the entry in the monitor table corresponding to an inflated object monitor
	private J9ObjectPointer object;

	/* Do not instantiate. Use the factory */
	protected ObjectMonitor_V1(J9ObjectPointer object) throws CorruptDataException
	{
		this.object = object;
		j9objectMonitor = MonitorTableList.peek(object);
		initializeLockword();
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.j9ddr.vm29.j9.ObjectMonitor#getObject()
	 */
	public J9ObjectPointer getObject()
	{
		return object;
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.j9ddr.vm29.j9.ObjectMonitor#getLockword()
	 */
	public J9ObjectMonitorPointer getLockword()
	{
		return lockword;
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.j9ddr.vm29.j9.ObjectMonitor#getInflatedMonitor()
	 */
	@Override
	public J9ThreadAbstractMonitorPointer getInflatedMonitor() 
	{
		return monitor;
	}

	/* (non-Javadoc)
	 * @see com.ibm.j9ddr.vm29.j9.ObjectMonitor#getInflatedMonitor()
	 */
	@Override
	public J9ObjectMonitorPointer getJ9ObjectMonitorPointer() 
	{
		return j9objectMonitor;
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.j9ddr.vm29.j9.ObjectMonitor#getOwner()
	 */
	public J9VMThreadPointer getOwner() throws CorruptDataException
	{
		if(!ownerAndCountInitialized) {
			initializeOwnerAndCount();
		}
		return owner;
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.j9ddr.vm29.j9.ObjectMonitor#getCount()
	 */
	public long getCount() throws CorruptDataException
	{
		if(!ownerAndCountInitialized) {
			initializeOwnerAndCount();
		}
		return count;
	}

	/* (non-Javadoc)
	 * @see com.ibm.j9ddr.vm29.j9.ObjectMonitor#isInflated()
	 */
	public boolean isInflated()
	{
		return isInflated;
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.j9ddr.vm29.j9.ObjectMonitor#isInTable()
	 */
	public boolean isInTable()
	{
		return j9objectMonitor.notNull();
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.j9ddr.vm29.j9.ObjectMonitor#getWaitingThreads()
	 */
	public List<J9VMThreadPointer> getWaitingThreads() throws CorruptDataException
	{
		if(waitingThreads == null) {
			initializeWaitingThreads();
		}
		return waitingThreads;
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.j9ddr.vm29.j9.ObjectMonitor#getBlockedThreads()
	 */
	public List<J9VMThreadPointer> getBlockedThreads() throws CorruptDataException
	{
		if(blockedThreads == null) {
			initializeBlockedThreads();
		}
		return blockedThreads;
	}
	
	public boolean isContended() throws CorruptDataException
	{
		if(isInflated) {
			return getBlockedThreads().size() > 0;		
		} else {
			return lockword.allBitsIn(OBJECT_HEADER_LOCK_FLC);
		}
	}
	
	private void initializeOwnerAndCount() throws CorruptDataException
	{
		if(isInflated) {
			J9ThreadPointer osOwner = monitor.owner();
			if(osOwner.notNull()) {
				owner = J9ThreadHelper.getVMThread(osOwner);
				count = monitor.count().longValue();
				if(count == 0) {
					owner = J9VMThreadPointer.NULL;
				}
			} else {
				owner = J9VMThreadPointer.NULL;
			}
		} else {
			owner = J9VMThreadPointer.cast(lockword.untag(OBJECT_HEADER_LOCK_BITS_MASK));
			if(owner.notNull()) {
				UDATA base = UDATA.cast(lockword).bitAnd(OBJECT_HEADER_LOCK_BITS_MASK).rightShift((int)OBJECT_HEADER_LOCK_RECURSION_OFFSET);
				if(J9BuildFlags.thr_lockReservation) {
					if(!lockword.allBitsIn(OBJECT_HEADER_LOCK_RESERVED)) {
						base = base.add(1);
					}
					count = base.longValue();
				} else {
					count = base.add(1).longValue();
				}
				if(count == 0) {
					/* this can happen if the lock is reserved but unowned */
					owner = J9VMThreadPointer.NULL;
				}
			}
		}
		ownerAndCountInitialized = true;
	}
	
	private void initializeWaitingThreads() throws CorruptDataException
	{
		waitingThreads = new ArrayList<J9VMThreadPointer>();
		if(!isInflated) {
			return;
		}
		J9ThreadPointer thread = monitor.waiting();
		while(thread.notNull()) {
			J9VMThreadPointer vmThread = J9ThreadHelper.getVMThread(thread);
			if(vmThread.notNull()) {
				waitingThreads.add(vmThread);
			}
			thread = thread.next();
		}
	}
	
	private void initializeBlockedThreads() throws CorruptDataException
	{
		blockedThreads = new ArrayList<J9VMThreadPointer>();
		if(isInflated) {
			if(OmrBuildFlags.OMR_THR_THREE_TIER_LOCKING) {
				J9ThreadPointer thread = J9ThreadMonitorHelper.getBlockingField(monitor);
				while(thread.notNull()) {
					J9VMThreadPointer vmThread = J9ThreadHelper.getVMThread(thread);
					if(vmThread.notNull()) {
						blockedThreads.add(vmThread);
					}
					thread = thread.next();
				}
			} else {
				List<J9VMThreadPointer> list = getBlockedThreads(object);
				if(list != null) {
					blockedThreads.addAll(list);
				}
			}
		} else {
			// For consistency we walk the thread list and check for blocking objects
			// rather than do lockword.allBitsIn(OBJECT_HEADER_LOCK_FLC); as the lockword
			// is set slightly later (the thread may spin first). See: CMVC 201473
			List<J9VMThreadPointer> list = getBlockedThreads(object);
			if(list != null) {
				blockedThreads.addAll(list);
			}
		}
	}

	private void initializeLockword() throws CorruptDataException
	{
		lockword = J9ObjectMonitorPointer.NULL;
		
		if(J9BuildFlags.thr_lockNursery) {
			// TODO : why isn't there a cast UDATA->IDATA?
			IDATA lockOffset = new IDATA(J9ObjectHelper.clazz(object).lockOffset());
			// TODO : why isn't there an int/long comparison
			if(lockOffset.gte(new IDATA(0))) {
				lockword = ObjectMonitorReferencePointer.cast(object.addOffset(lockOffset.longValue())).at(0);			
			} else {
				if (j9objectMonitor.notNull()) {
					lockword = j9objectMonitor.alternateLockword();
				}
			}
		} else {
			lockword = J9ObjectMonitorPointer.cast(J9ObjectHelper.monitor(object));
		}
		
		if(lockword.notNull()) {
			isInflated = lockword.allBitsIn(OBJECT_HEADER_LOCK_INFLATED);
			if (isInflated) {
				/* knowing it's inflated we could extract the J9ObjectMonitor doing:
				 * 
				 * 		J9ObjectMonitorPointer.cast(lockword.untag(OBJECT_HEADER_LOCK_INFLATED));
				 *	
				 *...but we already have it so why bother 
				 */
				monitor = J9ThreadAbstractMonitorPointer.cast(j9objectMonitor.monitor());
			}
		}
	}

	private static List<J9VMThreadPointer> getBlockedThreads(J9ObjectPointer blockingObject) throws CorruptDataException
	{
		// Repeatedly walking the thread list could get expensive. 
		// Do a single walk and cache all the results. 
		if(blockedThreadsCache == null) {
			blockedThreadsCache = new HashMap<J9ObjectPointer, List<J9VMThreadPointer>>();
			GCVMThreadListIterator iterator = GCVMThreadListIterator.from();
			while (iterator.hasNext()) {
				J9VMThreadPointer vmThread = iterator.next();
				if(vmThread.publicFlags().allBitsIn(J9Consts.J9_PUBLIC_FLAGS_THREAD_BLOCKED)) {
					J9ObjectPointer object = vmThread.blockingEnterObject(); 
					if(object.notNull()) {
						List<J9VMThreadPointer> list = blockedThreadsCache.get(object);
						if(list == null) {
							list = new ArrayList<J9VMThreadPointer>();
							blockedThreadsCache.put(object, list);
						}
						list.add(vmThread);
					}
				}
			}
		}
		return blockedThreadsCache.get(blockingObject);
	}
	
	@Override
	public int hashCode() {
		return super.hashCode();
	}

	public int compareTo(ObjectMonitor objectMonitor) {
		
		int result = 0;
		ObjectMonitor_V1 objectMonitor_V1 = (ObjectMonitor_V1)objectMonitor;
		result = this.object.compare(objectMonitor_V1.object);
		return result;
	}
	
}
