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
package com.ibm.j9ddr.vm29.view.dtfj.java.j9;

import java.util.Iterator;
import java.util.NoSuchElementException;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.ObjectMonitor;
import com.ibm.j9ddr.vm29.j9.walkers.MonitorIterator;
import com.ibm.j9ddr.vm29.pointer.generated.J9ThreadMonitorPointer;
import com.ibm.j9ddr.vm29.view.dtfj.DTFJContext;
import com.ibm.j9ddr.vm29.view.dtfj.java.DTFJJavaObjectMonitor;
import com.ibm.j9ddr.vm29.view.dtfj.java.DTFJJavaSystemMonitor;

//provides a DTFJ wrapper around the values returned from the J9 model

@SuppressWarnings("unchecked")
public class DTFJMonitorIterator implements Iterator {
	private final MonitorIterator monitors;
	
	public DTFJMonitorIterator() throws CorruptDataException {
		monitors = new MonitorIterator(DTFJContext.getVm());
	}

	public boolean hasNext() {
		return monitors.hasNext();
	}

	public Object next() {
		if(hasNext()) {
			Object current = monitors.next();
			if (current instanceof J9ThreadMonitorPointer) {
				return new DTFJJavaSystemMonitor((J9ThreadMonitorPointer)current);
			}
			if (current instanceof ObjectMonitor) {
				return new DTFJJavaObjectMonitor((ObjectMonitor)current);
			}
		}
		throw new NoSuchElementException();
	}

	public void remove() {
		monitors.remove();
	}

}
