/*******************************************************************************
 * Copyright (c) 2001, 2012 IBM Corp. and others
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

package com.ibm.j9.recreateclass.testclasses;

import static org.objectweb.asm.Opcodes.*;

import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.Label;
import org.objectweb.asm.MethodVisitor;

/**
 * This class generates a class which uses JSR/RET bytecodes to implement finally block.
 * 
 * @author ashutosh
 */
public class JSRRETTestGenerator {
	public static byte[] generateClassData() {
        ClassWriter cw = new ClassWriter(0);
        MethodVisitor mv;

		cw.visit(V1_1, ACC_PUBLIC, "com/ibm/j9/recreateclasscompare/testclasses/JSRRETTest", null, "java/lang/Object", null);
		{
			mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V");
			mv.visitInsn(RETURN);
			mv.visitMaxs(1, 1);
			mv.visitEnd();
		}
		{
			mv = cw.visitMethod(ACC_PUBLIC, "method", "()V", null, null);
			mv.visitCode();
			Label l0 = new Label();
			Label l1 = new Label();
			Label l2 = new Label();
			Label l3 = new Label();
			Label l4 = new Label();
			Label l5 = new Label();
			mv.visitTryCatchBlock(l0, l1, l2, null);
			mv.visitTryCatchBlock(l2, l3, l2, null);
			mv.visitLabel(l0);
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKEVIRTUAL, "JSRRETTest", "foo", "()V");
			mv.visitJumpInsn(JSR, l4);
			mv.visitLabel(l1);
			mv.visitJumpInsn(GOTO, l5);
			mv.visitLabel(l2);
			mv.visitVarInsn(ASTORE, 1);
			mv.visitJumpInsn(JSR, l4);
			mv.visitLabel(l3);
			mv.visitVarInsn(ALOAD, 1);
			mv.visitInsn(ATHROW);
			mv.visitLabel(l4);
			mv.visitVarInsn(ASTORE, 2);
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKEVIRTUAL, "JSRRETTest", "bar", "()V");
			mv.visitVarInsn(RET, 2);
			mv.visitLabel(l5);
			mv.visitInsn(RETURN);
			mv.visitMaxs(2, 3);
			mv.visitEnd();
		}
		{
			mv = cw.visitMethod(ACC_PUBLIC, "foo", "()V", null, null);
			mv.visitCode();
			mv.visitInsn(RETURN);
			mv.visitMaxs(0, 1);
			mv.visitEnd();
		}
		{
			mv = cw.visitMethod(ACC_PUBLIC, "bar", "()V", null, null);
			mv.visitCode();
			mv.visitInsn(RETURN);
			mv.visitMaxs(0, 1);
			mv.visitEnd();
		}
		cw.visitEnd();
        return cw.toByteArray();
	}
}
