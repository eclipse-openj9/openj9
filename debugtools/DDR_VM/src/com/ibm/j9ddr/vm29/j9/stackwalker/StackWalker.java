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
import com.ibm.j9ddr.InvalidDataTypeException;
import com.ibm.j9ddr.corereaders.osthread.IOSThread;
import com.ibm.j9ddr.corereaders.osthread.IRegister;
import com.ibm.j9ddr.vm29.pointer.generated.J9JITDecompilationInfoPointer;
import com.ibm.j9ddr.vm29.j9.AlgorithmPicker;
import com.ibm.j9ddr.vm29.j9.AlgorithmVersion;
import com.ibm.j9ddr.vm29.j9.BaseAlgorithm;
import com.ibm.j9ddr.vm29.j9.IAlgorithm;
import com.ibm.j9ddr.vm29.j9.J9ConfigFlags;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.U32Pointer;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.UDATAPointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ConstantPoolPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9I2JStatePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JITExceptionTablePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMMethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9SFJ2IFramePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9SFJITResolveFramePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9SFJNICallInFramePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9SFMethodFramePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9SFMethodTypeFramePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9SFSpecialFramePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9SFStackFramePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMEntryLocalStoragePointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ROMMethodHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ThreadHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9UTF8Helper;
import com.ibm.j9ddr.vm29.structure.J9JavaVM;
import com.ibm.j9ddr.vm29.structure.J9SFJ2IFrame;
import com.ibm.j9ddr.vm29.structure.J9SFJNICallInFrame;
import com.ibm.j9ddr.vm29.structure.J9SFStackFrame;
import com.ibm.j9ddr.vm29.types.U8;
import com.ibm.j9ddr.vm29.types.UDATA;

import static com.ibm.j9ddr.vm29.events.EventManager.raiseCorruptDataEvent;
import static com.ibm.j9ddr.vm29.j9.ROMHelp.*;
import static com.ibm.j9ddr.vm29.j9.stackmap.DebugLocalMap.*;
import static com.ibm.j9ddr.vm29.j9.stackmap.LocalMap.*;
import static com.ibm.j9ddr.vm29.j9.stackmap.StackMap.*;
import static com.ibm.j9ddr.vm29.j9.stackwalker.JITStackWalker.*;
import static com.ibm.j9ddr.vm29.j9.stackwalker.StackWalkerUtils.*;
import static com.ibm.j9ddr.vm29.structure.J9Consts.*;
import static com.ibm.j9ddr.vm29.structure.J9JavaAccessFlags.*;
import static com.ibm.j9ddr.vm29.structure.J9SFStackFrame.*;
import static com.ibm.j9ddr.vm29.structure.J9StackWalkConstants.*;
import static com.ibm.j9ddr.vm29.structure.J9StackWalkState.*;

/**
 * StackWalker
 * 
 * @author andhall
 * 
 */
public class StackWalker
{
	private static final long LOCALS_ARRAY_UBOUND = 65792;		//64k + 256 = upper limit which can be applied to the allocation of the results array when calling walkDescribedPushes
	/**
	 * use thread's sp, arg0ea, pc, literal and entryLocalStorage.
	 */
	public static StackWalkResult walkStackFrames(WalkState walkState)
	{
		return getImpl().walkStackFrames(walkState);
	}
	
	/**
	 *  use user provided sp, arg0ea, pc, literal and entryLocalStorage
	 */
	public static StackWalkResult walkStackFrames(WalkState walkState, UDATAPointer sp, UDATAPointer arg0ea, U8Pointer pc, J9MethodPointer literals, J9VMEntryLocalStoragePointer entryLocalStorage) {
		return getImpl().walkStackFrames(walkState, sp, arg0ea, pc, literals, entryLocalStorage);
	}
	
	/* Callback from JIT stack walker */
	static FrameCallbackResult walkFrame(WalkState walkState) throws CorruptDataException
	{
		return getImpl().walkFrame(walkState);
	}

	/* Walk locals and pendings of a bytecoded frame (also used to walk OSR frames) */
	public static void walkBytecodeFrameSlots(WalkState walkState, J9MethodPointer method, UDATA offsetPC, UDATAPointer pendingBase, UDATA pendingStackHeight, UDATAPointer localBase, UDATA numberOfLocals)
			throws CorruptDataException
	{
		getImpl().walkBytecodeFrameSlots(walkState, method, offsetPC, pendingBase, pendingStackHeight, localBase, numberOfLocals);
	}

	private static IStackWalker impl;
	
	private static final AlgorithmPicker<IStackWalker> picker = new AlgorithmPicker<IStackWalker>(AlgorithmVersion.STACK_WALKER_VERSION){

		@Override
		protected Iterable<? extends IStackWalker> allAlgorithms()
		{
			List<IStackWalker> toReturn = new LinkedList<IStackWalker>();
			toReturn.add(new StackWalker_29_V0());
			return toReturn;
		}};
	
	private static IStackWalker getImpl()
	{
		if (impl == null) {
			impl = picker.pickAlgorithm();
		}
		return impl;
	}

	private static interface IStackWalker extends IAlgorithm
	{
		public StackWalkResult walkStackFrames(WalkState walkState);
		
		public void walkBytecodeFrameSlots(WalkState walkState,
				J9MethodPointer method, UDATA offsetPC,
				UDATAPointer pendingBase, UDATA pendingStackHeight,
				UDATAPointer localBase, UDATA numberOfLocals) throws CorruptDataException;

		public StackWalkResult walkStackFrames(WalkState walkState, UDATAPointer sp, UDATAPointer arg0ea, U8Pointer pc, J9MethodPointer literals, J9VMEntryLocalStoragePointer entryLocalStorage);

		FrameCallbackResult walkFrame(WalkState walkState) throws CorruptDataException;
	}
	
	
	private static class StackWalker_29_V0 extends BaseAlgorithm implements IStackWalker
	{
	
		protected StackWalker_29_V0()
		{
			super(90, 0);
		}

		public StackWalkResult walkStackFrames(WalkState walkState)
		{
			try {
				return walkStackFrames(walkState, 
						walkState.walkThread.sp(), 
						walkState.walkThread.arg0EA(), 
						walkState.walkThread.pc(), 
						walkState.walkThread.literals(),
						walkState.walkThread.entryLocalStorage());
			} catch (CorruptDataException ex) {
				raiseCorruptDataEvent("CDE thrown extracting initial stack walk state. walkThread = " + walkState.walkThread.getHexAddress(), ex, true);
				return StackWalkResult.STACK_CORRUPT;
			}
		}
		
