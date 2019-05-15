/*******************************************************************************
 * Copyright (c) 2009, 2019 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.j9.stackwalker;

import java.util.LinkedList;
import java.util.List;

import com.ibm.j9ddr.AddressedCorruptDataException;
import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.gc.GCObjectIterator;
import com.ibm.j9ddr.vm29.j9.stackwalker.MethodMetaData.JITMaps;
import com.ibm.j9ddr.vm29.j9.AlgorithmPicker;
import com.ibm.j9ddr.vm29.j9.AlgorithmVersion;
import com.ibm.j9ddr.vm29.j9.BaseAlgorithm;
import com.ibm.j9ddr.vm29.j9.IAlgorithm;
import com.ibm.j9ddr.vm29.j9.J9ConfigFlags;
import com.ibm.j9ddr.vm29.pointer.AbstractPointer;
import com.ibm.j9ddr.vm29.pointer.ObjectReferencePointer;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.U32Pointer;
import com.ibm.j9ddr.vm29.pointer.U64Pointer;
import com.ibm.j9ddr.vm29.pointer.UDATAPointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ConstantPoolPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9I2JStatePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JITConfigPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JITDecompilationInfoPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JITExceptionTablePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JITFramePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JITStackAtlasPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9OSRBufferPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9OSRFramePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMMethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMMethodRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9SFJ2IFramePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9SFJITResolveFramePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9SFNativeMethodFramePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9SFSpecialFramePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9UTF8Pointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;
import com.ibm.j9ddr.vm29.pointer.generated.TRBuildFlags;
import com.ibm.j9ddr.vm29.pointer.helper.J9UTF8Helper;
import com.ibm.j9ddr.vm29.structure.J9JITFrame;
import com.ibm.j9ddr.vm29.structure.J9SFJ2IFrame;
import com.ibm.j9ddr.vm29.structure.J9SFNativeMethodFrame;
import com.ibm.j9ddr.vm29.structure.J9SFSpecialFrame;
import com.ibm.j9ddr.vm29.types.U8;
import com.ibm.j9ddr.vm29.types.UDATA;
import com.ibm.j9ddr.vm29.structure.J9ITable;
import com.ibm.j9ddr.vm29.pointer.generated.J9ITablePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;

import static com.ibm.j9ddr.vm29.j9.JITLook.*;
import static com.ibm.j9ddr.vm29.j9.ROMHelp.*;
import static com.ibm.j9ddr.vm29.j9.SendSlot.*;
import static com.ibm.j9ddr.vm29.j9.stackwalker.JITRegMap.*;
import static com.ibm.j9ddr.vm29.j9.stackwalker.MethodMetaData.*;
import static com.ibm.j9ddr.vm29.j9.stackwalker.StackWalker.*;
import static com.ibm.j9ddr.vm29.j9.stackwalker.StackWalkerUtils.*;
import static com.ibm.j9ddr.vm29.pointer.generated.J9StackWalkFlags.*;
import static com.ibm.j9ddr.vm29.structure.J9Consts.*;
import static com.ibm.j9ddr.vm29.structure.J9JavaAccessFlags.*;
import static com.ibm.j9ddr.vm29.structure.J9StackWalkConstants.*;
import static com.ibm.j9ddr.vm29.structure.J9StackWalkState.*;
import static com.ibm.j9ddr.vm29.structure.MethodMetaDataConstants.INTERNAL_PTR_REG_MASK;

/**
 * Stack walker for processing JIT frames. Don't call directly - go through
 * com.ibm.j9ddr.vm.j9.stackwalker.StackWalker
 * 
 * @author andhall
 *
 */
public class JITStackWalker
{

	static FrameCallbackResult jitWalkStackFrames(WalkState walkState) throws CorruptDataException
	{
		return getImpl().jitWalkStackFrames(walkState);
	}
	
	static void jitPrintRegisterMapArray(WalkState walkState, String description) throws CorruptDataException
	{
		getImpl().jitPrintRegisterMapArray(walkState,description);
	}
	
	static J9JITExceptionTablePointer jitGetExceptionTableFromPC(J9VMThreadPointer walkThread, U8Pointer pc) throws CorruptDataException
	{
		return getImpl().jitGetExceptionTableFromPC(walkThread, pc);
	}
	
	private static IJITStackWalker impl;
	
	private static final AlgorithmPicker<IJITStackWalker> picker = new AlgorithmPicker<IJITStackWalker>(AlgorithmVersion.JIT_STACK_WALKER_VERSION){

		@Override
		protected Iterable<? extends IJITStackWalker> allAlgorithms()
		{
			List<IJITStackWalker> toReturn = new LinkedList<IJITStackWalker>();
			toReturn.add(new JITStackWalker_29_V0());
			return toReturn;
		}};
	
	private static IJITStackWalker getImpl()
	{
		if (impl == null) {
			impl = picker.pickAlgorithm();
		}
		return impl;
	}
	
	private static interface IJITStackWalker extends IAlgorithm
	{
		public FrameCallbackResult jitWalkStackFrames(WalkState walkState) throws CorruptDataException;

		public void jitPrintRegisterMapArray(WalkState walkState, String description) throws CorruptDataException;
		
		public J9JITExceptionTablePointer jitGetExceptionTableFromPC(J9VMThreadPointer walkThread, U8Pointer pc) throws CorruptDataException;
	}

	/**
	 * JIT Stack Walker for Java 5 SR11
	 * @author andhall
	 */
	
	
	/**
	 * JIT Stack Walker for 2.6 circa early 2010
	 * @author andhall
	 */
	private static class JITStackWalker_29_V0 extends BaseAlgorithm implements IJITStackWalker
	{
		protected JITStackWalker_29_V0() {
			super(90, 0);
		}
		
		private static U8Pointer MASK_PC(AbstractPointer ptr)
		{
			if (J9ConfigFlags.arch_s390 && !J9BuildFlags.env_data64) {
				return U8Pointer.cast(UDATA.cast(ptr).bitAnd(0x7FFFFFFF));
			} else {
				return U8Pointer.cast(ptr);
			}
		}
		
		/* Used differently to macro it's based on. Argument is pcExpressionEA NOT pcExpression */
		private static void UPDATE_PC_FROM(WalkState walkState, PointerPointer pcExpressionEA) throws CorruptDataException
		{
			if (J9BuildFlags.jit_fullSpeedDebug) {
				walkState.pcAddress = pcExpressionEA;
				walkState.pc = MASK_PC(pcExpressionEA.at(0));
			} else {
				walkState.pc = MASK_PC(pcExpressionEA.at(0));
			}
		}
		

		private static void SET_A0_CP_METHOD(WalkState walkState) throws CorruptDataException
		{			
			walkState.arg0EA = UDATAPointer.cast(U8Pointer.cast(walkState.bp.add(walkState.jitInfo.slots())).add(J9JITFrame.SIZEOF).sub(UDATA.SIZEOF));
			walkState.method = walkState.jitInfo.ramMethod();
			walkState.constantPool = walkState.jitInfo.constantPool();
			walkState.argCount = new UDATA(walkState.jitInfo.slots());
		}

