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

import static com.ibm.j9ddr.vm29.j9.ArgBits.argBitsFromSignature;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBathrow;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBgotow;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBinvokeinterface;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBlookupswitch;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBtableswitch;
import static com.ibm.j9ddr.vm29.j9.BytecodeWalk.BCV_BASE_TYPE_DOUBLE;
import static com.ibm.j9ddr.vm29.j9.BytecodeWalk.BCV_BASE_TYPE_FLOAT;
import static com.ibm.j9ddr.vm29.j9.BytecodeWalk.BCV_BASE_TYPE_INT;
import static com.ibm.j9ddr.vm29.j9.BytecodeWalk.BCV_BASE_TYPE_LONG;
import static com.ibm.j9ddr.vm29.j9.BytecodeWalk.BCV_BASE_TYPE_NULL;
import static com.ibm.j9ddr.vm29.j9.BytecodeWalk.BCV_GENERIC_OBJECT;
import static com.ibm.j9ddr.vm29.j9.BytecodeWalk.BCV_WIDE_TYPE_MASK;
import static com.ibm.j9ddr.vm29.j9.BytecodeWalk.RTV_BRANCH;
import static com.ibm.j9ddr.vm29.j9.BytecodeWalk.RTV_MISC;
import static com.ibm.j9ddr.vm29.j9.BytecodeWalk.RTV_POP_STORE_TEMP;
import static com.ibm.j9ddr.vm29.j9.BytecodeWalk.RTV_RETURN;
import static com.ibm.j9ddr.vm29.j9.BytecodeWalk.RTV_WIDE_POP_STORE_TEMP;
import static com.ibm.j9ddr.vm29.j9.PCStack.J9JavaInstructionSizeAndBranchActionTable;
import static com.ibm.j9ddr.vm29.j9.ROMHelp.J9EXCEPTIONINFO_HANDLERS;
import static com.ibm.j9ddr.vm29.j9.ROMHelp.J9_ARG_COUNT_FROM_ROM_METHOD;
import static com.ibm.j9ddr.vm29.j9.ROMHelp.J9_BYTECODE_SIZE_FROM_ROM_METHOD;
import static com.ibm.j9ddr.vm29.j9.ROMHelp.J9_BYTECODE_START_FROM_ROM_METHOD;
import static com.ibm.j9ddr.vm29.j9.ROMHelp.J9_EXCEPTION_DATA_FROM_ROM_METHOD;
import static com.ibm.j9ddr.vm29.j9.ROMHelp.J9_TEMP_COUNT_FROM_ROM_METHOD;
import static com.ibm.j9ddr.vm29.j9.VrfyTbl.J9JavaBytecodeVerificationTable;
import static com.ibm.j9ddr.vm29.j9.stackmap.MapHelpers.PARALLEL_BITS;
import static com.ibm.j9ddr.vm29.j9.stackmap.MapHelpers.PARAM_16;
import static com.ibm.j9ddr.vm29.j9.stackmap.MapHelpers.PARAM_32;
import static com.ibm.j9ddr.vm29.j9.stackmap.MapHelpers.PARAM_8;
import static com.ibm.j9ddr.vm29.structure.J9JavaAccessFlags.J9AccMethodHasExceptionInfo;
import static com.ibm.j9ddr.vm29.structure.J9JavaAccessFlags.J9AccStatic;
import static java.util.logging.Level.FINE;

import java.util.Arrays;
import java.util.Stack;
import java.util.logging.Logger;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.AlgorithmVersion;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ExceptionHandlerPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ExceptionInfoPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMMethodPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9UTF8Helper;
import com.ibm.j9ddr.vm29.types.I16;
import com.ibm.j9ddr.vm29.types.I32;
import com.ibm.j9ddr.vm29.types.IDATA;
import com.ibm.j9ddr.vm29.types.UDATA;

/**
 * LocalMap used for stacks generated in debug mode
 * 
 * @author andhall
 *
 */
public class DebugLocalMap
{

	private static Logger logger = Logger.getLogger("j9ddr.stackwalker.localmap");
	
	private static final int BRANCH_TARGET = 0x01;
	private static final int BRANCH_EXCEPTION_START = 0x02;
	private static final int BRANCH_TARGET_IN_USE = 0x04;
	private static final int BRANCH_TARGET_TO_WALK = 0x08;
	
	private static class DebugLocalMapData {
		byte bytecodeMap[];
		int mapArray[];
		public final Stack<Integer> rootStack = new Stack<Integer>();
		public int resultsArray[];
		public J9ROMMethodPointer romMethod;
		public int targetPC;
		public int currentLocals;
	};
	
