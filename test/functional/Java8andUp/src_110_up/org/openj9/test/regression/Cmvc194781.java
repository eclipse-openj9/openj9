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
package org.openj9.test.regression;

import org.testng.annotations.Test;
import org.testng.AssertJUnit;
import java.lang.reflect.Field;
import java.security.PrivilegedAction;

import org.objectweb.asm.AnnotationVisitor;
import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.Opcodes;

import jdk.internal.misc.Unsafe;

/**
 * Validate that the java.lang.invoke.FrameIteratorSkip annotation is ignored for non-bootstrap classes.
 * This test validates this by using jdk.internal.reflect.Reflection.getCallerClass(1) from a method with the
 * FrameIteratorSkip annotation.  Since this annotated method is not on the bootclasspath, it shouldn't
 * be skipped by the stackwalk.
 */
@Test(groups = {"level.extended", "req.cmvc.194781"})
public class Cmvc194781 {

	@Test
	public void testFrameIteratorSkipAnnotationForNonBootstrapClass() throws Throwable {
		final byte[] classBytes = FrameIteratorTestGenerator.dump();
		
		Unsafe unsafe = Unsafe.getUnsafe();

        /* Force the class to be loaded in the Extension Loader so we can use jdk.internal.reflect.Reflection.getCallerClass() */
        ClassLoader extLoader = Cmvc194781.class.getClassLoader().getParent();
        Class<?> test = unsafe.defineClass("FrameIteratorTest", classBytes, 0, classBytes.length, extLoader, Cmvc194781.class.getProtectionDomain());
        
		PrivilegedAction<Class<?>> instance = (PrivilegedAction<Class<?>>)test.newInstance();
		Class<?> caller = (Class<?>)instance.run();
		AssertJUnit.assertEquals(Cmvc194781.class, caller);
	}

}

class FrameIteratorTestGenerator implements Opcodes {
	public static byte[] dump () throws Exception {
		ClassWriter cw = new ClassWriter(0);
		MethodVisitor mv;
		AnnotationVisitor av0;
		
		/* Determine if the new getCallerClass() with no parameter is available 
		 * or if only the original getCallerClass(int) is available.
		 */
		boolean doesGetCallerClassNeedParameter = false;
		try {
			jdk.internal.reflect.Reflection.class.getDeclaredMethod("getCallerClass");
		} catch(NoSuchMethodException nsme) {
			doesGetCallerClassNeedParameter = true;
		}
		
		cw.visit(V1_7, ACC_PUBLIC + ACC_SUPER, "FrameIteratorTest", null, "java/lang/Object", new String[] { "java/security/PrivilegedAction" });
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
			/* Method does the following: 
			 * 
			 * @java.lang.invoke.MethodHandle$FrameIteratorSkip		// <-- package private annotation that isn't visible here.  This is why we use ASM to generate the class
			 * @sun.reflect.CallerSensitive							// Needed to be able to use getCallerClass()
			 * public Object run {
			 * 		return jdk.internal.reflect.Reflection.getCallerClass(2);
			 * 		return jdk.internal.reflect.Reflection.getCallerClass();
			 * }
			 * 
			 * */
			mv = cw.visitMethod(ACC_PUBLIC, "run", "()Ljava/lang/Object;", null, null);
			{
				av0 = mv.visitAnnotation("Ljava/lang/invoke/MethodHandle$FrameIteratorSkip;", true);
				av0 = mv.visitAnnotation("Lsun/reflect/CallerSensitive;", true);
				av0.visitEnd();
			}
			mv.visitCode();
			if (doesGetCallerClassNeedParameter) {
				mv.visitInsn(ICONST_2);
				mv.visitMethodInsn(INVOKESTATIC, "jdk/internal/reflect/Reflection", "getCallerClass", "(I)Ljava/lang/Class;");
			} else {
				mv.visitMethodInsn(INVOKESTATIC, "jdk/internal/reflect/Reflection", "getCallerClass", "()Ljava/lang/Class;");
			}
			mv.visitInsn(ARETURN);
			mv.visitMaxs(1, 1);
			mv.visitEnd();
		}
		cw.visitEnd();

		return cw.toByteArray();
	}
}
