/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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
package com.ibm.j9.jsr292;

import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.FieldVisitor;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.Label;
import static org.objectweb.asm.Opcodes.*;

public class ClassBytes {

	public static byte[] classBytes_DifferentPackage() throws Exception {
		ClassWriter cw = new ClassWriter(0);
		MethodVisitor mv;
		cw.visit(V1_2, ACC_PUBLIC + ACC_SUPER, "com/ibm/j9/jsr292/SamePackageExample1", null, "java/lang/Object", null);

		cw.visitSource("SamePackageExample1.java", null);

		{
		mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
		mv.visitCode();
		Label l0 = new Label();
		mv.visitLabel(l0);
		mv.visitLineNumber(3, l0);
		mv.visitVarInsn(ALOAD, 0);
		mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V");
		mv.visitInsn(RETURN);
		Label l1 = new Label();
		mv.visitLabel(l1);
		mv.visitLocalVariable("this", "Lcom/ibm/j9/jsr292/SamePackageExample1;", null, l0, l1, 0);
		mv.visitMaxs(1, 1);
		mv.visitEnd();
		}
		cw.visitEnd();

		return cw.toByteArray();
	}
	
	public static byte[] invalidClassBytes_SamePackage() throws Exception {

		ClassWriter cw = new ClassWriter(0);
		FieldVisitor fv;
		MethodVisitor mv;

		cw.visit(V1_2, ACC_PUBLIC + ACC_SUPER, "com/ibm/j9/jsr292/api/SamePackageExample2", null, "java/lang/Object", null);

		cw.visitSource("SamePackageExample2.java", null);

		{
		mv = cw.visitMethod(ACC_PUBLIC, "<Corrupted>", "()V", null, null);
		mv.visitCode();
		Label l0 = new Label();
		mv.visitLabel(l0);
		mv.visitLineNumber(3, l0);
		mv.visitVarInsn(ALOAD, 0);
		mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V");
		mv.visitInsn(RETURN);
		Label l1 = new Label();
		mv.visitLabel(l1);
		mv.visitLocalVariable("this", "Lcom/ibm/j9/jsr292/api/SamePackageExample2;", null, l0, l1, 0);
		mv.visitMaxs(1, 1);
		mv.visitEnd();
		}
		cw.visitEnd();

		return cw.toByteArray();
		}
	
	public static byte[] validClassBytes_SamePackage() throws Exception {

		ClassWriter cw = new ClassWriter(0);
		FieldVisitor fv;
		MethodVisitor mv;

		cw.visit(V1_2, ACC_PUBLIC + ACC_SUPER, "com/ibm/j9/jsr292/api/SamePackageExample3", null, "java/lang/Object", null);

		cw.visitSource("SamePackageExample3.java", null);

		{
		fv = cw.visitField(ACC_STATIC + ACC_SYNTHETIC, "class$0", "Ljava/lang/Class;", null, null);
		fv.visitEnd();
		}
		{
		mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
		mv.visitCode();
		Label l0 = new Label();
		mv.visitLabel(l0);
		mv.visitLineNumber(3, l0);
		mv.visitVarInsn(ALOAD, 0);
		mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V");
		mv.visitInsn(RETURN);
		Label l1 = new Label();
		mv.visitLabel(l1);
		mv.visitLocalVariable("this", "Lcom/ibm/j9/jsr292/api/SamePackageExample3;", null, l0, l1, 0);
		mv.visitMaxs(1, 1);
		mv.visitEnd();
		}
		{
		mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "getClassName", "()Ljava/lang/String;", null, null);
		mv.visitCode();
		Label l0 = new Label();
		Label l1 = new Label();
		Label l2 = new Label();
		mv.visitTryCatchBlock(l0, l1, l2, "java/lang/ClassNotFoundException");
		Label l3 = new Label();
		mv.visitLabel(l3);
		mv.visitLineNumber(5, l3);
		mv.visitFieldInsn(GETSTATIC, "com/ibm/j9/jsr292/api/SamePackageExample3", "class$0", "Ljava/lang/Class;");
		mv.visitInsn(DUP);
		Label l4 = new Label();
		mv.visitJumpInsn(IFNONNULL, l4);
		mv.visitInsn(POP);
		mv.visitLabel(l0);
		mv.visitLdcInsn("com.ibm.j9.jsr292.api.SamePackageExample3");
		mv.visitMethodInsn(INVOKESTATIC, "java/lang/Class", "forName", "(Ljava/lang/String;)Ljava/lang/Class;");
		mv.visitLabel(l1);
		mv.visitInsn(DUP);
		mv.visitFieldInsn(PUTSTATIC, "com/ibm/j9/jsr292/api/SamePackageExample3", "class$0", "Ljava/lang/Class;");
		mv.visitJumpInsn(GOTO, l4);
		mv.visitLabel(l2);
		mv.visitTypeInsn(NEW, "java/lang/NoClassDefFoundError");
		mv.visitInsn(DUP_X1);
		mv.visitInsn(SWAP);
		mv.visitMethodInsn(INVOKEVIRTUAL, "java/lang/Throwable", "getMessage", "()Ljava/lang/String;");
		mv.visitMethodInsn(INVOKESPECIAL, "java/lang/NoClassDefFoundError", "<init>", "(Ljava/lang/String;)V");
		mv.visitInsn(ATHROW);
		mv.visitLabel(l4);
		mv.visitMethodInsn(INVOKEVIRTUAL, "java/lang/Class", "getCanonicalName", "()Ljava/lang/String;");
		mv.visitInsn(ARETURN);
		Label l5 = new Label();
		mv.visitLabel(l5);
		mv.visitLocalVariable("this", "Lcom/ibm/j9/jsr292/api/SamePackageExample3;", null, l3, l5, 0);
		mv.visitMaxs(3, 1);
		mv.visitEnd();
		}
		cw.visitEnd();

		return cw.toByteArray();
	}
}
