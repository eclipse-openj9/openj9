/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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
package com.ibm.j9ddr;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;
import java.util.regex.Pattern;

import com.ibm.j9ddr.StructureReader.ConstantDescriptor;
import com.ibm.j9ddr.StructureReader.FieldDescriptor;
import com.ibm.j9ddr.StructureReader.StructureDescriptor;
import com.ibm.j9ddr.tools.FlagStructureList;

import jdk.internal.org.objectweb.asm.AnnotationVisitor;
import jdk.internal.org.objectweb.asm.ClassWriter;
import jdk.internal.org.objectweb.asm.Label;
import jdk.internal.org.objectweb.asm.MethodVisitor;
import jdk.internal.org.objectweb.asm.Opcodes;
import jdk.internal.org.objectweb.asm.Type;

/**
 * Generates the class bytecodes needed by DDR to represent, as Java classes,
 * the structures and pointers described by the blob.
 */
public class BytecodeGenerator {

	public static byte[] getPointerClassBytes(StructureReader reader, StructureTypeManager typeManager,
			StructureDescriptor structure, String className) {
		if (FlagStructureList.isFlagsStructure(structure.getName())) {
			return FlagsHelper.getClassBytes(structure, className);
		} else {
			return PointerHelper.getClassBytes(reader, typeManager, structure, className);
		}
	}

	public static byte[] getStructureClassBytes(StructureDescriptor structure, String className) {
		return StructureHelper.getClassBytes(structure, className);
	}

}

final class FlagsHelper extends HelperBase {

	public static byte[] getClassBytes(StructureDescriptor structure, String className) {
		ClassWriter clazz = new ClassWriter(0);

		clazz.visit(V1_8, ACC_PUBLIC | ACC_FINAL | ACC_SUPER, className, null, "java/lang/Object", null);

		MethodVisitor clinit = clazz.visitMethod(ACC_STATIC, "<clinit>", voidMethod, null, null);

		clinit.visitCode();

		for (ConstantDescriptor constant : structure.getConstants()) {
			String name = constant.getName();

			clazz.visitField(ACC_PUBLIC | ACC_FINAL | ACC_STATIC, name, "Z", null, null).visitEnd();

			clinit.visitInsn(constant.getValue() == 0 ? ICONST_0 : ICONST_1);
			clinit.visitFieldInsn(PUTSTATIC, className, name, Type.BOOLEAN_TYPE.getDescriptor());
		}

		clinit.visitInsn(RETURN);
		clinit.visitMaxs(1, 0);
		clinit.visitEnd();

		MethodVisitor method = clazz.visitMethod(ACC_PRIVATE, "<init>", voidMethod, null, null);

		method.visitCode();
		method.visitVarInsn(ALOAD, 0);
		method.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", voidMethod, false);
		method.visitInsn(RETURN);
		method.visitMaxs(1, 1);
		method.visitEnd();

		clazz.visitEnd();

		return clazz.toByteArray();
	}

}

abstract class HelperBase implements Opcodes {

	static final String byteFromLong = Type.getMethodDescriptor(Type.BYTE_TYPE, Type.LONG_TYPE);

	static final String doubleFromLong = Type.getMethodDescriptor(Type.DOUBLE_TYPE, Type.LONG_TYPE);

	static final String doubleFromVoid = Type.getMethodDescriptor(Type.DOUBLE_TYPE);

	static final String floatFromLong = Type.getMethodDescriptor(Type.FLOAT_TYPE, Type.LONG_TYPE);

	static final String floatFromVoid = Type.getMethodDescriptor(Type.FLOAT_TYPE);

	static final String intFromLong = Type.getMethodDescriptor(Type.INT_TYPE, Type.LONG_TYPE);

	static final String longFromLong = Type.getMethodDescriptor(Type.LONG_TYPE, Type.LONG_TYPE);

	static final String longFromVoid = Type.getMethodDescriptor(Type.LONG_TYPE);

	static final String shortFromLong = Type.getMethodDescriptor(Type.SHORT_TYPE, Type.LONG_TYPE);

	static final String voidFromLong = Type.getMethodDescriptor(Type.VOID_TYPE, Type.LONG_TYPE);

	static final String voidMethod = Type.getMethodDescriptor(Type.VOID_TYPE);

	static final void addLong(MethodVisitor method, long value) {
		if (value != 0) {
			loadLong(method, value);
			method.visitInsn(LADD);
		}
	}

	static final void loadInt(MethodVisitor method, int value) {
		switch (value) {
		case -1:
			method.visitInsn(ICONST_M1);
			break;
		case 0:
			method.visitInsn(ICONST_0);
			break;
		case 1:
			method.visitInsn(ICONST_1);
			break;
		case 2:
			method.visitInsn(ICONST_2);
			break;
		case 3:
			method.visitInsn(ICONST_3);
			break;
		case 4:
			method.visitInsn(ICONST_4);
			break;
		case 5:
			method.visitInsn(ICONST_5);
			break;
		default:
			method.visitLdcInsn(Integer.valueOf(value));
			break;
		}
	}

	static final void loadLong(MethodVisitor method, long value) {
		if (value == 0) {
			method.visitInsn(LCONST_0);
		} else if (value == 1) {
			method.visitInsn(LCONST_1);
		} else {
			method.visitLdcInsn(Long.valueOf(value));
		}
	}

}

final class PointerHelper extends HelperBase {

	private static final Pattern ConstPattern = Pattern.compile("\\s*\\bconst\\s+");

	private static final Set<String> predefinedDataTypes;

	private static final Set<String> predefinedPointerTypes;

	private static final Pattern TypeTagPattern = Pattern.compile("\\s*\\b(class|enum|struct)\\s+");

	static {
		Set<String> common = addNames(new HashSet<String>(), "I8 U8 I16 U16 I32 U32 I64 U64 IDATA UDATA Void");

		predefinedDataTypes = new HashSet<String>(common);
		addNames(predefinedDataTypes, "Scalar IScalar UScalar");

		predefinedPointerTypes = new HashSet<String>(common);
		addNames(predefinedPointerTypes, "Abstract Bool Double Enum Float ObjectClassReference ObjectMonitorReference");
		addNames(predefinedPointerTypes, "ObjectReference Pointer SelfRelative Structure WideSelfRelative");
	}

	private static Set<String> addNames(Set<String> set, String names) {
		set.addAll(Arrays.asList(names.split(" ")));
		return set;
	}

	private static void doAccessorAnnotation(MethodVisitor method, FieldDescriptor field) {
		Type annotationType = Type.getObjectType("com/ibm/j9ddr/GeneratedFieldAccessor");

		AnnotationVisitor annotation = method.visitAnnotation(annotationType.getDescriptor(), true);

		annotation.visit("offsetFieldName", String.format("_%sOffset_", field.getName()));
		annotation.visit("declaredType", field.getDeclaredType());
		annotation.visitEnd();
	}

	/*
	 * The new DDR tooling doesn't always distinguish between IDATA and I32 or I64
	 * or between UDATA and U32 or U64. Thus the return types of accessor methods
	 * must be more general to be compatible with the pointer types derived from
	 * both 32-bit and 64-bit core files. This generalization must occur even for
	 * the pointer stubs generated from this code so that incompatibilities will
	 * be discovered at build time, rather than at run time.
	 */
	private static String generalizeSimpleType(String type) {
		if ("I32".equals(type) || "I64".equals(type)) {
			return "IDATA";
		} else if ("U32".equals(type) || "U64".equals(type)) {
			return "UDATA";
		} else {
			return type;
		}
	}

	static byte[] getClassBytes(StructureReader reader, StructureTypeManager typeManager, StructureDescriptor structure,
			String className) {
		PointerHelper helper = new PointerHelper(reader, typeManager, structure, className);

		return helper.generate();
	}

	private static String getEnumType(String enumType) {
		int start = enumType.startsWith("enum ") ? 5 : 0;
		int end = enumType.length();

		if (enumType.endsWith("[]")) {
			end -= 2;
		}

		return enumType.substring(start, end);
	}

	private static String getTargetType(String pointerType) {
		String type = ConstPattern.matcher(pointerType).replaceAll("").trim();
		int firstStar = type.indexOf('*');

		if (type.indexOf('*', firstStar + 1) > 0) {
			// two or more levels of indirection
			return "Pointer";
		}

		type = type.substring(0, firstStar).trim();

		if (type.equals("bool")) {
			return "Bool";
		} else if (type.equals("double")) {
			return "Double";
		} else if (type.equals("float")) {
			return "Float";
		} else if (type.equals("void")) {
			return "Void";
		} else {
			return type;
		}
	}

	private static String removeTypeTags(String type) {
		return TypeTagPattern.matcher(type).replaceAll("").trim();
	}

	private final Type abstractPointerType;

	private final String[] accessorThrows;

	private final String basePrefix;

	private final String className;

	private final Type classType;

	private final ClassWriter clazz;

	private final StructureReader reader;

	private final Type scalarType;

