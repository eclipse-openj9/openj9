/*******************************************************************************
 * Copyright (c) 2009, 2018 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.j9.stackmap;

import static com.ibm.j9ddr.vm29.j9.BCNames.JBdup;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBdup2;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBdup2x1;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBdup2x2;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBdupx1;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBdupx2;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBgetfield;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBgetstatic;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBgoto;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBinvokeinterface;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBinvokeinterface2;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBinvokespecial;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBinvokespecialsplit;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBinvokestatic;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBinvokestaticsplit;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBinvokevirtual;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBinvokehandle;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBinvokehandlegeneric;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBldc;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBldcw;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBmultianewarray;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBputfield;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBwithfield;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBputstatic;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBswap;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBtableswitch;
import static com.ibm.j9ddr.vm29.j9.PCStack.J9JavaInstructionSizeAndBranchActionTable;
import static com.ibm.j9ddr.vm29.j9.PCStack.JavaStackActionTable;
import static com.ibm.j9ddr.vm29.j9.ROMHelp.J9EXCEPTIONINFO_HANDLERS;
import static com.ibm.j9ddr.vm29.j9.ROMHelp.J9_BYTECODE_SIZE_FROM_ROM_METHOD;
import static com.ibm.j9ddr.vm29.j9.ROMHelp.J9_BYTECODE_START_FROM_ROM_METHOD;
import static com.ibm.j9ddr.vm29.j9.ROMHelp.J9_EXCEPTION_DATA_FROM_ROM_METHOD;
import static com.ibm.j9ddr.vm29.j9.stackmap.MapHelpers.PARAM_16;
import static com.ibm.j9ddr.vm29.j9.stackmap.MapHelpers.PARAM_32;
import static com.ibm.j9ddr.vm29.j9.stackmap.MapHelpers.PARAM_8;
import static com.ibm.j9ddr.vm29.structure.J9JavaAccessFlags.J9AccMethodHasExceptionInfo;
import static java.util.logging.Level.FINE;
import static java.util.logging.Level.FINER;
import static java.util.logging.Level.FINEST;

import java.util.Stack;
import java.util.logging.Logger;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.AlgorithmVersion;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ExceptionHandlerPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ExceptionInfoPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMConstantPoolItemPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMFieldRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMMethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMMethodRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9UTF8Pointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9UTF8Helper;
import com.ibm.j9ddr.vm29.types.I16;
import com.ibm.j9ddr.vm29.types.I32;
import com.ibm.j9ddr.vm29.types.U16;
import com.ibm.j9ddr.vm29.types.UDATA;

/**
 * The StackMap determines the shape of the operand stack at a
 * particular point in the code. This is used by the stackwalker
 * to figure out which stack slots hold objects and which hold
 * primitive data for each frame.
 * 
 * LocalMap does a similar job for the local/automatic stack variables.
 * 
 * @author andhall
 *
 */
public class StackMap
{
	private static Logger logger = Logger.getLogger("j9ddr.stackwalker.stackmap");

	public static int
	j9stackmap_StackBitsForPC(UDATA pc, J9ROMClassPointer romClass, J9ROMMethodPointer romMethod,
									int[] resultsArray, int resultArraySize) throws CorruptDataException
	{
		IStackMap map = getImpl();
		
		return map.j9stackmap_StackBitsForPC(pc, romClass, romMethod, resultsArray, resultArraySize);
	}

	private static IStackMap impl;
	
	private static IStackMap getImpl()
	{
		if (impl == null) {
			impl = getImpl(AlgorithmVersion.STACK_MAP_VERSION);
		}
		return impl;
	}
	
	private static IStackMap getImpl(String algorithmID) {
		AlgorithmVersion version = AlgorithmVersion.getVersionOf(algorithmID);
		switch (version.getAlgorithmVersion()) {
			// Add case statements for new algorithm versions
			default:
				return new StackMap_V1();
		}
	}

	private static interface IStackMap
	{
		public int
		j9stackmap_StackBitsForPC(UDATA pc, J9ROMClassPointer romClass, J9ROMMethodPointer romMethod,
										int[] resultsArray, int resultArraySize) throws CorruptDataException;
	}
	
