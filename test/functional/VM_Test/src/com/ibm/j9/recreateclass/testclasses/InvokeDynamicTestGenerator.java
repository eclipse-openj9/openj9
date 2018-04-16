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

import java.lang.invoke.CallSite;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;

import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.Handle;
import org.objectweb.asm.MethodVisitor;

/**
 * This class generates a class that uses invokedynamic bytecode.
 * 
 * @author ashutosh
 */
public class InvokeDynamicTestGenerator {
	public static byte[] generateClassData() {
		ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES);
		MethodVisitor mv;

		cw.visit(
				V1_7,
				ACC_PUBLIC,
				"com/ibm/j9/recreateclasscompare/testclasses/InvokeDynamicTest",
				null, "java/lang/Object", null);
		{
			mv = cw.visitMethod(0, "<init>", "()V", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>",
					"()V");
			mv.visitInsn(RETURN);
			mv.visitMaxs(1, 1);
			mv.visitEnd();
		}
		{
			mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "main",
					"([Ljava/lang/String;)V", null, null);
			mv.visitCode();
			mv.visitFieldInsn(GETSTATIC, "java/lang/System", "out",
					"Ljava/io/PrintStream;");
			mv.visitLdcInsn("hello");
			mv.visitLdcInsn("world");
			mv.visitMethodInsn(INVOKESTATIC, "MyTest", "concatStrings",
					"(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;");
			mv.visitMethodInsn(INVOKEVIRTUAL, "java/io/PrintStream", "println",
					"(Ljava/lang/String;)V");
			mv.visitFieldInsn(GETSTATIC, "java/lang/System", "out",
					"Ljava/io/PrintStream;");
			mv.visitInsn(ICONST_1);
			mv.visitInsn(ICONST_2);
			mv.visitMethodInsn(INVOKESTATIC, "MyTest", "addIntegers", "(II)I");
			mv.visitMethodInsn(INVOKEVIRTUAL, "java/io/PrintStream", "println",
					"(I)V");
			mv.visitInsn(RETURN);
			mv.visitMaxs(3, 1);
			mv.visitEnd();
		}

		{
			mv = cw.visitMethod(ACC_PUBLIC | ACC_STATIC, "concatStrings",
					"(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;",
					null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitVarInsn(ALOAD, 1);
			MethodType mt1 = MethodType.methodType(String.class, String.class,
					String.class);
			MethodType mt2 = MethodType.methodType(CallSite.class,
					MethodHandles.Lookup.class, String.class, MethodType.class);
			Handle handle = new Handle(
					H_INVOKESTATIC,
					"com/ibm/j9/recreateclasscompare/testclasses/BootstrapMethods",
					"bootstrapConcatStrings", mt2.toMethodDescriptorString());
			mv.visitInvokeDynamicInsn("concatStrings",
					mt1.toMethodDescriptorString(), handle);
			mv.visitInsn(ARETURN);
			mv.visitMaxs(2, 2);
			mv.visitEnd();
		}
		{
			mv = cw.visitMethod(ACC_PUBLIC | ACC_STATIC, "addIntegers",
					"(II)I", null, null);
			mv.visitCode();
			mv.visitVarInsn(ILOAD, 0);
			mv.visitVarInsn(ILOAD, 1);
			MethodType mt1 = MethodType.methodType(int.class, int.class,
					int.class);
			MethodType mt2 = MethodType.methodType(CallSite.class,
					MethodHandles.Lookup.class, String.class, MethodType.class);
			Handle handle = new Handle(
					H_INVOKESTATIC,
					"com/ibm/j9/recreateclasscompare/testclasses/BootstrapMethods",
					"bootstrapAddInts", mt2.toMethodDescriptorString());
			mv.visitInvokeDynamicInsn("addInts",
					mt1.toMethodDescriptorString(), handle);
			mv.visitInsn(IRETURN);
			mv.visitMaxs(2, 2);
			mv.visitEnd();
		}
		{
			mv = cw.visitMethod(ACC_PUBLIC | ACC_STATIC, "getStringConstant",
					"()Ljava/lang/String;", null, null);
			mv.visitCode();
			MethodType mt1 = MethodType.methodType(void.class, String.class);
			MethodType mt2 = MethodType.methodType(CallSite.class,
					MethodHandles.Lookup.class, String.class, MethodType.class);
			Handle handle = new Handle(
					H_INVOKESTATIC,
					"com/ibm/j9/recreateclasscompare/testclasses/BootstrapMethods",
					"bootstrapGetStringConstant", mt2
							.toMethodDescriptorString());
			mv.visitInvokeDynamicInsn("getStringConstant",
					mt1.toMethodDescriptorString(), handle, "String_Constant");
			mv.visitInsn(ARETURN);
			mv.visitMaxs(2, 2);
			mv.visitEnd();
		}
		cw.visitEnd();

		return cw.toByteArray();
	}
}