	private final StructureDescriptor structure;

	private final StructureTypeManager typeManager;

	private final Type udataType;

	private PointerHelper(StructureReader reader, StructureTypeManager typeManager, StructureDescriptor structure,
			String className) {
		super();

		int index = className.indexOf("/pointer/generated/");

		if (index <= 0) {
			throw new IllegalArgumentException(className);
		}

		String prefix = className.substring(0, index + 1); // ends with '/vmNN/'

		this.abstractPointerType = Type.getObjectType(prefix + "pointer/AbstractPointer");
		this.accessorThrows = new String[] { "com/ibm/j9ddr/CorruptDataException" };
		this.basePrefix = prefix;
		this.className = className;
		this.classType = Type.getObjectType(className);
		this.clazz = new ClassWriter(0);
		this.scalarType = Type.getObjectType(prefix + "types/Scalar");
		this.reader = reader;
		this.structure = structure;
		this.typeManager = typeManager;
		this.udataType = Type.getObjectType(prefix + "types/UDATA");
	}

	private void doAccessorMethods() {
		for (FieldDescriptor field : structure.getFields()) {
			String fieldName = field.getName();

			if (fieldName.isEmpty() || fieldName.indexOf('#') >= 0) {
				// Omit anonymous fields (xlc generates names containing '#').
				continue;
			}

			String typeName = field.getType();
			int type = typeManager.getType(typeName);

			switch (type) {
			case StructureTypeManager.TYPE_STRUCTURE:
				doStructureMethod(field);
				break;
			case StructureTypeManager.TYPE_STRUCTURE_POINTER:
				doStructurePointerMethod(field);
				break;
			case StructureTypeManager.TYPE_POINTER:
				doPointerMethod(field);
				break;
			case StructureTypeManager.TYPE_ARRAY:
				doArrayMethod(field);
				break;
			case StructureTypeManager.TYPE_J9SRP:
				doSRPMethod(field, false);
				break;
			case StructureTypeManager.TYPE_J9WSRP:
				doSRPMethod(field, true);
				break;
			case StructureTypeManager.TYPE_J9SRP_POINTER:
				doSRPPointerMethod(field, false);
				break;
			case StructureTypeManager.TYPE_J9WSRP_POINTER:
				doSRPPointerMethod(field, true);
				break;
			case StructureTypeManager.TYPE_FJ9OBJECT:
				doFJ9ObjectMethod(field);
				break;
			case StructureTypeManager.TYPE_FJ9OBJECT_POINTER:
				doFJ9ObjectPointerMethod(field);
				break;
			case StructureTypeManager.TYPE_J9OBJECTCLASS:
				doJ9ObjectClassMethod(field);
				break;
			case StructureTypeManager.TYPE_J9OBJECTCLASS_POINTER:
				doJ9ObjectClassPointerMethod(field);
				break;
			case StructureTypeManager.TYPE_J9OBJECTMONITOR:
				doJ9ObjectMonitorMethod(field);
				break;
			case StructureTypeManager.TYPE_J9OBJECTMONITOR_POINTER:
				doJ9ObjectMonitorPointerMethod(field);
				break;
			case StructureTypeManager.TYPE_VOID:
				doStructureMethod(field);
				break;
			case StructureTypeManager.TYPE_BOOL:
				doBooleanMethod(field);
				break;
			case StructureTypeManager.TYPE_DOUBLE:
				doDoubleMethod(field);
				break;
			case StructureTypeManager.TYPE_FLOAT:
				doFloatMethod(field);
				break;
			case StructureTypeManager.TYPE_ENUM:
				doEnumMethod(field);
				break;
			case StructureTypeManager.TYPE_ENUM_POINTER:
				doEnumPointerMethod(field);
				break;
			case StructureTypeManager.TYPE_BITFIELD:
				int colonIndex = typeName.indexOf(':');
				if (colonIndex <= 0) {
					throw new IllegalArgumentException(String.format("%s.%s : %s is not a valid bitfield",
							structure.getName(), fieldName, typeName));
				}
				String baseType = typeName.substring(0, colonIndex);
				int width = Integer.parseInt(typeName.substring(colonIndex + 1));
				doBitfieldMethod(field, baseType, width);
				break;

			default:
				if ((StructureTypeManager.TYPE_SIMPLE_MIN <= type) && (type <= StructureTypeManager.TYPE_SIMPLE_MAX)) {
					doSimpleTypeMethod(field, type);
				} else {
					// omit unhandled field type
				}
				break;
			}
		}
	}

	/* Sample generated code:
	 *
	 * public ObjectMonitorReferencePointer objectMonitorLookupCacheEA() throws CorruptDataException {
	 *     return ObjectMonitorReferencePointer.cast(nonNullFieldEA(J9VMThread._objectMonitorLookupCacheOffset_));
	 * }
	 */
	private void doArrayMethod(FieldDescriptor field) {
		String fieldType = field.getType();
		String componentType = fieldType.substring(0, fieldType.lastIndexOf('[')).trim();
		int type = typeManager.getType(componentType);

		switch (type) {
		case StructureTypeManager.TYPE_BOOL:
			doEAMethod("Bool", field);
			return;
		case StructureTypeManager.TYPE_DOUBLE:
			doEAMethod("Double", field);
			return;
		case StructureTypeManager.TYPE_ENUM:
			doEnumEAMethod(field);
			return;
		case StructureTypeManager.TYPE_FLOAT:
			doEAMethod("Float", field);
			return;
		case StructureTypeManager.TYPE_FJ9OBJECT:
			doEAMethod("ObjectReference", field);
			return;
		case StructureTypeManager.TYPE_J9OBJECTCLASS:
			doEAMethod("ObjectClassReference", field);
			return;
		case StructureTypeManager.TYPE_J9OBJECTMONITOR:
			doEAMethod("ObjectMonitorReference", field);
			return;
		case StructureTypeManager.TYPE_STRUCTURE:
			doEAMethod(removeTypeTags(componentType), field);
			return;
		case StructureTypeManager.TYPE_ARRAY:
		case StructureTypeManager.TYPE_FJ9OBJECT_POINTER:
		case StructureTypeManager.TYPE_J9OBJECTCLASS_POINTER:
		case StructureTypeManager.TYPE_J9OBJECTMONITOR_POINTER:
		case StructureTypeManager.TYPE_POINTER:
		case StructureTypeManager.TYPE_STRUCTURE_POINTER:
			// All these pointer types look the same.
			doEAMethod("Pointer", field);
			return;
		default:
			if ((StructureTypeManager.TYPE_SIMPLE_MIN <= type) && (type <= StructureTypeManager.TYPE_SIMPLE_MAX)) {
				doEAMethod(componentType, field);
				return;
			}
			break;
		case StructureTypeManager.TYPE_BITFIELD: // Is a bitfield array even legal?
		case StructureTypeManager.TYPE_J9SRP: // not implemented
		case StructureTypeManager.TYPE_J9WSRP: // not implemented
			break;
		}

		throw new IllegalArgumentException("Unrecognized array: " + fieldType);
	}

	/* Sample generated code:
	 *
	 * @com.ibm.j9ddr.GeneratedFieldAccessor(offsetFieldName="_isDirOffset_", declaredType="uint32_t:1")
	 * public U32 isDir() throws CorruptDataException {
	 *     return getU32Bitfield(J9FileStat._isDir_s_, J9FileStat._isDir_b_);
	 * }
	 */
	private void doBitfieldMethod(FieldDescriptor field, String baseType, int width) {
		String fieldName = field.getName();
		Type qualifiedBaseType = Type.getObjectType(qualifyType(baseType));
		Type qualifiedReturnType = Type.getObjectType(qualifyType(generalizeSimpleType(baseType)));
		String returnDesc = Type.getMethodDescriptor(qualifiedReturnType);
		String startFieldName = String.format("_%s_s_", fieldName);
		String accessorName = String.format("get%sBitfield", baseType);
		String accessorDesc = Type.getMethodDescriptor(qualifiedBaseType, Type.INT_TYPE, Type.INT_TYPE);

		MethodVisitor method = clazz.visitMethod(ACC_PUBLIC, fieldName, returnDesc, null, accessorThrows);

		doAccessorAnnotation(method, field);

		method.visitCode();
		method.visitVarInsn(ALOAD, 0);
		method.visitFieldInsn(GETSTATIC, getStructureClassName(), startFieldName, Type.INT_TYPE.getDescriptor());
		loadInt(method, width);
		method.visitMethodInsn(INVOKEVIRTUAL, className, accessorName, accessorDesc, false);
		method.visitInsn(ARETURN);
		method.visitMaxs(3, 1);
		method.visitEnd();

		// no EA method for a bitfield
	}

