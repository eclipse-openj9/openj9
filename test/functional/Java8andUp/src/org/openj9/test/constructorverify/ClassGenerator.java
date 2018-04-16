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
package org.openj9.test.constructorverify;

import java.util.*;
import org.objectweb.asm.*;

public class ClassGenerator implements Opcodes {

	public static byte[] dumpFlagThisUninitInTry() throws Exception {
		ClassWriter cw = new ClassWriter(0);
		FieldVisitor fv;
		MethodVisitor mv;
		AnnotationVisitor av0;

		cw.visit(V1_7, ACC_PUBLIC, "FlagThisUninitInTry", null, "java/lang/Object", null);

		{
			mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
			mv.visitCode();
			Label l0 = new Label();
			Label l1 = new Label();
			mv.visitTryCatchBlock(l0, l1, l1, "java/lang/Throwable");
			mv.visitLabel(l0);
			mv.visitInsn(ACONST_NULL);
			mv.visitInsn(ATHROW);
			mv.visitLabel(l1);
			mv.visitFrame(Opcodes.F_FULL, 0, new Object[] {}, 1, new Object[] {"java/lang/Throwable"});
			mv.visitInsn(RETURN);
			mv.visitMaxs(1, 1);
			mv.visitEnd();
		}
		cw.visitEnd();

		return cw.toByteArray();
	}
	public static byte[] dumpOnlyInvokespecialInTry() throws Exception {

		ClassWriter cw = new ClassWriter(0);
		FieldVisitor fv;
		MethodVisitor mv;
		AnnotationVisitor av0;

		cw.visit(V1_7, ACC_PUBLIC + ACC_SUPER, "OnlyInvokespecialInTry", null, "java/lang/ClassLoader", null);

		{
			mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
			mv.visitCode();
			Label l0 = new Label();
			Label l1 = new Label();
			Label l2 = new Label();
			mv.visitTryCatchBlock(l0, l1, l2, "java/lang/NoClassDefFoundError");
			Label l3 = new Label();
			mv.visitTryCatchBlock(l0, l1, l3, "java/lang/Exception");
			mv.visitVarInsn(ALOAD, 0);
			mv.visitLabel(l0);
			mv.visitMethodInsn(INVOKESPECIAL, "java/lang/ClassLoader", "<init>", "()V", false);
			mv.visitLabel(l1);
			mv.visitMethodInsn(INVOKESTATIC, "OnlyInvokespecialInTry", "xxxx", "()V", false);
			Label l4 = new Label();
			mv.visitJumpInsn(GOTO, l4);
			mv.visitLabel(l2);
			mv.visitFrame(Opcodes.F_FULL, 0, new Object[] {}, 1, new Object[] {"java/lang/NoClassDefFoundError"});
			mv.visitVarInsn(ASTORE, 1);
			mv.visitVarInsn(ALOAD, 1);
			mv.visitInsn(ATHROW);
			mv.visitLabel(l3);
			mv.visitFrame(Opcodes.F_SAME1, 0, null, 1, new Object[] {"java/lang/Exception"});
			mv.visitVarInsn(ASTORE, 1);
			mv.visitVarInsn(ALOAD, 1);
			mv.visitInsn(ATHROW);
			mv.visitLabel(l4);
			mv.visitFrame(Opcodes.F_SAME, 0, null, 0, null);
			mv.visitInsn(RETURN);
			mv.visitMaxs(2, 2);
			mv.visitEnd();
		}
		{
			mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "xxxx", "()V", null, null);
			mv.visitCode();
			mv.visitInsn(RETURN);
			mv.visitMaxs(0, 0);
			mv.visitEnd();
		}
		{
			mv = cw.visitMethod(ACC_PUBLIC, "checkWorking", "()V", null, null);
			mv.visitCode();
			mv.visitFieldInsn(GETSTATIC, "java/lang/System", "out", "Ljava/io/PrintStream;");
			mv.visitLdcInsn("Hello World!");
			mv.visitMethodInsn(INVOKEVIRTUAL, "java/io/PrintStream", "println", "(Ljava/lang/String;)V", false);
			mv.visitInsn(RETURN);
			mv.visitMaxs(2, 1);
			mv.visitEnd();
		}
		{
			mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "main", "([Ljava/lang/String;)V", null, new String[] { "java/lang/Exception" });
			mv.visitCode();
			mv.visitTypeInsn(NEW, "OnlyInvokespecialInTry");
			mv.visitInsn(DUP);
			mv.visitMethodInsn(INVOKESPECIAL, "OnlyInvokespecialInTry", "<init>", "()V", false);
			mv.visitVarInsn(ASTORE, 1);
			mv.visitVarInsn(ALOAD, 1);
			mv.visitMethodInsn(INVOKEVIRTUAL, "OnlyInvokespecialInTry", "checkWorking", "()V", false);
			mv.visitInsn(RETURN);
			mv.visitMaxs(2, 2);
			mv.visitEnd();
		}
		cw.visitEnd();

		return cw.toByteArray();
	}
	
	
	public static byte[] dumpTryAfterInvokespecial() throws Exception {

		ClassWriter cw = new ClassWriter(0);
		FieldVisitor fv;
		MethodVisitor mv;
		AnnotationVisitor av0;

		cw.visit(V1_7, ACC_PUBLIC + ACC_SUPER, "TryAfterInvokespecial", null, "java/lang/ClassLoader", null);

		{
			mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
			mv.visitCode();
			Label l0 = new Label();
			Label l1 = new Label();
			Label l2 = new Label();
			mv.visitTryCatchBlock(l0, l1, l2, "java/lang/NoClassDefFoundError");
			Label l3 = new Label();
			mv.visitTryCatchBlock(l0, l1, l3, "java/lang/NullPointerException");
			Label l4 = new Label();
			mv.visitTryCatchBlock(l0, l1, l4, "java/lang/Exception");
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKESPECIAL, "java/lang/ClassLoader", "<init>", "()V", false);
			mv.visitLabel(l0);
			mv.visitMethodInsn(INVOKESTATIC, "TryAfterInvokespecial", "xxxx", "()V", false);
			mv.visitLabel(l1);
			Label l5 = new Label();
			mv.visitJumpInsn(GOTO, l5);
			mv.visitLabel(l2);
			mv.visitFrame(Opcodes.F_FULL, 0, new Object[] {}, 1, new Object[] {"java/lang/NoClassDefFoundError"});
			mv.visitVarInsn(ASTORE, 1);
			mv.visitVarInsn(ALOAD, 1);
			mv.visitInsn(ATHROW);
			mv.visitLabel(l3);
			mv.visitFrame(Opcodes.F_SAME1, 0, null, 1, new Object[] {"java/lang/NullPointerException"});
			mv.visitVarInsn(ASTORE, 1);
			mv.visitFieldInsn(GETSTATIC, "java/lang/System", "out", "Ljava/io/PrintStream;");
			mv.visitLdcInsn("Should not be here");
			mv.visitMethodInsn(INVOKEVIRTUAL, "java/io/PrintStream", "println", "(Ljava/lang/String;)V", false);
			mv.visitJumpInsn(GOTO, l5);
			mv.visitLabel(l4);
			mv.visitFrame(Opcodes.F_SAME1, 0, null, 1, new Object[] {"java/lang/Exception"});
			mv.visitVarInsn(ASTORE, 1);
			mv.visitVarInsn(ALOAD, 1);
			mv.visitInsn(ATHROW);
			mv.visitLabel(l5);
			mv.visitFrame(Opcodes.F_SAME, 0, null, 0, null);
			mv.visitInsn(RETURN);
			mv.visitMaxs(2, 2);
			mv.visitEnd();
		}
		{
			mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "xxxx", "()V", null, null);
			mv.visitCode();
			mv.visitInsn(RETURN);
			mv.visitMaxs(0, 0);
			mv.visitEnd();
		}
		{
			mv = cw.visitMethod(ACC_PUBLIC, "checkWorking", "()V", null, null);
			mv.visitCode();
			mv.visitFieldInsn(GETSTATIC, "java/lang/System", "out", "Ljava/io/PrintStream;");
			mv.visitLdcInsn("Hello World!");
			mv.visitMethodInsn(INVOKEVIRTUAL, "java/io/PrintStream", "println", "(Ljava/lang/String;)V", false);
			mv.visitInsn(RETURN);
			mv.visitMaxs(2, 1);
			mv.visitEnd();
		}
		{
			mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "main", "([Ljava/lang/String;)V", null, new String[] { "java/lang/Exception" });
			mv.visitCode();
			mv.visitTypeInsn(NEW, "TryAfterInvokespecial");
			mv.visitInsn(DUP);
			mv.visitMethodInsn(INVOKESPECIAL, "TryAfterInvokespecial", "<init>", "()V", false);
			mv.visitVarInsn(ASTORE, 1);
			mv.visitVarInsn(ALOAD, 1);
			mv.visitMethodInsn(INVOKEVIRTUAL, "TryAfterInvokespecial", "checkWorking", "()V", false);
			mv.visitInsn(RETURN);
			mv.visitMaxs(2, 2);
			mv.visitEnd();
		}
		cw.visitEnd();

		return cw.toByteArray();
	}
}