		public StackWalkResult walkStackFrames(WalkState walkState, UDATAPointer sp, UDATAPointer arg0ea,  U8Pointer pc, J9MethodPointer literals, J9VMEntryLocalStoragePointer entryLocalStorage) 
		{
			UDATAPointer nextA0 = null;
			U8Pointer nextPC = null;
			J9MethodPointer nextLiterals;
			
			StackWalkResult rc = null;
			try {
				rc = walkState.walkThread.privateFlags().anyBitsIn(J9_PRIVATE_FLAGS_STACK_CORRUPT) ? StackWalkResult.STACK_CORRUPT : StackWalkResult.NONE;
			} catch (CorruptDataException e) {
				raiseCorruptDataEvent("CDE thrown extracting walkThread privateFlags. walkThread = " + walkState.walkThread.getHexAddress(), e, true);
				return StackWalkResult.STACK_CORRUPT;
			}
	
			/* Java-implementation specific variables start */
			boolean endOfStackReached = false;
			boolean keepWalking = true;
			boolean startAtJITFrame = false;
			
			resetOSlotsCorruptionThreshold();
			
			/* End of Java-implementation-specific variables */
	
			if ((walkState.flags & J9_STACKWALK_SAVE_STACKED_REGISTERS) != 0) {
				throw new UnsupportedOperationException("J9_STACKWALK_SAVE_STACKED_REGISTERS not supported by offline walker");
			}
	
			try {
				walkState.framesWalked = 0;
				walkState.previousFrameFlags = new UDATA(0);
				walkState.searchFrameFound = false;
				walkState.arg0EA = arg0ea;				
				walkState.pc = pc;
				walkState.pcAddress = walkState.walkThread.pcEA();
				walkState.walkSP = sp;
				walkState.literals = literals;
				walkState.argCount = new UDATA(0);
				walkState.frameFlags = new UDATA(0);
	
				if (J9BuildFlags.interp_nativeSupport) {
					walkState.jitInfo = J9JITExceptionTablePointer.NULL;
					walkState.inlineDepth = 0;
					walkState.inlinerMap = null;
					walkState.walkedEntryLocalStorage = entryLocalStorage;					
					//TODO this is &i2jState in the C code. Find out why it's different
					walkState.i2jState = walkState.walkedEntryLocalStorage.notNull() ? walkState.walkedEntryLocalStorage
							.i2jState()
							: null;
					walkState.j2iFrame = walkState.walkThread.j2iFrame();
	
					if (J9BuildFlags.jit_fullSpeedDebug) {
						walkState.decompilationStack = walkState.walkThread
								.decompilationStack();
						walkState.decompilationRecord = J9JITDecompilationInfoPointer.NULL;
						walkState.resolveFrameFlags = new UDATA(0);
					}
				}
	
			} catch (CorruptDataException ex) {
				raiseCorruptDataEvent("CDE thrown extracting initial stack walk state. walkThread = " + walkState.walkThread.getHexAddress(), ex, true);
				return StackWalkResult.STACK_CORRUPT;
			}

			/* fix up walkState values if we're starting in an inflight frame */
			try {
				IOSThread nativeThread = J9ThreadHelper.getOSThread(walkState.walkThread.osThread());
				
				if (nativeThread != null) {
					/* TODO: the jit map code subtracts 1 from this value because it expects the trap handlers to have added one
					 * to move the instruction to the target of the return from a child procedure. Do we need to alter it here on z/OS?
					 */
					/* does the current eip correspond to a jit exception table? */
					long eip = nativeThread.getInstructionPointer();
					
					J9JITExceptionTablePointer table = jitGetExceptionTableFromPC(walkState.walkThread, U8Pointer.cast(eip));

					if (!table.isNull()) {
						/* there's an exception table associated with this address so we're in jitted code. Let's try fixing it up */
						UDATAPointer javaSp = UDATAPointer.NULL;

						/* fetch the Java stack for the platform directly from the register file */
						String javaSPName = "";
						if (J9ConfigFlags.arch_power) {
							/* AIX shows as POWER not PPC */
							/* gpr14 */
							javaSPName = "gpr14";
						} else if (J9ConfigFlags.arch_s390) {
							/* r5 */
							javaSPName = "r5";
						} else if (J9ConfigFlags.arch_x86) {
							if (J9BuildFlags.env_data64) {
								/* rsp */
								javaSPName = "rsp";
							} else {
								/* esp */
								javaSPName = "esp";
							}
						} else {
							throw new IllegalArgumentException("Unsupported platform");
						}

						for (IRegister reg : nativeThread.getRegisters()) {
							if (reg.getName().equalsIgnoreCase(javaSPName)) {
								javaSp = UDATAPointer.cast(reg.getValue().longValue());
								break;
							}
						}

						if (!javaSp.isNull()) {
							/* if using startAtJitFrame */
							walkState.walkSP = javaSp;
							walkState.sp = walkState.walkSP;
							walkState.pc = U8Pointer.cast(eip);
							walkState.flags = walkState.flags | J9_STACKWALK_START_AT_JIT_FRAME | J9_STACKWALK_RECORD_BYTECODE_PC_OFFSET;
						}
					}
				}
			} catch (Exception ex) {
				//CMVC 176669 expand error handling to catch failures in underlying core readers
				CorruptDataException cde = null;
				if(ex instanceof CorruptDataException) {
					cde = (CorruptDataException) ex;
				} else {
					cde = new CorruptDataException(ex);
				}
				raiseCorruptDataEvent("CDE thrown while checking or fixing up JIT frames, attempting to walk without fixup", cde, true);
				return StackWalkResult.STACK_CORRUPT;
			}

			swPrintf(walkState, 1,
					"*** BEGIN STACK WALK, flags = {0} walkThread = {1} ***",
					String.format("%08X", walkState.flags), walkState.walkThread.getHexAddress());

			if ((walkState.flags & J9_STACKWALK_START_AT_JIT_FRAME) != 0)
				swPrintf(walkState, 2, "\tSTART_AT_JIT_FRAME");
			if ((walkState.flags & J9_STACKWALK_COUNT_SPECIFIED) != 0)
				swPrintf(walkState, 2, "\tCOUNT_SPECIFIED");
			if ((walkState.flags & J9_STACKWALK_INCLUDE_ARRAYLET_LEAVES) != 0)
				swPrintf(walkState, 2, "\tINCLUDE_ARRAYLET_LEAVES");
			if ((walkState.flags & J9_STACKWALK_INCLUDE_NATIVES) != 0)
				swPrintf(walkState, 2, "\tINCLUDE_NATIVES");
			if ((walkState.flags & J9_STACKWALK_INCLUDE_CALL_IN_FRAMES) != 0)
				swPrintf(walkState, 2, "\tINCLUDE_CALL_IN_FRAMES");
			if ((walkState.flags & J9_STACKWALK_ITERATE_FRAMES) != 0)
				swPrintf(walkState, 2, "\tITERATE_FRAMES");
			if ((walkState.flags & J9_STACKWALK_ITERATE_HIDDEN_JIT_FRAMES) != 0)
				swPrintf(walkState, 2, "\tITERATE_HIDDEN_JIT_FRAMES");
			if ((walkState.flags & J9_STACKWALK_ITERATE_O_SLOTS) != 0)
				swPrintf(walkState, 2, "\tITERATE_O_SLOTS");
			if ((walkState.flags & J9_STACKWALK_ITERATE_METHOD_CLASS_SLOTS) != 0)
				swPrintf(walkState, 2, "\tITERATE_METHOD_CLASS_SLOTS");
			if ((walkState.flags & J9_STACKWALK_MAINTAIN_REGISTER_MAP) != 0)
				swPrintf(walkState, 2, "\tMAINTAIN_REGISTER_MAP");
			if ((walkState.flags & J9_STACKWALK_SKIP_INLINES) != 0)
				swPrintf(walkState, 2, "\tSKIP_INLINES");
			if ((walkState.flags & J9_STACKWALK_VISIBLE_ONLY) != 0)
				swPrintf(walkState, 2, "\tVISIBLE_ONLY");
			if ((walkState.flags & J9_STACKWALK_HIDE_EXCEPTION_FRAMES) != 0)
				swPrintf(walkState, 2, "\tHIDE_EXCEPTION_FRAMES");
			if ((walkState.flags & J9_STACKWALK_RECORD_BYTECODE_PC_OFFSET) != 0)
				swPrintf(walkState, 2, "\tRECORD_BYTECODE_PC_OFFSET");
	
			//TODO why is the throwing exception stuff left out?
			swPrintf(
					walkState,
					2,
					"Initial values: walkSP = "
					+ walkState.walkSP.getHexAddress()
					+ ", PC = "
					+ walkState.pc.getHexAddress()
					+ ", literals = "
					+ walkState.literals.getHexAddress()
					+ ", A0 = "
					+ walkState.arg0EA.getHexAddress()
					+ ", j2iFrame = "
					+ (J9BuildFlags.interp_nativeSupport ? walkState.j2iFrame.getHexAddress() : "0")
					+ ", ELS = "
					+ (J9BuildFlags.interp_nativeSupport ? walkState.walkedEntryLocalStorage.getHexAddress() : "0")
					+ ", decomp = "
					+ (J9BuildFlags.jit_fullSpeedDebug ? walkState.decompilationStack.getHexAddress() : "0")
			);

	
			if (((walkState.flags & J9_STACKWALK_COUNT_SPECIFIED) != 0)
					&& (walkState.maxFrames == 0)) {
				keepWalking = false;
				// goto terminationPoint;
			}

			//TODO missing initializeObjectSlotBitVector
	
			if (J9BuildFlags.interp_nativeSupport) {
				if (walkState.flags == J9_STACKWALK_ITERATE_O_SLOTS) {
					walkState.flags |= J9_STACKWALK_SKIP_INLINES;
				}
	
				if ((walkState.flags & J9_STACKWALK_ITERATE_O_SLOTS) != 0) {
					walkState.flags |= J9_STACKWALK_MAINTAIN_REGISTER_MAP;
				}
	
				if ((walkState.flags & J9_STACKWALK_START_AT_JIT_FRAME) != 0) {
					// goto jitFrame
					startAtJITFrame = true;
				}
			}
	
			// Check callbacks have been provided if required
			if ((walkState.flags & (J9_STACKWALK_ITERATE_FRAMES | J9_STACKWALK_ITERATE_O_SLOTS)) != 0
					&& walkState.callBacks == null) {
				throw new NullPointerException(
						"User requested iterate frames or slots and didn't provide a callback. See WalkState.callBacks.");
			}
	
			try {
				WALKING_LOOP: while (keepWalking) {
					J9SFStackFramePointer fixedStackFrame;
	
					swPrintf(walkState, 200, "Top of WALKING_LOOP");
	
					if (!startAtJITFrame) {
						walkState.constantPool = J9ConstantPoolPointer.NULL;
						walkState.unwindSP = UDATAPointer.NULL;
						walkState.method = J9MethodPointer.NULL;
						walkState.sp = walkState.walkSP.sub(walkState.argCount);
						walkState.outgoingArgCount = walkState.argCount;
						walkState.bytecodePCOffset = U8Pointer.cast(-1);
	
						if (walkState.pc.getAddress() == J9SF_FRAME_TYPE_END_OF_STACK) {
							// goto endOfStack;
							swPrintf(walkState, 200, "PC Switch: End of Stack");
							endOfStackReached = true;
							break WALKING_LOOP;
						} else if (walkState.pc.getAddress() == J9SF_FRAME_TYPE_GENERIC_SPECIAL) {
							swPrintf(walkState, 200, "PC Switch: Generic Special");
							walkGenericSpecialFrame(walkState);
						} else if (walkState.pc.getAddress() == J9SF_FRAME_TYPE_METHOD
								|| walkState.pc.getAddress() == J9SF_FRAME_TYPE_NATIVE_METHOD
								|| walkState.pc.getAddress() == J9SF_FRAME_TYPE_JNI_NATIVE_METHOD) {
							swPrintf(walkState, 200, "PC Switch: walkMethodFrame");
							walkMethodFrame(walkState);
						} else if (walkState.pc.getAddress() == J9SF_FRAME_TYPE_METHODTYPE) {
							swPrintf(walkState, 200, "PC Switch: MethodType");
								walkMethodTypeFrame(walkState);
						} else if (J9BuildFlags.interp_nativeSupport
								&& walkState.pc.getAddress() == J9SF_FRAME_TYPE_JIT_RESOLVE) {
							swPrintf(walkState, 200, "PC Switch: JIT Resolve");
							walkJITResolveFrame(walkState);
						} else if (J9BuildFlags.interp_nativeSupport
								&& walkState.pc.getAddress() == J9SF_FRAME_TYPE_JIT_JNI_CALLOUT) {
							swPrintf(walkState, 200, "PC Switch: JIT_JNI_CALLOUT");
							walkJITJNICalloutFrame(walkState);
						} else {
							swPrintf(walkState, 200, "PC Switch: default");
							if ((new UDATA(walkState.pc.getAddress())).lte(new U8(
									J9SF_MAX_SPECIAL_FRAME_TYPE))) {
								printFrameType(walkState, "Unknown");
								swPrintf(walkState, 0,
										"Aborting walk due to unknown special frame type");
								break WALKING_LOOP;
							}
	
							if ((walkState.pc.eq(walkState.walkThread.javaVM()
									.callInReturnPC()))
									|| (walkState.pc
											.eq(walkState.walkThread.javaVM()
													.callInReturnPC().addOffset(3)))) {
								swPrintf(walkState, 200,
										"PC Switch: default: walkJNICallInFrame");
								walkJNICallInFrame(walkState);
							} else {
								swPrintf(walkState, 200,
										"PC Switch: default: walkBytecodeFrame");
								walkBytecodeFrame(walkState);
							}
						}
	
						/*
						 * Fetch the PC value before calling the frame walker since
						 * the debugger modifies PC values when breakpointing
						 * methods
						 */
	
						{
							UDATA nextPCUDATA = new UDATA(walkState.bp.getAddress());
							nextPCUDATA = nextPCUDATA.sub(J9SFStackFrame.SIZEOF);
							nextPCUDATA = nextPCUDATA.add(UDATA.SIZEOF);
							J9SFStackFramePointer tmpFrame = J9SFStackFramePointer
									.cast(nextPCUDATA);
							nextPC = tmpFrame.savedPC();
						}
	
						/* Walk the frame */
	
						if (walkFrame(walkState) != FrameCallbackResult.KEEP_ITERATING) {
							break WALKING_LOOP;
						}
						walkState.previousFrameFlags = walkState.frameFlags;
						walkState.resolveFrameFlags = new UDATA(0);
	
					} // End of (! startAtJITFrame)
	
					/*
					 * Call the JIT walker if the current frame is a transition from
					 * the JIT to the interpreter
					 */
					if (J9BuildFlags.interp_nativeSupport) {
						if (startAtJITFrame || (walkState.frameFlags.longValue() & J9_STACK_FLAGS_JIT_TRANSITION_TO_INTERPRETER_MASK) != 0) {
							// jitFrame:
							startAtJITFrame = false;
							if (jitWalkStackFrames(walkState) != FrameCallbackResult.KEEP_ITERATING) {
								break WALKING_LOOP;
							}
							walkState.decompilationRecord = J9JITDecompilationInfoPointer.NULL;
							continue WALKING_LOOP;
						}
					}
	
					/*
					 * Fetch the remaining frame values after calling the frame
					 * walker since the stack could grow during the iterator,
					 * changing all stack pointer values
					 */
					fixedStackFrame = J9SFStackFramePointer.cast(walkState.bp.subOffset(J9SFStackFrame.SIZEOF).addOffset(UDATA.SIZEOF));
					
					nextLiterals = fixedStackFrame.savedCP();
					// See definition for UNTAG2 and UNTAGGED_A0
					nextA0 = UDATAPointer.cast(fixedStackFrame.savedA0()).untag();
	
					/* Move to next frame */
					walkState.pcAddress = fixedStackFrame.savedPCEA();
					walkState.walkSP = walkState.arg0EA.add(1);
					walkState.pc = nextPC;
					walkState.literals = nextLiterals;
					walkState.arg0EA = nextA0;
				}
			} catch (CorruptDataException ex) {
				raiseCorruptDataEvent("CorruptDataException thrown walking stack. walkThread = "+ walkState.walkThread.getHexAddress(), ex, false);
				return StackWalkResult.STACK_CORRUPT;
			}
	
			// endOfStack:
			if (endOfStackReached) {
				swPrintf(walkState, 2, "<end of stack>");
			}
	
			//If we disabled o-slots walking because it kept throwing CorruptDataException, reset the rc here
			if (oSlotsCorruptionThresholdReached() && rc == StackWalkResult.NONE) {
				rc = StackWalkResult.STACK_CORRUPT;
			}
			
			// terminationPoint:
	
			swPrintf(walkState, 1, "*** END STACK WALK (rc = {0}) ***", rc);
	
			return rc;
		}
	
