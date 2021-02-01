package org.openj9.test.java.lang;

/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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
import org.testng.Assert;
import org.testng.AssertJUnit;

/**
 * Test JCL additions to java.lang.Class from JEP 397: Sealed Classes (Second Preview)
 * 
 * New methods include:
 * - Class<?>[] getPermittedSubclasses()
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
	public void test_getPermittedSubclasses_nonSealedClass() {
		Class<?>[] subclasses = TestClassNotSealed.class.getPermittedSubclasses();
		Assert.assertNull(subclasses);
		subclasses = int.class.getPermittedSubclasses();
		Assert.assertNull(subclasses);
		TestClassSealed[] sealedArray = new TestClassSealed[1];
		subclasses = sealedArray.getClass().getPermittedSubclasses();
		Assert.assertNull(subclasses);
	}

	@Test
	public void test_getPermittedSubclasses_oneSubclass() {
		Class<?>[] subclassList = TestClassSealed.class.getPermittedSubclasses();

		AssertJUnit.assertEquals(1, subclassList.length);

		Class<?> sub = TestSubclass.class;
		AssertJUnit.assertEquals(sub, subclassList[0]);
	}

	@Test
	public void test_getPermittedSubclasses_multiSubclasses() {
		Class<?>[] multiSubclassesList = TestClassSealedMulti.class.getPermittedSubclasses();

		AssertJUnit.assertEquals(3, multiSubclassesList.length);

		Class<?> sub1 = TestSubclass1.class;
		AssertJUnit.assertEquals(sub1, multiSubclassesList[0]);
		Class<?> sub2 = TestSubclass2.class;
		AssertJUnit.assertEquals(sub2, multiSubclassesList[1]);
		Class<?> sub3 = TestSubclass3.class;
		AssertJUnit.assertEquals(sub3, multiSubclassesList[2]);
	}
 }
