/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.MemoryAccessException;
import com.ibm.dtfj.java.JavaMonitor;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.java.JavaThread;

public class MonitorNode {

	public static final int UNKNOWN = 0;
	public static final int NO_DEADLOCK = 1;
	public static final int LOOP_DEADLOCK = 2;
	public static final int BRANCH_DEADLOCK = 3;
	
	private JavaMonitor monitor;
	public int visit;
	public MonitorNode waitingOn;
	public int deadlock;
	public NodeList inList;
	
	// For subclasses to call.
	protected MonitorNode() {
		visit = 0;
		waitingOn = null;
		deadlock = UNKNOWN;
		inList = null;
	}
	
	public MonitorNode(JavaMonitor _monitor)
	{
		monitor = _monitor;
		visit = 0;
		waitingOn = null;
		deadlock = UNKNOWN;
		inList = null;
	}

	private JavaMonitor getMonitor()
	{
		return monitor;
	}

	public Iterator getEnterWaiters() {
		return monitor.getEnterWaiters();
	}

	public JavaThread getOwner() throws CorruptDataException, MemoryAccessException {
		return monitor.getOwner();
	}

	public JavaObject getObject() {
		return monitor.getObject();
	}

	public String getType() {
		try {
			if( monitor.getObject() != null ) {
				return "monitor for " + monitor.getObject().getJavaClass().getName();
			} else {
				return "raw monitor";
			}
		} catch (CorruptDataException e) {
			return "monitor";
		}
	}
	
	public long getMonitorAddress() {
		return monitor.getID().getAddress();
	}
	
}
