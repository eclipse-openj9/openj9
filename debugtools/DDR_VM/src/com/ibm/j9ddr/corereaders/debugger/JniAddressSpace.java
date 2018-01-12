/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.corereaders.debugger;

import java.util.Collection;
import java.util.LinkedList;
import java.util.List;

import com.ibm.j9ddr.corereaders.ICore;
import com.ibm.j9ddr.corereaders.memory.IAddressSpace;
import com.ibm.j9ddr.corereaders.memory.IProcess;

public class JniAddressSpace extends JniSearchableMemory implements IAddressSpace {
	private final int id;
	List<JniProcess> processes;
	private final JniReader reader;

	/**
	 * Constructor
	 * @param id AddressSpace ID
	 * @param reader JNI reader instance
	 */
	public JniAddressSpace(int id, JniReader reader) {
		this.id = id;
		JniProcess process = new JniProcess(this);
		processes = new LinkedList<JniProcess>();
		processes.add(process);
		this.reader = reader;
	}

	public int getAddressSpaceId() {
		return id;
	}

	public Collection<? extends IProcess> getProcesses() {
		return processes;
	}

	/**
	 * This method returns the core reader instance. 
	 * Name of the method can be confusing in JNI cases where there is no core direct core file or no core file at all. 
	 * Therefore, core reader is actually the reader of the current state of attached process which 
	 *  a. might have loaded a core file with another debugger like dbg, dgb, windbg, etc
	 *  b. might have attached to running vm.
	 *  
	 *  @returns ICore instance of the core reader or debugged process reader.
	 */
	public ICore getCore() {
		return reader;
	}
	
	
}
