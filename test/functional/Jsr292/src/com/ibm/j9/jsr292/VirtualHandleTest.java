/*******************************************************************************
 * Copyright (c) 2014, 2018 IBM Corp. and others
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
package com.ibm.j9.jsr292;

import org.testng.annotations.Test;
import org.testng.AssertJUnit;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.lang.reflect.*;
 
import org.objectweb.asm.*;
import static org.objectweb.asm.Opcodes.*;

/**
 * A class to check if a specific method is correctly compiled by JIT
 */
public class VirtualHandleTest {

	@Test(groups = { "level.extended" })
	public void testIfVirtualHandleTestIsJitted() throws Throwable {
		try {
			TestClass_VH clazz = new TestClass_VH();
			MethodHandle mh = MethodHandles.lookup().findVirtual(TestClass_VH.class, "test", MethodType.methodType(void.class));
			AssertJUnit.assertTrue(mh.getClass().toString().equals("class java.lang.invoke.VirtualHandle"));
			try {
				while (true) {
					mh.invokeExact((TestClass_VH)clazz);
				}
			} catch (Error t) {
				t.printStackTrace();
			}
		} catch (JittingDetector j) {
			AssertJUnit.assertTrue(true);
		}
	}
	
	/**
	 * Class A declares a public method f. Class B extends A and overrides f with a protected implementation. 
	 * Class C extends B. A virtual invocation of f on C finds the protected method B.f.
	 * According to the reference implementation, this B.f is a valid virtual resolution.
	 */
	@Test(groups = { "level.extended" })
	public void test_bindTo_narrowedMethodImplementation_protected() throws Throwable {
		Helper h = new Helper(ACC_PROTECTED);
		MethodHandle mh = MethodHandles.lookup().findVirtual(h.classA, "f", MethodType.methodType(void.class));
		mh = mh.bindTo(h.classC.newInstance());
		mh.invokeExact();
	}
	
	/**
	 * Class A declares a public method f. Class B extends A and overrides f with a package private implementation. 
	 * Class C extends B. A virtual invocation of f on C finds the package private method B.f.
	 * According to the reference implementation, this B.f is a valid virtual resolution.
	 */
	@Test(groups = { "level.extended" })
	public void test_bindTo_narrowedMethodImplementation_package() throws Throwable {
		Helper h = new Helper(0);
		MethodHandle mh = MethodHandles.lookup().findVirtual(h.classA, "f", MethodType.methodType(void.class));
		mh = mh.bindTo(h.classC.newInstance());
		mh.invokeExact();
	}
	
	/**
	 * Class A declares a public method f. Class B extends A and overrides f with a private implementation. 
	 * Class C extends B. A virtual invocation of f on C finds the private method B.f.
	 * According to the reference implementation, this B.f is a valid virtual resolution.
	 */
	@Test(groups = { "level.extended" })
	public void test_bindTo_narrowedMethodImplementation_private() throws Throwable {
		Helper h = new Helper(ACC_PRIVATE);
		MethodHandle mh = MethodHandles.lookup().findVirtual(h.classA, "f", MethodType.methodType(void.class));
		mh = mh.bindTo(h.classC.newInstance());
		mh.invokeExact();
	}
	
	static class Helper {
		private static final String CLASS_A = "A";
		private static final String CLASS_B = "B";
		private static final String CLASS_C = "C";
		
		Class<?> classA;
		Class<?> classB;
		Class<?> classC;
		
