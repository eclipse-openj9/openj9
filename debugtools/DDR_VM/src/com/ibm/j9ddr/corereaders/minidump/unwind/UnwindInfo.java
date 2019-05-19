/*******************************************************************************
 * Copyright (c) 2012, 2019 IBM Corp. and others
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

package com.ibm.j9ddr.corereaders.minidump.unwind;

import java.io.IOException;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.corereaders.memory.IAddressSpace;
import com.ibm.j9ddr.corereaders.memory.MemoryFault;

public class UnwindInfo {

	private static final int FLAGS_CHAINED = 0x4;
	private long address;
	private UnwindModule module;
	private IAddressSpace process;
	
	// Offsets worked out from UNWIND_INFO in port\win64amd\j9signal.c
	// Also available from MSDN.
	// First and second fields and fifth and sixth fields are bit fields.
	private static final int OFFSET_VERSION = 0; // 3 bits
	private static final int OFFSET_FLAGS = 0;   // 4 bits
	private static final int OFFSET_SIZEOFPROLOG = 1;
	private static final int OFFSET_COUNTOFCODES = 2;
	private static final int OFFSET_FRAMEREGISTER = 3; // 4 bits
	private static final int OFFSET_FRAMEOFFSET = 3;   // 4 bits
	private static final int OFFSET_UNWINDCODE = 4;
	
	/**
	 * Constructor for UnwindInfo, takes the address in
	 * the module where the unwind information can be found.
	 * @param address
	 * @throws CorruptDataException 
	 * @throws IOException 
	 */
	public UnwindInfo(IAddressSpace process, UnwindModule module, long address) throws CorruptDataException {
		this.address = address;
		this.module = module;
		this.process = process;
		byte version = getVersion();
		if( version != 1 ) {
			throw new CorruptDataException(String.format("Incorrect version number %d in UNWIND_INFO at 0x&08x", version, address));
		}
	}
	
	/* The version is actually only the first 3 bits and should always
	 * be 1;
	 */
	private byte getVersion() throws MemoryFault {
		byte version = (byte)(process.getByteAt(OFFSET_VERSION + module.getLoadAddress() + address) & (byte)0x7);
		return version;
	}

	private byte getFlags() throws MemoryFault {
		byte flags = (byte)(process.getByteAt(OFFSET_FLAGS + module.getLoadAddress() + address) & (byte)0xf8);
		flags = (byte)(flags >> 3);
		return flags;
	}

	private byte getPrologSize() throws MemoryFault {
		byte prologSize = (byte)process.getByteAt(OFFSET_SIZEOFPROLOG + module.getLoadAddress() + address);
		return prologSize;
	}

	
	private byte getCountOfCodes() throws MemoryFault {
		byte codeCount = (byte)process.getByteAt(OFFSET_COUNTOFCODES + module.getLoadAddress() + address);
		return codeCount ;
	}
	
	public String toString() {
		String str = "";
		try {
			byte version = getVersion();
			byte flags = getFlags();
			byte prologSize = getPrologSize();
			byte countOfCodes = getCountOfCodes();
			str += String.format("Version: 0x%x, flags 0x%x, SizeOfProlog 0x%02x, CountOfCodes %d", version, flags, prologSize, countOfCodes );
			long unwindCodeAddr = OFFSET_UNWINDCODE + address;
			for ( byte codeIndex = 0; codeIndex < countOfCodes; ) { 
				UnwindCode c = new UnwindCode(process, module, this, unwindCodeAddr);
				str += "\n\t";
				str += c.toString();
				codeIndex += c.getNodeCount();
				unwindCodeAddr += 2*c.getNodeCount();// An unwind code is 2 bytes.
			}
			return str;
		} catch (MemoryFault mf ) {
			return "Error getting UnwindInfo " + mf.toString();
		} catch (CorruptDataException e) {
			return "Error getting UnwindInfo " + e.toString();
		}
	}

	/*
	 * Documentation on applying the unwind procedure:
	 * http://msdn.microsoft.com/en-us/library/8ydc79k6%28v=vs.110%29.aspx
	 */
	public long apply(long stackPointer) throws CorruptDataException {

		// Need to account for no codes and chained data.
		byte version = getVersion();
		if( version != 1 ) {
			// The version number should always be 1, if it goes up we won't know what to
			// do with the data anyway.
			throw new CorruptDataException("Invalid version " + version + " in UNWIND_INFO expected 1");
		}
		byte flags = getFlags();
		byte prologSize = getPrologSize();
		byte countOfCodes = getCountOfCodes();
		
		if( countOfCodes == 0 ) {
			// Just return stackPointer.
			// System.err.println("No unwind codes, returning rsp");
		} else {
			// Hopefully we are just interested in the effect on the stack pointer.
			long unwindCodeAddr = OFFSET_UNWINDCODE + address;

			for ( byte codeIndex = 0; codeIndex < countOfCodes; ) { 
				//System.err.println(String.format("Stack pointer is: 0x%08x", stackPointer));
				UnwindCode c = new UnwindCode(process, module, this, unwindCodeAddr);

				//System.err.println("Applying: " + c.formatOp());
				stackPointer = c.getNewSP(stackPointer);
				codeIndex += c.getNodeCount();
				unwindCodeAddr += 2*c.getNodeCount();// An unwind code is 2 bytes.
			}
		}
		if( (flags & FLAGS_CHAINED) == FLAGS_CHAINED) {
			UnwindInfo chainedInfo = getChainedUnwindInfo();
			//System.err.println("Applying chained info: " + chainedInfo);
			stackPointer = chainedInfo.apply(stackPointer);
		}
		return stackPointer;
	}

	private UnwindInfo getChainedUnwindInfo() throws CorruptDataException {
		// This is a runtime function entry that comes after the unwind codes, address + OFFSET_UNWINDCODE
		// Unwind codes are two bytes, (getCountOfCodes() * 2)
		// The address of the next unwind info is the third int field in that structure, (2*4)  
		long chainedAddress = process.getIntAt(module.getLoadAddress() + address + OFFSET_UNWINDCODE + (getCountOfCodes() * 2) + (2*4));
		UnwindInfo chainedEntry = new UnwindInfo(process, module, chainedAddress);
		return chainedEntry;
	}


	
}