		private void walkMethodFrame(WalkState walkState)
				throws CorruptDataException
		{
			J9SFMethodFramePointer methodFrame = J9SFMethodFramePointer
					.cast(walkState.walkSP
							.addOffset(UDATA.cast(walkState.literals)));
	
			walkState.bp = UDATAPointer.cast(methodFrame.savedA0EA());
			walkState.frameFlags = methodFrame.specialFrameFlags();
			walkState.method = methodFrame.method();
			walkState.unwindSP = UDATAPointer.cast(methodFrame);
	
			try {
				if (walkState.pc.getAddress() == J9SF_FRAME_TYPE_METHOD) {
					printFrameType(
							walkState,
							walkState.frameFlags
									.anyBitsIn(J9_STACK_FLAGS_JIT_NATIVE_TRANSITION) ? "JIT generic transition"
									: "Generic method");
				} else if (walkState.pc.getAddress() == J9SF_FRAME_TYPE_NATIVE_METHOD) {
					printFrameType(
							walkState,
							walkState.frameFlags
									.anyBitsIn(J9_STACK_FLAGS_JIT_NATIVE_TRANSITION) ? "JIT INL transition"
									: "INL native method");
				} else if (walkState.pc.getAddress() == J9SF_FRAME_TYPE_JNI_NATIVE_METHOD) {
					printFrameType(
							walkState,
							walkState.frameFlags
									.anyBitsIn(J9_STACK_FLAGS_JIT_NATIVE_TRANSITION) ? "JIT JNI transition"
									: "JNI native method");
				} else {
					printFrameType(
							walkState,
							walkState.frameFlags
									.anyBitsIn(J9_STACK_FLAGS_JIT_NATIVE_TRANSITION) ? "JIT unknown transition"
									: "Unknown method");
				}
		
				if (((walkState.flags & J9_STACKWALK_ITERATE_O_SLOTS) != 0)
						&& walkState.literals.notNull()) {
						if (walkState.frameFlags.anyBitsIn(J9_SSF_JNI_REFS_REDIRECTED)) {
							walkPushedJNIRefs(walkState);
						} else {
							walkObjectPushes(walkState);
						}
				}
			} catch (CorruptDataException ex) {
				handleOSlotsCorruption(walkState, "StackWalker_29_V0", "walkMethodFrame", ex);
			}
	
			if (walkState.method.notNull()) {
				J9ROMMethodPointer romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(walkState.method);
	
				walkState.constantPool = UNTAGGED_METHOD_CP(walkState.method);
				walkState.argCount = new UDATA(romMethod.argCount());
					
				if ((walkState.flags & J9_STACKWALK_ITERATE_O_SLOTS) != 0) {
					try {
						WALK_METHOD_CLASS(walkState);
	
						if (walkState.argCount.longValue() != 0) {
							/* Max size as argCount always <= 255 */
							int[] result = new int[8];
	
							swPrintf(walkState, 4, "\tUsing signature mapper");
							j9localmap_ArgBitsForPC0(romMethod, result);
	
							swPrintf(walkState, 4,
									"\tArguments starting at {0} for {1} slots",
									walkState.arg0EA.getHexAddress(),
									walkState.argCount);
							walkState.slotType = (int)J9_STACKWALK_SLOT_TYPE_METHOD_LOCAL;
							walkState.slotIndex = 0;
							if (walkState.frameFlags
									.anyBitsIn(J9_SSF_JNI_REFS_REDIRECTED)) {
								walkIndirectDescribedPushes(walkState,
										walkState.arg0EA, walkState.argCount
												.intValue(), result);
							} else {
								walkDescribedPushes(walkState, walkState.arg0EA,
										walkState.argCount.intValue(), result, walkState.argCount.intValue());
							}
						}
					} catch (CorruptDataException ex) {
						handleOSlotsCorruption(walkState, "StackWalker_29_V0", "walkMethodFrame", ex);
					}
				}
			} else {
				if (((walkState.flags & J9_STACKWALK_ITERATE_O_SLOTS) != 0)
						&& !walkState.arg0EA.eq(walkState.bp)) {
					try {
						walkJNIRefs(walkState, walkState.bp.add(1), walkState.arg0EA.sub(walkState.bp).intValue());
					} catch (CorruptDataException ex) {
						handleOSlotsCorruption(walkState, "StackWalker_29_V0", "walkMethodFrame", ex);
					} catch (InvalidDataTypeException e) {
						//if there is corruption such that the UDATA is larger than the corresponding signed Java int then we need to catch it
						CorruptDataException ex = new CorruptDataException("Corruption walking JNI references", e);
						handleOSlotsCorruption(walkState, "StackWalker_29_V0", "walkMethodFrame", ex);
					}
				}
				walkState.constantPool = null;
				walkState.argCount = new UDATA(0);
			}
	
		}
	
