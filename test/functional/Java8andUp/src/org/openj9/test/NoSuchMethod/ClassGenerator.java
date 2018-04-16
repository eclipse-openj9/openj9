/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
package org.openj9.test.NoSuchMethod;

import java.lang.invoke.MethodType;

import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.Opcodes;

public class ClassGenerator implements Opcodes {
	ClassWriter cw;

	public ClassGenerator(String name) {
		this(name, "java/lang/Object", null);
	}

	public ClassGenerator(String name, String superName, String[] interfaces) {
		cw = new ClassWriter(ClassWriter.COMPUTE_MAXS);
		cw.visit(52, ACC_PUBLIC + ACC_SUPER, name, null, superName, interfaces);

		//generate constructor
		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
		mv.visitCode();
		mv.visitVarInsn(ALOAD, 0);
		mv.visitMethodInsn(INVOKESPECIAL, superName, "<init>", "()V", false);
		mv.visitInsn(RETURN);
		mv.visitMaxs(0, 0);
		mv.visitEnd();
	}

	public void addMethod(String name, MethodType type) {
		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC, name, type.toMethodDescriptorString(), null, null);
		mv.visitCode();
		Class<?> retType = type.returnType();

		if (retType.isPrimitive()) {
			if (retType.equals(float.class)) {
				mv.visitInsn(FCONST_0);
				mv.visitInsn(FRETURN);
			} else if (retType.equals(double.class)) {
				mv.visitInsn(DCONST_0);
				mv.visitInsn(DRETURN);
			} else if (retType.equals(long.class)) {
				mv.visitInsn(LCONST_0);
				mv.visitInsn(LRETURN);
			} else if (retType.equals(void.class)) {
				mv.visitInsn(RETURN);
			} else {
				mv.visitInsn(ICONST_0);
				mv.visitInsn(IRETURN);
			}
		} else {
			mv.visitInsn(ACONST_NULL);
			mv.visitInsn(ARETURN);
		}

		mv.visitMaxs(0, 0);
		mv.visitEnd();
	}

	private void pushParam(MethodVisitor mv, Class<?> type) {
		if (type.isPrimitive()) {
			if (type.equals(float.class)) {
				mv.visitInsn(FCONST_0);
			} else if (type.equals(double.class)) {
				mv.visitInsn(DCONST_0);
			} else if (type.equals(long.class)) {
				mv.visitInsn(LCONST_0);
			} else if (type.equals(void.class)) {
			} else {
				mv.visitInsn(ICONST_0);
			}
		} else {
			mv.visitInsn(ACONST_NULL);
		}
	}

	private void pushFuncArguments(MethodVisitor mv, MethodType calleeType) {
		for (Class<?> paramType : calleeType.parameterArray()) {
			pushParam(mv, paramType);
		}

	}

	private void newObject(MethodVisitor mv, Class<?> type) {
		if (type.isPrimitive()) {
			//If we are dealing with a primitive, just create a 0 value
			pushParam(mv, type);
			return;
		}
		if (type.isArray()) {
			throw new RuntimeException("arrays unsupported");
		}

		String typeName = type.getName().replace('.', '/');
		mv.visitTypeInsn(NEW, typeName);
	}

	//creates a new object /w type 'type' calls constructor
	// and leaves pointer on top of stack
	// note expects name of format "com/pkg/SomeClass"
	private void newObject(MethodVisitor mv, String typeName) {
		mv.visitTypeInsn(NEW, typeName);
		mv.visitInsn(DUP);
		//TODO what is last bool arg?
		mv.visitMethodInsn(INVOKESPECIAL, typeName, "<init>", "()V", false);
	}

	public void addCaller(String name, String calledClass, String methodName, MethodType calleeType) {
		MethodType callerType = MethodType.methodType(void.class);
		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC, name, callerType.toMethodDescriptorString(), null, null);

		String calleeName = calledClass.replace('.', '/');
		//TODO should handle  statics
		mv.visitCode();

		newObject(mv, calleeName);

		pushFuncArguments(mv, calleeType);
		//TODO what is last bool arg?
		mv.visitMethodInsn(INVOKEVIRTUAL, calleeName, methodName, calleeType.toMethodDescriptorString(), false);
		mv.visitInsn(RETURN);
		mv.visitMaxs(0, 0);
		mv.visitEnd();
	}

	/**
	 * Dumps the class into bytecode representation
	 * @return array of bytes containing the bytecode for the class
	 */
	public byte[] dump() {
		cw.visitEnd();
		return cw.toByteArray();
	}
}