		Helper(final int flags) throws ClassNotFoundException {
			ClassLoader cl = new ClassLoader() {
	            public Class<?> findClass(String name) throws ClassNotFoundException {
	                if (CLASS_A.equals(name)) {
	                    byte[] classFile = getClassA();
	                    return defineClass(CLASS_A, classFile, 0, classFile.length);
	                } else if (CLASS_B.equals(name)) {
	                    byte[] classFile = getClassB(flags);
	                    return defineClass(CLASS_B, classFile, 0, classFile.length);
	                } else if (CLASS_C.equals(name)) {
	                    byte[] classFile = getClassC();
	                    return defineClass(CLASS_C, classFile, 0, classFile.length);
	                }
	                throw new ClassNotFoundException(name);
	            }
	            
	            /*
	             * class A {
	    		 *     public void f() {}
	    		 * }
	             */
	            private byte[] getClassA() {
	            	ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES | ClassWriter.COMPUTE_MAXS);
	                MethodVisitor mv;

	                cw.visit(49, ACC_PUBLIC | ACC_SUPER, CLASS_A, null, "java/lang/Object", null);
	                {
	                    mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
	                    mv.visitCode();
	                    mv.visitVarInsn(ALOAD, 0);
	                    mv.visitMethodInsn(INVOKESPECIAL, "java/lang/Object", "<init>", "()V", false);
	                    mv.visitInsn(RETURN);
	                    mv.visitMaxs(0, 0);
	                    mv.visitEnd();
	                }
	                {
	                    mv = cw.visitMethod(ACC_PUBLIC, "f", "()V", null, null);
	                    mv.visitCode();
	                    mv.visitInsn(RETURN);
	                    mv.visitMaxs(0, 0);
	                    mv.visitEnd();
	                }
	                return cw.toByteArray();
	            }
	            
	            /*
	             * class B extends A {
	    		 *     [flags] void f() {}
	    		 * }
	             */
	            private byte[] getClassB(int flags) {
	            	ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES | ClassWriter.COMPUTE_MAXS);
	                MethodVisitor mv;

	                cw.visit(49, ACC_PUBLIC | ACC_SUPER, CLASS_B, null, CLASS_A, null);
	                {
	                    mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
	                    mv.visitCode();
	                    mv.visitVarInsn(ALOAD, 0);
	                    mv.visitMethodInsn(INVOKESPECIAL, CLASS_A, "<init>", "()V", false);
	                    mv.visitInsn(RETURN);
	                    mv.visitMaxs(0, 0);
	                    mv.visitEnd();
	                }
	                {
	                	mv = cw.visitMethod(flags, "f", "()V", null, null);
	                    mv.visitCode();
	                    mv.visitInsn(RETURN);
	                    mv.visitMaxs(0, 0);
	                    mv.visitEnd();
	                }
	                return cw.toByteArray();
	            }
	            
	            /*
	             * class C extends B { }
	             */
	            private byte[] getClassC() {
	            	ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES | ClassWriter.COMPUTE_MAXS);
	                MethodVisitor mv;

	                cw.visit(49, ACC_PUBLIC | ACC_SUPER, CLASS_C, null, CLASS_B, null);
	                {
	                    mv = cw.visitMethod(ACC_PUBLIC, "<init>", "()V", null, null);
	                    mv.visitCode();
	                    mv.visitVarInsn(ALOAD, 0);
	                    mv.visitMethodInsn(INVOKESPECIAL, CLASS_B, "<init>", "()V", false);
	                    mv.visitInsn(RETURN);
	                    mv.visitMaxs(0, 0);
	                    mv.visitEnd();
	                }
	                return cw.toByteArray();
	            }
	        };
	        
	        classA = cl.loadClass(CLASS_A);
	        classB = cl.loadClass(CLASS_B);
	        classC = cl.loadClass(CLASS_C);
		}
	}
}

class TestClass_VH {

	static long PC = 0;

	static final Field throwable_walkback;

	static {
		Field f;
		try {
			f = Throwable.class.getDeclaredField("walkback");
			f.setAccessible(true);
			throwable_walkback = f;
		} catch (NoSuchFieldException | SecurityException e) {
			throw new RuntimeException();
		}
	}

	public void test() throws Throwable {
		Throwable t = new Throwable();
		Object walkback = throwable_walkback.get(t);
		if (walkback instanceof int[]) {
			int[] iWalkback = (int[])walkback;
			if (PC == 0) {
				PC = iWalkback[0];
			} else if (PC != iWalkback[0]) {
				throw new JittingDetector("detected jitting");
			}
		} else {
			long[] iWalkback = (long[])walkback;
			if (PC == 0) {
				PC = iWalkback[0];
			} else if (PC != iWalkback[0]) {
				throw new JittingDetector("detected jitting");
			}
		}
	}
}
