/*
 * Copyright IBM Corp. and others 2021
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */

package com.ibm.j9.jsr292.indyn;

import static jdk.internal.org.objectweb.asm.Opcodes.ACC_PUBLIC;
import static jdk.internal.org.objectweb.asm.Opcodes.ACC_STATIC;
import static jdk.internal.org.objectweb.asm.Opcodes.ACC_SUPER;
import static jdk.internal.org.objectweb.asm.Opcodes.H_INVOKESTATIC;
import static jdk.internal.org.objectweb.asm.Opcodes.RETURN;
import static jdk.internal.org.objectweb.asm.Opcodes.V1_7;

import java.lang.reflect.Method;

import jdk.internal.org.objectweb.asm.ClassWriter;
import jdk.internal.org.objectweb.asm.Handle;
import jdk.internal.org.objectweb.asm.MethodVisitor;
import jdk.internal.org.objectweb.asm.Type;

public class TestJSR292 {

	public static void main(String[] args) throws Throwable {
		new TestJSR292().testJSR292Perm();
	}

	public void testJSR292Perm() throws Throwable {
		CustomClassLoader cc = new CustomClassLoader();
		Class<?> genindyn = cc.loadClass("com.ibm.j9.jsr292.indyn.GenIndyn");
		Method method = genindyn.getMethod("testJSR292Perm");
		method.invoke(null);
		System.out.println("Expected exception wasn't thrown, the test failed!");
	}

	private static byte[] createIndyClass() throws Throwable {
		ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES);
		cw.visit(V1_7, ACC_PUBLIC | ACC_SUPER, "com/ibm/j9/jsr292/indyn/GenIndyn", null, "java/lang/Object", null);

		MethodVisitor mv;
		{
			mv = cw.visitMethod(ACC_PUBLIC | ACC_STATIC, "testJSR292Perm", "()V", null, null);
			mv.visitCode();
			mv.visitInvokeDynamicInsn("testJSR292Perm", "()V", new Handle(H_INVOKESTATIC,
					"com/ibm/j9/jsr292/indyn/bootpath/CodeNotTrusted", "bootstrap_test_privAction",
					Type.getType(
							"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/CallSite;")
							.getDescriptor()));
			mv.visitInsn(RETURN);
			mv.visitMaxs(0, 0);
			mv.visitEnd();
		}
		cw.visitEnd();

		return cw.toByteArray();
	}

	class CustomClassLoader extends ClassLoader {
		protected Class<?> findClass(final String name) throws ClassNotFoundException {
			if ("com.ibm.j9.jsr292.indyn.GenIndyn".equals(name)) {
				try {
					byte[] classBytes = createIndyClass();
					return defineClass(name, classBytes, 0, classBytes.length);
				} catch (Throwable e) {
					e.printStackTrace();
				}
			}
			return super.findClass(name);
		}
	}
}
