/*
 * Copyright IBM Corp. and others 2001
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package com.ibm.j9ddr.vm29.tools.ddrinteractive;

import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_ANNOTATION_UTF8;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_CLASS;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_CONSTANT_DYNAMIC;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_DOUBLE;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_FIELD;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_FLOAT;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_HANDLE_METHOD;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_INSTANCE_METHOD;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_INT;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_INTERFACE_INSTANCE_METHOD;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_INTERFACE_METHOD;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_INTERFACE_STATIC_METHOD;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_LONG;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_METHODHANDLE;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_METHOD_TYPE;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_STATIC_METHOD;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_STRING;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_UNUSED;
import static com.ibm.j9ddr.vm29.structure.J9ConstantPool.J9CPTYPE_UNUSED8;
import static com.ibm.j9ddr.vm29.structure.J9FieldFlags.J9FieldFlagConstant;
import static com.ibm.j9ddr.vm29.structure.J9FieldFlags.J9FieldFlagHasFieldAnnotations;
import static com.ibm.j9ddr.vm29.structure.J9FieldFlags.J9FieldFlagHasGenericSignature;
import static com.ibm.j9ddr.vm29.structure.J9FieldFlags.J9FieldSizeDouble;
import static com.ibm.j9ddr.vm29.structure.J9RecordComponentFlags.J9RecordComponentFlagHasGenericSignature;
import static com.ibm.j9ddr.vm29.structure.J9RecordComponentFlags.J9RecordComponentFlagHasAnnotations;
import static com.ibm.j9ddr.vm29.structure.J9RecordComponentFlags.J9RecordComponentFlagHasTypeAnnotations;

import java.nio.ByteOrder;

import com.ibm.j9ddr.vm29.structure.J9NonbuilderConstants;
import com.ibm.j9ddr.vm29.pointer.helper.ValueTypeHelper;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.corereaders.memory.IProcess;
import com.ibm.j9ddr.corereaders.memory.MemoryFault;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.vm29.j9.AlgorithmVersion;
import com.ibm.j9ddr.vm29.j9.BCNames;
import static com.ibm.j9ddr.vm29.j9.BCNames.*;
import com.ibm.j9ddr.vm29.j9.ConstantPoolHelpers;
import com.ibm.j9ddr.vm29.j9.J9ROMFieldShapeIterator;
import com.ibm.j9ddr.vm29.j9.OptInfo;
import com.ibm.j9ddr.vm29.j9.ROMHelp;
import com.ibm.j9ddr.vm29.j9.walkers.LocalVariableTable;
import com.ibm.j9ddr.vm29.j9.walkers.LocalVariableTableIterator;
import com.ibm.j9ddr.vm29.pointer.I32Pointer;
import com.ibm.j9ddr.vm29.pointer.I64Pointer;
import com.ibm.j9ddr.vm29.pointer.Pointer;
import com.ibm.j9ddr.vm29.pointer.PointerPointer;
import com.ibm.j9ddr.vm29.pointer.SelfRelativePointer;
import com.ibm.j9ddr.vm29.pointer.StructurePointer;
import com.ibm.j9ddr.vm29.pointer.U16Pointer;
import com.ibm.j9ddr.vm29.pointer.U32Pointer;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.VoidPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9EnclosingObjectPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ExceptionHandlerPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ExceptionInfoPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9LineNumberPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodDebugInfoPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodParameterPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodParametersDataPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMArrayClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMClassPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMConstantDynamicRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMConstantPoolItemPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMFieldRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMFieldShapePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMMethodHandleRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMMethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMMethodRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMMethodTypeRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMNameAndSignaturePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMRecordComponentShapePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9ROMStringRefPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9SourceDebugExtensionPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9MethodDebugInfoHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ROMClassHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9ROMMethodHelper;
import com.ibm.j9ddr.vm29.structure.J9EnclosingObject;
import com.ibm.j9ddr.vm29.structure.J9ExceptionHandler;
import com.ibm.j9ddr.vm29.structure.J9ExceptionInfo;
import com.ibm.j9ddr.vm29.structure.J9MethodDebugInfo;
import com.ibm.j9ddr.vm29.structure.J9ROMClass;
import com.ibm.j9ddr.vm29.structure.J9ROMConstantPoolItem;
import com.ibm.j9ddr.vm29.structure.J9ROMRecordComponentShape;
import com.ibm.j9ddr.vm29.structure.J9SourceDebugExtension;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.IClassWalkCallbacks.SlotType;
import com.ibm.j9ddr.vm29.types.I32;
import com.ibm.j9ddr.vm29.types.U16;
import com.ibm.j9ddr.vm29.types.U32;
import com.ibm.j9ddr.vm29.types.U64;
import com.ibm.j9ddr.vm29.types.UDATA;

/**
 * Walk every slot and sections of a ROMClass
 *
 * @author jeanpb
 */
public class RomClassWalker extends ClassWalker {

	private final J9ROMClassPointer romClass;
	private final Context context;
	public static final int CFR_STACKMAP_TYPE_OBJECT = 0x07;
	public static final int CFR_STACKMAP_SAME = 0;
	public static final int CFR_STACKMAP_SAME_LOCALS_1_STACK = 64;
	public static final int CFR_STACKMAP_SAME_LOCALS_1_STACK_END = 128;
	public static final int CFR_STACKMAP_SAME_LOCALS_1_STACK_EXTENDED = 247;
	public static final int CFR_STACKMAP_SAME_EXTENDED = 251;
	public static final int CFR_STACKMAP_APPEND_BASE = 251;
	public static final int CFR_STACKMAP_FULL = 255;

	public RomClassWalker(StructurePointer clazz, Context context) {
		this.clazz = clazz;
		this.context = context;
		if (clazz instanceof J9ROMClassPointer) {
			this.romClass = (J9ROMClassPointer) clazz;
		} else {
			this.romClass = J9ROMClassPointer.NULL;
		}
	}

	public Context getContext() {
		return context;
	}

	public void allSlotsInObjectDo(IClassWalkCallbacks classWalker) throws CorruptDataException {
		this.classWalkerCallback = classWalker;
		if (null == romClass) {
			throw new CorruptDataException("The StructurePointer clazz is not an instance of J9ClassPointer");
		}
		this.classWalkerCallback = classWalker;

		allSlotsInROMHeaderDo();
		allSlotsInConstantPoolDo();
		allSlotsInROMMethodsSectionDo();
		allSlotsInROMFieldsSectionDo();
		allSlotsInCPShapeDescriptionDo();
		allSlotsInOptionalInfoDo();
		allSlotsInVarHandleMethodTypeLookupTableDo();
		allSlotsInStaticSplitMethodRefIndexesDo();
		allSlotsInSpecialSplitMethodRefIndexesDo();
	}

