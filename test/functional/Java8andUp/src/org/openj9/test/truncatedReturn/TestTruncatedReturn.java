/*******************************************************************************
 * Copyright (c) 2006, 2019 IBM Corp. and others
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

package org.openj9.test.truncatedReturn;

import org.testng.annotations.Test;
import org.testng.AssertJUnit;

import java.lang.reflect.Method;
import java.net.URLClassLoader;
import java.io.FileOutputStream;
import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.Opcodes;

import java.lang.invoke.MethodType;
import java.net.URLClassLoader;
import java.net.URL;

/* Test that ireturn bytecode properly truncates results */


@Test(groups = { "level.sanity" })
public class TestTruncatedReturn {

    private static String testClassName = "TestClass";

    /* Generates the bytes which define our test class
     * For each type T from the truncated int types (bool, byte, char, short)
     * We define a test method:
     * T test_T_impl(int i) { return i; }
     *
     * and a trampoline:
     * int test_T(int i){ return test_T_impl(i); }
     *
     * The trampoline exists since otherwise proper truncation is performed
     * when boxing the return value
     */
    public static byte[] getTestClassBytes(){
        ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_MAXS);
        cw.visit(52, Opcodes.ACC_PUBLIC + Opcodes.ACC_SUPER, testClassName, null, "java/lang/Object", null);
        Class<?>[] testTypes = {
            boolean.class,
            byte.class,
            char.class,
            short.class
        };

        String trampolineDesc = MethodType.methodType(int.class, int.class).toMethodDescriptorString();
        for(Class<?> type : testTypes){
            String trampolineName = "test_" + type.getSimpleName();
            String implMethodName = trampolineName + "_impl";
            String implMethodDesc = MethodType.methodType(type, int.class).toMethodDescriptorString();


            // Create the test method which just returns the passed param
            MethodVisitor mv = cw.visitMethod(Opcodes.ACC_PRIVATE | Opcodes.ACC_STATIC , implMethodName, implMethodDesc, null, null);
            mv.visitCode();
            mv.visitVarInsn(Opcodes.ILOAD, 0);
            mv.visitInsn(Opcodes.IRETURN);
            mv.visitMaxs(0,0);
            mv.visitEnd();


            // Create the trampoline
            mv = cw.visitMethod(Opcodes.ACC_PUBLIC | Opcodes.ACC_STATIC , trampolineName, trampolineDesc, null, null);
            mv.visitCode();
            mv.visitVarInsn(Opcodes.ILOAD, 0);
            mv.visitMethodInsn(Opcodes.INVOKESTATIC, testClassName, implMethodName, implMethodDesc, false);
            mv.visitInsn(Opcodes.IRETURN);
            mv.visitMaxs(0,0);
            mv.visitEnd();
        }

        cw.visitEnd();
		return cw.toByteArray();
    }

    public static class  CustomClassLoader extends URLClassLoader {
        public CustomClassLoader() {
            super(new URL[0]);
        }

        public Class<?> buildClass(String name, byte[] classBytes){
            return defineClass(name, classBytes, 0, classBytes.length);
        }
    }

    CustomClassLoader cl = new CustomClassLoader();
    Class<?> testClass = cl.buildClass(testClassName, getTestClassBytes());

    @Test
    public void TestReturnBool() throws Exception{
        Method m = testClass.getMethod("test_boolean", int.class);
        // When returning bool, stack value is AND'ed with 1
        AssertJUnit.assertEquals((int) m.invoke(null, 0), 0);
        AssertJUnit.assertEquals((int) m.invoke(null, 1), 1);
        AssertJUnit.assertEquals((int) m.invoke(null, 2), 0);
        AssertJUnit.assertEquals((int) m.invoke(null, 3), 1);

    }

    @Test
    public void TestReturnByte()  throws Exception {
        Method m = testClass.getMethod("test_byte", int.class);
        AssertJUnit.assertEquals((int) m.invoke(null, 1), 1);
        AssertJUnit.assertEquals((int) m.invoke(null, Byte.MAX_VALUE), Byte.MAX_VALUE);
        AssertJUnit.assertEquals((int) m.invoke(null, Byte.MAX_VALUE + 1), Byte.MIN_VALUE);
        AssertJUnit.assertEquals((int) m.invoke(null, Byte.MIN_VALUE), Byte.MIN_VALUE);
        AssertJUnit.assertEquals((int) m.invoke(null, Byte.MIN_VALUE - 1), Byte.MAX_VALUE);
    }

    @Test
    public void TestReturnShort()  throws Exception{
        Method m = testClass.getMethod("test_short", int.class);
        AssertJUnit.assertEquals((int) m.invoke(null, 1), 1);
        AssertJUnit.assertEquals((int) m.invoke(null, Short.MAX_VALUE), Short.MAX_VALUE);
        AssertJUnit.assertEquals((int) m.invoke(null, Short.MAX_VALUE + 1), Short.MIN_VALUE);
        AssertJUnit.assertEquals((int) m.invoke(null, Short.MIN_VALUE), Short.MIN_VALUE);
        AssertJUnit.assertEquals((int) m.invoke(null, Short.MIN_VALUE - 1), Short.MAX_VALUE);
    }

    @Test
    public void TestReturnChar()  throws Exception {
        Method m = testClass.getMethod("test_char", int.class);
        AssertJUnit.assertEquals((int) m.invoke(null, 1), 1);
        AssertJUnit.assertEquals((int) m.invoke(null, Character.MAX_VALUE), Character.MAX_VALUE);
        AssertJUnit.assertEquals((int) m.invoke(null, Character.MAX_VALUE + 1), Character.MIN_VALUE);
        AssertJUnit.assertEquals((int) m.invoke(null, Character.MIN_VALUE), Character.MIN_VALUE);
        AssertJUnit.assertEquals((int) m.invoke(null, Character.MIN_VALUE - 1), Character.MAX_VALUE);
    }
}
