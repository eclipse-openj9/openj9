/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;

import javassist.bytecode.ClassFile;
import javassist.bytecode.CodeIterator;
import javassist.bytecode.MethodInfo;
import javassist.bytecode.Opcode;

import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.MethodVisitor;

/**
 * This class generates a class containing a method 'sharedInvokers' having following 
 * bytecodes sharing same InterfaceMethodRef CP entry:
 * 	- invokespecial
 *  - invokeinterface
 *  - invokestatic
 *  
 * The interface implemented by the generated class is created by SharedInvokersTestIntfGenerator.
 * 
 * @author ashutosh
 */
public class SharedInvokersTestGenerator {

	/*
	 * This method is to workaround a limitation of ASM which prevents
	 * generating class file where invokeinterface and invoke[static|special]
	 * bytecodes share same constant pool entry.
	 * 
	 * To overcome this limitation, after the ASM has generated class data, the
	 * operand of invoke[static|special] bytecodes is modified to be same as the
	 * operand of invokeinterface bytecode. It uses javassist APIs to make this
	 * modification.
	 */
	public static byte[] modifyInvokerOperand(byte[] data) throws Exception {
		int cpIndex = 0;
		ByteArrayInputStream bais = new ByteArrayInputStream(data);
		ClassFile classFile = new ClassFile(new DataInputStream(bais));

		/* Record operand of invokeinterface in sharedInvokers() */
		MethodInfo methodInfo = classFile.getMethod("sharedInvokers");
		CodeIterator ci = methodInfo.getCodeAttribute().iterator();
		while (ci.hasNext()) {
			int bcIndex = ci.next();
			int bc = ci.byteAt(bcIndex);
			if (bc == Opcode.INVOKEINTERFACE) {
				/* found invokeinterface bytecode */
				cpIndex = ci.s16bitAt(bcIndex + 1);
				break;
			}
		}

		/*
		 * Modify operand of invokestatic/invokespecial to cpIndex recorded
		 * above
		 */
		ci = methodInfo.getCodeAttribute().iterator();
		while (ci.hasNext()) {
			int bcIndex = ci.next();
			int bc = ci.byteAt(bcIndex);
			if ((bc == Opcode.INVOKESPECIAL) || (bc == Opcode.INVOKESTATIC)) {
				/*
				 * found invokespecial or invokestatic bytecode; update the
				 * operand constant pool index
				 */
				ci.write16bit(cpIndex, bcIndex + 1);
			}
		}

		ByteArrayOutputStream byteStream = new ByteArrayOutputStream();
		DataOutputStream dataStream = new DataOutputStream(byteStream);
		classFile.write(dataStream);
		return byteStream.toByteArray();
	}

	public static byte[] generateClassData() throws Exception {
		ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES);
		MethodVisitor mv;
		String classInternalName = "com/ibm/j9/recreateclasscompare/testclasses/SharedInvokersTest";
		String intfInternalName = "com/ibm/j9/recreateclasscompare/testclasses/SharedInvokersTestIntf";

		cw.visit(V1_7, ACC_PUBLIC, classInternalName, null, "java/lang/Object",
				new String[] { intfInternalName });
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
			mv = cw.visitMethod(ACC_PUBLIC, "sharedInvokers", "()V", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKESPECIAL, intfInternalName, "foo",
					"()V", true);
			mv.visitVarInsn(ALOAD, 0);
			mv.visitVarInsn(ASTORE, 1);
			mv.visitVarInsn(ALOAD, 1);
			mv.visitMethodInsn(INVOKEINTERFACE, intfInternalName, "foo",
					"()V", true);
			mv.visitMethodInsn(INVOKESTATIC, intfInternalName, "foo",
					"()V", true);
			mv.visitInsn(RETURN);
			mv.visitMaxs(1, 2);
			mv.visitEnd();
		}
		{
			mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "main",
					"([Ljava/lang/String;)V", null, null);
			mv.visitCode();
			mv.visitTypeInsn(NEW, classInternalName);
			mv.visitInsn(DUP);
			mv.visitMethodInsn(INVOKESPECIAL, classInternalName, "<init>",
					"()V");
			mv.visitVarInsn(ASTORE, 1);
			mv.visitVarInsn(ALOAD, 1);
			mv.visitMethodInsn(INVOKEVIRTUAL, classInternalName,
					"sharedInvokers", "()V");
			mv.visitInsn(RETURN);
			mv.visitMaxs(2, 2);
			mv.visitEnd();
		}
		cw.visitEnd();
		byte[] classData = cw.toByteArray();
		classData = modifyInvokerOperand(classData);

		return classData;
	}
}
