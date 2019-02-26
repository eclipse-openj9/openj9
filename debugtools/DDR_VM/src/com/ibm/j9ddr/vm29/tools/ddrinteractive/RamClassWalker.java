/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.tools.ddrinteractive;

import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_ANNOTATION_UTF8;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_CLASS;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_DOUBLE;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_FIELD;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_FLOAT;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_HANDLE_METHOD;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_INSTANCE_METHOD;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_INT;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_INTERFACE_METHOD;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_LONG;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_METHOD_TYPE;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_METHODHANDLE;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_STATIC_METHOD;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_STRING;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_UNUSED;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_UNUSED8;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9_CP_BITS_PER_DESCRIPTION;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9_CP_DESCRIPTIONS_PER_U32;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9_CP_DESCRIPTION_MASK;
import static com.ibm.j9ddr.vm29.structure.J9FieldFlags.J9FieldFlagObject;
import static com.ibm.j9ddr.vm29.structure.J9FieldFlags.J9FieldSizeDouble;

import java.util.HashMap;
import java.util.Iterator;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.vm29.types.I64;
import com.ibm.j9ddr.vm29.j9.AlgorithmVersion;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.J9ObjectFieldOffset;
import com.ibm.j9ddr.vm29.j9.J9ObjectFieldOffsetIterator;
import com.ibm.j9ddr.vm29.pointer.I64Pointer;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.StructurePointer;
import com.ibm.j9ddr.vm29.pointer.U32Pointer;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.UDATAPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ArrayClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9ClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ConstantPoolPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ITablePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9RAMClassRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9RAMConstantRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9RAMFieldRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9RAMInterfaceMethodRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9RAMMethodHandleRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9RAMMethodRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9RAMMethodTypeRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9RAMStaticFieldRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9RAMStaticMethodRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9RAMStringRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMFieldShapePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VTableHeaderPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9ClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ROMClassHelper;
import com.ibm.j9ddr.vm29.structure.J9Class;
import com.ibm.j9ddr.vm29.structure.J9Consts;
import com.ibm.j9ddr.vm29.structure.J9ITable;
import com.ibm.j9ddr.vm29.structure.J9Method;
import com.ibm.j9ddr.vm29.structure.J9RAMClassRef;
import com.ibm.j9ddr.vm29.structure.J9RAMConstantPoolItem;
import com.ibm.j9ddr.vm29.structure.J9RAMConstantRef;
import com.ibm.j9ddr.vm29.structure.J9RAMFieldRef;
import com.ibm.j9ddr.vm29.structure.J9RAMInterfaceMethodRef;
import com.ibm.j9ddr.vm29.structure.J9RAMMethodHandleRef;
import com.ibm.j9ddr.vm29.structure.J9RAMMethodRef;
import com.ibm.j9ddr.vm29.structure.J9RAMMethodTypeRef;
import com.ibm.j9ddr.vm29.structure.J9RAMStaticMethodRef;
import com.ibm.j9ddr.vm29.structure.J9RAMStringRef;
import com.ibm.j9ddr.vm29.structure.J9ROMFieldOffsetWalkState;
import com.ibm.j9ddr.vm29.structure.J9VTableHeader;
import com.ibm.j9ddr.vm29.structure.J9JavaAccessFlags;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.IClassWalkCallbacks.SlotType;
import com.ibm.j9ddr.vm29.types.Scalar;
import com.ibm.j9ddr.vm29.types.U32;
import com.ibm.j9ddr.vm29.types.UDATA;

/**
 * Walk every slot and sections of a RAMClass
 * The sections are:<br>
 * -jit vTable<br>
 * -ramHeader<br>
 * -vTable<br>
 * -Extended method block<br>
 * -RAM methods<br>
 * -Constant Pool<br>
 * -Ram static<br>
 * -Superclasses<br>
 * -InstanceDescription<br>
 * -iTable
 * -StaticSplitTable<br>
 * -SpecialSplitTable<br>
 * @author jeanpb
 *
 */
