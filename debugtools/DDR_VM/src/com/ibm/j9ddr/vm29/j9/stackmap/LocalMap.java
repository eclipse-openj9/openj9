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
import static com.ibm.j9ddr.vm29.j9.BCNames.JBgoto;
import static com.ibm.j9ddr.vm29.j9.BCNames.JBtableswitch;
import static com.ibm.j9ddr.vm29.j9.PCStack.J9BytecodeSlotUseTable;
import static com.ibm.j9ddr.vm29.j9.PCStack.J9JavaInstructionSizeAndBranchActionTable;
import static com.ibm.j9ddr.vm29.j9.ROMHelp.J9EXCEPTIONINFO_HANDLERS;
import static com.ibm.j9ddr.vm29.j9.ROMHelp.J9ROMMETHOD_SIGNATURE;
import static com.ibm.j9ddr.vm29.j9.ROMHelp.J9_ARG_COUNT_FROM_ROM_METHOD;
import static com.ibm.j9ddr.vm29.j9.ROMHelp.J9_BYTECODE_SIZE_FROM_ROM_METHOD;
import static com.ibm.j9ddr.vm29.j9.ROMHelp.J9_BYTECODE_START_FROM_ROM_METHOD;
import static com.ibm.j9ddr.vm29.j9.ROMHelp.J9_EXCEPTION_DATA_FROM_ROM_METHOD;
import static com.ibm.j9ddr.vm29.j9.ROMHelp.J9_TEMP_COUNT_FROM_ROM_METHOD;
import static com.ibm.j9ddr.vm29.j9.stackmap.MapHelpers.PARALLEL_BITS;
import static com.ibm.j9ddr.vm29.j9.stackmap.MapHelpers.PARAM_16;
import static com.ibm.j9ddr.vm29.j9.stackmap.MapHelpers.PARAM_32;
import static com.ibm.j9ddr.vm29.j9.stackmap.MapHelpers.PARAM_8;
import static com.ibm.j9ddr.vm29.structure.J9JavaAccessFlags.J9AccMethodHasExceptionInfo;
import static com.ibm.j9ddr.vm29.structure.J9JavaAccessFlags.J9AccStatic;
import static java.util.logging.Level.FINER;
import static java.util.logging.Level.FINEST;

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
import com.ibm.j9ddr.vm29.types.U32;
import com.ibm.j9ddr.vm29.types.UDATA;

/**
 * Provides subset of function in stackmap/localmap.c
 * 
 * @author andhall
 *
 */
public class LocalMap
{

	private static Logger logger = Logger.getLogger("j9ddr.stackwalker.localmap");
	
	public static final int ENCODED_INDEX = 0x04;
	public static final int	ENCODED_MASK = 0x03;
	public static final int	WIDE_INDEX = 0x08;

	public static final int	DOUBLE_ACCESS = 0x020;
	public static final int	SINGLE_ACCESS = 0x040;
	public static final int	OBJECT_ACCESS = 0x080;
	public static final int	WRITE_ACCESS = 0x010;

	public static final int INT = 0x0;
	public static final int OBJ = 0x1;
	public static final int	NOT_FOUND = -1;
	
	public static final int	WALKED = 0x1;

	

	public static void j9localmap_ArgBitsForPC0 (J9ROMMethodPointer romMethod, int[] resultsArray) throws CorruptDataException
	{
		getImpl().j9localmap_ArgBitsForPC0(romMethod, resultsArray);
	}
	
	/**
	 * Builds a map of stack use for supplied ROMmethod.
	 * 
	 * Used to identify which slots on the stack hold objects.
	 * 
	 * @param romMethod ROM method under test
	 * @param pc 
	 * @param resultsArray Stack map bit array. One bit per slot.
	 * @return Less than 0 if there was an error. >=0 on success.
	 */
	public static int 
	j9localmap_LocalBitsForPC(J9ROMMethodPointer romMethod, UDATA pc, int[] resultsArray) throws CorruptDataException
	{
		return getImpl().j9localmap_LocalBitsForPC(romMethod, pc, resultsArray);
	}
	
	private static LocalMapImpl impl = null;
	
	private static LocalMapImpl getImpl()
	{
		if (impl == null) {
			impl = getImpl(AlgorithmVersion.LOCAL_MAP_VERSION);
		}
		return impl;
	}

