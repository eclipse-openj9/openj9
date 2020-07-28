package org.openj9.test.records;

/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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

import org.testng.annotations.Test;
import org.testng.AssertJUnit;

import java.lang.invoke.MethodHandles;
import java.lang.invoke.VarHandle;
import java.lang.reflect.Field;
import jdk.internal.misc.Unsafe;

@Test(groups = { "level.sanity" })
public class RecordFinalFieldTests {

    static Unsafe unsafe = Unsafe.getUnsafe();

    record TestRecord(Integer recordComponent) {
        static String modifiableField = "old";
        static final String finalField = "old";
    }

    /* All generated record component fields are final and are not modifiable through reflection. */
    @Test(expectedExceptions = java.lang.IllegalAccessException.class)
    public void test_javaLangReflectFieldSet_finalInstanceRecordField() throws Throwable {
        TestRecord recordClassObject = new TestRecord(0);
        Field finalRecordField = recordClassObject.getClass().getDeclaredField("recordComponent");
        finalRecordField.setAccessible(true);
        finalRecordField.set(recordClassObject, 5);
    }

    /* Final static fields in record classes are not modifiable through reflection. */
    @Test(expectedExceptions = java.lang.IllegalAccessException.class)
    public void test_javaLangReflectFieldSet_finalStaticRecordField() throws Throwable {
        TestRecord recordClassObject = new TestRecord(0);
        Field finalRecordField = recordClassObject.getClass().getDeclaredField("finalField");
        finalRecordField.setAccessible(true);
        finalRecordField.set(recordClassObject, "new");
    }

    /* Make sure non-final fields in record classes can still be modified. */
    @Test
    public void test_javaLangReflectFieldSet_staticRecordField() throws Throwable {
        TestRecord recordClassObject = new TestRecord(0);
        Field recordField = recordClassObject.getClass().getDeclaredField("modifiableField");
        recordField.setAccessible(true);
        recordField.set(recordClassObject, "new");
    }

    /* Verify behavior of setting final instance field in a record constructor. */
    record TestRecordWithConstructorSettingFinalInstance(Integer recordComponent) {
        TestRecordWithConstructorSettingFinalInstance {
            recordComponent = 0;
        }
    }

    @Test
    public void test_recordClassConstructor_setFinalInstanceField() {
        TestRecordWithConstructorSettingFinalInstance record = new TestRecordWithConstructorSettingFinalInstance(0);
    }

    /* Verify behavior of setting final static field in a record constructor. */
    record TestRecordWithConstructorSettingFinalStatic() {
        static final String finalField = "old";

        TestRecordWithConstructorSettingFinalStatic {
            try {
                Field finalRecordField = this.getClass().getDeclaredField("finalField");
                finalRecordField.setAccessible(true);
                finalRecordField.set(this, "new");
                AssertJUnit.fail("No exception was thrown.");
            } catch(IllegalAccessException e) {
                /* expected exception - test passed*/
            } catch(Throwable e) {
                AssertJUnit.fail(e.getMessage());
            }
        }
    }

    @Test
    public void test_recordClassConstructor_setFinalStaticField() {
        TestRecordWithConstructorSettingFinalStatic record = new TestRecordWithConstructorSettingFinalStatic();
    }

    /* Verify behavior of setting static field in a record constructor. */
    record TestRecordWithConstructorSettingModifiableStatic() {
        static String modifiableField = "old";

        TestRecordWithConstructorSettingModifiableStatic {
            try {
                Field finalRecordField = this.getClass().getDeclaredField("modifiableField");
                finalRecordField.setAccessible(true);
                finalRecordField.set(this, "new");
            } catch(Throwable e) {
                AssertJUnit.fail(e.getMessage());
            }
        }
    }

    @Test
    public void test_recordClassConstructor_setStaticField() {
        TestRecordWithConstructorSettingModifiableStatic record = new TestRecordWithConstructorSettingModifiableStatic();
    }

    /* Check that Unsafe.objectFieldOffset supports records. */
    @Test
    public void test_jdkInternalMiscUnsafe_objectFieldOffset() throws Throwable {
        Field finalRecordField = TestRecord.class.getDeclaredField("recordComponent");
        unsafe.objectFieldOffset(finalRecordField);
    }

    /* Check that Unsafe.staticFieldBase supports records. */
    @Test
    public void test_jdkInternalMiscUnsafe_staticFieldOffset() throws Throwable {
        Field finalRecordField = TestRecord.class.getDeclaredField("finalField");
        unsafe.staticFieldOffset(finalRecordField);
    }

    /* Check that Unsafe.staticFieldBase supports records. */
    @Test
    public void test_jdkInternalMiscUnsafe_staticFieldBase() throws Throwable {
        Field finalRecordField = TestRecord.class.getDeclaredField("finalField");
        unsafe.staticFieldBase(finalRecordField);
    }

    /* VarHandle.set is not supported for records. */
    // TODO this should be UnsupportedOperationException - openj9 is throwing WrongMethodTypeException from method handles code which is being replaced.
    @Test(expectedExceptions = java.lang.Exception.class)
    public void test_javaLangInvokeMethodHandles_unreflectVarHandle_instance() throws Throwable {
        Field finalRecordField = TestRecord.class.getDeclaredField("recordComponent");
        finalRecordField.setAccessible(true);
        VarHandle finalRecordFieldHandle = MethodHandles.lookup().unreflectVarHandle(finalRecordField);
        finalRecordFieldHandle.set(new Integer(5));
    }

    /* VarHandle.set is not supported for records. */
    @Test(expectedExceptions = java.lang.UnsupportedOperationException.class)
    public void test_javaLangInvokeMethodHandles_unreflectVarHandle_static() throws Throwable {
        Field finalRecordField = TestRecord.class.getDeclaredField("finalField");
        finalRecordField.setAccessible(true);
        VarHandle finalRecordFieldHandle = MethodHandles.lookup().unreflectVarHandle(finalRecordField);
        finalRecordFieldHandle.set("new");
    }
}
