/*******************************************************************************
 * Copyright (c) 2014, 2019 IBM Corp. and others
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
package com.ibm.j9ddr.corereaders.elf.unwind;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.PrintStream;
import java.util.HashMap;
import java.util.Map;
import java.util.Stack;

import javax.imageio.stream.ImageInputStream;
import javax.imageio.stream.MemoryCacheImageInputStream;

import com.ibm.j9ddr.corereaders.memory.IProcess;
import com.ibm.j9ddr.corereaders.memory.MemoryFault;

import static com.ibm.j9ddr.corereaders.elf.unwind.Unwind.readSignedLEB128;
import static com.ibm.j9ddr.corereaders.elf.unwind.Unwind.readUnsignedLEB128;

/* Class to hold the DWARF opcodes and operations to perform
 * in order to unwind a stack frame on Linux.
 * 
 * The different instructions and the rules they create are
 * described in:
 * DWARF Debugging Information Format, Version 3
 */
public class UnwindTable {

	private FDE fde;
	private Unwind unwinder;
	
	// Allow us to store the CFA rule and location in the rules table with the rules.
	// CFA (canonical frame address) is virtually a register.
	private static final int CFA_RULE_ID = Integer.valueOf(-1);
	
	
	private class RuleState {
		private RegisterOffsetCFARule cfaRule;
		private Map<Integer, RegisterRule> registerRules;
		private Map<Integer, RegisterRule> cieRegisterRules;
	}
	
	private RuleState state;
	
	// Not valid until we've finished unwinding.
	private long cfa = -1;
	private Long[] registers;
	
	private Stack<RuleState> ruleStack = new Stack<RuleState>();
	
	// DW_CFA op codes
	/* IMPORTANT - This list is incomplete.
	 * Not all opcodes are implemented. Mostly implementing unsupported opcodes is easy.
	 * ** Finding stacks that require specific opcodes is not! **
	 * Please do not add opcodes unless you have found a core dump that you can test them
	 * with. (Debugging incorrectly implemented opcodes is not going to be easy!)  
	 */
	private static class DW_CFA {
		static final int advance_loc = 0x40;
		static final int offset = 0x80;
		static final int restore = 0xc0;
		
		static final int nop = 0x00;
		static final int set_loc = 0x01;
		static final int advance_loc1 = 0x02;
		static final int advance_loc2 = 0x03;
		static final int advance_loc4 = 0x04;
		static final int offset_extended = 0x05;
		
		//static final int restore_extended = 0x6;
		
		static final int undefined = 0x7;
		static final int same_value = 0x8;
		static final int register = 0x9; 
		static final int remember_state = 0xa;
		static final int restore_state = 0xb;
		static final int def_cfa = 0xc;
		static final int def_cfa_register = 0xd;
		static final int def_cfa_offset = 0xe;
		
		//static final int def_cfa_expression = 0xf;
		//static final int expression = 0x10;
		
		static final int offset_extended_sf = 0x11;
		static final int def_cfa_sf = 0x12;
		
 		//static final int def_cfa_offset_sf = 0x13;

	}
	
	public UnwindTable(FDE fde, Unwind unwinder, long instructionPointer) throws IOException {
		
		this.fde = fde;
		this.unwinder = unwinder;
		
//		dumpInstructions(System.err, fde.cie.getInitialInstructions(), fde.cie);
//		dumpInstructions(System.err, fde.getCallFrameInstructions(), fde.cie);
		
		buildRuleTable(fde, instructionPointer);
		
	}
	