	/**
	 * Represents Java operand stack.
	 * 
	 * Integers or objects are pushed on and popped off.
	 * 
	 * A stack of these stacks is maintained to keep track of branches.
	 * 
	 * @author andhall
	 *
	 */
	private static class J9MappingStack
	{
		/* Program counter - used for maintaining branch state */
		public int pc;
		
		private int topOfStack = 0;
		private byte[] stack;
		
		public J9MappingStack(int stackSize)
		{
			this.stack = new byte[stackSize];
		}
		
		/**
		 * Copy constructor. Used when branching (when we need to store
		 * a copy of the existing stack and reuse it)
		 * @param toClone
		 */
		public J9MappingStack(J9MappingStack toClone)
		{
			this.stack = new byte[toClone.stack.length];
			
			System.arraycopy(toClone.stack, 0, this.stack, 0, toClone.topOfStack);
			
			this.topOfStack = toClone.topOfStack;
		}
		
		public void PUSH(byte value) throws CorruptDataException
		{
			if (topOfStack >= stack.length) {
				throw new CorruptDataException("Operand stack overflow. Stack size = " + stack.length + ", top of stack " + topOfStack);
			}
			
			stack[topOfStack++] = value;
		}
		
		public byte POP() throws CorruptDataException
		{
			if (topOfStack == 0) {
				throw new CorruptDataException("Operand stack underflow in StackMap");
			}
			
			return stack[--topOfStack];
		}
		
		public void reset()
		{
			topOfStack = 0;
		}
		
		public int stackSize()
		{
			return topOfStack;
		}
	}
	
	private static class StackMap_V1 implements IStackMap
	{
		/* Constants */
		public static final byte INT = 0x0;
		public static final byte OBJ = 0x1;
		
		public static final byte WALKED = 0x1;
		public static final byte STACK_REQUEST = 0x2;
		
		public static final int BCT_ERR_NO_ERROR = 0;
		public static final int BCT_ERR_STACK_MAP_FAILED = -5;

		
		/* Fields */
		
		/* These are used instead of pass by reference between functions. */
		
		/* Variable to pass back the result from mapStack to j9stackmap_StackBitsForPC. */
		private J9MappingStack resultStack;
		
		/* Current stack */
		private J9MappingStack startStack;
		
		/* Stack currently being used by mapStack */
		private J9MappingStack liveStack;
		
		/* Pointers for walking bytecodes. Manipulated by pushStack() and popStack() */
		private U8Pointer bcStart, bcIndex, bcEnd;
		
		/* Number of exceptions for current method */
		private int exceptionsToWalk;
		
		private J9ExceptionInfoPointer exceptionData;
		
		/**
		 * Stack for keeping track of branches. (the stack of stacks)
		 */
		private Stack<J9MappingStack> branchStack = new Stack<J9MappingStack>();
	
		protected StackMap_V1() {
			super();
		}
		
		public int j9stackmap_StackBitsForPC(UDATA pc,
				J9ROMClassPointer romClass, J9ROMMethodPointer romMethod,
				int[] resultsArray, int resultArraySize) throws CorruptDataException
		{
			UDATA length;
			U16 stackStructSize;
			byte[] map;
			int rc;
			
			logger.logp(FINE,"StackMap_V1","j9stackmap_StackBitsForPC","romClass={0}, romMethod={1}, pc={2}",new Object[]{Long.toHexString(romClass.getAddress()),
																															Long.toHexString(romMethod.getAddress()),
																															pc});
			
			//This method differs from the native significantly because we don't allocate the scratch array here.
			//Scratch storage is held on-heap in stack data structures and managed inside mapStack.
			
			//Build map array - one slot for every PC in method
			length = J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod);
			
			stackStructSize = romMethod.maxStack();
			
			map = new byte[length.intValue()];
			
			/* Flag the map target bytecode */
			map[pc.intValue()] = STACK_REQUEST;

			if ((rc = mapStack(stackStructSize, map, romClass, romMethod)) == BCT_ERR_NO_ERROR) {
				rc = outputStackMap(resultsArray, resultArraySize);
			}
			
			return rc;
		}
		
