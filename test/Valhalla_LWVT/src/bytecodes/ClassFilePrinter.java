/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

 package bytecodes;

import java.io.File;
import java.io.FileOutputStream;
import org.objectweb.asm.*;

public class ClassFilePrinter implements Opcodes {

	public static final int DEFAULTVALUE = 204;
	public static final int WITHFIELD = 203;
	public static final int ACC_VALUE_TYPE = 0x100;

	public static final String OUTPUT_DIR = "out/";

	public static void main(String[] args) throws Throwable {
		printFromDump(ValueDump(), "Value");
		printFromDump(AlmostValueDump(), "AlmostValue");
		printFromDump(NotValueDump(), "NotValue");
		printFromDump(AbstractDump(), "Abstract");
		printFromDump(InterfaceDump(), "Interface");
		printFromDump(TestDump(), "Test");
	}

	public static void printFromDump(byte[] dump, String name) throws Throwable {
		File file = new File(OUTPUT_DIR + name + ".class");
		FileOutputStream stream = new FileOutputStream(file);
		try {
			stream.write(dump);
		} finally {
			stream.close();
		}
	}

	/* MODIFIED */
	public static byte[] ValueDump () throws Exception {

		ClassWriter cw = new ClassWriter(0);
		FieldVisitor fv;
		MethodVisitor mv;
		AnnotationVisitor av0;

		cw.visit(V1_8, ACC_PUBLIC + ACC_FINAL + ACC_SUPER + ACC_VALUE_TYPE, "bytecodes/Value", null, "java/lang/Object", null);

		cw.visitSource("Value.java", null);

		{
		fv = cw.visitField(ACC_PUBLIC + ACC_STATIC, "theStatic", "I", null, null);
		fv.visitEnd();
		}
		{
		fv = cw.visitField(ACC_PUBLIC + ACC_FINAL, "theDouble", "D", null, null);
		fv.visitEnd();
		}
		{
		fv = cw.visitField(ACC_PUBLIC + ACC_FINAL, "theObject", "Lbytecodes/Value;", null, null);
		fv.visitEnd();
		}
		{
		fv = cw.visitField(ACC_PUBLIC + ACC_FINAL, "theSingle", "I", null, null);
		fv.visitEnd();
		}
		{
		mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
		mv.visitCode();
		Label l0 = new Label();
		mv.visitLabel(l0);
		mv.visitLineNumber(3, l0);
		mv.visitVarInsn(ALOAD, 0);
		mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
		mv.visitInsn(RETURN);
		Label l1 = new Label();
		mv.visitLabel(l1);
		mv.visitLocalVariable("this", "Lbytecodes/Value;", null, l0, l1, 0);
		mv.visitMaxs(1, 1);
		mv.visitEnd();
		}
		{
		mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "makeValue", "(DLbytecodes/Value;I)Lbytecodes/Value;", null, null);
		mv.visitCode();
		Label l0 = new Label();
		mv.visitLabel(l0);
		mv.visitLineNumber(15, l0);
		mv.visitTypeInsn(DEFAULTVALUE, "bytecodes/Value");
		mv.visitInsn(DUP);
		mv.visitMethodInsn(INVOKESPECIAL, "bytecodes/Value", "<init>", "()V", false);
		Label l1 = new Label();
		mv.visitLabel(l1);
		mv.visitLineNumber(16, l1);
		mv.visitVarInsn(DLOAD, 0);
		mv.visitFieldInsn(WITHFIELD, "bytecodes/Value", "theDouble", "D");
		Label l2 = new Label();
		mv.visitLabel(l2);
		mv.visitLineNumber(17, l2);
		mv.visitVarInsn(ALOAD, 2);
		mv.visitFieldInsn(WITHFIELD, "bytecodes/Value", "theObject", "Lbytecodes/Value;");
		Label l3 = new Label();
		mv.visitLabel(l3);
		mv.visitLineNumber(18, l3);
		mv.visitVarInsn(ILOAD, 3);
		mv.visitFieldInsn(WITHFIELD, "bytecodes/Value", "theSingle", "I");
		Label l4 = new Label();
		mv.visitLabel(l4);
		mv.visitLineNumber(19, l4);
		mv.visitInsn(ARETURN);
		Label l5 = new Label();
		mv.visitLabel(l5);
		mv.visitLocalVariable("theDouble", "D", null, l0, l5, 0);
		mv.visitLocalVariable("theObject", "Lbytecodes/Value;", null, l0, l5, 2);
		mv.visitLocalVariable("theSingle", "I", null, l0, l5, 3);
		mv.visitLocalVariable("result", "Lbytecodes/Value;", null, l1, l5, 4);
		mv.visitMaxs(3, 5);
		mv.visitEnd();
		}
		{
		mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "witherStatic", "()V", null, null);
		mv.visitCode();
		Label l0 = new Label();
		mv.visitLabel(l0);
		mv.visitLineNumber(40, l0);
		mv.visitInsn(ICONST_0);
		mv.visitFieldInsn(WITHFIELD, "bytecodes/Value", "theStatic", "I");
		Label l1 = new Label();
		mv.visitLabel(l1);
		mv.visitLineNumber(41, l1);
		mv.visitInsn(RETURN);
		mv.visitMaxs(1, 0);
		mv.visitEnd();
		}
		cw.visitEnd();

		return cw.toByteArray();
	}

	public static byte[] AlmostValueDump () throws Exception {

		ClassWriter cw = new ClassWriter(0);
		FieldVisitor fv;
		MethodVisitor mv;
		AnnotationVisitor av0;

		cw.visit(V1_8, ACC_PUBLIC + ACC_FINAL + ACC_SUPER + ACC_VALUE_TYPE, "bytecodes/AlmostValue", null, "java/lang/Object", null);

		cw.visitSource("AlmostValue.java", null);

		{
		fv = cw.visitField(ACC_PUBLIC, "field", "I", null, null);
		fv.visitEnd();
		}
		{
		mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
		mv.visitCode();
		Label l0 = new Label();
		mv.visitLabel(l0);
		mv.visitLineNumber(3, l0);
		mv.visitVarInsn(ALOAD, 0);
		mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
		mv.visitInsn(RETURN);
		Label l1 = new Label();
		mv.visitLabel(l1);
		mv.visitLocalVariable("this", "Lbytecodes/AlmostValue;", null, l0, l1, 0);
		mv.visitMaxs(1, 1);
		mv.visitEnd();
		}
		{
		mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "makeAlmostValue", "(I)Lbytecodes/AlmostValue;", null, null);
		mv.visitCode();
		Label l0 = new Label();
		mv.visitLabel(l0);
		mv.visitLineNumber(7, l0);
		mv.visitTypeInsn(DEFAULTVALUE, "bytecodes/AlmostValue");
		mv.visitInsn(DUP);
		mv.visitMethodInsn(INVOKESPECIAL, "bytecodes/AlmostValue", "<init>", "()V", false);
		Label l1 = new Label();
		mv.visitLabel(l1);
		mv.visitLineNumber(8, l1);
		mv.visitVarInsn(ILOAD, 0);
		mv.visitFieldInsn(WITHFIELD, "bytecodes/AlmostValue", "field", "I");
		Label l2 = new Label();
		mv.visitLabel(l2);
		mv.visitLineNumber(9, l2);
		mv.visitInsn(ARETURN);
		Label l3 = new Label();
		mv.visitLabel(l3);
		mv.visitLocalVariable("field", "I", null, l0, l3, 0);
		mv.visitLocalVariable("av", "Lbytecodes/AlmostValue;", null, l1, l3, 1);
		mv.visitMaxs(2, 2);
		mv.visitEnd();
		}
		cw.visitEnd();

		return cw.toByteArray();
	}

	public static byte[] NotValueDump () throws Exception {

		ClassWriter cw = new ClassWriter(0);
		FieldVisitor fv;
		MethodVisitor mv;
		AnnotationVisitor av0;

		cw.visit(V1_8, ACC_PUBLIC + ACC_SUPER, "bytecodes/NotValue", null, "java/lang/Object", null);

		cw.visitSource("NotValue.java", null);

		{
		fv = cw.visitField(0, "field", "I", null, null);
		fv.visitEnd();
		}
		{
		mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
		mv.visitCode();
		Label l0 = new Label();
		mv.visitLabel(l0);
		mv.visitLineNumber(3, l0);
		mv.visitVarInsn(ALOAD, 0);
		mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
		mv.visitInsn(RETURN);
		Label l1 = new Label();
		mv.visitLabel(l1);
		mv.visitLocalVariable("this", "Lbytecodes/NotValue;", null, l0, l1, 0);
		mv.visitMaxs(1, 1);
		mv.visitEnd();
		}
		{
		mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "defaultvalue", "()Lbytecodes/NotValue;", null, null);
		mv.visitCode();
		Label l0 = new Label();
		mv.visitLabel(l0);
		mv.visitLineNumber(29, l0);
		mv.visitTypeInsn(DEFAULTVALUE, "bytecodes/NotValue");
		mv.visitInsn(DUP);
		mv.visitMethodInsn(INVOKESPECIAL, "bytecodes/NotValue", "<init>", "()V", false);
		mv.visitInsn(ARETURN);
		mv.visitMaxs(2, 0);
		mv.visitEnd();
		}
		{
		mv = cw.visitMethod(ACC_PUBLIC, "withfield", "()Lbytecodes/NotValue;", null, null);
		mv.visitCode();
		Label l0 = new Label();
		mv.visitLabel(l0);
		mv.visitLineNumber(11, l0);
		mv.visitVarInsn(ALOAD, 0);
		mv.visitInsn(ICONST_3);
		mv.visitFieldInsn(WITHFIELD, "bytecodes/NotValue", "field", "I");
		Label l1 = new Label();
		mv.visitLabel(l1);
		mv.visitLineNumber(12, l1);
		mv.visitInsn(ARETURN);
		Label l2 = new Label();
		mv.visitLabel(l2);
		mv.visitLocalVariable("this", "Lbytecodes/NotValue;", null, l0, l2, 0);
		mv.visitMaxs(2, 1);
		mv.visitEnd();
		}
		cw.visitEnd();

		return cw.toByteArray();
	}

	public static byte[] AbstractDump () throws Exception {

		ClassWriter cw = new ClassWriter(0);
		FieldVisitor fv;
		MethodVisitor mv;
		AnnotationVisitor av0;

		cw.visit(V1_8, ACC_PUBLIC + ACC_SUPER + ACC_ABSTRACT, "bytecodes/Abstract", null, "java/lang/Object", null);

		cw.visitSource("Abstract.java", null);

		{
		mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
		mv.visitCode();
		Label l0 = new Label();
		mv.visitLabel(l0);
		mv.visitLineNumber(3, l0);
		mv.visitVarInsn(ALOAD, 0);
		mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
		mv.visitInsn(RETURN);
		Label l1 = new Label();
		mv.visitLabel(l1);
		mv.visitLocalVariable("this", "Lbytecodes/Abstract;", null, l0, l1, 0);
		mv.visitMaxs(1, 1);
		mv.visitEnd();
		}
		{
		mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "defaultvalue", "()V", null, null);
		mv.visitCode();
		Label l0 = new Label();
		mv.visitLabel(l0);
		mv.visitLineNumber(5, l0);
		mv.visitTypeInsn(DEFAULTVALUE, "bytecodes/Abstract");
		Label l1 = new Label();
		mv.visitLabel(l1);
		mv.visitLineNumber(6, l1);
		mv.visitInsn(RETURN);
		Label l2 = new Label();
		mv.visitLabel(l2);
		mv.visitLocalVariable("a", "Lbytecodes/Abstract;", null, l1, l2, 0);
		mv.visitMaxs(2, 1);
		mv.visitEnd();
		}
		cw.visitEnd();

		return cw.toByteArray();
	}

	public static byte[] InterfaceDump () throws Exception {

		ClassWriter cw = new ClassWriter(0);
		FieldVisitor fv;
		MethodVisitor mv;
		AnnotationVisitor av0;

		cw.visit(V1_8, ACC_PUBLIC + ACC_ABSTRACT + ACC_INTERFACE, "bytecodes/Interface", null, "java/lang/Object", null);

		cw.visitSource("Interface.java", null);

		{
		mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "defaultvalue", "()V", null, null);
		mv.visitCode();
		Label l0 = new Label();
		mv.visitLabel(l0);
		mv.visitLineNumber(6, l0);
		mv.visitTypeInsn(DEFAULTVALUE, "bytecodes/Interface");
		Label l1 = new Label();
		mv.visitLabel(l1);
		mv.visitLineNumber(7, l1);
		mv.visitInsn(RETURN);
		Label l2 = new Label();
		mv.visitLabel(l2);
		mv.visitLocalVariable("i", "Lbytecodes/Interface;", null, l1, l2, 0);
		mv.visitMaxs(2, 1);
		mv.visitEnd();
		}
		cw.visitEnd();

		return cw.toByteArray();
	}

	public static byte[] TestDump () throws Exception {

		ClassWriter cw = new ClassWriter(0);
		FieldVisitor fv;
		MethodVisitor mv;
		AnnotationVisitor av0;

		cw.visit(V1_8, ACC_PUBLIC + ACC_SUPER, "bytecodes/Test", null, "java/lang/Object", null);

		cw.visitSource("Test.java", null);

		{
		fv = cw.visitField(ACC_FINAL + ACC_STATIC + ACC_SYNTHETIC, "$assertionsDisabled", "Z", null, null);
		fv.visitEnd();
		}
		{
		mv = cw.visitMethod(ACC_STATIC, "<clinit>", "()V", null, null);
		mv.visitCode();
		Label l0 = new Label();
		mv.visitLabel(l0);
		mv.visitLineNumber(25, l0);
		mv.visitLdcInsn(Type.getType("Lbytecodes/Test;"));
		mv.visitMethodInsn(INVOKEVIRTUAL, "java/lang/Class", "desiredAssertionStatus", "()Z", false);
		Label l1 = new Label();
		mv.visitJumpInsn(IFNE, l1);
		mv.visitInsn(ICONST_1);
		Label l2 = new Label();
		mv.visitJumpInsn(GOTO, l2);
		mv.visitLabel(l1);
		mv.visitFrame(Opcodes.F_SAME, 0, null, 0, null);
		mv.visitInsn(ICONST_0);
		mv.visitLabel(l2);
		mv.visitFrame(Opcodes.F_SAME1, 0, null, 1, new Object[] {Opcodes.INTEGER});
		mv.visitFieldInsn(PUTSTATIC, "bytecodes/Test", "$assertionsDisabled", "Z");
		mv.visitInsn(RETURN);
		mv.visitMaxs(1, 0);
		mv.visitEnd();
		}
		{
		mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
		mv.visitCode();
		Label l0 = new Label();
		mv.visitLabel(l0);
		mv.visitLineNumber(25, l0);
		mv.visitVarInsn(ALOAD, 0);
		mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
		mv.visitInsn(RETURN);
		Label l1 = new Label();
		mv.visitLabel(l1);
		mv.visitLocalVariable("this", "Lbytecodes/Test;", null, l0, l1, 0);
		mv.visitMaxs(1, 1);
		mv.visitEnd();
		}
		{
		mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "main", "([Ljava/lang/String;)V", null, null);
		mv.visitCode();
		Label l0 = new Label();
		mv.visitLabel(l0);
		mv.visitLineNumber(28, l0);
		mv.visitMethodInsn(INVOKESTATIC, "bytecodes/Test", "testdefaultvalueAndwithfield", "()V", false);
		Label l1 = new Label();
		mv.visitLabel(l1);
		mv.visitLineNumber(29, l1);
		mv.visitMethodInsn(INVOKESTATIC, "bytecodes/Test", "testdefaultvalueAbstractInterface", "()V", false);
		Label l2 = new Label();
		mv.visitLabel(l2);
		mv.visitLineNumber(30, l2);
		mv.visitMethodInsn(INVOKESTATIC, "bytecodes/Test", "testnewValue", "()V", false);
		Label l3 = new Label();
		mv.visitLabel(l3);
		mv.visitLineNumber(31, l3);
		mv.visitMethodInsn(INVOKESTATIC, "bytecodes/Test", "testdefaultvalueNotValue", "()V", false);
		Label l4 = new Label();
		mv.visitLabel(l4);
		mv.visitLineNumber(32, l4);
		mv.visitMethodInsn(INVOKESTATIC, "bytecodes/Test", "testwithfieldNotValue", "()V", false);
		Label l5 = new Label();
		mv.visitLabel(l5);
		mv.visitLineNumber(33, l5);
		mv.visitMethodInsn(INVOKESTATIC, "bytecodes/Test", "testwithfieldStatic", "()V", false);
		Label l6 = new Label();
		mv.visitLabel(l6);
		mv.visitLineNumber(34, l6);
		mv.visitMethodInsn(INVOKESTATIC, "bytecodes/Test", "testwithfieldNonFinal", "()V", false);
		Label l7 = new Label();
		mv.visitLabel(l7);
		mv.visitLineNumber(35, l7);
		mv.visitMethodInsn(INVOKESTATIC, "bytecodes/Test", "testwithfieldOutsideClass", "()V", false);
		Label l8 = new Label();
		mv.visitLabel(l8);
		mv.visitLineNumber(36, l8);
		mv.visitMethodInsn(INVOKESTATIC, "bytecodes/Test", "testwithfieldNullPtr", "()V", false);
		Label l9 = new Label();
		mv.visitLabel(l9);
		mv.visitLineNumber(37, l9);
		mv.visitMethodInsn(INVOKESTATIC, "bytecodes/Test", "testputfieldValue", "()V", false);
		Label l10 = new Label();
		mv.visitLabel(l10);
		mv.visitLineNumber(38, l10);
		mv.visitInsn(RETURN);
		Label l11 = new Label();
		mv.visitLabel(l11);
		mv.visitLocalVariable("args", "[Ljava/lang/String;", null, l0, l11, 0);
		mv.visitMaxs(0, 1);
		mv.visitEnd();
		}
		{
		mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "testdefaultvalueAndwithfield", "()V", null, null);
		mv.visitCode();
		Label l0 = new Label();
		mv.visitLabel(l0);
		mv.visitLineNumber(40, l0);
		mv.visitLdcInsn(new Double("4.0"));
		mv.visitInsn(ACONST_NULL);
		mv.visitIntInsn(BIPUSH, 40);
		mv.visitMethodInsn(INVOKESTATIC, "bytecodes/Value", "makeValue", "(DLbytecodes/Value;I)Lbytecodes/Value;", false);
		mv.visitVarInsn(ASTORE, 0);
		Label l1 = new Label();
		mv.visitLabel(l1);
		mv.visitLineNumber(41, l1);
		mv.visitLdcInsn(new Double("2.8"));
		mv.visitVarInsn(ALOAD, 0);
		mv.visitIntInsn(BIPUSH, 95);
		mv.visitMethodInsn(INVOKESTATIC, "bytecodes/Value", "makeValue", "(DLbytecodes/Value;I)Lbytecodes/Value;", false);
		mv.visitVarInsn(ASTORE, 1);
		Label l2 = new Label();
		mv.visitLabel(l2);
		mv.visitLineNumber(42, l2);
		mv.visitFieldInsn(GETSTATIC, "bytecodes/Test", "$assertionsDisabled", "Z");
		Label l3 = new Label();
		mv.visitJumpInsn(IFNE, l3);
		mv.visitVarInsn(ALOAD, 0);
		mv.visitFieldInsn(GETFIELD, "bytecodes/Value", "theDouble", "D");
		mv.visitLdcInsn(new Double("4.0"));
		mv.visitInsn(DCMPL);
		mv.visitJumpInsn(IFEQ, l3);
		mv.visitTypeInsn(NEW, "java/lang/AssertionError");
		mv.visitInsn(DUP);
		mv.visitMethodInsn(INVOKESPECIAL, "java/lang/AssertionError", "<init>", "()V", false);
		mv.visitInsn(ATHROW);
		mv.visitLabel(l3);
		mv.visitLineNumber(43, l3);
		mv.visitFrame(Opcodes.F_APPEND,2, new Object[] {"bytecodes/Value", "bytecodes/Value"}, 0, null);
		mv.visitFieldInsn(GETSTATIC, "bytecodes/Test", "$assertionsDisabled", "Z");
		Label l4 = new Label();
		mv.visitJumpInsn(IFNE, l4);
		mv.visitVarInsn(ALOAD, 0);
		mv.visitFieldInsn(GETFIELD, "bytecodes/Value", "theObject", "Lbytecodes/Value;");
		mv.visitJumpInsn(IFNULL, l4);
		mv.visitTypeInsn(NEW, "java/lang/AssertionError");
		mv.visitInsn(DUP);
		mv.visitMethodInsn(INVOKESPECIAL, "java/lang/AssertionError", "<init>", "()V", false);
		mv.visitInsn(ATHROW);
		mv.visitLabel(l4);
		mv.visitLineNumber(44, l4);
		mv.visitFrame(Opcodes.F_SAME, 0, null, 0, null);
		mv.visitFieldInsn(GETSTATIC, "bytecodes/Test", "$assertionsDisabled", "Z");
		Label l5 = new Label();
		mv.visitJumpInsn(IFNE, l5);
		mv.visitVarInsn(ALOAD, 0);
		mv.visitFieldInsn(GETFIELD, "bytecodes/Value", "theSingle", "I");
		mv.visitIntInsn(BIPUSH, 40);
		mv.visitJumpInsn(IF_ICMPEQ, l5);
		mv.visitTypeInsn(NEW, "java/lang/AssertionError");
		mv.visitInsn(DUP);
		mv.visitMethodInsn(INVOKESPECIAL, "java/lang/AssertionError", "<init>", "()V", false);
		mv.visitInsn(ATHROW);
		mv.visitLabel(l5);
		mv.visitLineNumber(45, l5);
		mv.visitFrame(Opcodes.F_SAME, 0, null, 0, null);
		mv.visitFieldInsn(GETSTATIC, "bytecodes/Test", "$assertionsDisabled", "Z");
		Label l6 = new Label();
		mv.visitJumpInsn(IFNE, l6);
		mv.visitVarInsn(ALOAD, 1);
		mv.visitFieldInsn(GETFIELD, "bytecodes/Value", "theDouble", "D");
		mv.visitLdcInsn(new Double("2.8"));
		mv.visitInsn(DCMPL);
		mv.visitJumpInsn(IFEQ, l6);
		mv.visitTypeInsn(NEW, "java/lang/AssertionError");
		mv.visitInsn(DUP);
		mv.visitMethodInsn(INVOKESPECIAL, "java/lang/AssertionError", "<init>", "()V", false);
		mv.visitInsn(ATHROW);
		mv.visitLabel(l6);
		mv.visitLineNumber(46, l6);
		mv.visitFrame(Opcodes.F_SAME, 0, null, 0, null);
		mv.visitFieldInsn(GETSTATIC, "bytecodes/Test", "$assertionsDisabled", "Z");
		Label l7 = new Label();
		mv.visitJumpInsn(IFNE, l7);
		mv.visitVarInsn(ALOAD, 1);
		mv.visitFieldInsn(GETFIELD, "bytecodes/Value", "theObject", "Lbytecodes/Value;");
		mv.visitVarInsn(ALOAD, 0);
		mv.visitJumpInsn(IF_ACMPEQ, l7);
		mv.visitTypeInsn(NEW, "java/lang/AssertionError");
		mv.visitInsn(DUP);
		mv.visitMethodInsn(INVOKESPECIAL, "java/lang/AssertionError", "<init>", "()V", false);
		mv.visitInsn(ATHROW);
		mv.visitLabel(l7);
		mv.visitLineNumber(47, l7);
		mv.visitFrame(Opcodes.F_SAME, 0, null, 0, null);
		mv.visitFieldInsn(GETSTATIC, "bytecodes/Test", "$assertionsDisabled", "Z");
		Label l8 = new Label();
		mv.visitJumpInsn(IFNE, l8);
		mv.visitVarInsn(ALOAD, 1);
		mv.visitFieldInsn(GETFIELD, "bytecodes/Value", "theSingle", "I");
		mv.visitIntInsn(BIPUSH, 95);
		mv.visitJumpInsn(IF_ICMPEQ, l8);
		mv.visitTypeInsn(NEW, "java/lang/AssertionError");
		mv.visitInsn(DUP);
		mv.visitMethodInsn(INVOKESPECIAL, "java/lang/AssertionError", "<init>", "()V", false);
		mv.visitInsn(ATHROW);
		mv.visitLabel(l8);
		mv.visitLineNumber(48, l8);
		mv.visitFrame(Opcodes.F_SAME, 0, null, 0, null);
		mv.visitInsn(RETURN);
		Label l9 = new Label();
		mv.visitLabel(l9);
		mv.visitLocalVariable("a2", "Lbytecodes/Value;", null, l1, l9, 0);
		mv.visitLocalVariable("v", "Lbytecodes/Value;", null, l2, l9, 1);
		mv.visitMaxs(4, 2);
		mv.visitEnd();
		}
		{
		mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "testdefaultvalueAbstractInterface", "()V", null, null);
		mv.visitCode();
		Label l0 = new Label();
		Label l1 = new Label();
		mv.visitTryCatchBlock(l0, l1, l1, "java/lang/InstantiationError");
		Label l2 = new Label();
		Label l3 = new Label();
		mv.visitTryCatchBlock(l2, l3, l3, "java/lang/InstantiationError");
		mv.visitLabel(l0);
		mv.visitLineNumber(52, l0);
		mv.visitMethodInsn(INVOKESTATIC, "bytecodes/Abstract", "defaultvalue", "()V", false);
		Label l4 = new Label();
		mv.visitLabel(l4);
		mv.visitLineNumber(53, l4);
		mv.visitFieldInsn(GETSTATIC, "bytecodes/Test", "$assertionsDisabled", "Z");
		mv.visitJumpInsn(IFNE, l2);
		mv.visitTypeInsn(NEW, "java/lang/AssertionError");
		mv.visitInsn(DUP);
		mv.visitMethodInsn(INVOKESPECIAL, "java/lang/AssertionError", "<init>", "()V", false);
		mv.visitInsn(ATHROW);
		mv.visitLabel(l1);
		mv.visitLineNumber(54, l1);
		mv.visitFrame(Opcodes.F_SAME1, 0, null, 1, new Object[] {"java/lang/InstantiationError"});
		mv.visitVarInsn(ASTORE, 0);
		mv.visitLabel(l2);
		mv.visitLineNumber(57, l2);
		mv.visitFrame(Opcodes.F_SAME, 0, null, 0, null);
		mv.visitMethodInsn(INVOKESTATIC, "bytecodes/Interface", "defaultvalue", "()V", false);
		Label l5 = new Label();
		mv.visitLabel(l5);
		mv.visitLineNumber(58, l5);
		mv.visitFieldInsn(GETSTATIC, "bytecodes/Test", "$assertionsDisabled", "Z");
		Label l6 = new Label();
		mv.visitJumpInsn(IFNE, l6);
		mv.visitTypeInsn(NEW, "java/lang/AssertionError");
		mv.visitInsn(DUP);
		mv.visitMethodInsn(INVOKESPECIAL, "java/lang/AssertionError", "<init>", "()V", false);
		mv.visitInsn(ATHROW);
		mv.visitLabel(l3);
		mv.visitLineNumber(59, l3);
		mv.visitFrame(Opcodes.F_SAME1, 0, null, 1, new Object[] {"java/lang/InstantiationError"});
		mv.visitVarInsn(ASTORE, 0);
		mv.visitLabel(l6);
		mv.visitLineNumber(60, l6);
		mv.visitFrame(Opcodes.F_SAME, 0, null, 0, null);
		mv.visitInsn(RETURN);
		mv.visitMaxs(2, 1);
		mv.visitEnd();
		}
		{
		mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "testnewValue", "()V", null, null);
		mv.visitCode();
		Label l0 = new Label();
		Label l1 = new Label();
		Label l2 = new Label();
		mv.visitTryCatchBlock(l0, l1, l2, "java/lang/InstantiationError");
		mv.visitLabel(l0);
		mv.visitLineNumber(64, l0);
		mv.visitTypeInsn(NEW, "bytecodes/Value");
		mv.visitInsn(DUP);
		mv.visitMethodInsn(INVOKESPECIAL, "bytecodes/Value", "<init>", "()V", false);
		mv.visitVarInsn(ASTORE, 0);
		mv.visitLabel(l1);
		mv.visitLineNumber(65, l1);
		Label l3 = new Label();
		mv.visitJumpInsn(GOTO, l3);
		mv.visitLabel(l2);
		mv.visitFrame(Opcodes.F_SAME1, 0, null, 1, new Object[] {"java/lang/InstantiationError"});
		mv.visitVarInsn(ASTORE, 0);
		mv.visitLabel(l3);
		mv.visitLineNumber(66, l3);
		mv.visitFrame(Opcodes.F_SAME, 0, null, 0, null);
		mv.visitInsn(RETURN);
		mv.visitMaxs(2, 1);
		mv.visitEnd();
		}
		{
		mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "testdefaultvalueNotValue", "()V", null, null);
		mv.visitCode();
		Label l0 = new Label();
		Label l1 = new Label();
		mv.visitTryCatchBlock(l0, l1, l1, "java/lang/Throwable");
		mv.visitLabel(l0);
		mv.visitLineNumber(70, l0);
		mv.visitMethodInsn(INVOKESTATIC, "bytecodes/NotValue", "defaultvalue", "()Lbytecodes/NotValue;", false);
		mv.visitVarInsn(ASTORE, 0);
		Label l2 = new Label();
		mv.visitLabel(l2);
		mv.visitLineNumber(71, l2);
		mv.visitFieldInsn(GETSTATIC, "bytecodes/Test", "$assertionsDisabled", "Z");
		Label l3 = new Label();
		mv.visitJumpInsn(IFNE, l3);
		mv.visitTypeInsn(NEW, "java/lang/AssertionError");
		mv.visitInsn(DUP);
		mv.visitMethodInsn(INVOKESPECIAL, "java/lang/AssertionError", "<init>", "()V", false);
		mv.visitInsn(ATHROW);
		mv.visitLabel(l1);
		mv.visitLineNumber(72, l1);
		mv.visitFrame(Opcodes.F_SAME1, 0, null, 1, new Object[] {"java/lang/Throwable"});
		mv.visitVarInsn(ASTORE, 0);
		Label l4 = new Label();
		mv.visitLabel(l4);
		mv.visitLineNumber(73, l4);
		mv.visitFieldInsn(GETSTATIC, "bytecodes/Test", "$assertionsDisabled", "Z");
		mv.visitJumpInsn(IFNE, l3);
		mv.visitVarInsn(ALOAD, 0);
		mv.visitTypeInsn(INSTANCEOF, "java/lang/IncompatibleClassChangeError");
		mv.visitJumpInsn(IFNE, l3);
		mv.visitTypeInsn(NEW, "java/lang/AssertionError");
		mv.visitInsn(DUP);
		mv.visitMethodInsn(INVOKESPECIAL, "java/lang/AssertionError", "<init>", "()V", false);
		mv.visitInsn(ATHROW);
		mv.visitLabel(l3);
		mv.visitLineNumber(75, l3);
		mv.visitFrame(Opcodes.F_SAME, 0, null, 0, null);
		mv.visitInsn(RETURN);
		mv.visitLocalVariable("nv", "Lbytecodes/NotValue;", null, l2, l1, 0);
		mv.visitLocalVariable("e", "Ljava/lang/Throwable;", null, l4, l3, 0);
		mv.visitMaxs(2, 1);
		mv.visitEnd();
		}
		{
		mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "testwithfieldNotValue", "()V", null, null);
		mv.visitCode();
		Label l0 = new Label();
		Label l1 = new Label();
		mv.visitTryCatchBlock(l0, l1, l1, "java/lang/Throwable");
		Label l2 = new Label();
		mv.visitLabel(l2);
		mv.visitLineNumber(78, l2);
		mv.visitTypeInsn(NEW, "bytecodes/NotValue");
		mv.visitInsn(DUP);
		mv.visitMethodInsn(INVOKESPECIAL, "bytecodes/NotValue", "<init>", "()V", false);
		mv.visitVarInsn(ASTORE, 0);
		mv.visitLabel(l0);
		mv.visitLineNumber(80, l0);
		mv.visitVarInsn(ALOAD, 0);
		mv.visitMethodInsn(INVOKEVIRTUAL, "bytecodes/NotValue", "withfield", "()Lbytecodes/NotValue;", false);
		mv.visitVarInsn(ASTORE, 0);
		Label l3 = new Label();
		mv.visitLabel(l3);
		mv.visitLineNumber(81, l3);
		mv.visitFieldInsn(GETSTATIC, "bytecodes/Test", "$assertionsDisabled", "Z");
		Label l4 = new Label();
		mv.visitJumpInsn(IFNE, l4);
		mv.visitTypeInsn(NEW, "java/lang/AssertionError");
		mv.visitInsn(DUP);
		mv.visitMethodInsn(INVOKESPECIAL, "java/lang/AssertionError", "<init>", "()V", false);
		mv.visitInsn(ATHROW);
		mv.visitLabel(l1);
		mv.visitLineNumber(82, l1);
		mv.visitFrame(Opcodes.F_FULL, 1, new Object[] {"bytecodes/NotValue"}, 1, new Object[] {"java/lang/Throwable"});
		mv.visitVarInsn(ASTORE, 1);
		Label l5 = new Label();
		mv.visitLabel(l5);
		mv.visitLineNumber(83, l5);
		mv.visitFieldInsn(GETSTATIC, "bytecodes/Test", "$assertionsDisabled", "Z");
		mv.visitJumpInsn(IFNE, l4);
		mv.visitVarInsn(ALOAD, 1);
		mv.visitTypeInsn(INSTANCEOF, "java/lang/IncompatibleClassChangeError");
		mv.visitJumpInsn(IFNE, l4);
		mv.visitTypeInsn(NEW, "java/lang/AssertionError");
		mv.visitInsn(DUP);
		mv.visitMethodInsn(INVOKESPECIAL, "java/lang/AssertionError", "<init>", "()V", false);
		mv.visitInsn(ATHROW);
		mv.visitLabel(l4);
		mv.visitLineNumber(85, l4);
		mv.visitFrame(Opcodes.F_SAME, 0, null, 0, null);
		mv.visitInsn(RETURN);
		Label l6 = new Label();
		mv.visitLabel(l6);
		mv.visitLocalVariable("nv", "Lbytecodes/NotValue;", null, l0, l6, 0);
		mv.visitLocalVariable("e", "Ljava/lang/Throwable;", null, l5, l4, 1);
		mv.visitMaxs(2, 2);
		mv.visitEnd();
		}
		{
		mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "testwithfieldStatic", "()V", null, null);
		mv.visitCode();
		Label l0 = new Label();
		Label l1 = new Label();
		mv.visitTryCatchBlock(l0, l1, l1, "java/lang/IncompatibleClassChangeError");
		mv.visitLabel(l0);
		mv.visitLineNumber(89, l0);
		mv.visitMethodInsn(INVOKESTATIC, "bytecodes/Value", "witherStatic", "()V", false);
		Label l2 = new Label();
		mv.visitLabel(l2);
		mv.visitLineNumber(90, l2);
		mv.visitFieldInsn(GETSTATIC, "bytecodes/Test", "$assertionsDisabled", "Z");
		Label l3 = new Label();
		mv.visitJumpInsn(IFNE, l3);
		mv.visitTypeInsn(NEW, "java/lang/AssertionError");
		mv.visitInsn(DUP);
		mv.visitMethodInsn(INVOKESPECIAL, "java/lang/AssertionError", "<init>", "()V", false);
		mv.visitInsn(ATHROW);
		mv.visitLabel(l1);
		mv.visitLineNumber(91, l1);
		mv.visitFrame(Opcodes.F_SAME1, 0, null, 1, new Object[] {"java/lang/IncompatibleClassChangeError"});
		mv.visitVarInsn(ASTORE, 0);
		mv.visitLabel(l3);
		mv.visitLineNumber(92, l3);
		mv.visitFrame(Opcodes.F_SAME, 0, null, 0, null);
		mv.visitInsn(RETURN);
		mv.visitMaxs(2, 1);
		mv.visitEnd();
		}
		{
		mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "testwithfieldNonFinal", "()V", null, null);
		mv.visitCode();
		Label l0 = new Label();
		Label l1 = new Label();
		mv.visitTryCatchBlock(l0, l1, l1, "java/lang/IllegalAccessError");
		mv.visitLabel(l0);
		mv.visitLineNumber(96, l0);
		mv.visitInsn(ICONST_0);
		mv.visitMethodInsn(INVOKESTATIC, "bytecodes/AlmostValue", "makeAlmostValue", "(I)Lbytecodes/AlmostValue;", false);
		mv.visitVarInsn(ASTORE, 0);
		Label l2 = new Label();
		mv.visitLabel(l2);
		mv.visitLineNumber(97, l2);
		mv.visitFieldInsn(GETSTATIC, "bytecodes/Test", "$assertionsDisabled", "Z");
		Label l3 = new Label();
		mv.visitJumpInsn(IFNE, l3);
		mv.visitTypeInsn(NEW, "java/lang/AssertionError");
		mv.visitInsn(DUP);
		mv.visitMethodInsn(INVOKESPECIAL, "java/lang/AssertionError", "<init>", "()V", false);
		mv.visitInsn(ATHROW);
		mv.visitLabel(l1);
		mv.visitLineNumber(98, l1);
		mv.visitFrame(Opcodes.F_SAME1, 0, null, 1, new Object[] {"java/lang/IllegalAccessError"});
		mv.visitVarInsn(ASTORE, 0);
		mv.visitLabel(l3);
		mv.visitLineNumber(99, l3);
		mv.visitFrame(Opcodes.F_SAME, 0, null, 0, null);
		mv.visitInsn(RETURN);
		mv.visitLocalVariable("av", "Lbytecodes/AlmostValue;", null, l2, l1, 0);
		mv.visitMaxs(2, 1);
		mv.visitEnd();
		}
		{
		mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "testwithfieldOutsideClass", "()V", null, null);
		mv.visitCode();
		Label l0 = new Label();
		Label l1 = new Label();
		mv.visitTryCatchBlock(l0, l1, l1, "java/lang/IllegalAccessError");
		Label l2 = new Label();
		mv.visitLabel(l2);
		mv.visitLineNumber(102, l2);
		mv.visitInsn(DCONST_0);
		mv.visitInsn(ACONST_NULL);
		mv.visitInsn(ICONST_0);
		mv.visitMethodInsn(INVOKESTATIC, "bytecodes/Value", "makeValue", "(DLbytecodes/Value;I)Lbytecodes/Value;", false);
		mv.visitVarInsn(ASTORE, 0);
		mv.visitLabel(l0);
		mv.visitLineNumber(104, l0);
		mv.visitVarInsn(ALOAD, 0);
		mv.visitIntInsn(BIPUSH, 40);
		mv.visitFieldInsn(WITHFIELD, "bytecodes/Value", "theSingle", "I"); /* WITHFIELD */
		Label l3 = new Label();
		mv.visitLabel(l3);
		mv.visitLineNumber(105, l3);
		mv.visitFieldInsn(GETSTATIC, "bytecodes/Test", "$assertionsDisabled", "Z");
		Label l4 = new Label();
		mv.visitJumpInsn(IFNE, l4);
		mv.visitTypeInsn(NEW, "java/lang/AssertionError");
		mv.visitInsn(DUP);
		mv.visitMethodInsn(INVOKESPECIAL, "java/lang/AssertionError", "<init>", "()V", false);
		mv.visitInsn(ATHROW);
		mv.visitLabel(l1);
		mv.visitLineNumber(106, l1);
		mv.visitFrame(Opcodes.F_FULL, 1, new Object[] {"bytecodes/Value"}, 1, new Object[] {"java/lang/IllegalAccessError"});
		mv.visitVarInsn(ASTORE, 1);
		mv.visitLabel(l4);
		mv.visitLineNumber(107, l4);
		mv.visitFrame(Opcodes.F_SAME, 0, null, 0, null);
		mv.visitInsn(RETURN);
		Label l5 = new Label();
		mv.visitLabel(l5);
		mv.visitLocalVariable("v", "Lbytecodes/Value;", null, l0, l5, 0);
		mv.visitMaxs(4, 2);
		mv.visitEnd();
		}
		{
		mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "testwithfieldNullPtr", "()V", null, null);
		mv.visitCode();
		Label l0 = new Label();
		Label l1 = new Label();
		mv.visitTryCatchBlock(l0, l1, l1, "java/lang/NullPointerException");
		Label l2 = new Label();
		mv.visitLabel(l2);
		mv.visitLineNumber(110, l2);
		mv.visitInsn(ACONST_NULL);
		mv.visitVarInsn(ASTORE, 0);
		mv.visitLabel(l0);
		mv.visitLineNumber(112, l0);
		mv.visitVarInsn(ALOAD, 0);
		mv.visitIntInsn(BIPUSH, 40);
		mv.visitFieldInsn(WITHFIELD, "bytecodes/Value", "theSingle", "I"); /* WITHFIELD */
		Label l3 = new Label();
		mv.visitLabel(l3);
		mv.visitLineNumber(113, l3);
		mv.visitFieldInsn(GETSTATIC, "bytecodes/Test", "$assertionsDisabled", "Z");
		Label l4 = new Label();
		mv.visitJumpInsn(IFNE, l4);
		mv.visitTypeInsn(NEW, "java/lang/AssertionError");
		mv.visitInsn(DUP);
		mv.visitMethodInsn(INVOKESPECIAL, "java/lang/AssertionError", "<init>", "()V", false);
		mv.visitInsn(ATHROW);
		mv.visitLabel(l1);
		mv.visitLineNumber(114, l1);
		mv.visitFrame(Opcodes.F_FULL, 1, new Object[] {"bytecodes/Value"}, 1, new Object[] {"java/lang/NullPointerException"});
		mv.visitVarInsn(ASTORE, 1);
		mv.visitLabel(l4);
		mv.visitLineNumber(115, l4);
		mv.visitFrame(Opcodes.F_SAME, 0, null, 0, null);
		mv.visitInsn(RETURN);
		Label l5 = new Label();
		mv.visitLabel(l5);
		mv.visitLocalVariable("v", "Lbytecodes/Value;", null, l0, l5, 0);
		mv.visitMaxs(2, 2);
		mv.visitEnd();
		}
		{
		mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "testputfieldValue", "()V", null, null);
		mv.visitCode();
		Label l0 = new Label();
		Label l1 = new Label();
		mv.visitTryCatchBlock(l0, l1, l1, "java/lang/IncompatibleClassChangeError");
		Label l2 = new Label();
		mv.visitLabel(l2);
		mv.visitLineNumber(118, l2);
		mv.visitInsn(DCONST_0);
		mv.visitInsn(ACONST_NULL);
		mv.visitInsn(ICONST_0);
		mv.visitMethodInsn(INVOKESTATIC, "bytecodes/Value", "makeValue", "(DLbytecodes/Value;I)Lbytecodes/Value;", false);
		mv.visitVarInsn(ASTORE, 0);
		mv.visitLabel(l0);
		mv.visitLineNumber(120, l0);
		mv.visitVarInsn(ALOAD, 0);
		mv.visitInsn(ICONST_3);
		mv.visitFieldInsn(PUTFIELD, "bytecodes/Value", "theSingle", "I");
		Label l3 = new Label();
		mv.visitLabel(l3);
		mv.visitLineNumber(121, l3);
		mv.visitFieldInsn(GETSTATIC, "bytecodes/Test", "$assertionsDisabled", "Z");
		Label l4 = new Label();
		mv.visitJumpInsn(IFNE, l4);
		mv.visitTypeInsn(NEW, "java/lang/AssertionError");
		mv.visitInsn(DUP);
		mv.visitMethodInsn(INVOKESPECIAL, "java/lang/AssertionError", "<init>", "()V", false);
		mv.visitInsn(ATHROW);
		mv.visitLabel(l1);
		mv.visitLineNumber(122, l1);
		mv.visitFrame(Opcodes.F_FULL, 1, new Object[] {"bytecodes/Value"}, 1, new Object[] {"java/lang/IncompatibleClassChangeError"});
		mv.visitVarInsn(ASTORE, 1);
		mv.visitLabel(l4);
		mv.visitLineNumber(123, l4);
		mv.visitFrame(Opcodes.F_SAME, 0, null, 0, null);
		mv.visitInsn(RETURN);
		Label l5 = new Label();
		mv.visitLabel(l5);
		mv.visitLocalVariable("v", "Lbytecodes/Value;", null, l0, l5, 0);
		mv.visitMaxs(4, 2);
		mv.visitEnd();
		}
		cw.visitEnd();

		return cw.toByteArray();
	}
}
