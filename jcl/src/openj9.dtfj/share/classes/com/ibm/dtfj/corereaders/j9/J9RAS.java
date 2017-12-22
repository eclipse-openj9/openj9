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
 * Represents the basic J9RAS structure found in all Java versions
 	U_8 eyecatcher[8];
	U_32 bitpattern1;
	U_32 bitpattern2;
	I_32 version;
	I_32 length;
 * 
 * @author Adam Pilkington
 *
 */
public class J9RAS implements J9RASFragment {
	public static final String J9RAS_EYECATCHER = "J9VMRAS\0";
	public static final byte[] J9RAS_EYECATCHER_BYTES = J9RAS_EYECATCHER.getBytes();
	public static final long INTEGRITY_CHECK = 0xaa55aa55aa55aa55L;
	private long address = 0xDEADBEEFBAADF00DL;
	private long integrity = 0;		//this is the combined bitpattern1 and bitpattern2 fields
	private int version = -1;		//version of J9RAS structure
	private int length = -1;		//length of the J9RAS structure
	private boolean valid = false;	//flag to indicate if this structure is valid or not
	private J9RASFragment fragment = null;	//the underlying fragment which is specific to the version of the VM in the core file
	private Memory memory = null;	//reference to the enclosing address space - used for equality tests
	
	public J9RAS (Memory memory, long address) throws MemoryAccessException {
		this.address = address;
		integrity = memory.getLongAt(address + 8);	//read the integrity bytes
		valid = (integrity == INTEGRITY_CHECK);		//check to see if this is actually a J9RAS structure
		if(valid) {									//this is a valid J9RAS structure
			processStructure(memory);				//decide which version of the structure is present
			this.memory = memory;
		}
	}
	
	/**
	 * Processes the core J9RAS information (length and version) to determine which 
	 * underlying J9RAS fragment to create.
	 * 
	 *  32 bit
	 *  	Java 5 < SR10 : length < 0x120, version =  0x10000		No TID, PID or DDR
	 *  	Java 5 = SR10 : length = 0x120, version =  0x10000		TID + PID, No DDR
	 *  	Java 5 = SR12 : length = 0x128, version >= 0x20000		TID+PID+DDR (there is a 4 byte padding field present as well as the DDR pointer)
	 *  
	 *   	Java 6 < SR5  : length < 0x240, version =  0x10000		No TID, PID or DDR
	 *      Java 6 >=SR6  : length = 0x240, version =  0x10000		PID+TID, No DDR
	 *      Java 6 >=SR9  : length = 0x248, version >= 0x20000		PID+TID+DDR
	 *      Java 6 >=SR10 : length = 0x328, version >= 0x30000		PID+TID+DDR, long hostname
	 *      Java 7 = GA   : length = 0x328, version >= 0x30000		PID+TID+DDR, long hostname
	 *      
	 *  64 bit
	 *  	Java 5 < SR10 : length < 0x158, version =  0x10000		No TID, PID or DDR
	 *  	Java 5 = SR10 : length = 0x158, version =  0x10000		TID + PID, No DDR
	 *  	Java 5 = SR11 : length = 0x160, version >= 0x20000		TID+PID+DDR
	 *  
	 *      Java 6 = GA   : length < 0x278, version =  0x10000		No TID, PID or DDR
	 *      Java 6 >=SR6  : length = 0x278, version =  0x10000		PID+TID, No DDR
	 *      Java 6 >=SR9  : length = 0x280, version >= 0x20000		PID+TID+DDR
	 *      Java 6 >=SR10 : length = 0x360, version >= 0x30000		PID+TID+DDR + long hostname
	 *      Java 7 = GA   : length = 0x360, version >= 0x30000		PID+TID+DDR + long hostname
	 *  
	 * @param memory
	 * @throws MemoryAccessException
	 */
	private void processStructure(Memory memory) throws MemoryAccessException {
		version = memory.getIntAt(address + 16);	//get the J9RAS version
		length = memory.getIntAt(address + 20);		//get the length of the J9RAS structure
		if(memory.bytesPerPointer() == 4) {			//32 bit VM	(know this from the core file)
			process32bitStructure(memory);
		} else {									//64 bit VM (know this from the core file)
			process64bitStructure(memory);
		}
	}

