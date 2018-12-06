/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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
package org.openj9.test.booleans;

import org.objectweb.asm.*;
import static org.objectweb.asm.Opcodes.*;

class BooleanTestGenerator extends ClassLoader {
	
	static BooleanTestGenerator generator = null;
	
	static {
		generator = new BooleanTestGenerator();
	}
	
	static byte[] generateTestBoolTester() throws Exception {
		ClassWriter cw = new ClassWriter(0);
		FieldVisitor fv;
		MethodVisitor mv;
				
		cw.visit(52, ACC_PUBLIC + ACC_SUPER, "TestBoolTester", null, "java/lang/Object", null);
		mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "callReturnBooleanValue", "(I)I", null, null);
		mv.visitVarInsn(ILOAD, 0);
		mv.visitMethodInsn(INVOKESTATIC, "org/openj9/test/booleans/BooleanTest", "returnBooleanValue", "(I)Z", false);
		mv.visitInsn(IRETURN);
		mv.visitMaxs(1, 1);
		mv.visitEnd();
		
		mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "callSetBool", "(Lorg/openj9/test/booleans/BooleanTest$TestBool;I)I", null, null);
		mv.visitVarInsn(ALOAD, 0);
		mv.visitVarInsn(ILOAD, 1);
		mv.visitFieldInsn(PUTFIELD, "org/openj9/test/booleans/BooleanTest$TestBool", "z", "Z");
		mv.visitVarInsn(ALOAD, 0);
		mv.visitFieldInsn(GETFIELD, "org/openj9/test/booleans/BooleanTest$TestBool", "z", "Z");
		mv.visitInsn(IRETURN);
		mv.visitMaxs(2, 2);
		mv.visitEnd();

		mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "callSetBoolStatic", "(I)I", null, null);
		mv.visitVarInsn(ILOAD, 0);
		mv.visitFieldInsn(PUTSTATIC, "org/openj9/test/booleans/BooleanTest$TestBool", "zStatic", "Z");
		mv.visitFieldInsn(GETSTATIC, "org/openj9/test/booleans/BooleanTest$TestBool", "zStatic", "Z");
		mv.visitInsn(IRETURN);
		mv.visitMaxs(2, 1);
		mv.visitEnd();
		
		mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "returnBoolean", "(I)Z", null, null);
		mv.visitVarInsn(ILOAD, 0);
		mv.visitInsn(IRETURN);
		mv.visitMaxs(1, 1);
		mv.visitEnd();
		
		mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "testSMUnsafeBooleanGet", "(Lsun/misc/Unsafe;J)I", null, null);
		mv.visitVarInsn(ALOAD, 0);
		mv.visitInsn(ACONST_NULL);
		mv.visitVarInsn(LLOAD, 1);
		mv.visitMethodInsn(INVOKEVIRTUAL, "sun/misc/Unsafe", "getBoolean", "(Ljava/lang/Object;J)Z", false);
		mv.visitInsn(IRETURN);
		mv.visitMaxs(4, 3);
		
		mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "testSMUnsafeBooleanPut", "(Lsun/misc/Unsafe;JI)V", null, null);
		mv.visitVarInsn(ALOAD, 0);
		mv.visitInsn(ACONST_NULL);
		mv.visitVarInsn(LLOAD, 1);
		mv.visitVarInsn(ILOAD, 3);
		mv.visitMethodInsn(INVOKEVIRTUAL, "sun/misc/Unsafe", "putBoolean", "(Ljava/lang/Object;JZ)V", false);
		mv.visitInsn(RETURN);
		mv.visitMaxs(5, 4);
		cw.visitEnd();
		return cw.toByteArray();
	}
	
	static Class generateClass(String name, byte[] array) {
		return generator.defineClass(name, array, 0, array.length);
	}
}