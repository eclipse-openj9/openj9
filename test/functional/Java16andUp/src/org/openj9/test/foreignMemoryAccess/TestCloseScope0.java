package org.openj9.test.foreignMemoryAccess;

/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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

import org.testng.Assert;
import static org.testng.Assert.*;
import org.testng.annotations.Test;

import java.lang.reflect.*;
import java.lang.ref.Cleaner;

import org.objectweb.asm.*;
import static org.objectweb.asm.Opcodes.*;

@Test(groups = { "level.sanity" })
public class TestCloseScope0 {
	public static byte[] dump() throws Exception {
		ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES);
		MethodVisitor mv;
		AnnotationVisitor av0;

		cw.visit(V16, ACC_PUBLIC + ACC_SUPER, "jdk/internal/misc/RunInScoped", null, "java/lang/Object", null);
		{
			mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V");
			mv.visitInsn(RETURN);
			mv.visitMaxs(0, 0);
			mv.visitEnd();
		}
		{
			/**
			 * @Scoped		// <-- package private annotation that isn't visible here.  This is why we use ASM to generate the class
			 * public static void runInScoped(Runnable r, Scope scope) {
			 *      r.run();
			 *      Reference.reachabilityFence(scope);
			 * }
			 */
			mv = cw.visitMethod(ACC_PUBLIC + ACC_STATIC, "runInScoped", "(Ljava/lang/Runnable;Ljdk/internal/misc/ScopedMemoryAccess$Scope;)V", null, null);
			{
				av0 = mv.visitAnnotation("Ljdk/internal/misc/ScopedMemoryAccess$Scoped;", true);
				av0.visitEnd();
			}
			mv.visitCode();
			mv.visitVarInsn(ALOAD, 0);
			mv.visitMethodInsn(INVOKEINTERFACE, "java/lang/Runnable", "run", "()V", true);
			mv.visitVarInsn(ALOAD, 1);
			mv.visitMethodInsn(INVOKESTATIC, "java/lang/ref/Reference", "reachabilityFence", "(Ljava/lang/Object;)V", false);
			mv.visitInsn(RETURN);
			mv.visitMaxs(0, 0);
			mv.visitEnd();
		}
		cw.visitEnd();

		return cw.toByteArray();
	}

	private static Throwable expected = null;

	@Test(expectedExceptions=java.lang.IllegalStateException.class)
	public static void closeScopeDuringAccess() throws Throwable {
		/* Reflect setup */
		Class c = Class.forName("jdk.internal.foreign.MemoryScope");
		Method createShared = c.getDeclaredMethod("createShared", new Class[] {Object.class, Runnable.class, Cleaner.class});
		createShared.setAccessible(true);
		Method close = c.getDeclaredMethod("close");
		close.setAccessible(true);
		final Object scope = createShared.invoke(null, null, new Thread(), null);

		Class Scope = Class.forName("jdk.internal.misc.ScopedMemoryAccess$Scope");
		/* End Reflect setup */

		/* ASM setup */
		final byte[] classBytes = dump();

		ClassLoader loader = ClassLoader.getSystemClassLoader();
		Class cls = Class.forName("java.lang.ClassLoader");
		Method defineClass = cls.getDeclaredMethod(
						"defineClass", 
						new Class[] { String.class, byte[].class, int.class, int.class });
		defineClass.setAccessible(true);
		
		Object[] dcArgs = new Object[] {"jdk.internal.misc.RunInScoped", classBytes, 0, classBytes.length};
		Class RunInScoped = (Class)defineClass.invoke(loader, dcArgs);
		Method runInScoped = RunInScoped.getDeclaredMethod("runInScoped", new Class[] {Runnable.class, Scope});
		/* End ASM setup */
		
		Synch synch1 = new Synch();
		Synch synch2 = new Synch();

		Thread t1 = new Thread(()->{
			try {
				synchronized (synch1) {
					synch1.wait();
				}
				close.invoke(scope);
			} catch (InvocationTargetException e) {
				// This is the expected behaviour (throws IllegalStateException)
				expected = e.getCause();
			} catch (InterruptedException | IllegalAccessException e) {
				e.printStackTrace();
			} finally {
				synchronized (synch2) {
					synch2.notifyAll();
				}
			}
		}, "ScopeCloserThread");
		
		class MyRunnable implements Runnable {
			public void run() {
				try {
					synchronized (synch1) {
						synch1.notify();
					}
					synchronized (synch2) {
						synch2.wait();
					}
				} catch (Exception e) {
					e.printStackTrace();
				}
			}
		}
		
		MyRunnable r = new MyRunnable();
		Thread t2 = new Thread(()->{
			try {
				runInScoped.invoke(null, r, scope);
			} catch (Exception e) {
				e.printStackTrace();
			}
		}, "RunInScopeThread");

		t1.start();
		t2.start();

		synchronized (synch2) {
			synch2.wait();
		}
		if (expected != null) throw expected;
	}
}

class Synch {}