	/* Sample generated code:
	 *
	 * @com.ibm.j9ddr.GeneratedFieldAccessor(offsetFieldName="__isAnonOffset_", declaredType="bool")
	 * public boolean _isAnon() throws CorruptDataException {
	 *     return getBoolAtOffset(ClassFileWriter.__isAnonOffset_);
	 * }
	 */
	private void doBooleanMethod(FieldDescriptor field) {
		String returnDesc = Type.getMethodDescriptor(Type.BOOLEAN_TYPE);
		String accessorDesc = Type.getMethodDescriptor(Type.BOOLEAN_TYPE, Type.LONG_TYPE);

		MethodVisitor method = clazz.visitMethod(ACC_PUBLIC, field.getName(), returnDesc, null, accessorThrows);

		doAccessorAnnotation(method, field);

		method.visitCode();
		method.visitVarInsn(ALOAD, 0);
		loadLong(method, field.getOffset());
		method.visitMethodInsn(INVOKEVIRTUAL, className, "getBoolAtOffset", accessorDesc, false);
		method.visitInsn(IRETURN);
		method.visitMaxs(3, 1);
		method.visitEnd();

		doEAMethod("Bool", field);
	}

	private void doClassAnnotation() {
		// @com.ibm.j9ddr.GeneratedPointerClass(structureClass=BASE.class)
		Type annotationType = Type.getObjectType("com/ibm/j9ddr/GeneratedPointerClass");
		AnnotationVisitor annotation = clazz.visitAnnotation(annotationType.getDescriptor(), true);
		Type structureType = Type.getObjectType(getStructureClassName());

		annotation.visit("structureClass", structureType);
		annotation.visitEnd();
	}

