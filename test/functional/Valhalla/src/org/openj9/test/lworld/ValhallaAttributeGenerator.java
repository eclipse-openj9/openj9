/*******************************************************************************
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
 *******************************************************************************/
package org.openj9.test.lworld;

import org.objectweb.asm.*;

import static org.objectweb.asm.Opcodes.*;

public class ValhallaAttributeGenerator extends ClassLoader {
	private static ValhallaAttributeGenerator generator = new ValhallaAttributeGenerator();

	public static Class<?> generateClassWithTwoPreloadAttributes(String name, String[] classList1, String[] classList2) throws Throwable {
		byte[] bytes = generateClass(name, new Attribute[] {
			new PreloadAttribute(classList1),
			new PreloadAttribute(classList2)});
		return generator.defineClass(name, bytes, 0, bytes.length);
	}

	public static Class<?> generateClassWithPreloadAttribute(String name, String[] classList) throws Throwable {
		byte[] bytes = generateClass(name, new Attribute[] {new PreloadAttribute(classList)});
		return generator.defineClass(name, bytes, 0, bytes.length);
	}

	public static Class<?> findLoadedTestClass(String name) {
		return generator.findLoadedClass(name);
	}

	public static byte[] generateClass(String name, Attribute[] attributes) {
		ClassWriter classWriter = new ClassWriter(0);
		classWriter.visit(ValhallaUtils.CLASS_FILE_MAJOR_VERSION, ACC_PUBLIC | ACC_FINAL, name, null, "java/lang/Object", null);

		/* Generate base constructor which just calls super.<init>() */
		MethodVisitor methodVisitor = classWriter.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
		methodVisitor.visitCode();
		methodVisitor.visitVarInsn(ALOAD, 0);
		methodVisitor.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
		methodVisitor.visitInsn(RETURN);
		methodVisitor.visitMaxs(1, 1);
		methodVisitor.visitEnd();

		/* Generate passed in attributes if there are any */
		if (null != attributes) {
			for (Attribute attr : attributes) {
				classWriter.visitAttribute(attr);
			}
		}

		classWriter.visitEnd();
		return classWriter.toByteArray();
	}

	final static class PreloadAttribute extends Attribute {
		private final String[] classes;

		public PreloadAttribute(String[] classes) {
			super("Preload");
			this.classes = classes;
		}

		public Label[] getLabels() {
			return null;
		}

		public boolean isCodeAttribute() {
			return false;
		}

		public boolean isUnknown() {
			return true;
		}

		public ByteVector write(ClassWriter cw, byte[] code, int len, int maxStack, int maxLocals) {
			ByteVector b = new ByteVector();

			b.putShort(classes.length);

			int cpIndex;
			for (int i = 0; i < classes.length; i++) {
				cpIndex = cw.newClass(classes[i].replace('.', '/'));
				b.putShort(cpIndex);
			}
			return b;
		}
	}

	final static class ImplicitCreationAttribute extends Attribute {
		private final int flags;

		public ImplicitCreationAttribute(int flags) {
			super("ImplicitCreation");
			this.flags = flags;
		}

		public Label[] getLabels() {
			return null;
		}

		public boolean isCodeAttribute() {
			return false;
		}

		public boolean isUnknown() {
			return true;
		}

		public ByteVector write(ClassWriter cw, byte[] code, int len, int maxStack, int maxLocals) {
			ByteVector b = new ByteVector();
			b.putShort(flags);
			return b;
		}
	}

	final static class NullRestrictedAttribute extends Attribute {
		public NullRestrictedAttribute() {
			super("NullRestricted");
		}

		public Label[] getLabels() {
			return null;
		}

		public boolean isCodeAttribute() {
			return false;
		}

		public boolean isUnknown() {
			return true;
		}

		public ByteVector write(ClassWriter cw, byte[] code, int len, int maxStack, int maxLocals) {
			return new ByteVector();
		}
	}
}
