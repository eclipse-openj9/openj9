/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2008, 2017 IBM Corp. and others
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
package com.ibm.dtfj.phd;

import java.util.ArrayList;
import java.util.Iterator;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.image.ImagePointer;
import com.ibm.dtfj.image.MemoryAccessException;
import com.ibm.dtfj.java.JavaMonitor;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.java.JavaThread;

/**
 * Java monitor information
 * @author ajohnson
 */
public class PHDJavaMonitor implements JavaMonitor {

	private ImagePointer id;
	private String name;
	private CorruptData name_cd;
	private JavaObject object;
	private JavaThread owner;
	private CorruptData owner_cd;
	private ArrayList<JavaThread> enterWaiters = new ArrayList<JavaThread>();
	private ArrayList<JavaThread> notifyWaiters = new ArrayList<JavaThread>();
	
	/**
	 * Build Java monitor information from a JavaMonitor from another dump type.
	 * Extract all the information on object construction.
	 * @param space
	 * @param runtime
	 * @param source
	 */
	PHDJavaMonitor(ImageAddressSpace space, PHDJavaRuntime runtime, JavaMonitor source) {
		id = space.getPointer(source.getID().getAddress());
		try {
			name = source.getName();
		} catch (CorruptDataException e) {
			name_cd = new PHDCorruptData(space, e);
		}
		JavaObject obj = source.getObject();
		if (obj != null) {
			try {
				object = runtime.getObjectAtAddress(space.getPointer(obj.getID().getAddress()));
			} catch (CorruptDataException e) {
			} catch (DataUnavailable e) {
			} catch (MemoryAccessException e) {
			}
		}
		try {
		    JavaThread thr = source.getOwner();
		    if (thr != null) {
		    	owner = runtime.getThread(thr);
		    }
		} catch (CorruptDataException e) {
			owner_cd = new PHDCorruptData(space, e);
		}
		for (Iterator it = source.getEnterWaiters(); it.hasNext(); ) {
			Object next = it.next();
			if (next instanceof CorruptData) {
				JavaThread thr = new PHDCorruptJavaThread(space, (CorruptData)next);
				enterWaiters.add(thr);
			} else {
				enterWaiters.add(runtime.getThread((JavaThread)next));
			}
		}
		for (Iterator it = source.getNotifyWaiters(); it.hasNext(); ) {
			Object next = it.next();
			if (next instanceof CorruptData) {
				JavaThread thr = new PHDCorruptJavaThread(space, (CorruptData)next);
				notifyWaiters.add(thr);
			} else {
				notifyWaiters.add(runtime.getThread((JavaThread)next));
			}
		}
	}
	
	public Iterator<JavaThread> getEnterWaiters() {
		return enterWaiters.iterator();
	}

	public ImagePointer getID() {
		return id;
	}

	public String getName() throws CorruptDataException {
		if (name_cd != null) throw new CorruptDataException(name_cd);
		return name;
	}

	public Iterator<JavaThread> getNotifyWaiters() {
		return notifyWaiters.iterator();
	}

	public JavaObject getObject() {
		return object;
	}

	public JavaThread getOwner() throws CorruptDataException {
		if (owner_cd != null) throw new CorruptDataException(owner_cd);
		return owner;
	}

	public boolean equals(Object o) {
		if (o == null || !getClass().equals(o.getClass())) return false;
		return id == ((PHDJavaMonitor)o).id;
	}
	
	public int hashCode() {
		return id.hashCode();
	}
}