public class RamClassWalker extends ClassWalker {

	private final J9ClassPointer ramClass;
	private final Context context;
	
	public RamClassWalker(StructurePointer clazz, Context context) {
		this.clazz = clazz;
		this.context = context;
		if (clazz instanceof J9ClassPointer) {
			this.ramClass = (J9ClassPointer)clazz;
		} else {
			this.ramClass = J9ClassPointer.NULL;
		}

		fillDebugExtMap();
	}
	
	public Context getContext() {
		return context;
	}
	
	public void allSlotsInObjectDo(IClassWalkCallbacks classWalker) throws CorruptDataException{

		this.classWalkerCallback = classWalker;
		if (ramClass.isNull()) {
			throw new CorruptDataException("The StructurePointer clazz is not an instance of J9ClassPointer");
		}
		this.classWalkerCallback = classWalker;
		
		allSlotsInJitVTablesDo();
		allSlotsInRAMHeaderDo();
		allSlotsInVTableDo();
		allSlotsInExtendedMethodBlockDo();
		// Padding
		allSlotsInRAMMethodsSectionDo();
		allSlotsInConstantPoolDo();
		allSlotsInRAMStaticsDo();
		allSlotsInRAMSuperClassesDo();
		allSlotsInInstanceDescriptionBitsDo();
		allSlotsIniTableDo();
		allSlotsInStaticSplitTableDo();
		allSlotsInSpecialSplitTableDo();
	}

	private void allSlotsInRAMSuperClassesDo() throws CorruptDataException {
		/* The classDepth represent the number of superclasses */
		final long classDepth = J9ClassHelper.classDepth(ramClass).longValue();

		/*
		 * The superclasses is an array, ramClass.superclasses() points to the
		 * head and we get the next superclasses by moving the pointer.
		 * superclasses.add(i) adds sizeof(UDATA) to the value of the
		 * superclass'pointer
		 */
		final PointerPointer superclasses = ramClass.superclasses();
		for (int i = 0; i < classDepth; i++) {
			classWalkerCallback.addSlot(clazz, SlotType.J9_UDATA, superclasses.add(i), "SuperClass address", "!j9class");
		}
		classWalkerCallback.addSection(clazz, superclasses, classDepth * PointerPointer.SIZEOF, "Superclasses", false);
	}
	
	private void allSlotsInStaticSplitTableDo() throws CorruptDataException {
		int count = ramClass.romClass().staticSplitMethodRefCount().intValue();
		PointerPointer splitTable = ramClass.staticSplitMethodTable();
		for (int i = 0; i < count; i++) {
			classWalkerCallback.addSlot(clazz, SlotType.J9_UDATA, splitTable.add(i), "J9Method address", "!j9method");
		}
		classWalkerCallback.addSection(clazz, splitTable, count * PointerPointer.SIZEOF, "StaticSplitTable", false);
	}
	
	private void allSlotsInSpecialSplitTableDo() throws CorruptDataException {
		int count = ramClass.romClass().specialSplitMethodRefCount().intValue();
		PointerPointer splitTable = ramClass.specialSplitMethodTable();
		for (int i = 0; i < count; i++) {
			classWalkerCallback.addSlot(clazz, SlotType.J9_UDATA, splitTable.add(i), "J9Method address", "!j9method");
		}
		classWalkerCallback.addSection(clazz, splitTable, count * PointerPointer.SIZEOF, "SpecialSplitTable", false);
	}