	private void buildRuleTable(FDE fde, long instructionPointer) throws IOException {

		PrintStream out = System.err;
		
		state = new RuleState();
		
		state.registerRules = new HashMap<Integer, RegisterRule>();

		{
//			out.printf("CIE has initial instructions:\n");
//			
//			String s = "";
//			for( byte b: fde.getCIE().getInitialInstructions() ) {
//				s += String.format("0x%x ", b);
//			}
//			out.println(s);
			
//			ByteBuffer initialInstructionStream = ByteBuffer.wrap(fde.getCIE().getInitialInstructions());
//			initialInstructionStream.order(fde.getCIE().byteOrder);
			
			InputStream i = new ByteArrayInputStream(fde.getCIE().getInitialInstructions());
			ImageInputStream initialInstructionStream = new MemoryCacheImageInputStream(i);
			initialInstructionStream.setByteOrder(fde.getCIE().byteOrder);
			
//			System.err.println("Initial Instructions:");

			while( initialInstructionStream.getStreamPosition() < fde.getCIE().getInitialInstructions().length) {
				// currentAddress is 0 as initialInstructions are always applied.
				processInstruction(/*out,*/ initialInstructionStream, fde.getCIE(), state, 0);
//				out.println();
			}
			state.cieRegisterRules = new HashMap<Integer, RegisterRule>();
			state.cieRegisterRules.putAll(state.registerRules); // We can copy the rules as rules are immutable. (Don't need a deep copy.) 
		}

		{
//			System.err.printf("FDE has instructions:\n");
//			
//			String s = "";
//			for( byte b: fde.getCallFrameInstructions()) {
//				s += String.format("0x%x ", b);
//			}
//			out.println(s);
					
			InputStream i = new ByteArrayInputStream(fde.getCallFrameInstructions());
			ImageInputStream instructionStream = new MemoryCacheImageInputStream(i);
			instructionStream.setByteOrder(fde.getCIE().byteOrder);
			
			Map<Integer, RegisterRule> fdeRules = new HashMap<Integer, RegisterRule>();
//			fdeRules.putAll(ruleRows.get(ruleRows.size()-1));
			long currentAddress = fde.getBaseAddress();
//			out.printf("@0x%x -\t", currentAddress);
//			System.err.println("FDE Instructions:");
			while( instructionStream.getStreamPosition() < fde.getCallFrameInstructions().length
					&& instructionPointer >= currentAddress ) {
				currentAddress = processInstruction(/*out,*/ instructionStream, fde.getCIE(), state, currentAddress);
//				out.println();
//				out.printf("@0x%x -\t", currentAddress);
			}
//			out.println();
		}
	}
	
	// Applies this unwind table to the given register set and returns the new set of
	// registers for the next call frame.
	public Map<String, Number> apply(Map<String, Number> elfRegisters, String[] registerMapping) throws MemoryFault {
		// Translate registers to DWARF ids.
		registers = new Long[registerMapping.length + 1]; // Add a space on the end for the CFA.
		int cfaIndex = registers.length - 1;
		for( int i = 0; i < registerMapping.length; i++ ) {
			String mappingName = null;
			Number regVal = null;
			mappingName = registerMapping[i];
			if( mappingName != null ) {
				regVal = elfRegisters.get(mappingName);
			}
			if( regVal != null) {
				registers[i] = elfRegisters.get(registerMapping[i]).longValue();
			} else {
				registers[i] = 0l;
			}
		}
		
		// Apply unwind codes.
		// - Apply all of initial instructions from CIE to setup register state
		// - Apply rules to registers while L1 > L2
		state.cfaRule.apply(registers, cfaIndex, unwinder.process);
//		System.err.printf("After cfa rule - CFA is 0x%x\n", registers[cfaIndex]);
		for( RegisterRule r: state.registerRules.values()) {
			if( r == null ) {
				continue;
			}
			r.apply(registers, cfaIndex, unwinder.process);
		}
		
		// Translate registers back from DWARF ids.
		Map<String, Number> newElfRegisters = new HashMap<String, Number>();
		for( int i = 0; i < registerMapping.length; i++ ) {
			newElfRegisters.put(registerMapping[i], registers[i]);
		}
		
//		System.err.printf("Returning new registers, cfa is: 0x%x (index %d)\n", registers[cfaIndex], cfaIndex);
		cfa = registers[cfaIndex];
		
		return newElfRegisters;
	}
	
	public long getFrameAddress() {
		return cfa;
	}
	
	public long getReturnAddress() {
//		System.err.println("Return address in register " + fde.cie.returnAddressRegister + " val is " + Long.toHexString(registers[(int)fde.cie.returnAddressRegister]));
		Long returnAddress = registers[(int) fde.cie.returnAddressRegister];
		return returnAddress == null ? 0 : returnAddress;
	}
	
