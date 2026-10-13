/*
 * Copyright IBM Corp. and others 2026
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

import java.io.FileOutputStream;
import java.io.IOException;

import org.objectweb.asm.*;

import static org.objectweb.asm.Opcodes.*;

public class ValhallaAOTTestClassGenerator extends ClassLoader {
	private static ValhallaAOTTestClassGenerator generator = new ValhallaAOTTestClassGenerator();

	public static byte[] generateClassWithNullRestrictedFields(String className, String fieldClassName, boolean[] nullRestrictedFields) {
		int fieldCount = nullRestrictedFields.length;
			Attribute[] attrs = new Attribute[fieldCount];

		String classNameWithSlashes = className.replace(".", "/");
		String classDescriptor = "L" + classNameWithSlashes + ";";

		String fieldClassNameWithSlashes = fieldClassName.replace(".", "/");
		String fieldClassDescriptor = "L" + fieldClassNameWithSlashes + ";";

		String jloDescriptor = "Ljava/lang/Object;";

		// Generate class of form, with fields annotated as null-restricted as dictated by nullRestrictedFields,
		// where <n> is equal to nullRestrictedFields.length - 1
		//
		//	public class <className>
		//		public Object before;
		//		public <fieldClassName> f0;
		//		public <fieldClassName> f1;
		//		...
		//		public <fieldClassName> f<n>;
		//		public Object after;
		//	}
		//
		ClassWriter classWriter = new ClassWriter(0);
		classWriter.visit(ValhallaUtils.VALUE_TYPE_CLASS_FILE_VERSION, ACC_PUBLIC + ValhallaUtils.ACC_IDENTITY,
			classNameWithSlashes, null, "java/lang/Object", null);

		FieldVisitor beforeVisitor = classWriter.visitField(ACC_PUBLIC, "before", jloDescriptor, null, null);

		for (int i = 0; i < fieldCount; i++) {
			boolean isNullRestrictedField = nullRestrictedFields[i];
			String fieldName = "f" + i;
			int fieldFlags = ACC_PUBLIC + (isNullRestrictedField ? ValhallaUtils.ACC_STRICT_INIT : 0);
			FieldVisitor fieldVisitor = classWriter.visitField(ACC_PUBLIC, fieldName, fieldClassDescriptor, null, null);

			if (isNullRestrictedField) {
				fieldVisitor.visitAttribute(new ValhallaUtils.NullRestrictedAttribute());
			}
		}

		FieldVisitor afterVisitor = classWriter.visitField(ACC_PUBLIC, "after", jloDescriptor, null, null);

		// Generate constructor of form
		//
		//	public <className> {
		//		super();
		//		before = this;
		//		f0 = new <fieldClassName>(0);
		//		f1 = new <fieldClassName>(1);
		//              ...
		//		f<n> = new <fieldClassName>(<n>);
		//		after = this;
		//		return;
		//	}
		MethodVisitor constructorVisitor = classWriter.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
		constructorVisitor.visitCode();

		constructorVisitor.visitVarInsn(ALOAD, 0);
		constructorVisitor.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V");

		constructorVisitor.visitVarInsn(ALOAD, 0);
		constructorVisitor.visitVarInsn(ALOAD, 0);
		constructorVisitor.visitFieldInsn(PUTFIELD, classNameWithSlashes, "before", jloDescriptor);

		for (int i = 0; i < fieldCount; i++) {
			constructorVisitor.visitVarInsn(ALOAD, 0);
			constructorVisitor.visitTypeInsn(NEW, fieldClassNameWithSlashes);
			constructorVisitor.visitInsn(DUP);
			constructorVisitor.visitIntInsn(BIPUSH, i);
			constructorVisitor.visitMethodInsn(INVOKESPECIAL, fieldClassNameWithSlashes, "<init>", "(I)V");
			String fieldName = "f" + i;
			constructorVisitor.visitFieldInsn(PUTFIELD, classNameWithSlashes, "f"+i, fieldClassDescriptor);
		}

		constructorVisitor.visitVarInsn(ALOAD, 0);
		constructorVisitor.visitVarInsn(ALOAD, 0);
		constructorVisitor.visitFieldInsn(PUTFIELD, classNameWithSlashes, "after", jloDescriptor);

		constructorVisitor.visitInsn(RETURN);
		constructorVisitor.visitMaxs(4, 1);
		constructorVisitor.visitEnd();

		classWriter.visitEnd();
		byte[] classBytes = classWriter.toByteArray();

		return classBytes;
	}

	public static final void main(String[] args) throws IOException {
		String containerClassName = args[0];
		String fieldsClassName = args[1];
		String numFieldsArg = args[2];
		String classPathOutputPattern = args[3];

		final int numFields = Integer.parseInt(numFieldsArg);

		final int numClasses = 1 << numFields;

		// Create 2^numFields classes with all combinations of null-restricted and non-null-restricted fields
		for (int classNum = 0; classNum < numClasses; classNum++) {
			boolean[] nullRestrictedFields = new boolean[numFields];
			int mask = 1;

			for (int i = 0; i < numFields; i++) {
				if ((mask & classNum) != 0) {
					nullRestrictedFields[i] = true;
				}
				mask = mask << 1;
			}

			byte[] classBytes = generateClassWithNullRestrictedFields(containerClassName, fieldsClassName, nullRestrictedFields);
			FileOutputStream fos = new FileOutputStream(String.format(classPathOutputPattern, classNum) + "/"
                                                       + containerClassName.replace(".", "/")+".class");
			fos.write(classBytes);
			fos.close();
		}
	}
}