	private void allSlotsIniTableDo() throws CorruptDataException {
		J9ITablePointer iTable = J9ITablePointer.cast(ramClass.iTable());
		int interfaceSize = 0;
		final J9ITablePointer superclassITable;
		final J9ClassPointer superclass = J9ClassHelper.superclass(ramClass);
		if (superclass.isNull()) {
			superclassITable = J9ITablePointer.NULL;
		} else {
			superclassITable = J9ITablePointer.cast(superclass.iTable());
		}

		/*
		 * Loops through the linked list of interfaces until it hit it's
		 * superclass'itable
		 */
		while (!iTable.eq(superclassITable)) {
			classWalkerCallback.addSlot(clazz, SlotType.J9_UDATA, iTable.interfaceClassEA(), "iTable->interfaceClass", "!j9class");
			classWalkerCallback.addSlot(clazz, SlotType.J9_UDATA, iTable.nextEA(), "iTable->next", "!j9itable");

			/*
			 * The iTables for the interface classes do not contain entries for
			 * methods.
			 */
			if (!ramClass.romClass().modifiers().allBitsIn(J9JavaAccessFlags.J9AccInterface)) {
				int methodCount = iTable.interfaceClass().romClass().romMethodCount().intValue();
				for (int i = 0; i < methodCount; i++) {
					/*
					 * Add the method after the iTable. We add i + 1 to the
					 * iTable.nextEA() because we do not want to print the
					 * iTable.nextEA() but actually the value after it which 
					 * will be a method
					 */
					classWalkerCallback.addSlot(clazz, SlotType.J9_iTableMethod, iTable.nextEA().add(i + 1), "method", "!j9method");
					interfaceSize += UDATA.SIZEOF;
				}
			}
			iTable = iTable.next();
			interfaceSize += J9ITable.SIZEOF;
		}
		classWalkerCallback.addSection(clazz, ramClass.iTable(), interfaceSize, "iTable", false);
	}

	private void allSlotsInInstanceDescriptionBitsDo()
			throws CorruptDataException {

		/*
		 * The instanceDescription bits can be stored inline or can be a pointer
		 * to an array of instanceDescription bits. If the last bit is 1, the
		 * value is inline and this section is useless. If the last bit is 0
		 * then this section will display the array of instanceDescription bits.
		 */
		if (!ramClass.instanceDescription().allBitsIn(1)) {
			final int totalInstanceSize = ramClass.totalInstanceSize().intValue();
			
			final int fj9object_t_SizeOf =
				(J9BuildFlags.gc_compressedPointers ? U32.SIZEOF : UDATA.SIZEOF);
			final long totalSize = totalInstanceSize / fj9object_t_SizeOf;

			/* calculate number of slots required to store description bits */
			long shapeSlots;
			shapeSlots = (totalSize + (UDATA.SIZEOF * 8 - 1)) & (~(UDATA.SIZEOF * 8 - 1));
			shapeSlots = shapeSlots / (UDATA.SIZEOF * 8);

			for (int i = 0; i < shapeSlots; i++) {
				classWalkerCallback.addSlot(clazz, SlotType.J9_UDATA, ramClass.instanceDescription().add(i), "instanceDescription");
				classWalkerCallback.addSlot(clazz, SlotType.J9_UDATA, ramClass.instanceLeafDescription().add(i), "instanceLeafDescription");
			}
			/*
			 * We are using shapeSlots * 2 because we want one section for both
			 * instanceDescription and instanceLeafDescription
			 */
			classWalkerCallback.addSection(clazz, ramClass.instanceDescription(),
					shapeSlots * 2 * UDATA.SIZEOF, "InstanceDescription", false);
		}
	}

