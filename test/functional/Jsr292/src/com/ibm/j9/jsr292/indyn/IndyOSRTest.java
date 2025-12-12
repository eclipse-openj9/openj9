/*
 * Copyright IBM Corp. and others 2001
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

import org.openj9.test.util.VersionCheck;

import org.testng.annotations.Test;
import org.testng.annotations.DataProvider;

import org.testng.Assert;
import org.testng.AssertJUnit;
import org.objectweb.asm.*;
import static org.objectweb.asm.Opcodes.*;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.ConstantCallSite;
import java.lang.invoke.CallSite;
import java.lang.invoke.MethodType;
import java.lang.reflect.Method;

public class IndyOSRTest {

	private static Throwable thrower = null;
	private static String dummyClassPathName = "com/ibm/j9/jsr292/indyn/TestBSMError";
	private static String dummyClassFullName = "com.ibm.j9.jsr292.indyn.TestBSMError";

	// generate method with invokedynamic bytecode that uses the BSM "bootstrap" below
	private static byte[] generate(int version) {
		ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES | ClassWriter.COMPUTE_MAXS);
		MethodVisitor mv;
		cw.visit(VersionCheck.major() + V1_8 - 8, ACC_PUBLIC, dummyClassPathName + version, null, "java/lang/Object", null);

		mv = cw.visitMethod(ACC_PUBLIC | ACC_STATIC, "dummy", "()V", null, null);
		mv.visitCode();

		Handle bsm = new Handle(
			H_INVOKESTATIC,
			"com/ibm/j9/jsr292/indyn/IndyOSRTest",
			"bootstrap",
			"(Ljava/lang/invoke/MethodHandles$Lookup;Ljava/lang/String;Ljava/lang/invoke/MethodType;)Ljava/lang/invoke/CallSite;",
			false
		);

		mv.visitFieldInsn(GETSTATIC, "java/lang/System", "out", "Ljava/io/PrintStream;");
		mv.visitLdcInsn(1L);
		mv.visitLdcInsn(2L);
		//mv.visitVarInsn(ILOAD, 0);
		mv.visitLdcInsn(3);
		mv.visitLdcInsn(4);
		mv.visitInvokeDynamicInsn("someName", "(JJII)Ljava/lang/String;", bsm);
		mv.visitMethodInsn(INVOKEVIRTUAL, "java/io/PrintStream", "println", "(Ljava/lang/String;)V", false);
		mv.visitInsn(RETURN);
		mv.visitMaxs(6, 4);
		mv.visitEnd();
		cw.visitEnd();
		return cw.toByteArray();
	}



	public static CallSite bootstrap(MethodHandles.Lookup lookup, String name, MethodType mt) throws Throwable {
		if (thrower == null) {
			MethodHandle target = MethodHandles.lookup().findStatic(IndyTest.class, "sanity", MethodType.methodType(String.class, long.class, long.class, int.class, int.class));
			return new ConstantCallSite(target);
		}
		System.out.println("Throwing");
		throw thrower;
	}

	public static String sanity(long a, long b, int c, int d) {
		return "bootstrap" + a + "," + b + "," + c + "," + d;
	}

	private class ByteArrayClassLoader extends ClassLoader {
        public Class<?> getc(String name, byte[] b) {
            return defineClass(name,b,0,b.length);
        }
	}

	@DataProvider(name="throwableProvider")
	public static Object[][] throwableProvider() {
        return new Object[][] {{new NullPointerException(), 1}, {new StackOverflowError(), 2}, {new IllegalArgumentException(), 3},
		                {new ClassCastException(), 4}};
	}

	@Test(groups = {"level.extended"}, dataProvider="throwableProvider", invocationCount=1)
	public void testBSMErrorThrow(Throwable t, int version) {
		thrower = t;
		ByteArrayClassLoader c = new ByteArrayClassLoader();
		byte[] b = IndyOSRTest.generate(version);
		System.out.println(b.length);
		Class<?> cls = c.getc(dummyClassFullName + version, b);

		// attempt once in the interpreter
		try {
			if (t == null){
				Assert.assertTrue(cls.getMethod("dummy").invoke(null).equals("bootstrap1,2,3,4"));
			} else {
				cls.getMethod("dummy").invoke(null);
			}
		} catch(IllegalAccessException e) {
			Assert.fail("Cannot access dummy method");
		} catch(NoSuchMethodException e) {
			Assert.fail("Cannot find dummy method");
		} catch (java.lang.reflect.InvocationTargetException e) {
			if (t == null) {
				Assert.fail("Bootsrap method should not throw error when parameter t is null");
			}
		} catch (Throwable t2) {
			System.out.println("Caught something");
		}

		// run again after compilation
		try {
			if (t == null){
				Assert.assertTrue(cls.getMethod("dummy").invoke(null).equals("bootstrap1,2,3,4"));
			} else {
				cls.getMethod("dummy").invoke(null);
			}
		} catch(IllegalAccessException e) {
			Assert.fail("Cannot access dummy method");
		} catch(NoSuchMethodException e) {
			Assert.fail("Cannot find dummy method");
		} catch (java.lang.reflect.InvocationTargetException e) {
			if (t == null) {
				Assert.fail("Bootsrap method should not throw error when parameter t is null");
			}
        }
    }
}