	/**
	 * Determine the J9RAS fragment to use based on length of the J9RAS structure
	 * @param memory memory to use
	 * @throws MemoryAccessException re-thrown from the memory
	 */
	private void process64bitStructure(Memory memory) throws MemoryAccessException {
		
		if(length < 0x260) {						//Java 5
			if(version >= 0x20000) {
				//TID+PID+DDR
				fragment = new J9RASFragmentJ5v2(memory, address, true);
			} else {
				if(length == 0x158) {				//supported, no DDR
					fragment = new J9RASFragmentJ5SR10(memory, address, true);
				} else {							//not supported
					fragment = new J9RASFragmentJ5GA();
				}
			}
		} else {									//Java 6 or later
			if (version >= 0x30000) {
				//TID+PID+DDR + long hostname
				fragment = new J9RASFragmentJ6v3(memory, address, true);
			} else if (version >= 0x20000) {
				//TID+PID+DDR
				fragment = new J9RASFragmentJ6v2(memory, address, true);
			} else {
				if(length == 0x278) {				//supported, no DDR
					fragment = new J9RASFragmentJ6SR6(memory, address, true);
				} else {							//not supported
					fragment = new J9RASFragmentJ6GA();
				}
			}
		}
	}
	
	/**
	 * Determine the J9RAS fragment to use based on length of the J9RAS structure
	 * @param memory memory to use
	 * @throws MemoryAccessException re-thrown from the memory
	 */
	private void process32bitStructure(Memory memory) throws MemoryAccessException {
		if(length < 0x238) {						//Java 5
			if(version >= 0x20000) {
				//TID+PID+DDR
				fragment = new J9RASFragmentJ5v2(memory, address, false);
			} else {
				if(length == 0x120) {				//supported, no DDR
						fragment = new J9RASFragmentJ5SR10(memory, address, false);
				} else {							//not supported
					fragment = new J9RASFragmentJ5GA();	
				}
			}
		} else {									//Java 6 or later
			if (version >= 0x30000) {
				//TID+PID+DDR + long hostname
				fragment = new J9RASFragmentJ6v3(memory, address, false);
			} else if (version >= 0x20000) {
				//TID+PID+DDR
				fragment = new J9RASFragmentJ6v2(memory, address, false);
			} else {
				if(length == 0x240) {				//supported, no DDR
					fragment = new J9RASFragmentJ6SR6(memory, address, false);
				} else {							//not supported
					fragment = new J9RASFragmentJ6GA();
				}
			}
		}
	}
	
	/**
	 * Flag to indicate if this represents a valid J9RAS structure
	 * @return true if this is a valid structure
	 */
	public boolean isValid() {
		return valid;
	}
	
	/**
	 * Get the version of the J9RAS structure
	 * @return version
	 */
	public int getVersion() {
		return version;
	}

	/**
	 * Get the length of the J9RAS structure
	 * @return length
	 */
	public int getLength() {
		return length;
	}

	/**
	 * The address of this structure within the address space
	 * @return address
	 */
	public long getAddress() {
		return address;
	}
	
	/* (non-Javadoc)
	 * @see java.lang.Object#equals(java.lang.Object)
	 * J9RAS structures are considered equal if they have the same address within the same address space
	 */
	public boolean equals(Object obj) {
		if(obj instanceof J9RAS) {
			J9RAS ras = (J9RAS) obj;
			return (ras.address == address) && (ras.memory.equals(memory));
		}
		return false;
	}

	/* (non-Javadoc)
	 * @see java.lang.Object#hashCode()
	 */
	public int hashCode() {
		return super.hashCode();
	}

	/* (non-Javadoc)
	 * @see java.lang.Object#toString()
	 */
	public String toString() {
		return "J9RAS @0x" + Long.toHexString(address) + " length " + length + " version " + version;
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.corereaders.j9.J9RASFragment#currentThreadSupported()
	 */
	public boolean currentThreadSupported() {
		if(fragment == null) {
			return false;
		}
		return fragment.currentThreadSupported();
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.corereaders.j9.J9RASFragment#getPID()
	 */
	public long getPID() {
		if(valid && (fragment != null)) {
			return fragment.getPID();
		}
		throw new UnsupportedOperationException("Get PID() is not supported as this is not a valid J9RAS structure");
	}

	/* (non-Javadoc)
	 * @see com.ibm.dtfj.corereaders.j9.J9RASFragment#getTID()
	 */
	public long getTID() {
		if(valid && (fragment != null)) {
			return fragment.getTID();
		}
		throw new UnsupportedOperationException("Get TID() is not supported as this is not a valid J9RAS structure");
	}
	
	
}