		public FrameCallbackResult jitWalkStackFrames(WalkState walkState)
				throws CorruptDataException
		{
			FrameCallbackResult rc = FrameCallbackResult.STOP_ITERATING;
			JITMaps maps = new JITMaps();
			U8Pointer failedPC;
			PointerPointer returnTable;
			UDATAPointer returnSP;
			long i;

			if ( (rc = walkTransitionFrame(walkState)) != FrameCallbackResult.KEEP_ITERATING) {
				return rc;
			}
			
			walkState.frameFlags = new UDATA(0);
			
			while ( (walkState.jitInfo = jitGetExceptionTable(walkState)).notNull() ) {
				walkState.bp = walkState.unwindSP.add(getJitTotalFrameSize(walkState.jitInfo));
				
				/* Point walkState->sp to the last outgoing argument */
				if (J9SW_NEEDS_JIT_2_INTERP_CALLEE_ARG_POP) {
					walkState.sp = walkState.unwindSP.sub(walkState.argCount);
				} else {
					walkState.sp = walkState.unwindSP;
				}
				
				walkState.outgoingArgCount = walkState.argCount;
				
				if (((walkState.flags & J9_STACKWALK_SKIP_INLINES) == 0) && getJitInlinedCallInfo(walkState.jitInfo).notNull()) {
					jitGetMapsFromPC(walkState.walkThread.javaVM(), walkState.jitInfo, UDATA.cast(walkState.pc), maps);
					if (maps.inlineMap.notNull()) {  
						VoidPointer inlinedCallSite = getFirstInlinedCallSite(walkState.jitInfo, VoidPointer.cast(maps.inlineMap));
						walkState.arg0EA = UDATAPointer.NULL;
						if (inlinedCallSite.notNull()) {
							walkState.inlineDepth = getJitInlineDepthFromCallSite(walkState.jitInfo, inlinedCallSite).longValue();
							walkState.arg0EA = UDATAPointer.NULL;
							do {
								J9MethodPointer inlinedMethod = J9MethodPointer.cast(getInlinedMethod(inlinedCallSite));
								
								walkState.method = inlinedMethod;
								walkState.constantPool = UNTAGGED_METHOD_CP(walkState.method);
								walkState.bytecodePCOffset = U8Pointer.cast(getCurrentByteCodeIndexAndIsSameReceiver(walkState.jitInfo, VoidPointer.cast(maps.inlineMap), inlinedCallSite, null));
								jitPrintFrameType(walkState, "JIT inline");
								
								if ((rc = walkFrame(walkState)) != FrameCallbackResult.KEEP_ITERATING) {
									return rc;
								}
								
								inlinedCallSite = getNextInlinedCallSite(walkState.jitInfo, inlinedCallSite);
							} while(--walkState.inlineDepth > 0);
						}
					}
				} else if ((walkState.flags & J9_STACKWALK_RECORD_BYTECODE_PC_OFFSET) != 0) {
					jitGetMapsFromPC(walkState.walkThread.javaVM(), walkState.jitInfo, UDATA.cast(walkState.pc),maps);
				}
				
				SET_A0_CP_METHOD(walkState);
				if ((walkState.flags & J9_STACKWALK_RECORD_BYTECODE_PC_OFFSET) != 0) {
					walkState.bytecodePCOffset = (maps.inlineMap.isNull()) ? U8Pointer.cast(-1) : U8Pointer.cast(getCurrentByteCodeIndexAndIsSameReceiver(walkState.jitInfo, VoidPointer.cast(maps.inlineMap), VoidPointer.NULL, null));
				}

				jitPrintFrameType(walkState, "JIT");

				if ((walkState.flags & J9_STACKWALK_ITERATE_METHOD_CLASS_SLOTS) != 0) {
					markClassesInInlineRanges(walkState.jitInfo, walkState);
				}
				
				if ((walkState.flags & J9_STACKWALK_ITERATE_O_SLOTS) != 0) {
					try {
						jitWalkFrame(walkState, true, VoidPointer.cast(maps.stackMap));
					} catch (CorruptDataException ex) {
						handleOSlotsCorruption(walkState, "JITStackWalker_29_V0", "jitWalkStackFrames", ex);
					}
				}

				if ((rc = walkFrame(walkState)) != FrameCallbackResult.KEEP_ITERATING) {
					return rc;
				}

				if ((walkState.flags & J9_STACKWALK_MAINTAIN_REGISTER_MAP) != 0) {
					try {
						CLEAR_LOCAL_REGISTER_MAP_ENTRIES(walkState);
						jitAddSpilledRegisters(walkState, VoidPointer.cast(maps.stackMap));
					} catch (CorruptDataException ex) {
						handleOSlotsCorruption(walkState, "JITStackWalker_29_V0", "jitWalkStackFrames", ex);
					}						
				}

				UNWIND_TO_NEXT_FRAME(walkState);
			}
			
			/* JIT pair with no mapping indicates a bytecoded frame */
			failedPC = walkState.pc;
			returnTable = PointerPointer.cast(walkState.walkThread.javaVM().jitConfig().i2jReturnTable());
			if (returnTable.notNull()) {
				for (i = 0; i < J9SW_JIT_RETURN_TABLE_SIZE; ++i) {
					if (failedPC.eq(U8Pointer.cast(returnTable.at(i)))) break;
				}
				if (i == J9SW_JIT_RETURN_TABLE_SIZE) {
					throw new AddressedCorruptDataException(failedPC.getAddress(),"Invalid JIT return address");
				}
			}
			UPDATE_PC_FROM(walkState, walkState.i2jState.pcEA());
			walkState.literals = walkState.i2jState.literals();
			walkState.arg0EA = walkState.i2jState.a0();
			returnSP = walkState.i2jState.returnSP();
			walkState.previousFrameFlags = new UDATA(0);
			if (returnSP.anyBitsIn(J9_STACK_FLAGS_ARGS_ALIGNED)) {
				swPrintf(walkState, 2, "I2J args were copied for alignment");
				walkState.previousFrameFlags = new UDATA(J9_STACK_FLAGS_JIT_ARGS_ALIGNED);
			}
			walkState.walkSP = returnSP.untag(3L);
			swPrintf(walkState, 2, "I2J values: PC = {0}, A0 = {1}, walkSP = {2}, literals = {3}, JIT PC = {4}, pcAddress = {5}, decomp = {6}", 
					walkState.pc.getHexAddress(),
					walkState.arg0EA.getHexAddress(), 
					walkState.walkSP.getHexAddress(), 
					walkState.literals.getHexAddress(), 
					failedPC.getHexAddress(), J9BuildFlags.jit_fullSpeedDebug ? walkState.pcAddress.getHexAddress() : "0",
					J9BuildFlags.jit_fullSpeedDebug ? walkState.decompilationStack.getHexAddress() : "0");

			return rc;
		}

		private J9JITExceptionTablePointer jitGetExceptionTable(
				WalkState walkState) throws CorruptDataException
		{
			/* this is done with a macro in C */
			if (! J9BuildFlags.jit_fullSpeedDebug) {
				return jitGetExceptionTableFromPC(walkState.walkThread, walkState.pc);
			}
			
			J9JITDecompilationInfoPointer stack;
			J9JITExceptionTablePointer result = jitGetExceptionTableFromPC(walkState.walkThread, walkState.pc);

			walkState.decompilationRecord = J9JITDecompilationInfoPointer.NULL;
			if (result.notNull()) return result;

			/* Check to see if the PC is a decompilation return point and if so, use the real PC for finding the metaData */

			if (walkState.decompilationStack != null && walkState.decompilationStack.notNull()) {
				swPrintf(walkState, 3, "(ws pcaddr = {0}, dc tos = {1}, pcaddr = {2}, pc = {3})", 
						walkState.pcAddress.getHexAddress(), 
						walkState.decompilationStack.getHexAddress(), 
						walkState.decompilationStack.pcAddress().getHexAddress(), 
						walkState.decompilationStack.pc().getHexAddress());
				if (walkState.pcAddress.eq(walkState.decompilationStack.pcAddress())) {
					walkState.pc = walkState.decompilationStack.pc();
					if (walkState.resolveFrameFlags.bitAnd(J9_STACK_FLAGS_JIT_FRAME_SUB_TYPE_MASK).eq(J9_STACK_FLAGS_JIT_EXCEPTION_CATCH_RESOLVE)) {
						walkState.pc = walkState.pc.add(1);
					}
					walkState.decompilationRecord = walkState.decompilationStack;
					walkState.decompilationStack = walkState.decompilationStack.next();
					return jitGetExceptionTableFromPC(walkState.walkThread,walkState.pc);
				}

				stack = walkState.decompilationStack;
				while ((stack = stack.next()).notNull()) {
					if (walkState.pcAddress == walkState.decompilationStack.pcAddress()) {
						swPrintf(walkState, 1, "**** decomp found not on TOS! ****");
					}
				}
			}

			return J9JITExceptionTablePointer.NULL;
		}

		public J9JITExceptionTablePointer jitGetExceptionTableFromPC(
				J9VMThreadPointer walkThread, U8Pointer pc) throws CorruptDataException
		{
			J9JITConfigPointer jitConfig = walkThread.javaVM().jitConfig();
			
			if (jitConfig.isNull()) {
				return J9JITExceptionTablePointer.NULL;
			}
			
			return jit_artifact_search(jitConfig.translationArtifacts(), UDATA.cast(MASK_PC(pc)));
		}

		/* State shared between the various walk resolve methods: */
		UDATAPointer stackSpillCursor;
		UDATA stackSpillCount;
		UDATA floatRegistersRemaining;
		UDATAPointer pendingSendScanCursor;
		int charIndex;
		