	private static LocalMapImpl getImpl(String algorithmID) {
		AlgorithmVersion version = AlgorithmVersion.getVersionOf(algorithmID);
		switch (version.getAlgorithmVersion()) {
			// Add case statements for new algorithm versions
			default:
				return new LocalMap_V1();
		}
	}
	
	private static interface LocalMapImpl
	{
		public void j9localmap_ArgBitsForPC0 (J9ROMMethodPointer romMethod, int[] resultsArray) throws CorruptDataException;
		
		public int j9localmap_LocalBitsForPC(J9ROMMethodPointer romMethod, UDATA pc, int[] resultsArray) throws CorruptDataException;
	}
	
	/**
	 * Data class used for tracking branches in mapLocalSet.
	 * 
	 * In the native code this data is encoded onto the end of unknownsByPC.
	 * @author andhall
	 *
	 */
	private static class BranchState
	{
		public final int seekLocals;
		
		public final int programCounter;
		
		public BranchState(int programCounter, int seekLocals)
		{
			this.seekLocals = seekLocals;
			this.programCounter = programCounter;
		}
	}

	
	private static class LocalMap_V1 implements LocalMapImpl
	{
		/* Fields used instead of primitive pass-by-reference */
		private boolean unknownsWereUpdated = false;
		/* Bitmask of slots we have found/handled */
		private int knownLocals = 0;
		/* Bitmask of slots we know are objects */
		private int knownObjects = 0;
		
		protected LocalMap_V1() {
			super();
		}
		
		public void j9localmap_ArgBitsForPC0 (J9ROMMethodPointer romMethod, int[] resultsArray) throws CorruptDataException
		{
			argBitsFromSignature(
				J9UTF8Helper.stringValue(J9ROMMETHOD_SIGNATURE(romMethod)),
				resultsArray,
				J9_ARG_COUNT_FROM_ROM_METHOD(romMethod).add(31).rightShift(5).intValue(),
				romMethod.modifiers().anyBitsIn(J9AccStatic));
		}
		
		public int 
		j9localmap_LocalBitsForPC(J9ROMMethodPointer romMethod, UDATA pc, int[] resultsArray) throws CorruptDataException
		{
			/* The native version of this function (see localmap.c) spends a lot of time sizing the scratch storage
			 * for the subsequent operations (counting branches etc.). The Java implementation only uses the scratch
			 * storage for tracking known locals per PC, not for tracking branches (which is done on a dedicated stack
			 * in mapLocalSet).
			 */
			
			UDATA mapWords = (new UDATA(J9_TEMP_COUNT_FROM_ROM_METHOD(romMethod).add(J9_ARG_COUNT_FROM_ROM_METHOD(romMethod).add(31)))).rightShift(5);
			/* Bytecode size for ROMMethod */
			UDATA length;
			int[] scratchBuffer;
			
			/* clear the result map as we may not write all of it */
			Arrays.fill(resultsArray,0,mapWords.intValue() - 1, 0);
			
			length = J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod);
			
			/* Scratch buffer is just one 32 bitmask per byte of length */
			scratchBuffer = new int[length.intValue()];
			
			mapAllLocals(romMethod,scratchBuffer,pc,resultsArray);
			
			return 0;
		}
		