	private void allSlotsInROMHeaderDo() throws CorruptDataException {
		classWalkerCallback.addSection(clazz, clazz, J9ROMClass.SIZEOF, "romHeader", true);

		/* walk the immediate fields in the J9ROMClass */
		if (J9ROMClassHelper.isArray(romClass)) {
			addObjectsasSlot(J9ROMArrayClassPointer.cast(romClass));
			return;
		} else {
			addObjectsasSlot(romClass);
		}

		/* walk interfaces SRPs block */
		SelfRelativePointer srpCursor = romClass.interfaces();
		long count = romClass.interfaceCount().longValue();
		classWalkerCallback.addSection(clazz, srpCursor, count * SelfRelativePointer.SIZEOF, "interfacesSRPs", true);
		for (int i = 0; i < count; i++) {
			classWalkerCallback.addSlot(clazz, SlotType.J9_ROM_UTF8, srpCursor, "interfaceUTF8");
			srpCursor = srpCursor.add(1);
		}

		/* walk inner classes SRPs block */
		srpCursor = romClass.innerClasses();
		count = romClass.innerClassCount().intValue();
		classWalkerCallback.addSection(clazz, srpCursor, count * SelfRelativePointer.SIZEOF, "innerClassesSRPs", true);
		for (; 0 != count; count--) {
			classWalkerCallback.addSlot(clazz, SlotType.J9_ROM_UTF8, srpCursor, "innerClassNameUTF8");
			srpCursor = srpCursor.add(1);
		}

		try {
			/* walk enclosed inner classes SRPs block */
			srpCursor = romClass.enclosedInnerClasses();
			count = romClass.enclosedInnerClassCount().intValue();
			classWalkerCallback.addSection(clazz, srpCursor, count * SelfRelativePointer.SIZEOF, "enclosedInnerClassesSRPs", true);
			for (; 0 != count; count--) {
				classWalkerCallback.addSlot(clazz, SlotType.J9_ROM_UTF8, srpCursor, "enclosedInnerClassesNameUTF8");
				srpCursor = srpCursor.add(1);
			}
		} catch (NoSuchFieldException e) {
			// ignore: VM that generated the core dump doesn't have enclosedInnerClasses or enclosedInnerClassCount
		}

		/* add CP NAS section */
		J9ROMMethodPointer firstMethod = romClass.romMethods();
		long size = (firstMethod.getAddress() - srpCursor.getAddress());
		classWalkerCallback.addSection(clazz, srpCursor, size, "cpNamesAndSignaturesSRPs", true);
	}

	private void allSlotsInROMMethodsSectionDo() throws CorruptDataException {
		int count;
		J9ROMMethodPointer firstMethod;
		J9ROMMethodPointer nextMethod;
		J9ROMMethodPointer methodCursor;

		methodCursor = firstMethod = romClass.romMethods();
		for (count = romClass.romMethodCount().intValue(); (methodCursor.notNull()) && (count > 0); count--) {
			nextMethod = allSlotsInROMMethodDo(methodCursor);
			classWalkerCallback.addSection(clazz, methodCursor, nextMethod.getAddress() - methodCursor.getAddress(), "method", true);
			methodCursor = nextMethod;
		}
		classWalkerCallback.addSection(clazz, firstMethod, methodCursor.getAddress() - firstMethod.getAddress(), "methods", true);
	}

	private void allSlotsInROMFieldsSectionDo() throws CorruptDataException
	{
		J9ROMFieldShapeIterator iterator = new J9ROMFieldShapeIterator(romClass.romFields(), romClass.romFieldCount());
		int size = 0;
		while (iterator.hasNext()) {
			J9ROMFieldShapePointer currentField = (J9ROMFieldShapePointer) iterator.next();
			int fieldLength = allSlotsInROMFieldDo(currentField);
			if (0 == fieldLength) {
				/* Range validation failed on the field, break early. */
				break;
			}
			size += fieldLength;
		}
		classWalkerCallback.addSection(clazz, romClass.romFields(), size, "fields", true);
	}

	private J9ROMMethodPointer allSlotsInROMMethodDo(J9ROMMethodPointer method) throws CorruptDataException
	{
		U32Pointer cursor;

		addObjectsasSlot(method);

		cursor = ROMHelp.J9_EXTENDED_MODIFIERS_ADDR_FROM_ROM_METHOD(method);

		allSlotsInBytecodesDo(method);

		if (J9ROMMethodHelper.hasExtendedModifiers(method)) {
			classWalkerCallback.addSlot(clazz, SlotType.J9_U32, cursor, "extendedModifiers");
			cursor = cursor.add(1);
		}

		if (J9ROMMethodHelper.hasGenericSignature(method)) {
			classWalkerCallback.addSlot(clazz, SlotType.J9_ROM_UTF8, cursor, "methodUTF8");
			cursor = cursor.add(1);
		}

		if (J9ROMMethodHelper.hasExceptionInfo(method)) {
			J9ExceptionInfoPointer exceptionInfo = J9ExceptionInfoPointer.cast(cursor);
			long exceptionInfoSize = J9ExceptionInfo.SIZEOF +
				exceptionInfo.catchCount().longValue() * J9ExceptionHandler.SIZEOF +
				exceptionInfo.throwCount().longValue() * SelfRelativePointer.SIZEOF;

			allSlotsInExceptionInfoDo(exceptionInfo);
			cursor = cursor.addOffset(exceptionInfoSize);
		}

		if (J9ROMMethodHelper.hasMethodAnnotations(method)) {
			cursor = cursor.add(allSlotsInAnnotationDo(cursor, "methodAnnotation"));
		}

		if (J9ROMMethodHelper.hasParameterAnnotations(method)) {
			cursor = cursor.add(allSlotsInAnnotationDo(cursor, "parameterAnnotations"));
		}

		if (J9ROMMethodHelper.hasMethodTypeAnnotations(method)) {
			cursor = cursor.add(allSlotsInAnnotationDo(cursor, "method typeAnnotations"));
		}

		if (J9ROMMethodHelper.hasCodeTypeAnnotations(method)) {
			cursor = cursor.add(allSlotsInAnnotationDo(cursor, "code typeAnnotations"));
		}

		if (J9ROMMethodHelper.hasDefaultAnnotation(method)) {
			cursor = cursor.add(allSlotsInAnnotationDo(cursor, "defaultAnnotation"));
		}

		if (J9ROMMethodHelper.hasDebugInfo(method)) {
			cursor = cursor.add(allSlotsInMethodDebugInfoDo(cursor));
		}

		if (J9ROMMethodHelper.hasStackMap(method)) {
			long stackMapSize = cursor.at(0).longValue();
			U8Pointer stackMap = U8Pointer.cast(cursor.add(1));
			classWalkerCallback.addSection(clazz, cursor, stackMapSize, "stackMap", true);
			classWalkerCallback.addSlot(clazz, SlotType.J9_U32, cursor, "stackMapSize");
			allSlotsInStackMapDo(stackMap);

			cursor = cursor.addOffset(stackMapSize);
		}

		if (J9ROMMethodHelper.hasMethodParameters(method)) {
			cursor = cursor.add(allSlotsInMethodParametersDataDo(cursor));
		}

		return J9ROMMethodPointer.cast(cursor);
	}

	private long allSlotsInMethodParametersDataDo(U32Pointer cursor) throws CorruptDataException
	{
		J9MethodParametersDataPointer methodParametersData = J9MethodParametersDataPointer.cast(cursor);
		J9MethodParameterPointer parameters = methodParametersData.parameters();
		long methodParametersSize = ROMHelp.J9_METHOD_PARAMS_SIZE_FROM_NUMBER_OF_PARAMS(methodParametersData.parameterCount().longValue());
		long padding = U32.SIZEOF - (methodParametersSize % U32.SIZEOF);
		long size = 0;

		if (padding == U32.SIZEOF) {
			padding = 0;
		}

		size = methodParametersSize + padding;

		classWalkerCallback.addSlot(clazz, SlotType.J9_SRP, methodParametersData.parameterCountEA(), "parameterCount");

		for (int i = 0; i < methodParametersData.parameterCount().longValue(); i++) {
			classWalkerCallback.addSlot(clazz, SlotType.J9_ROM_UTF8, parameters.nameEA(), "methodParameterName");
			classWalkerCallback.addSlot(clazz, SlotType.J9_U16, parameters.flagsEA(), "methodParameterFlag");
		}

		cursor = cursor.addOffset(methodParametersSize);
		for (; padding > 0; padding--) {
			classWalkerCallback.addSlot(clazz, SlotType.J9_U8, cursor, "MethodParameters padding");
			cursor.addOffset(1);
		}

		classWalkerCallback.addSection(clazz, methodParametersData, size, "Method Parameters", true);
		return size/U32.SIZEOF;
	}

