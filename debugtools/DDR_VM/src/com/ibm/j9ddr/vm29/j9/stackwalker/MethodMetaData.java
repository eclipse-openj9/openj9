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

import static com.ibm.j9ddr.vm29.j9.stackwalker.JITStackWalker.jitPrintRegisterMapArray;
import static com.ibm.j9ddr.vm29.j9.stackwalker.StackWalkerUtils.*;
import static com.ibm.j9ddr.vm29.pointer.generated.J9StackWalkFlags.J9SW_REGISTER_MAP_WALK_REGISTERS_LOW_TO_HIGH;
import static com.ibm.j9ddr.vm29.structure.J9StackWalkConstants.J9SW_POTENTIAL_SAVED_REGISTERS;
import static com.ibm.j9ddr.vm29.structure.J9StackWalkConstants.J9SW_REGISTER_MAP_MASK;
import static com.ibm.j9ddr.vm29.structure.MethodMetaDataConstants.INTERNAL_PTR_REG_MASK;
import static com.ibm.j9ddr.vm29.structure.J9JITExceptionTable.*;

import java.util.LinkedList;
import java.util.List;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.vm29.j9.AlgorithmPicker;
import com.ibm.j9ddr.vm29.j9.AlgorithmVersion;
import com.ibm.j9ddr.vm29.j9.J9ConfigFlags;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.U16Pointer;
import com.ibm.j9ddr.vm29.pointer.U32Pointer;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.UDATAPointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ConstantPoolPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JITExceptionTablePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JITStackAtlasPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.TRBuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.TR_ByteCodeInfoPointer;
import com.ibm.j9ddr.vm29.pointer.generated.TR_InlinedCallSitePointer;
import com.ibm.j9ddr.vm29.structure.CLimits;
import com.ibm.j9ddr.vm29.structure.J9JITStackAtlas;
import com.ibm.j9ddr.vm29.structure.TR_InlinedCallSite;
import com.ibm.j9ddr.vm29.types.I16;
import com.ibm.j9ddr.vm29.types.I32;
import com.ibm.j9ddr.vm29.types.IDATA;
import com.ibm.j9ddr.vm29.types.Scalar;
import com.ibm.j9ddr.vm29.types.U16;
import com.ibm.j9ddr.vm29.types.U32;
import com.ibm.j9ddr.vm29.types.U64;
import com.ibm.j9ddr.vm29.types.U8;
import com.ibm.j9ddr.vm29.types.UDATA;

/**
 * Java-equivalent of MethodMetaData.c.
 * 
 * Encapsulates the JIT-specific information required for stack walking.
 * 
 * Note that in the C, there's typedef J9JITExceptionTablePointer J9TR_MethodMetaData. That alias isn't
 * represented in the Java.
 * 
 * @author andhall
 *
 */
public class MethodMetaData
{
	public static final long REGISTER_MAP_VALUE_FOR_GAP = 0xFADECAFE;
	
	public static final long BYTE_CODE_INFO_VALUE_FOR_GAP = 0x0;
	
	public static I16 getJitTotalFrameSize(J9JITExceptionTablePointer md) throws CorruptDataException
	{
		return getImpl().getJitTotalFrameSize(md);
	}
	
	public static VoidPointer getJitInlinedCallInfo(J9JITExceptionTablePointer md) throws CorruptDataException
	{
		return getImpl().getJitInlinedCallInfo(md);
	}
	
	public static void jitAddSpilledRegistersForDataResolve(WalkState walkState) throws CorruptDataException
	{
		getImpl().jitAddSpilledRegistersForDataResolve(walkState);
	}
	
	public static UDATA getJitDataResolvePushes() throws CorruptDataException
	{
		return getImpl().getJitDataResolvePushes();
	}
	
	public static VoidPointer getStackMapFromJitPC(J9JavaVMPointer javaVM, J9JITExceptionTablePointer methodMetaData, UDATA jitPC) throws CorruptDataException
	{
		return getImpl().getStackMapFromJitPC(javaVM,methodMetaData,jitPC);
	}
	
	public static J9JITStackAtlasPointer getJitGCStackAtlas(J9JITExceptionTablePointer md) throws CorruptDataException
	{
		return getImpl().getJitGCStackAtlas(md);
	}
	
	public static VoidPointer getFirstInlinedCallSite(J9JITExceptionTablePointer methodMetaData, VoidPointer stackMap) throws CorruptDataException
	{
		return getImpl().getFirstInlinedCallSite(methodMetaData,stackMap);
	}
	
	public static UDATA getJitInlineDepthFromCallSite(J9JITExceptionTablePointer methodMetaData, VoidPointer inlinedCallSite) throws CorruptDataException
	{
		return getImpl().getJitInlineDepthFromCallSite(methodMetaData,inlinedCallSite);
	}
	
	public static boolean hasMoreInlinedMethods(VoidPointer inlinedCallSite) throws CorruptDataException
	{
		return getImpl().hasMoreInlinedMethods(inlinedCallSite);
	}
	
	public static VoidPointer getInlinedMethod(VoidPointer inlinedCallSite) throws CorruptDataException
	{
		return getImpl().getInlinedMethod(inlinedCallSite);
	}
	
	public static UDATA getCurrentByteCodeIndexAndIsSameReceiver(J9JITExceptionTablePointer methodMetaData, VoidPointer stackMap, VoidPointer currentInlinedCallSite, boolean[] isSameReceiver) throws CorruptDataException
	{
		return getImpl().getCurrentByteCodeIndexAndIsSameReceiver(methodMetaData, stackMap, currentInlinedCallSite, isSameReceiver);
	}
	
	public static void jitAddSpilledRegisters(WalkState walkState) throws CorruptDataException
	{
		getImpl().jitAddSpilledRegisters(walkState);
	}
	
	public static void jitAddSpilledRegisters(WalkState walkState, VoidPointer stackMap) throws CorruptDataException
	{
		getImpl().jitAddSpilledRegisters(walkState, stackMap);
	}
	
	public static UDATAPointer getObjectArgScanCursor(WalkState walkState) throws CorruptDataException
	{
		return getImpl().getObjectArgScanCursor(walkState);
	}
	
	public static U8Pointer getJitDescriptionCursor(VoidPointer stackMap, WalkState walkState) throws CorruptDataException
	{
		return getImpl().getJitDescriptionCursor(stackMap, walkState);
	}
	
	public static U16 getJitNumberOfMapBytes(J9JITStackAtlasPointer sa) throws CorruptDataException
	{
		return getImpl().getJitNumberOfMapBytes(sa);
	}
	
	public static U32 getJitRegisterMap(J9JITExceptionTablePointer methodMetaData, VoidPointer stackMap) throws CorruptDataException
	{
		return getImpl().getJitRegisterMap(methodMetaData,stackMap);
	}
	
	public static U8Pointer getNextDescriptionCursor(J9JITExceptionTablePointer metadata, VoidPointer stackMap, U8Pointer jitDescriptionCursor) throws CorruptDataException
	{
		return getImpl().getNextDescriptionCursor(metadata,stackMap,jitDescriptionCursor);
	}
	
	public static U16 getJitNumberOfParmSlots(J9JITStackAtlasPointer sa) throws CorruptDataException
	{
		return getImpl().getJitNumberOfParmSlots(sa);
	}
	
	public static U8Pointer getJitInternalPointerMap(J9JITStackAtlasPointer sa) throws CorruptDataException
	{
		return getImpl().getJitInternalPointerMap(sa);
	}
	
	public static void walkJITFrameSlotsForInternalPointers(WalkState walkState,  U8Pointer jitDescriptionCursor, UDATAPointer scanCursor, VoidPointer stackMap, J9JITStackAtlasPointer gcStackAtlas) throws CorruptDataException
	{
		getImpl().walkJITFrameSlotsForInternalPointers(walkState,jitDescriptionCursor,scanCursor,stackMap,gcStackAtlas);
	}
	
	public static VoidPointer getNextInlinedCallSite(
			J9JITExceptionTablePointer methodMetaData,
			VoidPointer inlinedCallSite) throws CorruptDataException
	{
		return getImpl().getNextInlinedCallSite(methodMetaData,inlinedCallSite);
	}
	
