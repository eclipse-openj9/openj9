/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.j9.walkers;

import static com.ibm.j9ddr.vm29.events.EventManager.raiseCorruptDataEvent;

import java.util.Iterator;
import java.util.NoSuchElementException;
import java.util.logging.Level;
import java.util.logging.Logger;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.logging.LoggerNames;
import com.ibm.j9ddr.vm29.j9.ObjectAccessBarrier;
import com.ibm.j9ddr.vm29.j9.ObjectMonitor;
import com.ibm.j9ddr.vm29.pointer.generated.J9AbstractThreadPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ThreadAbstractMonitorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ThreadLibraryPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ThreadMonitorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ThreadMonitorPoolPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;
import com.ibm.j9ddr.vm29.structure.J9ThreadAbstractMonitor;
import com.ibm.j9ddr.vm29.structure.J9ThreadMonitorPool;
import com.ibm.j9ddr.vm29.types.UDATA;

/**
 * The monitor iterator walks over the monitor table to get the system and inflated object monitors. It then 
 * walks the heap in order to find any flat object monitors.
 * 
 * @author Adam Pilkington
 */
@SuppressWarnings("rawtypes")
public class MonitorIterator implements Iterator {
	private static final UDATA FREE_TAG = new UDATA(-1);
	private final Logger log = Logger.getLogger(LoggerNames.LOGGER_WALKERS);
	private J9ThreadMonitorPoolPointer pool;
	private int index = 0;
	private J9ThreadMonitorPointer poolEntries;
	private long poolSize = 0;
	private Object current;
	private boolean walkThreadPool = true;
	private final Iterator<ObjectMonitor> flatMonitorIterator;
	private boolean EOI = false;			//indicates if End Of Iterator has been reached			
	
	public MonitorIterator(J9JavaVMPointer vm) throws CorruptDataException {
		J9AbstractThreadPointer ptr = getThreadLibrary(vm);
		J9ThreadLibraryPointer lib = ptr.library();
		log.fine(String.format("Thread library 0x%016x", lib.getAddress()));
		pool = lib.monitor_pool();
		if(pool.notNull()) {
			poolEntries = pool.entriesEA();
			poolSize = J9ThreadMonitorPool.MONITOR_POOL_SIZE;
		}
		flatMonitorIterator = HeapWalker.getFlatLockedMonitors().iterator();
	}
	
	public boolean hasNext() {
		if(current != null) {
			return true;		//have an object to serve out through the iterator
		}
		if(EOI) {
			return false;		//EOI so no more elements to return
		}
		boolean result = false;
		try {
			if(walkThreadPool) {	//walking the thread pool for the system and inflated monitors
				result = threadPoolHasNext();
				walkThreadPool = result;
			}
			if(!walkThreadPool) {	//walking the heap for flat object monitors
				result = flatMonitorIterator.hasNext();
				if(result && (current == null)) {
					current = flatMonitorIterator.next();
				}
			}
		} catch (CorruptDataException e) {
			raiseCorruptDataEvent("Could not locate the next monitor", e, true);
			EOI = true;		//indicate no more monitors can be retrieved
		}
		return (current != null) || result;
	}
	
	private boolean threadPoolHasNext() throws CorruptDataException {
		while((current == null) && pool.notNull()) {
			while(index < poolSize) {
				J9ThreadMonitorPointer monitor = J9ThreadMonitorPointer.cast(poolEntries.add(index));
				index++;
				if (!FREE_TAG.eq(monitor.count())) {
					J9ThreadAbstractMonitorPointer lock = J9ThreadAbstractMonitorPointer.cast(monitor);
					if (lock.flags().allBitsIn(J9ThreadAbstractMonitor.J9THREAD_MONITOR_OBJECT)) {
						if(!lock.userData().eq(0)) {	//this is an object monitor in the system monitor table
							J9ObjectPointer obj = J9ObjectPointer.cast(lock.userData());
							ObjectMonitor objmon = ObjectAccessBarrier.getMonitor(obj);

							// This check is to exclude flat object monitors. Flat object monitors are accounted for during the heap walk
							if((objmon == null) || !objmon.isInflated())
							{
								continue;
							}
							current = objmon;		//return an object monitor
						}
					} else {
						current = monitor;		//return a system monitor
					}
					if (log.isLoggable(Level.FINE)) {
						log.fine(String.format("Found monitor @ 0x%016x : %s", monitor.getAddress(), monitor.name().getCStringAtOffset(0)));
					}
					return true;
				}
			}
			pool = pool.next();
			if(pool.notNull()) {
				index = 0;
				poolEntries = pool.entriesEA();
			}
		}
		return pool.notNull();
	}
	
	public Object next() {
		if(hasNext()) {
			Object retval = current;
			current = null;
			return retval;
		}
		throw new NoSuchElementException();
	}

	public void remove() {
		throw new UnsupportedOperationException("This iterator is read only");
	}

	private J9AbstractThreadPointer getThreadLibrary(J9JavaVMPointer vm) throws CorruptDataException {
		J9VMThreadPointer remoteThread = vm.mainThread();
		if(remoteThread.notNull()) {
			J9AbstractThreadPointer ptr = J9AbstractThreadPointer.cast(remoteThread.osThread());
			if(ptr.notNull()) {
				return ptr;
			}
		}
		throw new CorruptDataException("Cannot locate thread library");
	}
	
}