	private int allSlotsInROMFieldDo(J9ROMFieldShapePointer field) throws CorruptDataException {
		int fieldLength = 0;

		U32Pointer initialValue;
		UDATA modifiers;

		J9ROMNameAndSignaturePointer fieldNAS = field.nameAndSignature();
		classWalkerCallback.addSlot(clazz, SlotType.J9_ROM_UTF8, fieldNAS.nameEA(), "name");
		classWalkerCallback.addSlot(clazz, SlotType.J9_ROM_UTF8, fieldNAS.signatureEA(), "signature");
		classWalkerCallback.addSlot(clazz, SlotType.J9_U32, field.modifiersEA(), "modifiers");

		modifiers = field.modifiers();
		initialValue = U32Pointer.cast(field.add(1));

		if (modifiers.anyBitsIn(J9FieldFlagConstant)) {
			if (modifiers.anyBitsIn(J9FieldSizeDouble)) {
				classWalkerCallback.addSlot(clazz, SlotType.J9_I64, I64Pointer.cast(initialValue), "fieldInitialValue");
				initialValue = initialValue.add(2);
			} else {
				classWalkerCallback.addSlot(clazz, SlotType.J9_I32, initialValue, "fieldInitialValue");
				initialValue = initialValue.add(1);
			}
		}

		if (modifiers.anyBitsIn(J9FieldFlagHasGenericSignature)) {
			classWalkerCallback.addSlot(clazz, SlotType.J9_ROM_UTF8, initialValue, "fieldGenSigUTF8");
			initialValue = initialValue.add(1);
		}

		if (modifiers.allBitsIn(J9FieldFlagHasFieldAnnotations)) {
			initialValue = initialValue.add(allSlotsInAnnotationDo(initialValue, "fieldAnnotation"));
		}

		fieldLength = (int) (initialValue.getAddress() - field.getAddress());
		classWalkerCallback.addSection(clazz, field, fieldLength, "field", true);

		return fieldLength;
	}
	void allSlotsInExceptionInfoDo(J9ExceptionInfoPointer exceptionInfo) throws CorruptDataException
	{
		J9ExceptionHandlerPointer exceptionHandler;
		SelfRelativePointer throwNames;

		classWalkerCallback.addSlot(clazz, SlotType.J9_U16, exceptionInfo.catchCountEA(), "catchCount");
		classWalkerCallback.addSlot(clazz, SlotType.J9_U16, exceptionInfo.throwCountEA(), "throwCount");

		exceptionHandler = ROMHelp.J9EXCEPTIONINFO_HANDLERS(exceptionInfo); /* endian safe */
		for (int i = 0; i < exceptionInfo.catchCount().longValue(); i++) {
			classWalkerCallback.addSlot(clazz, SlotType.J9_U32, exceptionHandler.startPCEA(), "startPC");
			classWalkerCallback.addSlot(clazz, SlotType.J9_U32, exceptionHandler.endPCEA(), "endPC");
			classWalkerCallback.addSlot(clazz, SlotType.J9_U32, exceptionHandler.handlerPCEA(), "handlerPC");
			classWalkerCallback.addSlot(clazz, SlotType.J9_U32, exceptionHandler.exceptionClassIndexEA(), "exceptionClassIndex");
			exceptionHandler = exceptionHandler.add(1);
		}

		/* not using J9EXCEPTIONINFO_THROWNAMES...we're already there */
		throwNames = SelfRelativePointer.cast(exceptionHandler);
		for (int i = 0; i < exceptionInfo.throwCount().intValue(); i++, throwNames = throwNames.add(1)) {
			classWalkerCallback.addSlot(clazz, SlotType.J9_ROM_UTF8, throwNames, "throwNameUTF8");
		}
	}
	private void allSlotsInBytecodesDo(J9ROMMethodPointer method) throws CorruptDataException {
		U8Pointer pc, bytecodes;

		/* bytecodeSizeLow already walked */
		long length = ROMHelp.J9_BYTECODE_SIZE_FROM_ROM_METHOD(method).longValue();

		if (length == 0) {
			return;
		}

		pc = bytecodes = ROMHelp.J9_BYTECODE_START_FROM_ROM_METHOD(method);

		while ((pc.getAddress() - bytecodes.getAddress()) < length) {
			int bc = (int) pc.at(0).intValue();
			try {
				classWalkerCallback.addSlot(clazz, SlotType.J9_U8, pc, BCNames.getName(bc));
			} catch (Exception e) {
			}

			pc = pc.add(1);
			I32 low;
			I32 high;
			long i;
			if ((bc == JBbipush)
				|| (bc == JBldc)
				|| (bc == JBiload)
				|| (bc == JBlload)
				|| (bc == JBfload)
				|| (bc == JBdload)
				|| (bc == JBaload)
				|| (bc == JBistore)
				|| (bc == JBlstore)
				|| (bc == JBfstore)
				|| (bc == JBdstore)
				|| (bc == JBastore)
				|| (bc == JBnewarray)
			) {
				/* Single 8-bit argument, no flip. */
				classWalkerCallback.addSlot(clazz, SlotType.J9_U8, pc, "bcArg8");
				pc = pc.add(1);
			} else if (bc == JBinvokeinterface2) {
				/* Single 16-bit argument */
				classWalkerCallback.addSlot(clazz, SlotType.J9_U8, pc, "bcArg8");
				pc = pc.add(1);
				classWalkerCallback.addSlot(clazz, SlotType.J9_U8, pc, "bcArg8");
				pc = pc.add(1);
				classWalkerCallback.addSlot(clazz, SlotType.J9_U16, pc, "bcArg16");
				pc = pc.add(2);
			} else if ((bc == JBsipush)
				|| (bc == JBldcw)
				|| (bc == JBldc2dw)
				|| (bc == JBldc2lw)
				|| (bc == JBiloadw)
				|| (bc == JBlloadw)
				|| (bc == JBfloadw)
				|| (bc == JBdloadw)
				|| (bc == JBaloadw)
				|| (bc == JBistorew)
				|| (bc == JBlstorew)
				|| (bc == JBfstorew)
				|| (bc == JBdstorew)
				|| (bc == JBastorew)
				|| (bc == JBifeq)
				|| (bc == JBifne)
				|| (bc == JBiflt)
				|| (bc == JBifge)
				|| (bc == JBifgt)
				|| (bc == JBifle)
				|| (bc == JBificmpeq)
				|| (bc == JBificmpne)
				|| (bc == JBificmplt)
				|| (bc == JBificmpge)
				|| (bc == JBificmpgt)
				|| (bc == JBificmple)
				|| (bc == JBifacmpeq)
				|| (bc == JBifacmpne)
				|| (bc == JBgoto)
				|| (bc == JBifnull)
				|| (bc == JBifnonnull)
				|| (bc == JBgetstatic)
				|| (bc == JBputstatic)
				|| (bc == JBgetfield)
				|| (bc == JBputfield)
				|| (bc == JBinvokevirtual)
				|| (bc == JBinvokespecial)
				|| (bc == JBinvokestatic)
				|| (bc == JBinvokehandle)
				|| (bc == JBinvokehandlegeneric)
				|| (bc == JBinvokedynamic)
				|| (bc == JBinvokeinterface)
				|| (bc == JBnew)
				|| (bc == JBnewdup)
				|| (bc == JBanewarray)
				|| (bc == JBcheckcast)
				|| (bc == JBinstanceof)
				|| (bc == JBinvokestaticsplit)
				|| (bc == JBinvokespecialsplit)
			) {
				classWalkerCallback.addSlot(clazz, SlotType.J9_U16, pc, "bcArg16");
				pc = pc.add(2);
			} else if (bc == JBiinc) {
				/* Two 8-bit arguments. */
				classWalkerCallback.addSlot(clazz, SlotType.J9_U8, pc, "bcArg8");
				pc = pc.add(1);
				classWalkerCallback.addSlot(clazz, SlotType.J9_U8, pc, "bcArg8");
				pc = pc.add(1);
			} else if (bc == JBiincw) {
				/* Two 16-bit arguments */
				classWalkerCallback.addSlot(clazz, SlotType.J9_U16, pc, "bcArg16");
				pc = pc.add(2);
				classWalkerCallback.addSlot(clazz, SlotType.J9_U16, pc, "bcArg16");
				pc = pc.add(2);
			} else if (bc == JBmultianewarray) {
				/* 16-bit argument followed by 8-bit argument */
				classWalkerCallback.addSlot(clazz, SlotType.J9_U16, pc, "bcArg16");
				pc = pc.add(2);
				classWalkerCallback.addSlot(clazz, SlotType.J9_U8, pc, "bcArg8");
				pc = pc.add(1);
			} else if (bc == JBgotow) {
				/* Single 32-bit argument */
				classWalkerCallback.addSlot(clazz, SlotType.J9_U32, pc, "bcArg32");
				pc = pc.add(4);
			} else if (bc == JBtableswitch) {
				int delta = (int) (pc.getAddress() - bytecodes.getAddress() - 1);
				switch(delta % 4) {
					case 0:
						classWalkerCallback.addSlot(clazz, SlotType.J9_U8, pc, "bcPad");
						pc = pc.add(1);
					case 1:
						classWalkerCallback.addSlot(clazz, SlotType.J9_U8, pc, "bcPad");
						pc = pc.add(1);
					case 2:
						classWalkerCallback.addSlot(clazz, SlotType.J9_U8, pc, "bcPad");
						pc = pc.add(1);
					case 3:
						break;
				}
				classWalkerCallback.addSlot(clazz, SlotType.J9_U32, pc, "bcArg32");
				pc = pc.add(4);
				classWalkerCallback.addSlot(clazz, SlotType.J9_U32, pc, "bcArg32");
				low = I32Pointer.cast(pc).at(0);
				pc = pc.add(4);
				classWalkerCallback.addSlot(clazz, SlotType.J9_U32, pc, "bcArg32");
				high = I32Pointer.cast(pc).at(0);
				pc = pc.add(4);
				for (i = 0; i <= high.sub(low).longValue(); ++i) {
					classWalkerCallback.addSlot(clazz, SlotType.J9_U32, pc, "bcArg32");
					pc = pc.add(4);
				}
			} else if (bc == JBlookupswitch) {
				int delta2 = (int) (pc.getAddress() - bytecodes.getAddress() - 1);
				switch(delta2 % 4) {
					case 0:
						classWalkerCallback.addSlot(clazz, SlotType.J9_U8, pc, "bcArg8");
						pc = pc.add(1);
					case 1:
						classWalkerCallback.addSlot(clazz, SlotType.J9_U8, pc, "bcArg8");
						pc = pc.add(1);
					case 2:
						classWalkerCallback.addSlot(clazz, SlotType.J9_U8, pc, "bcArg8");
						pc = pc.add(1);
					case 3:
						break;
				}
				classWalkerCallback.addSlot(clazz, SlotType.J9_U32, pc, "bcArg32");
				pc = pc.add(4);
				classWalkerCallback.addSlot(clazz, SlotType.J9_U32, pc, "bcArg32");
				i = U32Pointer.cast(pc).at(0).longValue();
				pc = pc.add(4);
				while (i-- > 0) {
					classWalkerCallback.addSlot(clazz, SlotType.J9_U32, pc, "bcArg32");
					pc = pc.add(4);
					classWalkerCallback.addSlot(clazz, SlotType.J9_U32, pc, "bcArg32");
					pc = pc.add(4);
				}
			}
		}

		/* Walk the padding bytes. */
		long roundedLength = ROMHelp.J9_ROUNDED_BYTECODE_SIZE_FROM_ROM_METHOD(method).longValue();
		while (length++ < roundedLength) {
			classWalkerCallback.addSlot(clazz, SlotType.J9_U8, pc, "bcSectionPadding");
			pc = pc.add(1);
		}
		classWalkerCallback.addSection(clazz, bytecodes, pc.getAddress() - bytecodes.getAddress(), "methodBytecodes", true);
	}