		private void jitWalkResolveMethodFrame(WalkState walkState) throws CorruptDataException
		{
			boolean walkStackedReceiver;
			J9UTF8Pointer signature;
			UDATA pendingSendSlots;
			char sigChar;
			
			//Init shared data
			stackSpillCount = null;
			stackSpillCursor = null;
			floatRegistersRemaining = new UDATA(0);
			
			if (J9SW_JIT_FLOAT_ARGUMENT_REGISTER_COUNT_DEFINED) {
				floatRegistersRemaining = new UDATA(J9SW_JIT_FLOAT_ARGUMENT_REGISTER_COUNT);
			}
			
			UDATA resolveFrameType = walkState.frameFlags.bitAnd(J9_STACK_FLAGS_JIT_FRAME_SUB_TYPE_MASK);
			
			walkState.slotType = (int) J9_STACKWALK_SLOT_TYPE_INTERNAL;
			walkState.slotIndex = -1;

			if (resolveFrameType.eq(J9_STACK_FLAGS_JIT_RECOMPILATION_RESOLVE)) {
				J9ROMMethodPointer romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(J9MethodPointer.cast(JIT_RESOLVE_PARM(walkState,1)));

				signature = J9ROMMETHOD_SIGNATURE(romMethod);
				
				pendingSendSlots = new UDATA(J9_ARG_COUNT_FROM_ROM_METHOD(romMethod)); /* receiver is already included in this arg count */
				walkStackedReceiver = romMethod.modifiers().anyBitsIn(J9AccStatic);
				if (J9SW_ARGUMENT_REGISTER_COUNT_DEFINED) {
					stackSpillCount = new UDATA(J9SW_ARGUMENT_REGISTER_COUNT);
					walkState.unwindSP = walkState.unwindSP.add(J9SW_ARGUMENT_REGISTER_COUNT);
					stackSpillCursor = walkState.unwindSP.sub(1);
				}

				walkState.unwindSP = walkState.unwindSP.add(getJitRecompilationResolvePushes());
			} else if (resolveFrameType.eq(J9_STACK_FLAGS_JIT_LOOKUP_RESOLVE)) {
				UDATAPointer interfaceObjectAndISlot = UDATAPointer.cast(JIT_RESOLVE_PARM(walkState,2));
				J9ClassPointer resolvedClass = J9ClassPointer.cast(interfaceObjectAndISlot.at(0));
				J9ROMMethodPointer romMethod;
				if (AlgorithmVersion.getVersionOf(AlgorithmVersion.ITABLE_VERSION).getAlgorithmVersion() < 1) {
					UDATA methodIndex = interfaceObjectAndISlot.at(1);
					romMethod = resolvedClass.romClass().romMethods();
					while (! methodIndex.eq(0)) {
						romMethod = nextROMMethod(romMethod);
						methodIndex = methodIndex.sub(1);
					}
				} else {
					long iTableOffset = interfaceObjectAndISlot.at(1).longValue();
					J9MethodPointer ramMethod;
					if (0 != (iTableOffset & J9_ITABLE_OFFSET_DIRECT)) {
						ramMethod = J9MethodPointer.cast(iTableOffset).untag(J9_ITABLE_OFFSET_TAG_BITS);
					} else if (0 != (iTableOffset & J9_ITABLE_OFFSET_VIRTUAL)) {
						long vTableOffset = iTableOffset & ~J9_ITABLE_OFFSET_TAG_BITS;
						J9JavaVMPointer vm = walkState.walkThread.javaVM();
						// C code uses Object from the VM constant pool, but that's not easily
						// accessible to DDR. Any class will do.
						J9ClassPointer clazz = vm.booleanArrayClass();
						ramMethod = J9MethodPointer.cast(PointerPointer.cast(clazz.longValue() + vTableOffset).at(0));
					} else {
						long methodIndex = (iTableOffset - J9ITable.SIZEOF) / UDATA.SIZEOF;
						/* The iTable now contains every method from inherited interfaces.
						 * Find the appropriate segment for the referenced method within the
						 * resolvedClass iTable.
						 */
						J9ITablePointer allInterfaces = J9ITablePointer.cast(resolvedClass.iTable());
						for(;;) {
							J9ClassPointer interfaceClass = allInterfaces.interfaceClass();
							long methodCount = interfaceClass.romClass().romMethodCount().longValue();
							if (methodIndex < methodCount) {
								/* iTable segment located */
								ramMethod = interfaceClass.ramMethods().add(methodIndex);
								break;
							}
							methodIndex -= methodCount;
							allInterfaces = allInterfaces.next();
						}
					}
					romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(ramMethod);
				}

				signature = J9ROMMETHOD_SIGNATURE(romMethod);
				pendingSendSlots = new UDATA(J9_ARG_COUNT_FROM_ROM_METHOD(romMethod)); /* receiver is already included in this arg count */
				walkStackedReceiver = true;
				if (J9SW_ARGUMENT_REGISTER_COUNT_DEFINED) {
					stackSpillCount = new UDATA(J9SW_ARGUMENT_REGISTER_COUNT);
					walkState.unwindSP = walkState.unwindSP.add(J9SW_ARGUMENT_REGISTER_COUNT);
					stackSpillCursor = walkState.unwindSP.sub(1);
				}

				if (J9SW_JIT_LOOKUP_INTERFACE_RESOLVE_OFFSET_TO_SAVED_RECEIVER_DEFINED) {
					if ((walkState.flags & J9_STACKWALK_ITERATE_O_SLOTS) != 0) {
						try {
							swPrintf(walkState, 4, "\tObject push (picBuilder interface saved receiver)");
							WALK_O_SLOT(walkState, PointerPointer.cast(walkState.unwindSP.add(J9SW_JIT_LOOKUP_INTERFACE_RESOLVE_OFFSET_TO_SAVED_RECEIVER)));
						} catch (CorruptDataException ex) {
							handleOSlotsCorruption(walkState, "JITStackWalker_29_V0", "jitWalkResolveMethodFrame", ex);
						}
					}
				}
				
				walkState.unwindSP = walkState.unwindSP.add(getJitVirtualMethodResolvePushes());
			} else if (resolveFrameType.eq(J9_STACK_FLAGS_JIT_INDUCE_OSR_RESOLVE)) {
				throw new CorruptDataException("Induced OSR resolve not handled yet");
			} else {
				UDATA cpIndex;
				J9ConstantPoolPointer constantPool;
				J9ROMMethodRefPointer romMethodRef;

				if (resolveFrameType.eq(J9_STACK_FLAGS_JIT_STATIC_METHOD_RESOLVE) || resolveFrameType.eq(J9_STACK_FLAGS_JIT_SPECIAL_METHOD_RESOLVE)) {
					if (J9SW_JIT_FLOAT_ARGUMENT_REGISTER_COUNT_DEFINED) {
						floatRegistersRemaining = new UDATA(0);
					}
					constantPool = J9ConstantPoolPointer.cast(JIT_RESOLVE_PARM(walkState,2));
					cpIndex = JIT_RESOLVE_PARM(walkState,3);
					walkState.unwindSP = walkState.unwindSP.add(getJitStaticMethodResolvePushes());
					walkStackedReceiver = (resolveFrameType.eq(J9_STACK_FLAGS_JIT_SPECIAL_METHOD_RESOLVE));
					if (J9SW_ARGUMENT_REGISTER_COUNT_DEFINED) {
						stackSpillCount = new UDATA(0);
					}
				} else {
					UDATAPointer indexAndLiterals = UDATAPointer.cast(JIT_RESOLVE_PARM(walkState,1));

					constantPool = J9ConstantPoolPointer.cast(indexAndLiterals.at(0));
					cpIndex = indexAndLiterals.at(1);

					walkStackedReceiver = true;
					if (J9SW_ARGUMENT_REGISTER_COUNT_DEFINED) {
						stackSpillCount = new UDATA(J9SW_ARGUMENT_REGISTER_COUNT);
						walkState.unwindSP = walkState.unwindSP.add(J9SW_ARGUMENT_REGISTER_COUNT);
						stackSpillCursor = walkState.unwindSP.sub(1);
					}

					if (J9SW_JIT_VIRTUAL_METHOD_RESOLVE_OFFSET_TO_SAVED_RECEIVER_DEFINED) {
						if ((walkState.flags & J9_STACKWALK_ITERATE_O_SLOTS) != 0) {
							try {
								swPrintf(walkState, 4, "\tObject push (picBuilder virtual saved receiver)");
								WALK_O_SLOT(walkState, PointerPointer.cast(walkState.unwindSP.add(J9SW_JIT_VIRTUAL_METHOD_RESOLVE_OFFSET_TO_SAVED_RECEIVER)));
							} catch (CorruptDataException ex) {
								handleOSlotsCorruption(walkState, "JITStackWalker_29_V0", "jitWalkResolveMethodFrame", ex);
							}
						}
					}

					walkState.unwindSP = walkState.unwindSP.add(getJitVirtualMethodResolvePushes());
				}

				romMethodRef = J9ROMMethodRefPointer.cast(constantPool.romConstantPool().add(cpIndex));
				signature = romMethodRef.nameAndSignature().signature();
				pendingSendSlots = getSendSlotsFromSignature(signature).add(walkStackedReceiver ? 1 : 0);
			}
			
			if ((walkState.flags & J9_STACKWALK_ITERATE_O_SLOTS) != 0) {
				try {
					pendingSendScanCursor = walkState.unwindSP.add(pendingSendSlots).sub(1);
	
					swPrintf(walkState, 3, "\tPending send scan cursor initialized to {0}", pendingSendScanCursor.getHexAddress());
					if (walkStackedReceiver) {
						if (J9SW_ARGUMENT_REGISTER_COUNT_DEFINED && ! stackSpillCount.eq(0)) {
							swPrintf(walkState, 4, "\tObject push (receiver in register spill area)");
							if ((walkState.flags & J9_STACKWALK_ITERATE_O_SLOTS) != 0) {
								try {
									WALK_O_SLOT(walkState,PointerPointer.cast(stackSpillCursor));
								} catch (CorruptDataException ex) {
									handleOSlotsCorruption(walkState, "JITStackWalker_29_V0", "jitWalkResolveMethodFrame", ex);
								}
							}
							stackSpillCount = stackSpillCount.sub(1);
							stackSpillCursor = stackSpillCursor.sub(1);
						} else {
							swPrintf(walkState, 4, "\tObject push (receiver in stack)");
							if ((walkState.flags & J9_STACKWALK_ITERATE_O_SLOTS) != 0) {
								try {
									WALK_O_SLOT(walkState, PointerPointer.cast(pendingSendScanCursor));
								} catch (CorruptDataException ex) {
									handleOSlotsCorruption(walkState, "JITStackWalker_29_V0", "jitWalkResolveMethodFrame", ex);
								}
							}
						}
						pendingSendScanCursor = pendingSendScanCursor.sub(1);
					}
	
					swPrintf(walkState, 3, "\tMethod signature: {0}", J9UTF8Helper.stringValue(signature));
	
					charIndex = 1; /* skip the opening ( */
					String signatureString = J9UTF8Helper.stringValue(signature);
					
					while ((sigChar = jitNextSigChar(signatureString)) != ')') {
						
						switch (sigChar) {
							case 'L':
								if (J9SW_ARGUMENT_REGISTER_COUNT_DEFINED && ! stackSpillCount.eq(0) ) {
									if ((walkState.flags & J9_STACKWALK_ITERATE_O_SLOTS) != 0) {
										try {
											WALK_NAMED_O_SLOT(walkState, PointerPointer.cast(stackSpillCursor), "JIT-sig-reg-");
										} catch (CorruptDataException ex) {
											handleOSlotsCorruption(walkState, "JITStackWalker_29_V0", "walkMethodFrame", ex);
										}
									}
									stackSpillCount = stackSpillCount.sub(1);
									stackSpillCursor = stackSpillCursor.sub(1);
								} else {
										if ((walkState.flags & J9_STACKWALK_ITERATE_O_SLOTS) != 0) {
											try {
												WALK_NAMED_O_SLOT(walkState, PointerPointer.cast(pendingSendScanCursor), "JIT-sig-stk-");
											} catch (CorruptDataException ex) {
												handleOSlotsCorruption(walkState, "JITStackWalker_29_V0", "walkMethodFrame", ex);
											}
										}
								}
								break;
	
							case 'D':
								if (J9SW_JIT_FLOAT_ARGUMENT_REGISTER_COUNT_DEFINED) {
									jitWalkResolveMethodFrame_walkD(walkState);
								} else {
									/* no float regs - double treated as long */
									jitWalkResolveMethodFrame_walkJ(walkState);
								}
								break;
	
			 	 			case 'F':
			 	 				if (J9SW_JIT_FLOAT_ARGUMENT_REGISTER_COUNT_DEFINED) {
									jitWalkResolveMethodFrame_walkF(walkState);
			 	 				} else {
									/* no float regs - float treated as int */
			 	 					jitWalkResolveMethodFrame_walkI(walkState);
			 	 				}
								break;
	
							case 'J':
								jitWalkResolveMethodFrame_walkJ(walkState);
								break;
							
							case 'I':
							default:
								jitWalkResolveMethodFrame_walkI(walkState);
								break;
						}
						pendingSendScanCursor = pendingSendScanCursor.sub(1);
					}
				} catch (CorruptDataException ex) {
					handleOSlotsCorruption(walkState, "JITStackWalker_29_V0", "jitWalkResolveMethodFrame", ex);
				}
			}
			
			if (J9SW_NEEDS_JIT_2_INTERP_CALLEE_ARG_POP) {
				walkState.unwindSP = walkState.unwindSP.add(pendingSendSlots);
			}
			walkState.argCount = pendingSendSlots;
		}