	private void doConstructors(String superClassName) {
		final String abstractPointerFromLong = Type.getMethodDescriptor(abstractPointerType, Type.LONG_TYPE);
		final String abstractPointerFromScalar = Type.getMethodDescriptor(abstractPointerType, scalarType);
		final String classFromLong = Type.getMethodDescriptor(classType, Type.LONG_TYPE);
		final String classFromScalar = Type.getMethodDescriptor(classType, scalarType);
		MethodVisitor method;

		// protected SELF(long address) { super(address); }
		{
			method = clazz.visitMethod(ACC_PROTECTED, "<init>", voidFromLong, null, null);

			method.visitCode();
			method.visitVarInsn(ALOAD, 0);
			method.visitVarInsn(LLOAD, 1);
			method.visitMethodInsn(INVOKESPECIAL, superClassName, "<init>", voidFromLong, false);
			method.visitInsn(RETURN);
			method.visitMaxs(3, 3);
			method.visitEnd();
		}

		// public static SELF cast(long address) { if (address == 0) return NULL; return new SELF(address); }
		{
			method = clazz.visitMethod(ACC_PUBLIC | ACC_STATIC, "cast", classFromLong, null, null);

			Label nonNull = new Label();

			method.visitCode();
			method.visitVarInsn(LLOAD, 0);
			method.visitInsn(LCONST_0);
			method.visitInsn(LCMP);
			method.visitJumpInsn(IFNE, nonNull);
			method.visitFieldInsn(GETSTATIC, className, "NULL", classType.getDescriptor());
			method.visitInsn(ARETURN);

			method.visitLabel(nonNull);
			method.visitFrame(F_SAME, 0, null, 0, null);
			method.visitTypeInsn(NEW, className);
			method.visitInsn(DUP);
			method.visitVarInsn(LLOAD, 0);
			method.visitMethodInsn(INVOKESPECIAL, className, "<init>", voidFromLong, false);
			method.visitInsn(ARETURN);

			method.visitMaxs(4, 2);
			method.visitEnd();
		}

		// public static SELF cast(AbstractPointer structure) { return cast(structure.getAddress()); }
		{
			method = clazz.visitMethod(ACC_PUBLIC | ACC_STATIC, "cast",
					Type.getMethodDescriptor(classType, abstractPointerType), null, null);

			method.visitCode();
			method.visitVarInsn(ALOAD, 0);
			method.visitMethodInsn(INVOKEVIRTUAL, abstractPointerType.getInternalName(), "getAddress", longFromVoid,
					false);
			method.visitMethodInsn(INVOKESTATIC, className, "cast", classFromLong, false);
			method.visitInsn(ARETURN);
			method.visitMaxs(2, 1);
			method.visitEnd();
		}

		// public static SELF cast(UDATA udata) { return cast(udata.longValue()); }
		{
			method = clazz.visitMethod(ACC_PUBLIC | ACC_STATIC, "cast", Type.getMethodDescriptor(classType, udataType),
					null, null);

			method.visitCode();
			method.visitVarInsn(ALOAD, 0);
			method.visitMethodInsn(INVOKEVIRTUAL, udataType.getInternalName(), "longValue", longFromVoid, false);
			method.visitMethodInsn(INVOKESTATIC, className, "cast", classFromLong, false);
			method.visitInsn(ARETURN);
			method.visitMaxs(2, 1);
			method.visitEnd();
		}

		// public SELF add(long count) { return addOffset(count * BASE.SIZEOF); }
		{
			method = clazz.visitMethod(ACC_PUBLIC, "add", classFromLong, null, null);

			method.visitCode();
			method.visitVarInsn(ALOAD, 0);
			method.visitVarInsn(LLOAD, 1);
			loadLong(method, structure.getSizeOf());
			method.visitInsn(LMUL);
			method.visitMethodInsn(INVOKEVIRTUAL, className, "addOffset", classFromLong, false);
			method.visitInsn(ARETURN);
			method.visitMaxs(5, 3);
			method.visitEnd();

			// bridge: AbstractPointer add(long count)
			method = clazz.visitMethod(ACC_PUBLIC | ACC_BRIDGE | ACC_SYNTHETIC, "add", abstractPointerFromLong, null, null);

			method.visitCode();
			method.visitVarInsn(ALOAD, 0);
			method.visitVarInsn(LLOAD, 1);
			method.visitMethodInsn(INVOKEVIRTUAL, className, "add", classFromLong, false);
			method.visitInsn(ARETURN);
			method.visitMaxs(3, 3);
			method.visitEnd();
		}

		// public SELF add(Scalar count) { return add(count.longValue()); }
		{
			method = clazz.visitMethod(ACC_PUBLIC, "add", classFromScalar, null, null);

			method.visitCode();
			method.visitVarInsn(ALOAD, 0);
			method.visitVarInsn(ALOAD, 1);
			method.visitMethodInsn(INVOKEVIRTUAL, scalarType.getInternalName(), "longValue", longFromVoid, false);
			method.visitMethodInsn(INVOKEVIRTUAL, className, "add", classFromLong, false);
			method.visitInsn(ARETURN);
			method.visitMaxs(3, 2);
			method.visitEnd();

			// bridge: AbstractPointer add(Scalar count)
			method = clazz.visitMethod(ACC_PUBLIC | ACC_BRIDGE | ACC_SYNTHETIC, "add", abstractPointerFromScalar, null, null);

			method.visitCode();
			method.visitVarInsn(ALOAD, 0);
			method.visitVarInsn(ALOAD, 1);
			method.visitMethodInsn(INVOKEVIRTUAL, className, "add", classFromScalar, false);
			method.visitInsn(ARETURN);
			method.visitMaxs(2, 2);
			method.visitEnd();
		}

		// public SELF addOffset(long offset) { return cast(address + offset); }
		{
			method = clazz.visitMethod(ACC_PUBLIC, "addOffset", classFromLong, null, null);

			method.visitCode();
			method.visitVarInsn(ALOAD, 0);
			method.visitFieldInsn(GETFIELD, className, "address", Type.LONG_TYPE.getDescriptor());
			method.visitVarInsn(LLOAD, 1);
			method.visitInsn(LADD);
			method.visitMethodInsn(INVOKESTATIC, className, "cast", classFromLong, false);
			method.visitInsn(ARETURN);
			method.visitMaxs(4, 3);
			method.visitEnd();

			// bridge: AbstractPointer addOffset(long offset)
			method = clazz.visitMethod(ACC_PUBLIC | ACC_BRIDGE | ACC_SYNTHETIC, "addOffset", abstractPointerFromLong, null, null);

			method.visitCode();
			method.visitVarInsn(ALOAD, 0);
			method.visitVarInsn(LLOAD, 1);
			method.visitMethodInsn(INVOKEVIRTUAL, className, "addOffset", classFromLong, false);
			method.visitInsn(ARETURN);
			method.visitMaxs(3, 3);
			method.visitEnd();
		}

		// public SELF addOffset(Scalar offset) { return addOffset(offset.longValue()); }
		{
			method = clazz.visitMethod(ACC_PUBLIC, "addOffset", classFromScalar, null, null);

			method.visitCode();
			method.visitVarInsn(ALOAD, 0);
			method.visitVarInsn(ALOAD, 1);
			method.visitMethodInsn(INVOKEVIRTUAL, scalarType.getInternalName(), "longValue", longFromVoid, false);
			method.visitMethodInsn(INVOKEVIRTUAL, className, "addOffset", classFromLong, false);
			method.visitInsn(ARETURN);
			method.visitMaxs(3, 2);
			method.visitEnd();

			// bridge: AbstractPointer addOffset(Scalar offset)
			method = clazz.visitMethod(ACC_PUBLIC | ACC_BRIDGE | ACC_SYNTHETIC, "addOffset", abstractPointerFromScalar, null, null);

			method.visitCode();
			method.visitVarInsn(ALOAD, 0);
			method.visitVarInsn(ALOAD, 1);
			method.visitMethodInsn(INVOKEVIRTUAL, className, "addOffset", classFromScalar, false);
			method.visitInsn(ARETURN);
			method.visitMaxs(2, 2);
			method.visitEnd();
		}

		// public SELF sub(long count) { return subOffset(count * SIZEOF); }
		{
			method = clazz.visitMethod(ACC_PUBLIC, "sub", classFromLong, null, null);

			method.visitCode();
			method.visitVarInsn(ALOAD, 0);
			method.visitVarInsn(LLOAD, 1);
			loadLong(method, structure.getSizeOf());
			method.visitInsn(LMUL);
			method.visitMethodInsn(INVOKEVIRTUAL, className, "subOffset", classFromLong, false);
			method.visitInsn(ARETURN);
			method.visitMaxs(5, 3);
			method.visitEnd();

			// bridge: AbstractPointer sub(long count)
			method = clazz.visitMethod(ACC_PUBLIC | ACC_BRIDGE | ACC_SYNTHETIC, "sub", abstractPointerFromLong, null, null);

			method.visitCode();
			method.visitVarInsn(ALOAD, 0);
			method.visitVarInsn(LLOAD, 1);
			method.visitMethodInsn(INVOKEVIRTUAL, className, "sub", classFromLong, false);
			method.visitInsn(ARETURN);
			method.visitMaxs(3, 3);
			method.visitEnd();
		}

		// public SELF sub(Scalar count) { return sub(count.longValue()); }
		{
			method = clazz.visitMethod(ACC_PUBLIC, "sub", classFromScalar, null, null);

			method.visitCode();
			method.visitVarInsn(ALOAD, 0);
			method.visitVarInsn(ALOAD, 1);
			method.visitMethodInsn(INVOKEVIRTUAL, scalarType.getInternalName(), "longValue", longFromVoid, false);
			method.visitMethodInsn(INVOKEVIRTUAL, className, "sub", classFromLong, false);
			method.visitInsn(ARETURN);
			method.visitMaxs(3, 2);
			method.visitEnd();

			// bridge: AbstractPointer sub(Scalar count)
			method = clazz.visitMethod(ACC_PUBLIC | ACC_BRIDGE | ACC_SYNTHETIC, "sub", abstractPointerFromScalar, null, null);

			method.visitCode();
			method.visitVarInsn(ALOAD, 0);
			method.visitVarInsn(ALOAD, 1);
			method.visitMethodInsn(INVOKEVIRTUAL, className, "sub", classFromScalar, false);
			method.visitInsn(ARETURN);
			method.visitMaxs(2, 2);
			method.visitEnd();
		}

		// public SELF subOffset(long offset) { return cast(address - offset); }
		{
			method = clazz.visitMethod(ACC_PUBLIC, "subOffset", classFromLong, null, null);

			method.visitCode();
			method.visitVarInsn(ALOAD, 0);
			method.visitFieldInsn(GETFIELD, className, "address", Type.LONG_TYPE.getDescriptor());
			method.visitVarInsn(LLOAD, 1);
			method.visitInsn(LSUB);
			method.visitMethodInsn(INVOKESTATIC, className, "cast", classFromLong, false);
			method.visitInsn(ARETURN);
			method.visitMaxs(5, 3);
			method.visitEnd();

			// bridge: AbstractPointer subOffset(long offset)
			method = clazz.visitMethod(ACC_PUBLIC | ACC_BRIDGE | ACC_SYNTHETIC, "subOffset", abstractPointerFromLong, null, null);

			method.visitCode();
			method.visitVarInsn(ALOAD, 0);
			method.visitVarInsn(LLOAD, 1);
			method.visitMethodInsn(INVOKEVIRTUAL, className, "subOffset", classFromLong, false);
			method.visitInsn(ARETURN);
			method.visitMaxs(3, 3);
			method.visitEnd();
		}

		// public SELF subOffset(Scalar offset) { return subOffset(offset.longValue()); }
		{
			method = clazz.visitMethod(ACC_PUBLIC, "subOffset", classFromScalar, null, null);

			method.visitCode();
			method.visitVarInsn(ALOAD, 0);
			method.visitVarInsn(ALOAD, 1);
			method.visitMethodInsn(INVOKEVIRTUAL, scalarType.getInternalName(), "longValue", longFromVoid, false);
			method.visitMethodInsn(INVOKEVIRTUAL, className, "subOffset", classFromLong, false);
			method.visitInsn(ARETURN);
			method.visitMaxs(3, 2);
			method.visitEnd();

			// bridge: AbstractPointer subOffset(Scalar offset)
			method = clazz.visitMethod(ACC_PUBLIC | ACC_BRIDGE | ACC_SYNTHETIC, "subOffset", abstractPointerFromScalar, null, null);

			method.visitCode();
			method.visitVarInsn(ALOAD, 0);
			method.visitVarInsn(ALOAD, 1);
			method.visitMethodInsn(INVOKEVIRTUAL, className, "subOffset", classFromScalar, false);
			method.visitInsn(ARETURN);
			method.visitMaxs(2, 2);
			method.visitEnd();
		}

		// public SELF untag(long mask) { return cast(address & ~mask); }
		{
			method = clazz.visitMethod(ACC_PUBLIC, "untag", classFromLong, null, null);

			method.visitCode();
			method.visitVarInsn(ALOAD, 0);
			method.visitFieldInsn(GETFIELD, className, "address", Type.LONG_TYPE.getDescriptor());
			method.visitVarInsn(LLOAD, 1);
			loadLong(method, -1);
			method.visitInsn(LXOR);
			method.visitInsn(LAND);

			method.visitMethodInsn(INVOKESTATIC, className, "cast", classFromLong, false);
			method.visitInsn(ARETURN);
			method.visitMaxs(6, 3);
			method.visitEnd();

			// bridge: AbstractPointer untag(long tagBits)
			method = clazz.visitMethod(ACC_PUBLIC | ACC_BRIDGE | ACC_SYNTHETIC, "untag", abstractPointerFromLong, null, null);

			method.visitCode();
			method.visitVarInsn(ALOAD, 0);
			method.visitVarInsn(LLOAD, 1);
			method.visitMethodInsn(INVOKEVIRTUAL, className, "untag", classFromLong, false);
			method.visitInsn(ARETURN);
			method.visitMaxs(3, 3);
			method.visitEnd();
		}

		// public SELF untag() { return untag(UDATA.SIZEOF - 1); }
		{
			String classFromVoid = Type.getMethodDescriptor(classType);

			method = clazz.visitMethod(ACC_PUBLIC, "untag", classFromVoid, null, null);

			method.visitCode();
			method.visitVarInsn(ALOAD, 0);
			loadLong(method, reader.getSizeOfUDATA() - 1);
			method.visitMethodInsn(INVOKEVIRTUAL, className, "untag", classFromLong, false);
			method.visitInsn(ARETURN);
			method.visitMaxs(3, 1);
			method.visitEnd();

			// bridge: AbstractPointer untag()
			method = clazz.visitMethod(ACC_PUBLIC | ACC_BRIDGE | ACC_SYNTHETIC, "untag", Type.getMethodDescriptor(abstractPointerType), null, null);

			method.visitCode();
			method.visitVarInsn(ALOAD, 0);
			method.visitMethodInsn(INVOKEVIRTUAL, className, "untag", classFromVoid, false);
			method.visitInsn(ARETURN);
			method.visitMaxs(1, 1);
			method.visitEnd();
		}

		// protected long sizeOfBaseType() { return BASE.SIZEOF; }
		{
			method = clazz.visitMethod(ACC_PROTECTED, "sizeOfBaseType", longFromVoid, null, null);

			method.visitCode();
			loadLong(method, structure.getSizeOf());
			method.visitInsn(LRETURN);
			method.visitMaxs(2, 1);
			method.visitEnd();
		}
	}

	/* Sample generated code:
	 *
	 * @com.ibm.j9ddr.GeneratedFieldAccessor(offsetFieldName="_fifteenMinuteAverageOffset_", declaredType="double")
	 * public double fifteenMinuteAverage() throws CorruptDataException {
	 *     return getDoubleAtOffset(J9PortSysInfoLoadData._fifteenMinuteAverageOffset_);
	 * }
	 */
	private void doDoubleMethod(FieldDescriptor field) {
		MethodVisitor method = clazz.visitMethod(ACC_PUBLIC, field.getName(), doubleFromVoid, null, accessorThrows);

		doAccessorAnnotation(method, field);

		method.visitCode();
		method.visitVarInsn(ALOAD, 0);
		loadLong(method, field.getOffset());
		method.visitMethodInsn(INVOKEVIRTUAL, className, "getDoubleAtOffset", doubleFromLong, false);
		method.visitInsn(DRETURN);
		method.visitMaxs(3, 1);
		method.visitEnd();

		doEAMethod("Double", field);
	}