	void allSlotsInCPShapeDescriptionDo() throws CorruptDataException
	{
		U32Pointer cpShapeDescription = J9ROMClassHelper.cpShapeDescription(romClass);
		final int romConstantPoolCount = romClass.romConstantPoolCount().intValue();
		int count = (romConstantPoolCount + (U32.SIZEOF * 2) - 1) / (U32.SIZEOF * 2);

		classWalkerCallback.addSection(clazz, cpShapeDescription, count * U32.SIZEOF, "cpShapeDescription", true);
		for (int i = 0; i < count; i++) {
			classWalkerCallback.addSlot(clazz, SlotType.J9_U32, cpShapeDescription.add(i), "cpShapeDescriptionU32");
			//callbacks.slotCallback(romClass, J9ROM_U32, &cpShapeDescription[i], "cpShapeDescriptionU32", userData);
		}
	}

	private void allSlotsInConstantPoolDo() throws CorruptDataException
	{
		J9ROMConstantPoolItemPointer constantPool = J9ROMClassHelper.constantPool(romClass);
		U32Pointer cpShapeDescription = J9ROMClassHelper.cpShapeDescription(romClass);

		if (cpShapeDescription.isNull()) {
			return;
		}

		int constPoolCount = romClass.romConstantPoolCount().intValue();
		PointerPointer cpEntry = PointerPointer.cast(J9ROMClassHelper.constantPool(romClass));

		// The spaces at the end of "Constant Pool" are important since the
		// regions are sorted
		// by address and size, but when they are equal they are sorted by
		// longest name and "Constant Pool" has to come first
		classWalkerCallback.addSection(clazz, constantPool, constPoolCount * U64.SIZEOF, "constantPool              ", true);
		for (int index = 0; index < constPoolCount; index++) {
			long shapeDesc = ConstantPoolHelpers.J9_CP_TYPE(cpShapeDescription, index);
			if (shapeDesc == J9CPTYPE_CLASS) {
				J9ROMStringRefPointer ref = J9ROMStringRefPointer.cast(cpEntry);
				classWalkerCallback.addSlot(clazz, SlotType.J9_ROM_UTF8, ref.utf8DataEA(), "cpFieldUtf8");
				classWalkerCallback.addSlot(clazz, SlotType.J9_U32, ref.cpTypeEA(), "cpFieldType");

			} else if((shapeDesc == J9CPTYPE_STRING) || (shapeDesc == J9CPTYPE_ANNOTATION_UTF8)) {
				J9ROMStringRefPointer ref = J9ROMStringRefPointer.cast(cpEntry);
				classWalkerCallback.addSlot(clazz, SlotType.J9_ROM_UTF8, ref.utf8DataEA(), "cpFieldUtf8");
				classWalkerCallback.addSlot(clazz, SlotType.J9_U32, ref.cpTypeEA(), "cpFieldType");

			} else if (shapeDesc == J9CPTYPE_INT) {
				J9ROMConstantPoolItemPointer ref = J9ROMConstantPoolItemPointer.cast(cpEntry);
				classWalkerCallback.addSlot(clazz, SlotType.J9_U32, ref.slot1EA(), "cpFieldInt");
				classWalkerCallback.addSlot(clazz, SlotType.J9_U32, ref.slot2EA(), "cpFieldIntUnused");

			} else if (shapeDesc == J9CPTYPE_FLOAT) {
				J9ROMConstantPoolItemPointer ref = J9ROMConstantPoolItemPointer.cast(cpEntry);
				classWalkerCallback.addSlot(clazz, SlotType.J9_U32, ref.slot1EA(), "cpFieldFloat");
				classWalkerCallback.addSlot(clazz, SlotType.J9_U32, ref.slot2EA(), "cpFieldFloatUnused");

			} else if (shapeDesc == J9CPTYPE_LONG) {
				classWalkerCallback.addSlot(clazz, SlotType.J9_I64, I64Pointer.cast(cpEntry), "cpField8");

			} else if (shapeDesc == J9CPTYPE_DOUBLE) {
				classWalkerCallback.addSlot(clazz, SlotType.J9_I64, I64Pointer.cast(cpEntry), "cpField8");

			} else if (shapeDesc == J9CPTYPE_FIELD) {
				J9ROMFieldRefPointer ref = J9ROMFieldRefPointer.cast(cpEntry);
				classWalkerCallback.addSlot(clazz, SlotType.J9_SRPNAS, ref.nameAndSignatureEA(), "cpFieldNAS");
				classWalkerCallback.addSlot(clazz, SlotType.J9_U32, ref.classRefCPIndexEA(), "cpFieldClassRef");

			} else if ((shapeDesc == J9CPTYPE_HANDLE_METHOD) ||
					(shapeDesc == J9CPTYPE_STATIC_METHOD) || 
					(shapeDesc == J9CPTYPE_INSTANCE_METHOD) ||
					(shapeDesc == J9CPTYPE_INTERFACE_METHOD) ||
					(shapeDesc == J9CPTYPE_INTERFACE_INSTANCE_METHOD) ||
					(shapeDesc == J9CPTYPE_INTERFACE_STATIC_METHOD)) {
				J9ROMMethodRefPointer ref = J9ROMMethodRefPointer.cast(cpEntry);
				classWalkerCallback.addSlot(clazz, SlotType.J9_SRPNAS, ref.nameAndSignatureEA(), "cpFieldNAS");
				classWalkerCallback.addSlot(clazz, SlotType.J9_U32, ref.classRefCPIndexEA(), "cpFieldClassRef");

			} else if (shapeDesc == J9CPTYPE_METHOD_TYPE) {
				J9ROMMethodTypeRefPointer ref = J9ROMMethodTypeRefPointer.cast(cpEntry);
				classWalkerCallback.addSlot(clazz, SlotType.J9_ROM_UTF8, ref.signatureEA(), "signature");
				classWalkerCallback.addSlot(clazz, SlotType.J9_U32, ref.cpTypeEA(), "cpType");

			} else if (shapeDesc == J9CPTYPE_METHODHANDLE) {
				J9ROMMethodHandleRefPointer ref = J9ROMMethodHandleRefPointer.cast(cpEntry);
				classWalkerCallback.addSlot(clazz, SlotType.J9_U32, ref.methodOrFieldRefIndexEA(), "methodOrFieldRefIndex");
				classWalkerCallback.addSlot(clazz, SlotType.J9_U32, ref.handleTypeAndCpTypeEA(), "handleTypeAndCpType");

			} else if (shapeDesc == J9CPTYPE_CONSTANT_DYNAMIC) {
				try {
					J9ROMConstantDynamicRefPointer ref = J9ROMConstantDynamicRefPointer.cast(cpEntry);
					classWalkerCallback.addSlot(clazz, SlotType.J9_SRPNAS, ref.nameAndSignatureEA(), "cpFieldNAS");
					classWalkerCallback.addSlot(clazz, SlotType.J9_U32, ref.bsmIndexAndCpTypeEA(), "cpFieldBSMIndexAndCpType");
				} catch (NoClassDefFoundError | NoSuchFieldException e) {
					// J9ROMConstantDynamicRef should be known to a VM that understands constant dynamic
					throw new CorruptDataException(e);
				}
			} else if ((shapeDesc == J9CPTYPE_UNUSED) || (shapeDesc == J9CPTYPE_UNUSED8)) {
				classWalkerCallback.addSlot(clazz, SlotType.J9_I64, I64Pointer.cast(cpEntry), "cpFieldUnused");

			} else {
				throw new CorruptDataException("Unknown CP entry type: " + shapeDesc);
			}

			cpEntry = cpEntry.addOffset(J9ROMConstantPoolItem.SIZEOF);
		}
	}