	public static U8 getNextDescriptionBit(U8Pointer jitDescriptionCursor) throws CorruptDataException
	{
		return getImpl().getNextDescriptionBit(jitDescriptionCursor);
	}
	
	public static UDATAPointer getObjectTempScanCursor(WalkState walkState) throws CorruptDataException
	{
		return getImpl().getObjectTempScanCursor(walkState);
	}
	
	public static int getJitRecompilationResolvePushes()
	{
		return getImpl().getJitRecompilationResolvePushes();
	}
	
	public static int getJitVirtualMethodResolvePushes()
	{
		return getImpl().getJitVirtualMethodResolvePushes();
	}
	
	public static int getJitStaticMethodResolvePushes()
	{
		return getImpl().getJitStaticMethodResolvePushes();
	}
	
	public static VoidPointer getStackAllocMapFromJitPC(J9JavaVMPointer javaVM, J9JITExceptionTablePointer methodMetaData, UDATA jitPC, VoidPointer curStackMap) throws CorruptDataException
	{
		return getImpl().getStackAllocMapFromJitPC(javaVM, methodMetaData, jitPC, curStackMap);
	}
	
	public static U8Pointer getJitStackSlots(J9JITExceptionTablePointer metaData,  VoidPointer stackMap) throws CorruptDataException
	{
		return getImpl().getJitStackSlots(metaData, stackMap);
	}
	
	public static void markClassesInInlineRanges(J9JITExceptionTablePointer metaData, WalkState walkState) throws CorruptDataException
	{
		getImpl().markClassesInInlineRanges(metaData, walkState);
	}
	
	/**
	 * Class used to pass maps by reference to jitGetMapsFromPC.
	 *
	 */
	public static class JITMaps
	{
		public PointerPointer stackMap = PointerPointer.NULL;
		public PointerPointer inlineMap = PointerPointer.NULL;
	}
	
	public static void jitGetMapsFromPC(J9JavaVMPointer javaVM,
			J9JITExceptionTablePointer methodMetaData, UDATA jitPC, JITMaps maps) throws CorruptDataException
	{
		getImpl().jitGetMapsFromPC(javaVM, methodMetaData, jitPC, maps);
	}

	
	private static MethodMetaDataImpl impl;
	
	private static final AlgorithmPicker<MethodMetaDataImpl> picker = new AlgorithmPicker<MethodMetaDataImpl>(AlgorithmVersion.METHOD_META_DATA_VERSION){

		@Override
		protected Iterable<? extends MethodMetaDataImpl> allAlgorithms()
		{
			List<MethodMetaDataImpl> toReturn = new LinkedList<MethodMetaDataImpl>();
			toReturn.add(new MethodMetaData_29_V0());
			return toReturn;
		}};
	
	private static MethodMetaDataImpl getImpl()
	{
		if (impl == null) {
			impl = picker.pickAlgorithm();
		}
		return impl;
	}
	
	/* In Java 8 builds, the (interim) compiler gets confused if IAlgorithm is not fully qualified. */
	private static interface MethodMetaDataImpl extends com.ibm.j9ddr.vm29.j9.IAlgorithm
	{
		public UDATAPointer getObjectTempScanCursor(WalkState walkState) throws CorruptDataException;
		
		public int getJitStaticMethodResolvePushes();

		public int getJitVirtualMethodResolvePushes();

		public int getJitRecompilationResolvePushes();

		public U8Pointer getJitDescriptionCursor(VoidPointer stackMap, WalkState walkState) throws CorruptDataException;
		
		public void walkJITFrameSlotsForInternalPointers(WalkState walkState,U8Pointer jitDescriptionCursor, UDATAPointer scanCursor,VoidPointer stackMap, J9JITStackAtlasPointer gcStackAtlas) throws CorruptDataException;

		public U8Pointer getJitInternalPointerMap(J9JITStackAtlasPointer sa) throws CorruptDataException;

		public U16 getJitNumberOfParmSlots(J9JITStackAtlasPointer sa) throws CorruptDataException;

		public U8Pointer getNextDescriptionCursor(J9JITExceptionTablePointer metadata, VoidPointer stackMap,U8Pointer jitDescriptionCursor) throws CorruptDataException;

		public U32 getJitRegisterMap(J9JITExceptionTablePointer methodMetaData,VoidPointer stackMap) throws CorruptDataException;

		public U16 getJitNumberOfMapBytes(J9JITStackAtlasPointer sa) throws CorruptDataException;

		public I16 getJitTotalFrameSize(J9JITExceptionTablePointer md) throws CorruptDataException;

		public UDATAPointer getObjectArgScanCursor(WalkState walkState) throws CorruptDataException;

		public UDATA getCurrentByteCodeIndexAndIsSameReceiver(
				J9JITExceptionTablePointer methodMetaData,
				VoidPointer stackMap, VoidPointer currentInlinedCallSite,
				boolean[] isSameReceiver) throws CorruptDataException;

		public boolean hasMoreInlinedMethods(VoidPointer inlinedCallSite) throws CorruptDataException;
		
		public VoidPointer getInlinedMethod(VoidPointer inlinedCallSite) throws CorruptDataException;

		public UDATA getJitInlineDepthFromCallSite(
				J9JITExceptionTablePointer methodMetaData,
				VoidPointer inlinedCallSite) throws CorruptDataException;

		public VoidPointer getFirstInlinedCallSite(
				J9JITExceptionTablePointer methodMetaData, VoidPointer stackMap) throws CorruptDataException;
		
		public VoidPointer getNextInlinedCallSite(J9JITExceptionTablePointer methodMetaData,VoidPointer inlinedCallSite) throws CorruptDataException;

		public J9JITStackAtlasPointer getJitGCStackAtlas(
				J9JITExceptionTablePointer md) throws CorruptDataException;

		public VoidPointer getStackMapFromJitPC(J9JavaVMPointer javaVM,
				J9JITExceptionTablePointer methodMetaData, UDATA jitPC)  throws CorruptDataException;

		public UDATA getJitDataResolvePushes() throws CorruptDataException;

		public void jitAddSpilledRegistersForDataResolve(WalkState walkState) throws CorruptDataException;

		public VoidPointer getJitInlinedCallInfo(J9JITExceptionTablePointer md) throws CorruptDataException;
		
		public void jitGetMapsFromPC(J9JavaVMPointer javaVM,
				J9JITExceptionTablePointer methodMetaData, UDATA jitPC, JITMaps maps) throws CorruptDataException;
		
		public void jitAddSpilledRegisters(WalkState walkState) throws CorruptDataException;
		
		public void jitAddSpilledRegisters(WalkState walkState, VoidPointer stackMap) throws CorruptDataException;
		
		public U8 getNextDescriptionBit(U8Pointer jitDescriptionCursor) throws CorruptDataException;
		
		public VoidPointer getStackAllocMapFromJitPC(J9JavaVMPointer javaVM, J9JITExceptionTablePointer methodMetaData, UDATA jitPC, VoidPointer curStackMap) throws CorruptDataException;
		
		public U8Pointer getJitStackSlots(J9JITExceptionTablePointer metaData,  VoidPointer stackMap) throws CorruptDataException;
		
		public void markClassesInInlineRanges(J9JITExceptionTablePointer metaData, WalkState walkState) throws CorruptDataException;
	}

	/* In Java 8 builds, the (interim) compiler gets confused if BaseAlgorithm is not fully qualified. */
	private static class MethodMetaData_29_V0 extends com.ibm.j9ddr.vm29.j9.BaseAlgorithm implements MethodMetaDataImpl
	{

		private static boolean alignStackMaps = J9ConfigFlags.arch_arm || TRBuildFlags.host_SH4 || TRBuildFlags.host_MIPS;

		protected MethodMetaData_29_V0() {
			super(90,0);
		}
		
		public I16 getJitTotalFrameSize(J9JITExceptionTablePointer md)
				throws CorruptDataException
		{
			return new I16(md.totalFrameSize());
		}
		