	private void allSlotsInConstantPoolDo() throws CorruptDataException {
		final J9ROMClassPointer romClass = ramClass.romClass();
		final int constPoolCount = romClass.ramConstantPoolCount().intValue();
		final J9ConstantPoolPointer cpp = J9ConstantPoolPointer.cast(ramClass.ramConstantPool());

		U32Pointer cpDescriptionSlots = J9ROMClassHelper.cpShapeDescription(romClass);
		PointerPointer cpEntry = PointerPointer.cast(ramClass.ramConstantPool());
		long cpDescription = 0;
		long cpEntryCount = ramClass.romClass().ramConstantPoolCount().longValue();
		long cpDescriptionIndex = 0;

		while (cpEntryCount > 0) {
			if (0 == cpDescriptionIndex) {
				// Load a new description word
				cpDescription = cpDescriptionSlots.at(0).longValue();
				cpDescriptionSlots = cpDescriptionSlots.add(1);
				cpDescriptionIndex = J9_CP_DESCRIPTIONS_PER_U32;
			}
			/*
			 * A switch statement can't be used on long type, it might be
			 * erroneous to cast it to an int as it might change in the future.
			 */
			long slotType = cpDescription & J9_CP_DESCRIPTION_MASK;
			
			if ((slotType == J9CPTYPE_STRING) || (slotType == J9CPTYPE_ANNOTATION_UTF8)) {
				J9RAMStringRefPointer ref = J9RAMStringRefPointer.cast(cpEntry);

				classWalkerCallback.addSlot(clazz, SlotType.J9_RAM_UTF8, ref.stringObjectEA(), "stringObject");
				classWalkerCallback.addSlot(clazz, SlotType.J9_UDATA, ref.unusedEA(), "unused");

				if (slotType == J9CPTYPE_STRING) {
					classWalkerCallback.addSection(clazz, ref, J9RAMStringRef.SIZEOF, "J9CPTYPE_STRING", false);
				} else {
					classWalkerCallback.addSection(clazz, ref, J9RAMStringRef.SIZEOF, "J9CPTYPE_ANNOTATION_UTF8", false);
				}
			} else if (slotType == J9CPTYPE_METHOD_TYPE) {
				J9RAMMethodTypeRefPointer ref = J9RAMMethodTypeRefPointer.cast(cpEntry);
				J9ObjectPointer slot = ref.type();
				if (slot.notNull()) {
					classWalkerCallback.addSlot(clazz, SlotType.J9_UDATA, ref.typeEA(), "type", "!j9object");
				} else {
					classWalkerCallback.addSlot(clazz, SlotType.J9_UDATA, ref.typeEA(), "type");
				}
				classWalkerCallback.addSlot(clazz, SlotType.J9_UDATA, ref.slotCountEA(), "slotCount");

				classWalkerCallback.addSection(clazz, ref, J9RAMMethodTypeRef.SIZEOF, "J9CPTYPE_METHOD_TYPE", false);
			} else if (slotType == J9CPTYPE_METHODHANDLE) {
				J9RAMMethodHandleRefPointer ref = J9RAMMethodHandleRefPointer.cast(cpEntry);
				J9ObjectPointer slot = ref.methodHandle();
				if (slot.notNull()) {
					classWalkerCallback.addSlot(clazz, SlotType.J9_UDATA, ref.methodHandleEA(), "methodHandle", "!j9object");
				} else {
					classWalkerCallback.addSlot(clazz, SlotType.J9_UDATA, ref.methodHandleEA(), "methodHandle");
				}
				classWalkerCallback.addSlot(clazz, SlotType.J9_UDATA, ref.unusedEA(), "unused");
				classWalkerCallback.addSection(clazz, ref, J9RAMMethodHandleRef.SIZEOF, "J9CPTYPE_METHODHANDLE", false);

			} else if (slotType == J9CPTYPE_CLASS) {
				J9RAMClassRefPointer ref = J9RAMClassRefPointer.cast(cpEntry);
				if (ref.value().notNull()) {
					classWalkerCallback.addSlot(clazz, SlotType.J9_UDATA, ref.valueEA(), "value", "!j9class");
				} else {
					classWalkerCallback.addSlot(clazz, SlotType.J9_UDATA, ref.valueEA(), "value");
				}
				classWalkerCallback.addSlot(clazz, SlotType.J9_UDATA, ref.modifiersEA(), "modifiers");
				classWalkerCallback.addSection(clazz, ref, J9RAMClassRef.SIZEOF, "J9CPTYPE_CLASS", false);
			} else if (slotType == J9CPTYPE_INT) {
				J9RAMConstantRefPointer ref = J9RAMConstantRefPointer.cast(cpEntry);
				
				classWalkerCallback.addSlot(clazz, SlotType.J9_UDATA, ref.slot1EA(), "cpFieldInt");
				classWalkerCallback.addSlot(clazz, SlotType.J9_UDATA, ref.slot2EA(), "cpFieldIntUnused");
				classWalkerCallback.addSection(clazz, ref, J9RAMConstantRef.SIZEOF, "J9CPTYPE_INT", false);
			} else if (slotType == J9CPTYPE_FLOAT) {
				J9RAMConstantRefPointer ref = J9RAMConstantRefPointer.cast(cpEntry);
				
				classWalkerCallback.addSlot(clazz, SlotType.J9_UDATA, ref.slot1EA(), "cpFieldFloat");
				classWalkerCallback.addSlot(clazz, SlotType.J9_UDATA, ref.slot2EA(), "cpFieldIntUnused");
				classWalkerCallback.addSection(clazz, ref, J9RAMConstantRef.SIZEOF, "J9CPTYPE_FLOAT", false);
			} else if (slotType == J9CPTYPE_LONG) {
				J9RAMConstantRefPointer ref = J9RAMConstantRefPointer.cast(cpEntry);
				
				classWalkerCallback.addSlot(clazz, SlotType.J9_I64, I64Pointer.cast(cpEntry), "J9CPTYPE_LONG");
				classWalkerCallback.addSection(clazz, ref, J9RAMConstantRef.SIZEOF, "J9CPTYPE_LONG", false);
			} else if (slotType == J9CPTYPE_DOUBLE) {
				J9RAMConstantRefPointer ref = J9RAMConstantRefPointer.cast(cpEntry);
				
				classWalkerCallback.addSlot(clazz, SlotType.J9_I64, I64Pointer.cast(cpEntry), "J9CPTYPE_DOUBLE");
				classWalkerCallback.addSection(clazz, ref, I64.SIZEOF, "J9CPTYPE_DOUBLE", false);
			} else if (slotType == J9CPTYPE_FIELD) {
				J9RAMFieldRefPointer ref = J9RAMFieldRefPointer.cast(cpEntry);
				J9RAMStaticFieldRefPointer staticRef = J9RAMStaticFieldRefPointer.cast(cpEntry);
				
				/* if the field ref is resolved static, it has 'flagsAndClass', for other cases (unresolved and resolved instance) it has 'flags'. */
				if ((staticRef.flagsAndClass().longValue() > 0) && !staticRef.valueOffset().eq(UDATA.MAX)) {
					classWalkerCallback.addSlot(clazz, SlotType.J9_IDATA, staticRef.flagsAndClassEA(), "flagsAndClass");
				} else {
					classWalkerCallback.addSlot(clazz, SlotType.J9_UDATA, ref.flagsEA(), "flags");
				}
				classWalkerCallback.addSlot(clazz, SlotType.J9_UDATA, ref.valueOffsetEA(), "valueOffset");
				classWalkerCallback.addSection(clazz, ref, J9RAMFieldRef.SIZEOF, "J9CPTYPE_FIELD", false);
				
			} else if (slotType == J9CPTYPE_INTERFACE_METHOD) {
				J9RAMInterfaceMethodRefPointer ref = J9RAMInterfaceMethodRefPointer.cast(cpEntry);
				classWalkerCallback.addSlot(clazz, SlotType.J9_UDATA, ref.interfaceClassEA(), "interfaceClass", "!j9class");
				classWalkerCallback.addSlot(clazz, SlotType.J9_UDATA, ref.methodIndexAndArgCountEA(), "methodIndexAndArgCount");
				
				classWalkerCallback.addSection(clazz, ref, J9RAMInterfaceMethodRef.SIZEOF, "J9CPTYPE_INTERFACE_METHOD", false);
			} else if (slotType == J9CPTYPE_STATIC_METHOD) {
				J9RAMStaticMethodRefPointer ref = J9RAMStaticMethodRefPointer.cast(cpEntry);
				classWalkerCallback.addSlot(clazz, SlotType.J9_UDATA, ref.methodEA(), "method", "!j9method");
				classWalkerCallback.addSlot(clazz, SlotType.J9_UDATA, ref.methodIndexAndArgCountEA(), "unused");
			
				classWalkerCallback.addSection(clazz, ref, J9RAMStaticMethodRef.SIZEOF, "J9CPTYPE_STATIC_METHOD", false);
			} else if ((slotType == J9CPTYPE_UNUSED) || (slotType == J9CPTYPE_UNUSED8)) {
				classWalkerCallback.addSlot(clazz, SlotType.J9_UDATA, cpEntry, "unused");
				classWalkerCallback.addSlot(clazz, SlotType.J9_UDATA, cpEntry.add(1), "unused");
				classWalkerCallback.addSection(clazz, cpEntry, 2 * UDATA.SIZEOF, "J9CPTYPE_UNUSED", false);
			} else if (slotType == J9CPTYPE_INSTANCE_METHOD) {
				J9RAMMethodRefPointer ref = J9RAMMethodRefPointer.cast(cpEntry);
				classWalkerCallback.addSlot(clazz, SlotType.J9_UDATA, ref.methodIndexAndArgCountEA(), "methodIndexAndArgCount");
				classWalkerCallback.addSlot(clazz, SlotType.J9_UDATA, ref.methodEA(), "method", "!j9method");
				classWalkerCallback.addSection(clazz, ref, J9RAMMethodRef.SIZEOF, "J9CPTYPE_INSTANCE_METHOD", false);
			} else if (slotType == J9CPTYPE_HANDLE_METHOD) {
				J9RAMMethodRefPointer ref = J9RAMMethodRefPointer.cast(cpEntry);
				classWalkerCallback.addSlot(clazz, SlotType.J9_UDATA, ref.methodIndexAndArgCountEA(), "methodTypeIndexAndArgCount");
				classWalkerCallback.addSlot(clazz, SlotType.J9_UDATA, ref.methodEA(), "unused");
	
				classWalkerCallback.addSection(clazz, ref, J9RAMMethodRef.SIZEOF, "J9CPTYPE_HANDLE_METHOD", false);
			}
			cpEntry = cpEntry.addOffset(J9RAMConstantPoolItem.SIZEOF);
			cpEntryCount -= 1;
			cpDescription >>= J9_CP_BITS_PER_DESCRIPTION;
			cpDescriptionIndex -= 1;
		}
		// The spaces at the end of "Constant Pool" are important since the
		// regions are sorted
		// by address and size, but when they are equal they are sorted by
		// longest name and "Constant Pool" has to come first
		classWalkerCallback.addSection(clazz, cpp, constPoolCount * 2 * UDATA.SIZEOF, "Constant Pool               ", false);
	}