		private void walkMethodTypeFrame(WalkState walkState)
				throws CorruptDataException
		{
			J9SFMethodTypeFramePointer methodTypeFrame = J9SFMethodTypeFramePointer.cast(walkState.walkSP.addOffset(UDATA.cast(walkState.literals)));
			walkState.bp = UDATAPointer.cast(methodTypeFrame.savedA0EA());
			walkState.frameFlags = methodTypeFrame.specialFrameFlags();
			printFrameType(walkState, "MethodType");
			walkState.method = J9MethodPointer.NULL;
			walkState.unwindSP = UDATAPointer.cast(methodTypeFrame);
			if ((walkState.flags & J9_STACKWALK_ITERATE_O_SLOTS) != 0) {
				int[] descriptionSlots = new int[8];
				U32Pointer descriptorCursor = U32Pointer.cast(walkState.bp.add(1));
				for (int i = 0; i < descriptionSlots.length; ++i) {
					descriptionSlots[i] = descriptorCursor.at(0).intValue();
					descriptorCursor.add(1);
				}
				if (walkState.literals.notNull()) {
					walkObjectPushes(walkState);
				}
				WALK_O_SLOT(walkState, PointerPointer.cast(methodTypeFrame.methodType()));
				walkState.argCount = methodTypeFrame.argStackSlots().add(1);
				walkState.slotType = (int)J9_STACKWALK_SLOT_TYPE_METHOD_LOCAL;
				walkState.slotIndex = 0;
				walkDescribedPushes(walkState, walkState.arg0EA, walkState.argCount.intValue(), descriptionSlots, walkState.argCount.intValue());
			}
			
		}
		