	void allSlotsInOptionalInfoDo() throws CorruptDataException
	{
		U32Pointer optionalInfo = J9ROMClassHelper.optionalInfo(romClass);
		SelfRelativePointer cursor = SelfRelativePointer.cast(optionalInfo);

		if (romClass.optionalFlags().anyBitsIn(J9NonbuilderConstants.J9_ROMCLASS_OPTINFO_SOURCE_FILE_NAME)) {
			classWalkerCallback.addSlot(clazz, SlotType.J9_ROM_UTF8, cursor, "optionalFileNameUTF8");
			cursor = cursor.add(1);
		}
		if (romClass.optionalFlags().anyBitsIn(J9NonbuilderConstants.J9_ROMCLASS_OPTINFO_GENERIC_SIGNATURE)) {
			classWalkerCallback.addSlot(clazz, SlotType.J9_ROM_UTF8, cursor, "optionalGenSigUTF8");
			cursor = cursor.add(1);
		}
		if (romClass.optionalFlags().anyBitsIn(J9NonbuilderConstants.J9_ROMCLASS_OPTINFO_SOURCE_DEBUG_EXTENSION)) {
			classWalkerCallback.addSlot(clazz, SlotType.J9_SRP, cursor, "optionalSourceDebugExtSRP");
			allSlotsInSourceDebugExtensionDo(J9SourceDebugExtensionPointer.cast(cursor.get()));
			cursor = cursor.add(1);
		}
		if (romClass.optionalFlags().anyBitsIn(J9NonbuilderConstants.J9_ROMCLASS_OPTINFO_ENCLOSING_METHOD)) {
			classWalkerCallback.addSlot(clazz, SlotType.J9_SRP, cursor, "optionalEnclosingMethodSRP");
			allSlotsInEnclosingObjectDo(J9EnclosingObjectPointer.cast(cursor.get()));
			cursor = cursor.add(1);
		}
		if (romClass.optionalFlags().anyBitsIn(J9NonbuilderConstants.J9_ROMCLASS_OPTINFO_SIMPLE_NAME)) {
			classWalkerCallback.addSlot(clazz, SlotType.J9_ROM_UTF8, cursor, "optionalSimpleNameUTF8");
			cursor = cursor.add(1);
		}
		if (romClass.optionalFlags().allBitsIn(J9NonbuilderConstants.J9_ROMCLASS_OPTINFO_VERIFY_EXCLUDE)) {
			classWalkerCallback.addSlot(clazz, SlotType.J9_U32, cursor, "optionalVerifyExclude");
			cursor = cursor.add(1);
		}
		if (romClass.optionalFlags().allBitsIn(J9NonbuilderConstants.J9_ROMCLASS_OPTINFO_CLASS_ANNOTATION_INFO)) {
			classWalkerCallback.addSlot(clazz, SlotType.J9_SRP, cursor, "classAnnotationsSRP");
			allSlotsInAnnotationDo(U32Pointer.cast(cursor.get()), "classAnnotations");
			cursor = cursor.add(1);
		}
		if (romClass.optionalFlags().allBitsIn(J9NonbuilderConstants.J9_ROMCLASS_OPTINFO_RECORD_ATTRIBUTE)) {
			classWalkerCallback.addSlot(clazz, SlotType.J9_SRP, cursor, "recordAttributeSRP");
			recordAttributeDo(U32Pointer.cast(cursor.get()));
			cursor = cursor.add(1);
		}
		if (romClass.optionalFlags().allBitsIn(J9NonbuilderConstants.J9_ROMCLASS_OPTINFO_PERMITTEDSUBCLASSES_ATTRIBUTE)) {
			classWalkerCallback.addSlot(clazz, SlotType.J9_SRP, cursor, "permittedSubclassesAttributeSRP");
			permittedSubclassAttributeDo(U32Pointer.cast(cursor.get()));
			cursor = cursor.add(1);
		}
		if (ValueTypeHelper.getValueTypeHelper().areValueTypesSupported()) {
			if (romClass.optionalFlags().allBitsIn(J9NonbuilderConstants.J9_ROMCLASS_OPTINFO_INJECTED_INTERFACE_INFO)) {
				classWalkerCallback.addSlot(clazz, SlotType.J9_SRP, cursor, "optionalInjectedInterfaces");
				cursor = cursor.add(1);
			}

			if (J9ROMClassHelper.hasLoadableDescriptorsAttribute(romClass)) {
				classWalkerCallback.addSlot(clazz, SlotType.J9_SRP, cursor, "loadableDescriptorsAttributeSRP");
				loadableDescriptorsAttributeDo(U32Pointer.cast(cursor.get()));
				cursor = cursor.add(1);
			}
		}
		if (J9ROMClassHelper.hasImplicitCreationAttribute(romClass)) {
			classWalkerCallback.addSlot(clazz, SlotType.J9_SRP, cursor, "implicitCreationAttributeSRP");
			implicitCreationAttributeDo(U32Pointer.cast(cursor.get()));
			cursor = cursor.add(1);
		}
		classWalkerCallback.addSection(clazz, optionalInfo, cursor.getAddress() - optionalInfo.getAddress(), "optionalInfo", true);
	}