	private void allSlotsInRAMStaticsDo() throws CorruptDataException {
		if (ramClass.ramStatics().isNull()) {
			return;
		}
		Iterator<?> ofoIterator = J9ObjectFieldOffsetIterator.J9ObjectFieldOffsetIteratorFor(ramClass, J9ClassHelper.superclass(ramClass), new U32(J9ROMFieldOffsetWalkState.J9VM_FIELD_OFFSET_WALK_INCLUDE_STATIC));
		J9ObjectFieldOffset fields = null;
		while (ofoIterator.hasNext()) {
			fields = (J9ObjectFieldOffset) ofoIterator.next();
			J9ROMFieldShapePointer field = fields.getField();
			
			String info = fields.getName();
			UDATA modifiers = field.modifiers();
			UDATAPointer fieldAddress = ramClass.ramStatics().addOffset(fields.getOffsetOrAddress());
			
			String additionalInfo = modifiers.anyBitsIn(J9FieldFlagObject) ? "!j9object" : "";
			
			if (modifiers.anyBitsIn(J9FieldSizeDouble)) {
				classWalkerCallback.addSlot(clazz, SlotType.J9_I64, I64Pointer.cast(fieldAddress), info, additionalInfo);
			} else {
				classWalkerCallback.addSlot(clazz, SlotType.J9_UDATA, fieldAddress, info, additionalInfo);
			}
		}
		
		UDATA staticSlotCount = ramClass.romClass().objectStaticCount().add(ramClass.romClass().singleScalarStaticCount());
		if (J9BuildFlags.env_data64) {
			staticSlotCount = staticSlotCount.add(ramClass.romClass().doubleScalarStaticCount());
		} else {
			staticSlotCount = staticSlotCount.add(1).bitAnd(~1L).add(ramClass.romClass().doubleScalarStaticCount().mult(2));
		}
		UDATA size = Scalar.convertSlotsToBytes(new UDATA(staticSlotCount));
		classWalkerCallback.addSection(clazz, ramClass.ramStatics(), size.longValue(), "Ram static", false);
	}

