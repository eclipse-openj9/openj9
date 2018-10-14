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
import org.testng.Assert;
import org.testng.AssertJUnit;
import static org.objectweb.asm.Opcodes.*;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.lang.reflect.Field;

import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.MethodVisitor;

import org.openj9.test.util.VersionCheck;

/**
 * A class to check if a specific method is correctly compiled by JIT
 */
public class InterfaceHandleTest {

	@Test(groups = { "level.extended" })
	public void testIfInterfaceHandleIsJitted() throws Throwable {
		try {
			TestClass_IH clazz = new TestClass_IH();
			MethodHandle mh = MethodHandles.lookup().findVirtual(TestInterface.class, "test", MethodType.methodType(void.class));
			AssertJUnit.assertTrue(mh.getClass().toString().equals("class java.lang.invoke.InterfaceHandle"));
			try {
				while (true) {
					mh.invokeExact((TestInterface)clazz);
				}
			} catch (Error t) {
				t.printStackTrace();
			}
		} catch (JittingDetector j) {
			AssertJUnit.assertTrue(true);
		}
	}
	
	/**
	 * Class C inherits a public implementation of the method f from class B, which is declared in interface A.
	 * Because the implementation is public, B.f is a valid implementation of A.f.
	 */
	@Test(groups = { "level.extended" })
	public void test_bindTo_interfaceMethodImplementation_public() throws Throwable {
		Helper h = new Helper(ACC_PUBLIC);
		MethodHandle mh = MethodHandles.lookup().findVirtual(h.classA, "f", MethodType.methodType(void.class));
		mh = mh.bindTo(h.classC.newInstance());
		mh.invokeExact();
	}
	
	/**
	 * Class C inherits a protected implementation of the method f from class B.
	 * The method is also declared in interface A, which C implements.
	 * Because the implementation is protected, B.f is an invalid implementation of A.f.
	 */
	@Test(groups = { "level.extended" })
	public void test_bindTo_interfaceMethodImplementation_protected() throws Throwable {
		Helper h = new Helper(ACC_PROTECTED);
		MethodHandle mh = MethodHandles.lookup().findVirtual(h.classA, "f", MethodType.methodType(void.class));
		mh = mh.bindTo(h.classC.newInstance());
		try {
			mh.invokeExact();
			Assert.fail("Successfully invoked protected implementation of an interface method.");
		} catch (IllegalAccessError e) { }
	}
	
	/**
	 * Class C inherits a package private implementation of the method f from class B.
	 * The method is also declared in interface A, which C implements.
	 * Because the implementation is package private, B.f is an invalid implementation of A.f.
	 */
	@Test(groups = { "level.extended" })
	public void test_bindTo_interfaceMethodImplementation_package() throws Throwable {
		Helper h = new Helper(0);
		MethodHandle mh = MethodHandles.lookup().findVirtual(h.classA, "f", MethodType.methodType(void.class));
		mh = mh.bindTo(h.classC.newInstance());
		try {
			mh.invokeExact();
			Assert.fail("Successfully invoked package private implementation of an interface method.");
		} catch (IllegalAccessError e) { }
	}
	
	/**
	 * Class C inherits a private implementation of the method f from class B.
	 * The method is also declared in interface A, which C implements.
	 * Because the implementation is private, B.f is an invalid implementation of A.f.
	 */
	@Test(groups = { "level.extended" })
	public void test_bindTo_interfaceMethodImplementation_private() throws Throwable {
		Helper h = new Helper(ACC_PRIVATE);
		MethodHandle mh = MethodHandles.lookup().findVirtual(h.classA, "f", MethodType.methodType(void.class));
		mh = mh.bindTo(h.classC.newInstance());
		try {
			mh.invokeExact();
			Assert.fail("Successfully invoked private implementation of an interface method.");
		} catch (IllegalAccessError e) {
			if (VersionCheck.major() >= 11) {
				Assert.fail("IllegalAccessError thrown instead of AbstractMethodError");				
			}
		} catch (AbstractMethodError e) {
			if (VersionCheck.major() < 11) {
				Assert.fail("AbstractMethodError thrown instead of IllegalAccessError");				
			}
		}
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
	             * interface A {
	    		 *     void f();
	    		 * }
	             */
	            private byte[] getClassA() {
	            	ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES | ClassWriter.COMPUTE_MAXS);
	                MethodVisitor mv;

	                cw.visit(49, ACC_PUBLIC | ACC_ABSTRACT | ACC_INTERFACE, CLASS_A, null, "java/lang/Object", null);
	                {
		                mv = cw.visitMethod(ACC_PUBLIC | ACC_ABSTRACT, "f", "()V", null, null);
		                mv.visitEnd();
	                }
	                return cw.toByteArray();
	            }
	            
	            /*
	             * class B {
	    		 *     [flags] void f() {}
	    		 * }
	             */
	            private byte[] getClassB(int flags) {
	            	ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES | ClassWriter.COMPUTE_MAXS);
	                MethodVisitor mv;

	                cw.visit(49, ACC_PUBLIC | ACC_SUPER, CLASS_B, null, "java/lang/Object", null);
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
	                	mv = cw.visitMethod(flags, "f", "()V", null, null);
	                    mv.visitCode();
	                    mv.visitInsn(RETURN);
	                    mv.visitMaxs(0, 0);
	                    mv.visitEnd();
	                }
	                return cw.toByteArray();
	            }
	            
	            /*
	             * class C extends B implements A { }
	             */
	            private byte[] getClassC() {
	            	ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES | ClassWriter.COMPUTE_MAXS);
	                MethodVisitor mv;

	                cw.visit(49, ACC_PUBLIC | ACC_SUPER, CLASS_C, null, CLASS_B, new String[] {CLASS_A});
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

class TestClass_IH implements TestInterface {

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

	@Test(groups = { "level.extended" })
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
