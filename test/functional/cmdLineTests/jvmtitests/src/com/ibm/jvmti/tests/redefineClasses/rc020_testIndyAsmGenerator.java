/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

package com.ibm.jvmti.tests.redefineClasses;

import static org.objectweb.asm.Opcodes.V1_7;
import static org.objectweb.asm.Opcodes.ACC_PUBLIC;
import static org.objectweb.asm.Opcodes.ACC_SUPER;
import static org.objectweb.asm.Opcodes.ALOAD;
import static org.objectweb.asm.Opcodes.INVOKESPECIAL;
import static org.objectweb.asm.Opcodes.RETURN;
import static org.objectweb.asm.Opcodes.ACC_STATIC;
import static org.objectweb.asm.Opcodes.H_INVOKESTATIC;

import org.objectweb.asm.*;
import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.Handle;
import org.objectweb.asm.MethodVisitor;

import java.lang.invoke.CallSite;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodHandles.Lookup;
import java.lang.invoke.MethodType;

public class rc020_testIndyAsmGenerator {
	public static Class<?> testClass = rc020_Source.class;
	public static String testClassName = testClass.getSimpleName();
	public static String testMethodName = "testCall";
	public static String testMethodName2 = "testCall2";

	/* string constants for invokedynamic methods */
	private static String slashSeperatedPackage = "com/ibm/jvmti/tests/redefineClasses/";
	private static String bootstrapFullClassName = slashSeperatedPackage + rc020_testIndyBSMs.class.getSimpleName();
	private static String testFullClassName = slashSeperatedPackage + testClassName;

	private static String voidSignature = "()V";
	private static String initMethodName = "<init>";
	private static String bootstrap1MethodName = "bootstrapMethod1";
	private static String bootstrap2MethodName = "bootstrapMethod2";
	private static String bootstrap3MethodName = "bootstrapMethod3"; // extra argument
	private static String bootstrap4MethodName = "bootstrapMethod4"; // two extra arguments
	private static String bootstrap5MethodName = "bootstrapMethod5"; // extra arg with double slot
	private static String bootstrap6MethodName = "bootstrapMethod5"; // three args with double slot
	private static String bootstrap1Signature = 
		MethodType.methodType(CallSite.class, MethodHandles.Lookup.class, String.class, MethodType.class).toMethodDescriptorString();
	private static String bootstrap2Signature = bootstrap1Signature;	
	private static String bootstrap3Signature = 
		"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;I)Ljava/lang/invoke/CallSite;";
	private static String bootstrap4Signature = 
		"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;Ljava/lang/String;I)Ljava/lang/invoke/CallSite;";
	private static String bootstrap5Signature = 
		"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;Ljava/lang/String;D)Ljava/lang/invoke/CallSite;";
	private static String bootstrap6Signature = 
		"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;IDLjava/lang/String;)Ljava/lang/invoke/CallSite;";
	private static String bootstrapStringArg = "testtesttest";
	private static int bootstrapIntArg = 5;
	private static double bootstrapDoubleArg = 5.0;
	private static String bootstrapStringArg2 = "testtesttest2";
	private static int bootstrapIntArg2 = 7;
	private static double bootstrapDoubleArg2 = 7.4;
	private static String dynamicMethodName = "indyMethod";

	public byte[] generateExistingIndyClass() throws Throwable {
		ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES);
		cw.visit(V1_7, ACC_PUBLIC | ACC_SUPER, testFullClassName, null, "java/lang/Object", null);
		cw.visitSource(null, null);
		
		generateConstructorMethod(cw);
		generateEmptyStaticMethod(cw, testMethodName);
		generateEmptyStaticMethod(cw, testMethodName2);

		cw.visitEnd();

