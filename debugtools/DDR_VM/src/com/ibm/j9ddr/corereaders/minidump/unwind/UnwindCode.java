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

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.corereaders.memory.IAddressSpace;
import com.ibm.j9ddr.corereaders.memory.MemoryFault;

public class UnwindCode {

	private long address;
	private UnwindModule module;
	private UnwindInfo info;
	private IAddressSpace process;
	
	// Offsets worked out from UNWIND_CODE in port\win64amd\j9signal.c
	// Also available from MSDN.
	private static final int OFFSET_CODEOFFSET = 0;
	private static final int OFFSET_UNWINDOP = 1; // First four bits.
	private static final int OFFSET_OPINFO = 1;   // Second four bits.
	private static final int OFFSET_SLOT1 = 2; // Next two slots, optional.
	private static final int OFFSET_SLOT2 = 4;
	
	final static String[] REGISTERS = {"RAX", "RCX", "RDX", "RBX", "RSP", "RBP", "RSI", "RDI",
		"R8", "R9", "R10", "R11", "R12", "R13", "R14", "R15"};
	
	public UnwindCode(IAddressSpace process, UnwindModule module, UnwindInfo info, long address) {
		this.process = process;
		this.module = module;
		this.info = info;
		this.address = address;
	}
	
	private byte getOffSet() throws MemoryFault {
		byte offset = (byte)(process.getByteAt(OFFSET_CODEOFFSET + module.getLoadAddress() + address));
		return offset;
	}
	
	private byte getOpCode() throws MemoryFault {
		byte code = (byte)(process.getByteAt(OFFSET_UNWINDOP + module.getLoadAddress() + address) & (byte)0x0F);
		return code;
	}
	
	private byte getOpInfo() throws MemoryFault {
		byte info = (byte)(process.getByteAt(OFFSET_OPINFO + module.getLoadAddress() + address) & (byte)0xF0);
		info = (byte)((info >> (byte)4) & 0x0F);
		return info;
	}
	
	private short getNode1() throws MemoryFault {
		short node = (short)(process.getShortAt(OFFSET_SLOT1 + module.getLoadAddress() + address));
		return node;
	}
	
	private short getNode2() throws MemoryFault {
		short node = (short)(process.getShortAt(OFFSET_SLOT2 + module.getLoadAddress() + address));
		return node;
	}
	
	/* Format the arguments for an operation. n1 and n2 are the next two
	 * slots in case this operation requires them.
	 * Full explanations of each op are on the msdn page for UNWIND_CODE here:
	 * http://msdn.microsoft.com/en-US/library/ck9asaa9%28v=vs.80%29
	 * The operations marked as untested haven't yet been found in any stack
	 * (and are quite rare to find in the unwind info for dll's at all) so
	 * if have found and corrected one please remove the untested label.
	 */
	public String formatOp() throws CorruptDataException {
		byte opCode = getOpCode();
		byte opInfo = getOpInfo();
		switch (opCode) {
		case (byte)0: //UWOP_PUSH_NONVOL
			return String.format("Op: PUSH_NONVOL, Info: 0x%x, register = %s", opInfo, REGISTERS[opInfo]); 
		case (byte)1: //UWOP_ALLOC_LARGE
			if( opInfo == 0 ) {
				return String.format("Op: ALLOC_LARGE, Info: 0x%x, size = 0x%x", opInfo, (getNode1()*8));
			} else {
				return String.format("Op: ALLOC_LARGE - Untested");
			}
		case (byte)2: //UWOP_ALLOC_SMALL
			return String.format("Op: ALLOC_SMALL, Info: 0x%x, size = 0x%x", opInfo, (opInfo*8) + 8); 
		case (byte)3: //UWOP_SET_FPREG
			return String.format("Op: SET_FPREG,Info: 0x%x, Set %s to  RSP + FrameOffset", opInfo, "FrameRegister"); // opInfo is reserved and should not be used for this operation.
		case (byte)4: //UWOP_SAVE_NONVOL
			return String.format("Op: SAVE_NONVOL, Info: 0x%x, register = %s, offset=0x%x", opInfo, REGISTERS[opInfo], (getNode1()*8)); 
		case (byte)5: //UWOP_SAVE_NONVOL_FAR
			return String.format("Op: SAVE_NONVOL_FAR, Info: 0x%x, register = %s, offset=0x%x", opInfo, REGISTERS[opInfo], ((int)(getNode1())) << 16 + (int)getNode2()); 
		case (byte)8: //UWOP_SAVE_XMM128
			return String.format("Op: SAVE_XMM128, Info: 0x%x, register = %s, offset=0x%x", opInfo, REGISTERS[opInfo], (getNode1()*16));
		case (byte)9: //UWOP_SAVE_XMM128_FAR
			return String.format("Op: SAVE_XMM128_FAR, Info:  register = %s, offset=0x%x", REGISTERS[opInfo], ((int)(getNode1())) << 16 + (int)getNode2());
		case (byte)10://UWOP_PUSH_MACHFRAME 
			if( opInfo == 0 ) {
				return "Op: UWOP_PUSH_MACHFRAME with no error code - Untested";
			} else {
				return "Op: UWOP_PUSH_MACHFRAME with error code - Untested";
			}
		default:
			//System.err.println("Unknown operation " + opCode);
			throw new CorruptDataException("Invalid stack unwind opcode " + opCode);
		}
	}
	