		private char jitNextSigChar(String signatureString) throws CorruptDataException
		{
			char utfChar = signatureString.charAt(charIndex++);
			
			switch (utfChar) {
			case '[':
				do {
					utfChar = signatureString.charAt(charIndex++);
				} while (utfChar == '[');
				if (utfChar != 'L') {
					utfChar = 'L';
					break;
				}
				/* Fall through to consume type name, utfChar == 'L' for return value */
			case 'L':
				while (signatureString.charAt(charIndex++) != ';') ;
			}
			return utfChar;
		}

		private U64Pointer jitFPRParmAddress(WalkState walkState, UDATA fpParmNumber) throws CorruptDataException
		{
			U64Pointer base = U64Pointer.cast(walkState.walkedEntryLocalStorage.jitFPRegisterStorageBase());

			if (J9ConfigFlags.arch_s390) {
				/* 390 uses FPR0/2/4/6 for arguments, so double fpParmNumber to get the right register */
				fpParmNumber = fpParmNumber.add(fpParmNumber);
				/* On 390, either vector or floating point registers are preserved in the ELS, not both.
				 * Matching VRs and FPRs overlap, with the FPR contents in the high-order bits of the VR,
				 * meaning that when the VR is saved to memory, the FPR contents are in the lowest-memory
				 * address of the VR save memory (due to 390 being big endian).
				 *
				 * Similarly, float values loaded using the LE instruction occupy the high-order bits of
				 * the target FPR, meaning that the float contents are also in the lowest-memory address
				 * of the save memory, whether it be VR or FPR.
				 *
				 * Currently, the 32 128-bit save slots for JIT VRs appear directly after the 16 64-bit
				 * save slots for JIT FPRs, so if vector registers are enabled, the save location for a
				 * JIT FPR is (base + (16 * 64) + (128 * FPRNumber)), or (base + (64 * (16 + (2 * FPRNumber)))).
				 */
				if (0 != (walkState.walkThread.javaVM().extendedRuntimeFlags().longValue() & J9_EXTENDED_RUNTIME_USE_VECTOR_REGISTERS)) {
					fpParmNumber = fpParmNumber.add(fpParmNumber).add(16);
				}
			}
			return base.add(fpParmNumber);
		}

		private void jitWalkResolveMethodFrame_walkI(WalkState walkState) throws CorruptDataException
		{
			if (J9SW_ARGUMENT_REGISTER_COUNT_DEFINED && ! stackSpillCount.eq(0)) {
				if ((walkState.flags & J9_STACKWALK_ITERATE_O_SLOTS) != 0) {
					try {
						WALK_NAMED_I_SLOT(walkState, PointerPointer.cast(stackSpillCursor), "JIT-sig-reg-");
					} catch (CorruptDataException ex) {
						handleOSlotsCorruption(walkState, "JITStackWalker_29_V0", "jitWalkResolveMethodFrame_walkI", ex);
					}
				}
				stackSpillCount = stackSpillCount.sub(1);
				stackSpillCursor = stackSpillCursor.sub(1);
			} else {
				if ((walkState.flags & J9_STACKWALK_ITERATE_O_SLOTS) != 0) {
					try {
						WALK_NAMED_I_SLOT(walkState, PointerPointer.cast(pendingSendScanCursor), "JIT-sig-stk-");
					} catch (CorruptDataException ex) {
						handleOSlotsCorruption(walkState, "JITStackWalker_29_V0", "jitWalkResolveMethodFrame_walkI", ex);
					}
				}
			}
		}
		
		private void jitWalkResolveMethodFrame_walkF(WalkState walkState) throws CorruptDataException
		{
			if (! floatRegistersRemaining.eq(0)) {
				UDATA fpParmNumber = new UDATA(J9SW_JIT_FLOAT_ARGUMENT_REGISTER_COUNT).sub(floatRegistersRemaining);
				if ((walkState.flags & J9_STACKWALK_ITERATE_O_SLOTS) != 0) {
					try {
						UDATAPointer fprSave = UDATAPointer.cast(jitFPRParmAddress(walkState, fpParmNumber));
						if (J9SW_JIT_FLOATS_PASSED_AS_DOUBLES_DEFINED && !J9BuildFlags.env_data64) {
							WALK_NAMED_I_SLOT(walkState, PointerPointer.cast(fprSave.add(1)), "JIT-sig-reg-");
						}
						WALK_NAMED_I_SLOT(walkState, PointerPointer.cast(fprSave), "JIT-sig-reg-");
					} catch (CorruptDataException ex) {
						handleOSlotsCorruption(walkState, "JITStackWalker_29_V0", "jitWalkResolveMethodFrame_walkF", ex);
					}
				}
				floatRegistersRemaining = floatRegistersRemaining.sub(1);
			} else {
				if ((walkState.flags & J9_STACKWALK_ITERATE_O_SLOTS) != 0) {
					try {
						WALK_NAMED_I_SLOT(walkState, PointerPointer.cast(pendingSendScanCursor), "JIT-sig-stk-");
					} catch (CorruptDataException ex) {
						handleOSlotsCorruption(walkState, "JITStackWalker_29_V0", "jitWalkResolveMethodFrame_walkF", ex);
					}
				}
			}
		}
		
