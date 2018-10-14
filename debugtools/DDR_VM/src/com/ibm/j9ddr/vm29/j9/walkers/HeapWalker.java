/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

import static com.ibm.j9ddr.vm29.events.EventManager.register;
import static com.ibm.j9ddr.vm29.events.EventManager.unregister;

import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.NoSuchElementException;
import java.util.Set;
import java.util.SortedSet;
import java.util.TreeSet;
import java.util.logging.Logger;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.events.IEventListener;
import com.ibm.j9ddr.logging.LoggerNames;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.ObjectAccessBarrier;
import com.ibm.j9ddr.vm29.j9.ObjectModel;
import com.ibm.j9ddr.vm29.j9.ObjectMonitor;
import com.ibm.j9ddr.vm29.j9.gc.GCExtensions;
import com.ibm.j9ddr.vm29.j9.gc.GCHeapRegionDescriptor;
import com.ibm.j9ddr.vm29.j9.gc.GCHeapRegionIterator;
import com.ibm.j9ddr.vm29.j9.gc.GCObjectHeapIterator;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_GCExtensionsPointer;
import com.ibm.j9ddr.vm29.pointer.generated.MM_HeapRegionManagerPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import static com.ibm.j9ddr.vm29.events.EventManager.raiseCorruptDataEvent;

public class HeapWalker implements IEventListener {
	private static final int STATE_INIT = 0;
	private static final int STATE_OBJECT_LIVE = 1;
	private static final int STATE_OBJECT_DEAD = 2;
	private static final int STATE_SCANNING_LIVE = 3;
	private static final int STATE_SCANNING_DEAD = 4;
	private static final int STATE_FINISHED = 5;
	private int state = STATE_INIT;
	
	private final Logger log = Logger.getLogger(LoggerNames.LOGGER_WALKERS); 
	private final GCHeapRegionDescriptor region;
	private final GCObjectHeapIterator heapIterator;
	
	private int segmentIndex = -1;
	private long objectCount = 0;
	private long totalObjectCount = 0;
	private boolean lastObjectCorrupt = false;
	
	private final HeapWalkerEvents events;
	
	private Set<ObjectMonitor> localFlatLockedMonitors;
	
	private static Map<GCHeapRegionDescriptor, Set<ObjectMonitor>> flatLockedMonitorsByRegion = new HashMap<GCHeapRegionDescriptor, Set<ObjectMonitor>>();
	private static SortedSet<ObjectMonitor> flatLockedMonitors;
		
	public HeapWalker(J9JavaVMPointer vm, GCHeapRegionDescriptor hrd, HeapWalkerEvents sink) throws CorruptDataException {
		events = sink;
		region = hrd;
		heapIterator = GCObjectHeapIterator.fromHeapRegionDescriptor(region, true, true);
		
		//Checks whether we should populate the flatLockedMonitor list on this walk
		if (! flatLockedMonitorsByRegion.containsKey(region)) {
			localFlatLockedMonitors = new HashSet<ObjectMonitor>(256);
		}
	}
	
	public boolean hasNext() {
		register(this);
		try {
			return hasNextNoHandler();
		} finally {
			unregister(this);
		}
	}
	
	/* Private version of hasNext so we don't have to unregister/register event handlers for internal calls. */
	private boolean hasNextNoHandler() {
		boolean result = heapIterator.hasNext();
		
		if (!result) { // iteration ended, store the monitors		
			if (localFlatLockedMonitors != null && ! flatLockedMonitorsByRegion.containsKey(region)) {
				flatLockedMonitorsByRegion.put(region, localFlatLockedMonitors);
				localFlatLockedMonitors = null;
			}
		}
		return result;
	}
	
	public void walk() {
		register(this);
		try {
			if(hasNextNoHandler()) {
				try {
					J9ObjectPointer object = heapIterator.next();
					doState(object);
					if(!hasNextNoHandler()) {
						state = STATE_FINISHED;
						doState(object);	
					}
				} catch (CorruptDataException e) {
					/* CMVC 173382: Due to the corrupt data registration mechanism, we shouldn't
					 * take action in the event of CDE to prevent double reporting.
					 */
				}
			} else {
				throw new NoSuchElementException("The heap walk is complete");
			}
		} finally {
			unregister(this);
		}
	}
	