		public VoidPointer getJitInlinedCallInfo(J9JITExceptionTablePointer md)
				throws CorruptDataException
		{
			return md.inlinedCalls();
		}

		public void jitAddSpilledRegistersForDataResolve(WalkState walkState)
				throws CorruptDataException
		{
			UDATAPointer slotCursor = walkState.unwindSP.add(getJitSlotsBeforeSavesInDataResolve());
			int mapCursor = 0;
			
			for (int i = 0; i < J9SW_POTENTIAL_SAVED_REGISTERS; ++i) 
			{
				walkState.registerEAs[mapCursor++] = slotCursor;
				slotCursor = slotCursor.add(1);
			}

			swPrintf(walkState, 2, "\t{0} slots skipped before scalar registers", getJitSlotsBeforeSavesInDataResolve());
			jitPrintRegisterMapArray(walkState, "DataResolve");
		}
		
		/* Number of slots of data pushed after the integer registers during data resolves */
		
		private static UDATA getJitSlotsBeforeSavesInDataResolve()
		{
			if (J9ConfigFlags.arch_x86) {
				if (J9BuildFlags.env_data64) {
					/* AMD64 data resolve shape
					 16 slots of XMM registers
					 16 slots of scalar registers
					 1 slot flags
					 1 slot call from snippet
					 1 slot call from codecache to snippet
					 */
					return new UDATA(16);
				} else {
					/* IA32 data resolve shape
					 20 slots FP saves
					 7 slots integer registers
					 1 slot saved flags
					 1 slot return address in snippet
					 1 slot literals
					 1 slot cpIndex
					 1 slot call from codecache to snippet
					 */
					return new UDATA(20);
				}
			} else {
				return new UDATA(0);
			}
		}

		public void jitAddSpilledRegisters(WalkState walkState) throws CorruptDataException
		{
			throw new UnsupportedOperationException("Not implemented at 2.6");
		}
		
		public void jitAddSpilledRegisters(WalkState walkState, VoidPointer stackMap) throws CorruptDataException
		{
			UDATA savedGPRs = new UDATA(0), saveOffset = new UDATA(0), lowestRegister = new UDATA(0);
			UDATAPointer saveCursor = UDATAPointer.NULL;
			//UDATA ** mapCursor = (UDATA **) &(walkState->registerEAs);
			int mapCursor = 0;
			J9JITExceptionTablePointer md = walkState.jitInfo;
			UDATA registerSaveDescription = walkState.jitInfo.registerSaveDescription();

			if (J9ConfigFlags.arch_x86) {
				UDATA prologuePushes = new UDATA(getJitProloguePushes(walkState.jitInfo));
				U8 i = new U8(1); 

				if (! prologuePushes.eq(0)) {
					saveCursor = walkState.bp.sub(  new UDATA(getJitScalarTempSlots(walkState.jitInfo)).add(new UDATA(getJitObjectTempSlots(walkState.jitInfo)).add(prologuePushes)) );

					registerSaveDescription = registerSaveDescription.bitAnd(J9SW_REGISTER_MAP_MASK);
					do
					{
						if (registerSaveDescription.anyBitsIn(1))
						{
							walkState.registerEAs[mapCursor] = saveCursor;
							saveCursor = saveCursor.add(1);
						}
						i = i.add(1);
						++mapCursor;
						registerSaveDescription = registerSaveDescription.rightShift(1);
					}
					while (! registerSaveDescription.eq(0));
				}
			} else if (J9ConfigFlags.arch_power || TRBuildFlags.host_MIPS) {
				if (J9ConfigFlags.arch_power) {
					/*
					 * see PPCLinkage for a description of the RSD 
					 * the save offset is located from bits 18-32 
					 * so first mask it off to get the bit vector 
					 * corresponding to the saved GPRS
					 */
					savedGPRs = registerSaveDescription.bitAnd(new UDATA(0x1FFFFL));
					saveOffset = registerSaveDescription.rightShift(17).bitAnd(new UDATA(0xFFFFL));
					lowestRegister = new UDATA(15); /* gpr15 is the first saved GPR, so move 15 spaces */
				} else if (TRBuildFlags.host_MIPS) {
					savedGPRs = registerSaveDescription.bitAnd(31);
					saveOffset = registerSaveDescription.rightShift(13);
					/* on MIPS, the top eight registers are not used for objects */
					lowestRegister = new UDATA(24).sub(savedGPRs);
				}
				saveCursor = walkState.bp.subOffset(saveOffset);

				if (J9ConfigFlags.arch_power) {
					mapCursor += lowestRegister.intValue(); /* access gpr15 in the vm register state */
					U8 i = new U8(lowestRegister.add(1));
					do 
					{
						if (savedGPRs.anyBitsIn(1)) {
							walkState.registerEAs[mapCursor] = saveCursor;
							saveCursor = saveCursor.add(1);
						}

						i = i.add(1);
						++mapCursor;
						savedGPRs = savedGPRs.rightShift(1);
					}
					while (! savedGPRs.eq(0));
				} else {
					mapCursor += lowestRegister.intValue();
					while (! savedGPRs.eq(0))
					{
						walkState.registerEAs[mapCursor++] = saveCursor;
						saveCursor = saveCursor.add(1);
						savedGPRs = savedGPRs.sub(1);
					}
				}
			} else if (J9ConfigFlags.arch_arm || TRBuildFlags.host_SH4 || J9ConfigFlags.arch_s390) {
				savedGPRs = registerSaveDescription.bitAnd(new UDATA(0xFFFF));

				if (! savedGPRs.eq(0))
				{
					saveOffset = new UDATA(0xFFFF).bitAnd(registerSaveDescription.rightShift(16)); /* UDATA is a  ptr, so clean high word */
																				/* for correctness on 64bit platforms  */
					saveCursor = walkState.bp.subOffset(saveOffset);
					U8 i = new U8(1);
					do
					{
						if (savedGPRs.anyBitsIn(1))
						{
							walkState.registerEAs[mapCursor] = saveCursor;
							saveCursor = saveCursor.add(1);
						}
						i = i.add(1);
						++mapCursor;
						savedGPRs = savedGPRs.rightShift(1);
					}
					while (! savedGPRs.eq(0));
				}
			}

			jitPrintRegisterMapArray(walkState, "Frame");
		}
		
		private U8Pointer GET_REGISTER_SAVE_DESCRIPTION_CURSOR(boolean fourByteOffset, VoidPointer stackMap)
		{
			return U8Pointer.cast(stackMap).add(SIZEOF_MAP_OFFSET(fourByteOffset)).add(U32.SIZEOF);
		}

		public U16 getJitProloguePushes(J9JITExceptionTablePointer md) throws CorruptDataException
		{
			return md.prologuePushes(); 
		}
		
		public I16 getJitScalarTempSlots(J9JITExceptionTablePointer md) throws CorruptDataException
		{
			return md.scalarTempSlots(); 
		}

		public I16 getJitObjectTempSlots(J9JITExceptionTablePointer md) throws CorruptDataException
		{ 
			return md.objectTempSlots(); 
		}
		
		public UDATA getJitDataResolvePushes() throws CorruptDataException {
			if (J9ConfigFlags.arch_x86) {
				if (J9BuildFlags.env_data64) {
					/* AMD64 data resolve shape
					   16 slots of XMM registers
					   16 slots of integer registers
					   1 slot flags
					   1 slot call from snippet
					   1 slot call from codecache to snippet
					*/
					return new UDATA(35);
				} else {
					/* IA32 data resolve shape
					   20 slots FP saves
					   7 slots integer registers
					   1 slot saved flags
					   1 slot return address in snippet
					   1 slot literals
					   1 slot cpIndex
					   1 slot call from codecache to snippet
					*/
					return new UDATA(32);
				}
			} else if (J9ConfigFlags.arch_arm) {
				/* ARM data resolve shape
				   12 slots saved integer registers
				*/
				return new UDATA(12);
			} else if (J9ConfigFlags.arch_s390) {
				/* 390 data resolve shape
				   16 integer registers
				*/
				if (J9BuildFlags.jit_32bitUses64bitRegisters) {
					return new UDATA(32);
				} else {
					return new UDATA(16);
				}
			} else if (J9ConfigFlags.arch_power) {
				/* PPC data resolve shape
				   32 integer registers
				   CR
				*/
				return new UDATA(33);
			} else if (TRBuildFlags.host_MIPS) {
				/* MIPS data resolve shape
				   32 integer registers
				*/
				return new UDATA(32);
			} else if (TRBuildFlags.host_SH4) {
				/* SH4 data resolve shape
				   16 integer registers 
				*/
				return new UDATA(16);
			} else {
				return new UDATA(0);
			}
		}