		private void jitWalkResolveMethodFrame_walkJ(WalkState walkState) throws CorruptDataException
		{
			/* Longs always take 2 stack slots */

			pendingSendScanCursor = pendingSendScanCursor.sub(1);

			if (J9SW_ARGUMENT_REGISTER_COUNT_DEFINED && ! stackSpillCount.eq(0)) {
				/* On 64-bit, the long is in a single register.
				 *
				 * On 32-bit, parameter registers are used for longs such that the lower-numbered register
				 * holds the lower-memory half of the long (so that a store multiple on the register pair
				 * could be used to put the long back in memory).  This remains true for the case where only
				 * one parameter register remains.
				 */
				try {
					WALK_NAMED_I_SLOT(walkState, PointerPointer.cast(stackSpillCursor), "JIT-sig-reg-");
				} catch (CorruptDataException ex) {
					handleOSlotsCorruption(walkState, "JITStackWalker_29_V0", "jitWalkResolveMethodFrame_walkJ", ex);
				}
				stackSpillCount = stackSpillCount.sub(1);
				stackSpillCursor = stackSpillCursor.sub(1);
				if (!J9BuildFlags.env_data64) {
					if (! stackSpillCount.eq(0)) {
						try {
							WALK_NAMED_I_SLOT(walkState, PointerPointer.cast(stackSpillCursor), "JIT-sig-reg-");
						} catch (CorruptDataException ex) {
							handleOSlotsCorruption(walkState, "JITStackWalker_29_V0", "jitWalkResolveMethodFrame_walkJ", ex);
						}
						stackSpillCount = stackSpillCount.sub(1);
						stackSpillCursor = stackSpillCursor.sub(1);
					} else {
						if ((walkState.flags & J9_STACKWALK_ITERATE_O_SLOTS) != 0) {
							try {
								WALK_NAMED_I_SLOT(walkState, PointerPointer.cast(pendingSendScanCursor.add(1)), "JIT-sig-stk-");
							} catch (CorruptDataException ex) {
								handleOSlotsCorruption(walkState, "JITStackWalker_29_V0", "jitWalkResolveMethodFrame_walkJ", ex);
							}
						}
					}
				}
			} else {
				/* Long is entirely in memory */
				if ((walkState.flags & J9_STACKWALK_ITERATE_O_SLOTS) != 0) {
					try {
						if (!J9BuildFlags.env_data64) {
							WALK_NAMED_I_SLOT(walkState, PointerPointer.cast(pendingSendScanCursor.add(1)), "JIT-sig-stk-");
						}
						WALK_NAMED_I_SLOT(walkState, PointerPointer.cast(pendingSendScanCursor), "JIT-sig-stk-");
					} catch (CorruptDataException ex) {
						handleOSlotsCorruption(walkState, "JITStackWalker_29_V0", "jitWalkResolveMethodFrame_walkJ", ex);
					}
				}
			}
		}
		