	/*
	 * Dump the instructions, basically a copy of processInstruction with just the print
	 * statements left in.
	 * Some instructions create rules, others adjust the address the instructions 
	 * apply to and others save/restore state to a stack.
	 * See: DWARF Debugging Information Format, Version 3 section 6.4.2
	 * (Also extra instructions defined for Exception handling in:
	 * http://refspecs.linux-foundation.org/LSB_4.0.0/LSB-Core-generic/LSB-Core-generic/dwarfext.html
	 * )
	 */
	public static void dumpInstructions(PrintStream out, byte[] instructions, CIE cie) throws IOException {
		try {	
			InputStream i = new ByteArrayInputStream(instructions);
			ImageInputStream instructionStream = new MemoryCacheImageInputStream(i);
			instructionStream.setByteOrder(cie.byteOrder);
			
			while( instructionStream.getStreamPosition() < instructions.length ) {
				dumpInstruction(out, instructionStream, cie);
				out.println();
			}
		} catch (Exception e) {
			out.println("Error dumping instructions: " + e);
			e.printStackTrace(out);
		}
	}
	
	private static void dumpInstruction(PrintStream out, ImageInputStream instructionStream, CIE cie) throws IOException {
		int next = instructionStream.readByte();
		// The first 3 op codes use just the top two bits
		// Their arguments are encoding in the bottom six
//		out.printf("Looking at 0x%x\n", next & 0xc0);
//		out.printf("offset code: 0x%x, advance_loc code: 0x%x, restore code: 0x%x\n", DW_CFA.advance_loc, DW_CFA.offset, DW_CFA.restore);
		switch( next & 0xc0  ) {
			case DW_CFA.advance_loc: {
				byte delta = (byte)(next & 0x3f);
				out.printf("advance_loc: 0x%x, %d", delta, delta*cie.codeAlignmentFactor);
				return;
			}
			case DW_CFA.offset: {
				int register = (byte)(next & 0x3f);
				long offset = readUnsignedLEB128(instructionStream);
				// Need to multiply offset by data alignment.
				offset *= cie.dataAlignmentFactor;
				out.printf("offset: r%d offset cfa%+d", register, offset);
				return;
			}
			case DW_CFA.restore: {
				byte register = (byte)(next & 0x3f);
				out.printf("restore: r%d", register);
				return;
			}
		}

		// Process op codes that may take arguments as parameters.
		switch(next) {
			case DW_CFA.nop: {
				// A no-op, skip.
				break;
			}
			case DW_CFA.set_loc: {
				// One operand, an address
				long address;
				if( cie.wordSize == 8 ) {
					address = instructionStream.readLong();
				} else {
					address = instructionStream.readInt() & 0xFFFFFFFF;
				}
				out.printf("set_loc: 0x%x", address);
				break;
			}
			case DW_CFA.advance_loc1: {
				// One operand, 1 unsigned byte delta.
				long delta = ((long)instructionStream.readByte()) & 0xFF;
				out.printf("advance_loc1: 0x%x, %d", delta, delta*cie.codeAlignmentFactor);
				break;
			}
			case DW_CFA.advance_loc2: {
				// One operand, 1 unsigned 2 byte delta.
				long delta = ((long)instructionStream.readShort()) & 0xFFFF;
				out.printf("advance_loc2: 0x%x, %d", delta, delta*cie.codeAlignmentFactor);
				break;
			}
			case DW_CFA.advance_loc4: {
				// One operand, 1 unsigned 4 byte delta.
				long delta = ((long)instructionStream.readInt()) & 0xFFFFFFFF;
				out.printf("advance_loc4: 0x%x, %d", delta, delta*cie.codeAlignmentFactor);
				break;
			}
			case DW_CFA.offset_extended: {
				// Two operands, register and offset.
				long registerId = readUnsignedLEB128(instructionStream);
				long offset = readUnsignedLEB128(instructionStream);
				// Need to multiply offset by data alignment.
				offset *= cie.dataAlignmentFactor;
				out.printf("offset_extended: r%d offset cfa%+d", registerId, offset);
				break;
			}
			case DW_CFA.undefined: {
				// // One operand, a register
				long registerId = readUnsignedLEB128(instructionStream);
				out.printf("undefined: r%d ", registerId);
				break;
			}
			case DW_CFA.same_value: {
				// // One operand, a register
				long registerId = readUnsignedLEB128(instructionStream);
				out.printf("same_value: r%d ", registerId);
				break;
			}
			case DW_CFA.register: {
				// Two operands, a register and another register.
				long registerDest = readUnsignedLEB128(instructionStream);
				long registerSource = readUnsignedLEB128(instructionStream);
				out.printf("register: r%d is in r%d", registerDest, registerSource);
				break;
			}
			case DW_CFA.remember_state: {
				// No operands
				out.printf("remember_state");
				break;
			}
			case DW_CFA.restore_state: {
				// No operands
				out.printf("restore_state");
				break;
			}
			case DW_CFA.def_cfa: {
				// Two operands, register and offset.
				long registerId = readUnsignedLEB128(instructionStream);
				long offset = readUnsignedLEB128(instructionStream);
//				instruction = new DEF_CFA(registerId, offset);
				out.printf("def_cfa: r%d offset: %d", registerId, offset);
				break;
			}
			case DW_CFA.def_cfa_register: {
				// One operand, register
				long registerId = readUnsignedLEB128(instructionStream);
				// This rule needs the offset from the last rule
				// that affeted the CFA so we need to walk the list and find the
				// last instance of CFARule.
				out.printf("def_cfa_register: r%d", registerId);
				break;
			}
			case DW_CFA.def_cfa_offset: {
				// One operand, offset. (Use previous register)
				long offset = readUnsignedLEB128(instructionStream);
				out.printf("def_cfa_offset: offset: %d", offset);
				break;
			}
			case DW_CFA.def_cfa_sf: {
				// Two operands, register and offset.
				long registerId = readUnsignedLEB128(instructionStream);
				long offset = readSignedLEB128(instructionStream);
				long factoredOffset = offset*cie.dataAlignmentFactor;
				out.printf("def_cfa_sf: r%d offset: %d (%d)", registerId, offset, factoredOffset);
				break;
			}
				
//			case DW_CFA.def_cfa_expression: {
//				// One operand a DW_FORM_block (which is an unsigned LEB128 length followed
//				// by that many instructions).
//				long length = readUnsignedLEB128(instructionStream);
//				byte[]dest = new byte[(int)length];
//				instructionStream.read(dest);
//				String instructionBlock = Unwind.byteArrayToHexString(dest);
//				out.printf("def_cfa_expression: len %d block: %s", length, instructionBlock);
//				break;
//			}
//			case DW_CFA.expression: {
//				// Two operands, a register and a DW_FORM_block (which is an unsigned LEB128 length followed
//				// by that many instructions).
//				long registerId = readUnsignedLEB128(instructionStream);
//				long length = readUnsignedLEB128(instructionStream);
//				byte[]dest = new byte[(int)length];
//				instructionStream.read(dest);
//				String instructionBlock = Unwind.byteArrayToHexString(dest);
//				out.printf("expression: r%d len %d block: %s", registerId, length, instructionBlock);
//				break;
//			}
			case DW_CFA.offset_extended_sf: {
				// Two operands, register and offset.
				long registerId = readUnsignedLEB128(instructionStream);
				long offset = readSignedLEB128(instructionStream);
				// Need to multiply offset by data alignment.
				offset *= cie.dataAlignmentFactor;
				out.printf("offset_extended_sf: r%d offset cfa%+d", registerId, offset);
				break;
			}
			default: {
				out.printf("?? 0x%x", next);
				//throw new RuntimeException(String.format("Unhandled DWARF unwind code 0x%x found!", next));
			}
				// TODO - there are some special extra instructions for exception frames.
//				return null;
		}
//		out.print(rule.dump());
	}
	