	void allSlotsInIntermediateClassDataDo () throws CorruptDataException
	{
		UDATA count = romClass.intermediateClassDataLength();
		if (count.gt(0)) {
			U8Pointer cursor = romClass.intermediateClassData();
			String j9xHelp = "!j9x "+cursor.getHexAddress()+","+count.getHexValue();
			classWalkerCallback.addSlot(clazz, SlotType.J9_IntermediateClassData, cursor, "intermediateClassData", j9xHelp);
			classWalkerCallback.addSection(clazz, cursor, count.longValue(), "intermediateClassDataSection", true);
		}
	}

	void allSlotsInVarHandleMethodTypeLookupTableDo() throws CorruptDataException
	{
		if (J9BuildFlags.J9VM_OPT_METHOD_HANDLE && !J9BuildFlags.J9VM_OPT_OPENJDK_METHODHANDLE) {
			try {
				int count = romClass.varHandleMethodTypeCount().intValue();

				if (count > 0) {
					Pointer cursorVoidEA = romClass.varHandleMethodTypeLookupTableEA();
					VoidPointer cursorVoid = SelfRelativePointer.cast(cursorVoidEA).get();
					U16Pointer cursor = U16Pointer.cast(cursorVoid);

					classWalkerCallback.addSection(clazz, cursor, count * U16.SIZEOF, "varHandleMethodTypeLookupTable", true);
					for (int i = 0; i < count; i++) {
						classWalkerCallback.addSlot(clazz, SlotType.J9_U16, cursor.add(i), "cpIndex");
					}
				}
			} catch (NoSuchFieldException e) {
				throw new CorruptDataException(e);
			}
		}
	}

	void allSlotsInStaticSplitMethodRefIndexesDo() throws CorruptDataException
	{
		int count = romClass.staticSplitMethodRefCount().intValue();
		U16Pointer cursor = romClass.staticSplitMethodRefIndexes();

		if (count > 0) {
			classWalkerCallback.addSection(clazz, cursor, count * U16.SIZEOF, "staticSplitMethodRefIndexes", true);
			for (int i = 0; i < count; i++) {
				classWalkerCallback.addSlot(clazz, SlotType.J9_U16, cursor.add(i), "cpIndex");
			}
		}
	}

	void allSlotsInSpecialSplitMethodRefIndexesDo() throws CorruptDataException
	{
		int count = romClass.specialSplitMethodRefCount().intValue();
		U16Pointer cursor = romClass.specialSplitMethodRefIndexes();

		if (count > 0) {
			classWalkerCallback.addSection(clazz, cursor, count * U16.SIZEOF, "specialSplitMethodRefIndexes", true);
			for (int i = 0; i < count; i++) {
				classWalkerCallback.addSlot(clazz, SlotType.J9_U16, cursor.add(i), "cpIndex");
			}
		}
	}

	private long allSlotsInStackMapFramesDo(U8Pointer cursor, long frameCount) throws CorruptDataException
	{
		U8Pointer cursorStart = U8Pointer.NULL;
		long count;

		for (; frameCount > 0; frameCount--) {
			long frameType;
			int length;

			if (cursorStart.isNull()) {
				cursorStart = cursor;
			}

			classWalkerCallback.addSlot(clazz, SlotType.J9_U8, cursor, "stackMapFrameType");
			frameType = cursor.at(0).longValue();
			cursor = cursor.add(1);

			if (CFR_STACKMAP_SAME_LOCALS_1_STACK > frameType) { /* 0..63 */
				/* SAME frame - no extra data */
			} else if (CFR_STACKMAP_SAME_LOCALS_1_STACK_END > frameType) { /* 64..127 */
				length = allSlotsInVerificationTypeInfoDo(cursor);
				if (0 == length) {
					return cursor.getAddress() - cursorStart.getAddress();
				}
				cursor = cursor.add(length);
			} else if (CFR_STACKMAP_SAME_LOCALS_1_STACK_EXTENDED > frameType) { /* 128..246 */
				/* Reserved frame types - no extra data */
			} else if (CFR_STACKMAP_SAME_LOCALS_1_STACK_EXTENDED == frameType) { /* 247 */
				classWalkerCallback.addSlot(clazz, SlotType.J9_U16, cursor, "stackMapFrameOffset");
				cursor = cursor.add(2);
				length = allSlotsInVerificationTypeInfoDo(cursor);
				if (0 == length) {
					return cursor.getAddress() - cursorStart.getAddress();
				}
				cursor = cursor.add(length);
			} else if (CFR_STACKMAP_SAME_EXTENDED >= frameType) { /* 248..251 */
				classWalkerCallback.addSlot(clazz, SlotType.J9_U16, cursor, "stackMapFrameOffset");
				cursor = cursor.add(2);
			} else if (CFR_STACKMAP_FULL > frameType) { /* 252..254 */
				classWalkerCallback.addSlot(clazz, SlotType.J9_U16, cursor, "stackMapFrameOffset");
				cursor = cursor.add(2);
				count = frameType - CFR_STACKMAP_APPEND_BASE;
				for (; count > 0; count--) {
					length = allSlotsInVerificationTypeInfoDo(cursor);
					if (0 == length) {
						return cursor.getAddress() - cursorStart.getAddress();
					}
					cursor = cursor.add(length);
				}
			} else if (CFR_STACKMAP_FULL == frameType) { /* 255 */
				classWalkerCallback.addSlot(clazz, SlotType.J9_U16, cursor, "stackMapFrameOffset");
				cursor = cursor.add(2);
				classWalkerCallback.addSlot(clazz, SlotType.J9_U16, cursor, "stackMapFrameLocalsCount");
				count = U16Pointer.cast(cursor).at(0).longValue();

				count = SWAP2BE((short) count);
				cursor = cursor.add(2);

				for (; count > 0; count--) {
					length = allSlotsInVerificationTypeInfoDo(cursor);
					if (0 == length) {
						return cursor.getAddress() - cursorStart.getAddress();
					}
					cursor = cursor.add(length);
				}

				classWalkerCallback.addSlot(clazz, SlotType.J9_U16, cursor, "stackMapFrameItemsCount");
				count = U16Pointer.cast(cursor).at(0).longValue();

				count = SWAP2BE((short) count);
				cursor = cursor.add(2);

				for (; count > 0; count--) {
					length = allSlotsInVerificationTypeInfoDo(cursor);
					if (0 == length) {
						return cursor.getAddress() - cursorStart.getAddress();
					}
					cursor = cursor.add(length);
				}
			}
		}
		return cursor.getAddress() - cursorStart.getAddress();
	}

	private void allSlotsInStackMapDo(U8Pointer stackMap) throws CorruptDataException
	{
		U8Pointer cursor = stackMap;
		long frameCount;
		int stackMapSize = U16.SIZEOF;

		if (stackMap.isNull()) {
			return;
		}

		/* TODO: Questions?
		 *  - do we want to display the non-flipped value?
		 *  - do we want to flip and display?
		 *  - do we want to have a J9ROM_U16_BE type?
		 *  */
		classWalkerCallback.addSlot(clazz, SlotType.J9_U16, U16Pointer.cast(cursor), "stackMapFrameCount");
		frameCount = U16Pointer.cast(cursor).at(0).longValue();
		frameCount = SWAP2BE((short) frameCount);
		cursor = cursor.add(2);

		stackMapSize += allSlotsInStackMapFramesDo(cursor, frameCount);
	}

