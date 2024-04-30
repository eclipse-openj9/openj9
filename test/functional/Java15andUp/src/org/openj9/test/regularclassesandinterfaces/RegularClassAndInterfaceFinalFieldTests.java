package org.openj9.test.regularclassesandinterfaces;

/*
 * Copyright IBM Corp. and others 2022
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

import jdk.internal.misc.Unsafe;
import org.testng.annotations.Test;

import java.lang.invoke.MethodHandles;
import java.lang.invoke.VarHandle;
import java.lang.reflect.Field;

@Test(groups = { "level.sanity" })
public class RegularClassAndInterfaceFinalFieldTests {
    static Unsafe unsafe = Unsafe.getUnsafe();

    private class TestClass {
        static String modifiableField = "old";
        static final String finalField = "old";
    }

    private interface TestInterface {
        static final String finalField = "old";
    }

    /* Final static fields in regular classes are not modifiable through reflection. */
    @Test(expectedExceptions = java.lang.IllegalAccessException.class)
    public void test_javaLangReflectFieldSet_finalStaticClassField() throws Throwable {
        TestClass classObject = new TestClass();
        Field finalClassField = classObject.getClass().getDeclaredField("finalField");
        finalClassField.setAccessible(true);
        finalClassField.set(classObject, "new");
    }

    /* Make sure non-final fields in regular classes can still be modified. */
    @Test
    public void test_javaLangReflectFieldSet_staticClassField() throws Throwable {
        TestClass classObject = new TestClass();
        Field classField = classObject.getClass().getDeclaredField("modifiableField");
        classField.setAccessible(true);
        classField.set(null, "new");
    }

    /* Final static fields in interface are not modifiable through reflection. */
    @Test(expectedExceptions = java.lang.IllegalAccessException.class)
    public void test_javaLangReflectFieldSet_finalStaticInterfaceField() throws Throwable {
        Field finalInterfaceField = TestInterface.class.getDeclaredField("finalField");
        finalInterfaceField.setAccessible(true);
        finalInterfaceField.set(null, "new");
    }

    /* Check that Unsafe.staticFieldOffset supports regular classes. */
    @Test
    public void test_jdkInternalMiscUnsafe_staticClassFieldOffset() throws Throwable {
        Field finalClassField = TestClass.class.getDeclaredField("finalField");
        unsafe.staticFieldOffset(finalClassField);
    }

    /* Check that Unsafe.staticFieldBase supports regular classes. */
    @Test
    public void test_jdkInternalMiscUnsafe_staticClassFieldBase() throws Throwable {
        Field finalClassField = TestClass.class.getDeclaredField("finalField");
        unsafe.staticFieldBase(finalClassField);
    }

    /* Check that Unsafe.staticFieldOffset supports interfaces. */
    @Test
    public void test_jdkInternalMiscUnsafe_staticInterfaceFieldOffset() throws Throwable {
        Field finalInterfaceField = TestInterface.class.getDeclaredField("finalField");
        unsafe.staticFieldOffset(finalInterfaceField);
    }

    /* Check that Unsafe.staticFieldBase supports interfaces. */
    @Test
    public void test_jdkInternalMiscUnsafe_staticInterfaceFieldBase() throws Throwable {
        Field finalInterfaceField = TestInterface.class.getDeclaredField("finalField");
        unsafe.staticFieldBase(finalInterfaceField);
    }

    /* VarHandle.set is not supported for final field in regular classes. */
    @Test(expectedExceptions = java.lang.UnsupportedOperationException.class)
    public void test_javaLangInvokeMethodHandles_unreflectVarHandle_class_static() throws Throwable {
        Field finalClassField = TestClass.class.getDeclaredField("finalField");
        finalClassField.setAccessible(true);
        VarHandle finalClassFieldHandle = MethodHandles.lookup().unreflectVarHandle(finalClassField);
        finalClassFieldHandle.set("new");
    }

    /* VarHandle.set is not supported for final field in interfaces. */
    @Test(expectedExceptions = java.lang.UnsupportedOperationException.class)
    public void test_javaLangInvokeMethodHandles_unreflectVarHandle_interface_static() throws Throwable {
        Field finalInterfaceField = TestInterface.class.getDeclaredField("finalField");
        finalInterfaceField.setAccessible(true);
        VarHandle finalInterfaceFieldHandle = MethodHandles.lookup().unreflectVarHandle(finalInterfaceField);
        finalInterfaceFieldHandle.set("new");
    }
}
