/*******************************************************************************
 * Copyright (c) 2016, 2020 IBM Corp. and others
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
package javaagent;

import java.lang.instrument.ClassFileTransformer;
import java.lang.instrument.IllegalClassFormatException;
import java.lang.instrument.UnmodifiableClassException;
import java.security.ProtectionDomain;
import java.util.Random;

import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.Label;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.Opcodes;

public class ProtectedClassFileTransformer implements ClassFileTransformer,
		Opcodes {

	@Override
	public byte[] transform(ClassLoader loader, String className,
			Class<?> classBeingRedefined, ProtectionDomain protectionDomain,
			byte[] classfileBuffer) throws IllegalClassFormatException {

		/*
		 * When multiple classes are retransformed at the same time via JVMTI
		 * the verifier will not see all new class data while each new class is
		 * being verified. So if we redefine A and B, and verifying A required
		 * looking at B, we'll see the old B.
		 * 
		 * This test retransforms 2 classes (A and B) in the same call, but
		 * modifies A such that B should fail verification.
		 * 
		 * See JAZZ103 40748 and CMVC 193469
		 */

		if (className.equals("b/B")) {
			JavaAgent.transformCount += 1;
		}

		if (JavaAgent.transformCount > 1) {

			/*
			 * This is an asmified version of
			 * VM_Test/VM/cmdLineTester_doProtectedAccessCheck/src/a/A.java
			 * except the foo method is changed from public to protected.
			 */
			if (className.equals("a/A")) {
				ClassWriter cw = new ClassWriter(0);
				MethodVisitor mv;

				cw.visit(V1_7, ACC_PUBLIC + ACC_SUPER, "a/A", null,
						"java/lang/Object", null);

				{
					mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
					mv.visitCode();
					mv.visitVarInsn(ALOAD, 0);
					mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object",
							"<init>", "()V");
					mv.visitInsn(RETURN);
					mv.visitMaxs(1, 1);
					mv.visitEnd();
				}
				{
					mv = cw.visitMethod(ACC_PROTECTED, "foo", "()V", null, null);
					mv.visitCode();
					mv.visitFieldInsn(GETSTATIC, "java/lang/System", "out",
							"Ljava/io/PrintStream;");
					mv.visitLdcInsn("retransformed A");
					mv.visitMethodInsn(INVOKEVIRTUAL, "java/io/PrintStream",
							"println", "(Ljava/lang/String;)V");
					mv.visitInsn(RETURN);
					mv.visitMaxs(2, 1);
					mv.visitEnd();

				}
				cw.visitEnd();

				return cw.toByteArray();
			}

			/*
			 * This is an asmified version of
			 * VM_Test/VM/cmdLineTester_doProtectedAccessCheck/src/b/B.java
			 */
			if (className.equals("b/B")) {
				ClassWriter cw = new ClassWriter(0);
				MethodVisitor mv;

				cw.visit(V1_7, ACC_PUBLIC + ACC_SUPER, "b/B", null, "a/A", null);

				{
					mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
					mv.visitCode();
					mv.visitVarInsn(ALOAD, 0);
					mv.visitMethodInsn(INVOKESPECIAL, "a/A", "<init>", "()V");
					mv.visitInsn(RETURN);
					mv.visitMaxs(1, 1);
					mv.visitEnd();
				}
				{
					mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "main",
							"([Ljava/lang/String;)V", null, null);
					mv.visitCode();
					mv.visitTypeInsn(NEW, "a/A");
					mv.visitInsn(DUP);
					mv.visitMethodInsn(INVOKESPECIAL, "a/A", "<init>", "()V");
					mv.visitVarInsn(ASTORE, 1);
					mv.visitVarInsn(ALOAD, 1);
					mv.visitMethodInsn(INVOKEVIRTUAL, "a/A", "foo", "()V");
					mv.visitInsn(RETURN);
					mv.visitMaxs(2, 2);
					mv.visitEnd();
				}

				cw.visitEnd();

				return cw.toByteArray();
			}
		}

		return classfileBuffer;
	}
}