		private void walkJNIRefs(WalkState walkState, UDATAPointer currentRef,
				int refCount) throws CorruptDataException
		{
			swPrintf(walkState, 4,
					"\tJNI local ref pushes starting at {0} for {1} slots",
					currentRef.getHexAddress(), refCount);
	
			walkState.slotType = (int)J9_STACKWALK_SLOT_TYPE_JNI_LOCAL;
			walkState.slotIndex = 0;
	
			do {
				if (currentRef.at(0).anyBitsIn(1)) {
					// J9Object **realRef = (J9Object **) (*currentRef & ~1);
					PointerPointer realRef = PointerPointer.cast(currentRef.at(0)
							.bitAnd(new UDATA(1).bitNot()));
	
					/*
					 * There's no need to mark indirect slots as object since they
					 * will never point to valid objects (so will not be whacked)
					 */
					WALK_NAMED_INDIRECT_O_SLOT(walkState, realRef, VoidPointer.cast(currentRef),"Indir-Lcl-JNI-Ref");
				} else {
					WALK_NAMED_O_SLOT(walkState, PointerPointer.cast(currentRef), "Lcl-JNI-Ref");
				}
				currentRef = currentRef.add(1);
				walkState.slotIndex++;
			} while (--refCount > 0);
		}
	
		private void walkIndirectDescribedPushes(WalkState walkState,
				UDATAPointer highestIndirectSlot, int slotCount,
				int[] descriptionSlots) throws CorruptDataException
		{
			int descriptionBitsRemaining = 0;
			int description = 0;
			int descriptionSlotPointer = 0;
	
			while (slotCount > 0) {
				if (descriptionBitsRemaining == 0) {
					description = descriptionSlots[descriptionSlotPointer++];
					descriptionBitsRemaining = 32;
				}
				if ((description & 1) != 0) {
					/*
					 * There's no need to mark indirect slots as object since they
					 * will never point to valid objects (so will not be whacked)
					 */
					WALK_INDIRECT_O_SLOT(walkState, PointerPointer
							.cast(highestIndirectSlot.at(0).bitAnd(
									new UDATA(1).bitNot())), VoidPointer
							.cast(highestIndirectSlot));
				} else {
					/* CMVC 197745 - After fixing this defect, the stack grower no longer
					 * redirects I slots.
					 */
					if (AlgorithmVersion.getVersionOf("VM_STACK_GROW_VERSION").getAlgorithmVersion() >= 1) {
						WALK_I_SLOT(walkState, PointerPointer.cast(highestIndirectSlot));
					} else {
						WALK_INDIRECT_I_SLOT(walkState, PointerPointer
								.cast(highestIndirectSlot.at(0).bitAnd(
										new UDATA(1).bitNot())), VoidPointer
								.cast(highestIndirectSlot));
					}
				}
	
				description >>= 1;
				--descriptionBitsRemaining;
				highestIndirectSlot = highestIndirectSlot.sub(1);
				--slotCount;
				++(walkState.slotIndex);
			}
		}
	
		private void walkObjectPushes(WalkState walkState)
				throws CorruptDataException
		{
			UDATA byteCount = UDATA.cast(walkState.literals);
			PointerPointer currentSlot = PointerPointer.cast(walkState.walkSP);
	
			swPrintf(walkState, 4, "\tObject pushes starting at {0} for {1} slots",
					currentSlot.getHexAddress(), byteCount.longValue()
							/ UDATA.SIZEOF);
	
			walkState.slotType = (int)J9_STACKWALK_SLOT_TYPE_INTERNAL;
			walkState.slotIndex = 0;
	
			while (!byteCount.eq(0)) {
				WALK_NAMED_O_SLOT(walkState, currentSlot, "Push");
				currentSlot = currentSlot.add(1);
				byteCount = byteCount.sub(UDATA.SIZEOF);
				++(walkState.slotIndex);
			}
	
		}
	
		private void walkPushedJNIRefs(WalkState walkState)
				throws CorruptDataException
		{
			UDATA refCount = walkState.frameFlags
					.bitAnd(J9_SSF_JNI_PUSHED_REF_COUNT_MASK);
			// TODO think about unsigned 64 bit divide
			UDATA pushCount = new UDATA(walkState.literals.getAddress()
					/ UDATA.SIZEOF).sub(refCount);
	
			if (!pushCount.eq(0)) {
				walkState.literals = J9MethodPointer.cast(pushCount.longValue()
						* UDATA.SIZEOF);
				walkObjectPushes(walkState);
			}
	
			if (!refCount.eq(0)) {
				walkJNIRefs(walkState, walkState.walkSP.add(pushCount), refCount
						.intValue());
			}
		}
	
