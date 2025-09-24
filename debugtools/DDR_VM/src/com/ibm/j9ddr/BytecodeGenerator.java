/*
 * Copyright IBM Corp. and others 1991
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
package com.ibm.j9ddr;

import java.util.Arrays;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.TreeMap;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import com.ibm.j9ddr.StructureReader.ConstantDescriptor;
import com.ibm.j9ddr.StructureReader.FieldDescriptor;
import com.ibm.j9ddr.StructureReader.StructureDescriptor;
import com.ibm.j9ddr.tools.FlagStructureList;

/*[IF JAVA_SPEC_VERSION < 24]*/
import jdk.internal.org.objectweb.asm.AnnotationVisitor;
import jdk.internal.org.objectweb.asm.ClassWriter;
import jdk.internal.org.objectweb.asm.Label;
import jdk.internal.org.objectweb.asm.MethodVisitor;
import jdk.internal.org.objectweb.asm.Opcodes;
import jdk.internal.org.objectweb.asm.Type;
/*[ELSE] JAVA_SPEC_VERSION < 24 */
import java.lang.classfile.Annotation;
import java.lang.classfile.AnnotationElement;
import java.lang.classfile.ClassBuilder;
import java.lang.classfile.ClassFile;
import java.lang.classfile.CodeBuilder;
import java.lang.classfile.Label;
import java.lang.classfile.MethodBuilder;
import java.lang.classfile.Opcode;
import java.lang.classfile.attribute.ExceptionsAttribute;
import java.lang.classfile.attribute.RuntimeVisibleAnnotationsAttribute;
import java.lang.constant.ClassDesc;
import java.lang.constant.ConstantDescs;
import java.lang.constant.MethodTypeDesc;
import java.util.function.Consumer;
/*[ENDIF] JAVA_SPEC_VERSION < 24 */

/**
 * Generates the class bytecodes needed by DDR to represent, as Java classes,
 * the structures and pointers described by the blob.
 */
public class BytecodeGenerator {

	/**
	 * Should getFlagCName() be used to adjust fields names for className?
	 */
	public static boolean shouldUseCNameFor(String className) {
		return className.equals("J9BuildFlags");
	}

	private static final String[] knownNames = { "AOT", "Arg0EA", "INL", "JIT", "JNI" };

	/**
	 * This produces the same "cname" as getDataModelExtension()
	 * in com.ibm.j9.uma.platform.PlatformImplementation for flags
	 * whose names don't already match what appears in native code.
	 */
	public static String getFlagCName(String id) {
		switch (id) {
		case "EsVersionMajor":
		case "EsVersionMajor_DEFINED":
		case "EsVersionMinor":
		case "EsVersionMinor_DEFINED":
		case "JAVA_SPEC_VERSION":
		case "JAVA_SPEC_VERSION_DEFINED":
			/*
			 * These are explicitly listed in j9cfg.h (generated from j9cfg.h.in
			 * or j9cfg.h.ftl), or they are derived from definitions there: They
			 * don't need mapping. Also, they are only present in system dumps
			 * from Semeru VMs and not in those produced by IBM VMs.
			 */
			return id;
		default:
			if (id.startsWith("J9VM_")) {
				// This already looks like a cname derived from a definition in j9cfg.h.
				return id;
			}
			break;
		}

		StringBuilder cname = new StringBuilder("J9VM_");
		boolean addUnderscore = false;
		int idLength = id.length();

outer:
		for (int i = 0; i < idLength;) {
			if (addUnderscore) {
				cname.append('_');
				addUnderscore = false;
			}

			for (String knownName : knownNames) {
				int knownLength = knownName.length();

				if (id.regionMatches(i, knownName, 0, knownLength)) {
					cname.append(knownName.toUpperCase());
					addUnderscore = true;
					i += knownLength;
					continue outer;
				}
			}

			char ch = id.charAt(i);

			cname.append(Character.toUpperCase(ch));
			i += 1;

			if (!Character.isUpperCase(ch)) {
				if (i < idLength && Character.isUpperCase(id.charAt(i))) {
					addUnderscore = true;
				}
			}
		}

		return cname.toString();
	}

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
		boolean useCName = BytecodeGenerator.shouldUseCNameFor(structure.getName());
		Map<String, Boolean> values = new TreeMap<>();

		for (ConstantDescriptor constant : structure.getConstants()) {
			String name = constant.getName();

			if (useCName) {
				name = BytecodeGenerator.getFlagCName(name);
			}

			values.put(name, Boolean.valueOf(constant.getValue() != 0));
		}

		/*[IF JAVA_SPEC_VERSION < 24]*/
		ClassWriter clazz = new ClassWriter(0);

		clazz.visit(V1_8, ACC_PUBLIC | ACC_FINAL | ACC_SUPER, className, null, "java/lang/Object", null);

		MethodVisitor clinit = clazz.visitMethod(ACC_STATIC, "<clinit>", voidMethod, null, null);

		clinit.visitCode();

		for (Map.Entry<String, Boolean> entry : values.entrySet()) {
			String name = entry.getKey();
			Boolean value = entry.getValue();

			clazz.visitField(ACC_PUBLIC | ACC_STATIC | ACC_FINAL, name, "Z", null, null).visitEnd();

			clinit.visitInsn(value.booleanValue() ? ICONST_1 : ICONST_0);
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
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		ClassDesc classDesc = ClassDesc.ofInternalName(className);

		Consumer<CodeBuilder> clinit = body -> {
			for (Map.Entry<String, Boolean> entry : values.entrySet()) {
				body.loadConstant(entry.getValue().booleanValue() ? 1 : 0);
				body.putstatic(classDesc, entry.getKey(), ConstantDescs.CD_boolean);
			}

			body.return_();
		};

		Consumer<CodeBuilder> init = body -> {
			body.aload(0);
			body.invokespecial(ConstantDescs.CD_Object, ConstantDescs.INIT_NAME, ConstantDescs.MTD_void);
			body.return_();
		};

		Consumer<ClassBuilder> builder = body -> {
			body.withFlags(ClassFile.ACC_PUBLIC | ClassFile.ACC_FINAL | ClassFile.ACC_SUPER);

			for (String name : values.keySet()) {
				body.withField(name, ConstantDescs.CD_boolean,
						ClassFile.ACC_PUBLIC | ClassFile.ACC_STATIC | ClassFile.ACC_FINAL);
			}

			body.withMethodBody(ConstantDescs.CLASS_INIT_NAME, ConstantDescs.MTD_void, ClassFile.ACC_STATIC, clinit);

			body.withMethodBody(ConstantDescs.INIT_NAME, ConstantDescs.MTD_void, ClassFile.ACC_PRIVATE, init);
		};

		return ClassFile.of().build(classDesc, builder);
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */
	}

}

