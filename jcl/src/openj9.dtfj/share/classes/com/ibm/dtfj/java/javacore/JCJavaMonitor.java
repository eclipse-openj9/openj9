/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2007, 2017 IBM Corp. and others
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
package com.ibm.dtfj.java.javacore;

import java.util.Iterator;
import java.util.Vector;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.javacore.JCCorruptData;
import com.ibm.dtfj.java.JavaMonitor;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.java.JavaThread;
import com.ibm.dtfj.javacore.builder.IBuilderData;

public class JCJavaMonitor implements JavaMonitor{
	
	private Vector fEnterWaiters;
	private Vector fNotifyWaiters;
	private String fName;
	private JavaObject fEncompassingObject;
	private final ImagePointer fMonitorPointer;
	private final JCJavaRuntime fRuntime;
	private long fOwner;
	
	public JCJavaMonitor(JCJavaRuntime javaRuntime, ImagePointer monitorPointer, String name) throws JCInvalidArgumentsException{
		if (monitorPointer == null) {
			throw new JCInvalidArgumentsException("A monitor must have a valid pointer identifier");
		}
		if (javaRuntime == null) {
			throw new JCInvalidArgumentsException("A monitor must be associated with a valid runtime");
		}
		fRuntime = javaRuntime;
		fMonitorPointer = monitorPointer;
		fName = name;
		fEncompassingObject = null;
		fOwner = IBuilderData.NOT_AVAILABLE;
		
		fEnterWaiters = new Vector();
		fNotifyWaiters = new Vector();
		
		// Be sure to register this monitor with the runtime
		fRuntime.addMonitor(this);
	}

	
	/**
	 * 
	 */
	public ImagePointer getID() {
		return fMonitorPointer;
	}

	
	/**
	 * 
	 */
	public String getName() throws CorruptDataException {
		if (fName == null) {
			String address = Long.toHexString(fMonitorPointer.getAddress());
			fName = (fEncompassingObject == null) 
					? "(un-named monitor @0x" + address + ")" 
					: "(un-named monitor @0x" + address + " for object @0x" + Long.toHexString(fEncompassingObject.getID().getAddress()) + ")";
		} 
		return fName;
	}
	
	/**
	 * 
	 */
	public Iterator getEnterWaiters() {
		return getThreads(fEnterWaiters);
	}
	
	/**
	 * 
	 */
	public Iterator getNotifyWaiters() {
		return getThreads(fNotifyWaiters);
	}
	
	/**
	 * 
	 * @param threadIDs
	 * 
	 */
	private Iterator getThreads(Vector threadIDs) {
		Vector threads = new Vector();
		Iterator it = threadIDs.iterator();
		while(it.hasNext()) {
			ImagePointer pointer = (ImagePointer) it.next();
			JCJavaThread waitingThread = fRuntime.findJavaThread(pointer.getAddress());
			if (waitingThread != null) {
				threads.add(waitingThread);
			} else {
				threads.add(new JCCorruptData("Unknown thread", pointer));
			}
		}
		return threads.iterator();
	}

	/**
	 * 
	 */
	public JavaObject getObject() {
		return fEncompassingObject;
	}
	
	/**
	 * 
	 */
	public JavaThread getOwner() throws CorruptDataException {
		if (fOwner != IBuilderData.NOT_AVAILABLE) {
			if (fOwner < 0x10000 && fEncompassingObject != null) {
				// Sov flat locked - so owner id is an index, but the object being locked has the proper owner
				JCJavaMonitor jm2 = fRuntime.findMonitor(fEncompassingObject.getID().getAddress());
				if (jm2 != null) {
					try {
						JavaThread javaThread = jm2.getOwner();
						return javaThread;
					} catch (CorruptDataException e) {
						// Ignore so that we throw our own exception
					}
				}
			}
			JavaThread javaThread = fRuntime.findJavaThread(fOwner);
			if(javaThread != null) {
				return javaThread;
			}
			throw new CorruptDataException(new JCCorruptData("No owner for this monitor", fMonitorPointer.getAddressSpace().getPointer(fOwner)));
		}
		return null;
	}
	
	
	/**
	 * NOT in DTFJ
	 * @param threadID
	 */
	public void addEnterWaiter(ImagePointer threadID) {
		if (threadID != null) {
			fEnterWaiters.add(threadID);
		}
	}
	
	/**
	 * NOT in DTFJ
	 * @param threadID
	 */
	public void addNotifyWaiter(ImagePointer threadID) {
		if (threadID != null) {
			fNotifyWaiters.add(threadID);
		}
	}
	
	/**
	 * NOT in DTFJ
	 * @param encompassingObject
	 */
	public void setObject(JavaObject encompassingObject) {
		fEncompassingObject = encompassingObject;
	}
	
	/**
	 * NOT in DTFJ
	 * @param javaThread
	 */
	public void setOwner(long javaThread) {
		fOwner = javaThread;
	}
}
