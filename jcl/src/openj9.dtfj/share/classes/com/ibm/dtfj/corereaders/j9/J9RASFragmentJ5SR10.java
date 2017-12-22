/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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
package com.ibm.dtfj.corereaders.j9;

import com.ibm.dtfj.corereaders.MemoryAccessException;

/**
 * Represents the J9RAS structure from Java 5 SR10 when TID and PID fields were added to the RAS structure
 * Java 5 32 bit : length =  0x120, version = 0x10000		TID + PID, No J9DDR
 * Java 5 64 bit : length =  0x158, version = 0x10000		TID + PID, No J9DDR
 * 
 * @author Adam Pilkington
 *
 */
public class J9RASFragmentJ5SR10 implements J9RASFragment {
	private long tid = 0;	
	private long pid = 0;
	
	/**
	 * Create a Java 5 SR10, version 1 J9RAS fragment
	 * @param memory address space
	 * @param address address of the J9RAS structure
	 * @param is64bit true if a 64 bit core file
	 * @throws MemoryAccessException rethrow memory exceptions
	 */
	public J9RASFragmentJ5SR10(Memory memory, long address, boolean is64bit) throws MemoryAccessException {
		if(is64bit) {
			pid = memory.getLongAt(address + 0x148);
			tid = memory.getLongAt(address + 0x150);
		} else {
			pid = memory.getIntAt(address + 0x118);
			tid = memory.getIntAt(address + 0x11C);			
		}
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.corereaders.j9.J9RASFragment#currentThreadSupported()
	 */
	public boolean currentThreadSupported() {
		return true;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.corereaders.j9.J9RASFragment#getPID()
	 */
	public long getPID() {
		return pid;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.corereaders.j9.J9RASFragment#getTID()
	 */
	public long getTID() {
		return tid;
	}

}
