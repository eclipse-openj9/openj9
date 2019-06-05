/*******************************************************************************
 * Copyright (c) 2015, 2019 IBM Corp. and others
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
package org.openj9.test.jsr335.interfacePrivateMethod;

import static org.objectweb.asm.Opcodes.ACC_PUBLIC;
import static org.objectweb.asm.Opcodes.ACC_SUPER;
import static org.objectweb.asm.Opcodes.ALOAD;
import static org.objectweb.asm.Opcodes.INVOKESPECIAL;
import static org.objectweb.asm.Opcodes.RETURN;
import static org.objectweb.asm.Opcodes.V1_8;
import static org.objectweb.asm.Opcodes.INVOKEVIRTUAL;
import static org.objectweb.asm.Opcodes.GETSTATIC;

import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.MethodVisitor;

/* This class is used to generate bytecodes for the required classes which implement our required interfaces from their respective ASM dumps,  
 * and return the bytecodes to the calling classloaders in the form of byte arrays.
 */
public class GenerateImplClass {

	/**
	 * Generate bytecodes for class named by <code>className</code>, implementing interfaces stored in <code>implementedInterfaceName</code>.
	 * This class does not declare any methods.
	 * 
	 * @param className						Name of the class to be created
	 * @param implementatedInterfaceName	Array of interfaces which the class implements
	 * @return								byte[] containing bytecodes for the class created
	 */
	public static byte[] dump (String className, String[] implementatedInterfaceName) {

		ClassWriter cw = new ClassWriter(0);
		MethodVisitor mv;
		
		cw.visit(V1_8, ACC_PUBLIC + ACC_SUPER, className, null, "java/lang/Object", implementatedInterfaceName);
		{
			mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
			mv.visitInsn(RETURN);
			mv.visitMaxs(1, 1);
			mv.visitEnd();
		}
		cw.visitEnd();
		return cw.toByteArray();
	}
	
	/**
	 * Generate bytecodes for class named by <code>className</code>, implementing interfaces stored in <code>implementedInterfaceName</code>.
	 * This class contains a simple implementation of method <code>public void m()</code>.
	 * 
	 * @param className						Name of the class to be created
	 * @param implementatedInterfaceName	Array of interfaces which the class implements
	 * @return								byte[] containing bytecodes for the class created
	 */
	public static byte[] dumpWithDefaultImplementation (String className, String[] implementatedInterfaceName) {

		ClassWriter cw = new ClassWriter(0);
		MethodVisitor mv;
		
		cw.visit(V1_8, ACC_PUBLIC + ACC_SUPER, className, null, "java/lang/Object", implementatedInterfaceName);
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
			mv = cw.visitMethod(ACC_PUBLIC, "m", "()V", null, null);
			mv.visitCode();
			mv.visitFieldInsn(GETSTATIC, "java/lang/System", "out", "Ljava/io/PrintStream;");
			mv.visitLdcInsn("Hello:");
			mv.visitMethodInsn(INVOKEVIRTUAL, "java/io/PrintStream", "println", "(Ljava/lang/String;)V", false);
			mv.visitInsn(RETURN);
			mv.visitMaxs(2, 1);
			mv.visitEnd();
		}
		cw.visitEnd();
		return cw.toByteArray();
	}

}