		private void jitWalkResolveMethodFrame_walkD(WalkState walkState) throws CorruptDataException
		{

			/* Doubles always occupy two stack slots.  Move the pendingSendScanCursor back to the
			 * beginning of the two slots.
			 */
			pendingSendScanCursor = pendingSendScanCursor.sub(1);

			if (! floatRegistersRemaining.eq(0)) {
				UDATA fpParmNumber = new UDATA(J9SW_JIT_FLOAT_ARGUMENT_REGISTER_COUNT).sub(floatRegistersRemaining);
				if ((walkState.flags & J9_STACKWALK_ITERATE_O_SLOTS) != 0) {
					try {
						UDATAPointer fprSave = UDATAPointer.cast(jitFPRParmAddress(walkState, fpParmNumber));
						if (!J9BuildFlags.env_data64) {
							WALK_NAMED_I_SLOT(walkState, PointerPointer.cast(fprSave.add(1)), "JIT-sig-reg-");
						}
						WALK_NAMED_I_SLOT(walkState, PointerPointer.cast(fprSave), "JIT-sig-reg-");
					} catch (CorruptDataException ex) {
						handleOSlotsCorruption(walkState, "JITStackWalker_29_V0", "jitWalkResolveMethodFrame_walkD", ex);
					}
				}
				floatRegistersRemaining = floatRegistersRemaining.sub(1);
			} else {
				if ((walkState.flags & J9_STACKWALK_ITERATE_O_SLOTS) != 0) {
					try {
						if (!J9BuildFlags.env_data64) {
							WALK_NAMED_I_SLOT(walkState, PointerPointer.cast(pendingSendScanCursor.add(1)), "JIT-sig-reg-");
						}
						WALK_NAMED_I_SLOT(walkState, PointerPointer.cast(pendingSendScanCursor), "JIT-sig-stk-");
					} catch (CorruptDataException ex) {
						handleOSlotsCorruption(walkState, "JITStackWalker_29_V0", "jitWalkResolveMethodFrame_walkD", ex);
					}
				}
			}
		}

		
		private FrameCallbackResult walkTransitionFrame(WalkState walkState) throws CorruptDataException
		{
			if ((walkState.flags & J9_STACKWALK_START_AT_JIT_FRAME) != 0) {
				walkState.flags &= ~J9_STACKWALK_START_AT_JIT_FRAME;
				walkState.unwindSP = walkState.walkSP;
				if (J9BuildFlags.jit_fullSpeedDebug) {
					walkState.pcAddress = PointerPointer.NULL;
				}
				return FrameCallbackResult.KEEP_ITERATING;
			}

			if (walkState.frameFlags.anyBitsIn(J9_STACK_FLAGS_JIT_RESOLVE_FRAME)) {
				J9SFJITResolveFramePointer resolveFrame = J9SFJITResolveFramePointer.cast(U8Pointer.cast(walkState.bp).sub(com.ibm.j9ddr.vm29.structure.J9SFJITResolveFrame.SIZEOF).add(UDATA.SIZEOF));
				UDATA resolveFrameType = walkState.frameFlags.bitAnd(J9_STACK_FLAGS_JIT_FRAME_SUB_TYPE_MASK);
				
				//Using if rather than switch because you can only switch on int constants
				
				if (resolveFrameType.eq(J9_STACK_FLAGS_JIT_GENERIC_RESOLVE)) swPrintf(walkState, 3, "\tGeneric resolve");
				else if (resolveFrameType.eq(J9_STACK_FLAGS_JIT_STACK_OVERFLOW_RESOLVE_FRAME)) swPrintf(walkState, 3, "\tStack overflow resolve");
				else if (resolveFrameType.eq(J9_STACK_FLAGS_JIT_DATA_RESOLVE)) swPrintf(walkState, 3, "\tData resolve");
				else if (resolveFrameType.eq(J9_STACK_FLAGS_JIT_RUNTIME_HELPER_RESOLVE)) swPrintf(walkState, 3, "\tRuntime helper resolve");
				else if (resolveFrameType.eq(J9_STACK_FLAGS_JIT_LOOKUP_RESOLVE)) swPrintf(walkState, 3, "\tInterface lookup resolve");
				else if (resolveFrameType.eq(J9_STACK_FLAGS_JIT_INTERFACE_METHOD_RESOLVE)) swPrintf(walkState, 3, "\tInterface method resolve");
				else if (resolveFrameType.eq(J9_STACK_FLAGS_JIT_SPECIAL_METHOD_RESOLVE)) swPrintf(walkState, 3, "\tSpecial method resolve");
				else if (resolveFrameType.eq(J9_STACK_FLAGS_JIT_STATIC_METHOD_RESOLVE)) swPrintf(walkState, 3, "\tStatic method resolve");
				else if (resolveFrameType.eq(J9_STACK_FLAGS_JIT_VIRTUAL_METHOD_RESOLVE)) swPrintf(walkState, 3, "\tVirtual method resolve");
				else if (resolveFrameType.eq(J9_STACK_FLAGS_JIT_RECOMPILATION_RESOLVE)) swPrintf(walkState, 3, "\tRecompilation resolve");
				else if (resolveFrameType.eq(J9_STACK_FLAGS_JIT_MONITOR_ENTER_RESOLVE)) swPrintf(walkState, 3, "\tMonitor enter resolve");
				else if (resolveFrameType.eq(J9_STACK_FLAGS_JIT_METHOD_MONITOR_ENTER_RESOLVE)) swPrintf(walkState, 3, "\tMethod monitor enter resolve");
				else if (resolveFrameType.eq(J9_STACK_FLAGS_JIT_FAILED_METHOD_MONITOR_ENTER_RESOLVE)) swPrintf(walkState, 3, "\tFailed method monitor enter resolve");
				else if (resolveFrameType.eq(J9_STACK_FLAGS_JIT_ALLOCATION_RESOLVE)) swPrintf(walkState, 3, "\tAllocation resolve");
				else if (resolveFrameType.eq(J9_STACK_FLAGS_JIT_BEFORE_ANEWARRAY_RESOLVE)) swPrintf(walkState, 3, "\tBefore anewarray resolve");
				else if (resolveFrameType.eq(J9_STACK_FLAGS_JIT_BEFORE_MULTIANEWARRAY_RESOLVE)) swPrintf(walkState, 3, "\tBefore multianewarray resolve");
				else if (resolveFrameType.eq(J9_STACK_FLAGS_JIT_INDUCE_OSR_RESOLVE)) swPrintf(walkState, 3, "\tInduce OSR resolve");
				else if (resolveFrameType.eq(J9_STACK_FLAGS_JIT_EXCEPTION_CATCH_RESOLVE)) swPrintf(walkState, 3, "\tException catch resolve");
				else swPrintf(walkState, 3, "\tUnknown resolve type {0}", resolveFrameType.getHexValue());

				if (resolveFrameType.eq(J9_STACK_FLAGS_JIT_EXCEPTION_CATCH_RESOLVE)) {
					swPrintf(walkState, 3, "\tAt exception catch - incrementing PC to make map fetching work");
					walkState.pc = walkState.pc.add(1);
				}

				UPDATE_PC_FROM(walkState, resolveFrame.returnAddressEA());
				if (J9BuildFlags.jit_fullSpeedDebug) {
					walkState.resolveFrameFlags = walkState.frameFlags;
				}

				walkState.unwindSP = UDATAPointer.cast(resolveFrame.taggedRegularReturnSP()).untag(3);
				swPrintf(walkState, 3, "\tunwindSP initialized to {0}", walkState.unwindSP.getHexAddress());
				if (J9SW_JIT_HELPERS_PASS_PARAMETERS_ON_STACK) {
					walkState.unwindSP = walkState.unwindSP.add(resolveFrame.parmCount());
					swPrintf(walkState, 3, "\tAdding {0} slots of pushed resolve parameters", resolveFrame.parmCount());
				}

				if (resolveFrameType.eq(J9_STACK_FLAGS_JIT_DATA_RESOLVE)) {
					if ((walkState.flags & J9_STACKWALK_MAINTAIN_REGISTER_MAP) != 0) {
						jitAddSpilledRegistersForDataResolve(walkState);
					}

					walkState.unwindSP = walkState.unwindSP.add(getJitDataResolvePushes());
					swPrintf(walkState, 3, "\tData resolve added {0} slots", getJitDataResolvePushes());
				} else {
					if ((walkState.flags & J9_STACKWALK_MAINTAIN_REGISTER_MAP) != 0) {
						jitAddSpilledRegistersForResolve(walkState);
					}

					if ((resolveFrameType.eq(J9_STACK_FLAGS_JIT_VIRTUAL_METHOD_RESOLVE))
					|| (resolveFrameType.eq(J9_STACK_FLAGS_JIT_INTERFACE_METHOD_RESOLVE))
					|| (resolveFrameType.eq(J9_STACK_FLAGS_JIT_SPECIAL_METHOD_RESOLVE))
					|| (resolveFrameType.eq(J9_STACK_FLAGS_JIT_STATIC_METHOD_RESOLVE))
					|| (resolveFrameType.eq(J9_STACK_FLAGS_JIT_LOOKUP_RESOLVE))
					|| (resolveFrameType.eq(J9_STACK_FLAGS_JIT_RECOMPILATION_RESOLVE))
					|| (resolveFrameType.eq(J9_STACK_FLAGS_JIT_INDUCE_OSR_RESOLVE))
					) {
						jitWalkResolveMethodFrame(walkState);
					} else if ((resolveFrameType.eq(J9_STACK_FLAGS_JIT_STACK_OVERFLOW_RESOLVE_FRAME))
					|| (resolveFrameType.eq(J9_STACK_FLAGS_JIT_FAILED_METHOD_MONITOR_ENTER_RESOLVE))
					) {
						boolean inMethodPrologue = resolveFrameType.eq(J9_STACK_FLAGS_JIT_STACK_OVERFLOW_RESOLVE_FRAME);
						walkState.jitInfo = jitGetExceptionTable(walkState);
						walkState.bp = walkState.unwindSP;
						if (!inMethodPrologue) {
							walkState.bp = walkState.bp.add(getJitTotalFrameSize(walkState.jitInfo));
						}
						walkState.outgoingArgCount = new UDATA(0);
						SET_A0_CP_METHOD(walkState);
						jitPrintFrameType(walkState, "JIT (hidden)");

						try {
							if ((walkState.flags & J9_STACKWALK_ITERATE_O_SLOTS) != 0) {
								jitWalkFrame(walkState, !inMethodPrologue, VoidPointer.NULL);
							}
						} catch (CorruptDataException ex) {
							handleOSlotsCorruption(walkState, "JITStackWalker_29_V0", "walkTransitionFrame", ex);
						}
					
						if ((walkState.flags & J9_STACKWALK_ITERATE_HIDDEN_JIT_FRAMES) != 0) {
							FrameCallbackResult rc;

							walkState.frameFlags = new UDATA(0);
							if ((rc = walkFrame(walkState)) != FrameCallbackResult.KEEP_ITERATING) {
								return rc;
							}
						}

						if ((walkState.flags & J9_STACKWALK_MAINTAIN_REGISTER_MAP) != 0) {
							CLEAR_LOCAL_REGISTER_MAP_ENTRIES(walkState);
						}

						UNWIND_TO_NEXT_FRAME(walkState);
					}
				}
			} else if (walkState.frameFlags.anyBitsIn(J9_STACK_FLAGS_JIT_JNI_CALL_OUT_FRAME)) {	
				J9SFNativeMethodFramePointer nativeMethodFrame = J9SFNativeMethodFramePointer.cast(walkState.bp.subOffset(J9SFNativeMethodFrame.SIZEOF).addOffset(UDATA.SIZEOF));

				UPDATE_PC_FROM(walkState, nativeMethodFrame.savedCPEA());
				if (J9BuildFlags.interp_growableStacks) {
					if (walkState.frameFlags.anyBitsIn(J9_SSF_JNI_REFS_REDIRECTED) && (walkState.flags & J9_STACKWALK_ITERATE_O_SLOTS) != 0) {
						try {
							UDATAPointer currentBP = walkState.bp;
	
							walkState.jitInfo = jitGetExceptionTable(walkState);
							walkState.unwindSP = UDATAPointer.cast(nativeMethodFrame.savedPC()).add(1);
							walkState.bp = walkState.unwindSP.add(getJitTotalFrameSize(walkState.jitInfo));
							SET_A0_CP_METHOD(walkState);
							jitPrintFrameType(walkState, "JIT (redirected JNI refs)");
							jitWalkFrame(walkState, true, VoidPointer.NULL);
							walkState.bp = currentBP;
						} catch (CorruptDataException ex) {
							handleOSlotsCorruption(walkState, "JITStackWalker_29_V0", "walkTransitionFrame", ex);
						}
					}
				}
				walkState.unwindSP = walkState.bp.add(1);
			} else if (walkState.frameFlags.anyBitsIn(J9_STACK_FLAGS_JIT_CALL_IN_FRAME)) {
				UDATA callInFrameType = walkState.frameFlags.bitAnd(J9_STACK_FLAGS_JIT_FRAME_SUB_TYPE_MASK);
				if (callInFrameType.eq(J9_STACK_FLAGS_JIT_CALL_IN_TYPE_J2_I)) {
					J9SFJ2IFramePointer j2iFrame = J9SFJ2IFramePointer.cast(walkState.bp.subOffset(J9SFJ2IFrame.SIZEOF).addOffset(UDATA.SIZEOF));
	
					walkState.i2jState = J9I2JStatePointer.cast(j2iFrame.i2jStateEA());
					walkState.j2iFrame = j2iFrame.previousJ2iFrame();
	
					if ((walkState.flags & J9_STACKWALK_MAINTAIN_REGISTER_MAP) != 0) {
						jitAddSpilledRegistersForJ2I(walkState);
					}
	
					walkState.unwindSP = j2iFrame.taggedReturnSP().untag(3L);
					UPDATE_PC_FROM(walkState, j2iFrame.returnAddressEA());
				}
			} else { /* inl */
				J9SFSpecialFramePointer specialFrame = J9SFSpecialFramePointer.cast(walkState.bp.subOffset(J9SFSpecialFrame.SIZEOF).addOffset(UDATA.SIZEOF));

				if ((walkState.flags & J9_STACKWALK_MAINTAIN_REGISTER_MAP) != 0) {
					jitAddSpilledRegistersForINL(walkState);
				}
				/* Do not add J9SW_JIT_STACK_SLOTS_USED_BY_CALL as it has been popped from the stack by the call to INL helper */
				if (J9SW_NEEDS_JIT_2_INTERP_CALLEE_ARG_POP) {
					walkState.unwindSP = walkState.arg0EA.add(1);
				} else {
					walkState.unwindSP = specialFrame.savedA0().untag(3L);
				}
				UPDATE_PC_FROM(walkState, specialFrame.savedCPEA());
			}

			return FrameCallbackResult.KEEP_ITERATING;
		}
		
		private void jitAddSpilledRegistersForResolve(WalkState walkState) throws CorruptDataException
		{
			try {
				UDATAPointer slotCursor = walkState.walkedEntryLocalStorage.jitGlobalStorageBase();
				int mapCursor = 0;
				int i;

				for (i = 0; i < J9SW_POTENTIAL_SAVED_REGISTERS; ++i) {
					walkState.registerEAs[mapCursor++] = slotCursor;
					slotCursor = slotCursor.add(1);
				}

				jitPrintRegisterMapArray(walkState, "Resolve");
			} catch (CorruptDataException ex) {
				handleOSlotsCorruption(walkState, "JITStackWalker_29_V0", "jitWalkStackFrames", ex);
			}
		}