		public FrameCallbackResult walkFrame(WalkState walkState)
				throws CorruptDataException
		{
			if ((walkState.flags & J9_STACKWALK_VISIBLE_ONLY) != 0) {
				if ((UDATA.cast(walkState.pc).eq(J9SF_FRAME_TYPE_NATIVE_METHOD) || 
						UDATA.cast(walkState.pc).eq(J9SF_FRAME_TYPE_JNI_NATIVE_METHOD))
						&& ((walkState.flags & J9_STACKWALK_INCLUDE_NATIVES) == 0)) {
					return FrameCallbackResult.KEEP_ITERATING;
				}
	
				if ((!J9BuildFlags.interp_nativeSupport)
						|| walkState.jitInfo.isNull()) {
	
					if (walkState.bp.at(0).anyBitsIn(J9SF_A0_INVISIBLE_TAG)) {
						if ((walkState.flags & J9_STACKWALK_INCLUDE_CALL_IN_FRAMES) == 0
								|| !walkState.pc.eq(walkState.walkThread.javaVM()
										.callInReturnPC())) {
							return FrameCallbackResult.KEEP_ITERATING;
						}
					}
	
				}
				
				if (walkState.skipCount > 0) {
					--walkState.skipCount;
					return FrameCallbackResult.KEEP_ITERATING;
				}
	
				if ((walkState.flags & J9_STACKWALK_HIDE_EXCEPTION_FRAMES) != 0) {
					J9ROMMethodPointer romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(walkState.method);
	
					if (!romMethod.modifiers().anyBitsIn(J9AccStatic)) {
						if (J9UTF8Helper.stringValue(romMethod.nameAndSignature().name())
								.charAt(0) == '<') {
							if (walkState.arg0EA.at(0).eq(
									UDATA.cast(walkState.restartException))) {
								return FrameCallbackResult.KEEP_ITERATING;
							}
						}
						walkState.flags &= ~J9_STACKWALK_HIDE_EXCEPTION_FRAMES;
					}
				}
			}
	
			// Caching code not reproduced in offline walker
	
			++walkState.framesWalked;
			// Named block to imitate goto behaviour
			walkIt: do {
				if ((walkState.flags & J9_STACKWALK_COUNT_SPECIFIED) != 0) {
					if (walkState.framesWalked == walkState.maxFrames) {
						break walkIt;
					}
				}
	
				if ((walkState.flags & J9_STACKWALK_ITERATE_FRAMES) != 0) {
					break walkIt;
				}
	
				return FrameCallbackResult.KEEP_ITERATING;
			} while (false);
	
			// walkIt:
			if ((walkState.flags & J9_STACKWALK_ITERATE_FRAMES) != 0) {
				FrameCallbackResult rc = walkState.callBacks.frameWalkFunction(
						walkState.walkThread, walkState);
	
				if ((walkState.flags & J9_STACKWALK_COUNT_SPECIFIED) != 0) {
					if (walkState.framesWalked == walkState.maxFrames) {
						rc = FrameCallbackResult.STOP_ITERATING;
					}
				}
	
				return rc;
			}
	
			return FrameCallbackResult.STOP_ITERATING;
		}

		public void walkBytecodeFrameSlots(WalkState walkState, J9MethodPointer method, UDATA offsetPC, UDATAPointer pendingBase, UDATA pendingStackHeight, UDATAPointer localBase, UDATA numberOfLocals)
				throws CorruptDataException
		{
			J9ROMClassPointer romClass = walkState.constantPool.ramClass().romClass();
			J9ROMMethodPointer romMethod = getOriginalROMMethod(walkState.method);
			UDATA numberOfMappedLocals = numberOfLocals;
			UDATAPointer bp = localBase.sub(numberOfLocals);

			/*
			 * U_32* result in native is modelled as int array in Java. Since
			 * all arrays are heap allocated we don't reproduce the smallResult
			 * / result logic here.
			 */
			int[] result;
	
			swPrintf(walkState, 3, "\tBytecode index = {0}", offsetPC.longValue());

			if (romMethod.modifiers().anyBitsIn(J9AccSynchronized)) {
				swPrintf(walkState, 4, "\tSync object for synchronized method");
				walkState.slotType = (int)J9_STACKWALK_SLOT_TYPE_INTERNAL;
				walkState.slotIndex = -1;
				WALK_NAMED_O_SLOT(walkState, PointerPointer.cast(bp.add(1)), "Sync O-Slot");
				numberOfMappedLocals = numberOfMappedLocals.sub(1);
			} else if (J9ROMMethodHelper.isNonEmptyObjectConstructor(romMethod)) {
				/* Non-empty java.lang.Object.<init> has one hidden temp to hold a copy of the receiver */
				swPrintf(walkState, 4, "\tReceiver object for java.lang.Object.<init>\n");
				walkState.slotType = (int)J9_STACKWALK_SLOT_TYPE_INTERNAL;
				walkState.slotIndex = -1;
				WALK_NAMED_O_SLOT(walkState, PointerPointer.cast(bp.add(1)), "Receiver O-Slot");
				numberOfMappedLocals = numberOfMappedLocals.sub(1);
			}
	
			swPrintf(walkState, 200, "numberOfMappedLocals={0}, pendingStackHeight={1}",
					numberOfMappedLocals.getHexValue(), pendingStackHeight.getHexValue());
	
			int resultArraySize = 0;
			if (numberOfMappedLocals.gt(32) || pendingStackHeight.gt(32)) {
				UDATA maxCount = numberOfMappedLocals.gt(pendingStackHeight) ? numberOfMappedLocals
						: pendingStackHeight;
				resultArraySize = maxCount.add(31).rightShift(5).intValue();
				//CMVC 176560 add an upper bounds check on the array initialisation
				if (( resultArraySize <= 0) || (resultArraySize > LOCALS_ARRAY_UBOUND)) {
					throw new CorruptDataException("Corrupt frame detected : " + walkState.pc.longValue() );
				}
			} else {
				resultArraySize = 1;
			}
			swPrintf(walkState, 200, "Result array size = {0}", resultArraySize);
			result = new int[resultArraySize];
	
			if (numberOfMappedLocals.intValue() != 0) {
				getLocalsMap(walkState, romClass, romMethod, U8Pointer.cast(offsetPC), result, numberOfMappedLocals);
				swPrintf(walkState, 4, "\tLocals starting at {0} for {1} slots",
						localBase.getHexAddress(), numberOfMappedLocals.getHexValue());
				walkState.slotType = (int)J9_STACKWALK_SLOT_TYPE_METHOD_LOCAL;
				walkState.slotIndex = 0;
				walkDescribedPushes(walkState, localBase, numberOfMappedLocals.intValue(), result, romMethod.argCount().intValue());
			}
	
			if (pendingStackHeight.intValue() != 0) {
				getStackMap(walkState, romClass, romMethod, offsetPC, pendingStackHeight, result);
				swPrintf(walkState, 4,
						"\tPending stack starting at {0} for {1} slots",
						pendingBase.getHexAddress(), pendingStackHeight);
				walkState.slotType = (int)J9_STACKWALK_SLOT_TYPE_PENDING;
				walkState.slotIndex = 0;
				walkDescribedPushes(walkState, pendingBase, pendingStackHeight.intValue(), result, 0);
			}
		}