	private void doState(J9ObjectPointer object) throws CorruptDataException {
		switch(state) {
			case STATE_INIT : 
				log.fine("Scanning Region @ 0x" + Long.toHexString(region.getHeapRegionDescriptorPointer().getAddress()));
				if(!ObjectModel.isDeadObject(object)) {
					state = STATE_OBJECT_LIVE;
					doState(object);
				} else {
					events.doDeadObject(object);	//dead object at the start of the segment
				}
				break;
			case STATE_OBJECT_LIVE :
				segmentIndex++;
				log.fine(String.format("\tStarting scan of heap segment %d start=0x%08x",
						segmentIndex,
						object.getAddress()
				));
				objectCount++;
				state = STATE_SCANNING_LIVE;
				events.doSectionStart(object.getAddress());		//user events go after state change
				events.doLiveObject(object);
				
				if (localFlatLockedMonitors != null) {
					processFlatMonitor(object);
				}
				break;
			case STATE_OBJECT_DEAD :
				log.fine(String.format(" end=0x%08x object count = %d\n",
						object.getAddress(),
						objectCount
				));
				totalObjectCount += objectCount;
				objectCount = 0;
				state = STATE_SCANNING_DEAD;
				events.doSectionEnd(object.getAddress());
				events.doDeadObject(object);
				break;
			case STATE_SCANNING_LIVE :
				if(ObjectModel.isDeadObject(object)) {
					state = STATE_OBJECT_DEAD;
					doState(object);
				} else {
					objectCount++;
					events.doLiveObject(object);
					
					if (localFlatLockedMonitors != null) {
						processFlatMonitor(object);
					}
				}
				break;
			case STATE_SCANNING_DEAD :
				if(!ObjectModel.isDeadObject(object)) {
					state = STATE_OBJECT_LIVE;
					doState(object);
				} else {
					events.doDeadObject(object);
				}
				break;
			case STATE_FINISHED :
				long address = object.getAddress();
				if(!ObjectModel.isDeadObject(object)) {
					address += ObjectModel.getSizeInBytesWithHeader(object).longValue();
				}
				log.fine(String.format(" end=0x%08x object count = %d\n",
						address,
						objectCount
				));
				totalObjectCount += objectCount;
				log.fine("Total object count = " + totalObjectCount);
				events.doSectionEnd(address);
				break;
			default :
					throw new IllegalStateException("Invalid state of " + state + " was detected");
		}
	}

	private void processFlatMonitor(J9ObjectPointer object)
	{
		
		try {
			/* Get all the J9ObjectMonitor(s) associated with this object */
			ObjectMonitor objMonitor = ObjectAccessBarrier.getMonitor(object);
			if ((null != objMonitor) && !objMonitor.isInflated()) {
				localFlatLockedMonitors.add(objMonitor);
			}
		} catch (CorruptDataException e) {
			raiseCorruptDataEvent("Could not process flat monitor", e, false);		//can recover from this
		}
	}

	/**
	 * Returns a lazily-initialized list of flat-locked object monitors.
	 * 
	 * Will run initialization if it hasn't already been performed.
	 * 
	 * @return List of flat-locked object monitors
	 */
	public static SortedSet<ObjectMonitor> getFlatLockedMonitors() throws CorruptDataException
	{
		if (flatLockedMonitors == null) {
			initializeFlatLockedMonitors();
		}

		return Collections.unmodifiableSortedSet(flatLockedMonitors);
	}

	private static void initializeFlatLockedMonitors() throws CorruptDataException 
	{
		J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
		MM_GCExtensionsPointer gcext = GCExtensions.getGCExtensionsPointer();
		MM_HeapRegionManagerPointer hrm = gcext.heapRegionManager();
		
		flatLockedMonitors = new TreeSet<ObjectMonitor>();
		GCHeapRegionIterator regions = GCHeapRegionIterator.fromMMHeapRegionManager(hrm, true, true);
		while (regions.hasNext()) {
			GCHeapRegionDescriptor region = regions.next();
			
			if (! flatLockedMonitorsByRegion.containsKey(region)) {
				runFlatLockMonitorRegionWalk(vm, region);
			}
		
			/* Running the walk should have populated the flatLockedMonitors map */
			assert ( flatLockedMonitorsByRegion.containsKey(region) );
		
			flatLockedMonitors.addAll(flatLockedMonitorsByRegion.get(region));
		}
	}

	private static void runFlatLockMonitorRegionWalk(J9JavaVMPointer vm, GCHeapRegionDescriptor region) throws CorruptDataException 
	{
		HeapWalker walker = new HeapWalker(vm, region, new DummyHeapWalkerEvents());
		
		while (walker.hasNext()) {
			walker.walk();
		}
	}

	public void corruptData(String message, CorruptDataException e,
			boolean fatal) {
		if( !lastObjectCorrupt ) {
			lastObjectCorrupt = true;
			events.doCorruptData(e);
		}
	}
	
	private static final class DummyHeapWalkerEvents implements HeapWalkerEvents
	{
		public void doSectionStart(long address)
		{
		}
		
		public void doSectionEnd(long address)
		{
		}
		
		public void doLiveObject(J9ObjectPointer object)
		{
		}
		
		public void doDeadObject(J9ObjectPointer object)
		{
		}
		
		public void doCorruptData(CorruptDataException e)
		{
		}
	}
}