		private void jitAddSpilledRegistersForINL(WalkState walkState) throws CorruptDataException
		{
			UDATAPointer slotCursor = walkState.walkedEntryLocalStorage.jitGlobalStorageBase();
			int i;
		
			for (i = 0; i < J9SW_JIT_CALLEE_PRESERVED_SIZE; ++i) {
				int regNumber = jitCalleeSavedRegisterList[i];

				walkState.registerEAs[regNumber] = slotCursor.add(regNumber);
			}

			jitPrintRegisterMapArray(walkState, "INL");
		}

		
		private void jitAddSpilledRegistersForJ2I(WalkState walkState) throws CorruptDataException
		{
			J9SFJ2IFramePointer j2iFrame = J9SFJ2IFramePointer.cast(walkState.bp.subOffset(J9SFJ2IFrame.SIZEOF).addOffset(UDATA.SIZEOF));
			UDATAPointer slotCursor = UDATAPointer.cast(j2iFrame.previousJ2iFrameEA()).add(1);
			int i;
		 
			for (i = 0; i < J9SW_JIT_CALLEE_PRESERVED_SIZE; ++i) {
				int regNumber = jitCalleeSavedRegisterList[i];
				
				walkState.registerEAs[regNumber] = slotCursor;
				slotCursor = slotCursor.add(1);
			}
		
			jitPrintRegisterMapArray(walkState, "J2I");
		}

		
		public void jitPrintRegisterMapArray(WalkState walkState, String description) throws CorruptDataException
		{
			for (int i = 0; i < J9SW_POTENTIAL_SAVED_REGISTERS; ++i) {
				UDATAPointer registerSaveAddress = walkState.registerEAs[i];

				if (!registerSaveAddress.eq(UDATAPointer.NULL)) {
					swPrintf(walkState, 3, "\tJIT-{0}-RegisterMap[{1}] = {2} ({3})", description, 
						registerSaveAddress.getHexAddress(), registerSaveAddress.at(0), jitRegisterNames[i]);
				}
			}
		}
		
		private void CLEAR_LOCAL_REGISTER_MAP_ENTRIES(WalkState walkState)
		{
			int clearRegNum;
			for (clearRegNum = 0; clearRegNum < (J9SW_POTENTIAL_SAVED_REGISTERS - J9SW_JIT_CALLEE_PRESERVED_SIZE); ++clearRegNum) {
				walkState.registerEAs[jitCalleeDestroyedRegisterList[clearRegNum]] = UDATAPointer.NULL;
			}
		}

		private void UNWIND_TO_NEXT_FRAME(WalkState walkState) throws CorruptDataException
		{
			if (J9SW_NEEDS_JIT_2_INTERP_CALLEE_ARG_POP) {
				walkState.unwindSP = UDATAPointer.cast(J9JITFramePointer.cast(walkState.bp).add(1)).add(walkState.argCount);
				COMMON_UNWIND_TO_NEXT_FRAME(walkState);
			} else {
				/* arguments are passed in register, argument area lives in caller frame */
				walkState.unwindSP = UDATAPointer.cast(J9JITFramePointer.cast(walkState.bp).add(1));
				COMMON_UNWIND_TO_NEXT_FRAME(walkState);
			}
			
		}

		private void COMMON_UNWIND_TO_NEXT_FRAME(WalkState walkState) throws CorruptDataException
		{
			if (J9BuildFlags.jit_fullSpeedDebug) {
				walkState.resolveFrameFlags = new UDATA(0);
			}
			UPDATE_PC_FROM(walkState, J9JITFramePointer.cast(walkState.bp).returnPCEA());
		}

		private void jitWalkFrame(WalkState walkState, boolean walkLocals, VoidPointer stackMap) throws CorruptDataException
		{
			J9JITStackAtlasPointer gcStackAtlas;
			UDATAPointer objectArgScanCursor;
			UDATAPointer objectTempScanCursor;
			WALK_METHOD_CLASS(walkState);

			if (stackMap.isNull()) {
				stackMap = getStackMapFromJitPC(walkState.walkThread.javaVM(), walkState.jitInfo, UDATA.cast(walkState.pc));
				if (stackMap.isNull()) {
					throw new AddressedCorruptDataException(walkState.jitInfo.getAddress(),"Unable to locate JIT stack map");
				}
			}

			gcStackAtlas = getJitGCStackAtlas(walkState.jitInfo);

			swPrintf(walkState, 2, "\tstackMap={0}, slots={1} parmBaseOffset={2}, parmSlots={3}, localBaseOffset={4}",
							stackMap.getHexAddress(), walkState.jitInfo.slots(), gcStackAtlas.parmBaseOffset(), gcStackAtlas.numberOfParmSlots(), gcStackAtlas.localBaseOffset());

			objectArgScanCursor = getObjectArgScanCursor(walkState); 
			jitBitsRemaining = new UDATA(0);
			mapBytesRemaining = new UDATA(getJitNumberOfMapBytes(gcStackAtlas));

			jitDescriptionCursor = getJitStackSlots(walkState.jitInfo, stackMap);
			stackAllocMapCursor = U8Pointer.cast(getStackAllocMapFromJitPC(walkState.walkThread.javaVM(), walkState.jitInfo, UDATA.cast(walkState.pc), stackMap));
			stackAllocMapBits = new U8(0);

			walkState.slotType = (int)J9_STACKWALK_SLOT_TYPE_METHOD_LOCAL;
			walkState.slotIndex = 0;

			if (! getJitNumberOfParmSlots(gcStackAtlas).eq(0)) {
				swPrintf(walkState, 4, "\tDescribed JIT args starting at {0} for {1} slots", objectArgScanCursor.getHexAddress(), gcStackAtlas.numberOfParmSlots());
				walkJITFrameSlots(walkState, objectArgScanCursor, new UDATA(getJitNumberOfParmSlots(gcStackAtlas)), stackMap, J9JITStackAtlasPointer.NULL, ": a");
			}

			if (walkLocals) {
				objectTempScanCursor = getObjectTempScanCursor(walkState);

				if (! walkState.bp.sub(UDATA.cast(objectTempScanCursor)).eq(UDATAPointer.NULL)) {
					swPrintf(walkState, 4, "\tDescribed JIT temps starting at {0} for {1} slots", objectTempScanCursor.getHexAddress(), walkState.bp.sub(objectTempScanCursor));
					walkJITFrameSlots(walkState, objectTempScanCursor, new UDATA(walkState.bp.subOffset(UDATA.cast(objectTempScanCursor)).longValue() / UDATA.SIZEOF), stackMap, gcStackAtlas, ": t");
				}
			}

			jitWalkRegisterMap(walkState, stackMap, gcStackAtlas);

			/* If there is an OSR buffer attached to this frame, walk it */

			J9JITDecompilationInfoPointer decompilationRecord = walkState.decompilationRecord;
			if (!decompilationRecord.isNull()) {
				J9OSRBufferPointer osrBuffer = J9OSRBufferPointer.cast(decompilationRecord.osrBufferEA());

				if (!osrBuffer.numberOfFrames().eq(0)) {
					jitWalkOSRBuffer(walkState, osrBuffer);
				}
			}

		}
		
		// Shared data for walkJITFrameSlots
		// Passed by reference in native implementation
		
		private U8 jitDescriptionBits;
		private U8 stackAllocMapBits;
		private U8Pointer jitDescriptionCursor;
		private U8Pointer stackAllocMapCursor;
		private UDATA jitBitsRemaining;
		private UDATA mapBytesRemaining;
		
		private void walkJITFrameSlots(WalkState walkState, UDATAPointer scanCursor, UDATA slotsRemaining, VoidPointer stackMap, J9JITStackAtlasPointer gcStackAtlas, String slotDescription) throws CorruptDataException
		{
			/* GC stack atlas passed in to this function is NULL when this method 
			   is called for parameters in the frame; when this method is called for 
			   autos in the frame, the actual stack atlas is passed in. This 
			   works because no internal pointers/pinning arrays can be parameters.
			   We must avoid doing internal pointer fixup twice for a 
			   given stack frame as this method is called twice for each 
			   stack frame (once for parms and once for autos). 
			
			   If internal pointer map exists in the atlas it means there are internal pointer 
			   regs/autos in this method.
			*/

			if (gcStackAtlas.notNull() && getJitInternalPointerMap(gcStackAtlas).notNull()) 
			{
				walkJITFrameSlotsForInternalPointers(walkState, jitDescriptionCursor, scanCursor, stackMap, gcStackAtlas);
			}
			
			while (! slotsRemaining.eq(0)) 
			{
				if (jitBitsRemaining.eq(0)) 
				{
					if (! mapBytesRemaining.eq(0)) {
						jitDescriptionBits = getNextDescriptionBit(jitDescriptionCursor);
						//We have to move jitDescriptionCursor on ourselves, DDR getNextDescriptionBits doesn't do it for us  
						jitDescriptionCursor = jitDescriptionCursor.add(1);
						
						if (stackAllocMapCursor.notNull()) {
							stackAllocMapBits = getNextDescriptionBit(stackAllocMapCursor);
							stackAllocMapCursor = stackAllocMapCursor.add(1);
						}
						
						mapBytesRemaining = mapBytesRemaining.sub(1);
					} else {
						jitDescriptionBits = new U8(0);
					}
					jitBitsRemaining = new UDATA(8);
				}
				
				if (jitDescriptionBits.anyBitsIn(1)) {
					String indexedTag = String.format("O-Slot: %s%d", slotDescription, slotsRemaining.sub(1).intValue());
					WALK_NAMED_O_SLOT(walkState,PointerPointer.cast(scanCursor), indexedTag);
				} else if (stackAllocMapBits.anyBitsIn(1)) {
					jitWalkStackAllocatedObject(walkState, J9ObjectPointer.cast(scanCursor));
				} else {
					String indexedTag = String.format("I-Slot: %s%d", slotDescription, slotsRemaining.sub(1).intValue());
					WALK_NAMED_I_SLOT(walkState,PointerPointer.cast(scanCursor), indexedTag);
				}

				walkState.slotIndex++;
				scanCursor = scanCursor.add(1);
				jitBitsRemaining = jitBitsRemaining.sub(1);
				jitDescriptionBits = jitDescriptionBits.rightShift(1);
				stackAllocMapBits = stackAllocMapBits.rightShift(1);
				slotsRemaining = slotsRemaining.sub(1);
			}
		}
		