	int allSlotsInVerificationTypeInfoDo(U8Pointer cursor) throws CorruptDataException
	{
		try {
			classWalkerCallback.addSlot(clazz, SlotType.J9_U8, cursor, "typeInfoTag");
			long type = cursor.at(0).longValue();
			cursor = cursor.add(1);

			if (type < CFR_STACKMAP_TYPE_OBJECT) {
				return 1;
			}

			classWalkerCallback.addSlot(clazz, SlotType.J9_U16, cursor, "typeInfoU16");
		} catch (MemoryFault e) {
			return 0;
		}
		return 3;
	}

	void recordAttributeDo(U32Pointer attribute) throws CorruptDataException
	{
		if (attribute.isNull()) {
			return;
		}
		U32Pointer attributeStart = attribute;
		int numRecordComponents = attribute.at(0).intValue();
		classWalkerCallback.addSlot(clazz, SlotType.J9_U32, attribute, "numberRecordComponents");
		attribute = attribute.add(1);
		for (int i = 0; i < numRecordComponents; i++) {
			try {
				J9ROMRecordComponentShapePointer recordComponent = J9ROMRecordComponentShapePointer.cast(attribute);
				attribute = U32Pointer.cast(recordComponent.add(1));

				J9ROMNameAndSignaturePointer recordComponentNAS = recordComponent.nameAndSignature();
				classWalkerCallback.addSlot(clazz, SlotType.J9_ROM_UTF8, recordComponentNAS.nameEA(), "name");
				classWalkerCallback.addSlot(clazz, SlotType.J9_ROM_UTF8, recordComponentNAS.signatureEA(), "signature");
				classWalkerCallback.addSlot(clazz, SlotType.J9_U32, recordComponent.attributeFlagsEA(), "attributeFlags");
				classWalkerCallback.addSection(clazz, recordComponent, J9ROMRecordComponentShape.SIZEOF, "recordComponentShape", true);

				/* process variable attributes */
				UDATA attributeFlags = recordComponent.attributeFlags();

				if (attributeFlags.anyBitsIn(J9RecordComponentFlagHasGenericSignature)) {
					classWalkerCallback.addSlot(clazz, SlotType.J9_ROM_UTF8, attribute, "recordComponentGenSigUTF8");
					attribute = attribute.add(1);
				}
				if (attributeFlags.anyBitsIn(J9RecordComponentFlagHasAnnotations)) {
					int increment = allSlotsInAnnotationDo(attribute, "recordComponentAnnotation");
					attribute = attribute.add(increment);
				}
				if (attributeFlags.anyBitsIn(J9RecordComponentFlagHasTypeAnnotations)) {
					int increment = allSlotsInAnnotationDo(attribute, "recordComponentTypeAnnotation");
					attribute = attribute.add(increment);
				}
			} catch (NoClassDefFoundError | NoSuchFieldException e) {
				// J9ROMRecordComponentShape should be known to a VM that supports records
				throw new CorruptDataException(e);
			}
		}
		/* calculate RecordComponent padding */
		int recordComponentLength = (int)(attribute.getAddress() - attributeStart.getAddress());
		int padding = recordComponentLength % U32.SIZEOF;
		if (0 != padding) {
			padding = U32.SIZEOF - padding;
		}
		U8Pointer attributeU8 = U8Pointer.cast(attribute);
		for (int i = 0; i < padding; i++) {
			classWalkerCallback.addSlot(clazz, SlotType.J9_U8, attributeU8, "recordComponent padding");
			attributeU8 = attributeU8.add(1);
		}
		classWalkerCallback.addSection(clazz, attributeStart, recordComponentLength + padding, "recordComponent", true);
	}

	void permittedSubclassAttributeDo(U32Pointer attribute) throws CorruptDataException
	{
		if (attribute.isNull()) {
			return;
		}
		U32Pointer attributeStart = attribute;
		int numPermittedSubclasses = attribute.at(0).intValue();
		classWalkerCallback.addSlot(clazz, SlotType.J9_U32, attribute, "numberPermittedSubclasses");
		attribute = attribute.add(1);
		for (int i = 0; i < numPermittedSubclasses; i++) {
			classWalkerCallback.addSlot(clazz, SlotType.J9_ROM_UTF8, attribute, "permittedSubclassName");
			attribute = attribute.add(1);
		}
		classWalkerCallback.addSection(clazz, attributeStart, attribute.getAddress() - attributeStart.getAddress(), "permittedSubclass", true);
	}

	void loadableDescriptorsAttributeDo(U32Pointer attribute) throws CorruptDataException
	{
		if (attribute.isNull()) {
			return;
		}
		U32Pointer attributeStart = attribute;
		classWalkerCallback.addSlot(clazz, SlotType.J9_U32, attribute, "numberLoadableDescriptors");
		for (int i = 0, numLoadableDescriptors = attribute.at(0).intValue(); i < numLoadableDescriptors; i++) {
			attribute = attribute.add(1);
			classWalkerCallback.addSlot(clazz, SlotType.J9_ROM_UTF8, attribute, "loadableDescriptorName");
		}
		attribute = attribute.add(1);
		classWalkerCallback.addSection(clazz, attributeStart,
			attribute.getAddress() - attributeStart.getAddress(),
			"loadableDescriptorsAttribute", true);
	}

	void implicitCreationAttributeDo(U32Pointer attribute) throws CorruptDataException
	{
		if (attribute.isNull()) {
			return;
		}
		U32Pointer attributeStart = attribute;
		classWalkerCallback.addSlot(clazz, SlotType.J9_U32, attribute, "implicitCreationFlags");
		attribute = attribute.add(1);
		classWalkerCallback.addSection(clazz, attributeStart, attribute.getAddress() - attributeStart.getAddress(), "implicitCreationAttribute", true);
	}

	int allSlotsInAnnotationDo(U32Pointer annotation, String annotationSectionName) throws CorruptDataException
	{
		int increment = 0;
		int annotationLength = annotation.at(0).intValue();
		/* determine how many U_32 sized chunks to increment initialValue by
		 * NOTE: annotation length is U_32 and does not include the length field itself
		 * annotations are padded to U_32 which is also not included in the length field*/
		int padding = U32.SIZEOF - (annotationLength % U32.SIZEOF);
		increment = annotationLength / U32.SIZEOF;
		if (U32.SIZEOF == padding) {
			padding = 0;
		}
		if (padding > 0) {
			increment ++;
		}
		classWalkerCallback.addSlot(clazz, SlotType.J9_U32, annotation, "annotation length");

		int count = annotationLength;
		U8Pointer cursor = U8Pointer.cast(annotation.add(1));
		for (; count > 0; count--) {
			classWalkerCallback.addSlot(clazz, SlotType.J9_U8, cursor, "annotation data");
			cursor = cursor.add(1);
		}
		count = padding;
		for (; count > 0; count--) {
			classWalkerCallback.addSlot(clazz, SlotType.J9_U8, cursor, "annotation padding");
			cursor = cursor.add(1);
		}
		/* move past the annotation length */
		increment += 1;
		classWalkerCallback.addSection(clazz, annotation, increment * U32.SIZEOF, annotationSectionName, true);
		return increment;
	}

