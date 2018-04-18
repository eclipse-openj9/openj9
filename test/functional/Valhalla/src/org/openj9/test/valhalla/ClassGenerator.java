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
	
	public static byte[] generateClass(String name, Attribute[] attributes) {
		ClassWriter classWriter = new ClassWriter(0);
		classWriter.visit(V1_8, ACC_PUBLIC | ACC_SUPER, name, null, "java/lang/Object", null);
		
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

	public static byte[] MultipleNestMembersAttributesdump () throws Exception {
		String[] nestMembersList = {};
		NestMembersAttribute attr1 = new NestMembersAttribute(nestMembersList);
		NestMembersAttribute attr2 = new NestMembersAttribute(nestMembersList);
		return generateClass("MultipleNestMembersAttributes", new Attribute[]{ attr1, attr2});
	}

	public static byte[] MultipleNestHostAttributesdump () throws Exception {
		NestHostAttribute attr1 = new NestHostAttribute("clazzname");
		NestHostAttribute attr2 = new NestHostAttribute("clazzname");
		return generateClass("MultipleNestHostAttributes", new Attribute[]{ attr1, attr2 });
	}

	public static byte[] MultipleNestAttributesdump () throws Exception {
		String[] nestMembersList = { "DoesNotExist" };
		NestMembersAttribute nestMembersAttribute = new NestMembersAttribute(nestMembersList);
		NestHostAttribute nestHostAttribute = new NestHostAttribute("DoesNotExist");
		return generateClass("MultipleNestAttributes", new Attribute[]{ nestMembersAttribute, nestHostAttribute });
	}

	public static byte[] ClassIsOwnNestHostdump () throws Exception {
		NestHostAttribute nestHostAttribute = new NestHostAttribute("DoesNotExist");
		return generateClass("ClassIsOwnNestHost", new Attribute[]{ nestHostAttribute });
	}

	public static byte[] NestMemberIsItselfdump () throws Exception {
		String[] nestMembersList = { "NestMemberIsItself" };
		NestMembersAttribute nestMembersAttribute = new NestMembersAttribute(nestMembersList);
		return generateClass("NestMemberIsItself", new Attribute[]{ nestMembersAttribute });
	}	

	public static byte[] fieldAccessDump () throws Exception {
		ClassWriter cw = new ClassWriter(0);
		MethodVisitor mv;
		
		cw.visit(V1_8, ACC_PUBLIC | ACC_SUPER, "FieldAccess", null, "java/lang/Object", null);
		cw.visitInnerClass("FieldAccess$Inner", "FieldAccess", "Inner", ACC_PRIVATE | ACC_STATIC);
		String[] nestMembersList = { "FieldAccess$Inner" };
		NestMembersAttribute nestMembersAttribute = new NestMembersAttribute(nestMembersList);
		cw.visitAttribute(nestMembersAttribute);

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
			mv = cw.visitMethod(ACC_PUBLIC | ACC_STATIC, "foo", "()I", null, null);
			mv.visitCode();
			mv.visitFieldInsn(GETSTATIC, "FieldAccess$Inner", "f", "I");
			mv.visitInsn(IRETURN);
			mv.visitMaxs(1, 0);
			mv.visitEnd();
		}
		cw.visitEnd();
		return cw.toByteArray();
	}

	public static byte[] fieldAccess$InnerDump () throws Exception {
		ClassWriter cw = new ClassWriter(0);
		FieldVisitor fv;
		MethodVisitor mv;
		
		cw.visit(V1_8, ACC_SUPER, "FieldAccess$Inner", null, "java/lang/Object", null);
		NestHostAttribute nestHostAttribute = new NestHostAttribute("FieldAccess");
		cw.visitAttribute(nestHostAttribute);
		
		cw.visitInnerClass("FieldAccess$Inner", "FieldAccess", "Inner", ACC_PRIVATE | ACC_STATIC);
		{
			fv = cw.visitField(ACC_PRIVATE | ACC_STATIC, "f", "I", null, null);
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
		cw.visitEnd();
		return cw.toByteArray();
	}

	public static byte[] methodAccessDump () throws Exception {
		ClassWriter cw = new ClassWriter(0);
		MethodVisitor mv;
		
		cw.visit(V1_8, ACC_PUBLIC | ACC_SUPER, "MethodAccess", null, "java/lang/Object", null);
		String[] nestMembersList = { "MethodAccess$Inner" };
		NestMembersAttribute nestMembersAttribute = new NestMembersAttribute(nestMembersList);
		cw.visitAttribute(nestMembersAttribute);
		
		cw.visitInnerClass("MethodAccess$Inner", "MethodAccess", "Inner", ACC_PRIVATE | ACC_STATIC);
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
			mv = cw.visitMethod(ACC_PUBLIC | ACC_STATIC, "bar", "()I", null, null);
			mv.visitCode();
			mv.visitMethodInsn(INVOKESTATIC, "MethodAccess$Inner", "foo", "()I", false);
			mv.visitInsn(IRETURN);
			mv.visitMaxs(1, 0);
			mv.visitEnd();
		}
		cw.visitEnd();
		return cw.toByteArray();
	}

	public static byte[] methodAcccess$InnerDump () throws Exception {
		ClassWriter cw = new ClassWriter(0);
		MethodVisitor mv;
		
		cw.visit(V1_8, ACC_SUPER, "MethodAccess$Inner", null, "java/lang/Object", null);
		NestHostAttribute nestHostAttribute = new NestHostAttribute("MethodAccess");
		cw.visitAttribute(nestHostAttribute);
		cw.visitInnerClass("MethodAccess$Inner", "MethodAccess", "Inner", ACC_PRIVATE | ACC_STATIC);
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
			mv = cw.visitMethod(ACC_PRIVATE | ACC_STATIC, "foo", "()I", null, null);
			mv.visitCode();
			mv.visitInsn(ICONST_3);
			mv.visitInsn(IRETURN);
			mv.visitMaxs(1, 0);
			mv.visitEnd();
		}
		cw.visitEnd();
		return cw.toByteArray();
	}

	public static byte[] fieldAccessDump_different_package () throws Exception {
		ClassWriter cw = new ClassWriter(0);
		MethodVisitor mv;
		
		cw.visit(V1_8, ACC_PUBLIC | ACC_SUPER, "FieldAccess", null, "java/lang/Object", null);
		cw.visitInnerClass("pkg/FieldAccess$Inner", "FieldAccess", "Inner", ACC_PRIVATE | ACC_STATIC);
		String[] nestMembersList = { "pkg/FieldAccess$Inner" };
		NestMembersAttribute nestMembersAttribute = new NestMembersAttribute(nestMembersList);
		cw.visitAttribute(nestMembersAttribute);
		
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
			mv = cw.visitMethod(ACC_PUBLIC | ACC_STATIC, "foo", "()I", null, null);
			mv.visitCode();
			mv.visitFieldInsn(GETSTATIC, "pkg/FieldAccess$Inner", "f", "I");
			mv.visitInsn(IRETURN);
			mv.visitMaxs(1, 0);
			mv.visitEnd();
		}
		cw.visitEnd();
		return cw.toByteArray();
	}

	public static byte[] fieldAccess$InnerDump_different_package () throws Exception {
		ClassWriter cw = new ClassWriter(0);
		FieldVisitor fv;
		MethodVisitor mv;

		cw.visit(V1_8, ACC_SUPER, "pkg/FieldAccess$Inner", null, "java/lang/Object", null);
		NestHostAttribute nestHostAttribute = new NestHostAttribute("FieldAccess");
		cw.visitAttribute(nestHostAttribute);
		cw.visitInnerClass("pkg/FieldAccess$Inner", "FieldAccess", "Inner", ACC_PRIVATE | ACC_STATIC);
		{
			fv = cw.visitField(ACC_PRIVATE | ACC_STATIC, "f", "I", null, null);
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
			mv.visitFieldInsn(PUTSTATIC, "pkg/FieldAccess$Inner", "f", "I");
			mv.visitInsn(RETURN);
			mv.visitMaxs(1, 0);
			mv.visitEnd();
		}
		cw.visitEnd();
		return cw.toByteArray();
	}

	public static byte[] fieldAccessDump_nest_member_not_verified () throws Exception {
		ClassWriter cw = new ClassWriter(0);
		MethodVisitor mv;

		cw.visit(V1_8, ACC_PUBLIC | ACC_SUPER, "FieldAccess", null, "java/lang/Object", null);
		cw.visitInnerClass("FieldAccess$Inner", "FieldAccess", "Inner", ACC_PRIVATE | ACC_STATIC);
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
			mv = cw.visitMethod(ACC_PUBLIC | ACC_STATIC, "foo", "()I", null, null);
			mv.visitCode();
			mv.visitFieldInsn(GETSTATIC, "FieldAccess$Inner", "f", "I");
			mv.visitInsn(IRETURN);
			mv.visitMaxs(1, 0);
			mv.visitEnd();
		}
		cw.visitEnd();
		return cw.toByteArray();
	}

	public static byte[] fieldAccess$InnerDump_nest_member_not_verified () throws Exception {
		ClassWriter cw = new ClassWriter(0);
		FieldVisitor fv;
		MethodVisitor mv;

		cw.visit(V1_8, ACC_SUPER, "FieldAccess$Inner", null, "java/lang/Object", null);
		NestHostAttribute nestHostAttribute = new NestHostAttribute("FieldAccess");
		cw.visitAttribute(nestHostAttribute);
		
		cw.visitInnerClass("FieldAccess$Inner", "FieldAccess", "Inner", ACC_PRIVATE | ACC_STATIC);
		{
			fv = cw.visitField(ACC_PRIVATE | ACC_STATIC, "f", "I", null, null);
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
		cw.visitEnd();
		return cw.toByteArray();
	}

	public static byte[] fieldAccessDump_nest_host_not_claimed () throws Exception {
		ClassWriter cw = new ClassWriter(0);
		MethodVisitor mv;

		cw.visit(V1_8, ACC_PUBLIC | ACC_SUPER, "FieldAccess", null, "java/lang/Object", null);
		cw.visitInnerClass("FieldAccess$Inner", "FieldAccess", "Inner", ACC_PRIVATE | ACC_STATIC);
		String[] nestMembersList = { "FieldAccess$Inner" };
		NestMembersAttribute nestMembersAttribute = new NestMembersAttribute(nestMembersList);
		cw.visitAttribute(nestMembersAttribute);
		
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
			mv = cw.visitMethod(ACC_PUBLIC | ACC_STATIC, "foo", "()I", null, null);
			mv.visitCode();
			mv.visitFieldInsn(GETSTATIC, "FieldAccess$Inner", "f", "I");
			mv.visitInsn(IRETURN);
			mv.visitMaxs(1, 0);
			mv.visitEnd();
		}
		cw.visitEnd();
		return cw.toByteArray();
	}

	public static byte[] fieldAccess$InnerDump_nest_host_not_claimed () throws Exception {
		ClassWriter cw = new ClassWriter(0);
		FieldVisitor fv;
		MethodVisitor mv;

		cw.visit(V1_8, ACC_SUPER, "FieldAccess$Inner", null, "java/lang/Object", null);
		cw.visitInnerClass("FieldAccess$Inner", "FieldAccess", "Inner", ACC_PRIVATE | ACC_STATIC);
		{
			fv = cw.visitField(ACC_PRIVATE | ACC_STATIC, "f", "I", null, null);
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
		cw.visitEnd();
		return cw.toByteArray();
	}
}
