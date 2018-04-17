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

import org.objectweb.asm.Attribute;
import org.objectweb.asm.ByteVector;
import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.FieldVisitor;
import org.objectweb.asm.MethodVisitor;

/**
 * This class generates a class which has ACC_SYNTHETIC flag set in class/field/method access_flags.
 * 
 * @author ashutosh
 */
public class SyntheticTestGenerator {
	static class SyntheticAttribute extends Attribute {
		public SyntheticAttribute() {
			super("Synthetic");
		}

		public boolean isUnknown() {
			return false;
		}

		protected ByteVector write(ClassWriter cw, byte[] code, int len,
				int maxStack, int maxLocals) {
			return new ByteVector(0);
		}
	}
	
	public static byte[] generateClassData() {
        ClassWriter cw = new ClassWriter(0);
        FieldVisitor fv;
        MethodVisitor mv;

		cw.visit(V1_1, ACC_PUBLIC, "com/ibm/j9/recreateclasscompare/testclasses/SyntheticTest", null, "java/lang/Object", null);
		
		cw.visitAttribute(new SyntheticAttribute());
		
		{
			fv = cw.visitField(ACC_PRIVATE, "i", "I", null, null);
			fv.visitAttribute(new SyntheticAttribute());
			fv.visitEnd();
		}

		{
			mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V");
			mv.visitInsn(RETURN);
			mv.visitMaxs(1, 1);
			mv.visitEnd();
		}
		{
			mv = cw.visitMethod(ACC_PUBLIC, "foo", "()V", null, null);
			mv.visitAttribute(new SyntheticAttribute());
			mv.visitInsn(RETURN);
			mv.visitMaxs(0, 1);
			mv.visitEnd();
		}
		
		cw.visitEnd();
		
        return cw.toByteArray();                
	}
}