	long allSlotsInMethodDebugInfoDo(U32Pointer cursor) throws CorruptDataException
	{
		J9MethodDebugInfoPointer methodDebugInfo;
		U8Pointer currentLineNumberPtr;
		/* if data is out of line, then the size of the data inline in the method is a single SRP in sizeof(U_32 increments), currently assuming J9SRP is U_32 aligned*/
		long inlineSize = SelfRelativePointer.SIZEOF / U32.SIZEOF;
		long sectionSizeBytes = 0;
		boolean inlineDebugExtension = (1 == (cursor.at(0).intValue() & 1));

		/* check for low tag to indicate inline or out of line debug information */
		if (inlineDebugExtension) {
			methodDebugInfo = J9MethodDebugInfoPointer.cast(cursor);
			/* set the inline size to stored size in terms of U_32
			 * NOTE: stored size is aligned on a U32
			 * tag bit will be dropped by the '/' operation */
			inlineSize = cursor.at(0).intValue() / U32.SIZEOF;
			sectionSizeBytes = inlineSize * U32.SIZEOF;
		} else {
			methodDebugInfo = J9MethodDebugInfoPointer.cast(SelfRelativePointer.cast(cursor).get());
			if (AlgorithmVersion.getVersionOf("VM_LINE_NUMBER_TABLE_VERSION").getAlgorithmVersion() < 1) {
				sectionSizeBytes = J9MethodDebugInfo.SIZEOF + (J9MethodDebugInfoHelper.getLineNumberCount(methodDebugInfo).intValue() * U32.SIZEOF);
			} else {
				sectionSizeBytes = J9MethodDebugInfo.SIZEOF + J9MethodDebugInfoHelper.getLineNumberCompressedSize(methodDebugInfo).intValue();
				/* When out of line debug information, align on U_16 */
				sectionSizeBytes = (sectionSizeBytes + U16.SIZEOF  - 1) & ~(U16.SIZEOF - 1);
			}
		}

		if (!inlineDebugExtension) {
			if (inlineSize == 1) {
				classWalkerCallback.addSlot(clazz, SlotType.J9_SRP, cursor, "SRP to DebugInfo");
				classWalkerCallback.addSection(clazz, cursor, inlineSize * U32.SIZEOF, "methodDebugInfo out of line", true);
			}
		}

		classWalkerCallback.addSlot(clazz, SlotType.J9_SRP, methodDebugInfo.srpToVarInfoEA(), "SizeOfDebugInfo(low tagged)");

		if (AlgorithmVersion.getVersionOf("VM_LINE_NUMBER_TABLE_VERSION").getAlgorithmVersion() < 1) {
			classWalkerCallback.addSlot(clazz, SlotType.J9_U32, methodDebugInfo.lineNumberCountEA(), "lineNumberCount");
			classWalkerCallback.addSlot(clazz, SlotType.J9_U32, methodDebugInfo.varInfoCountEA(), "varInfoCount");

			J9LineNumberPointer lineNumberPtr = J9MethodDebugInfoHelper.getLineNumberTableForROMClass(methodDebugInfo);
			if (lineNumberPtr.notNull()) {
				for (int j = 0; j < methodDebugInfo.lineNumberCount().intValue(); j++, lineNumberPtr = lineNumberPtr.add(1)) {
					//FIXME : Silo
//					classWalkerCallback.addSlot(clazz, SlotType.J9_U16, lineNumberPtr.offsetLocationEA(), "offsetLocation");
					classWalkerCallback.addSlot(clazz, SlotType.J9_U16, lineNumberPtr.lineNumberEA(), "lineNumber");
				}
			}
		} else {
			classWalkerCallback.addSlot(clazz, SlotType.J9_U32, methodDebugInfo.lineNumberCountEA(), "lineNumberCount(encoded)");
			classWalkerCallback.addSlot(clazz, SlotType.J9_U32, methodDebugInfo.varInfoCountEA(), "varInfoCount");
			if (methodDebugInfo.lineNumberCount().allBitsIn(1)) {
				classWalkerCallback.addSlot(clazz, SlotType.J9_U32, U32Pointer.cast(methodDebugInfo.add(1)), "compressed line number size");
			}

			currentLineNumberPtr = J9MethodDebugInfoHelper.getCompressedLineNumberTableForROMClassV1(methodDebugInfo);
			if (currentLineNumberPtr.notNull()) {
				for (int j = 0; j < J9MethodDebugInfoHelper.getLineNumberCompressedSize(methodDebugInfo).intValue(); j++) {
					classWalkerCallback.addSlot(clazz, SlotType.J9_U8, currentLineNumberPtr, "pc, lineNumber compressed");
					currentLineNumberPtr = currentLineNumberPtr.add(1);
				}
			}
		}

		U8Pointer variableTable = OptInfo.getV1VariableTableForMethodDebugInfo(methodDebugInfo);
		if (variableTable.notNull()) {
			LocalVariableTableIterator variableInfoValuesIterator = LocalVariableTableIterator.localVariableTableIteratorFor(methodDebugInfo);
			U8Pointer start = variableInfoValuesIterator.getLocalVariableTablePtr();
			while (variableInfoValuesIterator.hasNext()) {
				LocalVariableTable values = variableInfoValuesIterator.next();

				// Need to walk the name and signature to add them to the UTF8 section
				classWalkerCallback.addSlot(clazz, SlotType.J9_ROM_UTF8, values.getNameSrp(), "name");
				classWalkerCallback.addSlot(clazz, SlotType.J9_ROM_UTF8, values.getSignatureSrp(), "signature");
				if (values.getGenericSignature().notNull()) {
					classWalkerCallback.addSlot(clazz, SlotType.J9_ROM_UTF8, values.getGenericSignatureSrp(), "genericSignature");
				}
			}
			U8Pointer end = variableInfoValuesIterator.getLocalVariableTablePtr();
			int localVariableSectionSize = end.sub(start).intValue();

			for (int j = 0; j < localVariableSectionSize; j++) {
				classWalkerCallback.addSlot(clazz, SlotType.J9_U8, start, "variableInfo compressed");
				start = start.add(1);
			}

			classWalkerCallback.addSection(clazz, variableTable, localVariableSectionSize, "variableInfo" + (inlineDebugExtension?" Inline":""), inlineDebugExtension);
		}
		classWalkerCallback.addSection(clazz, methodDebugInfo, sectionSizeBytes, "methodDebugInfo" + (inlineDebugExtension?" Inline":""), inlineDebugExtension);
		return inlineSize;
	}

	void allSlotsInEnclosingObjectDo(J9EnclosingObjectPointer enclosingObject) throws CorruptDataException
	{
		if (enclosingObject.isNull()) {
			return;
		}
		classWalkerCallback.addSlot(clazz, SlotType.J9_U32, enclosingObject.classRefCPIndexEA(), "classRefCPIndex");
		classWalkerCallback.addSlot(clazz, SlotType.J9_SRPNAS, enclosingObject.nameAndSignatureEA(), "nameAndSignature");
		classWalkerCallback.addSection(clazz, enclosingObject, J9EnclosingObject.SIZEOF, "enclosingObject", true);

		addObjectsasSlot(enclosingObject);
	}

	void allSlotsInSourceDebugExtensionDo(J9SourceDebugExtensionPointer sde) throws CorruptDataException
	{
		if (sde.isNull()) {
			return;
		}

		classWalkerCallback.addSlot(clazz, SlotType.J9_U32, sde.sizeEA(), "optionalSourceDebugExtSize");
		long size = sde.size().longValue();
		long alignedSize = (size + U32.SIZEOF - 1) & ~(U32.SIZEOF - 1);

		U8Pointer data = U8Pointer.cast(sde.add(1));
		for (long i = 0; i < size; ++i) {
			classWalkerCallback.addSlot(clazz, SlotType.J9_U8, data.add(i), "optionalSourceDebugExtData");
		}
		for (long i = size; i < alignedSize; ++i) {
			classWalkerCallback.addSlot(clazz, SlotType.J9_U8, data.add(i), "optionalSourceDebugExtPadding");
		}

		classWalkerCallback.addSection(clazz, sde, J9SourceDebugExtension.SIZEOF + alignedSize, "optionalSourceDebugExt", true);
	}

	private short SWAP2BE(short in) {
		IProcess process = context.process;
		if (process.getByteOrder() == ByteOrder.LITTLE_ENDIAN) {
			return Short.reverseBytes(in);
		}
		return in;
	}

}