	/* Sample generated code:
	 *
	 * public UDATAPointer castClassCacheEA() throws CorruptDataException {
	 *     return UDATAPointer.cast(nonNullFieldEA(J9Class._castClassCacheOffset_));
	 * }
	 */
	private void doEAMethod(String actualType, FieldDescriptor field) {
		String accessorName = field.getName() + "EA";
		String returnType = generalizeSimpleType(actualType);
		String qualifiedReturnType = qualifyPointerType(returnType);
		String qualifiedActualType = qualifyPointerType(actualType);

		String returnDesc = Type.getMethodDescriptor(Type.getObjectType(qualifiedReturnType));
		String actualDesc = Type.getMethodDescriptor(Type.getObjectType(qualifiedActualType), Type.LONG_TYPE);

		MethodVisitor method = clazz.visitMethod(ACC_PUBLIC, accessorName, returnDesc, null, accessorThrows);

		method.visitCode();
		method.visitVarInsn(ALOAD, 0);
		loadLong(method, field.getOffset());
		method.visitMethodInsn(INVOKEVIRTUAL, className, "nonNullFieldEA", longFromLong, false);
		method.visitMethodInsn(INVOKESTATIC, qualifiedActualType, "cast", actualDesc, false);
		method.visitInsn(ARETURN);
		method.visitMaxs(3, 1);
		method.visitEnd();
	}

	/* Sample generated code:
	 *
	 * public EnumPointer _buildResultEA() throws CorruptDataException {
	 *     return EnumPointer.cast(nonNullFieldEA(ClassFileWriter.__buildResultOffset_, BuildResult.class));
	 * }
	 */
	private void doEnumEAMethod(FieldDescriptor field) {
		String accessorName = field.getName() + "EA";
		String enumPointerDesc = qualifyPointerType("Enum");
		Type enumPointerType = Type.getObjectType(enumPointerDesc);
		String returnDesc = Type.getMethodDescriptor(enumPointerType);
		String castDesc = Type.getMethodDescriptor(enumPointerType, Type.LONG_TYPE, Type.getType(Class.class));
		Type enumType = Type.getObjectType(qualifyType(getEnumType(field.getType())));

		MethodVisitor method = clazz.visitMethod(ACC_PUBLIC, accessorName, returnDesc, null, accessorThrows);

		method.visitCode();
		method.visitVarInsn(ALOAD, 0);
		loadLong(method, field.getOffset());
		method.visitMethodInsn(INVOKEVIRTUAL, className, "nonNullFieldEA", longFromLong, false);
		method.visitLdcInsn(enumType);
		method.visitMethodInsn(INVOKESTATIC, enumPointerDesc, "cast", castDesc, false);
		method.visitInsn(ARETURN);
		method.visitMaxs(3, 1);
		method.visitEnd();
	}

	/* Sample generated code:
	 *
	 * @com.ibm.j9ddr.GeneratedFieldAccessor(offsetFieldName="_physicalProcessorOffset_", declaredType="J9ProcessorArchitecture")
	 * public long physicalProcessor() throws CorruptDataException {
	 *     return getIntAtOffset(J9ProcessorDesc._physicalProcessorOffset_);
	 * }
	 *
	 * If the size of the field type is not 4, getByteAtOffset, getShortAtOffset
	 * or getLongAtOffset is used as appropriate.
	 */
	private void doEnumMethod(FieldDescriptor field) {
		String enumType = getEnumType(field.getType());
		int enumSize = reader.getStructureSizeOf(enumType);
		PrimitiveAccessor accessor = PrimitiveAccessor.forSize(enumSize);

		MethodVisitor method = clazz.visitMethod(ACC_PUBLIC, field.getName(), longFromVoid, null, accessorThrows);

		doAccessorAnnotation(method, field);

		method.visitCode();
		method.visitVarInsn(ALOAD, 0);
		loadLong(method, field.getOffset());
		method.visitMethodInsn(INVOKEVIRTUAL, className, accessor.methodName, accessor.descriptor, false);
		if (!accessor.returnsLong) {
			method.visitInsn(I2L);
		}
		method.visitInsn(LRETURN);
		method.visitMaxs(3, 1);
		method.visitEnd();

		doEnumEAMethod(field);
	}

	/* Sample generated code:
	 *
	 * @com.ibm.j9ddr.GeneratedFieldAccessor(offsetFieldName="__resultOffset_", declaredType="BuildResult*")
	 * public EnumPointer _result() throws CorruptDataException {
	 *     return EnumPointer.cast(getPointerAtOffset(ROMClassVerbosePhase.__resultOffset_), BuildResult.class);
	 * }
	 */
	private void doEnumPointerMethod(FieldDescriptor field) {
		String enumPointerDesc = qualifyPointerType("Enum");
		Type enumPointerType = Type.getObjectType(enumPointerDesc);
		String returnDesc = Type.getMethodDescriptor(enumPointerType);
		String castDesc = Type.getMethodDescriptor(enumPointerType, Type.LONG_TYPE, Type.getType(Class.class));
		String targetType = getTargetType(getEnumType(field.getType()));
		Type enumType = Type.getObjectType(qualifyType(targetType));

		MethodVisitor method = clazz.visitMethod(ACC_PUBLIC, field.getName(), returnDesc, null, accessorThrows);

		method.visitCode();
		method.visitVarInsn(ALOAD, 0);
		loadLong(method, field.getOffset());
		method.visitMethodInsn(INVOKEVIRTUAL, className, "getPointerAtOffset", longFromLong, false);
		method.visitLdcInsn(enumType);
		method.visitMethodInsn(INVOKESTATIC, enumPointerDesc, "cast", castDesc, false);
		method.visitInsn(ARETURN);
		method.visitMaxs(3, 2);
		method.visitEnd();

		doEAMethod("Pointer", field);
	}

	/* Sample generated code:
	 *
	 * @com.ibm.j9ddr.GeneratedFieldAccessor(offsetFieldName="_refOffset_", declaredType="fj9object_t")
	 * public J9ObjectPointer ref() throws CorruptDataException {
	 *     return getObjectReferenceAtOffset(Example._refOffset_);
	 * }
	 */
	private void doFJ9ObjectMethod(FieldDescriptor field) {
		Type objectType = Type.getObjectType(qualifyPointerType("J9Object"));
		String returnDesc = Type.getMethodDescriptor(objectType);
		String accessorDesc = Type.getMethodDescriptor(objectType, Type.LONG_TYPE);

		MethodVisitor method = clazz.visitMethod(ACC_PUBLIC, field.getName(), returnDesc, null, accessorThrows);

		doAccessorAnnotation(method, field);

		method.visitCode();
		method.visitVarInsn(ALOAD, 0);
		loadLong(method, field.getOffset());
		method.visitMethodInsn(INVOKEVIRTUAL, className, "getObjectReferenceAtOffset", accessorDesc, false);
		method.visitInsn(ARETURN);
		method.visitMaxs(3, 1);
		method.visitEnd();

		doEAMethod("ObjectReference", field);
	}

	/* Sample generated code:
	 *
	 * @com.ibm.j9ddr.GeneratedFieldAccessor(offsetFieldName="_fieldAddressOffset_", declaredType="const fj9object_t*")
	 * public ObjectReferencePointer fieldAddress() throws CorruptDataException {
	 *     return ObjectReferencePointer.cast(getPointerAtOffset(J9MM_IterateObjectRefDescriptor._fieldAddressOffset_));
	 * }
	 */
	private void doFJ9ObjectPointerMethod(FieldDescriptor field) {
		String returnType = qualifyPointerType("ObjectReference");
		Type objectType = Type.getObjectType(returnType);
		String returnDesc = Type.getMethodDescriptor(objectType);
		String castDesc = Type.getMethodDescriptor(objectType, Type.LONG_TYPE);

		MethodVisitor method = clazz.visitMethod(ACC_PUBLIC, field.getName(), returnDesc, null, accessorThrows);

		doAccessorAnnotation(method, field);

		method.visitCode();
		method.visitVarInsn(ALOAD, 0);
		loadLong(method, field.getOffset());
		method.visitMethodInsn(INVOKEVIRTUAL, className, "getPointerAtOffset", longFromLong, false);
		method.visitMethodInsn(INVOKESTATIC, returnType, "cast", castDesc, false);
		method.visitInsn(ARETURN);
		method.visitMaxs(3, 1);
		method.visitEnd();

		doEAMethod("Pointer", field);
	}

