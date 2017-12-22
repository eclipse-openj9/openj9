/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.view.dtfj.java;

import static com.ibm.j9ddr.vm29.structure.J9VMThread.J9VMTHREAD_STATE_BLOCKED;
import static com.ibm.j9ddr.vm29.structure.J9VMThread.J9VMTHREAD_STATE_WAITING;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.logging.Level;
import java.util.logging.Logger;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.java.JavaMonitor;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.java.JavaThread;
import com.ibm.j9ddr.view.dtfj.J9DDRDTFJUtils;
import com.ibm.j9ddr.vm29.j9.J9VMThreadPointerUtil.ThreadInfo;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ThreadMonitorPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ThreadPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;
import com.ibm.j9ddr.vm29.pointer.generated.OMR_VMThreadPointer;
import com.ibm.j9ddr.vm29.view.dtfj.DTFJContext;

public class DTFJJavaSystemMonitor implements JavaMonitor {
	private final Logger log = DTFJContext.getLogger();
	private final J9ThreadMonitorPointer monitor;
	private String name = null;
	
	public DTFJJavaSystemMonitor(J9ThreadMonitorPointer ptr)  {
		monitor = ptr;
		log.fine(String.format("Created object monitor 0x%016x", ptr.getAddress()));
	}
	
	@SuppressWarnings("rawtypes")
	public Iterator getEnterWaiters() {
		try {
			return scanThreads(J9VMTHREAD_STATE_BLOCKED);
		} catch (Throwable t) {
			CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
			return J9DDRDTFJUtils.corruptIterator(cd);
		}
	}

	public ImagePointer getID() {
		return DTFJContext.getImagePointer(monitor.getAddress());
	}

	public String getName() throws CorruptDataException {
		if(name == null) {
			try {
				name = monitor.name().getCStringAtOffset(0);
			} catch (com.ibm.j9ddr.corereaders.memory.MemoryFault e) {
				name = "Unknown monitor name";
			} catch (Throwable t) {
				throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
			}
		}
		return name;
	}

	@SuppressWarnings("rawtypes")
	public Iterator getNotifyWaiters() {
		try {
			return scanThreads(J9VMTHREAD_STATE_WAITING);
		} catch (Throwable t) {
			CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
			return J9DDRDTFJUtils.corruptIterator(cd);
		}
	}

	public JavaObject getObject() {
		try {
			J9ObjectPointer object = J9ObjectPointer.cast(monitor.userData());
			if(object.isNull()) {
				return null;
			} else {
				return new DTFJJavaObject(object);
			}
		} catch (Throwable t) {
			// intercept but do not re-throw as the API behaviour is to return null
			log.log(Level.FINE, "Throwable when getting object", t);
			J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
		}
		return null;
	}

	public JavaThread getOwner() throws CorruptDataException {
		JavaThread javaThread = null;
		try {
			J9ThreadPointer owner = monitor.owner();
			if (!owner.isNull()) {
				int vmThreadKey = DTFJContext.getVm().omrVM()._vmThreadKey().intValue() - 1;
				OMR_VMThreadPointer omrVmThread = OMR_VMThreadPointer.cast(owner.tlsEA().at(vmThreadKey));
				if (!omrVmThread.isNull()) {
					javaThread = new DTFJJavaThread(J9VMThreadPointer.cast(omrVmThread._language_vmthread()));
				}
			}
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
		}
		return javaThread;
	}

	private Iterator<DTFJJavaThread> scanThreads(long threadState) throws com.ibm.j9ddr.CorruptDataException {
		List<ThreadInfo> threadInfoCache = DTFJContext.getThreadInfoCache();
		ArrayList<DTFJJavaThread> threads = new ArrayList<DTFJJavaThread>();
		for(int i = 0; i < threadInfoCache.size(); i++) {
			try {
				ThreadInfo info = threadInfoCache.get(i);
				if((info.getState() & threadState) != 0) {		//thread is in the correct state
					if((info.getRawLock() != null) && info.getRawLock().eq(monitor)) {
						DTFJJavaThread dtfjThread = new DTFJJavaThread(info.getThread());				
						threads.add(dtfjThread);		//raw monitor
					}
				}
			} catch (Throwable t) {
				//log error and try to carry on
				J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
			}
		}
		return threads.iterator();
	}
	
	@Override
	public boolean equals(Object obj) {
		if((obj == null) || !(obj instanceof DTFJJavaSystemMonitor)) {
			return false;
		}
		DTFJJavaSystemMonitor test = (DTFJJavaSystemMonitor) obj;
		return monitor.eq(test.monitor);
	}

	@Override
	public int hashCode() {
		return monitor.hashCode();
	}
	
	
	
}