	/* Mapping the verification encoding in J9JavaBytecodeVerificationTable */
	private static final int decodeTable[] = {
			0x0,                                            /* return index */
			BCV_BASE_TYPE_NULL,                     /* CFR_STACKMAP_TYPE_NULL */
			BCV_BASE_TYPE_INT,                      /* CFR_STACKMAP_TYPE_INT */
			BCV_BASE_TYPE_FLOAT,            /* CFR_STACKMAP_TYPE_FLOAT */
			BCV_BASE_TYPE_LONG,                     /* CFR_STACKMAP_TYPE_LONG */
			BCV_BASE_TYPE_DOUBLE,           /* CFR_STACKMAP_TYPE_DOUBLE */
			0x6,                                            /* return index */
			BCV_GENERIC_OBJECT,                     /* CFR_STACKMAP_TYPE_OBJECT */
			0x8,                                            /* return index */
			0x9,                                            /* return index */
			0xA,                                            /* return index */
			0xB,                                            /* return index */
			0xC,                                            /* return index */
			0xD,                                            /* return index */
			0xE,                                            /* return index */
			0xF                                                     /* return index */
	};


	private static interface DebugLocalMapImpl
	{
		public int j9localmap_DebugLocalBitsForPC(J9ROMMethodPointer romMethod, UDATA pc, int[] resultsArray) throws CorruptDataException;
	}
	
	public static int j9localmap_DebugLocalBitsForPC(J9ROMMethodPointer romMethod, UDATA pc, int[] resultsArray) throws CorruptDataException
	{
		return getImpl().j9localmap_DebugLocalBitsForPC(romMethod, pc, resultsArray);
	}
	
	private static DebugLocalMapImpl impl;
	
	private static DebugLocalMapImpl getImpl()
	{
		if (impl == null) {
			impl = getImpl(AlgorithmVersion.DEBUG_LOCAL_MAP_VERSION);
		}
		return impl;
	}
	
	private static DebugLocalMapImpl getImpl(String algorithmID) {
		AlgorithmVersion version = AlgorithmVersion.getVersionOf(algorithmID);
		switch (version.getAlgorithmVersion()) {
			// Add case statements for new algorithm versions
			default:
				return new DebugLocalMap_V1();
		}
	}

	private static class DebugLocalMap_V1 implements DebugLocalMapImpl
	{

		public int j9localmap_DebugLocalBitsForPC(J9ROMMethodPointer romMethod,
				UDATA pc, int[] resultsArray) throws CorruptDataException
		{
			UDATA length;
			DebugLocalMapData mapData = new DebugLocalMapData();
			logger.logp(FINE,"DebugLocalMap","j9localmap_DebugLocalBitsForPC","[][][] entering DEBUG local map in {0}{1} at {2}\n", 
					new Object[]{J9UTF8Helper.stringValue(romMethod.nameAndSignature().name()),
					J9UTF8Helper.stringValue(romMethod.nameAndSignature().signature()),
					pc});

			mapData.romMethod = romMethod;
			mapData.targetPC = pc.intValue();
			mapData.resultsArray = resultsArray;
	
			length = J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod);
	
			mapData.bytecodeMap = new byte[length.intValue()];
			mapData.mapArray = new int[length.intValue()];

			debugBuildBranchMap(mapData);
			//rootStack dynamically allocated

			debugMapAllLocals(mapData);

			return 0;
		}

		private void debugMapAllLocals(DebugLocalMapData mapData) throws CorruptDataException
		{
			J9ROMMethodPointer romMethod = mapData.romMethod;
			int localIndexBase = 0;
			int writeIndex = 0;
			int remainingLocals = J9_TEMP_COUNT_FROM_ROM_METHOD(romMethod).add(J9_ARG_COUNT_FROM_ROM_METHOD(romMethod)).intValue();
			int[] parallelResultArrayBase = mapData.resultsArray;
			UDATA length = J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod);

			/* Start with the arguments configured from the signature */
			/* A side effect is zeroing out the result array */
			argBitsFromSignature(
					J9UTF8Helper.stringValue(romMethod.nameAndSignature().signature()),
					mapData.resultsArray,
					(remainingLocals + 31) >> 5,
					romMethod.modifiers().anyBitsIn(J9AccStatic));

