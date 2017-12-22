/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.logging.Logger;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.java.JavaMonitor;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.java.JavaThread;
import com.ibm.j9ddr.view.dtfj.J9DDRDTFJUtils;
import com.ibm.j9ddr.view.dtfj.image.J9DDRImagePointer;
import com.ibm.j9ddr.vm29.j9.ObjectMonitor;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;
import com.ibm.j9ddr.vm29.view.dtfj.DTFJContext;

public class DTFJJavaObjectMonitor implements JavaMonitor {
	private final Logger log = DTFJContext.getLogger();
	private final ObjectMonitor monitor;
	private String name = null;
	
	public DTFJJavaObjectMonitor(ObjectMonitor ptr)  {
		monitor = ptr;
		log.fine(String.format("Created object monitor 0x%016x", monitor.getObject().getAddress()));
	}
	
	@SuppressWarnings("rawtypes")
	public Iterator getEnterWaiters() {
		try {
			return convertToDTFJThreads(monitor.getBlockedThreads());
		} catch (Throwable t) {
			CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
			return J9DDRDTFJUtils.corruptIterator(cd);
		}
	}

	private Iterator<?> convertToDTFJThreads(List<J9VMThreadPointer> threads) {
		ArrayList<DTFJJavaThread> dtfjthreads = new ArrayList<DTFJJavaThread>();
		for(J9VMThreadPointer thread : threads)
		{
			dtfjthreads.add(new DTFJJavaThread(thread));
		}
		return dtfjthreads.iterator();		
	}
	
	public ImagePointer getID() {
		try {
			if(monitor.isInflated()) {
				return DTFJContext.getImagePointer(monitor.getInflatedMonitor().getAddress());
			} else {
				return DTFJContext.getImagePointer(monitor.getObject().getAddress());	
			}
		} catch (Throwable t) {
			J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
		}
		//can't throw a corrupt data exception here so indicate that the pointer is invalid
		return new J9DDRImagePointer(DTFJContext.getProcess(), 0xDEADBEEFBADF00DL);
	}

	public String getName() throws CorruptDataException {
		if(name == null) {
			try {
				String objaddress = Long.toHexString(monitor.getObject().getAddress());
				String id = Long.toHexString(getID().getAddress());
				name = String.format("(un-named monitor @0x%s for object @0x%s)", id, objaddress);
			} catch (Throwable t) {
				throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
			}
		}
		return name;
	}

	@SuppressWarnings("rawtypes")
	public Iterator getNotifyWaiters() {
		try {
			return convertToDTFJThreads(monitor.getWaitingThreads());
		} catch (Throwable t) {
			CorruptData cd = J9DDRDTFJUtils.handleAsCorruptData(DTFJContext.getProcess(), t);
			return J9DDRDTFJUtils.corruptIterator(cd);
		}
	}

	public JavaObject getObject() {
		return new DTFJJavaObject(monitor.getObject());
	}

	public JavaThread getOwner() throws CorruptDataException {
		try {
			J9VMThreadPointer ptr = monitor.getOwner();
			if((ptr == null) || ptr.isNull()) 
			{
				return null;
			} else {
				return new DTFJJavaThread(ptr);
			}
		} catch (Throwable t) {
			throw J9DDRDTFJUtils.handleAsCorruptDataException(DTFJContext.getProcess(), t);
		}
	}
	
	@Override
	public boolean equals(Object obj) {
		if((obj == null) || !(obj instanceof DTFJJavaObjectMonitor)) {
			return false;
		}
		DTFJJavaObjectMonitor test = (DTFJJavaObjectMonitor) obj;
		return getID().equals(test.getID());
	}

	@Override
	public int hashCode() {
		return getID().hashCode();
	}
}