		public VoidPointer getStackMapFromJitPC(J9JavaVMPointer javaVM,
				J9JITExceptionTablePointer methodMetaData, UDATA jitPC) throws CorruptDataException
		{
			JITMaps maps = new JITMaps();
			
			jitGetMapsFromPC(javaVM, methodMetaData, jitPC, maps);
			return VoidPointer.cast(maps.stackMap);
		}

		public J9JITStackAtlasPointer getJitGCStackAtlas(J9JITExceptionTablePointer md) throws CorruptDataException
		{
			return J9JITStackAtlasPointer.cast(md.gcStackAtlas());
		}
		
		public void jitGetMapsFromPC(J9JavaVMPointer javaVM,
				J9JITExceptionTablePointer methodMetaData, UDATA jitPC,
				JITMaps maps) throws CorruptDataException
		{
			MapIterator i = new MapIterator();
			
			/* We subtract one from the jitPC as jitPCs that come in are always pointing at the beginning of the next instruction
			 * (ie: the return address from the child call).  In the case of trap handlers, the GP handler has already bumped the PC
			 * forward by one, expecting this -1 to happen.
			 */
			UDATA offsetPC = jitPC.sub(methodMetaData.startPC()).sub(1);
			
			boolean fourByteOffsets = HAS_FOUR_BYTE_OFFSET(methodMetaData); 
			
			maps.stackMap = PointerPointer.NULL;
			maps.inlineMap = PointerPointer.NULL;
			
			if (methodMetaData.gcStackAtlas().isNull()) { 
				return;
			}

			initializeIterator(i, methodMetaData);
			
			findMapsAtPC(i, offsetPC, maps, fourByteOffsets);
		}

		private void findMapsAtPC(MapIterator i, UDATA offsetPC, JITMaps maps,
				boolean fourByteOffsets) throws CorruptDataException
		{
			while (getNextMap(i, fourByteOffsets).notNull()) {
				if (matchingRange(i, offsetPC)) {
					maps.stackMap = currentStackMap(i);
					maps.inlineMap = currentInlineMap(i);
					break;
				}
			}
		}

		private static PointerPointer currentInlineMap(MapIterator i)
		{
			return PointerPointer.cast(i._currentInlineMap);
		}

		private static PointerPointer currentStackMap(MapIterator i)
		{
			return PointerPointer.cast(i._currentStackMap);
		}

		private boolean matchingRange(MapIterator i, UDATA offset)
		{
			return i._rangeStartOffset.lte(offset) && offset.lte(i._rangeEndOffset);
		}

		private U8Pointer getNextMap(MapIterator i, boolean fourByteOffsets) throws CorruptDataException
		{
			i._currentMap = i._nextMap;

			if (i._currentMap.notNull()) {
				i._currentInlineMap = i._currentMap;
				if (!IS_BYTECODEINFO_MAP(fourByteOffsets, i._currentMap)) {
					i._currentStackMap = i._currentMap;
				}
				i._rangeStartOffset = GET_LOW_PC_OFFSET_VALUE(fourByteOffsets, i._currentMap);
				i._mapIndex = i._mapIndex.add(1);
				if (i._mapIndex.lt(i._stackAtlas.numberOfMaps())) {
					i._nextMap = GET_NEXT_STACK_MAP(fourByteOffsets, i._currentMap, i._stackAtlas);
					i._rangeEndOffset = GET_LOW_PC_OFFSET_VALUE(fourByteOffsets, i._nextMap).sub(1);
				}
				else {
					i._nextMap = U8Pointer.NULL;
					i._rangeEndOffset = i._methodMetaData.endPC().sub(i._methodMetaData.startPC()).sub(1);
				}
			}

			return i._currentMap;
		}
		
		private static UDATA GET_LOW_PC_OFFSET_VALUE(boolean fourByteOffset, U8Pointer stackMap) throws CorruptDataException
		{
			Scalar returnValue = null;
			if (fourByteOffset) {
				returnValue = U32Pointer.cast(ADDRESS_OF_LOW_PC_OFFSET_IN_STACK_MAP(fourByteOffset,stackMap)).at(0);
			} else {
				returnValue = U16Pointer.cast(ADDRESS_OF_LOW_PC_OFFSET_IN_STACK_MAP(fourByteOffset,stackMap)).at(0);
			}
			
			return new UDATA(returnValue);
		}
		
		private static U32 GET_SIZEOF_BYTECODEINFO_MAP(boolean fourByteOffset)
		{
			return new U32(U32.SIZEOF).add(SIZEOF_MAP_OFFSET(fourByteOffset));
		}
		
		private static U8Pointer ADDRESS_OF_BYTECODEINFO_IN_STACK_MAP(boolean fourByteOffset, U8Pointer stackMap)
		{
			return stackMap.add(SIZEOF_MAP_OFFSET(fourByteOffset));
		}
		
		private static U8Pointer ADDRESS_OF_LOW_PC_OFFSET_IN_STACK_MAP(boolean fourByteOffset, U8Pointer stackMap)
		{
			return stackMap;
		}
		
		@SuppressWarnings("unused")
		private static U8Pointer ADDRESS_OF_REGISTERMAP(boolean fourByteOffset, U8Pointer stackMap)
		{
			return stackMap.add(SIZEOF_MAP_OFFSET(fourByteOffset)).add(2 * U32.SIZEOF);
		}
		
		private static boolean RANGE_NEEDS_FOUR_BYTE_OFFSET(Scalar s)
		{
			//#define RANGE_NEEDS_FOUR_BYTE_OFFSET(r)   (((r) >= (USHRT_MAX   )) ? 1 : 0)
			return s.gte(new U64(CLimits.USHRT_MAX));
		}

		private static boolean HAS_FOUR_BYTE_OFFSET(J9JITExceptionTablePointer md) throws CorruptDataException
		{
			if (AlgorithmVersion.getVersionOf(AlgorithmVersion.FOUR_BYTE_OFFSETS_VERSION).getAlgorithmVersion() > 0) {
				if (md.flags().anyBitsIn(JIT_METADATA_FLAGS_USED_FOR_SIZE)) {
					return (md.flags().anyBitsIn(JIT_METADATA_GC_MAP_32_BIT_OFFSETS));	
				}
			}
			return RANGE_NEEDS_FOUR_BYTE_OFFSET(md.endPC().sub(md.startPC()));
		}
		
		private static U32 SIZEOF_MAP_OFFSET(boolean fourByteOffset)
		{
			return (alignStackMaps || fourByteOffset) ? new U32(4) : new U32(2);
		}
		
		private static boolean IS_BYTECODEINFO_MAP(boolean fourByteOffset, U8Pointer stackMap) throws CorruptDataException 
		{
			return ! TR_ByteCodeInfoPointer.cast(ADDRESS_OF_BYTECODEINFO_IN_STACK_MAP(fourByteOffset, stackMap))._doNotProfile().eq(0);
		}
		
		//#define GET_REGISTER_MAP_CURSOR(fourByteOffset, stackMap) ((U_8 *)stackMap + SIZEOF_MAP_OFFSET(fourByteOffset) + 2*sizeof(U_32))
		private static U8Pointer GET_REGISTER_MAP_CURSOR(boolean fourByteOffset, U8Pointer stackMap)
		{
			return stackMap.add(SIZEOF_MAP_OFFSET(fourByteOffset).add(2 * U32.SIZEOF));
		}
		