	/* Sample generated code:
	 *
	 * @com.ibm.j9ddr.GeneratedFieldAccessor(offsetFieldName="_fOffset_", declaredType="jfloat")
	 * public float f() throws CorruptDataException {
	 *     return getFloatAtOffset(jvalue._fOffset_);
	 * }
	 */
	private void doFloatMethod(FieldDescriptor field) {
		MethodVisitor method = clazz.visitMethod(ACC_PUBLIC, field.getName(), floatFromVoid, null, accessorThrows);

		doAccessorAnnotation(method, field);

		method.visitCode();
		method.visitVarInsn(ALOAD, 0);
		loadLong(method, field.getOffset());
		method.visitMethodInsn(INVOKEVIRTUAL, className, "getFloatAtOffset", floatFromLong, false);
		method.visitInsn(FRETURN);
		method.visitMaxs(3, 1);
		method.visitEnd();

		doEAMethod("Float", field);
	}

	/* Sample generated code:
	 *
	 * @com.ibm.j9ddr.GeneratedFieldAccessor(offsetFieldName="_clazzOffset_", declaredType="j9objectclass_t")
	 * public J9ClassPointer clazz() throws CorruptDataException {
	 *     return getObjectClassAtOffset(J9Object._clazzOffset_);
	 * }
	 */
	private void doJ9ObjectClassMethod(FieldDescriptor field) {
		String returnType = qualifyPointerType("J9Class");
		Type objectType = Type.getObjectType(returnType);
		String returnDesc = Type.getMethodDescriptor(objectType);
		String accessorDesc = Type.getMethodDescriptor(objectType, Type.LONG_TYPE);

		MethodVisitor method = clazz.visitMethod(ACC_PUBLIC, field.getName(), returnDesc, null, accessorThrows);

		doAccessorAnnotation(method, field);

		method.visitCode();
		method.visitVarInsn(ALOAD, 0);
		loadLong(method, field.getOffset());
		method.visitMethodInsn(INVOKEVIRTUAL, className, "getObjectClassAtOffset", accessorDesc, false);
		method.visitInsn(ARETURN);
		method.visitMaxs(3, 1);
		method.visitEnd();

		doEAMethod("ObjectClassReference", field);
	}

	/* Sample generated code:
	 *
	 * @com.ibm.j9ddr.GeneratedFieldAccessor(offsetFieldName="_classPtrOffset_", declaredType="j9objectclass_t*")
	 * public ObjectClassReferencePointer classPtr() throws CorruptDataException {
	 *     return ObjectClassReferencePointer.cast(getPointerAtOffset(Example._classPtrOffset_));
	 * }
	 */
	private void doJ9ObjectClassPointerMethod(FieldDescriptor field) {
		String returnType = qualifyPointerType("ObjectClassReference");
		Type objectType = Type.getObjectType(returnType);
		String returnDesc = Type.getMethodDescriptor(objectType);
		String accessorDesc = Type.getMethodDescriptor(objectType, Type.LONG_TYPE);

		MethodVisitor method = clazz.visitMethod(ACC_PUBLIC, field.getName(), returnDesc, null, accessorThrows);

		doAccessorAnnotation(method, field);

		method.visitCode();
		method.visitVarInsn(ALOAD, 0);
		loadLong(method, field.getOffset());
		method.visitMethodInsn(INVOKEVIRTUAL, className, "getPointerAtOffset", longFromLong, false);
		method.visitMethodInsn(INVOKESTATIC, returnType, "cast", accessorDesc, false);
		method.visitInsn(ARETURN);
		method.visitMaxs(3, 1);
		method.visitEnd();

		doEAMethod("Pointer", field);
	}

	/* Sample generated code:
	 *
	 * @com.ibm.j9ddr.GeneratedFieldAccessor(offsetFieldName="_monitorOffset_", declaredType="j9objectmonitor_t")
	 * public J9ObjectMonitorPointer monitor() throws CorruptDataException {
	 *     return getObjectMonitorAtOffset(Example._monitorOffset_);
	 * }
	 */
	private void doJ9ObjectMonitorMethod(FieldDescriptor field) {
		String returnType = qualifyPointerType("J9ObjectMonitor");
		Type objectType = Type.getObjectType(returnType);
		String returnDesc = Type.getMethodDescriptor(objectType);
		String accessorDesc = Type.getMethodDescriptor(objectType, Type.LONG_TYPE);

		MethodVisitor method = clazz.visitMethod(ACC_PUBLIC, field.getName(), returnDesc, null, accessorThrows);

		doAccessorAnnotation(method, field);

		method.visitCode();
		method.visitVarInsn(ALOAD, 0);
		loadLong(method, field.getOffset());
		method.visitMethodInsn(INVOKEVIRTUAL, className, "getObjectMonitorAtOffset", accessorDesc, false);
		method.visitInsn(ARETURN);
		method.visitMaxs(3, 1);
		method.visitEnd();

		doEAMethod("ObjectMonitorReference", field);
	}

	/* Sample generated code:
	 *
	 * @com.ibm.j9ddr.GeneratedFieldAccessor(offsetFieldName="_lockwordOffset_", declaredType="j9objectmonitor_t*")
	 * public ObjectMonitorReferencePointer lockword() throws CorruptDataException {
	 *     return ObjectMonitorReferencePointer.cast(getPointerAtOffset(Example._lockwordOffset_));
	 * }
	 */
	private void doJ9ObjectMonitorPointerMethod(FieldDescriptor field) {
		String returnType = qualifyPointerType("ObjectMonitorReference");
		Type objectType = Type.getObjectType(returnType);
		String returnDesc = Type.getMethodDescriptor(objectType);
		String castDesc = Type.getMethodDescriptor(objectType, Type.LONG_TYPE);

		MethodVisitor method = clazz.visitMethod(ACC_PUBLIC, field.getName(), returnDesc, null, accessorThrows);

		doAccessorAnnotation(method, field);

		method.visitCode();
		method.visitVarInsn(ALOAD, 0);
		loadLong(method, field.getOffset());
		method.visitMethodInsn(INVOKEVIRTUAL, className, "getPointerAtOffset", longFromLong, false);
		method.visitMethodInsn(INVOKESTATIC, returnType, "cast", castDesc, false);
		method.visitInsn(ARETURN);
		method.visitMaxs(3, 1);
		method.visitEnd();

		doEAMethod("Pointer", field);
	}

	private void doNullInstance() {
		// public static final SELF NULL = new SELF(0);
		clazz.visitField(ACC_PUBLIC | ACC_FINAL | ACC_STATIC, "NULL", classType.getDescriptor(), null, null).visitEnd();

		MethodVisitor clinit = clazz.visitMethod(ACC_STATIC, "<clinit>", voidMethod, null, null);

		clinit.visitCode();
		clinit.visitTypeInsn(NEW, className);
		clinit.visitInsn(DUP);
		clinit.visitInsn(LCONST_0);
		clinit.visitMethodInsn(INVOKESPECIAL, className, "<init>", voidFromLong, false);
		clinit.visitFieldInsn(PUTSTATIC, className, "NULL", classType.getDescriptor());
		clinit.visitInsn(RETURN);
		clinit.visitMaxs(4, 0);
		clinit.visitEnd();
	}

	/* Sample generated code:
	 *
	 * @com.ibm.j9ddr.GeneratedFieldAccessor(offsetFieldName="_ramConstantPoolOffset_", declaredType="UDATA*")
	 * public UDATAPointer ramConstantPool() throws CorruptDataException {
	 *     return UDATAPointer.cast(getPointerAtOffset(J9Class._ramConstantPoolOffset_));
	 * }
	 */
	private void doPointerMethod(FieldDescriptor field) {
		String targetType = getTargetType(removeTypeTags(field.getType()));
		String qualifiedTargetType = qualifyPointerType(targetType);
		String castDesc = Type.getMethodDescriptor(Type.getObjectType(qualifiedTargetType), Type.LONG_TYPE);

		String returnType = generalizeSimpleType(targetType);
		String qualifiedReturnType = qualifyPointerType(returnType);
		String returnDesc = Type.getMethodDescriptor(Type.getObjectType(qualifiedReturnType));

		MethodVisitor method = clazz.visitMethod(ACC_PUBLIC, field.getName(), returnDesc, null, accessorThrows);

		doAccessorAnnotation(method, field);

		method.visitCode();
		method.visitVarInsn(ALOAD, 0);
		loadLong(method, field.getOffset());
		method.visitMethodInsn(INVOKEVIRTUAL, className, "getPointerAtOffset", longFromLong, false);
		method.visitMethodInsn(INVOKESTATIC, qualifiedTargetType, "cast", castDesc, false);
		method.visitInsn(ARETURN);
		method.visitMaxs(3, 1);
		method.visitEnd();

		doEAMethod("Pointer", field);
	}

