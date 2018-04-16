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
package j9vm.test.intermediateclasscreate;

import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.MethodVisitor;

import static org.objectweb.asm.Opcodes.*;

public class ClassGenerator {
	public static byte[] generateClassData(int version, boolean isValidClassFormat) {
		ClassWriter cw = new ClassWriter(0);
		MethodVisitor mv;

		cw.visit(V1_7, ACC_PUBLIC, IntermediateClassCreateTest.SAMPLE_CLASS_NAME
				.replace('.', '/'), null, "java/lang/Object", null);

		{
			mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>",
					"()V");
			mv.visitInsn(RETURN);
			if (isValidClassFormat) {
				mv.visitMaxs(1, 1);
			} else {
				/* Deliberately set max_stack and max_locals to 0 to create invalid classfile */
				mv.visitMaxs(0, 0); 
			}
			mv.visitEnd();
		}
		{
			mv = cw.visitMethod(ACC_PUBLIC, "foo", "()I", null, null);
			mv.visitCode();
			mv.visitIntInsn(BIPUSH, version);
			mv.visitInsn(IRETURN);
			if (isValidClassFormat) {
				mv.visitMaxs(1, 1);
			} else {
				/* Deliberately set max_stack and max_locals to 0 to create invalid classfile */
				mv.visitMaxs(0, 0); 
			}
			mv.visitEnd();
		}
		cw.visitEnd();

		return cw.toByteArray();
	}
}