		/**
		 * Manages calls to mapLocalSet.
		 * 
		 * Multiplexes calls to mapLocalSet across sets of locals (32 at a time - to fit in the various locals bitmasks).
		 * 
		 * Initially does a mapLocalSet on the method, then applies mapLocalSet to each exception handler until all local
		 * slots are accounted for.
		 * 	
		 * @param romMethod Method whose bytecodes should be walked.
		 * @param unknownsByPC Buffer used to store a bitmask of which locals are known by each PC
		 * @param startPC The PC at which to start mapping.
		 * @param resultArrayBase Memory into which the result should be stored.
		 */
		private void mapAllLocals(J9ROMMethodPointer romMethod,
			int[] unknownsByPC, UDATA startPC, int[] resultsArray) throws CorruptDataException
			{
			J9ExceptionInfoPointer exceptionData = null;
			J9ExceptionHandlerPointer handler = null;
			UDATA exceptionCount = new UDATA(0);
			UDATA remainingLocals = new UDATA(J9_TEMP_COUNT_FROM_ROM_METHOD(romMethod).add(J9_ARG_COUNT_FROM_ROM_METHOD(romMethod)));
			int writeIndex = 0;
		
		
			/* Base index of the 32 value localSet */
			int localIndexBase = 0;
		
			if (romMethod.modifiers().anyBitsIn(J9AccMethodHasExceptionInfo)) {
				exceptionData = J9_EXCEPTION_DATA_FROM_ROM_METHOD(romMethod);
				exceptionCount = new UDATA(exceptionData.catchCount());
			}
		
			while (remainingLocals.gt(0)) {
				
				/* Reset the unknown list for each local set */
				Arrays.fill(unknownsByPC, 0, J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod).intValue() - 1, 0);
				
				knownLocals = 0;
				
				/* If we have more than PARALLEL_BITS locals, we need to multiplex the locals
				 * across multiple calls to mapLocalSet
				 */
				if (remainingLocals.gt(PARALLEL_BITS)) {
					remainingLocals = remainingLocals.sub(PARALLEL_BITS);
				} else {
					if (remainingLocals.lt(new U32(PARALLEL_BITS))) {
						/* Set all knownLocal bits, then shift in 0s for remainingLocals */
						knownLocals = -1;
						knownLocals <<= remainingLocals.intValue();
					}
					remainingLocals = new UDATA(0);
				}
				
				knownObjects = 0;
				mapLocalSet(romMethod,unknownsByPC,startPC,localIndexBase);
				
				/* If we haven't found all the locals and we have exceptions to walk */
				if ( (knownLocals != -1) && exceptionCount.gt(0) ) {
					/* Walk the exceptions */
					boolean keepLooking = true;
					int unknownLocals = 0;
					
					while (keepLooking) {
						/* since each exception handler may walk through the start/end range of another handler,
						 continue until all handlers are walked or the remaining handlers could not have been
						 called (no foot prints passed through their protection range) */
						keepLooking = false;
						handler = J9EXCEPTIONINFO_HANDLERS(exceptionData);
					
						for (int e=0; e < exceptionCount.intValue(); e++) {
							int rawUnknowns;
						
							/* Collect up any locals that were unknown in the covered exception range. */
							unknownLocals = 0;
							for (UDATA j = handler.startPC(); j.lt(handler.endPC()); j = j.add(1)) {
								unknownLocals |= unknownsByPC[j.intValue()];
							}
						
							/* Don't bother looking for locals we know about definitively. */
							rawUnknowns = unknownLocals;
							unknownLocals &= ~knownLocals;
							
							if (logger.isLoggable(FINER)) {
								StringBuffer buffer = new StringBuffer();
								buffer.append(String.format("handler[%d] unknowns raw=0x%X masked=0x%X [",e,rawUnknowns,unknownLocals));
								for (int bitIndex=0; bitIndex < 32; bitIndex++) {
									int bit = 1 << bitIndex;
									if ((unknownLocals & bit) != 0) {
										buffer.append(String.format("%d ",bitIndex + localIndexBase));
									}
								}
								buffer.append("]");
								logger.logp(FINER, "LocalMap", "mapAllLocals", buffer.toString());
							}
				
							/* If this handler knows locals that we don't know about */
							if ( (~unknownsByPC[handler.handlerPC().intValue()] & unknownLocals) != 0) {
								//Note: we are using fields to replace the C pass-by-reference design.
								//Here we have to juggle variables to call mapLocalSet a second time without losing
								//the knownLocals state from the first call.
								int originalKnownLocals = knownLocals;
								knownLocals = ~unknownLocals;
								int localsBeforeWalk = knownLocals;
								unknownsWereUpdated = false;
							
								mapLocalSet(romMethod,unknownsByPC,new UDATA(handler.handlerPC()),localIndexBase);
								
								int exceptionKnownLocals = knownLocals;
								knownLocals = originalKnownLocals;
								
								/* keepLooking if found new stuff, or updated per-PC metadata */
								keepLooking = keepLooking || (exceptionKnownLocals != localsBeforeWalk);
								keepLooking = keepLooking || unknownsWereUpdated;
		
								/* Note all new locals found */
								knownLocals |= (exceptionKnownLocals & ~localsBeforeWalk);
							}
						
							handler = handler.add(1);
						}
					}
				}
			
				/* save the local set result */
				resultsArray[writeIndex] = knownObjects;
				writeIndex += 1;
				localIndexBase += PARALLEL_BITS;
			}
		}

	
		/** 
		 * 	Walk the bytecodes in romMethod starting at startPC. 
		 * 	@param romMethod Method containing the bytecodes to walk.
		 * @param unknownsByPC Bitmask of locals unknown by PC. Unlike native, this doesn't contain the branch stack
		 * @param startPC Bytecode index to begin the walk.
		 * @param localIndexBase Offset of the first local, usually zero unless method has > PARALLEL_BITS locals. 
		 * @param knownLocals Bitfield of locals whose types are known definitively.
		 * @param knownObject Bitfield of locals that are definitively objects.
		 * @param unknownsUpdated Pointer to a boolean value updated iff. any per-PC metadata was updated.
		 */
		private int
		mapLocalSet(J9ROMMethodPointer romMethod, int[] unknownsByPC, UDATA startPC, int localIndexBase) throws CorruptDataException 
		{
			U8Pointer bcStart;
			U8Pointer bcIndex;
			U8Pointer bcEnd;
			int pc;
			int seekLocals = ~knownLocals;
			/* Bytecode value */
			UDATA bc;
			UDATA temp1;
			UDATA size;
			UDATA target;
			U32 index;
			U32 npairs;
			int setBit;
			I32 offset;
			I32 high;
			I32 low;
			Stack<BranchState> branchStack = new Stack<BranchState>();
		
			/* Assume nothing changed */
			unknownsWereUpdated = false;

			bcStart = J9_BYTECODE_START_FROM_ROM_METHOD(romMethod);
			bcEnd = bcStart.add(J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod));
			bcIndex = bcStart.add(startPC);
			
			while (bcIndex .lt(bcEnd)) {
				boolean shouldBeWalked;
				
				pc = (int) (bcIndex.getAddress() - bcStart.getAddress());
				shouldBeWalked = (~unknownsByPC[pc] & seekLocals) != 0;
				
				if (logger.isLoggable(FINER))
				{
					int bitIndex;
					StringBuffer buffer = new StringBuffer();
					
					buffer.append(String.format("pc=%d walk=%s seekLocals=0x%X [",pc, (shouldBeWalked ? "true" : "false"), seekLocals));
					
					for (bitIndex=0; bitIndex < 32; bitIndex++) {
						int bit = 1 << bitIndex;
						if ((seekLocals & bit) != 0) {
							buffer.append(String.format("%d ",bitIndex));
						}
					}
		
					buffer.append("]");
					
					logger.logp(FINER,"LocalMap","mapLocalSet",buffer.toString());
				}
				
				/* Continue if any seekLocals is not in the beenHere set */
				if ((~unknownsByPC[pc] & seekLocals) != 0) {
					/* leave footprints */
					unknownsWereUpdated = true;
					unknownsByPC[pc] |= seekLocals;
					
					bc = new UDATA(bcIndex.at(0));
					size = new UDATA(0xFF & J9JavaInstructionSizeAndBranchActionTable[bc.intValue()]);
					
					if (logger.isLoggable(FINEST)) {
						logger.logp(FINEST, "LocalMap", "mapLocalSet", "bcIndex={0}, bc={1}, size={2}",new Object[]{Long.toHexString(bcIndex.getAddress()),
																													bc,
																													size});
					}
					
					switch (size.intValue() >>> 4) {
					case 0:
						temp1 = new UDATA(J9BytecodeSlotUseTable[bc.intValue()]);
						if (temp1.longValue() != 0) {

							if (temp1.anyBitsIn(ENCODED_INDEX)) {
								/* Get encoded index */
								index = new U32(temp1.bitAnd(ENCODED_MASK));
							} else {
								/* Get parameter byte index */
								index = new U32(PARAM_8(bcIndex, 1));
								if (temp1.anyBitsIn(WIDE_INDEX)) {
									/* get word index */
									index = new U32(PARAM_16(bcIndex, 1));
								}
							}

							/* Trace only those locals in the range of interest */
							index = index.sub(localIndexBase);

							boolean doubleAccess = true;
							while (doubleAccess) {
								if (index.lt(new UDATA(PARALLEL_BITS))) {

									setBit = 1 << index.intValue();
								

									/* First encounter with this local? */
									if ((seekLocals & setBit) != 0) {
										/* Stop looking for this local */
										seekLocals &= (~setBit);
										
										logger.logp(FINEST, "LocalMap", "mapLocalSet", "  stop looking for local={0}",index);
										
										/* Know what the local is if it is read */
										if (! temp1.anyBitsIn(WRITE_ACCESS)) {
											knownLocals |= setBit;
											
											logger.logp(FINEST,"LocalMap","mapLocalSet","  add local={0} to known locals.", index);

											/* Is it an Object */
											if (temp1.anyBitsIn(OBJECT_ACCESS)) {
												knownObjects |= setBit;
												logger.logp(FINEST, "LocalMap", "mapLocalSet", "  add local={0} to known objects.", index);
											}
										}
									} else {
										logger.logp(FINEST, "LocalMap", "mapLocalSet",  "  skipping reference to local={0}", index);
									}
								}

								if (temp1.anyBitsIn(DOUBLE_ACCESS)) {
									temp1 = temp1.bitAnd(new UDATA(DOUBLE_ACCESS).bitNot());
									index = index.add(1);
								} else {
									doubleAccess = false;
								}
							}
						}
						bcIndex = bcIndex.add(size);
						break;

					case 1:			/* ifs */
						offset = new I32(new I16(PARAM_16(bcIndex, 1)));
						branchStack.push(new BranchState(pc + offset.intValue(), seekLocals));
						/* fall through */
						
					case 6:		/* invokes */
						bcIndex = bcIndex.add(size.bitAnd(7));
						break;
						
					case 2:			/* gotos */
						if (bc.intValue() == JBgoto) {
							offset = new I32(new I16(PARAM_16(bcIndex, 1)));
							target = new UDATA(pc).add(offset);
						} else {
							offset = new I32(PARAM_32(bcIndex, 1));
							target = new UDATA(pc).add(offset);
						}
						bcIndex = bcStart.add(target);
						break;
						
					case 4:			/* returns/athrow */
						/* No further possible paths - finished */
						if (branchStack.empty()) {
							return 0;
						}
						BranchState newBranch = branchStack.pop();
						pc = newBranch.programCounter;
						seekLocals = newBranch.seekLocals;
						
						/* Don't look for already known locals */
						seekLocals &= ~knownLocals;
						bcIndex = bcStart.add(pc);
						break;
						
					case 5:			/* switches */
						bcIndex = bcIndex.add(4 - (pc & 3));
						offset = new I32(PARAM_32(bcIndex, 0));
						
						bcIndex = bcIndex.add(4);
						target = new UDATA(pc).add(offset);
						low = new I32(PARAM_32(bcIndex,0));
						bcIndex = bcIndex.add(4);
						
						if (bc.intValue() == JBtableswitch) {
							high = new I32(PARAM_32(bcIndex, 0));
							bcIndex = bcIndex.add(4);
							npairs = new U32(high.sub(low).add(1));
							temp1 = new UDATA(0);
						} else {
							npairs = new U32(low);
							/* used to skip over the tableswitch key entry */
							temp1 = new UDATA(4);
						}
						
						for (; npairs.gt(0); npairs = npairs.sub(1)) {
							bcIndex = bcIndex.add(temp1);
							offset = new I32(PARAM_32(bcIndex,0));
							bcIndex = bcIndex.add(4);
							branchStack.add(new BranchState(pc + offset.intValue(), seekLocals));
						}

						/* finally continue at the default switch case */
						bcIndex = bcStart.add(target);
						break;
					}
				} else {
					/* No further possible paths - finished */
					if (branchStack.empty()) {
						return 0;
					}
					BranchState newBranch = branchStack.pop();
					pc = newBranch.programCounter;
					seekLocals = newBranch.seekLocals;
					
					/* Don't look for already known locals */
					seekLocals &= ~knownLocals;
					bcIndex = bcStart.add(pc);
				}
			}
			
			return 0;
		}
	}

}