		/* This function is invoked for each stack allocated object. Unless a specific callback
		 * has been provided for stack allocated objects it creates synthetic slots for the object's
		 * fields and reports them using WALK_O_SLOT()
		 */
		private static void
		jitWalkStackAllocatedObject(WalkState walkState, J9ObjectPointer object) throws CorruptDataException
		{
//			if (J9_STACKWALK_INCLUDE_ARRAYLET_LEAVES == (walkState.flags & J9_STACKWALK_INCLUDE_ARRAYLET_LEAVES)) {
//				iterateObjectSlotsFlags = iterateObjectSlotsFlags.bitOr(j9mm_iterator_flag_include_arraylet_leaves);
//			}
			
			swPrintf(walkState, 4, "\t\tSA-Obj[{0}]", Long.toHexString(object.getAddress()));
			
			GCObjectIterator it = GCObjectIterator.fromJ9Object(object, false);
			
			while (it.hasNext()) {
				ObjectReferencePointer slot = ObjectReferencePointer.cast(it.nextAddress());
				
				swPrintf(walkState, 4, "\t\t\tF-Slot[{0}] = {1}", slot.getHexAddress(), slot.at(0).getHexAddress());

				if (walkState.callBacks != null) {
					walkState.callBacks.fieldSlotWalkFunction(walkState.walkThread, walkState, slot, VoidPointer.cast(slot));
				}

			}
			
		}
		
		
		private static void jitPrintFrameType(WalkState walkState, String frameType) throws CorruptDataException
		{
			swPrintf(walkState, 2, "{0} frame: bp = {1}, pc = {2}, unwindSP = {3}, cp = {4}, arg0EA = {5}, jitInfo = {6}", frameType, 
					walkState.bp.getHexAddress(), 
					walkState.pc.getHexAddress(), 
					walkState.unwindSP.getHexAddress(), 
					walkState.constantPool.getHexAddress(), 
					walkState.arg0EA.getHexAddress(), walkState.jitInfo.getHexAddress());
			try {
				swPrintMethod(walkState);
			} catch (CorruptDataException ex) {
				handleOSlotsCorruption(walkState, "JITStackWalker_29_V0", "jitWalkStackFrames", ex);
			}

			swPrintf(walkState, 3, "\tBytecode index = {0}, inlineDepth = {1}, PC offset = {2}", UDATA.cast(walkState.bytecodePCOffset).longValue(), walkState.inlineDepth, 
					walkState.pc.sub(UDATA.cast(walkState.method.extra())).getHexAddress());
		}
		
		
		private void jitWalkRegisterMap(WalkState walkState, VoidPointer stackMap, J9JITStackAtlasPointer gcStackAtlas) throws CorruptDataException
		{
			UDATA registerMap = new UDATA(getJitRegisterMap(walkState.jitInfo, stackMap).bitAnd(J9SW_REGISTER_MAP_MASK));

			swPrintf(walkState, 200, "\tIn jitWalkRegisterMap. stackMap={0}, gcStackAtlas={1}", Long.toHexString(stackMap.getAddress()), Long.toHexString(gcStackAtlas.getAddress()));
			
			swPrintf(walkState, 3, "\tJIT-RegisterMap = {0}", registerMap);

			if (gcStackAtlas.internalPointerMap().notNull()) {
				registerMap = registerMap.bitAnd(new UDATA(INTERNAL_PTR_REG_MASK).bitNot());
			}

			if (! registerMap.eq(0)) {
				int count = (int)J9SW_POTENTIAL_SAVED_REGISTERS;
				int mapCursor = 0;

				if (J9SW_REGISTER_MAP_WALK_REGISTERS_LOW_TO_HIGH) {
					mapCursor = 0;
				} else {
					mapCursor = (int)J9SW_POTENTIAL_SAVED_REGISTERS - 1;
				}

				walkState.slotType = (int) J9_STACKWALK_SLOT_TYPE_JIT_REGISTER_MAP;
				walkState.slotIndex = 0;

				while (count != 0) {
					if (registerMap.anyBitsIn(1)) {
						PointerPointer targetObject = PointerPointer.cast(walkState.registerEAs[mapCursor]);

						J9ObjectPointer oldObject = targetObject.isNull() ? J9ObjectPointer.NULL : J9ObjectPointer.cast(targetObject.at(0));
						J9ObjectPointer newObject;
						swPrintf(walkState, 4, "\t\tJIT-RegisterMap-O-Slot[{0}] = {1} ({2})", 
								targetObject.getHexAddress(), 
								oldObject.getHexAddress(), jitRegisterNames[mapCursor]);
						walkState.callBacks.objectSlotWalkFunction(walkState.walkThread, walkState, targetObject, VoidPointer.cast(targetObject));
		
						newObject = targetObject.isNull() ? J9ObjectPointer.NULL : J9ObjectPointer.cast(targetObject.at(0));
		
						if (! oldObject.eq(newObject)) {
							swPrintf(walkState, 4, "\t\t\t-> {0}\n", newObject);
						}
					} else {
						UDATAPointer targetSlot = walkState.registerEAs[mapCursor];

						if (targetSlot.notNull()) {
							swPrintf(walkState, 5, "\t\tJIT-RegisterMap-I-Slot[{0}] = {1} ({2})", 
									targetSlot.getHexAddress(), targetSlot.at(0), jitRegisterNames[mapCursor]);
						}
					}

					++(walkState.slotIndex);
					--count;
					registerMap = registerMap.rightShift(1);
					
					if (J9SW_REGISTER_MAP_WALK_REGISTERS_LOW_TO_HIGH) {
						++mapCursor;
					} else {
						--mapCursor;
					}
				}
			}
		}

		private J9OSRFramePointer jitWalkOSRFrame(WalkState walkState, J9OSRFramePointer osrFrame) throws CorruptDataException
		{
			J9MethodPointer method = osrFrame.method();
			UDATA offsetPC = osrFrame.bytecodePCOffset();
			UDATA numberOfLocals = osrFrame.numberOfLocals();
			UDATA maxStack = osrFrame.maxStack();
			UDATA pendingStackHeight = osrFrame.pendingStackHeight();
			U8Pointer bytecodePC = method.bytecodes().add(offsetPC);
			UDATAPointer localSlots = UDATAPointer.cast(osrFrame.add(1)).add(maxStack);
			UDATAPointer nextFrame = localSlots.add(numberOfLocals);

			swPrintf(walkState, 3, "\tJIT-OSRFrame = {0}, bytecodePC = {1}, numberOfLocals = {2}, maxStack = {3}, pendingStackHeight = {4}", osrFrame.getHexAddress(), bytecodePC.getHexAddress(), numberOfLocals, maxStack, pendingStackHeight);
			swPrintMethod(walkState, method);

			StackWalker.walkBytecodeFrameSlots(walkState, method, offsetPC,
					localSlots.sub(1), pendingStackHeight,
					nextFrame.sub(1), numberOfLocals);

			return J9OSRFramePointer.cast(nextFrame);
		}

		private void jitWalkOSRBuffer(WalkState walkState, J9OSRBufferPointer osrBuffer) throws CorruptDataException
		{
			long numberOfFrames = osrBuffer.numberOfFrames().longValue();
			J9OSRFramePointer currentFrame = J9OSRFramePointer.cast(osrBuffer.add(1));

			swPrintf(walkState, 3, "\tJIT-OSRBuffer = {0}, numberOfFrames = {1}", osrBuffer.getHexAddress(), numberOfFrames);

			/* Walk all of the frames (assume there is at least one) */

			do {
				currentFrame = jitWalkOSRFrame(walkState, currentFrame);
				numberOfFrames -= 1;
			} while (0 != numberOfFrames);
		}

	}

}