		/* Note: this differs from the native version in that nextStackMap is returned - not passed by reference */
		private static U8Pointer GET_NEXT_STACK_MAP(boolean fourByteOffset, U8Pointer stackMap, J9JITStackAtlasPointer atlas) throws CorruptDataException
		{
			U8Pointer nextStackMap = stackMap;
			
			if (IS_BYTECODEINFO_MAP(fourByteOffset, stackMap)) {
				nextStackMap = nextStackMap.add(GET_SIZEOF_BYTECODEINFO_MAP(fourByteOffset));
			} else {
				nextStackMap = GET_REGISTER_MAP_CURSOR(fourByteOffset, stackMap);
				if ( U32Pointer.cast(nextStackMap).at(0).anyBitsIn(INTERNAL_PTR_REG_MASK) && atlas.internalPointerMap().notNull() ) {
					nextStackMap = nextStackMap.add(nextStackMap.add(4).at(0).add(1));
				}
				nextStackMap = nextStackMap.add(3).add(atlas.numberOfMapBytes());
				if (nextStackMap.at(0).anyBitsIn(128)) {
					nextStackMap = nextStackMap.add(atlas.numberOfMapBytes());
				}
				nextStackMap = nextStackMap.add(1);
			}
			
			return nextStackMap;
		}
		
		private void initializeIterator(MapIterator i,
				J9JITExceptionTablePointer methodMetaData) throws CorruptDataException
		{
			i._methodMetaData = methodMetaData;
			i._stackAtlas = J9JITStackAtlasPointer.cast(methodMetaData.gcStackAtlas());
			i._currentStackMap = U8Pointer.NULL;
			i._currentInlineMap = U8Pointer.NULL;
			i._nextMap = getFirstStackMap(i._stackAtlas);
			i._mapIndex = new U32(0);
		}

		private U8Pointer getFirstStackMap(J9JITStackAtlasPointer stackAtlas) throws CorruptDataException
		{
			return U8Pointer.cast(stackAtlas).add(J9JITStackAtlas.SIZEOF).add(stackAtlas.numberOfMapBytes());
		}

		public VoidPointer getFirstInlinedCallSite(
				J9JITExceptionTablePointer methodMetaData, VoidPointer stackMap)
				throws CorruptDataException
		{
			return getFirstInlinedCallSiteWithByteCodeInfo(methodMetaData, stackMap, VoidPointer.NULL);
		}

		//#define ADDRESS_OF_BYTECODEINFO_IN_STACK_MAP(fourByteOffset, stackMap) ((U_8 *)stackMap + SIZEOF_MAP_OFFSET(fourByteOffset))
		private VoidPointer ADDRESS_OF_BYTECODEINFO_IN_STACK_MAP(boolean fourByteOffset, VoidPointer stackMap)
		{
			return VoidPointer.cast(U8Pointer.cast(stackMap).add(SIZEOF_MAP_OFFSET(fourByteOffset)));
		}
		
		private VoidPointer getFirstInlinedCallSiteWithByteCodeInfo(J9JITExceptionTablePointer methodMetaData, VoidPointer stackMap, VoidPointer byteCodeInfo) throws CorruptDataException
		{
			if (byteCodeInfo.isNull()) {
				byteCodeInfo = ADDRESS_OF_BYTECODEINFO_IN_STACK_MAP(HAS_FOUR_BYTE_OFFSET(methodMetaData), stackMap);
			}
			I32 cix = new I32(TR_ByteCodeInfoPointer.cast(byteCodeInfo)._callerIndex());
			if (cix.lt(0)) {
				return  VoidPointer.NULL;
			}

			return getNotUnloadedInlinedCallSiteArrayElement(methodMetaData, cix);
		}
		

		private U32 sizeOfInlinedCallSiteArrayElement(J9JITExceptionTablePointer methodMetaData) throws CorruptDataException
		{
			return new U32(TR_InlinedCallSite.SIZEOF).add(J9JITStackAtlasPointer.cast(methodMetaData.gcStackAtlas()).numberOfMapBytes());
		}
		
		private U8Pointer getInlinedCallSiteArrayElement(J9JITExceptionTablePointer methodMetaData, I32 cix) throws CorruptDataException
		{
			U8Pointer inlinedCallSiteArray = U8Pointer.cast(getJitInlinedCallInfo(methodMetaData));
			if (inlinedCallSiteArray.notNull()) {
				return inlinedCallSiteArray.add(sizeOfInlinedCallSiteArrayElement(methodMetaData).mult(cix.intValue()));
			}

			return U8Pointer.NULL;
		}

		private boolean isUnloadedInlinedMethod(J9MethodPointer method)
		{
			return UDATA.cast(method).bitNot().eq(0); /* d142150: check for method==-1 in a way that works on ZOS */
		}
		
		private  VoidPointer getNotUnloadedInlinedCallSiteArrayElement(J9JITExceptionTablePointer methodMetaData, I32 cix) throws CorruptDataException
		{
			VoidPointer inlinedCallSite = VoidPointer.cast(getInlinedCallSiteArrayElement(methodMetaData, cix));
			while (isUnloadedInlinedMethod(J9MethodPointer.cast(getInlinedMethod(inlinedCallSite))))
			{
				inlinedCallSite = getNextInlinedCallSite(methodMetaData, inlinedCallSite);
				if (inlinedCallSite.isNull())
					break;
			}
			return inlinedCallSite;
		}

		public UDATA getJitInlineDepthFromCallSite(
				J9JITExceptionTablePointer methodMetaData,
				VoidPointer inlinedCallSite) throws CorruptDataException
		{
			UDATA inlineDepth = new UDATA(0);
			do {
				inlineDepth = inlineDepth.add(1);
				inlinedCallSite = getNextInlinedCallSite(methodMetaData, inlinedCallSite);
			} while (inlinedCallSite.notNull());
			return inlineDepth;
		}

		public VoidPointer getNextInlinedCallSite(
				J9JITExceptionTablePointer methodMetaData,
				VoidPointer inlinedCallSite) throws CorruptDataException
		{
			if (hasMoreInlinedMethods(inlinedCallSite)) {
				return getNotUnloadedInlinedCallSiteArrayElement(methodMetaData, new I32(getByteCodeInfo(inlinedCallSite)._callerIndex()));
			}
			return VoidPointer.NULL; 
		}
		
		public boolean hasMoreInlinedMethods(VoidPointer inlinedCallSite) throws CorruptDataException
		{
			TR_ByteCodeInfoPointer byteCodeInfo = getByteCodeInfo(inlinedCallSite);
			return ! byteCodeInfo._callerIndex().lt(new I32(0));
		}
		
		private static TR_ByteCodeInfoPointer getByteCodeInfo(VoidPointer inlinedCallSite) throws CorruptDataException
		{
			return TR_ByteCodeInfoPointer.cast(TR_InlinedCallSitePointer.cast(inlinedCallSite)._byteCodeInfoEA());
		}
		
		public VoidPointer getInlinedMethod(VoidPointer inlinedCallSite) throws CorruptDataException
		{
			return TR_InlinedCallSitePointer.cast(inlinedCallSite)._methodInfo();
		}
		
