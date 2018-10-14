/*******************************************************************************
 * Copyright (c) 2015, 2018 IBM Corp. and others
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
package org.openj9.test.jsr335.interfacePrivateMethod;

import static org.objectweb.asm.Opcodes.ACC_FINAL;
import static org.objectweb.asm.Opcodes.ACC_PUBLIC;
import static org.objectweb.asm.Opcodes.ACC_PRIVATE;
import static org.objectweb.asm.Opcodes.ACC_ABSTRACT;
import static org.objectweb.asm.Opcodes.ACC_INTERFACE;
import static org.objectweb.asm.Opcodes.INVOKEINTERFACE;
import static org.objectweb.asm.Opcodes.ACC_STATIC;
import static org.objectweb.asm.Opcodes.ALOAD;
import static org.objectweb.asm.Opcodes.ARETURN;
import static org.objectweb.asm.Opcodes.INVOKESTATIC;
import static org.objectweb.asm.Opcodes.INVOKESPECIAL;
import static org.objectweb.asm.Opcodes.INVOKEVIRTUAL;
import static org.objectweb.asm.Opcodes.V1_8;

import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.MethodVisitor;

/* This class is used to generate bytecodes for the required interfaces from their respective ASM dumps, and return the bytecodes to 
 * the calling classloaders in the form of byte arrays.
 * 
 * The method makeClassFile returns as byte array the bytecodes for interface ITest 
 */
public class GenerateInterfaceFromASM {
	
	public static byte[] makeClassFile ()  {

		ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_MAXS + ClassWriter.COMPUTE_FRAMES);
		MethodVisitor mv;

		cw.visit(V1_8, ACC_ABSTRACT + ACC_INTERFACE, "ITest", null, "java/lang/Object", null);
		{
			mv = cw.visitMethod(ACC_PRIVATE + ACC_STATIC, "private_static_method", "()Ljava/lang/String;", null, null);
			mv.visitCode();
			mv.visitLdcInsn("private_static_method");
			mv.visitInsn(ARETURN);
			mv.visitMaxs(1, 0);
			mv.visitEnd();
		}
		{
			mv = cw.visitMethod(ACC_PRIVATE, "private_non_static_method", "()Ljava/lang/String;", null, null);
			mv.visitCode();
			mv.visitLdcInsn("private_non_static_method");
			mv.visitInsn(ARETURN);
			mv.visitMaxs(1, 1);
			mv.visitEnd();
		}
		{
			mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "RunTest_private_static_method_with_InvokeStatic", "()Ljava/lang/String;", null, null);
			mv.visitCode();
			mv.visitMethodInsn(INVOKESTATIC, "ITest", "private_static_method", "()Ljava/lang/String;", true);
			mv.visitInsn(ARETURN);
			mv.visitMaxs(1, 0);
			mv.visitEnd();
		}
		{
			mv = cw.visitMethod(ACC_PUBLIC, "RunTest_private_non_static_method_with_InvokeStatic", "()Ljava/lang/String;", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKESTATIC, "ITest", "private_non_static_method", "()Ljava/lang/String;", true);
			mv.visitInsn(ARETURN);
			mv.visitMaxs(1, 1);
			mv.visitEnd();
		}
		{
			mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "RunTest_private_static_method_with_InvokeVirtual", "(LITest;)Ljava/lang/String;", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKEVIRTUAL, "ITest", "private_static_method", "()Ljava/lang/String;", false);
			mv.visitInsn(ARETURN);
			mv.visitMaxs(1, 1);
			mv.visitEnd();
		}
		{
			mv = cw.visitMethod(ACC_PUBLIC, "RunTest_private_non_static_method_with_InvokeVirtual", "(LITest;)Ljava/lang/String;", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKEVIRTUAL, "ITest", "private_non_static_method", "()Ljava/lang/String;", false);
			mv.visitInsn(ARETURN);
			mv.visitMaxs(1, 2);
			mv.visitEnd();
		}
		{
			mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "RunTest_private_static_method_with_InvokeSpecial", "(LITest;)Ljava/lang/String;", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKESPECIAL, "ITest", "private_static_method", "()Ljava/lang/String;", true);
			mv.visitInsn(ARETURN);
			mv.visitMaxs(1, 1);
			mv.visitEnd();
		}
		{
			mv = cw.visitMethod(ACC_PUBLIC, "RunTest_private_non_static_method_with_InvokeSpecial", "(LITest;)Ljava/lang/String;", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKESPECIAL, "ITest", "private_non_static_method", "()Ljava/lang/String;", true);
			mv.visitInsn(ARETURN);
			mv.visitMaxs(1, 2);
			mv.visitEnd();
		}
		cw.visitInnerClass("java/lang/invoke/MethodHandles$Lookup", "java/lang/invoke/MethodHandles", "Lookup", ACC_PUBLIC + ACC_FINAL + ACC_STATIC);
		{
			mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "getLookup", "()Ljava/lang/invoke/MethodHandles$Lookup;", null, null);
			mv.visitCode();
			mv.visitMethodInsn(INVOKESTATIC, "java/lang/invoke/MethodHandles", "lookup", "()Ljava/lang/invoke/MethodHandles$Lookup;", false);
			mv.visitInsn(ARETURN);
			mv.visitMaxs(1, 1);
			mv.visitEnd();
		}
		cw.visitEnd();

		return cw.toByteArray();
	}

}