		private void walkBytecodeFrame(WalkState walkState)
				throws CorruptDataException
		{
			walkState.method = walkState.literals;
			if (walkState.method.isNull()) {
				walkState.constantPool = J9ConstantPoolPointer.NULL;
				walkState.bytecodePCOffset = U8Pointer.cast(-1);
				walkState.argCount = new UDATA(0);
				if (walkState.arg0EA.eq(walkState.j2iFrame)) {
					walkState.bp = walkState.arg0EA;
					walkState.unwindSP = walkState.bp.subOffset(J9SFJ2IFrame.SIZEOF)
							.addOffset(UDATA.SIZEOF);
					walkState.frameFlags = J9SFJ2IFramePointer.cast(walkState.unwindSP)
							.specialFrameFlags();
					//TODO do I need MARK_SLOT_AS_OBJECT here?
					printFrameType(walkState, "invokeExact J2I");
				} else {
					walkState.bp = UDATAPointer.NULL;
					walkState.unwindSP = UDATAPointer.NULL;
					walkState.frameFlags = new UDATA(0);
					// TODO can we avoid crashing?
					printFrameType(walkState, "BAD bytecode (expect crash)");					
				}
			} else {
				UDATA argTempCount;
				J9ROMMethodPointer romMethod;
				J9JavaVMPointer vm = walkState.walkThread.javaVM();
				
				walkState.constantPool = UNTAGGED_METHOD_CP(walkState.method);
				if ((walkState.pc != vm.impdep1PC()) && (walkState.pc != (vm.impdep1PC().addOffset(3)))) {
					walkState.bytecodePCOffset = walkState.pc.sub(walkState.method.bytecodes().getAddress());	
				} else {
					walkState.bytecodePCOffset = U8Pointer.cast(new UDATA(0));
				}

				romMethod = getOriginalROMMethod(walkState.method);
		
				walkState.argCount = new UDATA(J9_ARG_COUNT_FROM_ROM_METHOD(romMethod));
				argTempCount = new UDATA(J9_TEMP_COUNT_FROM_ROM_METHOD(romMethod))
						.add(walkState.argCount);
				walkState.bp = UDATAPointer.cast(walkState.arg0EA).sub(argTempCount);
		
				if (romMethod.modifiers().anyBitsIn(J9AccSynchronized)) {
					argTempCount = argTempCount.add(1);
					walkState.bp = walkState.bp.sub(1);
				} else if (J9ROMMethodHelper.isNonEmptyObjectConstructor(romMethod)) {
					argTempCount = argTempCount.add(1);
					walkState.bp = walkState.bp.sub(1);
				}
		
				if (J9BuildFlags.interp_nativeSupport
						&& walkState.bp.eq(walkState.j2iFrame)) {
					walkState.unwindSP = walkState.bp.subOffset(J9SFJ2IFrame.SIZEOF)
							.addOffset(UDATA.SIZEOF);
					walkState.frameFlags = J9SFJ2IFramePointer.cast(walkState.unwindSP)
							.specialFrameFlags();
					//TODO do I need MARK_SLOT_AS_OBJECT here?
				} else {
					walkState.unwindSP = walkState.bp.subOffset(J9SFStackFrame.SIZEOF)
							.addOffset(UDATA.SIZEOF);
					walkState.frameFlags = new UDATA(0);
				}
		
				try {
					printFrameType(walkState, walkState.frameFlags.intValue() != 0 ? "J2I"
							: "Bytecode");
					if ((walkState.flags & J9_STACKWALK_ITERATE_O_SLOTS) != 0) {
						WALK_METHOD_CLASS(walkState);
						walkBytecodeFrameSlots(walkState, walkState.method, new UDATA(walkState.bytecodePCOffset.longValue()),
								walkState.unwindSP.sub(1), new UDATA(walkState.unwindSP.sub(walkState.walkSP)),
								walkState.arg0EA, argTempCount);

					}
				} catch (CorruptDataException ex) {
					handleOSlotsCorruption(walkState, "StackWalker_29_V0", "walkBytecodeFrame", ex);
				}	
			}
		}
	
		private void getStackMap(WalkState walkState, J9ROMClassPointer romClass,
				J9ROMMethodPointer romMethod, UDATA offsetPC, UDATA pushCount,
				int[] result) throws CorruptDataException
		{
			int errorCode;
	
			errorCode = j9stackmap_StackBitsForPC(offsetPC, romClass, romMethod,
					result, pushCount.intValue());
			if (errorCode < 0) {
				throw new AddressedCorruptDataException(romMethod.getAddress(),
						"Stack map failed, result = " + errorCode);
			}
			return;
		}
	
		private void walkDescribedPushes(WalkState walkState,
				UDATAPointer highestSlot, int slotCount, int[] descriptionSlots, int argCount)
				throws CorruptDataException
		{
			int descriptionBitsRemaining = 0;
			int description = 0;
			int descriptionSlotPointer = 0;
	
			while (slotCount > 0) {
				if (descriptionBitsRemaining == 0) {
					description = descriptionSlots[descriptionSlotPointer++];
					descriptionBitsRemaining = 32;
				}
				{
					String indexedTag;
					if (walkState.slotType == J9_STACKWALK_SLOT_TYPE_METHOD_LOCAL) {
						indexedTag = String.format("%s-Slot: %s%d", ((description & 1) != 0) ? "O" : "I" , (walkState.slotIndex >= argCount) ? "t" : "a", walkState.slotIndex);
					} else {
						indexedTag = String.format("%s-Slot: p%d", ((description & 1) != 0) ? "O" : "I" ,walkState.slotIndex);
					}
					
					if ((description & 1) != 0) {
						WALK_NAMED_O_SLOT(walkState, PointerPointer.cast(highestSlot), indexedTag);
					} else {
						WALK_NAMED_I_SLOT(walkState, PointerPointer.cast(highestSlot), indexedTag);
					}
				}
	
				description >>= 1;
				--descriptionBitsRemaining;
				highestSlot = highestSlot.sub(1);
				--slotCount;
				++(walkState.slotIndex);
			}
		}
	
		private static void getLocalsMap(WalkState walkState,
				J9ROMClassPointer romClass, J9ROMMethodPointer romMethod,
				U8Pointer offsetPC, int[] result, UDATA argTempCount)
				throws CorruptDataException
		{
			int errorCode;
	
			/*
			 * Detect method entry vs simply executing at PC 0. If the bytecode
			 * frame is invisible (method monitor enter or stack growth) or the
			 * previous frame was the special frame indicating method entry
			 * (reporting method enter), then use the signature mapper instead of
			 * the local mapper. This keeps the receiver alive for use within method
			 * enter events, even if the receiver is never used (in which case the
			 * local mapper would mark it as int).
			 */
	
			if ((walkState.bp.at(0).anyBitsIn(J9SF_A0_INVISIBLE_TAG))
					|| (walkState.previousFrameFlags.anyBitsIn(J9_SSF_METHOD_ENTRY))) {
				if ((walkState.bp.at(0).anyBitsIn(J9SF_A0_INVISIBLE_TAG))) {
					swPrintf(
							walkState,
							4,
							"\tAt method entry (hidden bytecode frame = monitor enter/stack grow), using signature mapper");
				} else {
					swPrintf(
							walkState,
							4,
							"\tAt method entry (previous frame = report monitor enter), using signature mapper");
				}
	
				/*
				 * j9localmap_ArgBitsForPC0 only deals with args, so zero out the
				 * result array to make sure the temps are non-object
				 */
				j9localmap_ArgBitsForPC0(romMethod, result);
				return;
			}

			long canAccessLocalsBit = J9JavaVM.J9VM_DEBUG_ATTRIBUTE_LOCAL_VARIABLE_TABLE;
			if (AlgorithmVersion.getVersionOf("VM_CAN_ACCESS_LOCALS_VERSION").getAlgorithmVersion() >= 1) {
				canAccessLocalsBit = J9JavaVM.J9VM_DEBUG_ATTRIBUTE_CAN_ACCESS_LOCALS;
			}

			if (walkState.walkThread.javaVM().requiredDebugAttributes().anyBitsIn(canAccessLocalsBit)) {
				swPrintf(walkState, 4, "\tUsing debug local mapper");
				errorCode = j9localmap_DebugLocalBitsForPC(romMethod, UDATA
						.cast(offsetPC), result);
			} else {
				swPrintf(walkState, 4, "\tUsing local mapper");
				errorCode = j9localmap_LocalBitsForPC(romMethod, UDATA
						.cast(offsetPC), result);
			}
	
			if (errorCode < 0) {
				swPrintf(walkState, 3, "Local map failed, result = {0}", errorCode);
				/*
				 * Local map failed, result = %p - aborting VM - needs new message
				 * TBD
				 */
				throw new CorruptDataException("Local map failed thread " + walkState.walkThread.getHexAddress() + ", romMethod = " + romMethod.getHexAddress() + ", result - " + errorCode);
			}
	
			return;
		}
	