	/*
	 * Process the instructions contained in the instruction stream of a CIE or FDE
	 * and build the table of rules that need to  be applied to restore register
	 * state.
	 * Some instructions create rules, others adjust the address the instructions 
	 * apply to and others save/restore state to a stack.
	 * See: DWARF Debugging Information Format, Version 3 section 6.4.2
	 * (Also extra instructions defined for Exception handling in:
	 * http://refspecs.linux-foundation.org/LSB_4.0.0/LSB-Core-generic/LSB-Core-generic/dwarfext.html
	 * )
	 */
	private long processInstruction(/*PrintStream out,*/ ImageInputStream instructionStream, CIE cie, RuleState currentState, long currentAddress) throws IOException {
		int next = instructionStream.readByte();
		// The first 3 op codes use just the top two bits
		// Their arguments are encoding in the bottom six
//		out.printf("Looking at 0x%x (0x%x)\n", next, next & 0xc0);
//		out.printf("offset code: 0x%x, advance_loc code: 0x%x, restore code: 0x%x\n", DW_CFA.advance_loc, DW_CFA.offset, DW_CFA.restore);
		switch( next & 0xc0  ) {
			case DW_CFA.advance_loc: {
				byte delta = (byte)(next & 0x3f);
//				out.printf("advance_loc: 0x%x, %d", delta, delta*cie.codeAlignmentFactor);
				return currentAddress + (delta*cie.codeAlignmentFactor);
			}
			case DW_CFA.offset: {
				int register = (byte)(next & 0x3f);
				long offset = readUnsignedLEB128(instructionStream);
				// Need to multiply offset by data alignment.
				offset *= cie.dataAlignmentFactor;
//				out.printf("offset: r%d offset cfa%+d", register, offset);
				currentState.registerRules.put(register, new CFAOffsetRegisterRule(register, offset));
				return currentAddress;
			}
			case DW_CFA.restore: {
				byte register = (byte)(next & 0x3f);
//				out.printf("restore: r%d", register);
				// TODO May need to handle these! (But I haven't encountered a case to test with yet.)
//				out.printf("***RESTORE INSTRUCTION!!!!****\n");
//				RegisterRule restoreRule = state.cieRegisterRules.get(register);
//				out.printf("Setting rule for r%d to %s\n", register, restoreRule);
//				RegisterRule oldRule = state.registerRules.put(register, restoreRule);
//				out.printf("Old rule was: %s (now %s)\n", oldRule, state.registerRules.get(register));
				return currentAddress;
			}
		}

		// Process op codes that may take arguments as parameters.
		switch(next) {
			case DW_CFA.nop: {
				// A no-op, skip.
				break;
			}
			case DW_CFA.set_loc: {
				// One operand, an address
				long address;
				if( cie.wordSize == 8 ) {
					address = instructionStream.readLong();
				} else {
					address = instructionStream.readInt() & 0xFFFFFFFF;
				}
//				out.printf("set_loc: 0x%x", address);
				currentAddress = address;
				break;
			}
			case DW_CFA.advance_loc1: {
				// One operand, 1 unsigned byte delta.
				long delta = ((long)instructionStream.readByte()) & 0xFF;
//				out.printf("advance_loc1: 0x%x, %d", delta, delta*cie.codeAlignmentFactor);
				currentAddress += (delta*cie.codeAlignmentFactor);
				break;
			}
			case DW_CFA.advance_loc2: {
				// One operand, 1 unsigned 2 byte delta.
				long delta = ((long)instructionStream.readShort()) & 0xFFFF;
//				out.printf("advance_loc2: 0x%x, %d", delta, delta*cie.codeAlignmentFactor);
				currentAddress += (delta*cie.codeAlignmentFactor);
				break;
			}
			case DW_CFA.advance_loc4: {
				// One operand, 1 unsigned 4 byte delta.
				long delta = ((long)instructionStream.readInt()) & 0xFFFFFFFF;
//				out.printf("advance_loc4: 0x%x, %d", delta, delta*cie.codeAlignmentFactor);
				currentAddress += (delta*cie.codeAlignmentFactor);
				break;
			}
			case DW_CFA.offset_extended: {
				// Two operands, register and offset.
				long registerId = readUnsignedLEB128(instructionStream);
				long offset = readUnsignedLEB128(instructionStream);
				// Need to multiply offset by data alignment.
				offset *= cie.dataAlignmentFactor;
//				out.printf("offset_extended: r%d offset cfa%+d", registerId, offset);
				currentState.registerRules.put((int)registerId, new CFAOffsetRegisterRule((int)registerId, offset));
				break;
			}
			case DW_CFA.undefined: {
				// // One operand, a register
				long registerId = readUnsignedLEB128(instructionStream);
//				out.printf("same_value: r%d ", registerId);
				currentState.registerRules.put((int)registerId, new UndefinedRegisterRule((int)registerId));
				break;
			}
			case DW_CFA.same_value: {
				// // One operand, a register
				long registerId = readUnsignedLEB128(instructionStream);
//				out.printf("same_value: r%d ", registerId);
				currentState.registerRules.put((int)registerId, new SameValueRegisterRule((int)registerId));
				break;
			}
			case DW_CFA.register: {
				// Two operands, a register and another register.
				long registerDest = readUnsignedLEB128(instructionStream);
				long registerSource = readUnsignedLEB128(instructionStream);
//				out.printf("register: r%d is in r%d", registerDest, registerSource);
				currentState.registerRules.put((int)registerDest, new RegisterRegisterRule((int)registerDest, (int)registerSource));
				break;
			}
			case DW_CFA.remember_state: {
				// No operands
				RuleState savedState = new RuleState();
				savedState.registerRules = new HashMap<Integer,RegisterRule>();
				savedState.cieRegisterRules = new HashMap<Integer,RegisterRule>();
				savedState.registerRules.putAll(currentState.registerRules); // We can copy the rules as rules are immutable. (Don't need a deep copy.)
				savedState.cieRegisterRules.putAll(currentState.cieRegisterRules);
				savedState.cfaRule = currentState.cfaRule;
				ruleStack.push(savedState);
//				out.printf("remember_state");
				break;
			}
			case DW_CFA.restore_state: {
				// No operands
				RuleState restoredState = ruleStack.pop();
				currentState.cfaRule = restoredState.cfaRule;
				currentState.registerRules = restoredState.registerRules;
				currentState.cieRegisterRules = restoredState.cieRegisterRules;
//				out.printf("restore_state");
				break;
			}
			case DW_CFA.def_cfa: {
				// Two operands, register and offset.
				long registerId = readUnsignedLEB128(instructionStream);
				long offset = readUnsignedLEB128(instructionStream);
//				instruction = new DEF_CFA(registerId, offset);
				currentState.cfaRule = new RegisterOffsetCFARule(CFA_RULE_ID, (int)registerId, offset);
//				out.printf("def_cfa: r%d offset: %d", registerId, offset);
				break;
			}
			case DW_CFA.def_cfa_register: {
				// One operand, register
				long registerId = readUnsignedLEB128(instructionStream);
				// This rule needs the offset from the last rule
				// that affeted the CFA so we need to walk the list and find the
				// last instance of CFARule.
				CFARule lastRule = (CFARule)currentState.cfaRule;
				currentState.cfaRule = new RegisterOffsetCFARule(CFA_RULE_ID, (int)registerId, lastRule.getOffset());
//				out.printf("def_cfa_register: r%d", registerId);
				break;
			}
			case DW_CFA.def_cfa_offset: {
				// One operand, offset. (Use previous register)
				long offset = readUnsignedLEB128(instructionStream);
//				out.printf("def_cfa_offset: offset: %d", offset);

				// This rule needs the register number from the last rule
				// that affeted the CFA so we need to walk the list and find the
				// last instance of CFARule.
				CFARule lastRule = (CFARule)currentState.cfaRule;
				currentState.cfaRule = new RegisterOffsetCFARule(CFA_RULE_ID, lastRule.getBaseRegister(), offset);
				break;
			}
			case DW_CFA.def_cfa_sf: {
				// Two operands, register and offset.
				long registerId = readUnsignedLEB128(instructionStream);
				long offset = readSignedLEB128(instructionStream);
				long factoredOffset = offset*cie.dataAlignmentFactor;
//				out.printf("def_cfa_sf: r%d offset: %d (%d)", registerId, offset, factoredOffset);
				currentState.cfaRule = new RegisterOffsetCFARule(CFA_RULE_ID, (int)registerId, factoredOffset);
				break;
			}
				
			/* Implementing these two operations means implementing DWARF expressions. */
//			case DW_CFA.def_cfa_expression: {
//				break;
//			}
//			case DW_CFA.expression: {
//				break;
//			}
			case DW_CFA.offset_extended_sf: {
				// Two operands, register and offset.
				long registerId = readUnsignedLEB128(instructionStream);
				long offset = readSignedLEB128(instructionStream);
				// Need to multiply offset by data alignment.
				offset *= cie.dataAlignmentFactor;
//				out.printf("offset_extended_sf: r%d offset cfa%+d", registerId, offset);
				currentState.registerRules.put((int)registerId, new CFAOffsetRegisterRule((int)registerId, offset));
				break;
			}
			default:
//				out.printf("?? 0x%x", next);
				// throw new RuntimeException(String.format("Unimplemented unwind instruction: 0x%x", next));
				// TODO - there are some special extra instructions for exception frames.
		}
		return currentAddress;
	}
	
