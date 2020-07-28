package org.openj9.test.java.lang;

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

import java.lang.constant.ClassDesc;

/**
 * Test JCL additions to java.lang.Class from JEP 360: Sealed Classes preview
 * 
 * New methods include:
 * - boolean isRecord()
 * - ClassDesc[] permittedSubclasses()
 */

 @Test(groups = { "level.sanity" })
 public class Test_Class {
    /* Test classes */
    class TestClassNotSealed {}

    sealed class TestClassSealed permits TestSubclass {}
    non-sealed class TestSubclass extends TestClassSealed {}

    sealed class TestClassSealedMulti permits TestSubclass1, TestSubclass2, TestSubclass3 {}
    non-sealed class TestSubclass1 extends TestClassSealedMulti {}
    non-sealed class TestSubclass2 extends TestClassSealedMulti {}
    non-sealed class TestSubclass3 extends TestClassSealedMulti {}

    @Test
    public void test_isSealed() {
        AssertJUnit.assertFalse("TestClassNotSealed is not a sealed class", TestClassNotSealed.class.isSealed());
        AssertJUnit.assertTrue("TestClassSealed is a sealed class", TestClassSealed.class.isSealed());
        AssertJUnit.assertFalse("TestSubclass is not a sealed class", TestSubclass.class.isSealed());
        AssertJUnit.assertFalse("Primitive type is not a sealed class", int.class.isSealed());
        TestClassSealed[] sealedArray = new TestClassSealed[1];
        AssertJUnit.assertFalse("Array class is not a sealed class", sealedArray.getClass().isSealed());
    }

    @Test
    public void test_permittedSubclasses_nonSealedClass() {
        ClassDesc[] subclasses = TestClassNotSealed.class.permittedSubclasses();

        AssertJUnit.assertTrue(0 == subclasses.length);
    }

    @Test
    public void test_permittedSubclasses_oneSubclass() {
        ClassDesc[] subclassList = TestClassSealed.class.permittedSubclasses();

        AssertJUnit.assertEquals(1, subclassList.length);

        /* verify ClassDesc content */
        ClassDesc desc = TestSubclass.class.describeConstable().get();
        AssertJUnit.assertEquals(desc, subclassList[0]);
    }

    @Test
    public void test_permittedSubclasses_multiSubclasses() {
        ClassDesc[] multiSubclassesList = TestClassSealedMulti.class.permittedSubclasses();

        AssertJUnit.assertEquals(3, multiSubclassesList.length);

        /* verify ClassDesc content */
        ClassDesc desc1 = TestSubclass1.class.describeConstable().get();
        AssertJUnit.assertEquals(desc1, multiSubclassesList[0]);
        ClassDesc desc2 = TestSubclass2.class.describeConstable().get();
        AssertJUnit.assertEquals(desc2, multiSubclassesList[1]);
        ClassDesc desc3 = TestSubclass3.class.describeConstable().get();
        AssertJUnit.assertEquals(desc3, multiSubclassesList[2]);
    }
 }
