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
package j9vm.test.multipleorphans;

import static org.objectweb.asm.Opcodes.*;

import java.lang.invoke.CallSite;
import java.lang.invoke.ConstantCallSite;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.lang.invoke.MethodHandles.Lookup;

import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.Handle;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.FieldVisitor;

/**
 * This class is used to generate classfile data containing invokedynamic bytecode using ASM library.
 * 
 * @author Ashutosh
 */
public class InvokeDynamicTestGenerator {
	public static final String GENERATED_CLASS_NAME = "j9vm.test.multipleorphans.InvokeDynamicTest";
	
	public static int addIntegers(int i1, int i2) {
		return i1 + i2;
	}

	/* Bootstrap method used by invokedynamic bytecode in the generated class file */
	public static CallSite bootstrapAddInts(Lookup caller, String name, MethodType type) throws Throwable {
		MethodHandles.Lookup lookup = MethodHandles.lookup();
		Class<?> thisClass = lookup.lookupClass();
		MethodHandle addIntegers = lookup.findStatic(thisClass, "addIntegers", type);
		return new ConstantCallSite(addIntegers);
	}
	
	public static byte[] generateClassData(int value) {
		ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES);
		MethodVisitor mv;
		FieldVisitor fv;

		cw.visit(V1_7, ACC_PUBLIC, GENERATED_CLASS_NAME.replace('.', '/'), null, "java/lang/Object", null);
		{
			fv = cw.visitField(ACC_STATIC, "random", "I", null, null);
			fv.visitEnd();
		}
		{
			mv = cw.visitMethod(0, "<init>", "()V", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V");
			mv.visitInsn(RETURN);
			mv.visitMaxs(1, 1);
			mv.visitEnd();
		}
		{
			mv = cw.visitMethod(ACC_PUBLIC | ACC_STATIC, "addIntegers", "(II)I", null, null);
			mv.visitCode();
			//mv.visitIntInsn(BIPUSH, value);
			mv.visitLdcInsn(new Integer(value));
			mv.visitFieldInsn(PUTSTATIC, GENERATED_CLASS_NAME.replace('.', '/'), "random", "I");
			mv.visitVarInsn(ILOAD, 0);
			mv.visitVarInsn(ILOAD, 1);
			MethodType mt1 = MethodType.methodType(int.class, int.class, int.class);
			MethodType mt2 = MethodType.methodType(CallSite.class, MethodHandles.Lookup.class, String.class, MethodType.class);
			Handle handle = new Handle(H_INVOKESTATIC, InvokeDynamicTestGenerator.class.getCanonicalName().replace('.', '/'), "bootstrapAddInts", mt2.toMethodDescriptorString());
			mv.visitInvokeDynamicInsn("addInts", mt1.toMethodDescriptorString(), handle);
			mv.visitInsn(IRETURN);
			mv.visitMaxs(2, 2);
			mv.visitEnd();
		}
		cw.visitEnd();

		return cw.toByteArray();
	}
}