	private static abstract class RegisterRule {
		
		private int registerNum;
		
		protected RegisterRule(int registerId) {
			this.registerNum = registerId;
		}
		
		/*
		 * Returns the register number that this rule applies to.
		 */
		public int getRegister() {
			return registerNum;
		}
		
		public abstract void apply(Long[] registers, int cfaIndex, IProcess process) throws MemoryFault;
		
	}
	

	
	/*
	 * Defines this registers value as being contained in another register.
	 */
	private static class RegisterRegisterRule extends RegisterRule {
		
		private int sourceRegisterNum;
		
		private RegisterRegisterRule(int registerNum, int sourceRegisterNum) {
			super(registerNum);
			this.sourceRegisterNum = sourceRegisterNum;
		}
		
		@Override
		public void apply(Long[] registers, int cfaIndex, IProcess process) throws MemoryFault {
			registers[getRegister()] = registers[sourceRegisterNum];
		}
		
	}
	
	/*
	 * Defines this registers value as unchanged (saved).
	 * Equivalent to saying (e.g) r1 is in r1, so extend RegisterRegisterRule.
	 */
	private static class SameValueRegisterRule extends RegisterRegisterRule {
	
		private SameValueRegisterRule(int registerNum) {
			super(registerNum, registerNum);
		}

	}
	