	private void allSlotsInRAMMethodsSectionDo() throws CorruptDataException {
		final int methodCount = ramClass.romClass().romMethodCount().intValue();

		if (methodCount != 0) {
			final J9MethodPointer methods = ramClass.ramMethods();

			for (int i = 0; i < methodCount; i++) {
				final J9MethodPointer method = methods.add(i);
				classWalkerCallback.addSection(clazz, method, J9Method.SIZEOF, "J9Method", false);
				addObjectsasSlot(method);
			}
			classWalkerCallback.addSection(clazz, ramClass.ramMethods(), methodCount * J9Method.SIZEOF, "RAM methods", false);
		}
	}

	private void allSlotsInExtendedMethodBlockDo() throws CorruptDataException {
		final J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());

		/* add in size of method trace array if extended method block active */
		if(vm.runtimeFlags().allBitsIn(J9Consts.J9_RUNTIME_EXTENDED_METHOD_BLOCK)) {
			long extendedMethodBlockSize = 0;
			int romMethodCount = ramClass.romClass().romMethodCount().intValue();
			extendedMethodBlockSize = romMethodCount + (UDATA.SIZEOF - 1);
			extendedMethodBlockSize &= ~(UDATA.SIZEOF - 1);
		
			/*
			 * Gets the start address of the extended methods. It is just before the ramMethods.
			 */
			U8Pointer extendedMethodStartAddr = U8Pointer.cast(ramClass.ramMethods()).sub(extendedMethodBlockSize);
			classWalkerCallback.addSection(clazz,extendedMethodStartAddr, romMethodCount, "Extended method block", false);
			
			while (romMethodCount-- > 0) {
				classWalkerCallback.addSlot(clazz, SlotType.J9_U8, extendedMethodStartAddr.add(romMethodCount), "method flag", "!j9extendedmethodflaginfo");
			}
		}
	}

	private void allSlotsInVTableDo() throws CorruptDataException {
		/* Check vTable algorithm version */
		if (AlgorithmVersion.getVersionOf(AlgorithmVersion.VTABLE_VERSION).getAlgorithmVersion() >= 1) {
			J9VTableHeaderPointer vTableHeader = J9ClassHelper.vTableHeader(ramClass);
			final long vTableMethods = vTableHeader.size().longValue();
			long vTableSize = UDATA.SIZEOF;
			UDATAPointer vTable = J9ClassHelper.vTable(vTableHeader);

			classWalkerCallback.addSlot(clazz, SlotType.J9_UDATA, vTableHeader, "VTable size");
			
			if (vTableMethods > 0) {
				vTableSize = J9VTableHeader.SIZEOF + (vTableMethods * UDATA.SIZEOF);
				/* First print the vTableHeader default methods */
				classWalkerCallback.addSlot(clazz, SlotType.J9_UDATA, vTableHeader.initialVirtualMethod(), "magic method", "!j9method");
				for (int i = 0; i < vTableMethods; i++) {
					classWalkerCallback.addSlot(clazz, SlotType.J9_UDATA, vTable.add(i), "method", "!j9method");
				}
			}
			classWalkerCallback.addSection(clazz, vTableHeader, vTableSize, "vTable", true);
		} else {
			UDATAPointer vTable = J9ClassHelper.oldVTable(ramClass);
			final long vTableSlots = Math.max(1,vTable.at(0).longValue());

			classWalkerCallback.addSlot(clazz, SlotType.J9_UDATA, vTable, "VTable size");

			for (int i = 1; i <= vTableSlots; i++) {
				/*
				 * The first method is a magic method. It's enough of a j9method to
				 * be able to run the resolve code for unresolved virtual references
				 * it doesn't have a rom method associated with it, so there's no
				 * name, etc. The only information about that method is the methodRunAddress.
				 */
				classWalkerCallback.addSlot(clazz, SlotType.J9_UDATA, vTable.add(i), ((i == 1) ? "magic " : "") + "method", "!j9method");
			}
			classWalkerCallback.addSection(clazz, vTable, vTableSlots * UDATA.SIZEOF, "vTable", true);
		}
		
	}

	private void allSlotsInJitVTablesDo() throws CorruptDataException {
		J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
		if(!vm.jitConfig().isNull()) {
			long jitvTableSlots;
			long jitvTableSlotsLength;

			/* Check vTable algorithm version */
			if (AlgorithmVersion.getVersionOf(AlgorithmVersion.VTABLE_VERSION).getAlgorithmVersion() >= 1) {
				jitvTableSlots = J9ClassHelper.vTableHeader(ramClass).size().longValue();
				jitvTableSlotsLength = ((jitvTableSlots - 1) * UDATA.SIZEOF) + J9VTableHeader.SIZEOF;
			} else {
				jitvTableSlots = J9ClassHelper.oldVTable(ramClass).at(0).longValue();
				jitvTableSlotsLength = jitvTableSlots * UDATA.SIZEOF;
			}
			
			for (int i = 0; i < jitvTableSlots; i++) {
				classWalkerCallback.addSlot(clazz, SlotType.J9_UDATA, UDATAPointer.cast(ramClass.subOffset(jitvTableSlotsLength)).add(i), "send target", "");
			}
			
			/*
			 * The vTable is before the RAMClass, to get the starting address,
			 * the length of the vTable is subtracted from the pointer of the
			 * class
			 */
			classWalkerCallback.addSection(clazz, ramClass.subOffset(jitvTableSlotsLength), jitvTableSlotsLength, "jitVTables", true);
		}
	}

	private void allSlotsInRAMHeaderDo() throws CorruptDataException {
		classWalkerCallback.addSection(clazz, clazz, J9Class.SIZEOF, "ramHeader", true);
		
		if (J9ClassHelper.isArrayClass(ramClass)) {
			addObjectsasSlot(J9ArrayClassPointer.cast(ramClass));
		} else {
			if (J9ClassHelper.isSwappedOut(ramClass)) {
				HashMap<String, String> renameFields = new HashMap<String, String>();
				renameFields.put("arrayClass", "currentClass");
				addObjectsAsSlot(ramClass, renameFields);
			}
			addObjectsasSlot(ramClass);
		}
	}
	
	@Override
	protected void fillDebugExtMap() {
		debugExtMap.put("romClass", "!dumpromclasslinear");
		debugExtMap.put("classLoader", "!j9classloader");
		debugExtMap.put("classObject", "!j9vmjavalangclass");
		debugExtMap.put("ramMethods", "!j9method");
		debugExtMap.put("arrayClass", "!j9class");
		debugExtMap.put("initializerCache", "!j9method");
		debugExtMap.put("subclassTraversalLink", "!j9class");
		debugExtMap.put("replacedClass", "!j9class");
		debugExtMap.put("constantPool", "!j9constantpool");
		debugExtMap.put("bytecodes", "!bytecodes");
	}
}
