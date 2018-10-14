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
package org.openj9.test.jsr335;

import org.objectweb.asm.*;

class ASMInterfaceEarlyInit implements Opcodes {
	public static byte[] dumpI() {		
		ClassWriter cw = new ClassWriter(0);
		FieldVisitor fv;
		MethodVisitor mv;
		AnnotationVisitor av0;
		
		cw.visit(52, ACC_PUBLIC + ACC_ABSTRACT + ACC_INTERFACE, "I", null, "java/lang/Object", null);
		
		{
		        mv = cw.visitMethod(ACC_PUBLIC, "foo", "()V", null, null);
		        mv.visitCode();
		        mv.visitInsn(RETURN);
		        mv.visitMaxs(2, 1);
		        mv.visitEnd();
		}
		{
		        mv = cw.visitMethod(ACC_STATIC, "<clinit>", "()V", null, null);
		        mv.visitCode();
		        mv.visitTypeInsn(NEW, "java/lang/InternalError");
		        mv.visitInsn(DUP);
		        mv.visitLdcInsn("interface");
		        mv.visitMethodInsn(INVOKESPECIAL, "java/lang/InternalError", "<init>", "(Ljava/lang/String;)V", false);
		        mv.visitInsn(ATHROW);
		        mv.visitMaxs(3, 0);
		        mv.visitEnd();
		}
		cw.visitEnd();
		
		return cw.toByteArray();
	}

	public static byte[] dumpC() {
		ClassWriter cw = new ClassWriter(0);
		FieldVisitor fv;
		MethodVisitor mv;
		AnnotationVisitor av0;
		
		cw.visit(52, ACC_PUBLIC + ACC_SUPER, "C", null, "java/lang/Object", new String[] { "I" });
		
		{
		        mv = cw.visitMethod(0, "<init>", "()V", null, null);
		        mv.visitCode();
		        mv.visitVarInsn(ALOAD, 0);
		        mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
		        mv.visitInsn(RETURN);
		        mv.visitMaxs(1, 1);
		        mv.visitEnd();
		}
		{
		        mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "bar", "()V", null, null);
		        mv.visitCode();
		        mv.visitInsn(RETURN);
		        mv.visitMaxs(2, 1);
		        mv.visitEnd();
		}
		{
		        mv = cw.visitMethod(ACC_STATIC, "<clinit>", "()V", null, null);
		        mv.visitCode();
		        mv.visitTypeInsn(NEW, "java/lang/InternalError");
		        mv.visitInsn(DUP);
		        mv.visitLdcInsn("class");
		        mv.visitMethodInsn(INVOKESPECIAL, "java/lang/InternalError", "<init>", "(Ljava/lang/String;)V", false);
		        mv.visitInsn(ATHROW);
		        mv.visitMaxs(3, 0);
		        mv.visitEnd();
		}
		cw.visitEnd();
		
		return cw.toByteArray();
	}
}