			while (remainingLocals != 0) {
				if (remainingLocals > PARALLEL_BITS) {
					remainingLocals -= PARALLEL_BITS;
				} else {
					remainingLocals = 0;
				}
				
				/* set currentLocals to the appropriate part of resultArrayBase contents */
				mapData.currentLocals = parallelResultArrayBase[writeIndex];

				/* clear the mapArray for each local set */
				Arrays.fill(mapData.mapArray, 0, length.intValue(), 0);

				/* Updated result is returned in currentLocals */
				debugMapLocalSet(mapData, localIndexBase);

				/* reset the bytecodeMap for next local set - leave only the original branch targets */
				if (remainingLocals != 0) {
					int i;
					for (i = 0; i < length.intValue(); i++) {
						mapData.bytecodeMap[i] &= (BRANCH_TARGET | BRANCH_EXCEPTION_START);
					}
				}

				/* save the local set result */
				parallelResultArrayBase[writeIndex] = mapData.mapArray[mapData.targetPC];
				writeIndex += 1;
				localIndexBase += PARALLEL_BITS;
			}
			return;
		}

		private int debugMapLocalSet(DebugLocalMapData mapData,
				int localIndexBase) throws CorruptDataException
		{
			J9ROMMethodPointer romMethod = mapData.romMethod;
			byte[] bytecodeMap = mapData.bytecodeMap;
			int pc = 0;
			int start, offset, low, high;
			int index;
			int npairs;
			int setBit;
			int offset16;
			int offset32;
			U8Pointer code;
			U8Pointer bcIndex;
			int popCount;
			int type1;
			int type2;
			int action;
			boolean justLoadedStack = false;
			boolean wideIndex = false;
			int bc;
			int length;
			int temp1;
			int target;
			boolean checkIfInsideException = romMethod.modifiers().anyBitsIn(J9AccMethodHasExceptionInfo);
			J9ExceptionInfoPointer exceptionData = J9_EXCEPTION_DATA_FROM_ROM_METHOD(romMethod);
			J9ExceptionHandlerPointer handler;
			UDATA exception;

			code = J9_BYTECODE_START_FROM_ROM_METHOD(romMethod);
			length = J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod).intValue();

			bcIndex = code;

			while (pc < length) {
				start = pc;
				
				if ((bytecodeMap[start] & BRANCH_EXCEPTION_START) != 0) {
					handler = J9EXCEPTIONINFO_HANDLERS(exceptionData);

					for (exception = new UDATA(0); exception.lt(exceptionData.catchCount()); exception = exception.add(1)) {
						/* find the matching branch target, and copy/merge the locals to the exception target */
						if (start == handler.startPC().intValue()) {
							debugMergeStacks (mapData, handler.handlerPC().intValue());
						}
						handler = handler.add(1);
					}
				}

				/* Merge all branchTargets encountered */	
				if ((bytecodeMap[start] & BRANCH_TARGET) != 0) {
					/* Don't try to merge a stack we just loaded */
					if (!justLoadedStack) {
						/* TBD merge map to this branch target in mapArray */
						debugMergeStacks (mapData, start);
						if (! mapData.rootStack.isEmpty()) {
							pc = mapData.rootStack.pop();
							mapData.currentLocals = mapData.mapArray[pc];
							bytecodeMap[pc] &= ~BRANCH_TARGET_TO_WALK;
							justLoadedStack = true;
						} else {
							/* else we are done the rootStack -- return */
							return 0;
						}
					}
				}
				justLoadedStack = false;
				
				bcIndex = code.add(start);
				bc = bcIndex.at(0).intValue() & 0xFF;
				
				if((J9JavaInstructionSizeAndBranchActionTable[bc] & 7) == 0) {
					throw new CorruptDataException("Invalid byte code");
				}
				
				pc += (J9JavaInstructionSizeAndBranchActionTable[bc] & 7);

				type1 = J9JavaBytecodeVerificationTable[bc];
				action = type1 >>> 8;
				type2 = (type1 >>> 4) & 0xF;
				type1 = decodeTable[type1 & 0xF];
				type2 = decodeTable[type2];

				switch (action) {

				case RTV_WIDE_POP_STORE_TEMP:
					wideIndex = true;	/* Fall through case !!! */

				case RTV_POP_STORE_TEMP:
					index = type2 & 0x7;
					if (type2 == 0) {
						index = PARAM_8(bcIndex, 1).intValue();
						if (wideIndex) {
							index = PARAM_16(bcIndex, 1).intValue();
							wideIndex = false;
						}
					}
					
					/* Trace only those locals in the range of interest */
					index -= localIndexBase;

					if (index < PARALLEL_BITS) {
						setBit = 1 << index;
						
						if (type1 == BCV_GENERIC_OBJECT) {
							mapData.currentLocals |= setBit;
						} else {
							mapData.currentLocals &= ~setBit;
							if ((type1 & BCV_WIDE_TYPE_MASK) != 0) {
								setBit <<= 1;
								mapData.currentLocals &= ~setBit;
							}
						}
					}

					/* should inside exception be in bit array?? */
					if (checkIfInsideException) {
						/* For all exception handlers covering this instruction */
						handler = J9EXCEPTIONINFO_HANDLERS(exceptionData);

						for (exception = new UDATA(0); exception.lt(exceptionData.catchCount()); exception = exception.add(1)) {
							/* find the matching branch target, and copy/merge the locals to the exception target */
							if ((start >= handler.startPC().intValue()) && (start < handler.endPC().intValue())) {
								debugMergeStacks (mapData, handler.handlerPC().intValue());
							}
							handler = handler.add(1);
						}
					}
					break;

				case RTV_BRANCH:
					popCount = type2 & 0x07;

					if (bc == JBgotow) {
						offset32 = new I32(PARAM_32(bcIndex,1)).intValue();
						target = start + offset32;
					} else {
						offset16 = new I16(PARAM_16(bcIndex,1)).intValue();
						target = start + offset16;
					}

					debugMergeStacks (mapData, target);

					/* Unconditional branch (goto family) */
					if (popCount == 0) {
						if (! mapData.rootStack.isEmpty()) {
							pc = mapData.rootStack.pop();
							mapData.currentLocals = mapData.mapArray[pc];
							bytecodeMap[pc] &= ~BRANCH_TARGET_TO_WALK;
							justLoadedStack = true;
						} else {
							/* else we are done the rootStack -- return */
							return 0;
						}
					}
					break;

				case RTV_RETURN:
					if (! mapData.rootStack.isEmpty()) {
						pc = mapData.rootStack.pop();
						mapData.currentLocals = mapData.mapArray[pc];
						bytecodeMap[pc] &= ~BRANCH_TARGET_TO_WALK;
						justLoadedStack = true;
					} else {
						/* else we are done the rootStack -- return */
						return 0;
					}
					break;

				case RTV_MISC:
					if (bc == JBathrow) {
						if (! mapData.rootStack.isEmpty()) {
							pc = mapData.rootStack.pop();
							mapData.currentLocals = mapData.mapArray[pc];
							bytecodeMap[pc] &= ~BRANCH_TARGET_TO_WALK;
							justLoadedStack = true;
						} else {
							/* else we are done the rootStack -- return */
							return 0;
						}
					} else if ((bc == JBtableswitch) || (bc == JBlookupswitch)) {
						bcIndex = U8Pointer.cast(UDATA.cast(bcIndex).add(4).bitAnd(new UDATA(3).bitNot()));
						offset = new I32(PARAM_32(bcIndex,0)).intValue();
						bcIndex = bcIndex.add(4);
						target = start + offset;
						debugMergeStacks (mapData, target);
						low = new I32(PARAM_32(bcIndex,0)).intValue();
						bcIndex = bcIndex.add(4);

						if (bc == JBtableswitch) {
							high = new I32(PARAM_32(bcIndex,0)).intValue();
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
							target = (start + offset);
							debugMergeStacks (mapData, target);
						}

						//goto _checkFinished;
						if (! mapData.rootStack.isEmpty()) {
							pc = mapData.rootStack.pop();
							mapData.currentLocals = mapData.mapArray[pc];
							bytecodeMap[pc] &= ~BRANCH_TARGET_TO_WALK;
							justLoadedStack = true;
						} else {
							/* else we are done the rootStack -- return */
							return 0;
						}
					}
					break;
				}
				continue;
				
			}

			return -1;					/* Should never get here */
		}

		private void debugMergeStacks(DebugLocalMapData mapData, int target)
		{
			if ((mapData.bytecodeMap[target] & BRANCH_TARGET_IN_USE) != 0) {
				int mergeResult = mapData.mapArray[target] & mapData.currentLocals;
				if (mergeResult == mapData.mapArray[target]) {
					return;
				} else {
					/* map changed - merge and rewalk */
					mapData.mapArray[target] = mergeResult;
					if ((mapData.bytecodeMap[target] & BRANCH_TARGET_TO_WALK) != 0) {
						/* already on rootStack */
						return;
					}
				}
			} else {
				/* new map - save locals */
				mapData.mapArray[target] = mapData.currentLocals;
				mapData.bytecodeMap[target] |= BRANCH_TARGET_IN_USE;
			}
			mapData.rootStack.push(target);
			mapData.bytecodeMap[target] |= BRANCH_TARGET_TO_WALK;
		}

		private int debugBuildBranchMap(DebugLocalMapData mapData) throws CorruptDataException
		{
			J9ROMMethodPointer romMethod = mapData.romMethod;
			byte[] bytecodeMap = mapData.bytecodeMap;
			U8Pointer bcStart;
			U8Pointer bcIndex;
			U8Pointer bcEnd;
			UDATA npairs, temp;
			int pc, start, high, low, pcs;
			int shortBranch;
			int longBranch;
			int bc;
			int size;
			int count = 0;

			bcStart = J9_BYTECODE_START_FROM_ROM_METHOD(romMethod);
			bcIndex = bcStart;
			bcEnd = bcStart.add(J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod));

			//Just allocated bytecodeMap - don't need to reset it

			/* Adjust JBinvokeinterface to point to the JBinvokeinterface2 */
			if (bcIndex.at(mapData.targetPC).eq(JBinvokeinterface)) {
				mapData.targetPC -= 2;
			}
			
			/* Allow space for the mapping target as it is pushed when encountered */
			bytecodeMap[mapData.targetPC] = BRANCH_TARGET;
			count++;

			while (bcIndex.lt(bcEnd)) {
				bc = bcIndex.at(0).intValue() & 0xFF;
				size = J9JavaInstructionSizeAndBranchActionTable[bc];

				switch (size >> 4) {

				case 5: /* switches */
					start = IDATA.cast(bcIndex.sub(UDATA.cast(bcStart))).intValue();
					pc = (start + 4) & ~3;
					bcIndex = bcStart.add(pc);
					longBranch = new I32(PARAM_32(bcIndex, 0)).intValue();
					bcIndex = bcIndex.add(4);
					if (bytecodeMap[start + longBranch] == 0) {
						bytecodeMap[start + longBranch] = BRANCH_TARGET;
						count++;
					}
					low = new I32(PARAM_32(bcIndex, 0)).intValue();
					bcIndex = bcIndex.add(4);
					
					if (bc == JBtableswitch) {
						high = new I32(PARAM_32(bcIndex,0)).intValue();
						bcIndex = bcIndex.add(4);
						npairs = new UDATA(high - low + 1);
						pcs = 0;
					} else {
						npairs = new UDATA(low);
						pcs = 4;
					}

					for (temp = new UDATA(0); temp.lt(npairs); temp = temp.add(1)) {
						bcIndex = bcIndex.add(pcs);
						longBranch = new I32(PARAM_32(bcIndex, 0)).intValue();
						bcIndex = bcIndex.add(4);
						if (bytecodeMap[start + longBranch] == 0) {
							bytecodeMap[start + longBranch] = BRANCH_TARGET;
							count++;
						}
					}
					continue;

				case 2: /* gotos */
					if (bc == JBgotow) {
						start = IDATA.cast(bcIndex.sub(UDATA.cast(bcStart))).intValue();
						longBranch = new I32(PARAM_32(bcIndex, 1)).intValue();
						if (bytecodeMap[start + longBranch] == 0) {
							bytecodeMap[start + longBranch] = BRANCH_TARGET;
							count++;
						}
						break;
					} /* fall through for JBgoto */

				case 1: /* ifs */
					shortBranch = new I16(PARAM_16(bcIndex,1)).intValue();
					start = IDATA.cast(bcIndex.sub(UDATA.cast(bcStart))).intValue();
					if (bytecodeMap[start + shortBranch] == 0) {
						bytecodeMap[start + shortBranch] = BRANCH_TARGET;
						count++;
					}
					break;

				}
				bcIndex = bcIndex.add(size & 7);
			}

			/* need to walk exceptions as well, since they are branch targets */
			if (romMethod.modifiers().anyBitsIn(J9AccMethodHasExceptionInfo)) {
				J9ExceptionInfoPointer exceptionData = J9_EXCEPTION_DATA_FROM_ROM_METHOD(romMethod);
				J9ExceptionHandlerPointer handler;

				if (! exceptionData.catchCount().eq(0)) {
					handler = J9EXCEPTIONINFO_HANDLERS(exceptionData);
					for (temp = new UDATA(0); temp.lt(exceptionData.catchCount()); temp = temp.add(1)) {
						pc = handler.startPC().intValue();
						pcs = handler.handlerPC().intValue();
						/* Avoid re-walking a handler that handles itself */
						if (pc != pcs) {
							bytecodeMap[pc] |= BRANCH_EXCEPTION_START;
						}
						if ((bytecodeMap[pcs] & BRANCH_TARGET) == 0) {
							bytecodeMap[pcs] |= BRANCH_TARGET;
							count++;
						}
						handler = handler.add(1);
					}
				}
			}

			return count;
		}
		
	}
}