	/* Sample generated code:
	 *
	 * @com.ibm.j9ddr.GeneratedFieldAccessor(offsetFieldName="_castClassCacheOffset_", declaredType="UDATA")
	 * public UDATA castClassCache() throws CorruptDataException {
	 *     return new UDATA(getLongAtOffset(J9Class._castClassCacheOffset_));
	 * }
	 */
	private void doSimpleTypeMethod(FieldDescriptor field, int type) {
		String fieldType = field.getType();
		String returnType = generalizeSimpleType(fieldType);
		String qualifiedFieldType = qualifyType(fieldType);
		String qualifiedReturnType = qualifyType(returnType);
		PrimitiveAccessor accessor = simpleTypeAccessor(type);
		String returnDesc = Type.getMethodDescriptor(Type.getObjectType(qualifiedReturnType));

		MethodVisitor method = clazz.visitMethod(ACC_PUBLIC, field.getName(), returnDesc, null, accessorThrows);

		doAccessorAnnotation(method, field);

		method.visitCode();
		method.visitTypeInsn(NEW, qualifiedFieldType);
		method.visitInsn(DUP);
		method.visitVarInsn(ALOAD, 0);
		loadLong(method, field.getOffset());
		method.visitMethodInsn(INVOKEVIRTUAL, className, accessor.methodName, accessor.descriptor, false);
		if (!accessor.returnsLong) {
			method.visitInsn(I2L);
		}
		method.visitMethodInsn(INVOKESPECIAL, qualifiedFieldType, "<init>", voidFromLong, false);
		method.visitInsn(ARETURN);
		method.visitMaxs(5, 1);
		method.visitEnd();

		doEAMethod(fieldType, field);
	}

	/* Sample generated code:
	 *
	 * public WideSelfRelativePointer puddleListEA() throws CorruptDataException {
	 *     return WideSelfRelativePointer.cast(nonNullFieldEA(J9Pool._puddleListOffset_));
	 * }
	 */
	private void doSRPEAMethod(FieldDescriptor field, boolean isWide) {
		String accessorName = field.getName() + "EA";
		String returnTypeName = qualifyPointerType(isWide ? "WideSelfRelative" : "SelfRelative");
		Type returnType = Type.getObjectType(returnTypeName);
		String returnDesc = Type.getMethodDescriptor(returnType);
		String castDesc = Type.getMethodDescriptor(returnType, Type.LONG_TYPE);

		MethodVisitor method = clazz.visitMethod(ACC_PUBLIC, accessorName, returnDesc, null, accessorThrows);

		method.visitCode();
		method.visitVarInsn(ALOAD, 0);
		loadLong(method, field.getOffset());
		method.visitMethodInsn(INVOKEVIRTUAL, className, "nonNullFieldEA", longFromLong, false);
		method.visitMethodInsn(INVOKESTATIC, returnTypeName, "cast", castDesc, false);
		method.visitInsn(ARETURN);
		method.visitMaxs(3, 1);
		method.visitEnd();
	}

	/* Sample generated code:
	 *
	 * @com.ibm.j9ddr.GeneratedFieldAccessor(offsetFieldName="_annotationDataOffset_", declaredType="J9SRP(UDATA)")
	 * public UDATAPointer annotationData() throws CorruptDataException {
	 *     int srp = getIntAtOffset(J9AnnotationInfoEntry._annotationDataOffset_);
	 *     if (srp == 0) {
	 *         return UDATAPointer.NULL;
	 *     }
	 *     return UDATAPointer.cast(address + J9AnnotationInfoEntry._annotationDataOffset_ + srp);
	 * }
	 */
	private void doSRPMethod(FieldDescriptor field, boolean isWide) {
		final String prefix = isWide ? "J9WSRP" : "J9SRP";
		final int prefixLength = prefix.length();
		final String rawFieldType = field.getType();
		String targetType;

		if (rawFieldType.startsWith(prefix) && rawFieldType.startsWith("(", prefixLength)) {
			targetType = rawFieldType.substring(prefixLength + 1, rawFieldType.length() - 1).trim();
		} else {
			targetType = "void";
		}

		int type = typeManager.getType(targetType);

		switch (type) {
		case StructureTypeManager.TYPE_J9SRP:
			targetType = "SelfRelative";
			break;
		case StructureTypeManager.TYPE_J9WSRP:
			targetType = "WideSelfRelative";
			break;
		case StructureTypeManager.TYPE_STRUCTURE:
			targetType = removeTypeTags(targetType);
			break;
		case StructureTypeManager.TYPE_VOID:
			targetType = "Void";
			break;
		default:
			if ((StructureTypeManager.TYPE_SIMPLE_MIN <= type) && (type <= StructureTypeManager.TYPE_SIMPLE_MAX)) {
				targetType = removeTypeTags(targetType);
			} else {
				throw new IllegalArgumentException("Unexpected SRP type: " + rawFieldType);
			}
			break;
		}

		String returnTypeName = qualifyPointerType(generalizeSimpleType(targetType));
		Type returnType = Type.getObjectType(returnTypeName);
		String returnDesc = Type.getMethodDescriptor(returnType);
		String qualifiedPointerName = qualifyPointerType(targetType);
		String qualifiedPointerDesc = Type.getObjectType(qualifiedPointerName).getDescriptor();

		MethodVisitor method = clazz.visitMethod(ACC_PUBLIC, field.getName(), returnDesc, null, accessorThrows);

		doAccessorAnnotation(method, field);

		Label nonNull = new Label();

		method.visitCode();
		method.visitVarInsn(ALOAD, 0);
		loadLong(method, field.getOffset());

		if (isWide) {
			method.visitMethodInsn(INVOKEVIRTUAL, className, "getPointerAtOffset", longFromLong, false);
			method.visitVarInsn(LSTORE, 1);

			method.visitVarInsn(LLOAD, 1);
			method.visitInsn(LCONST_0);
			method.visitInsn(LCMP);
		} else {
			method.visitMethodInsn(INVOKEVIRTUAL, className, "getIntAtOffset", intFromLong, false);
			method.visitVarInsn(ISTORE, 1);

			method.visitVarInsn(ILOAD, 1);
		}
		method.visitJumpInsn(IFNE, nonNull);

		method.visitFieldInsn(GETSTATIC, qualifiedPointerName, "NULL", qualifiedPointerDesc);
		method.visitInsn(ARETURN);

		method.visitLabel(nonNull);
		method.visitFrame(F_APPEND, 1, new Object[] { isWide ? LONG : INTEGER }, 0, null);
		method.visitVarInsn(ALOAD, 0);
		method.visitFieldInsn(GETFIELD, className, "address", Type.LONG_TYPE.getDescriptor());
		addLong(method, field.getOffset());
		if (isWide) {
			method.visitVarInsn(LLOAD, 1);
		} else {
			method.visitVarInsn(ILOAD, 1);
			method.visitInsn(I2L);
		}
		method.visitInsn(LADD);

		String castDesc = Type.getMethodDescriptor(returnType, Type.LONG_TYPE);

		method.visitMethodInsn(INVOKESTATIC, qualifiedPointerName, "cast", castDesc, false);
		method.visitInsn(ARETURN);
		method.visitMaxs(4, isWide ? 3 : 2);
		method.visitEnd();

		doSRPEAMethod(field, isWide);
	}

	/* Sample generated code:
	 *
	 * @com.ibm.j9ddr.GeneratedFieldAccessor(offsetFieldName="_sharedHeadNodePtrOffset_", declaredType="J9SRP*")
	 * public SelfRelativePointer sharedHeadNodePtr() throws CorruptDataException {
	 *     return SelfRelativePointer.cast(getPointerAtOffset(J9SharedInvariantInternTable._sharedHeadNodePtrOffset_));
	 * }
	 */
	private void doSRPPointerMethod(FieldDescriptor field, boolean wide) {
		String returnType = qualifyPointerType(wide ? "WideSelfRelative" : "SelfRelative");
		Type objectType = Type.getObjectType(returnType);
		String returnDesc = Type.getMethodDescriptor(objectType);
		String castDesc = Type.getMethodDescriptor(objectType, Type.LONG_TYPE);

		MethodVisitor method = clazz.visitMethod(ACC_PUBLIC, field.getName(), returnDesc, null, accessorThrows);

		doAccessorAnnotation(method, field);

		method.visitCode();
		method.visitVarInsn(ALOAD, 0);
		loadLong(method, field.getOffset());
		method.visitMethodInsn(INVOKEVIRTUAL, className, "getPointerAtOffset", longFromLong, false);
		method.visitMethodInsn(INVOKESTATIC, returnType, "cast", castDesc, false);
		method.visitInsn(ARETURN);
		method.visitMaxs(3, 1);
		method.visitEnd();

		doEAMethod("Pointer", field);
	}