abstract class HelperBase
		/*[IF JAVA_SPEC_VERSION < 24]*/
		implements Opcodes
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */
{

	static final String byteFromLong = "(J)B";

	static final String doubleFromLong = "(J)D";

	static final String doubleFromVoid = "()D";

	static final String floatFromLong = "(J)F";

	static final String floatFromVoid = "()F";

	static final String intFromLong = "(J)I";

	static final String longFromLong = "(J)J";

	static final String longFromVoid = "()J";

	static final String shortFromLong = "(J)S";

	static final String voidFromLong = "(J)V";

	static final String voidMethod = "()V";

	/*[IF JAVA_SPEC_VERSION < 24]*/

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
			if ((Byte.MIN_VALUE <= value) && (value <= Byte.MAX_VALUE)) {
				method.visitIntInsn(BIPUSH, value);
			} else if ((Short.MIN_VALUE <= value) && (value <= Short.MAX_VALUE)) {
				method.visitIntInsn(SIPUSH, value);
			} else {
				method.visitLdcInsn(Integer.valueOf(value));
			}
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

	/*[ELSE] JAVA_SPEC_VERSION < 24 */

	static final void addLong(CodeBuilder code, long value) {
		if (value != 0) {
			code.loadConstant(value).ladd();
		}
	}

	/*[ENDIF] JAVA_SPEC_VERSION < 24 */

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

	/*[IF JAVA_SPEC_VERSION < 24]*/
	private static boolean checkPresent(FieldDescriptor field, MethodVisitor method) {
		if (field.isPresent()) {
			return true;
		}

		String noSuchField = "java/lang/NoSuchFieldException";

		method.visitTypeInsn(NEW, noSuchField);
		method.visitInsn(DUP);
		method.visitMethodInsn(INVOKESPECIAL, noSuchField, "<init>", voidMethod, false);

		method.visitInsn(ATHROW);

		return false;
	}
	/*[ELSE] JAVA_SPEC_VERSION < 24 */
	private static boolean checkPresent(FieldDescriptor field, CodeBuilder body) {
		if (field.isPresent()) {
			return true;
		}

		body.new_(noSuchFieldDesc);
		body.dup();
		body.invokespecial(noSuchFieldDesc, ConstantDescs.INIT_NAME, ConstantDescs.MTD_void);
		body.athrow();

		return false;
	}
	/*[ENDIF] JAVA_SPEC_VERSION < 24 */

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

	private final String basePrefix;

	private final String className;

	private final StructureReader reader;

	private final StructureDescriptor structure;

	private final StructureTypeManager typeManager;

	/*[IF JAVA_SPEC_VERSION < 24]*/
	private final Type abstractPointerType;
	private final Type classType;
	private final ClassWriter clazz;
	private final String[] normalThrows;
	private final String[] optionalThrows;
	private final Type scalarType;
	private final Type udataType;
	/*[ELSE] JAVA_SPEC_VERSION < 24 */
	private static final ClassDesc corruptDataDesc = ClassDesc.of("com.ibm.j9ddr.CorruptDataException");
	private static final ClassDesc fieldAccessorDesc = ClassDesc.of("com.ibm.j9ddr.GeneratedFieldAccessor");
	private static final ClassDesc generatedPointerDesc = ClassDesc.of("com.ibm.j9ddr.GeneratedPointerClass");
	private static final ClassDesc noSuchFieldDesc = ClassDesc.of("java.lang.NoSuchFieldException");

	private static final List<ClassDesc> normalThrows = List.of(corruptDataDesc);
	private static final List<ClassDesc> optionalThrows = List.of(corruptDataDesc, noSuchFieldDesc);

	private static final MethodTypeDesc long2int = MethodTypeDesc.of(ConstantDescs.CD_int, ConstantDescs.CD_long);
	private static final MethodTypeDesc long2long = MethodTypeDesc.of(ConstantDescs.CD_long, ConstantDescs.CD_long);
	private static final MethodTypeDesc long2void = MethodTypeDesc.of(ConstantDescs.CD_void, ConstantDescs.CD_long);
	private static final MethodTypeDesc void2long = MethodTypeDesc.of(ConstantDescs.CD_long);

	private final ClassDesc abstractPointerDesc;
	private ClassBuilder classBuilder;
	private final ClassDesc classDesc;
	private final MethodTypeDesc long2abstact;
	private final MethodTypeDesc scalar2abstract;
	private final ClassDesc scalarDesc;
	private ClassDesc superDesc;
	private final ClassDesc udataDesc;
	/*[ENDIF] JAVA_SPEC_VERSION < 24 */

	private PointerHelper(StructureReader reader, StructureTypeManager typeManager, StructureDescriptor structure,
			String className) {
		super();

		int index = className.indexOf("/pointer/generated/");

		if (index <= 0) {
			throw new IllegalArgumentException(className);
		}

		String prefix = className.substring(0, index + 1); // ends with '/vmNN/'

		this.basePrefix = prefix;
		this.className = className;
		this.reader = reader;
		this.structure = structure;
		this.typeManager = typeManager;

		/*[IF JAVA_SPEC_VERSION < 24]*/
		this.abstractPointerType = Type.getObjectType(prefix + "pointer/AbstractPointer");
		this.classType = Type.getObjectType(className);
		this.clazz = new ClassWriter(0);
		this.normalThrows = new String[] { "com/ibm/j9ddr/CorruptDataException" };
		this.optionalThrows = new String[] { "com/ibm/j9ddr/CorruptDataException", "java/lang/NoSuchFieldException" };
		this.scalarType = Type.getObjectType(prefix + "types/Scalar");
		this.udataType = Type.getObjectType(prefix + "types/UDATA");
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		this.abstractPointerDesc = ClassDesc.ofInternalName(prefix + "pointer/AbstractPointer");
		this.classDesc = ClassDesc.ofInternalName(className);
		this.long2abstact = MethodTypeDesc.of(abstractPointerDesc, ConstantDescs.CD_long);
		this.scalarDesc = ClassDesc.ofInternalName(prefix + "types/Scalar");
		this.scalar2abstract = MethodTypeDesc.of(abstractPointerDesc, scalarDesc);
		this.udataDesc = ClassDesc.ofInternalName(prefix + "types/UDATA");
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */
	}

	/*[IF JAVA_SPEC_VERSION < 24]*/

	private MethodVisitor beginAnnotatedMethod(FieldDescriptor field, String name, String descriptor) {
		MethodVisitor method = beginMethod(field, name, descriptor);

		if (field.isPresent()) {
			Type annotationType = Type.getObjectType("com/ibm/j9ddr/GeneratedFieldAccessor");
			AnnotationVisitor annotation = method.visitAnnotation(annotationType.getDescriptor(), true);

			annotation.visit("offsetFieldName", String.format("_%sOffset_", field.getName()));
			annotation.visit("declaredType", field.getDeclaredType());
			annotation.visitEnd();
		}

		return method;
	}

	private MethodVisitor beginMethod(FieldDescriptor field, String name, String descriptor) {
		String[] exceptions = field.isOptional() ? optionalThrows : normalThrows;

		return clazz.visitMethod(ACC_PUBLIC, name, descriptor, null, exceptions);
	}

	/*[ELSE] JAVA_SPEC_VERSION < 24 */

	private void addAnnotatedMethod(FieldDescriptor field, String name, MethodTypeDesc type,
			Consumer<CodeBuilder> code) {
		Consumer<MethodBuilder> method = builder -> {
			List<ClassDesc> exceptions = field.isOptional() ? optionalThrows : normalThrows;

			builder.with(ExceptionsAttribute.ofSymbols(exceptions));

			if (field.isPresent()) {
				Annotation annotation = Annotation.of( //
						fieldAccessorDesc, //
						AnnotationElement.ofString("offsetFieldName", String.format("_%sOffset_", field.getName())), //
						AnnotationElement.ofString("declaredType", field.getDeclaredType()));

				builder.with(RuntimeVisibleAnnotationsAttribute.of(annotation));
			}

			builder.withCode(code);
		};

		classBuilder.withMethod(name, type, ClassFile.ACC_PUBLIC, method);
	}

	private void addMethod(FieldDescriptor field, String name, MethodTypeDesc type, Consumer<CodeBuilder> code) {
		Consumer<MethodBuilder> method = builder -> {
			List<ClassDesc> exceptions = field.isOptional() ? optionalThrows : normalThrows;

			builder.with(ExceptionsAttribute.ofSymbols(exceptions));
			builder.withCode(code);
		};

		classBuilder.withMethod(name, type, ClassFile.ACC_PUBLIC, method);
	}

	/*[ENDIF] JAVA_SPEC_VERSION < 24 */

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
	 * public UDATA isDir() throws CorruptDataException {
	 *     return getU32Bitfield(J9FileStat._isDir_s_, J9FileStat._isDir_b_);
	 * }
	 */
	private void doBitfieldMethod(FieldDescriptor field, String baseType, int width) {
		String fieldName = field.getName();
		String startFieldName = String.format("_%s_s_", fieldName);
		String accessorName = String.format("get%sBitfield", baseType);

		/*[IF JAVA_SPEC_VERSION < 24]*/
		Type qualifiedBaseType = Type.getObjectType(qualifyType(baseType));
		Type qualifiedReturnType = Type.getObjectType(qualifyType(generalizeSimpleType(baseType)));
		String returnDesc = Type.getMethodDescriptor(qualifiedReturnType);
		String accessorDesc = Type.getMethodDescriptor(qualifiedBaseType, Type.INT_TYPE, Type.INT_TYPE);
		MethodVisitor method = beginAnnotatedMethod(field, fieldName, returnDesc);

		method.visitCode();
		if (checkPresent(field, method)) {
			method.visitVarInsn(ALOAD, 0);
			method.visitFieldInsn(GETSTATIC, getStructureClassName(), startFieldName, Type.INT_TYPE.getDescriptor());
			loadInt(method, width);
			method.visitMethodInsn(INVOKEVIRTUAL, className, accessorName, accessorDesc, false);
			method.visitInsn(ARETURN);
		}
		method.visitMaxs(3, 1);
		method.visitEnd();
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		ClassDesc qualifiedBaseDesc = ClassDesc.ofInternalName(qualifyType(baseType));
		ClassDesc qualifiedReturnDesc = ClassDesc.ofInternalName(qualifyType(generalizeSimpleType(baseType)));
		MethodTypeDesc accessorDesc = MethodTypeDesc.of(qualifiedBaseDesc, ConstantDescs.CD_int, ConstantDescs.CD_int);
		ClassDesc structureDesc = ClassDesc.ofInternalName(getStructureClassName());

		MethodTypeDesc methodDesc = MethodTypeDesc.of(qualifiedReturnDesc);

		Consumer<CodeBuilder> accessor = body -> {
			if (checkPresent(field, body)) {
				body.aload(0);
				body.getstatic(structureDesc, startFieldName, ConstantDescs.CD_int);
				body.loadConstant(width);
				body.invokevirtual(classDesc, accessorName, accessorDesc);
				body.areturn();
			}
		};

		addAnnotatedMethod(field, field.getName(), methodDesc, accessor);
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */

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
		/*[IF JAVA_SPEC_VERSION < 24]*/
		String returnDesc = Type.getMethodDescriptor(Type.BOOLEAN_TYPE);
		String accessorDesc = Type.getMethodDescriptor(Type.BOOLEAN_TYPE, Type.LONG_TYPE);
		MethodVisitor method = beginAnnotatedMethod(field, field.getName(), returnDesc);

		method.visitCode();
		if (checkPresent(field, method)) {
			method.visitVarInsn(ALOAD, 0);
			loadLong(method, field.getOffset());
			method.visitMethodInsn(INVOKEVIRTUAL, className, "getBoolAtOffset", accessorDesc, false);
			method.visitInsn(IRETURN);
		}
		method.visitMaxs(3, 1);
		method.visitEnd();
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		MethodTypeDesc methodDesc = MethodTypeDesc.of(ConstantDescs.CD_boolean);
		MethodTypeDesc long2boolean = MethodTypeDesc.of(ConstantDescs.CD_boolean, ConstantDescs.CD_long);

		Consumer<CodeBuilder> accessor = body -> {
			if (checkPresent(field, body)) {
				body.aload(0);
				body.loadConstant((long) field.getOffset());
				body.invokevirtual(classDesc, "getBoolAtOffset", long2boolean);
				body.ireturn();
			}
		};

		addAnnotatedMethod(field, field.getName(), methodDesc, accessor);
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */

		doEAMethod("Bool", field);
	}

	private void doClassAnnotation() {
		// @com.ibm.j9ddr.GeneratedPointerClass(structureClass=BASE.class)
		/*[IF JAVA_SPEC_VERSION < 24]*/
		Type annotationType = Type.getObjectType("com/ibm/j9ddr/GeneratedPointerClass");
		AnnotationVisitor annotation = clazz.visitAnnotation(annotationType.getDescriptor(), true);
		Type structureType = Type.getObjectType(getStructureClassName());

		annotation.visit("structureClass", structureType);
		annotation.visitEnd();
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		ClassDesc structureDesc = ClassDesc.ofInternalName(getStructureClassName());
		AnnotationElement element = AnnotationElement.ofClass("structureClass", structureDesc);

		classBuilder.with(RuntimeVisibleAnnotationsAttribute.of(Annotation.of(generatedPointerDesc, element)));
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */
	}

	private void doConstructors(String superClassName) {
		/*[IF JAVA_SPEC_VERSION < 24]*/
		final String abstractPointerFromLong = Type.getMethodDescriptor(abstractPointerType, Type.LONG_TYPE);
		final String abstractPointerFromScalar = Type.getMethodDescriptor(abstractPointerType, scalarType);
		final String classFromLong = Type.getMethodDescriptor(classType, Type.LONG_TYPE);
		final String classFromScalar = Type.getMethodDescriptor(classType, scalarType);
		MethodVisitor method;
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		MethodTypeDesc long2self = MethodTypeDesc.of(classDesc, ConstantDescs.CD_long);
		MethodTypeDesc scalar2self = MethodTypeDesc.of(classDesc, scalarDesc);
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */

		// protected SELF(long address) { super(address); }
		/*[IF JAVA_SPEC_VERSION < 24]*/
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
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		{
			Consumer<CodeBuilder> init = body -> {
				body.aload(0);
				body.lload(1);
				body.invokespecial(superDesc, ConstantDescs.INIT_NAME, long2void);
				body.return_();
			};

			classBuilder.withMethodBody(ConstantDescs.INIT_NAME, long2void, ClassFile.ACC_PROTECTED, init);
		}
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */

		// public static SELF cast(long address) { if (address == 0) return NULL; return new SELF(address); }
		/*[IF JAVA_SPEC_VERSION < 24]*/
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
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		{
			Consumer<CodeBuilder> cast = body -> {
				Label nonNull = body.newLabel();

				body.lload(0);
				body.lconst_0();
				body.lcmp();
				body.ifne(nonNull);
				body.getstatic(classDesc, "NULL", classDesc);
				body.areturn();

				body.labelBinding(nonNull);
				body.new_(classDesc);
				body.dup();
				body.lload(0);
				body.invokespecial(classDesc, ConstantDescs.INIT_NAME, long2void);
				body.areturn();
			};

			classBuilder.withMethodBody("cast", long2self, ClassFile.ACC_PUBLIC | ClassFile.ACC_STATIC, cast);
		}
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */

		// public static SELF cast(AbstractPointer structure) { return cast(structure.getAddress()); }
		/*[IF JAVA_SPEC_VERSION < 24]*/
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
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		{
			MethodTypeDesc abstract2self = MethodTypeDesc.of(classDesc, abstractPointerDesc);

			Consumer<CodeBuilder> cast = body -> {
				body.aload(0);
				body.invokevirtual(abstractPointerDesc, "getAddress", void2long);
				body.invokestatic(classDesc, "cast", long2self);
				body.areturn();
			};

			classBuilder.withMethodBody("cast", abstract2self, ClassFile.ACC_PUBLIC | ClassFile.ACC_STATIC, cast);
		}
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */

		// public static SELF cast(UDATA udata) { return cast(udata.longValue()); }
		/*[IF JAVA_SPEC_VERSION < 24]*/
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
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		{
			MethodTypeDesc udata2self = MethodTypeDesc.of(classDesc, udataDesc);

			Consumer<CodeBuilder> cast = body -> {
				body.aload(0);
				body.invokevirtual(udataDesc, "longValue", void2long);
				body.invokestatic(classDesc, "cast", long2self);
				body.areturn();
			};

			classBuilder.withMethodBody("cast", udata2self, ClassFile.ACC_PUBLIC | ClassFile.ACC_STATIC, cast);
		}
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */

		// public SELF add(long count) { return addOffset(count * BASE.SIZEOF); }
		// bridge: AbstractPointer add(long count)
		/*[IF JAVA_SPEC_VERSION < 24]*/
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

			method = clazz.visitMethod(ACC_PUBLIC | ACC_BRIDGE | ACC_SYNTHETIC, "add", abstractPointerFromLong, null, null);

			method.visitCode();
			method.visitVarInsn(ALOAD, 0);
			method.visitVarInsn(LLOAD, 1);
			method.visitMethodInsn(INVOKEVIRTUAL, className, "add", classFromLong, false);
			method.visitInsn(ARETURN);
			method.visitMaxs(3, 3);
			method.visitEnd();
		}
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		{
			Consumer<CodeBuilder> add = body -> {
				body.aload(0);
				body.lload(1);
				body.loadConstant((long) structure.getSizeOf());
				body.lmul();
				body.invokevirtual(classDesc, "addOffset", long2self);
				body.areturn();
			};

			classBuilder.withMethodBody("add", long2self, ClassFile.ACC_PUBLIC, add);

			Consumer<CodeBuilder> bridge = body -> {
				body.aload(0);
				body.lload(1);
				body.invokevirtual(classDesc, "add", long2self);
				body.areturn();
			};

			classBuilder.withMethodBody("add", long2abstact,
					ClassFile.ACC_PUBLIC | ClassFile.ACC_BRIDGE | ClassFile.ACC_SYNTHETIC, bridge);
		}
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */

		// public SELF add(Scalar count) { return add(count.longValue()); }
		// bridge: AbstractPointer add(Scalar count)
		/*[IF JAVA_SPEC_VERSION < 24]*/
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

			method = clazz.visitMethod(ACC_PUBLIC | ACC_BRIDGE | ACC_SYNTHETIC, "add", abstractPointerFromScalar, null, null);

			method.visitCode();
			method.visitVarInsn(ALOAD, 0);
			method.visitVarInsn(ALOAD, 1);
			method.visitMethodInsn(INVOKEVIRTUAL, className, "add", classFromScalar, false);
			method.visitInsn(ARETURN);
			method.visitMaxs(2, 2);
			method.visitEnd();
		}
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		{
			Consumer<CodeBuilder> add = body -> {
				body.aload(0);
				body.aload(1);
				body.invokevirtual(scalarDesc, "longValue", void2long);
				body.invokevirtual(classDesc, "add", long2self);
				body.areturn();
			};

			classBuilder.withMethodBody("add", scalar2self, ClassFile.ACC_PUBLIC, add);

			Consumer<CodeBuilder> bridge = body -> {
				body.aload(0);
				body.aload(1);
				body.invokevirtual(classDesc, "add", scalar2self);
				body.areturn();
			};

			classBuilder.withMethodBody("add", scalar2abstract,
					ClassFile.ACC_PUBLIC | ClassFile.ACC_BRIDGE | ClassFile.ACC_SYNTHETIC, bridge);
		}
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */

		// public SELF addOffset(long offset) { return cast(address + offset); }
		// bridge: AbstractPointer addOffset(long offset)
		/*[IF JAVA_SPEC_VERSION < 24]*/
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

			method = clazz.visitMethod(ACC_PUBLIC | ACC_BRIDGE | ACC_SYNTHETIC, "addOffset", abstractPointerFromLong, null, null);

			method.visitCode();
			method.visitVarInsn(ALOAD, 0);
			method.visitVarInsn(LLOAD, 1);
			method.visitMethodInsn(INVOKEVIRTUAL, className, "addOffset", classFromLong, false);
			method.visitInsn(ARETURN);
			method.visitMaxs(3, 3);
			method.visitEnd();
		}
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		{
			Consumer<CodeBuilder> addOffset = body -> {
				body.aload(0);
				body.getfield(classDesc, "address", ConstantDescs.CD_long);
				body.lload(1);
				body.ladd();
				body.invokestatic(classDesc, "cast", long2self);
				body.areturn();
			};

			classBuilder.withMethodBody("addOffset", long2self, ClassFile.ACC_PUBLIC, addOffset);

			Consumer<CodeBuilder> bridge = body -> {
				body.aload(0);
				body.lload(1);
				body.invokevirtual(classDesc, "addOffset", long2self);
				body.areturn();
			};

			classBuilder.withMethodBody(
					"addOffset", long2abstact,
					ClassFile.ACC_PUBLIC | ClassFile.ACC_BRIDGE | ClassFile.ACC_SYNTHETIC, bridge);
		}
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */

		// public SELF addOffset(Scalar offset) { return addOffset(offset.longValue()); }
		// bridge: AbstractPointer addOffset(Scalar offset)
		/*[IF JAVA_SPEC_VERSION < 24]*/
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

			method = clazz.visitMethod(ACC_PUBLIC | ACC_BRIDGE | ACC_SYNTHETIC, "addOffset", abstractPointerFromScalar, null, null);

			method.visitCode();
			method.visitVarInsn(ALOAD, 0);
			method.visitVarInsn(ALOAD, 1);
			method.visitMethodInsn(INVOKEVIRTUAL, className, "addOffset", classFromScalar, false);
			method.visitInsn(ARETURN);
			method.visitMaxs(2, 2);
			method.visitEnd();
		}
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		{
			Consumer<CodeBuilder> addOffset = body -> {
				body.aload(0);
				body.aload(1);
				body.invokevirtual(scalarDesc, "longValue", void2long);
				body.invokevirtual(classDesc, "addOffset", long2self);
				body.areturn();
			};

			classBuilder.withMethodBody("addOffset", scalar2self, ClassFile.ACC_PUBLIC, addOffset);

			Consumer<CodeBuilder> bridge = body -> {
				body.aload(0);
				body.aload(1);
				body.invokevirtual(classDesc, "addOffset", scalar2self);
				body.areturn();
			};

			classBuilder.withMethodBody("addOffset", scalar2abstract,
					ClassFile.ACC_PUBLIC | ClassFile.ACC_BRIDGE | ClassFile.ACC_SYNTHETIC, bridge);
		}
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */

		// public SELF sub(long count) { return subOffset(count * SIZEOF); }
		// bridge: AbstractPointer sub(long count)
		/*[IF JAVA_SPEC_VERSION < 24]*/
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

			method = clazz.visitMethod(ACC_PUBLIC | ACC_BRIDGE | ACC_SYNTHETIC, "sub", abstractPointerFromLong, null, null);

			method.visitCode();
			method.visitVarInsn(ALOAD, 0);
			method.visitVarInsn(LLOAD, 1);
			method.visitMethodInsn(INVOKEVIRTUAL, className, "sub", classFromLong, false);
			method.visitInsn(ARETURN);
			method.visitMaxs(3, 3);
			method.visitEnd();
		}
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		{
			Consumer<CodeBuilder> sub = body -> {
				body.aload(0);
				body.lload(1);
				body.loadConstant((long) structure.getSizeOf());
				body.lmul();
				body.invokevirtual(classDesc, "subOffset", long2self);
				body.areturn();
			};

			classBuilder.withMethodBody("sub", long2self, ClassFile.ACC_PUBLIC, sub);

			Consumer<CodeBuilder> bridge = body -> {
				body.aload(0);
				body.lload(1);
				body.invokevirtual(classDesc, "sub", long2self);
				body.areturn();
			};

			classBuilder.withMethodBody("sub", long2abstact,
					ClassFile.ACC_PUBLIC | ClassFile.ACC_BRIDGE | ClassFile.ACC_SYNTHETIC, bridge);
		}
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */

		// public SELF sub(Scalar count) { return sub(count.longValue()); }
		// bridge: AbstractPointer sub(Scalar count)
		/*[IF JAVA_SPEC_VERSION < 24]*/
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

			method = clazz.visitMethod(ACC_PUBLIC | ACC_BRIDGE | ACC_SYNTHETIC, "sub", abstractPointerFromScalar, null, null);

			method.visitCode();
			method.visitVarInsn(ALOAD, 0);
			method.visitVarInsn(ALOAD, 1);
			method.visitMethodInsn(INVOKEVIRTUAL, className, "sub", classFromScalar, false);
			method.visitInsn(ARETURN);
			method.visitMaxs(2, 2);
			method.visitEnd();
		}
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		{
			Consumer<CodeBuilder> sub = body -> {
				body.aload(0);
				body.aload(1);
				body.invokevirtual(scalarDesc, "longValue", void2long);
				body.invokevirtual(classDesc, "sub", long2self);
				body.areturn();
			};

			classBuilder.withMethodBody("sub", scalar2self, ClassFile.ACC_PUBLIC, sub);

			Consumer<CodeBuilder> bridge = body -> {
				body.aload(0);
				body.aload(1);
				body.invokevirtual(classDesc, "sub", scalar2self);
				body.areturn();
			};

			classBuilder.withMethodBody("sub", scalar2abstract,
					ClassFile.ACC_PUBLIC | ClassFile.ACC_BRIDGE | ClassFile.ACC_SYNTHETIC, bridge);
		}
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */

		// public SELF subOffset(long offset) { return cast(address - offset); }
		// bridge: AbstractPointer subOffset(long offset)
		/*[IF JAVA_SPEC_VERSION < 24]*/
		{
			method = clazz.visitMethod(ACC_PUBLIC, "subOffset", classFromLong, null, null);

			method.visitCode();
			method.visitVarInsn(ALOAD, 0);
			method.visitFieldInsn(GETFIELD, className, "address", Type.LONG_TYPE.getDescriptor());
			method.visitVarInsn(LLOAD, 1);
			method.visitInsn(LSUB);
			method.visitMethodInsn(INVOKESTATIC, className, "cast", classFromLong, false);
			method.visitInsn(ARETURN);
			method.visitMaxs(4, 3);
			method.visitEnd();

			method = clazz.visitMethod(ACC_PUBLIC | ACC_BRIDGE | ACC_SYNTHETIC, "subOffset", abstractPointerFromLong, null, null);

			method.visitCode();
			method.visitVarInsn(ALOAD, 0);
			method.visitVarInsn(LLOAD, 1);
			method.visitMethodInsn(INVOKEVIRTUAL, className, "subOffset", classFromLong, false);
			method.visitInsn(ARETURN);
			method.visitMaxs(3, 3);
			method.visitEnd();
		}
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		{
			Consumer<CodeBuilder> subOffset = body -> {
				body.aload(0);
				body.getfield(classDesc, "address", ConstantDescs.CD_long);
				body.lload(1);
				body.lsub();
				body.invokestatic(classDesc, "cast", long2self);
				body.areturn();
			};

			classBuilder.withMethodBody("subOffset", long2self, ClassFile.ACC_PUBLIC, subOffset);

			Consumer<CodeBuilder> bridge = body -> {
				body.aload(0);
				body.lload(1);
				body.invokevirtual(classDesc, "subOffset", long2self);
				body.areturn();
			};

			classBuilder.withMethodBody("subOffset", long2abstact,
					ClassFile.ACC_PUBLIC | ClassFile.ACC_BRIDGE | ClassFile.ACC_SYNTHETIC, bridge);
		}
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */

		// public SELF subOffset(Scalar offset) { return subOffset(offset.longValue()); }
		// bridge: AbstractPointer subOffset(Scalar offset)
		/*[IF JAVA_SPEC_VERSION < 24]*/
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

			method = clazz.visitMethod(ACC_PUBLIC | ACC_BRIDGE | ACC_SYNTHETIC, "subOffset", abstractPointerFromScalar, null, null);

			method.visitCode();
			method.visitVarInsn(ALOAD, 0);
			method.visitVarInsn(ALOAD, 1);
			method.visitMethodInsn(INVOKEVIRTUAL, className, "subOffset", classFromScalar, false);
			method.visitInsn(ARETURN);
			method.visitMaxs(2, 2);
			method.visitEnd();
		}
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		{
			Consumer<CodeBuilder> subOffset = body -> {
				body.aload(0);
				body.aload(1);
				body.invokevirtual(scalarDesc, "longValue", void2long);
				body.invokevirtual(classDesc, "subOffset", long2self);
				body.areturn();
			};

			classBuilder.withMethodBody("subOffset", scalar2self, ClassFile.ACC_PUBLIC, subOffset);

			Consumer<CodeBuilder> bridge = body -> {
				body.aload(0);
				body.aload(1);
				body.invokevirtual(classDesc, "subOffset", scalar2self);
				body.areturn();
			};

			classBuilder.withMethodBody("subOffset", scalar2abstract,
					ClassFile.ACC_PUBLIC | ClassFile.ACC_BRIDGE | ClassFile.ACC_SYNTHETIC, bridge);
		}
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */

		// public SELF untag(long mask) { return cast(address & ~mask); }
		// bridge: AbstractPointer untag(long tagBits)
		/*[IF JAVA_SPEC_VERSION < 24]*/
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

			method = clazz.visitMethod(ACC_PUBLIC | ACC_BRIDGE | ACC_SYNTHETIC, "untag", abstractPointerFromLong, null, null);

			method.visitCode();
			method.visitVarInsn(ALOAD, 0);
			method.visitVarInsn(LLOAD, 1);
			method.visitMethodInsn(INVOKEVIRTUAL, className, "untag", classFromLong, false);
			method.visitInsn(ARETURN);
			method.visitMaxs(3, 3);
			method.visitEnd();
		}
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		{
			Consumer<CodeBuilder> untag = body -> {
				body.aload(0);
				body.getfield(classDesc, "address", ConstantDescs.CD_long);
				body.lload(1);
				body.loadConstant(-1L);
				body.lxor();
				body.land();
				body.invokestatic(classDesc, "cast", long2self);
				body.areturn();
			};

			classBuilder.withMethodBody("untag", long2self, ClassFile.ACC_PUBLIC, untag);

			Consumer<CodeBuilder> bridge = body -> {
				body.aload(0);
				body.lload(1);
				body.invokevirtual(classDesc, "untag", long2self);
				body.areturn();
			};

			classBuilder.withMethodBody("untag", long2abstact,
					ClassFile.ACC_PUBLIC | ClassFile.ACC_BRIDGE | ClassFile.ACC_SYNTHETIC, bridge);
		}
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */

		// public SELF untag() { return untag(UDATA.SIZEOF - 1); }
		// bridge: AbstractPointer untag()
		/*[IF JAVA_SPEC_VERSION < 24]*/
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

			method = clazz.visitMethod(ACC_PUBLIC | ACC_BRIDGE | ACC_SYNTHETIC, "untag", Type.getMethodDescriptor(abstractPointerType), null, null);

			method.visitCode();
			method.visitVarInsn(ALOAD, 0);
			method.visitMethodInsn(INVOKEVIRTUAL, className, "untag", classFromVoid, false);
			method.visitInsn(ARETURN);
			method.visitMaxs(1, 1);
			method.visitEnd();
		}
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		{
			MethodTypeDesc void2self = MethodTypeDesc.of(classDesc);

			Consumer<CodeBuilder> untag = body -> {
				body.aload(0);
				body.loadConstant((long) (reader.getSizeOfUDATA() - 1));
				body.invokevirtual(classDesc, "untag", long2self);
				body.areturn();
			};

			classBuilder.withMethodBody("untag", void2self, ClassFile.ACC_PUBLIC, untag);

			Consumer<CodeBuilder> bridge = body -> {
				body.aload(0);
				body.invokevirtual(classDesc, "untag", void2self);
				body.areturn();
			};

			classBuilder.withMethodBody("untag", MethodTypeDesc.of(abstractPointerDesc),
					ClassFile.ACC_PUBLIC | ClassFile.ACC_BRIDGE | ClassFile.ACC_SYNTHETIC, bridge);
		}
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */

		// protected long sizeOfBaseType() { return BASE.SIZEOF; }
		/*[IF JAVA_SPEC_VERSION < 24]*/
		{
			method = clazz.visitMethod(ACC_PROTECTED, "sizeOfBaseType", longFromVoid, null, null);

			method.visitCode();
			loadLong(method, structure.getSizeOf());
			method.visitInsn(LRETURN);
			method.visitMaxs(2, 1);
			method.visitEnd();
		}
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		{
			Consumer<CodeBuilder> sizeof = body -> {
				body.loadConstant((long) structure.getSizeOf());
				body.lreturn();
			};

			classBuilder.withMethodBody("sizeOfBaseType", void2long, ClassFile.ACC_PROTECTED, sizeof);
		}
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */
	}

	/* Sample generated code:
	 *
	 * @com.ibm.j9ddr.GeneratedFieldAccessor(offsetFieldName="_fifteenMinuteAverageOffset_", declaredType="double")
	 * public double fifteenMinuteAverage() throws CorruptDataException {
	 *     return getDoubleAtOffset(J9PortSysInfoLoadData._fifteenMinuteAverageOffset_);
	 * }
	 */
	private void doDoubleMethod(FieldDescriptor field) {
		/*[IF JAVA_SPEC_VERSION < 24]*/
		MethodVisitor method = beginAnnotatedMethod(field, field.getName(), doubleFromVoid);

		method.visitCode();
		if (checkPresent(field, method)) {
			method.visitVarInsn(ALOAD, 0);
			loadLong(method, field.getOffset());
			method.visitMethodInsn(INVOKEVIRTUAL, className, "getDoubleAtOffset", doubleFromLong, false);
			method.visitInsn(DRETURN);
		}
		method.visitMaxs(3, 1);
		method.visitEnd();
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		MethodTypeDesc methodDesc = MethodTypeDesc.of(ConstantDescs.CD_double);
		MethodTypeDesc long2double = MethodTypeDesc.of(ConstantDescs.CD_double, ConstantDescs.CD_long);

		Consumer<CodeBuilder> accessor = body -> {
			if (checkPresent(field, body)) {
				body.aload(0);
				body.loadConstant((long) field.getOffset());
				body.invokevirtual(classDesc, "getDoubleAtOffset", long2double);
				body.dreturn();
			}
		};

		addAnnotatedMethod(field, field.getName(), methodDesc, accessor);
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */

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

		/*[IF JAVA_SPEC_VERSION < 24]*/
		String returnDesc = Type.getMethodDescriptor(Type.getObjectType(qualifiedReturnType));
		String actualDesc = Type.getMethodDescriptor(Type.getObjectType(qualifiedActualType), Type.LONG_TYPE);

		MethodVisitor method = beginMethod(field, accessorName, returnDesc);

		method.visitCode();
		if (checkPresent(field, method)) {
			method.visitVarInsn(ALOAD, 0);
			loadLong(method, field.getOffset());
			method.visitMethodInsn(INVOKEVIRTUAL, className, "nonNullFieldEA", longFromLong, false);
			method.visitMethodInsn(INVOKESTATIC, qualifiedActualType, "cast", actualDesc, false);
			method.visitInsn(ARETURN);
		}
		method.visitMaxs(3, 1);
		method.visitEnd();
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		ClassDesc returnDesc = ClassDesc.ofInternalName(qualifiedReturnType);
		ClassDesc actualDesc = ClassDesc.ofInternalName(qualifiedActualType);
		MethodTypeDesc methodDesc = MethodTypeDesc.of(returnDesc);
		MethodTypeDesc castDesc = MethodTypeDesc.of(actualDesc, ConstantDescs.CD_long);

		Consumer<CodeBuilder> accessor = body -> {
			if (checkPresent(field, body)) {
				body.aload(0);
				body.loadConstant((long) field.getOffset());
				body.invokevirtual(classDesc, "nonNullFieldEA", long2long);
				body.invokestatic(actualDesc, "cast", castDesc);
				body.areturn();
			}
		};

		addMethod(field, accessorName, methodDesc, accessor);
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */
	}

	/* Sample generated code:
	 *
	 * public EnumPointer _buildResultEA() throws CorruptDataException {
	 *     return EnumPointer.cast(nonNullFieldEA(ClassFileWriter.__buildResultOffset_), BuildResult.class);
	 * }
	 */
	private void doEnumEAMethod(FieldDescriptor field) {
		String accessorName = field.getName() + "EA";
		String enumPointerDesc = qualifyPointerType("Enum");

		/*[IF JAVA_SPEC_VERSION < 24]*/
		Type enumPointerType = Type.getObjectType(enumPointerDesc);
		String returnDesc = Type.getMethodDescriptor(enumPointerType);
		String castDesc = Type.getMethodDescriptor(enumPointerType, Type.LONG_TYPE, Type.getType(Class.class));
		Type enumType = Type.getObjectType(qualifyType(getEnumType(field.getType())));

		MethodVisitor method = beginMethod(field, accessorName, returnDesc);

		method.visitCode();
		if (checkPresent(field, method)) {
			method.visitVarInsn(ALOAD, 0);
			loadLong(method, field.getOffset());
			method.visitMethodInsn(INVOKEVIRTUAL, className, "nonNullFieldEA", longFromLong, false);
			method.visitLdcInsn(enumType);
			method.visitMethodInsn(INVOKESTATIC, enumPointerDesc, "cast", castDesc, false);
			method.visitInsn(ARETURN);
		}
		method.visitMaxs(3, 1);
		method.visitEnd();
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		ClassDesc enumDesc = ClassDesc.ofInternalName(qualifyType(getEnumType(field.getType())));
		MethodTypeDesc castDesc = MethodTypeDesc.of(enumDesc, ConstantDescs.CD_long, ConstantDescs.CD_Class);
		MethodTypeDesc methodDesc = MethodTypeDesc.of(enumDesc);

		Consumer<CodeBuilder> accessor = body -> {
			if (checkPresent(field, body)) {
				body.aload(0);
				body.loadConstant((long) field.getOffset());
				body.invokevirtual(classDesc, "nonNullFieldEA", long2long);
				body.ldc(enumDesc);
				body.invokestatic(enumDesc, "cast", castDesc);
				body.areturn();
			}
		};

		addMethod(field, accessorName, methodDesc, accessor);
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */
	}

	/* Sample generated code:
	 *
	 * @com.ibm.j9ddr.GeneratedFieldAccessor(offsetFieldName="__allocationContextTypeOffset_", declaredType="const MM_AllocationContextTarok$AllocationContextType")
	 * public long _allocationContextType() throws CorruptDataException {
	 *     return getIntAtOffset(J9ProcessorDesc.__allocationContextTypeOffset_);
	 * }
	 *
	 * If the size of the field type is not 4, getByteAtOffset, getShortAtOffset
	 * or getLongAtOffset is used as appropriate.
	 */
	private void doEnumMethod(FieldDescriptor field) {
		String enumType = getEnumType(field.getType());
		int enumSize = reader.getStructureSizeOf(enumType);
		PrimitiveAccessor accessor = PrimitiveAccessor.forSize(enumSize);

		/*[IF JAVA_SPEC_VERSION < 24]*/
		MethodVisitor method = beginAnnotatedMethod(field, field.getName(), longFromVoid);

		method.visitCode();
		if (checkPresent(field, method)) {
			method.visitVarInsn(ALOAD, 0);
			loadLong(method, field.getOffset());
			method.visitMethodInsn(INVOKEVIRTUAL, className, accessor.methodName, accessor.descriptor, false);
			if (!accessor.returnsLong) {
				method.visitInsn(I2L);
			}
			method.visitInsn(LRETURN);
		}
		method.visitMaxs(3, 1);
		method.visitEnd();
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		Consumer<CodeBuilder> code = body -> {
			if (checkPresent(field, body)) {
				body.aload(0);
				body.loadConstant((long) field.getOffset());
				body.invokevirtual(classDesc, accessor.methodName, accessor.descriptor);
				if (!accessor.returnsLong) {
					body.i2l();
				}
				body.lreturn();
			}
		};

		addAnnotatedMethod(field, field.getName(), void2long, code);
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */

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

		/*[IF JAVA_SPEC_VERSION < 24]*/
		Type enumPointerType = Type.getObjectType(enumPointerDesc);
		String returnDesc = Type.getMethodDescriptor(enumPointerType);
		String castDesc = Type.getMethodDescriptor(enumPointerType, Type.LONG_TYPE, Type.getType(Class.class));
		String targetType = getTargetType(getEnumType(field.getType()));
		Type enumType = Type.getObjectType(qualifyType(targetType));

		MethodVisitor method = beginMethod(field, field.getName(), returnDesc);

		method.visitCode();
		if (checkPresent(field, method)) {
			method.visitVarInsn(ALOAD, 0);
			loadLong(method, field.getOffset());
			method.visitMethodInsn(INVOKEVIRTUAL, className, "getPointerAtOffset", longFromLong, false);
			method.visitLdcInsn(enumType);
			method.visitMethodInsn(INVOKESTATIC, enumPointerDesc, "cast", castDesc, false);
			method.visitInsn(ARETURN);
		}
		method.visitMaxs(3, 2);
		method.visitEnd();
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		ClassDesc enumDesc = ClassDesc.ofInternalName(enumPointerDesc);
		MethodTypeDesc methodDesc = MethodTypeDesc.of(enumDesc);
		MethodTypeDesc castDesc = MethodTypeDesc.of(enumDesc, ConstantDescs.CD_long);

		Consumer<CodeBuilder> code = body -> {
			if (checkPresent(field, body)) {
				body.aload(0);
				body.loadConstant((long) field.getOffset());
				body.invokevirtual(classDesc, "getPointerAtOffset", long2long);
				body.ldc(enumDesc);
				body.invokestatic(enumDesc, "cast", castDesc);
				body.areturn();
			}
		};

		addAnnotatedMethod(field, field.getName(), methodDesc, code);
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */

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
		/*[IF JAVA_SPEC_VERSION < 24]*/
		Type objectType = Type.getObjectType(qualifyPointerType("J9Object"));
		String returnDesc = Type.getMethodDescriptor(objectType);
		String accessorDesc = Type.getMethodDescriptor(objectType, Type.LONG_TYPE);

		MethodVisitor method = beginAnnotatedMethod(field, field.getName(), returnDesc);

		method.visitCode();
		if (checkPresent(field, method)) {
			method.visitVarInsn(ALOAD, 0);
			loadLong(method, field.getOffset());
			method.visitMethodInsn(INVOKEVIRTUAL, className, "getObjectReferenceAtOffset", accessorDesc, false);
			method.visitInsn(ARETURN);
		}
		method.visitMaxs(3, 1);
		method.visitEnd();
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		ClassDesc objectDesc = ClassDesc.ofInternalName(qualifyPointerType("J9Object"));
		MethodTypeDesc methodDesc = MethodTypeDesc.of(objectDesc);
		MethodTypeDesc accessorDesc = MethodTypeDesc.of(objectDesc, ConstantDescs.CD_long);

		Consumer<CodeBuilder> code = body -> {
			if (checkPresent(field, body)) {
				body.aload(0);
				body.loadConstant((long) field.getOffset());
				body.invokevirtual(classDesc, "getObjectReferenceAtOffset", accessorDesc);
				body.areturn();
			}
		};

		addAnnotatedMethod(field, field.getName(), methodDesc, code);
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */

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

		/*[IF JAVA_SPEC_VERSION < 24]*/
		Type objectType = Type.getObjectType(returnType);
		String returnDesc = Type.getMethodDescriptor(objectType);
		String castDesc = Type.getMethodDescriptor(objectType, Type.LONG_TYPE);

		MethodVisitor method = beginAnnotatedMethod(field, field.getName(), returnDesc);

		method.visitCode();
		if (checkPresent(field, method)) {
			method.visitVarInsn(ALOAD, 0);
			loadLong(method, field.getOffset());
			method.visitMethodInsn(INVOKEVIRTUAL, className, "getPointerAtOffset", longFromLong, false);
			method.visitMethodInsn(INVOKESTATIC, returnType, "cast", castDesc, false);
			method.visitInsn(ARETURN);
		}
		method.visitMaxs(3, 1);
		method.visitEnd();
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		ClassDesc returnDesc = ClassDesc.ofInternalName(returnType);
		MethodTypeDesc methodDesc = MethodTypeDesc.of(returnDesc);
		MethodTypeDesc castDesc = MethodTypeDesc.of(returnDesc, ConstantDescs.CD_long);

		Consumer<CodeBuilder> code = body -> {
			if (checkPresent(field, body)) {
				body.aload(0);
				body.loadConstant((long) field.getOffset());
				body.invokevirtual(classDesc, "getPointerAtOffset", long2long);
				body.invokestatic(returnDesc, "cast", castDesc);
				body.areturn();
			}
		};

		addAnnotatedMethod(field, field.getName(), methodDesc, code);
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */

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
		/*[IF JAVA_SPEC_VERSION < 24]*/
		MethodVisitor method = beginAnnotatedMethod(field, field.getName(), floatFromVoid);

		method.visitCode();
		if (checkPresent(field, method)) {
			method.visitVarInsn(ALOAD, 0);
			loadLong(method, field.getOffset());
			method.visitMethodInsn(INVOKEVIRTUAL, className, "getFloatAtOffset", floatFromLong, false);
			method.visitInsn(FRETURN);
		}
		method.visitMaxs(3, 1);
		method.visitEnd();
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		MethodTypeDesc methodDesc = MethodTypeDesc.of(ConstantDescs.CD_float);
		MethodTypeDesc long2double = MethodTypeDesc.of(ConstantDescs.CD_float, ConstantDescs.CD_long);

		Consumer<CodeBuilder> accessor = body -> {
			if (checkPresent(field, body)) {
				body.aload(0);
				body.loadConstant((long) field.getOffset());
				body.invokevirtual(classDesc, "getFloatAtOffset", long2double);
				body.freturn();
			}
		};

		addAnnotatedMethod(field, field.getName(), methodDesc, accessor);
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */

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

		/*[IF JAVA_SPEC_VERSION < 24]*/
		Type objectType = Type.getObjectType(returnType);
		String returnDesc = Type.getMethodDescriptor(objectType);
		String accessorDesc = Type.getMethodDescriptor(objectType, Type.LONG_TYPE);

		MethodVisitor method = beginAnnotatedMethod(field, field.getName(), returnDesc);

		method.visitCode();
		if (checkPresent(field, method)) {
			method.visitVarInsn(ALOAD, 0);
			loadLong(method, field.getOffset());
			method.visitMethodInsn(INVOKEVIRTUAL, className, "getObjectClassAtOffset", accessorDesc, false);
			method.visitInsn(ARETURN);
		}
		method.visitMaxs(3, 1);
		method.visitEnd();
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		ClassDesc objectDesc = ClassDesc.ofInternalName(returnType);
		MethodTypeDesc methodDesc = MethodTypeDesc.of(objectDesc);
		MethodTypeDesc accessorDesc = MethodTypeDesc.of(objectDesc, ConstantDescs.CD_long);

		Consumer<CodeBuilder> code = body -> {
			if (checkPresent(field, body)) {
				body.aload(0);
				body.loadConstant((long) field.getOffset());
				body.invokevirtual(classDesc, "getObjectClassAtOffset", accessorDesc);
				body.areturn();
			}
		};

		addAnnotatedMethod(field, field.getName(), methodDesc, code);
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */

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

		/*[IF JAVA_SPEC_VERSION < 24]*/
		Type objectType = Type.getObjectType(returnType);
		String returnDesc = Type.getMethodDescriptor(objectType);
		String accessorDesc = Type.getMethodDescriptor(objectType, Type.LONG_TYPE);

		MethodVisitor method = beginAnnotatedMethod(field, field.getName(), returnDesc);

		method.visitCode();
		if (checkPresent(field, method)) {
			method.visitVarInsn(ALOAD, 0);
			loadLong(method, field.getOffset());
			method.visitMethodInsn(INVOKEVIRTUAL, className, "getPointerAtOffset", longFromLong, false);
			method.visitMethodInsn(INVOKESTATIC, returnType, "cast", accessorDesc, false);
			method.visitInsn(ARETURN);
		}
		method.visitMaxs(3, 1);
		method.visitEnd();
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		ClassDesc objectDesc = ClassDesc.ofInternalName(returnType);
		MethodTypeDesc methodDesc = MethodTypeDesc.of(objectDesc);
		MethodTypeDesc accessorDesc = MethodTypeDesc.of(objectDesc, ConstantDescs.CD_long);

		Consumer<CodeBuilder> code = body -> {
			if (checkPresent(field, body)) {
				body.aload(0);
				body.loadConstant((long) field.getOffset());
				body.invokevirtual(classDesc, "getPointerAtOffset", long2long);
				body.invokestatic(objectDesc, "cast", accessorDesc);
				body.areturn();
			}
		};

		addAnnotatedMethod(field, field.getName(), methodDesc, code);
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */

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

		/*[IF JAVA_SPEC_VERSION < 24]*/
		Type objectType = Type.getObjectType(returnType);
		String returnDesc = Type.getMethodDescriptor(objectType);
		String accessorDesc = Type.getMethodDescriptor(objectType, Type.LONG_TYPE);

		MethodVisitor method = beginAnnotatedMethod(field, field.getName(), returnDesc);

		method.visitCode();
		if (checkPresent(field, method)) {
			method.visitVarInsn(ALOAD, 0);
			loadLong(method, field.getOffset());
			method.visitMethodInsn(INVOKEVIRTUAL, className, "getObjectMonitorAtOffset", accessorDesc, false);
			method.visitInsn(ARETURN);
		}
		method.visitMaxs(3, 1);
		method.visitEnd();
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		ClassDesc objectDesc = ClassDesc.ofInternalName(returnType);
		MethodTypeDesc methodDesc = MethodTypeDesc.of(objectDesc);
		MethodTypeDesc accessorDesc = MethodTypeDesc.of(objectDesc, ConstantDescs.CD_long);

		Consumer<CodeBuilder> code = body -> {
			if (checkPresent(field, body)) {
				body.aload(0);
				body.loadConstant((long) field.getOffset());
				body.invokevirtual(classDesc, "getObjectMonitorAtOffset", accessorDesc);
				body.areturn();
			}
		};

		addAnnotatedMethod(field, field.getName(), methodDesc, code);
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */

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

		/*[IF JAVA_SPEC_VERSION < 24]*/
		Type objectType = Type.getObjectType(returnType);
		String returnDesc = Type.getMethodDescriptor(objectType);
		String castDesc = Type.getMethodDescriptor(objectType, Type.LONG_TYPE);

		MethodVisitor method = beginAnnotatedMethod(field, field.getName(), returnDesc);

		method.visitCode();
		if (checkPresent(field, method)) {
			method.visitVarInsn(ALOAD, 0);
			loadLong(method, field.getOffset());
			method.visitMethodInsn(INVOKEVIRTUAL, className, "getPointerAtOffset", longFromLong, false);
			method.visitMethodInsn(INVOKESTATIC, returnType, "cast", castDesc, false);
			method.visitInsn(ARETURN);
		}
		method.visitMaxs(3, 1);
		method.visitEnd();
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		ClassDesc objectDesc = ClassDesc.ofInternalName(returnType);
		MethodTypeDesc methodDesc = MethodTypeDesc.of(objectDesc);
		MethodTypeDesc accessorDesc = MethodTypeDesc.of(objectDesc, ConstantDescs.CD_long);

		Consumer<CodeBuilder> code = body -> {
			if (checkPresent(field, body)) {
				body.aload(0);
				body.loadConstant((long) field.getOffset());
				body.invokevirtual(classDesc, "getPointerAtOffset", long2long);
				body.invokestatic(objectDesc, "cast", accessorDesc);
				body.areturn();
			}
		};

		addAnnotatedMethod(field, field.getName(), methodDesc, code);
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */

		doEAMethod("Pointer", field);
	}

	private void doNullInstance() {
		// public static final SELF NULL = new SELF(0);
		/*[IF JAVA_SPEC_VERSION < 24]*/
		{
			clazz.visitField(ACC_PUBLIC | ACC_STATIC | ACC_FINAL, "NULL", classType.getDescriptor(), null, null).visitEnd();

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
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		{
			classBuilder.withField("NULL", classDesc,
					ClassFile.ACC_PUBLIC | ClassFile.ACC_STATIC | ClassFile.ACC_FINAL);

			Consumer<CodeBuilder> clinit = body -> {
				body.new_(classDesc);
				body.dup();
				body.lconst_0();
				body.invokespecial(classDesc, ConstantDescs.INIT_NAME, long2void);
				body.putstatic(classDesc, "NULL", classDesc);
				body.return_();
			};

			classBuilder.withMethodBody(ConstantDescs.CLASS_INIT_NAME, ConstantDescs.MTD_void, ClassFile.ACC_STATIC,
					clinit);
		}
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */
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

		String returnType = generalizeSimpleType(targetType);
		String qualifiedReturnType = qualifyPointerType(returnType);

		/*[IF JAVA_SPEC_VERSION < 24]*/
		String castDesc = Type.getMethodDescriptor(Type.getObjectType(qualifiedTargetType), Type.LONG_TYPE);
		String returnDesc = Type.getMethodDescriptor(Type.getObjectType(qualifiedReturnType));
		MethodVisitor method = beginAnnotatedMethod(field, field.getName(), returnDesc);

		method.visitCode();
		if (checkPresent(field, method)) {
			method.visitVarInsn(ALOAD, 0);
			loadLong(method, field.getOffset());
			method.visitMethodInsn(INVOKEVIRTUAL, className, "getPointerAtOffset", longFromLong, false);
			method.visitMethodInsn(INVOKESTATIC, qualifiedTargetType, "cast", castDesc, false);
			method.visitInsn(ARETURN);
		}
		method.visitMaxs(3, 1);
		method.visitEnd();
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		ClassDesc returnDesc = ClassDesc.ofInternalName(qualifiedReturnType);
		ClassDesc targetDesc = ClassDesc.ofInternalName(qualifiedTargetType);
		MethodTypeDesc castDesc = MethodTypeDesc.of(targetDesc, ConstantDescs.CD_long);
		MethodTypeDesc methodDesc = MethodTypeDesc.of(returnDesc);

		Consumer<CodeBuilder> code = body -> {
			if (checkPresent(field, body)) {
				body.aload(0);
				body.loadConstant((long) field.getOffset());
				body.invokevirtual(classDesc, "getPointerAtOffset", long2long);
				body.invokestatic(targetDesc, "cast", castDesc);
				body.areturn();
			}
		};

		addAnnotatedMethod(field, field.getName(), methodDesc, code);
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */

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

		/*[IF JAVA_SPEC_VERSION < 24]*/
		String returnDesc = Type.getMethodDescriptor(Type.getObjectType(qualifiedReturnType));
		MethodVisitor method = beginAnnotatedMethod(field, field.getName(), returnDesc);

		method.visitCode();
		if (checkPresent(field, method)) {
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
		}
		method.visitMaxs(5, 1);
		method.visitEnd();
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		ClassDesc returnDesc = ClassDesc.ofInternalName(qualifiedReturnType);
		ClassDesc qualifiedFieldDesc = ClassDesc.ofInternalName(qualifiedFieldType);
		MethodTypeDesc methodDesc = MethodTypeDesc.of(returnDesc);

		Consumer<CodeBuilder> code = body -> {
			if (checkPresent(field, body)) {
				body.new_(qualifiedFieldDesc);
				body.dup();
				body.aload(0);
				body.loadConstant((long) field.getOffset());
				body.invokevirtual(classDesc, accessor.methodName, accessor.descriptor);
				if (!accessor.returnsLong) {
					body.i2l();
				}
				body.invokespecial(qualifiedFieldDesc, ConstantDescs.INIT_NAME, long2void);
				body.areturn();
			}
		};

		addAnnotatedMethod(field, field.getName(), methodDesc, code);
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */

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

		/*[IF JAVA_SPEC_VERSION < 24]*/
		Type returnType = Type.getObjectType(returnTypeName);
		String returnDesc = Type.getMethodDescriptor(returnType);
		String castDesc = Type.getMethodDescriptor(returnType, Type.LONG_TYPE);
		MethodVisitor method = beginMethod(field, accessorName, returnDesc);

		method.visitCode();
		if (checkPresent(field, method)) {
			method.visitVarInsn(ALOAD, 0);
			loadLong(method, field.getOffset());
			method.visitMethodInsn(INVOKEVIRTUAL, className, "nonNullFieldEA", longFromLong, false);
			method.visitMethodInsn(INVOKESTATIC, returnTypeName, "cast", castDesc, false);
			method.visitInsn(ARETURN);
		}
		method.visitMaxs(3, 1);
		method.visitEnd();
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		ClassDesc returnDesc = ClassDesc.ofInternalName(returnTypeName);
		MethodTypeDesc castDesc = MethodTypeDesc.of(returnDesc, ConstantDescs.CD_long);
		MethodTypeDesc methodDesc = MethodTypeDesc.of(returnDesc);

		Consumer<CodeBuilder> code = body -> {
			if (checkPresent(field, body)) {
				body.aload(0);
				body.loadConstant((long) field.getOffset());
				body.invokevirtual(classDesc, "nonNullFieldEA", long2long);
				body.invokestatic(returnDesc, "cast", castDesc);
				body.areturn();
			}
		};

		addMethod(field, accessorName, methodDesc, code);
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */
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
		String qualifiedPointerName = qualifyPointerType(targetType);

		/*[IF JAVA_SPEC_VERSION < 24]*/
		Type returnType = Type.getObjectType(returnTypeName);
		String returnDesc = Type.getMethodDescriptor(returnType);
		String qualifiedPointerDesc = Type.getObjectType(qualifiedPointerName).getDescriptor();
		MethodVisitor method = beginAnnotatedMethod(field, field.getName(), returnDesc);

		method.visitCode();
		if (checkPresent(field, method)) {
			Label nonNull = new Label();

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
		}
		method.visitMaxs(4, isWide ? 3 : 2);
		method.visitEnd();
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		ClassDesc returnDesc = ClassDesc.ofInternalName(returnTypeName);
		ClassDesc qualifiedPointerDesc = ClassDesc.ofInternalName(qualifiedPointerName);
		MethodTypeDesc castDesc = MethodTypeDesc.of(returnDesc, ConstantDescs.CD_long);
		MethodTypeDesc methodDesc = MethodTypeDesc.of(returnDesc);

		Consumer<CodeBuilder> code = body -> {
			if (checkPresent(field, body)) {
				Label nonNull = body.newLabel();

				body.aload(0);
				body.loadConstant((long) field.getOffset());
				if (isWide) {
					body.invokevirtual(classDesc, "getPointerAtOffset", long2long);
					body.lstore(1);

					body.lload(1);
					body.lconst_0();
					body.lcmp();
				} else {
					body.invokevirtual(classDesc, "getIntAtOffset", long2int);
					body.istore(1);

					body.iload(1);
				}
				body.ifne(nonNull);

				body.getstatic(qualifiedPointerDesc, "NULL", qualifiedPointerDesc);
				body.areturn();

				body.labelBinding(nonNull);
				body.aload(0);
				body.getfield(classDesc, "address", ConstantDescs.CD_long);
				addLong(body, field.getOffset());
				if (isWide) {
					body.lload(1);
				} else {
					body.iload(1).i2l();
				}
				body.ladd();

				body.invokestatic(qualifiedPointerDesc, "cast", castDesc);
				body.areturn();
			}
		};

		addAnnotatedMethod(field, field.getName(), methodDesc, code);
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */

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

		/*[IF JAVA_SPEC_VERSION < 24]*/
		Type objectType = Type.getObjectType(returnType);
		String returnDesc = Type.getMethodDescriptor(objectType);
		String castDesc = Type.getMethodDescriptor(objectType, Type.LONG_TYPE);
		MethodVisitor method = beginAnnotatedMethod(field, field.getName(), returnDesc);

		method.visitCode();
		if (checkPresent(field, method)) {
			method.visitVarInsn(ALOAD, 0);
			loadLong(method, field.getOffset());
			method.visitMethodInsn(INVOKEVIRTUAL, className, "getPointerAtOffset", longFromLong, false);
			method.visitMethodInsn(INVOKESTATIC, returnType, "cast", castDesc, false);
			method.visitInsn(ARETURN);
		}
		method.visitMaxs(3, 1);
		method.visitEnd();
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		ClassDesc returnDesc = ClassDesc.ofInternalName(returnType);
		MethodTypeDesc methodDesc = MethodTypeDesc.of(returnDesc);
		MethodTypeDesc castDesc = MethodTypeDesc.of(returnDesc, ConstantDescs.CD_long);

		Consumer<CodeBuilder> code = body -> {
			if (checkPresent(field, body)) {
				body.aload(0);
				body.loadConstant((long) field.getOffset());
				body.invokevirtual(classDesc, "getPointerAtOffset", long2long);
				body.invokestatic(returnDesc, "cast", castDesc);
				body.areturn();
			}
		};

		addAnnotatedMethod(field, field.getName(), methodDesc, code);
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */

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

		/*[IF JAVA_SPEC_VERSION < 24]*/
		Type objectType = Type.getObjectType(qualifiedReturnType);
		String returnDesc = Type.getMethodDescriptor(objectType);
		String castDesc = Type.getMethodDescriptor(objectType, Type.LONG_TYPE);
		MethodVisitor method = beginAnnotatedMethod(field, field.getName(), returnDesc);

		method.visitCode();
		if (checkPresent(field, method)) {
			method.visitVarInsn(ALOAD, 0);
			loadLong(method, field.getOffset());
			method.visitMethodInsn(INVOKEVIRTUAL, className, "nonNullFieldEA", longFromLong, false);
			method.visitMethodInsn(INVOKESTATIC, qualifiedReturnType, "cast", castDesc, false);
			method.visitInsn(ARETURN);
		}
		method.visitMaxs(3, 1);
		method.visitEnd();
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		ClassDesc returnDesc = ClassDesc.ofInternalName(qualifiedReturnType);
		MethodTypeDesc castDesc = MethodTypeDesc.of(returnDesc, ConstantDescs.CD_long);
		MethodTypeDesc methodDesc = MethodTypeDesc.of(returnDesc);

		Consumer<CodeBuilder> accessor = body -> {
			if (checkPresent(field, body)) {
				body.aload(0);
				body.loadConstant((long) field.getOffset());
				body.invokevirtual(classDesc, "nonNullFieldEA", long2long);
				body.invokestatic(returnDesc, "cast", castDesc);
				body.areturn();
			}
		};

		addAnnotatedMethod(field, field.getName(), methodDesc, accessor);
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */

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

		/*[IF JAVA_SPEC_VERSION < 24]*/
		Type objectType = Type.getObjectType(qualifiedReturnType);
		String returnDesc = Type.getMethodDescriptor(objectType);
		String castDesc = Type.getMethodDescriptor(objectType, Type.LONG_TYPE);
		MethodVisitor method = beginAnnotatedMethod(field, field.getName(), returnDesc);

		method.visitCode();
		if (checkPresent(field, method)) {
			method.visitVarInsn(ALOAD, 0);
			loadLong(method, field.getOffset());
			method.visitMethodInsn(INVOKEVIRTUAL, className, "getPointerAtOffset", longFromLong, false);
			method.visitMethodInsn(INVOKESTATIC, qualifiedReturnType, "cast", castDesc, false);
			method.visitInsn(ARETURN);
		}
		method.visitMaxs(3, 1);
		method.visitEnd();
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		ClassDesc returnDesc = ClassDesc.ofInternalName(qualifiedReturnType);
		MethodTypeDesc castDesc = MethodTypeDesc.of(returnDesc, ConstantDescs.CD_long);
		MethodTypeDesc methodDesc = MethodTypeDesc.of(returnDesc);

		Consumer<CodeBuilder> accessor = body -> {
			if (checkPresent(field, body)) {
				body.aload(0);
				body.loadConstant((long) field.getOffset());
				body.invokevirtual(classDesc, "getPointerAtOffset", long2long);
				body.invokestatic(returnDesc, "cast", castDesc);
				body.areturn();
			}
		};

		addAnnotatedMethod(field, field.getName(), methodDesc, accessor);
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */

		doEAMethod("Pointer", field);
	}

	private byte[] generate() {
		String superClassName = getSuperClassName();

		/*[IF JAVA_SPEC_VERSION < 24]*/
		clazz.visit(V1_8, ACC_PUBLIC | ACC_SUPER, className, null, superClassName, null);

		doClassAnnotation();
		doNullInstance();
		doConstructors(superClassName);
		doAccessorMethods();

		clazz.visitEnd();

		return clazz.toByteArray();
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		superDesc = ClassDesc.ofInternalName(superClassName);

		Consumer<ClassBuilder> builder = body -> {
			classBuilder = body;

			body.withFlags(ClassFile.ACC_PUBLIC | ClassFile.ACC_SUPER);
			body.withSuperclass(superDesc);

			doClassAnnotation();
			doNullInstance();
			doConstructors(superClassName);
			doAccessorMethods();
		};

		return ClassFile.of().build(classDesc, builder);
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */
	}

	private String getStructureClassName() {
		return basePrefix + "structure/" + structure.getName();
	}

	private String getSuperClassName() {
		String superName = structure.getSuperName();
		String superClassName;

		if (superName.isEmpty()) {
			superClassName = basePrefix + "pointer/StructurePointer";
		} else {
			superClassName = basePrefix + "pointer/generated/" + superName + "Pointer";
		}

		return superClassName;
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

	/*[IF JAVA_SPEC_VERSION < 24]*/
	final String descriptor;
	/*[ELSE] JAVA_SPEC_VERSION < 24 */
	final MethodTypeDesc descriptor;
	/*[ENDIF] JAVA_SPEC_VERSION < 24 */

	final String methodName;

	final boolean returnsLong;

	PrimitiveAccessor(String methodName, String descriptor, boolean returnsLong) {
		this.methodName = methodName;
		/*[IF JAVA_SPEC_VERSION < 24]*/
		this.descriptor = descriptor;
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		this.descriptor = MethodTypeDesc.ofDescriptor(descriptor);
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */
		this.returnsLong = returnsLong;
	}

}

final class StructureHelper extends HelperBase {

	private static final Pattern BaseTypePattern = Pattern.compile(".+?(\\d+)");

	private static int getCellSize(String type) {
		int size = 0;
		Matcher macher = BaseTypePattern.matcher(type);
		if (macher.matches()) {
			try {
				size = Integer.parseInt(macher.group(1));
			} catch (NumberFormatException e) {
				// ignore
			}
		}

		switch (size) {
		default:
			// Assume 32 for backwards compatibility.
			size = 32;
			break;
		case 8:
		case 16:
		case 32:
		case 64:
			break;
		}

		return size;
	}

	static byte[] getClassBytes(StructureDescriptor structure, String className) {
		StructureHelper helper = new StructureHelper(structure, className);

		return helper.generate();
	}

	private final String className;

	/*[IF JAVA_SPEC_VERSION < 24]*/
	private final ClassWriter clazz;

	private final MethodVisitor clinit;
	/*[ENDIF] JAVA_SPEC_VERSION < 24 */

	private final StructureDescriptor structure;

	private StructureHelper(StructureDescriptor structure, String className) {
		super();
		this.className = className;
		this.structure = structure;

		/*[IF JAVA_SPEC_VERSION < 24]*/
		this.clazz = new ClassWriter(0);
		this.clazz.visit(V1_8, ACC_PUBLIC | ACC_FINAL | ACC_SUPER, className, null, "java/lang/Object", null);
		this.clinit = clazz.visitMethod(ACC_STATIC, "<clinit>", voidMethod, null, null);
		this.clinit.visitCode();
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */
	}

	/*[IF JAVA_SPEC_VERSION < 24]*/
	private void defineField(String name, Type type, long value) {
		String typeDescriptor = type.getDescriptor();

		clazz.visitField(ACC_PUBLIC | ACC_STATIC | ACC_FINAL, name, typeDescriptor, null, null).visitEnd();

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
		for (Map.Entry<String, Integer> entry : getFieldMap().entrySet()) {
			String fieldName = entry.getKey();
			int fieldValue = entry.getValue().intValue();

			defineField(fieldName, Type.INT_TYPE, fieldValue);
		}

		clinit.visitInsn(RETURN);
		clinit.visitMaxs(2, 0);
		clinit.visitEnd();
	}
	/*[ENDIF] JAVA_SPEC_VERSION < 24 */

	private byte[] generate() {
		/*[IF JAVA_SPEC_VERSION < 24]*/
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
		/*[ELSE] JAVA_SPEC_VERSION < 24 */
		ClassDesc classDesc = ClassDesc.ofInternalName(className);
		Map<String, Integer> fieldMap = getFieldMap();

		Consumer<CodeBuilder> clinit = body -> {
			body.loadConstant((long) structure.getSizeOf());
			body.putstatic(classDesc, "SIZEOF", ConstantDescs.CD_long);

			// other constants
			for (ConstantDescriptor constant : structure.getConstants()) {
				body.loadConstant(constant.getValue());
				body.putstatic(classDesc, constant.getName(), ConstantDescs.CD_long);
			}

			for (Map.Entry<String, Integer> entry : fieldMap.entrySet()) {
				body.loadConstant(entry.getValue().intValue());
				body.putstatic(classDesc, entry.getKey(), ConstantDescs.CD_int);
			}

			body.return_();
		};

		Consumer<CodeBuilder> init = body -> {
			body.aload(0);
			body.invokespecial(ConstantDescs.CD_Object, ConstantDescs.INIT_NAME, ConstantDescs.MTD_void);
			body.return_();
		};

		Consumer<ClassBuilder> builder = classBody -> {
			classBody.withFlags(ClassFile.ACC_PUBLIC | ClassFile.ACC_FINAL | ClassFile.ACC_SUPER);

			classBody.withField("SIZEOF", ConstantDescs.CD_long,
					ClassFile.ACC_PUBLIC | ClassFile.ACC_STATIC | ClassFile.ACC_FINAL);

			// other constants
			for (ConstantDescriptor constant : structure.getConstants()) {
				classBody.withField(constant.getName(), ConstantDescs.CD_long,
						ClassFile.ACC_PUBLIC | ClassFile.ACC_STATIC | ClassFile.ACC_FINAL);
			}

			for (String fieldName : fieldMap.keySet()) {
				classBody.withField(fieldName, ConstantDescs.CD_int,
						ClassFile.ACC_PUBLIC | ClassFile.ACC_STATIC | ClassFile.ACC_FINAL);
			}

			classBody.withMethodBody(ConstantDescs.CLASS_INIT_NAME, ConstantDescs.MTD_void, ClassFile.ACC_STATIC,
					clinit);

			classBody.withMethodBody(ConstantDescs.INIT_NAME, ConstantDescs.MTD_void, ClassFile.ACC_PUBLIC, init);
		};

		return ClassFile.of().build(classDesc, builder);
		/*[ENDIF] JAVA_SPEC_VERSION < 24 */
	}

	private Map<String, Integer> getFieldMap() {
		Map<String, Integer> fields = new LinkedHashMap<>();

		int bitFieldBitCount = 0;
		for (FieldDescriptor field : structure.getFields()) {
			if (!field.isPresent()) {
				continue;
			}

			String fieldName = field.getName();
			int fieldOffset = field.getOffset();
			String type = field.getType();
			int colonIndex = type.lastIndexOf(':');

			// make sure match a bitfield, not a C++ namespace
			if (colonIndex <= 0 || type.charAt(colonIndex - 1) == ':') {
				// regular offset field
				fields.put(String.format("_%sOffset_", fieldName), fieldOffset);
			} else {
				// bitfield
				int bitSize = Integer.parseInt(type.substring(colonIndex + 1).trim());

				/*
				 * Newer blobs have accurate offsets of bitfields; adjust bitFieldBitCount
				 * to account for any fields preceding this field. In older blobs the byte
				 * offset of a bitfield is always zero, so this has no effect.
				 */
				bitFieldBitCount = Math.max(bitFieldBitCount, fieldOffset * Byte.SIZE);

				int cellSize = getCellSize(type.substring(0, colonIndex));

				if (bitSize > (cellSize - (bitFieldBitCount % cellSize))) {
					throw new InternalError(
							String.format("Bitfield %s->%s must not span cells", structure.getName(), fieldName));
				}

				// 's' field
				fields.put(String.format("_%s_s_", fieldName), bitFieldBitCount);

				// 'b' field
				fields.put(String.format("_%s_b_", fieldName), bitSize);

				bitFieldBitCount += bitSize;
			}
		}

		return fields;
	}

}