		public UDATA getCurrentByteCodeIndexAndIsSameReceiver(J9JITExceptionTablePointer methodMetaData, VoidPointer stackMap, VoidPointer currentInlinedCallSite, boolean[] isSameReceiver) throws CorruptDataException
		{
			TR_ByteCodeInfoPointer byteCodeInfo = TR_ByteCodeInfoPointer.cast(getByteCodeInfoFromStackMap(methodMetaData, stackMap));
			
			if (currentInlinedCallSite.notNull()) {
				VoidPointer inlinedCallSite = getFirstInlinedCallSiteWithByteCodeInfo(methodMetaData, stackMap, VoidPointer.cast(byteCodeInfo));
				if (! inlinedCallSite.eq(currentInlinedCallSite))
				{
					VoidPointer previousInlinedCallSite;
					do
					{
						previousInlinedCallSite = inlinedCallSite;
						inlinedCallSite = getNextInlinedCallSite(methodMetaData, inlinedCallSite);
					}
					while (! inlinedCallSite.eq(currentInlinedCallSite));
					byteCodeInfo = getByteCodeInfo(previousInlinedCallSite);
				}
			} else if (! byteCodeInfo._callerIndex().eq(-1)) {
				VoidPointer inlinedCallSite = getFirstInlinedCallSiteWithByteCodeInfo(methodMetaData, stackMap, VoidPointer.cast(byteCodeInfo));
				VoidPointer prevInlinedCallSite = inlinedCallSite;
				while (inlinedCallSite.notNull() && hasMoreInlinedMethods(inlinedCallSite))
				{
					prevInlinedCallSite = inlinedCallSite;
					inlinedCallSite = getNextInlinedCallSite(methodMetaData, inlinedCallSite);
				}
				byteCodeInfo = getByteCodeInfo(prevInlinedCallSite);
			}

			if (isSameReceiver != null) {
				isSameReceiver[0] = ! byteCodeInfo._isSameReceiver().eq(0);
			}	
			return new UDATA(byteCodeInfo._byteCodeIndex());
		}

		private VoidPointer getByteCodeInfoFromStackMap(
				J9JITExceptionTablePointer methodMetaData, VoidPointer stackMap) throws CorruptDataException
		{
			return VoidPointer.cast(ADDRESS_OF_BYTECODEINFO_IN_STACK_MAP(HAS_FOUR_BYTE_OFFSET(methodMetaData), U8Pointer.cast(stackMap)));
		}
		
		public UDATAPointer getObjectArgScanCursor(WalkState walkState) throws CorruptDataException
		{
			return UDATAPointer.cast(U8Pointer.cast(walkState.bp).addOffset(J9JITStackAtlasPointer.cast(walkState.jitInfo.gcStackAtlas()).parmBaseOffset()));
		}
		
		public U8Pointer getJitDescriptionCursor(VoidPointer stackMap, WalkState walkState) throws CorruptDataException
		{ 
			/* deprecated */
			return U8Pointer.NULL;
		}

		public U16 getJitNumberOfMapBytes(J9JITStackAtlasPointer sa) throws CorruptDataException
		{
			return sa.numberOfMapBytes();
		}

		public U32 getJitRegisterMap(J9JITExceptionTablePointer methodMetaData, VoidPointer stackMap) throws CorruptDataException
		{
			return U32Pointer.cast(GET_REGISTER_MAP_CURSOR(HAS_FOUR_BYTE_OFFSET(methodMetaData), stackMap)).at(0);
		}		
		
		public U8Pointer getNextDescriptionCursor(J9JITExceptionTablePointer metadata, VoidPointer stackMap, U8Pointer jitDescriptionCursor) throws CorruptDataException
		{
			throw new UnsupportedOperationException("Not implemented at 2.6");
		}

		public U8Pointer getJitInternalPointerMap(J9JITStackAtlasPointer sa) throws CorruptDataException
		{
			return sa.internalPointerMap();
		}
		
		public U16 getJitNumberOfParmSlots(J9JITStackAtlasPointer sa) throws CorruptDataException
		{
			return sa.numberOfParmSlots();
		}
		
		/* Note - unlike the native version of this function, we don't mutate the jitDescriptionCursor */
		public U8 getNextDescriptionBit(U8Pointer jitDescriptionCursor) throws CorruptDataException
		{
			return jitDescriptionCursor.at(0);
		}
		
		public UDATAPointer getObjectTempScanCursor(WalkState walkState) throws CorruptDataException
		{
			//return (UDATA *) (((U_8 *) walkState->bp) + ((J9JITStackAtlas *)walkState->jitInfo->gcStackAtlas)->localBaseOffset);
			return walkState.bp.addOffset( J9JITStackAtlasPointer.cast(walkState.jitInfo.gcStackAtlas()).localBaseOffset() );
		}
		
		/** Number of slots pushed between the unwindSP of the JIT frame and the pushed register arguments for recompilation resolves.

		   Include the slot for the pushed return address (if any) for the call from the codecache to the picbuilder.
		   Do not include the slot for return address for call from picbuilder to resolve helper.
		*/
		public int getJitRecompilationResolvePushes() {
			if (J9ConfigFlags.arch_x86) {
				if (J9BuildFlags.env_data64) {
					/* AMD64 recompilation resolve shape
					0: rcx (arg register)
					1: rdx (arg register)
					2: rsi (arg register)
					3: rax (arg register)
					4: 8 XMMs (1 8-byte slot each)				<== unwindSP points here
					12: return PC (caller of recompiled method)
					13: <last argument to method>				<== unwindSP should point here"
					*/
					return 9;
				} else {
					/* IA32 recompilation resolve shape
					0: return address (picbuilder)
					1: return PC (caller of recompiled method)
					2: old start address
					3: method
					4: EAX (contains receiver for virtual)	<== unwindSP points here
					5: EDX
					6: return PC (caller of recompiled method)
					7: <last argument to method>				<== unwindSP should point here"
					*/
					return 3;
				}
			} else if (J9ConfigFlags.arch_s390) {
				/* 390 recompilation resolve shape
				0:		r3 (arg register)
				1:		r2 (arg register)
				2:		r1 (arg register)
				3:		??							<== unwindSP points here
				4:		??											
				5:		??
				6:		64 bytes FP register saves
				7:		temp1
				8:		temp2
				9:		temp3
				XX:	<linkage area>							<== unwindSP should point here
				*/
				return 7 + (64 / UDATA.SIZEOF);
			} else if (J9ConfigFlags.arch_power) {
				/* PPC recompilation resolve shape
				0:		r10 (arg register)
				1:		r9 (arg register)
				2:		r8 (arg register)
				3:		r7 (arg register)
				4:		r6 (arg register)
				5:		r5 (arg register)
				6:		r4 (arg register)
				7:		r3 (arg register)
				8: 	r0	      						<== unwindSP points here
				9:	r12
				10:	r12
				XX:	<linkage area>						<== unwindSP should point here
				*/
				return 3;
			} else {
				return 0;
			}
		}
		
		/** Number of slots pushed between the unwindSP of the JIT frame and the pushed register arguments for virtual/interface method resolves.

		   Include the slot for the pushed return address (if any) for the call from the codecache to the picbuilder.
		   Do not include the slot for return address for call from picbuilder to resolve helper.
		*/
		public int getJitVirtualMethodResolvePushes() {
			if (J9ConfigFlags.arch_x86) {
				if (J9BuildFlags.env_data64) {
					/* AMD64 virtual resolve shape
					0: ret addr to picbuilder
					1: arg3
					2: arg2
					3: arg1
					4: arg0
					5: saved RDI								<==== unwindSP points here
					6: code cache return address
					7: last arg									<==== unwindSP should point here
					*/
					return 2;
				} else {
					/* IA32 virtual resolve shape
					0: return address (picbuilder)
					1: indexAndLiteralsEA
					2: jitEIP
					3: saved eax									<== unwindSP points here
					4: saved esi
					5: saved edi
					6: return address (code cache)
					7: <last argument to method>			<== unwindSP should point here
					 */
					return 4;
				}
			} else if (J9ConfigFlags.arch_power) {
				/* PPC doesn't save anything extra */
				return 0;
			} else if (TRBuildFlags.host_SH4) {
				/* SH4 virtual resolve shape
				0: saved r7
				1: saved r6
				2: saved r5
				3: saved r4
				4: return address                <== unwindSP points here    
				5: saved resolved data
				6: <last argument to method>     <== unwindSP should point here
				*/
				return 2;
			} else {
				return 0;
			}
		}
		
		/** Number of slots pushed between the unwindSP of the JIT frame and the pushed resolve arguments for static/special method resolves.

		   Include the slot for the pushed return address (if any) for the call from the codecache to the picbuilder.
		   Do not include the slot for return address for call from picbuilder to resolve helper.
		*/
		public int getJitStaticMethodResolvePushes() {
			if (J9ConfigFlags.arch_x86) {
				if (J9BuildFlags.env_data64) {
					/* AMD64 static resolve shape
					0: ret addr to picbuilder
					1: code cache return address		<==== unwindSP points here
					2: last arg									<==== unwindSP should point here
					 */
					return 1;
				} else {
					/* IA32 static resolve shape
					0: return address (picbuilder)
					1: jitEIP
					2: constant pool
					3: cpIndex
					4: return address (code cache)		<== unwindSP points here
					5: <last argument to method>			<== unwindSP should point here
					 */
					return 1;
				}
			} else {
				return 0;
			}
		}