		return cw.toByteArray();
	}

	public byte[] generateBasicIndyClass() throws Throwable {
		ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES);
		cw.visit(V1_7, ACC_PUBLIC | ACC_SUPER, testFullClassName, null, "java/lang/Object", null);
		cw.visitSource(null, null);

		generateConstructorMethod(cw);
		generateOneIndyMethod(cw, testMethodName);
		generateEmptyStaticMethod(cw, testMethodName2);

		cw.visitEnd();

		return cw.toByteArray();
	}

	public byte[] generateBasicIndyClassWithArgs() throws Throwable {
		ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES);
		cw.visit(V1_7, ACC_PUBLIC | ACC_SUPER, testFullClassName, null, "java/lang/Object", null);
		cw.visitSource(null, null);

		generateConstructorMethod(cw);
		generateOneIndyMethodWithArgs(cw, testMethodName);
		generateEmptyStaticMethod(cw, testMethodName2);

		cw.visitEnd();

		return cw.toByteArray();
	}

	public byte[] generateBasicIndyClassWithTwoArgs() throws Throwable {
		ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES);
		cw.visit(V1_7, ACC_PUBLIC | ACC_SUPER, testFullClassName, null, "java/lang/Object", null);
		cw.visitSource(null, null);

		generateConstructorMethod(cw);
		generateOneIndyMethodWithTwoArgs(cw, testMethodName);
		generateEmptyStaticMethod(cw, testMethodName2);

		cw.visitEnd();

		return cw.toByteArray();
	}

	public byte[] generateBasicIndyClassWithDoubleSlotArgs() throws Throwable {
		ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES);
		cw.visit(V1_7, ACC_PUBLIC | ACC_SUPER, testFullClassName, null, "java/lang/Object", null);
		cw.visitSource(null, null);

		generateConstructorMethod(cw);
		generateOneIndyMethodWithDoubleSlotArgs(cw, testMethodName);
		generateEmptyStaticMethod(cw, testMethodName2);

		cw.visitEnd();

		return cw.toByteArray();
	}

	public byte[] generateBasicIndyClassWithDoubleSlotWithThreeArgs() throws Throwable {
		ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES);
		cw.visit(V1_7, ACC_PUBLIC | ACC_SUPER, testFullClassName, null, "java/lang/Object", null);
		cw.visitSource(null, null);

		generateConstructorMethod(cw);
		generateOneIndyMethodWithThreeArgsDoubleSlot(cw, testMethodName);
		generateEmptyStaticMethod(cw, testMethodName2);

		cw.visitEnd();

		return cw.toByteArray();
	}

	public byte[] generateBasicIndyClassWithDoubleSlotWithThreeArgs2() throws Throwable {
		ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES);
		cw.visit(V1_7, ACC_PUBLIC | ACC_SUPER, testFullClassName, null, "java/lang/Object", null);
		cw.visitSource(null, null);

		generateConstructorMethod(cw);
		generateOneIndyMethodWithThreeArgsDoubleSlot2(cw, testMethodName);
		generateEmptyStaticMethod(cw, testMethodName2);

		cw.visitEnd();

		return cw.toByteArray();
	}

	public byte[] generateTwoSameIndyCallClass() throws Throwable {
		ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES);
		cw.visit(V1_7, ACC_PUBLIC | ACC_SUPER, testFullClassName, null, "java/lang/Object", null);
		cw.visitSource(null, null);

		generateConstructorMethod(cw);
		generateTwoIndyMethodSame(cw, testMethodName);
		generateEmptyStaticMethod(cw, testMethodName2);

		cw.visitEnd();

		return cw.toByteArray();
	}

	public byte[] generateTwoDiffIndyCallClass() throws Throwable {
		ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES);
		cw.visit(V1_7, ACC_PUBLIC | ACC_SUPER, testFullClassName, null, "java/lang/Object", null);
		cw.visitSource(null, null);

		generateConstructorMethod(cw);
		generateTwoIndyMethodDiff(cw, testMethodName);
		generateEmptyStaticMethod(cw, testMethodName2);

		cw.visitEnd();

		return cw.toByteArray();
	}

	public byte[] generateTwoDiffIndyCallClassWithArgs() throws Throwable {
		ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES);
		cw.visit(V1_7, ACC_PUBLIC | ACC_SUPER, testFullClassName, null, "java/lang/Object", null);
		cw.visitSource(null, null);

		generateConstructorMethod(cw);
		generateTwoIndyMethodDiffWithArgs(cw, testMethodName);
		generateEmptyStaticMethod(cw, testMethodName2);

		cw.visitEnd();

		return cw.toByteArray();
	}

	public byte[] generateThreeIndyCallClass() throws Throwable {
		ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES);
		cw.visit(V1_7, ACC_PUBLIC | ACC_SUPER, testFullClassName, null, "java/lang/Object", null);
		cw.visitSource(null, null);

		generateConstructorMethod(cw);
		generateThreeIndyMethod(cw, testMethodName);
		generateEmptyStaticMethod(cw, testMethodName2);

		cw.visitEnd();

		return cw.toByteArray();
	}

	public byte[] generateMultiMethodClass() throws Throwable {
		ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES);
		cw.visit(V1_7, ACC_PUBLIC | ACC_SUPER, testFullClassName, null, "java/lang/Object", null);
		cw.visitSource(null, null);

		generateConstructorMethod(cw);
		generateOneIndyMethod2(cw, testMethodName2);
		generateOneIndyMethod(cw, testMethodName);

		cw.visitEnd();

		return cw.toByteArray();
	}

	private void generateConstructorMethod(ClassWriter cw) {
		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC, initMethodName, voidSignature, null, null);
		mv.visitCode();
		mv.visitVarInsn(ALOAD, 0);
		mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", initMethodName, voidSignature);
		mv.visitInsn(RETURN);
		mv.visitMaxs(-1, -1);
		mv.visitEnd();
	}

	private void generateEmptyStaticMethod(ClassWriter cw, String methodName) {
		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC | ACC_STATIC, methodName, voidSignature, null, null);
		mv.visitCode();
		mv.visitInsn(RETURN);
		mv.visitMaxs(-1, -1);
		mv.visitEnd();
	}

	private void generateOneIndyMethod(ClassWriter cw, String methodName) {
		Handle bootstrap1 = new Handle(H_INVOKESTATIC, bootstrapFullClassName, bootstrap1MethodName, bootstrap1Signature);

		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC | ACC_STATIC, methodName, voidSignature, null, null);
		mv.visitCode();
		mv.visitInvokeDynamicInsn(dynamicMethodName, voidSignature, bootstrap1);
		mv.visitInsn(RETURN);
		mv.visitMaxs(-1, -1);
		mv.visitEnd();
	} 

	private void generateOneIndyMethod2(ClassWriter cw, String methodName) {
		Handle bootstrap2 = new Handle(H_INVOKESTATIC, bootstrapFullClassName, bootstrap2MethodName, bootstrap2Signature);

		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC | ACC_STATIC, methodName, voidSignature, null, null);
		mv.visitCode();
		mv.visitInvokeDynamicInsn(dynamicMethodName, voidSignature, bootstrap2);
		mv.visitInsn(RETURN);
		mv.visitMaxs(-1, -1);
		mv.visitEnd();
	} 

	private void generateOneIndyMethodWithArgs(ClassWriter cw, String methodName) {
		Handle bootstrap3 = new Handle(H_INVOKESTATIC, bootstrapFullClassName, bootstrap3MethodName, bootstrap3Signature);

		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC | ACC_STATIC, methodName, voidSignature, null, null);
		mv.visitCode();
		mv.visitInvokeDynamicInsn(dynamicMethodName, voidSignature, bootstrap3, bootstrapIntArg);
		mv.visitInsn(RETURN);
		mv.visitMaxs(-1, -1);
		mv.visitEnd();
	} 

	private void generateOneIndyMethodWithTwoArgs(ClassWriter cw, String methodName) {
		Handle bootstrap4 = new Handle(H_INVOKESTATIC, bootstrapFullClassName, bootstrap4MethodName, bootstrap4Signature);

		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC | ACC_STATIC, methodName, voidSignature, null, null);
		mv.visitCode();
		mv.visitInvokeDynamicInsn(dynamicMethodName, voidSignature, bootstrap4, bootstrapStringArg, bootstrapIntArg);
		mv.visitInsn(RETURN);
		mv.visitMaxs(-1, -1);
		mv.visitEnd();
	} 

	private void generateOneIndyMethodWithDoubleSlotArgs(ClassWriter cw, String methodName) {
		Handle bootstrap5 = new Handle(H_INVOKESTATIC, bootstrapFullClassName, bootstrap5MethodName, bootstrap5Signature);

		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC | ACC_STATIC, methodName, voidSignature, null, null);
		mv.visitCode();
		mv.visitInvokeDynamicInsn(dynamicMethodName, voidSignature, bootstrap5, bootstrapStringArg, bootstrapDoubleArg);
		mv.visitInsn(RETURN);
		mv.visitMaxs(-1, -1);
		mv.visitEnd();
	} 

	private void generateOneIndyMethodWithThreeArgsDoubleSlot(ClassWriter cw, String methodName) {
		Handle bootstrap6 = new Handle(H_INVOKESTATIC, bootstrapFullClassName, bootstrap6MethodName, bootstrap6Signature);

		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC | ACC_STATIC, methodName, voidSignature, null, null);
		mv.visitCode();
		mv.visitInvokeDynamicInsn(dynamicMethodName, voidSignature, bootstrap6, bootstrapIntArg, bootstrapDoubleArg, bootstrapStringArg);
		mv.visitInsn(RETURN);
		mv.visitMaxs(-1, -1);
		mv.visitEnd();
	} 

	private void generateOneIndyMethodWithThreeArgsDoubleSlot2(ClassWriter cw, String methodName) {
		Handle bootstrap6 = new Handle(H_INVOKESTATIC, bootstrapFullClassName, bootstrap6MethodName, bootstrap6Signature);

		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC | ACC_STATIC, methodName, voidSignature, null, null);
		mv.visitCode();
		mv.visitInvokeDynamicInsn(dynamicMethodName, voidSignature, bootstrap6, bootstrapIntArg2, bootstrapDoubleArg2, bootstrapStringArg2);
		mv.visitInsn(RETURN);
		mv.visitMaxs(-1, -1);
		mv.visitEnd();
	} 

	private void generateTwoIndyMethodSame(ClassWriter cw, String methodName) {
		Handle bootstrap1 = new Handle(H_INVOKESTATIC, bootstrapFullClassName, bootstrap1MethodName, bootstrap1Signature);

		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC | ACC_STATIC, methodName, voidSignature, null, null);
		mv.visitCode();
		mv.visitInvokeDynamicInsn(dynamicMethodName, voidSignature, bootstrap1);
		mv.visitInvokeDynamicInsn(dynamicMethodName + "1", voidSignature, bootstrap1);
		mv.visitInsn(RETURN);
		mv.visitMaxs(-1, -1);
		mv.visitEnd();
	}

	private void generateTwoIndyMethodDiff(ClassWriter cw, String methodName) {
		Handle bootstrap1 = new Handle(H_INVOKESTATIC, bootstrapFullClassName, bootstrap1MethodName, bootstrap1Signature);
		Handle bootstrap2 = new Handle(H_INVOKESTATIC, bootstrapFullClassName, bootstrap2MethodName, bootstrap2Signature);

		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC | ACC_STATIC, methodName, voidSignature, null, null);
		mv.visitCode();
		mv.visitInvokeDynamicInsn(dynamicMethodName, voidSignature, bootstrap1);
		mv.visitInvokeDynamicInsn(dynamicMethodName + "1", voidSignature, bootstrap2);
		mv.visitInsn(RETURN);
		mv.visitMaxs(-1, -1);
		mv.visitEnd();
	}

	private void generateTwoIndyMethodDiffWithArgs(ClassWriter cw, String methodName) {
		Handle bootstrap1 = new Handle(H_INVOKESTATIC, bootstrapFullClassName, bootstrap1MethodName, bootstrap1Signature);
		Handle bootstrap3 = new Handle(H_INVOKESTATIC, bootstrapFullClassName, bootstrap3MethodName, bootstrap3Signature);

		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC | ACC_STATIC, methodName, voidSignature, null, null);
		mv.visitCode();
		mv.visitInvokeDynamicInsn(dynamicMethodName, voidSignature, bootstrap1);
		mv.visitInvokeDynamicInsn(dynamicMethodName + "1", voidSignature, bootstrap3, bootstrapIntArg);
		mv.visitInsn(RETURN);
		mv.visitMaxs(-1, -1);
		mv.visitEnd();
	}

	private void generateThreeIndyMethod(ClassWriter cw, String methodName) {
		Handle bootstrap1 = new Handle(H_INVOKESTATIC, bootstrapFullClassName, bootstrap1MethodName, bootstrap1Signature);
		Handle bootstrap3 = new Handle(H_INVOKESTATIC, bootstrapFullClassName, bootstrap3MethodName, bootstrap3Signature);

		MethodVisitor mv = cw.visitMethod(ACC_PUBLIC | ACC_STATIC, methodName, voidSignature, null, null);
		mv.visitCode();
		mv.visitInvokeDynamicInsn(dynamicMethodName, voidSignature, bootstrap1);
		mv.visitInvokeDynamicInsn(dynamicMethodName + "1", voidSignature, bootstrap3, bootstrapIntArg);
		mv.visitInvokeDynamicInsn(dynamicMethodName + "2", voidSignature, bootstrap1);
		mv.visitInsn(RETURN);
		mv.visitMaxs(-1, -1);
		mv.visitEnd();
	}
}