		/**
		 * Steps through the bytecode, maintaining the stack shape as we go. Walk until we hit the PC whose
		 * map value is STACK_REQUEST.
		 */
		private int mapStack(U16 totalStack, byte[] map,
				J9ROMClassPointer romClass, J9ROMMethodPointer romMethod) throws CorruptDataException
		{
			int pc;
			UDATA length;
			int target;
			int bc;
			int size, action;
			byte type;
			int offset;
			exceptionData = J9_EXCEPTION_DATA_FROM_ROM_METHOD(romMethod);
			int rc;
			int temp1, temp2, temp3;
			int high, low, npairs;
			int index;
			char signature;
			J9ROMConstantPoolItemPointer pool = J9ROMConstantPoolItemPointer.cast(romClass.add(1));
			J9UTF8Pointer utf8Signature;

			if (romMethod.modifiers().anyBitsIn(J9AccMethodHasExceptionInfo)) {
				exceptionsToWalk = exceptionData.catchCount().intValue();
			}

			/* initialize the first stack */
			startStack = new J9MappingStack(totalStack.intValue());
			liveStack = startStack;
			
			bcStart = J9_BYTECODE_START_FROM_ROM_METHOD(romMethod);
			bcIndex = bcStart;
			length = J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod);
			bcEnd = bcStart.add(length);
			