		private void walkJNICallInFrame(WalkState walkState)
				throws CorruptDataException
		{
			J9SFJNICallInFramePointer callInFrame;
	
			walkState.bp = walkState.arg0EA;
			callInFrame = J9SFJNICallInFramePointer.cast(walkState.bp.subOffset(
					J9SFJNICallInFrame.SIZEOF).addOffset(UDATA.SIZEOF));
	
			/*
			 * Retain any non-argument object pushes after the call-in frame (the
			 * exit point may want them)
			 */
			walkState.unwindSP = UDATAPointer.cast(callInFrame.subOffset(UDATA
					.cast(walkState.literals)));
	
			walkState.frameFlags = callInFrame.specialFrameFlags();
			printFrameType(walkState, "JNI call-in");

			if ((walkState.flags & J9_STACKWALK_ITERATE_O_SLOTS) != 0) {
				try {
					/*
					 * The following can only be true if the call-in method has returned
					 * (removed args and pushed return value)
					 */
		
					if (!(walkState.walkSP.eq(walkState.unwindSP))) {
						if (!walkState.pc.eq(walkState.walkThread.javaVM()
								.callInReturnPC().add(3))) {
							swPrintf(
									walkState,
									0,
									"Error: PC should have been advanced in order to push return value, pc = {0}, cipc = {1} !!!",
									walkState.pc.getHexAddress(),
									walkState.walkThread.javaVM()
											.callInReturnPC().getHexAddress());
						}
						if (walkState.frameFlags.anyBitsIn(J9_SSF_RETURNS_OBJECT)) {
							swPrintf(walkState, 4,
									"\tObject push (return value from call-in method)");
							WALK_O_SLOT(walkState, PointerPointer
									.cast(walkState.walkSP));
						} else {
							swPrintf(
									walkState,
									2,
									"\tCall-in return value (non-object) takes {0} slots at {1}",
									UDATA.cast(walkState.unwindSP).sub(
											UDATA.cast(walkState.walkSP)),
									walkState.walkSP.getHexAddress());
						}
		
						walkState.walkSP = walkState.unwindSP; /*
																 * so walkObjectPushes
																 * works correctly
																 */
					}
		
					if (walkState.literals.notNull()) {
						walkObjectPushes(walkState);
					}
				} catch (CorruptDataException ex) {
					handleOSlotsCorruption(walkState, "StackWalker_29_V0", "walkJNICallInFrame", ex);
				}
			}
	
			if (J9BuildFlags.interp_nativeSupport) {
				walkState.walkedEntryLocalStorage = walkState.walkedEntryLocalStorage
						.oldEntryLocalStorage();
				walkState.i2jState = walkState.walkedEntryLocalStorage.getAddress() != 0 ? J9I2JStatePointer
						.cast(walkState.walkedEntryLocalStorage.i2jStateEA())
						: J9I2JStatePointer.cast(0);
	
				swPrintf(walkState, 2, "\tNew ELS = {0}",
						walkState.walkedEntryLocalStorage.getHexAddress());
			}
			walkState.argCount = new UDATA(0);
		}
	
		private void printFrameType(WalkState walkState, String frameType)
				throws CorruptDataException
		{
			swPrintf(
					walkState,
					2,
					"{0} frame: bp = {1}, sp = {2}, pc = {3}, cp = {4}, arg0EA = {5}, flags = {6}",
					frameType, 
					walkState.bp.getHexAddress(),
					walkState.walkSP.getHexAddress(), 
					walkState.pc.getHexAddress(),
					walkState.constantPool.getHexAddress(),
					walkState.arg0EA.getHexAddress(), 
					walkState.frameFlags.getHexValue());
			swPrintMethod(walkState);
		}
	
		private void walkJITJNICalloutFrame(WalkState walkState)
				throws CorruptDataException
		{
			J9SFMethodFramePointer methodFrame = J9SFMethodFramePointer
					.cast(walkState.walkSP
							.addOffset(UDATA.cast(walkState.literals)));
	
			walkState.argCount = new UDATA(0);
			walkState.bp = UDATAPointer.cast(methodFrame.savedA0EA());
			walkState.frameFlags = methodFrame.specialFrameFlags();
			walkState.method = methodFrame.method();
			walkState.constantPool = UNTAGGED_METHOD_CP(walkState.method);
	
			printFrameType(walkState, "JIT JNI call-out");
	
			if ((walkState.flags & J9_STACKWALK_ITERATE_O_SLOTS) != 0) {
				try {
					WALK_METHOD_CLASS(walkState);
					if (walkState.literals.notNull()) {
						walkPushedJNIRefs(walkState);
					}
				} catch (CorruptDataException ex) {
					handleOSlotsCorruption(walkState, "StackWalker_29_V0", "walkJITJNICalloutFrame", ex);
				}
			}
		}
	
		private void walkJITResolveFrame(WalkState walkState)
				throws CorruptDataException
		{
			J9SFJITResolveFramePointer jitResolveFrame = J9SFJITResolveFramePointer
					.cast(walkState.walkSP
							.addOffset(UDATA.cast(walkState.literals)));
	
			walkState.argCount = new UDATA(0);
			walkState.bp = UDATAPointer.cast(jitResolveFrame
					.taggedRegularReturnSPEA());
			walkState.frameFlags = jitResolveFrame.specialFrameFlags();

			try {
				printFrameType(walkState, "JIT resolve");
		
				if ((walkState.flags & J9_STACKWALK_ITERATE_O_SLOTS) != 0) {
					PointerPointer savedJITExceptionSlot = jitResolveFrame
							.savedJITExceptionEA();
		
					swPrintf(walkState, 4, "\tObject push (savedJITException)");
					WALK_O_SLOT(walkState, savedJITExceptionSlot);
		
					if (walkState.literals.notNull()) {
						walkObjectPushes(walkState);
					}
				}
			} catch (CorruptDataException ex) {
				handleOSlotsCorruption(walkState, "StackWalker_29_V0", "walkJITResolveFrame", ex);
			}
	
		}
	
		private void walkGenericSpecialFrame(WalkState walkState)
				throws CorruptDataException
		{
			J9SFSpecialFramePointer specialFrame = J9SFSpecialFramePointer
					.cast(walkState.walkSP
							.addOffset(UDATA.cast(walkState.literals)));
	
			walkState.bp = UDATAPointer.cast(specialFrame.savedA0EA());
			walkState.frameFlags = specialFrame.specialFrameFlags();

			printFrameType(walkState, "Generic special");

			if (((walkState.flags & J9_STACKWALK_ITERATE_O_SLOTS) != 0)
					&& walkState.literals.notNull()) {
				try {
					walkObjectPushes(walkState);
				} catch (CorruptDataException ex) {
					handleOSlotsCorruption(walkState, "StackWalker_29_V0", "walkGenericSpecialFrame", ex);
				}
			}
	
			walkState.argCount = new UDATA(0);
		}

	}
}