	/*
	 * Defines this registers value as undefined
	 */
	private static class UndefinedRegisterRule extends RegisterRule {
		
		private int sourceRegisterNum;
		
		private UndefinedRegisterRule(int registerNum) {
			super(registerNum);
		}
		
		@Override
		public void apply(Long[] registers, int cfaIndex, IProcess process) throws MemoryFault {
			registers[getRegister()] = null;
		}
		
	}
	
	/*
	 * Defines this registers value as being at a signed offset from the CFA.
	 */
	private static class CFAOffsetRegisterRule extends RegisterRule {
		
		private long offset;
		
		private CFAOffsetRegisterRule(int registerNum, long offset) {
			super(registerNum);
			this.offset = offset;
		}
		
		@Override
		public void apply(Long[] registers, int cfaIndex, IProcess process) throws MemoryFault {
			long cfa = registers[cfaIndex];
			long location = cfa + offset;
			registers[getRegister()] = process.getPointerAt(location);
		}
		
	}

	/*
	 * Parent class for rules that affect the CFA not a register. 
	 */
	private static abstract class CFARule extends RegisterRule {
		
		protected CFARule(int registerNum) {
			super(registerNum);
		}
		
		/*
		 * Return the offset that this rule used to add to the
		 * value in it's register.
		 */
		public abstract long getOffset();
		
		/*
		 * Return the register that this rule used as a base to
		 * add it's offset to.
		 */
		public abstract int getBaseRegister();
		
	}
	
	/*
	 * Defines this register (or cfa) as being 
	 */
	private static class RegisterOffsetCFARule extends CFARule {
		int register;
		long offset;
		
		private RegisterOffsetCFARule(int registerNum, int register, long offset ) {
			super(registerNum);
			this.register = register;
			this.offset = offset;
		}
		
		@Override
		public int getBaseRegister() {
			return register;
		}
		
		@Override
		public long getOffset() {
			return offset;
		}
		
		@Override
		// In this case getRegister() == cfaIndex should be true
		public void  apply(Long[] registers, int cfaIndex, IProcess process) {
			long regValue = registers[register];
			long cfaValue = regValue + offset;
			registers[cfaIndex] = cfaValue;
		}
		
	}
	
}
