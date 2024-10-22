/*
 * Copyright IBM Corp. and others 2023
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
package org.openj9.test.lworld;

import org.objectweb.asm.*;

import static org.objectweb.asm.Opcodes.*;

public class ValhallaAttributeGenerator extends ClassLoader {
	private static ValhallaAttributeGenerator generator = new ValhallaAttributeGenerator();

	public static Class<?> generateClassWithTwoLoadableDescriptorsAttributes(String name, String[] classList1, String[] classList2) throws Throwable {
		byte[] bytes = generateClass(name, ValhallaUtils.ACC_IDENTITY, new Attribute[] {
			new ValhallaUtils.LoadableDescriptorsAttribute(classList1),
			new ValhallaUtils.LoadableDescriptorsAttribute(classList2)});
		return generator.defineClass(name, bytes, 0, bytes.length);
	}

	public static Class<?> generateClassWithLoadableDescriptorsAttribute(String name, String[] classList) throws Throwable {
		byte[] bytes = generateClass(name, ValhallaUtils.ACC_IDENTITY, new Attribute[] {new ValhallaUtils.LoadableDescriptorsAttribute(classList)});
		return generator.defineClass(name, bytes, 0, bytes.length);
	}

	public static Class<?> generateClassWithTwoImplicitCreationAttributes(String name) throws Throwable {
		byte[] bytes = generateClass(name, ACC_FINAL,
			new Attribute[] {new ValhallaUtils.ImplicitCreationAttribute(), new ValhallaUtils.ImplicitCreationAttribute()});
		return generator.defineClass(name, bytes, 0, bytes.length);
	}

	public static Class<?> generateNonValueTypeClassWithImplicitCreationAttribute(String name) throws Throwable {
		byte[] bytes = generateClass(name, ValhallaUtils.ACC_IDENTITY, new Attribute[] {new ValhallaUtils.ImplicitCreationAttribute()});
		return generator.defineClass(name, bytes, 0, bytes.length);
	}

	public static Class<?> generateFieldWithMultipleNullRestrictedAttributes(String className, String fieldClassName) throws Throwable {
		/* Generate field class - value class with ImplicitCreation attribute and ACC_DEFAULT flag set  */
		byte[] fieldClassBytes = generateClass(fieldClassName, ACC_FINAL,
			new Attribute[] {new ValhallaUtils.ImplicitCreationAttribute()});
		Class<?> fieldClass = generator.defineClass(fieldClassName, fieldClassBytes, 0, fieldClassBytes.length);

		/* Generate class with field and multiple NullRestricted attributes */
		byte[] classBytes = generateIdentityClassWithField(className, 0,
			"field", fieldClass.descriptorString(), new Attribute[]{new ValhallaUtils.NullRestrictedAttribute(), new ValhallaUtils.NullRestrictedAttribute()});
		return generator.defineClass(className, classBytes, 0, classBytes.length);
	}

	public static Class<?> generateNullRestrictedAttributeInPrimitiveField(String className) throws Throwable {
		/* Generate class with primitive field and a NullRestricted attribute */
		byte[] classBytes = generateIdentityClassWithField(className, 0,
			"field", "I", new Attribute[]{new ValhallaUtils.NullRestrictedAttribute()});
		return generator.defineClass(className, classBytes, 0, classBytes.length);
	}

	public static Class<?> generateNullRestrictedAttributeInArrayField(String className, String fieldClassName) throws Throwable {
		/* Generate field class - value class with ImplicitCreation attribute and ACC_DEFAULT flag set.  */
		byte[] fieldClassBytes = generateClass(fieldClassName, ACC_FINAL,
			new Attribute[] {new ValhallaUtils.ImplicitCreationAttribute()});
		Class<?> fieldClass = generator.defineClass(fieldClassName, fieldClassBytes, 0, fieldClassBytes.length);

		/* Generate class field field that is an array with a NullRestricted attribute */
		byte[] classBytes = generateIdentityClassWithField(className, 0,
			"field", "[" + fieldClass.descriptorString(), new Attribute[]{new ValhallaUtils.NullRestrictedAttribute()});
		return generator.defineClass(className, classBytes, 0, classBytes.length);
	}

	public static Class<?> generatePutStaticNullToNullRestrictedField(String className, String fieldClassName) {
		String fieldName = "field";

		/* Generate field class - value class with ImplicitCreation attribute and ACC_DEFAULT flag set.  */
		byte[] fieldClassBytes = generateClass(fieldClassName, ACC_PUBLIC + ACC_FINAL,
			new Attribute[] {new ValhallaUtils.ImplicitCreationAttribute()});
		Class<?> fieldClass = generator.defineClass(fieldClassName, fieldClassBytes, 0, fieldClassBytes.length);

		ClassWriter classWriter = new ClassWriter(0);
		classWriter.visit(ValhallaUtils.VALUE_TYPE_CLASS_FILE_VERSION, ACC_PUBLIC + ValhallaUtils.ACC_IDENTITY, className, null, "java/lang/Object", null);

		/* static field of previously generated field class with NullRestrictd attribute */
		FieldVisitor fieldVisitor = classWriter.visitField(ACC_PUBLIC + ACC_STATIC, fieldName, fieldClass.descriptorString(), null, null);
		fieldVisitor.visitAttribute(new ValhallaUtils.NullRestrictedAttribute());

		/* assign field to null in <clinit> */
		MethodVisitor mvClinit = classWriter.visitMethod(ACC_STATIC, "<clinit>", "()V", null, null);
		mvClinit.visitCode();
		mvClinit.visitInsn(ACONST_NULL);
		mvClinit.visitFieldInsn(PUTSTATIC, className, fieldName, fieldClass.descriptorString());
		mvClinit.visitInsn(RETURN);
		mvClinit.visitMaxs(1, 0);
		mvClinit.visitEnd();

		/* <init> */
		MethodVisitor mvInit = classWriter.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
		mvInit.visitCode();
		mvInit.visitVarInsn(ALOAD, 0);
		mvInit.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V");
		mvInit.visitInsn(RETURN);
		mvInit.visitMaxs(1, 1);
		mvInit.visitEnd();

		classWriter.visitEnd();
		byte[] classBytes = classWriter.toByteArray();
		return generator.defineClass(className, classBytes, 0, classBytes.length);
	}

	public static Class<?> generatePutFieldNullToField(String className, String fieldClassName, boolean nullRestrictedField) {
		String fieldName = "field";

		/* Generate field class - value class with ImplicitCreation attribute and ACC_DEFAULT flag set.  */
		byte[] fieldClassBytes = generateClass(fieldClassName, ACC_PUBLIC + ACC_FINAL,
			new Attribute[] {new ValhallaUtils.ImplicitCreationAttribute()});
		Class<?> fieldClass = generator.defineClass(fieldClassName, fieldClassBytes, 0, fieldClassBytes.length);

		ClassWriter classWriter = new ClassWriter(0);
		classWriter.visit(ValhallaUtils.VALUE_TYPE_CLASS_FILE_VERSION, ACC_PUBLIC + ValhallaUtils.ACC_IDENTITY, className, null, "java/lang/Object", null);

		/* instance field of previously generated field class with NullRestrictd attribute */
		FieldVisitor fieldVisitor = classWriter.visitField(ACC_PUBLIC, fieldName, fieldClass.descriptorString(), null, null);
		if (nullRestrictedField) {
			fieldVisitor.visitAttribute(new ValhallaUtils.NullRestrictedAttribute());
		}

		/* assign field to null in <init> */
		MethodVisitor mvInit = classWriter.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
		mvInit.visitCode();
		mvInit.visitVarInsn(ALOAD, 0);
		mvInit.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V");
		mvInit.visitVarInsn(ALOAD, 0);
		mvInit.visitInsn(ACONST_NULL);
		mvInit.visitFieldInsn(PUTFIELD, className, fieldName, fieldClass.descriptorString());
		mvInit.visitInsn(RETURN);
		mvInit.visitMaxs(2, 1);
		mvInit.visitEnd();

		classWriter.visitEnd();
		byte[] classBytes = classWriter.toByteArray();
		return generator.defineClass(className, classBytes, 0, classBytes.length);
	}

	public static Class<?> generateNullRestrictedAttributeInIdentityClass(boolean isStatic, String className, String fieldClassName) throws Throwable {
		/* Generate field class - identity class */
		byte[] fieldClassBytes = generateClass(fieldClassName, ValhallaUtils.ACC_IDENTITY, null);
		Class<?> fieldClass = generator.defineClass(fieldClassName, fieldClassBytes, 0, fieldClassBytes.length);

		/* Generate class with field that is an identity class with a NullRestricted attribute */
		byte[] classBytes = generateIdentityClassWithField(className, isStatic ? ACC_STATIC : 0,
			"field", fieldClass.descriptorString(), new Attribute[]{new ValhallaUtils.NullRestrictedAttribute()});
		return generator.defineClass(className, classBytes, 0, classBytes.length);
	}

	public static Class<?> generateNullRestrictedAttributeInValueClassWithoutIC(boolean isStatic, String className, String fieldClassName) throws Throwable {
		/* Generate field class - value class with no attributes */
		byte[] fieldClassBytes = generateClass(fieldClassName, ACC_FINAL, null);
		Class<?> fieldClass = generator.defineClass(fieldClassName, fieldClassBytes, 0, fieldClassBytes.length);

		/* Generate class with field that has a NullRestricted attribute */
		byte[] classBytes = generateIdentityClassWithField(className, isStatic ? ACC_STATIC : 0,
			"field", fieldClass.descriptorString(), new Attribute[]{new ValhallaUtils.NullRestrictedAttribute()});
		return generator.defineClass(className, classBytes, 0, classBytes.length);
	}

	public static Class<?> generateNullRestrictedFieldWhereICHasNoDefaultFlag(boolean isStatic, String className, String fieldClassName) throws Throwable {
		/* Generate field class - value type with ImplicitCreation attribute, no flags */
		byte[] fieldClassBytes = generateClass(fieldClassName, ACC_FINAL,
			new Attribute[] {new ValhallaUtils.ImplicitCreationAttribute(0)});
		Class<?> fieldClass = generator.defineClass(fieldClassName, fieldClassBytes, 0, fieldClassBytes.length);

		/* Generate class with field with NullRestricted attribute */
		byte[] classBytes = generateIdentityClassWithField(className, isStatic ? ACC_STATIC : 0,
			"field", fieldClass.descriptorString(), new Attribute[]{new ValhallaUtils.NullRestrictedAttribute()});
		return generator.defineClass(className, classBytes, 0, classBytes.length);
	}

	public static Class<?> findLoadedTestClass(String name) {
		return generator.findLoadedClass(name);
	}

	public static byte[] generateClass(String name, int classFlags, Attribute[] attributes) {
		ClassWriter classWriter = new ClassWriter(0);
		classWriter.visit(ValhallaUtils.VALUE_TYPE_CLASS_FILE_VERSION, classFlags, name, null, "java/lang/Object", null);

		/* Generate passed in attributes if there are any */
		if (null != attributes) {
			for (Attribute attr : attributes) {
				classWriter.visitAttribute(attr);
			}
		}

		classWriter.visitEnd();
		return classWriter.toByteArray();
	}

	public static byte[] generateIdentityClassWithField(String name, int fieldFlags, String fieldName, String fieldDescriptor, Attribute[] fieldAttributes) {
		ClassWriter classWriter = new ClassWriter(0);
		classWriter.visit(ValhallaUtils.VALUE_TYPE_CLASS_FILE_VERSION, ValhallaUtils.ACC_IDENTITY, name, null, "java/lang/Object", null);

		FieldVisitor fieldVisitor = classWriter.visitField(fieldFlags, fieldName, fieldDescriptor, null, null);
		if (null != fieldAttributes) {
			for (Attribute attr : fieldAttributes) {
				fieldVisitor.visitAttribute(attr);
			}
		}

		/* <init> */
		MethodVisitor mvInit = classWriter.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
		mvInit.visitCode();
		mvInit.visitVarInsn(ALOAD, 0);
		mvInit.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V");
		mvInit.visitInsn(RETURN);
		mvInit.visitMaxs(1, 1);
		mvInit.visitEnd();

		classWriter.visitEnd();
		return classWriter.toByteArray();
	}
}
