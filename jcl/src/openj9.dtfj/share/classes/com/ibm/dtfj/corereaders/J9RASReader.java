/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2009, 2017 IBM Corp. and others
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

package com.ibm.dtfj.corereaders;

import com.ibm.dtfj.addressspace.IAbstractAddressSpace;
import com.ibm.dtfj.corereaders.j9.J9RAS;
import com.ibm.dtfj.corereaders.j9.Memory;

/*
 * Starting from Java50 SR10 and Java6 SR5, the J9RAS structure has 2 new fields: PID and TID,
 * which correspond to the ID of the failing Java process and the ID of the failing thread.
 * This was done to work around a long-standing issue with some platforms (Linux, Windows), on which
 * the process or the thread that generates the core dump is not the same that fails.
 * See CMVC 149882.
 * 
 *  Client code should:
 *  
 *  1. instantiate this class;
 *  2. check whether PID and TID are supported, calling this.j9RASSupportsTidAndPid();
 *  3. if they are, get the PID and TID with the appropriate getters.
 *  
 *  Here's the J9RAS structure:  
 * 
	typedef struct J9RAS {
	U_8 eyecatcher[8];
	U_32 bitpattern1;
	U_32 bitpattern2;
	I_32 version;
	I_32 length;
	UDATA mainThreadOffset;
	UDATA j9threadNextOffset;
	UDATA osthreadOffset;
	UDATA idOffset;
	UDATA typedefsLen;
	UDATA typedefs;
	UDATA env;
	UDATA vm;
	U_64 buildID;
	U_8 osversion[128];
	U_8 osarch[16];
	U_8 osname[48];
	U_32 cpus;
	char _j9padding0124[4]; // 4 bytes of automatic padding 
	char** environment;
	U_64 memory;
	struct J9RASCrashInfo* crashInfo;
	U_8 hostname[32];
	U_8 ipAddresses[256];
	struct J9Statistic** nextStatistic;
	UDATA pid;
	UDATA tid;
} J9RAS; 

*/

/*
 * Jazz 18947 : spoole : Implemented support for Java 5
 * 
 * The structure above is the Java 6 version, the Java 5 version is smaller and as of SR11 contains an additional
 * field which is a pointer to the J9DDR data structures.
 */


public class J9RASReader {
	private Memory memory = null;			//memory in which a J9RAS structure may be located
	private J9RAS j9ras = null;				//J9RAS structure

	
	public J9RASReader (IAbstractAddressSpace space, boolean is64Bit) {
		memory = new Memory(space);			//create a new memory object based on the address space
	}
	
	/**
	 * Attempt to find a valid J9RAS structure in the memory
	 * @throws MemoryAccessException can be thrown by the Memory class when searching
	 */
	private void findJ9RAS() throws MemoryAccessException {
		if((j9ras == null) || (!j9ras.isValid())) {					//see if already found a structure, and if so that it is valid
			long address = memory.findPattern(J9RAS.J9RAS_EYECATCHER_BYTES, 0, 0);	//search for the J9RAS eyecatcher
			while(address != -1) {
				j9ras = new J9RAS(memory, address);			//found an eyecatcher
				if(j9ras.isValid()) return;					//found a valid J9RAS structure
				address = memory.findPattern(J9RAS.J9RAS_EYECATCHER_BYTES, 0, address + 1);		//keep searching
			}
		}
	}

	/**
	 * Get ID of the thread which caused the core file to be produced
	 * @return the thread ID
	 * @throws UnsupportedOperationException thrown if this operation is not supported for this version of the VM
	 * @throws MemoryAccessException thrown when searching the memory encounters an error
	 * @throws CorruptCoreException cannot find a valid J9RAS structure
	 */
	protected long getThreadID() throws UnsupportedOperationException, MemoryAccessException, CorruptCoreException {
		findJ9RAS();									//try and find a valid J9RAS structure
		if((j9ras == null) || (!j9ras.isValid())) {		//if couldn't find one throw an error
			throw new CorruptCoreException("Could not find a valid J9RAS structure in the core file");
		}
		return j9ras.getTID();							
	}
	
	/**
	 * Get the ID of the process which caused the core file to be produced
	 * @return the process ID
	 * @throws UnsupportedOperationException thrown if this operation is not supported for this version of the VM
	 * @throws MemoryAccessException thrown when searching the memory encounters an error
	 * @throws CorruptCoreException cannot find a valid J9RAS structure
	 */
	protected long getProcessID() throws UnsupportedOperationException, MemoryAccessException, CorruptCoreException {
		findJ9RAS();									//try and find a valid J9RAS structure
		if((j9ras == null) || (!j9ras.isValid())) {		//if couldn't find one throw an error
			throw new CorruptCoreException("Could not find a valid J9RAS structure in the core file");
		}
		return j9ras.getPID();
	}
	
}
