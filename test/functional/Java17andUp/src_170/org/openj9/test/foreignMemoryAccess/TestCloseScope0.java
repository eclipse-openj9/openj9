package org.openj9.test.foreignMemoryAccess;

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

import org.testng.Assert;
import static org.testng.Assert.*;
import org.testng.annotations.Test;
import org.openj9.test.util.VersionCheck;

import java.lang.reflect.*;
import java.lang.ref.Cleaner;
import jdk.internal.misc.ScopedMemoryAccess.*;

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
	private static volatile boolean t1Started = false;
	private static volatile boolean t2Waiting = false;

	@Test(expectedExceptions=java.lang.IllegalStateException.class)
	public static void closeScopeDuringAccess() throws Throwable {
		/* Reflect setup */
		Class memoryOrResourceScope;
		Method createShared;
		Method close;
		Object scope;

		int version = VersionCheck.major();
		if (version == 16) {
			memoryOrResourceScope = Class.forName("jdk.internal.foreign.MemoryScope");
			createShared = memoryOrResourceScope.getDeclaredMethod("createShared", new Class[] {Object.class, Runnable.class, Cleaner.class});
			createShared.setAccessible(true);
			scope = createShared.invoke(null, null, new Thread(), null);
		} else {
			memoryOrResourceScope = Class.forName("jdk.internal.foreign.ResourceScopeImpl");
			createShared = memoryOrResourceScope.getDeclaredMethod("createShared", new Class[] {Cleaner.class});
			createShared.setAccessible(true);
			scope = createShared.invoke(null, Cleaner.create());
		}
		close = memoryOrResourceScope.getDeclaredMethod("close");
		close.setAccessible(true);
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
		Method runInScoped = RunInScoped.getDeclaredMethod("runInScoped", new Class[] {Runnable.class, Scope.class});
		/* End ASM setup */

		Synch synch1 = new Synch();
		Synch synch2 = new Synch();

		Thread t1 = new Thread(()->{
			try {
				synchronized (synch1) {
					t1Started = true;
					synch1.wait();
				}
				close.invoke(scope);
			} catch (InvocationTargetException e) {
				// This is the expected behaviour (throws IllegalStateException)
				expected = e.getCause();
			} catch (InterruptedException | IllegalAccessException e) {
				e.printStackTrace();
			} finally {
				while (!t2Waiting) {
					Thread.yield();
				}
				synchronized (synch2) {
					synch2.notify();
				}
			}
		}, "ScopeCloserThread");

		class MyRunnable implements Runnable {
			public void run() {
				try {
					while (!t1Started) {
						Thread.yield();
					}
					synchronized (synch1) {
						synch1.notify();
					}
					synchronized (synch2) {
						t2Waiting = true;
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

		t1.join();
		t2.join();

		if (expected != null) throw expected;
	}
}

class Synch {}
