package org.openj9.test.sealedclasses;

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

import org.openj9.test.utilities.CustomClassLoader;
import org.openj9.test.utilities.SealedClassGenerator;

/* JEP 360 sealed classes VM tests */

@Test(groups = { "level.sanity" })
public class SealedClassesTests {
    private String name = "TestIllegalSealedClass";

    /* sealed classes cannot be final */
    @Test(expectedExceptions = java.lang.ClassFormatError.class)
    public void test_sealedClassesCannotBeFinal() {
        CustomClassLoader classloader = new CustomClassLoader();
        byte[] bytes = SealedClassGenerator.generateFinalSealedClass(name);
        Class<?> clazz = classloader.getClass(name, bytes);
    }

    /* Class declarations for classloading tests */
    sealed class TestClassSealed permits TestSubclass {}
    non-sealed class TestSubclass extends TestClassSealed {}

    /* ClassLoading: a class cannot extend a sealed superclass if it is
     * not named in the superclasses PermittedSubclasses list. */
    @Test(expectedExceptions = java.lang.IncompatibleClassChangeError.class)
    public void test_loadIllegalSubclassOfSealedClass() {
        CustomClassLoader classloader = new CustomClassLoader();
        byte[] bytes = SealedClassGenerator.generateSubclassIllegallyExtendingSealedSuperclass(name, TestClassSealed.class);
        Class<?> clazz = classloader.getClass(name, bytes);
    }

    sealed interface TestInterfaceSealed permits TestSubinterface {}
    non-sealed interface TestSubinterface extends TestInterfaceSealed {}

    /* ClassLoading: an interface cannot extend a sealed superinterface if it is
     * not named in the superinterfaces PermittedSubclasses list. */
    @Test(expectedExceptions = java.lang.IncompatibleClassChangeError.class)
    public void test_loadIllegalSubinterfaceOfSealedInterface() {
        CustomClassLoader classloader = new CustomClassLoader();
        byte[] bytes = SealedClassGenerator.generateSubinterfaceIllegallyExtendingSealedSuperinterface(name, TestInterfaceSealed.class);
        Class<?> clazz = classloader.getClass(name, bytes);
    }
}