		public void walkJITFrameSlotsForInternalPointers(WalkState walkState,  U8Pointer jitDescriptionCursor, UDATAPointer scanCursor, VoidPointer stackMap, J9JITStackAtlasPointer gcStackAtlas) throws CorruptDataException
		{
			UDATA registerMap;
			U8 numInternalPtrMapBytes, numDistinctPinningArrays, i;
			UDATA parmStackMap;
			VoidPointer internalPointerMap = VoidPointer.cast(gcStackAtlas.internalPointerMap());
			U8Pointer tempJitDescriptionCursor = U8Pointer.cast(internalPointerMap);
			I16 indexOfFirstInternalPtr;
			I16 offsetOfFirstInternalPtr;
			U8 internalPointersInRegisters = new U8(0);

			/* If this PC offset corresponds to the parameter map, it means
			 zero initialization of internal pointers/pinning arrays has not
			 been done yet as execution is still in the prologue sequence
			 that does the stack check failure check (can result in GC). Avoid
			 examining internal pointer map in this case with uninitialized
			 internal pointer autos, as calling GC in this case is problematic.
			 */

			parmStackMap = UDATAPointer.cast(tempJitDescriptionCursor).at(0);
			if (parmStackMap.eq(UDATA.cast(stackMap))) {
				return;
			}

			registerMap = new UDATA(getJitRegisterMap(walkState.jitInfo, stackMap));

			/* Skip over the parameter map address (used in above compare)
			 */
			tempJitDescriptionCursor = tempJitDescriptionCursor.add(UDATA.SIZEOF);

			swPrintf(walkState, 6, "Address {0}", tempJitDescriptionCursor.getHexAddress());

			numInternalPtrMapBytes = tempJitDescriptionCursor.at(0);
			tempJitDescriptionCursor = tempJitDescriptionCursor.add(1);

			if (alignStackMaps)
				tempJitDescriptionCursor = tempJitDescriptionCursor.add(1);

			swPrintf(walkState, 6, "Num internal ptr map bytes {0}", numInternalPtrMapBytes);

			indexOfFirstInternalPtr = new I16(U16Pointer.cast(tempJitDescriptionCursor).at(0));

			swPrintf(walkState, 6, "Address {0}", tempJitDescriptionCursor.getHexAddress());
			
			tempJitDescriptionCursor = tempJitDescriptionCursor.add(2);

			swPrintf(walkState, 6,"Index of first internal ptr {0}", indexOfFirstInternalPtr);

			offsetOfFirstInternalPtr = new I16(U16Pointer.cast(tempJitDescriptionCursor).at(0));

			swPrintf(walkState, 6, "Address {0}", tempJitDescriptionCursor.getHexAddress());

			tempJitDescriptionCursor = tempJitDescriptionCursor.add(2);

			swPrintf(walkState, 6, "Offset of first internal ptr {0}", offsetOfFirstInternalPtr);


			swPrintf(walkState, 6, "Address {0}", tempJitDescriptionCursor.getHexAddress());
			numDistinctPinningArrays = tempJitDescriptionCursor.at(0);
			tempJitDescriptionCursor = tempJitDescriptionCursor.add(1);

			swPrintf(walkState, 6, "Num distinct pinning arrays {0}", numDistinctPinningArrays);


			i = new U8(0);
			if ((registerMap.anyBitsIn(INTERNAL_PTR_REG_MASK)) && (!registerMap.eq(new UDATA(0xFADECAFEL))))
				internalPointersInRegisters = new U8(1);


			while (i.lt(numDistinctPinningArrays))
			{
				U8 currPinningArrayIndex = tempJitDescriptionCursor.at(0);
				tempJitDescriptionCursor = tempJitDescriptionCursor.add(1);
				U8 numInternalPtrsForArray = tempJitDescriptionCursor.at(0);
				tempJitDescriptionCursor = tempJitDescriptionCursor.add(1);
				PointerPointer currPinningArrayCursor = PointerPointer.cast(walkState.bp.addOffset(offsetOfFirstInternalPtr.add(currPinningArrayIndex.intValue() * UDATA.SIZEOF)));
				J9ObjectPointer oldPinningArrayAddress = J9ObjectPointer.cast(currPinningArrayCursor.at(0));
				J9ObjectPointer newPinningArrayAddress;
				IDATA displacement = new IDATA(0);


				swPrintf(walkState, 6, "Before object slot walk &address : {0} address : {1} bp {2} offset of first internal ptr {3}", 
						currPinningArrayCursor.getHexAddress(), 
						oldPinningArrayAddress.getHexAddress(), 
						walkState.bp.getHexAddress(), 
						offsetOfFirstInternalPtr);
				walkState.callBacks.objectSlotWalkFunction(walkState.walkThread, walkState, currPinningArrayCursor, VoidPointer.cast(currPinningArrayCursor));
				newPinningArrayAddress = J9ObjectPointer.cast( currPinningArrayCursor.at(0) );
				displacement = new IDATA( UDATA.cast(newPinningArrayAddress).sub(UDATA.cast(oldPinningArrayAddress)));
				walkState.slotIndex++;

				swPrintf(walkState, 6, "After object slot walk for pinning array with &address : {0} old address {1} new address {2} displacement {3}", 
						currPinningArrayCursor.getHexAddress(), 
						oldPinningArrayAddress.getHexAddress(), 
						newPinningArrayAddress.getHexAddress(), 
						displacement);


				swPrintf(walkState, 6, "For pinning array {0} num internal pointer stack slots {1}", currPinningArrayIndex, numInternalPtrsForArray);


				/* If base array was moved by a non zero displacement
				 */
				if (! displacement.eq(0))
				{
					U8 j = new U8(0);

					while (j.lt(numInternalPtrsForArray))
					{
						U8 internalPtrAuto = tempJitDescriptionCursor.at(0);
						tempJitDescriptionCursor = tempJitDescriptionCursor.add(1);
						
						PointerPointer currInternalPtrCursor = PointerPointer.cast(walkState.bp.addOffset(offsetOfFirstInternalPtr.add( internalPtrAuto.intValue() * UDATA.SIZEOF )));

						swPrintf(walkState, 6, "For pinning array {0} internal pointer auto {1} old address {2} displacement {3}", 
								currPinningArrayIndex, 
								internalPtrAuto, 
								currInternalPtrCursor.at(0).getHexAddress(), 
								displacement);

						/*
						 * If the internal pointer is non null and the base pinning array object
						 * was moved by a non zero displacement, then adjust the internal pointer
						 */
						if (currInternalPtrCursor.at(0).notNull())
						{
							IDATA internalPtrAddress = IDATA.cast(currInternalPtrCursor.at(0));
							internalPtrAddress = internalPtrAddress.add(displacement);


							swPrintf(walkState, 6, "For pinning array %d internal pointer auto %d new address %p\n", currPinningArrayIndex, internalPtrAuto, internalPtrAddress);
							//TODO can't replicate fix-up here
							//*currInternalPtrCursor = (J9Object *) internalPtrAddress;
						}
						j = j.add(1);
					}

					if (! internalPointersInRegisters.eq(0))
					{
						U8Pointer tempJitDescriptionCursorForRegs;
						U8 numInternalPtrRegMapBytes;
						U8 k;

						registerMap = registerMap.bitAnd(J9SW_REGISTER_MAP_MASK);
						

						swPrintf(walkState, 6, "\tJIT-RegisterMap = {0}", registerMap);
						tempJitDescriptionCursorForRegs = U8Pointer.cast(stackMap);
					
						/* Skip the register map and register save description word */
						tempJitDescriptionCursorForRegs = tempJitDescriptionCursorForRegs.add(8);

						if (((walkState.jitInfo.endPC().sub(walkState.jitInfo.startPC()).gte(new UDATA(65535)) || (alignStackMaps))))
						{
							tempJitDescriptionCursorForRegs = tempJitDescriptionCursorForRegs.add(8);
						}
						else
						{
							tempJitDescriptionCursorForRegs = tempJitDescriptionCursorForRegs.add(6);
						}
						numInternalPtrRegMapBytes = tempJitDescriptionCursorForRegs.at(0);
						tempJitDescriptionCursorForRegs = tempJitDescriptionCursorForRegs.add(1);
						numDistinctPinningArrays = tempJitDescriptionCursorForRegs.at(0);
						tempJitDescriptionCursorForRegs = tempJitDescriptionCursorForRegs.add(1);
						k = new U8(0);
						while (k.lt(numDistinctPinningArrays))
						{
							U8 currPinningArrayIndexForRegister = tempJitDescriptionCursorForRegs.at(0);
							tempJitDescriptionCursorForRegs = tempJitDescriptionCursorForRegs.add(1);
							U8 numInternalPtrRegsForArray = tempJitDescriptionCursorForRegs.at(0);
							tempJitDescriptionCursorForRegs = tempJitDescriptionCursorForRegs.add(1);
							j = new U8(0);

							if (currPinningArrayIndexForRegister.eq(currPinningArrayIndex))
							{
								int mapCursor;
								
								if (J9SW_REGISTER_MAP_WALK_REGISTERS_LOW_TO_HIGH) {
									mapCursor = 0;
								} else {
									mapCursor = (int)J9SW_POTENTIAL_SAVED_REGISTERS - 1;
								}

								while (j.lt(numInternalPtrRegsForArray))
								{
									U8 internalPtrRegNum = tempJitDescriptionCursorForRegs.at(0);
									tempJitDescriptionCursorForRegs = tempJitDescriptionCursorForRegs.add(1);
									PointerPointer internalPtrObject;
									IDATA internalPtrAddress;


									if (J9SW_REGISTER_MAP_WALK_REGISTERS_LOW_TO_HIGH) {
										internalPtrObject = PointerPointer.cast(walkState.registerEAs[mapCursor + internalPtrRegNum.intValue() - 1].at(0));
									} else {
										internalPtrObject = PointerPointer.cast(walkState.registerEAs[mapCursor + internalPtrRegNum.intValue()].at(0));
									}

									internalPtrAddress = IDATA.cast(internalPtrObject.at(0));

									swPrintf(walkState, 6, "Original internal pointer reg address {0}", internalPtrAddress);

									if (! internalPtrAddress.eq(0))
										internalPtrAddress = internalPtrAddress.add(displacement);


									swPrintf(walkState, 6, "Adjusted internal pointer reg to be address {0} (disp {1})\n", internalPtrAddress, displacement);

									//TODO can't replicate this write
									//*internalPtrObject = (J9Object *) internalPtrAddress;
									j = j.add(1);
								}


								break;
							}
							else
								tempJitDescriptionCursorForRegs = tempJitDescriptionCursorForRegs.add(numInternalPtrRegsForArray); /* skip over requisite number of bytes */

							k = k.add(1);
						}
					}
				}
				else
					tempJitDescriptionCursor = tempJitDescriptionCursor.add(numInternalPtrsForArray); /* skip over requisite number of bytes */

				i = i.add(1);
			}

		}

