package org.openj9.test.java.lang.instrument;

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
import org.testng.log4testng.Logger;

import org.testng.AssertJUnit;

import org.openj9.test.utilities.CustomClassLoader;
import org.openj9.test.utilities.JavaAgent;
import org.openj9.test.utilities.SealedClassGenerator;

/**
 * Test Java instrumentation API changes for JEP 360: Sealed classes preview
 */

@Test(groups = { "level.sanity" })
public class Test_Instrumentation {
     String name = "TestClassSealed";

    /* Redefine sealed class with an extra entry in the PermittedSubclasses attribute. */
    @Test(expectedExceptions = java.lang.UnsupportedOperationException.class)
    public void test_redefineClass_addPermittedSubclassEntry() throws Throwable {
        CustomClassLoader classloader = new CustomClassLoader();
        byte[] bytes1 = SealedClassGenerator.generateSealedClass(name, new String[]{ "TestSubclass" });
        Class<?> clazz = classloader.getClass(name, bytes1);

        byte[] bytes2 = SealedClassGenerator.generateSealedClass(name, new String[]{ "TestSubclass", "TestSubclass2" });
        JavaAgent.redefineClass(clazz, bytes2);
    }

    /* Redefine sealed class with one less entry in the PermittedSubclasses attribute. */
    @Test(expectedExceptions = java.lang.UnsupportedOperationException.class)
    public void test_redefineClass_removePermittedSubclassEntry() throws Throwable {
        CustomClassLoader classloader = new CustomClassLoader();
        byte[] bytes1 = SealedClassGenerator.generateSealedClass(name, new String[]{ "TestSubclass", "TestSubclass2" });
        Class<?> clazz = classloader.getClass(name, bytes1);

        byte[] bytes2 = SealedClassGenerator.generateSealedClass(name, new String[]{ "TestSubclass" });
        JavaAgent.redefineClass(clazz, bytes2);
    }

    /* Redefine sealed class with permitted subclasses out of order. */
    @Test
    public void test_redefineClass_reorderPermittedSubclassEntries() throws Throwable {
        CustomClassLoader classloader = new CustomClassLoader();
        byte[] bytes1 = SealedClassGenerator.generateSealedClass(name, new String[]{ "TestSubclass", "TestSubclass2" });
        Class<?> clazz = classloader.getClass(name, bytes1);

        byte[] bytes2 = SealedClassGenerator.generateSealedClass(name, new String[]{ "TestSubclass2", "TestSubclass" });
        JavaAgent.redefineClass(clazz, bytes2);
    }

    /* Redefine sealed class with different entries in the PermittedSubclasses attribute. */
    @Test(expectedExceptions = java.lang.UnsupportedOperationException.class)
    public void test_redefineClass_changePermittedSubclassEntry() throws Throwable {
        CustomClassLoader classloader = new CustomClassLoader();
        byte[] bytes1 = SealedClassGenerator.generateSealedClass(name, new String[]{ "TestSubclass", "TestSubclass2" });
        Class<?> clazz = classloader.getClass(name, bytes1);

        byte[] bytes2 = SealedClassGenerator.generateSealedClass(name, new String[]{ "TestSubclass", "TestSubclass3" });
        JavaAgent.redefineClass(clazz, bytes2);
    }
}