	/* Sample generated code:
	 *
	 * @com.ibm.j9ddr.GeneratedFieldAccessor(offsetFieldName="_permOffset_", declaredType="J9Permission")
	 * public J9PermissionPointer perm() throws CorruptDataException {
	 *     return J9PermissionPointer.cast(nonNullFieldEA(J9FileStat._permOffset_));
	 * }
	 */
	private void doStructureMethod(FieldDescriptor field) {
		String fieldType = removeTypeTags(field.getType());
		String returnType = "void".equals(fieldType) ? "Void" : fieldType;
		String qualifiedReturnType = qualifyPointerType(returnType);
		Type objectType = Type.getObjectType(qualifiedReturnType);
		String returnDesc = Type.getMethodDescriptor(objectType);
		String castDesc = Type.getMethodDescriptor(objectType, Type.LONG_TYPE);

		MethodVisitor method = clazz.visitMethod(ACC_PUBLIC, field.getName(), returnDesc, null, accessorThrows);

		doAccessorAnnotation(method, field);

		method.visitCode();
		method.visitVarInsn(ALOAD, 0);
		loadLong(method, field.getOffset());
		method.visitMethodInsn(INVOKEVIRTUAL, className, "nonNullFieldEA", longFromLong, false);
		method.visitMethodInsn(INVOKESTATIC, qualifiedReturnType, "cast", castDesc, false);
		method.visitInsn(ARETURN);
		method.visitMaxs(3, 1);
		method.visitEnd();

		doEAMethod("Pointer", field);
	}

	/* Sample generated code:
	 *
	 * @com.ibm.j9ddr.GeneratedFieldAccessor(offsetFieldName="_hostClassOffset_", declaredType="J9Class*")
	 * public J9ClassPointer hostClass() throws CorruptDataException {
	 *     return J9ClassPointer.cast(getPointerAtOffset(J9Class._hostClassOffset_));
	 * }
	 */
	private void doStructurePointerMethod(FieldDescriptor field) {
		String returnType = getTargetType(removeTypeTags(field.getType()));
		String qualifiedReturnType = qualifyPointerType(returnType);
		Type objectType = Type.getObjectType(qualifiedReturnType);
		String returnDesc = Type.getMethodDescriptor(objectType);
		String castDesc = Type.getMethodDescriptor(objectType, Type.LONG_TYPE);

		MethodVisitor method = clazz.visitMethod(ACC_PUBLIC, field.getName(), returnDesc, null, accessorThrows);

		doAccessorAnnotation(method, field);

		method.visitCode();
		method.visitVarInsn(ALOAD, 0);
		loadLong(method, field.getOffset());
		method.visitMethodInsn(INVOKEVIRTUAL, className, "getPointerAtOffset", longFromLong, false);
		method.visitMethodInsn(INVOKESTATIC, qualifiedReturnType, "cast", castDesc, false);
		method.visitInsn(ARETURN);
		method.visitMaxs(3, 1);
		method.visitEnd();

		doEAMethod("Pointer", field);
	}

	private byte[] generate() {
		String superClassName = structure.getSuperName();

		if (superClassName.isEmpty()) {
			superClassName = basePrefix + "pointer/StructurePointer";
		} else {
			superClassName = basePrefix + "pointer/generated/" + superClassName + "Pointer";
		}

		clazz.visit(V1_8, ACC_PUBLIC | ACC_SUPER, className, null, superClassName, null);

		doClassAnnotation();
		doNullInstance();
		doConstructors(superClassName);
		doAccessorMethods();

		clazz.visitEnd();

		return clazz.toByteArray();
	}

	private String getStructureClassName() {
		return basePrefix + "structure/" + structure.getName();
	}

	private String qualifyPointerType(String type) {
		String subPackage = predefinedPointerTypes.contains(type) ? "pointer/" : "pointer/generated/";

		return basePrefix + subPackage + type + "Pointer";
	}

	private String qualifyType(String type) {
		String subPackage = predefinedDataTypes.contains(type) ? "types/" : "structure/";

		return basePrefix + subPackage + type;
	}

	private PrimitiveAccessor simpleTypeAccessor(int type) {
		int size;

		switch (type) {
		case StructureTypeManager.TYPE_I8:
		case StructureTypeManager.TYPE_U8:
			size = 1;
			break;
		case StructureTypeManager.TYPE_I16:
		case StructureTypeManager.TYPE_U16:
			size = 2;
			break;
		case StructureTypeManager.TYPE_I32:
		case StructureTypeManager.TYPE_U32:
			size = 4;
			break;
		case StructureTypeManager.TYPE_I64:
		case StructureTypeManager.TYPE_U64:
			size = 8;
			break;
		case StructureTypeManager.TYPE_IDATA:
		case StructureTypeManager.TYPE_UDATA:
			size = reader.getSizeOfUDATA();
			break;
		default:
			throw new IllegalArgumentException("type=" + type);
		}

		return PrimitiveAccessor.forSize(size);
	}

}

enum PrimitiveAccessor {

	BYTE("getByteAtOffset", HelperBase.byteFromLong, false),

	INT("getIntAtOffset", HelperBase.intFromLong, false),

	LONG("getLongAtOffset", HelperBase.longFromLong, true),

	SHORT("getShortAtOffset", HelperBase.shortFromLong, false);

	static PrimitiveAccessor forSize(int size) {
		switch (size) {
		case 1:
			return BYTE;
		case 2:
			return SHORT;
		case 4:
			return INT;
		case 8:
			return LONG;
		default:
			throw new IllegalArgumentException("size=" + size);
		}
	}

	final String descriptor;

	final String methodName;

	final boolean returnsLong;

	PrimitiveAccessor(String methodName, String descriptor, boolean returnsLong) {
		this.methodName = methodName;
		this.descriptor = descriptor;
		this.returnsLong = returnsLong;
	}

}

final class StructureHelper extends HelperBase {

	static byte[] getClassBytes(StructureDescriptor structure, String className) {
		StructureHelper helper = new StructureHelper(structure, className);

		return helper.generate();
	}

	private final String className;

	private final ClassWriter clazz;

	private final MethodVisitor clinit;

	private final StructureDescriptor structure;

	private StructureHelper(StructureDescriptor structure, String className) {
		super();
		this.className = className;
		this.clazz = new ClassWriter(0);
		this.clazz.visit(V1_8, ACC_PUBLIC | ACC_FINAL | ACC_SUPER, className, null, "java/lang/Object", null);
		this.clinit = clazz.visitMethod(ACC_STATIC, "<clinit>", voidMethod, null, null);
		this.clinit.visitCode();
		this.structure = structure;
	}

	private void defineField(String name, Type type, long value) {
		String typeDescriptor = type.getDescriptor();

		clazz.visitField(ACC_PUBLIC | ACC_FINAL | ACC_STATIC, name, typeDescriptor, null, null).visitEnd();

		if (type.getSort() == Type.INT) {
			loadInt(clinit, (int) value);
		} else {
			loadLong(clinit, value);
		}

		clinit.visitFieldInsn(PUTSTATIC, className, name, typeDescriptor);
	}

	private void defineFields() {
		defineField("SIZEOF", Type.LONG_TYPE, structure.getSizeOf());

		// other constants
		for (ConstantDescriptor constant : structure.getConstants()) {
			defineField(constant.getName(), Type.LONG_TYPE, constant.getValue());
		}

		// offsets
		int bitFieldBitCount = 0;
		for (FieldDescriptor field : structure.getFields()) {
			String fieldName = field.getName();
			int fieldOffset = field.getOffset();
			String type = field.getType();
			int colonIndex = type.lastIndexOf(':');

			// make sure match a bitfield, not a C++ namespace
			if (colonIndex <= 0 || type.charAt(colonIndex - 1) == ':') {
				// regular offset field
				defineField(String.format("_%sOffset_", fieldName), Type.INT_TYPE, fieldOffset);
			} else {
				// bitfield
				int bitSize = Integer.parseInt(type.substring(colonIndex + 1).trim());

				/*
				 * Newer blobs have accurate offsets of bitfields; adjust bitFieldBitCount
				 * to account for any fields preceding this field. In older blobs the byte
				 * offset of a bitfield is always zero, so this has no effect.
				 */
				bitFieldBitCount = Math.max(bitFieldBitCount, fieldOffset * Byte.SIZE);

				if (bitSize > (StructureReader.BIT_FIELD_CELL_SIZE
						- (bitFieldBitCount % StructureReader.BIT_FIELD_CELL_SIZE))) {
					throw new InternalError(
							String.format("Bitfield %s->%s must not span cells", structure.getName(), fieldName));
				}

				// 's' field
				defineField(String.format("_%s_s_", fieldName), Type.INT_TYPE, bitFieldBitCount);

				// 'b' field
				defineField(String.format("_%s_b_", fieldName), Type.INT_TYPE, bitSize);

				bitFieldBitCount += bitSize;
			}
		}

		clinit.visitInsn(RETURN);
		clinit.visitMaxs(2, 0);
		clinit.visitEnd();
	}

	private byte[] generate() {
		defineFields();

		MethodVisitor method = clazz.visitMethod(ACC_PUBLIC, "<init>", voidMethod, null, null);

		method.visitCode();
		method.visitVarInsn(ALOAD, 0);
		method.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", voidMethod, false);
		method.visitInsn(RETURN);
		method.visitMaxs(1, 1);
		method.visitEnd();

		clazz.visitEnd();

		return clazz.toByteArray();
	}

}
