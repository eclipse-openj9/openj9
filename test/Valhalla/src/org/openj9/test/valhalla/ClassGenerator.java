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

package org.openj9.test.valhalla;
import org.objectweb.asm.*;

public class ClassGenerator implements Opcodes {

	public static byte[] fieldAccessDump () throws Exception {
		ClassWriter cw = new ClassWriter(0);
		MethodVisitor mv;
		cw.visit(52, ACC_PUBLIC + ACC_SUPER, "FieldAccess", null, "java/lang/Object", null);
		cw.visitInnerClass("FieldAccess$Inner", "FieldAccess", "Inner", ACC_PRIVATE + ACC_STATIC);
		{
			mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
			mv.visitInsn(RETURN);
			mv.visitMaxs(1, 1);
			mv.visitEnd();
		}
		{
			mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "foo", "()I", null, null);
			mv.visitCode();
			mv.visitFieldInsn(GETSTATIC, "FieldAccess$Inner", "f", "I");
			mv.visitInsn(IRETURN);
			mv.visitMaxs(1, 0);
			mv.visitEnd();
		}
		{
			String[] nestmems = {"FieldAccess$Inner"};
			NestMembersAttribute attr = new NestMembersAttribute(nestmems);
			cw.visitAttribute(attr);
		}
		cw.visitEnd();

