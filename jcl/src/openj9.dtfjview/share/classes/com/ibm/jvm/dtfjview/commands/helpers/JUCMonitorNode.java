/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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
package com.ibm.jvm.dtfjview.commands.helpers;

import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.MemoryAccessException;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.dtfj.java.JavaThread;

public class JUCMonitorNode extends MonitorNode {

	private JavaObject lock;
	private JavaRuntime jr;
	
	public JUCMonitorNode(JavaObject jucLock, JavaRuntime rt) {
		this.lock = jucLock;
		this.jr = rt;
	}
	
	public Iterator getEnterWaiters() {
		List waiters = new LinkedList();
		Iterator itThread = jr.getThreads();
		while (itThread.hasNext()) {
			Object o = itThread.next();
			try {
				if( !(o instanceof JavaThread) ) {
					continue;
				}
				JavaThread jt = (JavaThread)o;
				if( (jt.getState() & JavaThread.STATE_PARKED) != 0 ) {
					JavaObject blocker = jt.getBlockingObject();
					if( blocker != null && blocker.equals(lock)) {
						waiters.add(jt);
					}
				}
			} catch(CorruptDataException cde) {
				//out.println("\nwarning, corrupt data encountered during scan for java.util.concurrent locks...");
			} catch(DataUnavailable du) {
				//out.println("\nwarning, data unavailable encountered during scan for java.util.concurrent locks...");
			}
		}
		return waiters.iterator();
	}

	public JavaThread getOwner() throws CorruptDataException, MemoryAccessException {
		return Utils.getParkBlockerOwner(lock, jr);
	}

	public JavaObject getObject() {
		return lock;
	}
	
	public String getType() {
		try {
			return lock.getJavaClass().getName();
		} catch (CorruptDataException e) {
			return "<unknown>";
		}
	}
}
