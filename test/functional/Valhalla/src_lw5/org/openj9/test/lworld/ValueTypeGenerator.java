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

public class ValueTypeGenerator extends ClassLoader {
	private static ValueTypeGenerator generator;

	static {
		generator = new ValueTypeGenerator();
	}

	private static class ClassConfiguration {
		private String name;
		private String[] fields;
		private boolean isReference;
		private boolean hasNonStaticSynchronizedMethods;

		public ClassConfiguration(String name) {
			this.name = name;
		}

		public ClassConfiguration(String name, String[] fields) {
			this.name = name;
			this.fields = fields;
		}

		public String getName() {
			return name;
		}

		public String[] getFields() {
			return fields;
		}

		public void setIsReference(boolean isReference) {
			this.isReference = isReference;
			/* Only reference type can have non-static synchronized methods. Value type cannot have one. */
			setHasNonStaticSynchronizedMethods(isReference);
		}

		public boolean isReference() {
			return isReference;
		}

		public void setHasNonStaticSynchronizedMethods(boolean hasNonStaticSynchronizedMethods) {
			this.hasNonStaticSynchronizedMethods = hasNonStaticSynchronizedMethods;
		}

		public boolean hasNonStaticSynchronizedMethods() {
			return hasNonStaticSynchronizedMethods;
		}
	}

	public static Class<?> generateRefClass(String name) throws Throwable {
		ClassConfiguration classConfig = new ClassConfiguration(name);
		classConfig.setIsReference(true);

		byte[] bytes = generateClass(classConfig);
		return generator.defineClass(name, bytes, 0, bytes.length);
	}

	public static Class<?> generateValueClass(String name) throws Throwable {
		ClassConfiguration classConfig = new ClassConfiguration(name);

		byte[] bytes = generateClass(classConfig);
		return generator.defineClass(name, bytes, 0, bytes.length);
	}

	public static Class<?> generateIllegalValueClassWithSychMethods(String name) throws Throwable {
		ClassConfiguration classConfig = new ClassConfiguration(name);
		classConfig.setHasNonStaticSynchronizedMethods(true);
		byte[] bytes = generateClass(classConfig);
		return generator.defineClass(name, bytes, 0, bytes.length);
	}

	private static byte[] generateClass(ClassConfiguration config) {
		String className = config.getName();
		String[] fields = config.getFields();
		boolean isRef = config.isReference();
		boolean addSyncMethods = config.hasNonStaticSynchronizedMethods();

		ClassWriter cw = new ClassWriter(0);

		int classFlags = ACC_PUBLIC + ACC_FINAL + (isRef? ValhallaUtils.ACC_IDENTITY : ValhallaUtils.ACC_VALUE_TYPE);
		cw.visit(ValhallaUtils.CLASS_FILE_MAJOR_VERSION, classFlags, className, null, "java/lang/Object", null);

		if (null != fields) {
			for (String s : fields) {
				String nameAndSigValue[] = s.split(":");
				final int fieldModifiers = ACC_PUBLIC;
				FieldVisitor fv = cw.visitField(fieldModifiers, nameAndSigValue[0], nameAndSigValue[1], null, null);
				fv.visitEnd();
			}
		}

		if (isRef) {
			addInit(cw);
			addMakeRef(cw, className);
			addTestMonitorEnterAndExitWithRefType(cw);
			addTestMonitorExitOnObject(cw);
		}
		addStaticSynchronizedMethods(cw);
		if (addSyncMethods) {
			addSynchronizedMethods(cw);
		}

		return cw.toByteArray();
	}

	private static void addInit(ClassWriter cw) {
		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
		mv.visitCode();
		mv.visitVarInsn(ALOAD, 0);
		mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
		mv.visitInsn(RETURN);
		mv.visitMaxs(1, 1);
		mv.visitEnd();
	}

	private static void addMakeRef(ClassWriter cw, String className) {
		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC  + ACC_STATIC, "makeRef", "()" + "L" + className + ";", null, null);
		mv.visitCode();
		mv.visitTypeInsn(NEW, className);
		mv.visitInsn(DUP);
		mv.visitMethodInsn(INVOKESPECIAL, className, "<init>", "()V");
		mv.visitVarInsn(ASTORE, 0);
		mv.visitVarInsn(ALOAD, 0);
		mv.visitInsn(ARETURN);
		mv.visitMaxs(2, 1);
		mv.visitEnd();
	}

	/*
	* This function should only be called in the
	* TestMonitorEnterAndExitWithRefType test
	*/
	private static void addTestMonitorEnterAndExitWithRefType(ClassWriter cw) {
		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "testMonitorEnterAndExitWithRefType", "(Ljava/lang/Object;)V", null, null);
		mv.visitCode();
		mv.visitVarInsn(ALOAD, 0);
		mv.visitInsn(DUP);
		mv.visitInsn(MONITORENTER);
		mv.visitInsn(MONITOREXIT);
		mv.visitInsn(RETURN);
		mv.visitMaxs(2,1);
		mv.visitEnd();
	}

	private static void addStaticSynchronizedMethods(ClassWriter cw) {
		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC + ACC_SYNCHRONIZED, "staticSynchronizedMethodReturnInt", "()I", null, null);
		mv.visitCode();
		mv.visitInsn(ICONST_1);
		mv.visitInsn(IRETURN);
		mv.visitMaxs(1, 0);
		mv.visitEnd();
	}

	private static void addSynchronizedMethods(ClassWriter cw) {
		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC + ACC_SYNCHRONIZED, "synchronizedMethodReturnInt", "()I", null, null);
		mv.visitCode();
		mv.visitInsn(ICONST_1);
		mv.visitInsn(IRETURN);
		mv.visitMaxs(1, 1);
		mv.visitEnd();
	}

	 /*
	  * This function should only be called in the
	  * TestMonitorExitOnValueType test and
	  * TestMonitorExitWithRefType test
	  */
	  private static void addTestMonitorExitOnObject(ClassWriter cw) {
		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "testMonitorExitOnObject", "(Ljava/lang/Object;)V", null, null);
		mv.visitCode();
		mv.visitVarInsn(ALOAD, 0);
		mv.visitInsn(MONITOREXIT);
		mv.visitInsn(RETURN);
		mv.visitMaxs(1, 1);
		mv.visitEnd();
	}
}