		return cw.toByteArray();
	}

	public static byte[] fieldAccess$InnerDump () throws Exception {
		ClassWriter cw = new ClassWriter(0);
		FieldVisitor fv;
		MethodVisitor mv;
		cw.visit(52, ACC_SUPER, "FieldAccess$Inner", null, "java/lang/Object", null);
		cw.visitInnerClass("FieldAccess$Inner", "FieldAccess", "Inner", ACC_PRIVATE + ACC_STATIC);
		{
			fv = cw.visitField(ACC_PRIVATE + ACC_STATIC, "f", "I", null, null);
			fv.visitEnd();
		}
		{
			mv = cw.visitMethod(ACC_PRIVATE, "<init>", "()V", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
			mv.visitInsn(RETURN);
			mv.visitMaxs(1, 1);
			mv.visitEnd();
		}
		{
			mv = cw.visitMethod(ACC_STATIC, "<clinit>", "()V", null, null);
			mv.visitCode();
			mv.visitInsn(ICONST_3);
			mv.visitFieldInsn(PUTSTATIC, "FieldAccess$Inner", "f", "I");
			mv.visitInsn(RETURN);
			mv.visitMaxs(1, 0);
			mv.visitEnd();
		}
		{
			NestHostAttribute attr = new NestHostAttribute("FieldAccess$Inner");
			cw.visitAttribute(attr);
		}
		cw.visitEnd();

		return cw.toByteArray();
	}

	public static byte[] methodAccessDump () throws Exception {
		ClassWriter cw = new ClassWriter(0);
		MethodVisitor mv;
		cw.visit(52, ACC_PUBLIC + ACC_SUPER, "MethodAccess", null, "java/lang/Object", null);
		cw.visitInnerClass("MethodAccess$Inner", "MethodAccess", "Inner", ACC_PRIVATE + ACC_STATIC);
		{
			mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
			mv.visitInsn(RETURN);
			mv.visitMaxs(1, 1);
			mv.visitEnd();
		}
		{
			mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "bar", "()I", null, null);
			mv.visitCode();
			mv.visitMethodInsn(INVOKESTATIC, "MethodAccess$Inner", "foo", "()I", false);
			mv.visitInsn(IRETURN);
			mv.visitMaxs(1, 0);
			mv.visitEnd();
		}
		{  /* Nestmates attribute(s) */
			NestHostAttribute attr = new NestHostAttribute("MethodAccess$Inner");
			cw.visitAttribute(attr);
		}
		cw.visitEnd();

		return cw.toByteArray();
	}

	public static byte[] methodAcccess$InnerDump () throws Exception {
		ClassWriter cw = new ClassWriter(0);
		MethodVisitor mv;
		cw.visit(52, ACC_SUPER, "MethodAccess$Inner", null, "java/lang/Object", null);
		cw.visitInnerClass("MethodAccess$Inner", "MethodAccess", "Inner", ACC_PRIVATE + ACC_STATIC);
		{
			mv = cw.visitMethod(ACC_PRIVATE, "<init>", "()V", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
			mv.visitInsn(RETURN);
			mv.visitMaxs(1, 1);
			mv.visitEnd();
		}
		{
			mv = cw.visitMethod(ACC_PRIVATE + ACC_STATIC, "foo", "()I", null, null);
			mv.visitCode();
			mv.visitInsn(ICONST_3);
			mv.visitInsn(IRETURN);
			mv.visitMaxs(1, 0);
			mv.visitEnd();
		}
		{
			String[] nestmems = {"MethodAccess"};
			NestMembersAttribute attr = new NestMembersAttribute(nestmems);
			cw.visitAttribute(attr);
		}
		cw.visitEnd();

		return cw.toByteArray();
	}

	public static byte[] multipleNestMembersDump () throws Exception {
		ClassWriter cw = new ClassWriter(0);
		MethodVisitor mv;
		cw.visit(52, ACC_PUBLIC + ACC_SUPER, "MultipleNestMembers", null, "java/lang/Object", null);
		cw.visitInnerClass("MultipleNestMembers$c", "MultipleNestMembers", "c", ACC_PRIVATE + ACC_STATIC);
		cw.visitInnerClass("MultipleNestMembers$b", "MultipleNestMembers", "b", ACC_PRIVATE);
		cw.visitInnerClass("MultipleNestMembers$a", "MultipleNestMembers", "a", ACC_PUBLIC);
		{
			mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
			mv.visitInsn(RETURN);
			mv.visitMaxs(1, 1);
			mv.visitEnd();
		}
		{
			String[] nestmems = {"MethodAccess$a", "MethodAccess$b", "MethodAccess$c"};
			NestMembersAttribute attr = new NestMembersAttribute(nestmems);
			cw.visitAttribute(attr);
		}
		cw.visitEnd();
		return cw.toByteArray();
	}

	public static byte[] multipleNestMembers$aDump () throws Exception {
		ClassWriter cw = new ClassWriter(0);
		FieldVisitor fv;
		MethodVisitor mv;
		cw.visit(52, ACC_PUBLIC + ACC_SUPER, "MultipleNestMembers$a", null, "java/lang/Object", null);
		cw.visitInnerClass("MultipleNestMembers$a", "MultipleNestMembers", "a", ACC_PUBLIC);
		{
			fv = cw.visitField(ACC_FINAL + ACC_SYNTHETIC, "this$0", "LMultipleNestMembers;", null, null);
			fv.visitEnd();
		}
		{
			mv = cw.visitMethod(ACC_PUBLIC, "<init>", "(LMultipleNestMembers;)V", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitVarInsn(ALOAD, 1);
			mv.visitFieldInsn(PUTFIELD, "MultipleNestMembers$a", "this$0", "LMultipleNestMembers;");
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
			mv.visitInsn(RETURN);
			mv.visitMaxs(2, 2);
			mv.visitEnd();
		}
		{
			NestHostAttribute attr = new NestHostAttribute("MultipleNestMembers");
			cw.visitAttribute(attr);
		}
		cw.visitEnd();
		return cw.toByteArray();
	}

	public static byte[] multipleNestMembers$bDump () throws Exception {
		ClassWriter cw = new ClassWriter(0);
		FieldVisitor fv;
		MethodVisitor mv;
		cw.visit(52, ACC_SUPER, "MultipleNestMembers$b", null, "java/lang/Object", null);
		cw.visitInnerClass("MultipleNestMembers$b", "MultipleNestMembers", "b", ACC_PRIVATE);
		{
			fv = cw.visitField(ACC_FINAL + ACC_SYNTHETIC, "this$0", "LMultipleNestMembers;", null, null);
			fv.visitEnd();
		}
		{
			mv = cw.visitMethod(ACC_PRIVATE, "<init>", "(LMultipleNestMembers;)V", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitVarInsn(ALOAD, 1);
			mv.visitFieldInsn(PUTFIELD, "MultipleNestMembers$b", "this$0", "LMultipleNestMembers;");
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
			mv.visitInsn(RETURN);
			mv.visitMaxs(2, 2);
			mv.visitEnd();
		}
		{
			NestHostAttribute attr = new NestHostAttribute("MultipleNestMembers");
			cw.visitAttribute(attr);
		}
		cw.visitEnd();
		return cw.toByteArray();
	}

	public static byte[] multipleNestMembers$cDump () throws Exception {
		ClassWriter cw = new ClassWriter(0);
		MethodVisitor mv;
		cw.visit(52, ACC_SUPER, "MultipleNestMembers$c", null, "java/lang/Object", null);
		cw.visitInnerClass("MultipleNestMembers$c", "MultipleNestMembers", "c", ACC_PRIVATE + ACC_STATIC);
		{
			mv = cw.visitMethod(ACC_PRIVATE, "<init>", "()V", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
			mv.visitInsn(RETURN);
			mv.visitMaxs(1, 1);
			mv.visitEnd();
		}
		{
			NestHostAttribute attr = new NestHostAttribute("MultipleNestMembers");
			cw.visitAttribute(attr);
		}
		cw.visitEnd();
		return cw.toByteArray();
	}

	public static byte[] MultipleNestMembersAttributesdump () throws Exception {
		ClassWriter cw = new ClassWriter(0);
		MethodVisitor mv;
		cw.visit(52, ACC_PUBLIC + ACC_SUPER, "MultipleNestMembersAttributes", null, "java/lang/Object", null);
		{
			mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
			mv.visitInsn(RETURN);
			mv.visitMaxs(1, 1);
			mv.visitEnd();
		}
		{
			String[] nestmems = {};
			NestMembersAttribute attr = new NestMembersAttribute(nestmems);
			cw.visitAttribute(attr);
		}
		{
			String[] nestmems = {};
			NestMembersAttribute attr = new NestMembersAttribute(nestmems);
			cw.visitAttribute(attr);
		}
		cw.visitEnd();
		return cw.toByteArray();
	}

	public static byte[] MultipleNestHostAttributesdump () throws Exception {
		ClassWriter cw = new ClassWriter(0);
		MethodVisitor mv;
		cw.visit(52, ACC_PUBLIC + ACC_SUPER, "MultipleNestHostAttributes", null, "java/lang/Object", null);
		{
			mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
			mv.visitInsn(RETURN);
			mv.visitMaxs(1, 1);
			mv.visitEnd();
		}
		{
			mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "main", "([Ljava/lang/String;)V", null, new String[] { "java/lang/Exception" });
			mv.visitCode();
			mv.visitInsn(RETURN);
			mv.visitMaxs(0, 1);
			mv.visitEnd();
		}
		{
			NestHostAttribute attr = new NestHostAttribute("clazzname");
			cw.visitAttribute(attr);
		}
		{
			NestHostAttribute attr = new NestHostAttribute("clazzname");
			cw.visitAttribute(attr);
		}
		cw.visitEnd();
		return cw.toByteArray();
	}

	public static byte[] MultipleNestAttributesdump () throws Exception {
		ClassWriter cw = new ClassWriter(0);
		MethodVisitor mv;
		cw.visit(52, ACC_PUBLIC + ACC_SUPER, "MultipleNestAttributes", null, "java/lang/Object", null);
		{
			mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
			mv.visitInsn(RETURN);
			mv.visitMaxs(1, 1);
			mv.visitEnd();
		}
		{
			String[] nestmems = {};
			NestMembersAttribute attr = new NestMembersAttribute(nestmems);
			cw.visitAttribute(attr);
		}
		{
			NestHostAttribute attr = new NestHostAttribute("clazzname");
			cw.visitAttribute(attr);
		}
		cw.visitEnd();
		return cw.toByteArray();
	}

	public static byte[] DifferentPackagedump () throws Exception {
		ClassWriter cw = new ClassWriter(0);
		MethodVisitor mv;
		cw.visit(52, ACC_PUBLIC + ACC_SUPER, "TestClass", null, "java/lang/Object", null);
		cw.visitInnerClass("TestClass$Inner", "TestClass", "Inner", ACC_PUBLIC);
		{
			mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
			mv.visitInsn(RETURN);
			mv.visitMaxs(1, 1);
			mv.visitEnd();
		}
		{
			String[] nestmems = {"p/TestClass$Inner"};
			NestMembersAttribute attr = new NestMembersAttribute(nestmems);
			cw.visitAttribute(attr);
		}
		cw.visitEnd();
		return cw.toByteArray();
	}

	public static byte[] DifferentPackage$Innerdump () throws Exception {
		ClassWriter cw = new ClassWriter(0);
		FieldVisitor fv;
		MethodVisitor mv;
		cw.visit(52, ACC_PUBLIC + ACC_SUPER, "p/TestClass$Inner", null, "java/lang/Object", null);
		cw.visitInnerClass("p/TestClass$Inner", "p/TestClass", "Inner", ACC_PUBLIC);
		{
			fv = cw.visitField(ACC_FINAL + ACC_SYNTHETIC, "this$0", "Lp/TestClass;", null, null);
			fv.visitEnd();
		}
		{
			mv = cw.visitMethod(ACC_PUBLIC, "<init>", "(Lp/TestClass;)V", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitVarInsn(ALOAD, 1);
			mv.visitFieldInsn(PUTFIELD, "p/TestClass$Inner", "this$0", "Lp/TestClass;");
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
			mv.visitInsn(RETURN);
			mv.visitMaxs(2, 2);
			mv.visitEnd();
		}
		{
			NestHostAttribute attr = new NestHostAttribute("TestClass");
			cw.visitAttribute(attr);
		}
		cw.visitEnd();
		return cw.toByteArray();
	}

	public static byte[] nestTopNotClaimedDump () throws Exception {
		ClassWriter cw = new ClassWriter(0);
		MethodVisitor mv;
		cw.visit(52, ACC_PUBLIC + ACC_SUPER, "NestTopNotClaimed", null, "java/lang/Object", null);
		cw.visitInnerClass("NestTopNotClaimed$Inner", "NestTopNotClaimed", "Inner", ACC_PRIVATE + ACC_STATIC);
		{
			mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
			mv.visitInsn(RETURN);
			mv.visitMaxs(1, 1);
			mv.visitEnd();
		}
		{
			mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "bar", "()I", null, null);
			mv.visitCode();
			mv.visitFieldInsn(GETSTATIC, "NestTopNotClaimed$Inner", "f", "I");			mv.visitInsn(IRETURN);
			mv.visitMaxs(1, 0);
			mv.visitEnd();
		}
		{  /* NestMembers attribute */
			String[] nestmems = {"NestTopNotClaimed$Inner"};
			NestMembersAttribute attr = new NestMembersAttribute(nestmems);
			cw.visitAttribute(attr);
		}
		cw.visitEnd();
		return cw.toByteArray();
	}

	public static byte[] nestTopNotClaimed$InnerDump () throws Exception {
		ClassWriter cw = new ClassWriter(0);
		FieldVisitor fv;
		MethodVisitor mv;
		cw.visit(52, ACC_SUPER, "NestTopNotClaimed$Inner", null, "java/lang/Object", null);
		cw.visitInnerClass("NestTopNotClaimed$Inner", "NestTopNotClaimed", "Inner", ACC_PRIVATE + ACC_STATIC);
		{
			fv = cw.visitField(ACC_PRIVATE + ACC_STATIC, "f", "I", null, null);
			fv.visitEnd();
		}
		{
			mv = cw.visitMethod(ACC_PRIVATE, "<init>", "()V", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
			mv.visitInsn(RETURN);
			mv.visitMaxs(1, 1);
			mv.visitEnd();
		}
		{
			mv = cw.visitMethod(ACC_STATIC, "<clinit>", "()V", null, null);
			mv.visitCode();
			mv.visitInsn(ICONST_3);
			mv.visitFieldInsn(PUTSTATIC, "NestTopNotClaimed$Inner", "f", "I");
			mv.visitInsn(RETURN);
			mv.visitMaxs(1, 0);
			mv.visitEnd();
		}
		cw.visitEnd();
		return cw.toByteArray();
	}

	public static byte[] nestMemberNotClaimedDump () throws Exception {
		ClassWriter cw = new ClassWriter(0);
		MethodVisitor mv;
		cw.visit(52, ACC_PUBLIC + ACC_SUPER, "NestMemberNotClaimed", null, "java/lang/Object", null);
		cw.visitInnerClass("NestMemberNotClaimed$Inner", "NestMemberNotClaimed", "Inner", ACC_PRIVATE + ACC_STATIC);
		{
			mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
			mv.visitInsn(RETURN);
			mv.visitMaxs(1, 1);
			mv.visitEnd();
		}
		{
			mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "bar", "()I", null, null);
			mv.visitCode();
			mv.visitFieldInsn(GETSTATIC, "NestMemberNotClaimed$Inner", "f", "I");			
			mv.visitInsn(IRETURN);
			mv.visitMaxs(1, 0);
			mv.visitEnd();
		}
		{  /* NestMembers attribute */
			String[] nestmems = {"NestMemberNotClaimed$Inner"};
			NestMembersAttribute attr = new NestMembersAttribute(nestmems);
			cw.visitAttribute(attr);
		}
		cw.visitEnd();
		return cw.toByteArray();
	}

	public static byte[] nestMemberNotClaimed$InnerDump () throws Exception {
		ClassWriter cw = new ClassWriter(0);
		FieldVisitor fv;
		MethodVisitor mv;
		cw.visit(52, ACC_SUPER, "NestMemberNotClaimed$Inner", null, "java/lang/Object", null);
		cw.visitInnerClass("NestMemberNotClaimed$Inner", "NestMemberNotClaimed", "Inner", ACC_PRIVATE + ACC_STATIC);
		{
			fv = cw.visitField(ACC_PRIVATE + ACC_STATIC, "f", "I", null, null);
			fv.visitEnd();
		}
		{
			mv = cw.visitMethod(ACC_PRIVATE, "<init>", "()V", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
			mv.visitInsn(RETURN);
			mv.visitMaxs(1, 1);
			mv.visitEnd();
		}
		{
			mv = cw.visitMethod(ACC_STATIC, "<clinit>", "()V", null, null);
			mv.visitCode();
			mv.visitInsn(ICONST_3);
			mv.visitFieldInsn(PUTSTATIC, "NestMemberNotClaimed$Inner", "f", "I");
			mv.visitInsn(RETURN);
			mv.visitMaxs(1, 0);
			mv.visitEnd();
		}
		cw.visitEnd();
		return cw.toByteArray();
	}
}