	/* Returns the number of unwind code slots this op
	 * occupies. Used to work out where the next opcode is.
	 */
	public int getNodeCount() throws CorruptDataException {
		byte opCode = getOpCode();
		byte opInfo = getOpInfo();
		switch (opCode) {
		case (byte)0: //UWOP_PUSH_NONVOL
			return 1; 
		case (byte)1: //UWOP_ALLOC_LARGE
			if( opInfo == 0 ) {
				return 2;
			} else {
				return 3;
			}
		case (byte)2: //UWOP_ALLOC_SMALL
			return 1;
		case (byte)3: //UWOP_SET_FPREG
			return 1;
		case (byte)4: //UWOP_SAVE_NONVOL
			return 2; 
		case (byte)5: //UWOP_SAVE_NONVOL_FAR
			return 3; 
		case (byte)8: //UWOP_SAVE_XMM128
			return 2; 
		case (byte)9: //UWOP_SAVE_XMM128_FAR
			return 3;  
		case (byte)10://UWOP_PUSH_MACHFRAME 
			return 1; 
		default:
			throw new CorruptDataException("Invalid stack unwind opcode " + opCode);
		}
	}

	/*
	 * To adjust the stack pointer we don't need to run the operation just
	 * work out by how much that operation adjusts the stack pointer.
	 * 
	 */
	/* TODO There's only about 8 operations but I've only some in real stacks so
	 * far and others only by dumping all the unwind info for all modules in
	 * a process. That's why several are marked untested. It's fairly easy to work
	 * out what to do from the docs here if you manage to find a stack containing untested
	 * unwind data:
	 * http://msdn.microsoft.com/en-US/library/ck9asaa9%28v=vs.80%29
	 * (If you fix one please remove the untested message.)
	 */
	public long getNewSP(long currentSP) throws CorruptDataException {
		byte opCode = getOpCode();
		byte opInfo = getOpInfo();
		switch (opCode) {
		case (byte)0: //UWOP_PUSH_NONVOL
			return currentSP + 8; // Decrements SP by 8, so return +8 
		case (byte)1: //UWOP_ALLOC_LARGE
			if( opInfo == 0 ) {
				return currentSP + (getNode1()*8); // Node1 contains alloc size / 8
			} else {
				//System.err.println("Found untested opcode UWOP_ALLOC_LARGE with opInfo != 0");
				throw new CorruptDataException("Untested stack unwind opcode " + opCode);
			}
		case (byte)2: //UWOP_ALLOC_SMALL
			return currentSP + ((opInfo*8) + 8); // Allocates (opInfo*8) + 8 bytes of stack space.
		case (byte)3: //UWOP_SET_FPREG
			//System.err.println("Found untested opcode UWOP_SET_FPREG");
			throw new CorruptDataException("Untested stack unwind opcode " + opCode);
		case (byte)4: //UWOP_SAVE_NONVOL
			return currentSP; // Saves in previously allocated space, SP doesn't change.
		case (byte)5: //UWOP_SAVE_NONVOL_FAR
			return currentSP; // Saves in previously allocated space, SP doesn't change.
		case (byte)8: //UWOP_SAVE_XMM128
			return currentSP; // Saves in previously allocated space, SP doesn't change.
		case (byte)9: //UWOP_SAVE_XMM128_FAR
			return currentSP; // Saves in previously allocated space, SP doesn't change.
		case (byte)10://UWOP_PUSH_MACHFRAME
			//System.err.println("Found untested opcode UWOP_PUSH_MACHFRAME");
			throw new CorruptDataException("Untested stack unwind opcode " + opCode);
		default:
			throw new CorruptDataException("Invalid stack unwind opcode " + opCode);
		}
	}
	
	public String toString() {
		try {
			return String.format("Offset 0x%02x : ", getOffSet()) + formatOp();
		} catch (MemoryFault e) {
			return "MemoryFault: " + e.getMessage();
		} catch (CorruptDataException e) {
			return "CorruptDataException: " + e.getMessage();
		}
	}
}