			while (bcIndex.lt(bcEnd)) {
				pc = (int)bcIndex.sub(UDATA.cast(bcStart)).getAddress();
				bc = bcIndex.at(0).intValue();
			
				if (map[pc] != 0) {
					/* See if this is the target pc */
					if ((map[pc] & STACK_REQUEST) != 0) {
						resultStack = liveStack;
						return BCT_ERR_NO_ERROR;
					}
					/* We have been here before, stop this scan */
					if ( (rc = nextRoot()) != BCT_ERR_NO_ERROR) {
						return rc;
					} else {
						continue;
					}
				}
				
				map[pc] = WALKED;
				
				size = 0xFF & J9JavaInstructionSizeAndBranchActionTable[bc];
				action = 0xFF & JavaStackActionTable[bc];
				
				if (logger.isLoggable(FINER)) {
					logger.logp(FINER,"StackMap_V1","mapStack","bcIndex=0x{0} pc={1}, bc={2}, size={3}, action=0x{4}", new Object[]{Long.toHexString(bcIndex.getAddress()),pc,bc,size,Long.toHexString(action)});
				}
				
				/* action encodes pushes and pops by the bytecode - 0x80 means something special */
				if (action != 0x80) {
					for (int i=0;i!=(action&7);i++) {
						POP();
					}

					if (action >= 0x10) {
						if (action >= 0x50) {
							PUSH(OBJ);
						} else {
							PUSH(INT);
							if (action >= 0x20) {
								PUSH(INT);
							}
						}
					}
						
					if ((size >>> 4) == 0) {
						bcIndex = bcIndex.add(size);
						if (size == 0) {
							/* Unknown bytecode */
							return BCT_ERR_STACK_MAP_FAILED;
						}
						continue;
					}
					
					switch (size >> 4) {

					case 1:			/* ifs */
						offset = new I16(PARAM_16(bcIndex, 1)).intValue();
						target = pc + offset;
						if ((map[target] & WALKED) == 0) {
							liveStack = pushStack(target);
						}
						bcIndex = bcIndex.add(size & 7);
						continue;

					case 2:			/* gotos */
						if (bc == JBgoto) {
							offset = new I16(PARAM_16(bcIndex, 1)).intValue();
							target = (pc + offset);
						} else {
							offset = new I32(PARAM_32(bcIndex, 1)).intValue();
							target = pc + offset;
						}
						bcIndex = bcStart.add(target);
						continue;

					case 4:			/* returns/athrow */
						if ( (rc = nextRoot()) != BCT_ERR_NO_ERROR) {
							return rc;
						} else {
							continue;
						}

					case 5:			/* switches */
						bcIndex = bcIndex.add(4 - (pc & 3));
						offset = new I32(PARAM_32(bcIndex, 0)).intValue();
						bcIndex = bcIndex.add(4);
						temp2 = pc + offset;
						low = new I32(PARAM_32(bcIndex, 0)).intValue();
						bcIndex = bcIndex.add(4);

						if (bc == JBtableswitch) {
							high = new I32(PARAM_32(bcIndex, 0)).intValue();
							bcIndex = bcIndex.add(4);
							npairs = (high - low + 1);
							temp1 = 0;
						} else {
							npairs = low;
							/* used to skip over the tableswitch key entry */
							temp1 = 4;
						}

						for (; npairs > 0; npairs--) {
							bcIndex = bcIndex.add(temp1);
							offset = new I32(PARAM_32(bcIndex, 0)).intValue();
							bcIndex = bcIndex.add(4);
							target = pc + offset;
							if ((map[target] & WALKED) == 0) {
								liveStack = pushStack(target);
							}
						}

						/* finally continue at the default switch case */
						bcIndex = bcStart.add(temp2);
						continue;

					case 7:			/* breakpoint */
						/* Unexpected bytecode - unknown */
						return BCT_ERR_STACK_MAP_FAILED;

					default:
						bcIndex.add(size & 7);
						continue;

					}
				} else {
					if ((bc == JBldc) || (bc == JBldcw)) {
						if (bc == JBldc) {
							index = PARAM_8(bcIndex, 1).intValue();
						} else {
							index = PARAM_16(bcIndex, 1).intValue();
						}

						if (pool.add(index).slot2().longValue() != 0) {
							PUSH(OBJ);
						} else {
							PUSH(INT);
						}
					} else if (bc == JBdup) {
						type = POP();
						PUSH(type);
						PUSH(type);
					} else if (bc == JBdupx1) {
						type = POP();
						temp1 = POP();
						PUSH(type);
						PUSH(temp1);
						PUSH(type);
					} else if (bc == JBdupx2) {
						type = POP();
						temp1 = POP();
						temp2 = POP();
						PUSH(type);
						PUSH(temp2);
						PUSH(temp1);
						PUSH(type);
					} else if (bc == JBdup2) {
						temp1 = POP();
						temp2 = POP();
						PUSH(temp2);
						PUSH(temp1);
						PUSH(temp2);
						PUSH(temp1);
					} else if (bc == JBdup2x1) {
						type = POP();
						temp1 = POP();
						temp2 = POP();
						PUSH(temp1);
						PUSH(type);
						PUSH(temp2);
						PUSH(temp1);
						PUSH(type);
					} else if (bc == JBdup2x2) {
						type = POP();
						temp1 = POP();
						temp2 = POP();
						temp3 = POP();
						PUSH(temp1);
						PUSH(type);
						PUSH(temp3);
						PUSH(temp2);
						PUSH(temp1);
						PUSH(type);
					} else if (bc == JBswap) {
						type = POP();
						temp1 = POP();
						PUSH(type);
						PUSH(temp1);
					} else if (bc == JBgetfield) {
						POP();
						index = PARAM_16(bcIndex, 1).intValue();
						utf8Signature = J9ROMFieldRefPointer.cast(pool.add(index)).nameAndSignature().signature();
						
						signature = J9UTF8Helper.stringValue(utf8Signature).charAt(0);
						if ((signature == 'L') || (signature == '[')) {
							PUSH(OBJ);
						} else {
							PUSH(INT);
							if ((signature == 'J') || (signature == 'D')) {
								PUSH(INT);
							}
						}
					} else if (bc == JBgetstatic) {
						index = PARAM_16(bcIndex, 1).intValue();
						utf8Signature = J9ROMFieldRefPointer.cast(pool.add(index)).nameAndSignature().signature();
						
						signature = J9UTF8Helper.stringValue(utf8Signature).charAt(0);
						if ((signature == 'L') || (signature == '[')) {
							PUSH(OBJ);
						} else {
							PUSH(INT);
							if ((signature == 'J') || (signature == 'D')) {
								PUSH(INT);
							}
						}
					} else if (bc == JBputfield) {
						POP();
						index = PARAM_16(bcIndex, 1).intValue();
						utf8Signature = J9ROMFieldRefPointer.cast(pool.add(index)).nameAndSignature().signature();
						signature = J9UTF8Helper.stringValue(utf8Signature).charAt(0);
						POP();
						if ((signature == 'D') || (signature == 'J')) {
							POP();
						}
					} else if ((bc == JBputstatic) || (bc == JBwithfield)) {
						index = PARAM_16(bcIndex, 1).intValue();
						utf8Signature = J9ROMFieldRefPointer.cast(pool.add(index)).nameAndSignature().signature();
						signature = J9UTF8Helper.stringValue(utf8Signature).charAt(0);
						POP();
						if ((signature == 'D') || (signature == 'J')) {
							POP();
						}
					} else if (bc == JBinvokeinterface2) {
						bcIndex = bcIndex.add(2);
						continue;
					} else if ((bc == JBinvokehandle) 
						|| (bc == JBinvokehandlegeneric) 
						|| (bc == JBinvokevirtual) 
						|| (bc == JBinvokespecial) 
						|| (bc == JBinvokespecialsplit)
						|| (bc == JBinvokeinterface)
					) {
						POP();
						index = PARAM_16(bcIndex, 1).intValue();
						if (bc == JBinvokestaticsplit) {
							index = romClass.staticSplitMethodRefIndexes().at(index).intValue();
						} else if (bc == JBinvokespecialsplit) {
							index = romClass.specialSplitMethodRefIndexes().at(index).intValue();
						}
						utf8Signature = J9ROMMethodRefPointer.cast(pool.add(index)).nameAndSignature().signature();
						char[] args = J9UTF8Helper.stringValue(utf8Signature).toCharArray();
						int i = 0;
						
						for (i = 1; args[i] != ')'; i++) {
							POP();
							if (args[i] == '[') {
								while (args[++i] == '[');
								if (args[i] != 'L') {
									continue;
								}
							}
							if (args[i] == 'L') {
								while (args[++i] != ';');
								continue;
							}
							if ((args[i] == 'D') || (args[i] == 'J')) {
								POP();
							}
						}

						signature = args[i + 1];
						if (signature != 'V') {
							if ((signature == 'L') || (signature == '[')) {
								PUSH(OBJ);
							} else {
								PUSH(INT);
								if ((signature == 'J') || (signature == 'D')) {
									PUSH(INT);
								}
							}
						}
						if (bc == JBinvokeinterface2) {
							bcIndex = bcIndex.sub(2);
						}
					} else if ((bc == JBinvokestatic)
						|| (bc == JBinvokestaticsplit)
					) {
						index = PARAM_16(bcIndex, 1).intValue();
						if (bc == JBinvokestaticsplit) {
							index = romClass.staticSplitMethodRefIndexes().at(index).intValue();
						} else if (bc == JBinvokespecialsplit) {
							index = romClass.specialSplitMethodRefIndexes().at(index).intValue();
						}
						utf8Signature = J9ROMMethodRefPointer.cast(pool.add(index)).nameAndSignature().signature();
						char[] args = J9UTF8Helper.stringValue(utf8Signature).toCharArray();
						int i = 0;
						
						for (i = 1; args[i] != ')'; i++) {
							POP();
							if (args[i] == '[') {
								while (args[++i] == '[');
								if (args[i] != 'L') {
									continue;
								}
							}
							if (args[i] == 'L') {
								while (args[++i] != ';');
								continue;
							}
							if ((args[i] == 'D') || (args[i] == 'J')) {
								POP();
							}
						}

						signature = args[i + 1];
						if (signature != 'V') {
							if ((signature == 'L') || (signature == '[')) {
								PUSH(OBJ);
							} else {
								PUSH(INT);
								if ((signature == 'J') || (signature == 'D')) {
									PUSH(INT);
								}
							}
						}
						if (bc == JBinvokeinterface2) {
							bcIndex = bcIndex.sub(2);
						}
					} else if (bc == JBmultianewarray) {
						index = PARAM_8(bcIndex, 3).intValue();
						for (int i=0; i < index; i++) {
							POP();
						}
						//Checking from C removed. If we've overflowed, POP() would have thrown an exception
						PUSH(OBJ);
						break;
					} 
					bcIndex = bcIndex.add(size & 7);
					continue;
				}
				/*
				 * case JBinvokedynamic:
				 * The case for the JBinvokedynamic byte code is unimplemented as the there is no way
				 * to test a port of the C code to Java without a suitable dump and the code is not
				 * trivial enough for a simple port to be safe.
				 * If an suitable dump can be found then we need to port the code in the equivalent
				 * switch statement in :
				 * runtime/stackmap/stackmap.c
				 */
			}
		
		
			return BCT_ERR_STACK_MAP_FAILED;					/* Fell off end of bytecode array - should never get here */
		}


		/**
		 * Stores a stack to follow a branch onto the branch stack
		 * 
		 * Resets liveStack.
		 * 
		 * Only returns to make the Java look more like the C
		 */
		private J9MappingStack pushStack(int targetPC)
		{
			logger.logp(FINEST,"StackMap_V1","pushStack","pushStack");
			liveStack.pc = targetPC;
			branchStack.push(liveStack);
			
			//Clone the existing stack to keep going
			liveStack = new J9MappingStack(liveStack);
			
			return liveStack;
		}
		
		/**
		 * Inverse of pushstack
		 */
		private void popStack()
		{
			logger.logp(FINEST,"StackMap_V1","popStack","popStack");
			liveStack = branchStack.pop();
			bcIndex = bcStart.add(liveStack.pc);
		}
		
		/**
		 * Implementation of _nextRoot goto in mapStack.
		 * Returns BCT_ERR_NO_ERROR if we successfully moved to a new branch, BCT_ERR_STACK_MAP_FAILED if we didn't (PC not found)
		 */
		private int nextRoot() throws CorruptDataException
		{
			if (liveStack == startStack) {
				if (exceptionsToWalk != 0) {
					/* target PC not found in regular code - try exceptions */
					J9ExceptionHandlerPointer handler = J9EXCEPTIONINFO_HANDLERS(exceptionData);

					/* exceptions are initialized with stacks containing only an object (the exception object) */
					liveStack.reset();
					PUSH(OBJ);

					while (exceptionsToWalk > 0) {
						/* branch all exceptions */
						pushStack(handler.add(--exceptionsToWalk).handlerPC().intValue());
					}

				} else {
					/* PC not found - possible if asked to map dead code return by shared class loading */
					return BCT_ERR_STACK_MAP_FAILED;
				}
			}
			
			popStack();
			return BCT_ERR_NO_ERROR;
		}

		/* Convenience methods that proxy onto liveStack. Makes the Java code look more like the C */ 
		public byte POP() throws CorruptDataException
		{
			logger.logp(FINEST,"StackMap_V1","POP","POP()");
			return liveStack.POP();
		}
		
		private void PUSH(int o) throws CorruptDataException
		{
			PUSH((byte)o);
		}
		
		public void PUSH(byte o) throws CorruptDataException
		{
			logger.logp(FINEST,"StackMap_V1","PUSH","PUSH({0})",o);
			liveStack.PUSH(o);
		}
		
		/* Relies on resultStack being set */
		private int outputStackMap(int[] resultsArray, int bits) throws CorruptDataException
		{
			int stackSize;
			int writePointer;

			stackSize = resultStack.stackSize();
			
			if (stackSize > 0 && resultsArray != null) {
				
				/* Get rid of unwanted stack entries. */
				/* Typically they are invoke parameters that are mapped as locals in the next frame */
				for (int i=0;i < (stackSize - bits); i++) {
					resultStack.POP();
				}

				writePointer = ((bits - 1) >> 5);
				resultsArray[writePointer] = 0;

				for (;;) {
					resultsArray[writePointer] <<= 1;
					
					if (resultStack.POP() == OBJ) {
						resultsArray[writePointer] |= 1;
					}
					
					if (--bits == 0) {
						break;
					}
					
					if ((bits & 31) == 0) {
						resultsArray[++writePointer] = 0;
					}
				}
			}
			
			if (logger.isLoggable(FINE)) {
				StringBuffer buffer = new StringBuffer();
				
				buffer.append("outputStackMap. bits = ");
				buffer.append(bits);
				buffer.append(" resultsArray size = " + resultsArray.length);
				buffer.append(" result: ");
				
				for (int i=0;i < resultsArray.length; i++) {
					buffer.append(Integer.toHexString(resultsArray[i]));
				}
				
				logger.logp(FINE,"StackMap_V1","outputStackMap",buffer.toString());
			}

			return stackSize;
		}
		
	}
	
}