		public VoidPointer getStackAllocMapFromJitPC(J9JavaVMPointer javaVM, J9JITExceptionTablePointer methodMetaData, UDATA jitPC, VoidPointer curStackMap) throws CorruptDataException
		{
			VoidPointer stackMap;
			PointerPointer stackAllocMap;
			
			if (methodMetaData.gcStackAtlas().isNull()) {
				return VoidPointer.NULL;
			}
			
			if (curStackMap.notNull()) {
				stackMap = curStackMap;
			} else {
				stackMap = getStackMapFromJitPC(javaVM, methodMetaData, jitPC);
			}
			
			stackAllocMap = PointerPointer.cast(J9JITStackAtlasPointer.cast(methodMetaData.gcStackAtlas()).stackAllocMap());
			
			if (stackAllocMap.notNull()) {
				UDATA returnValue;
				
				if (UDATAPointer.cast(stackAllocMap.at(0)).eq(stackMap)) {
					return VoidPointer.NULL;
				}
				
				returnValue = UDATA.cast(stackAllocMap).add(UDATA.SIZEOF);
				
				return VoidPointer.cast(returnValue);
			}
			
			return VoidPointer.NULL;
		}

		public U8Pointer getJitStackSlots(J9JITExceptionTablePointer metaData,
				VoidPointer stackMap) throws CorruptDataException
		{
			U8Pointer cursor = GET_REGISTER_MAP_CURSOR(HAS_FOUR_BYTE_OFFSET(metaData), stackMap);
			
			if ( U32Pointer.cast(cursor).at(0).anyBitsIn(INTERNAL_PTR_REG_MASK) && getJitInternalPointerMap(getJitGCStackAtlas(metaData)).notNull() ) {
				cursor = cursor.add( cursor.add(4).at(0).add(1));
			}
			
			cursor = cursor.add(4);
			
			return cursor;
		}
		
		private U8Pointer GET_REGISTER_MAP_CURSOR(boolean fourByteOffset, VoidPointer stackMap) 
		{
			return U8Pointer.cast(stackMap).add(SIZEOF_MAP_OFFSET(fourByteOffset).add(U32.SIZEOF * 2));
		}

		private U32 getNumInlinedCallSites(J9JITExceptionTablePointer methodMetaData) throws CorruptDataException
		{
			U32 sizeOfInlinedCallSites, numInlinedCallSites = new U32(0);

			if (methodMetaData.inlinedCalls().notNull())
			{
				sizeOfInlinedCallSites = new U32(UDATA.cast(methodMetaData.gcStackAtlas()).sub(UDATA.cast(methodMetaData.inlinedCalls())));

				numInlinedCallSites = new U32(sizeOfInlinedCallSites.longValue() / sizeOfInlinedCallSiteArrayElement(methodMetaData).longValue());
			}
			return numInlinedCallSites;
		}
		
		private U8Pointer getInlinedCallSiteArrayElement(J9JITExceptionTablePointer methodMetaData, int cix) throws CorruptDataException
		{
			U8Pointer inlinedCallSiteArray = U8Pointer.cast(getJitInlinedCallInfo(methodMetaData));
			if (inlinedCallSiteArray.notNull()) {
				return inlinedCallSiteArray.add(cix * sizeOfInlinedCallSiteArrayElement(methodMetaData).longValue() );
			}

			return U8Pointer.NULL;
		}
		
		private boolean isPatchedValue(J9MethodPointer m)
		{
			if ((J9ConfigFlags.arch_power && m.anyBitsIn(0x1))   ||  UDATA.cast(m).bitNot().eq(0)) {
				return true;
			}

			return false;
		}

		
		public void markClassesInInlineRanges(J9JITExceptionTablePointer methodMetaData, WalkState walkState)
				throws CorruptDataException
		{
			J9MethodPointer savedMethod = walkState.method;
			J9ConstantPoolPointer savedCP = walkState.constantPool;
			
			U32 numCallSites = getNumInlinedCallSites(methodMetaData);
			
			int i = 0;
			for (i=0; i<numCallSites.intValue(); i++)
			{
				U8Pointer inlinedCallSite = getInlinedCallSiteArrayElement(methodMetaData, i);
				J9MethodPointer inlinedMethod = J9MethodPointer.cast(getInlinedMethod(VoidPointer.cast(inlinedCallSite)));
				
				if (!isPatchedValue(inlinedMethod))
				{
					walkState.method = inlinedMethod;
					walkState.constantPool = UNTAGGED_METHOD_CP(walkState.method);
					
					WALK_METHOD_CLASS(walkState);
				}
			}

			walkState.method = savedMethod;
			walkState.constantPool = savedCP;

		}
		
	}

}
