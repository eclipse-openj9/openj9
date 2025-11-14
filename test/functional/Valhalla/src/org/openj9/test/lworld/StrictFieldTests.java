/*
 * Copyright IBM Corp. and others 2025
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
package org.openj9.test.lworld;

import org.objectweb.asm.*;

import static org.objectweb.asm.Opcodes.*;

import org.testng.annotations.Test;

@Test(groups = { "level.sanity" })
public class StrictFieldTests {
	/* A strict final field cannot be set outside earlyLarvel.
	 * Test in <init> after invokespecial (lateLarval).
	 */
	@Test(expectedExceptions = VerifyError.class, expectedExceptionsMessageRegExp = ".*<init>.*JBputfield.*")
	static public void testPutStrictFinalFieldLateLarval() throws Throwable{
		Class<?> c = StrictFieldGenerator.generateTestPutStrictFinalFieldLateLarval();
		c.newInstance();
	}

	/* A strict final field cannot be set outside earlyLarvel.
	 * Test outside of <init> (initialization state unrestricted).
	 */
	@Test(expectedExceptions = VerifyError.class, expectedExceptionsMessageRegExp = ".*putI.*JBputfield.*")
	static public void testPutStrictFinalFieldUnrestricted() throws Throwable {
		Class<?> c = StrictFieldGenerator.generateTestPutStrictFinalFieldUnrestricted();
		c.newInstance();
	}

	/* If a strict static field in larval state is unset, getstatic throws an exception. */
	@Test(expectedExceptions = IllegalStateException.class)
	static public void testGetStaticInLarvalForUnsetStrictField() throws Throwable {
		Class<?> c = StrictFieldGenerator.generateTestGetStaticInLarvalForUnsetStrictField();
		try {
			c.newInstance();
		} catch (ExceptionInInitializerError e) {
			throw e.getException();
		}
	}

	/* If a strict static final field in larval state is set after it has been
	 * read, an exception is thrown.
	 */
	@Test(expectedExceptions = IllegalStateException.class)
	static public void testPutStaticInLarvalForReadStrictFinalField() throws Throwable {
		Class<?> c = StrictFieldGenerator.generateTestPutStaticInLarvalForReadStrictFinalField();
		try {
			c.newInstance();
		} catch (ExceptionInInitializerError e) {
			throw e.getException();
		}
	}

	/* If at the end of the larval state a strict static field is not set,
	 * class initialization fails and an exception should be thrown.
	 * Test with one unset field.
	 */
	@Test(expectedExceptions = IllegalStateException.class,
		expectedExceptionsMessageRegExp = ".*Strict static fields are unset after initialization of.*")
	static public void testStrictStaticFieldNotSetInLarval() throws Throwable {
		Class<?> c = StrictFieldGenerator.generateTestStrictStaticFieldNotSetInLarval();
		try {
			c.newInstance();
		} catch (ExceptionInInitializerError e) {
			throw e.getException();
		}
	}

	/* If at the end of the larval state a strict static field is not set,
	 * class initialization fails and an exception should be thrown.
	 * Test with one set field and one unset field.
	 */
	@Test(expectedExceptions = IllegalStateException.class,
		expectedExceptionsMessageRegExp = ".*Strict static fields are unset after initialization of.*")
	static public void testStrictStaticFieldNotSetInLarvalMulti() throws Throwable {
		Class<?> c = StrictFieldGenerator.generateTestStrictStaticFieldNotSetInLarvalMulti();
		try {
			c.newInstance();
		} catch (ExceptionInInitializerError e) {
			throw e.getException();
		}
	}

	/* A strict instance field must be assigned by putfield before invoking an
	 * instance initialization method of the direct superclass.
	 */
	@Test
	static public void testInstanceStrictFieldSetBeforeSuperclassInit() throws Throwable {
		Class<?> c = StrictFieldGenerator.generateTestInstanceStrictFieldSetBeforeSuperclassInit();
		c.newInstance();
	}

	@Test(expectedExceptions = VerifyError.class, expectedExceptionsMessageRegExp = ".*<init>.*JBinvokespecial.*")
	static public void testInstanceStrictFieldNotSetBeforeSuperclassInit() throws Throwable {
		Class<?> c = StrictFieldGenerator.generateTestInstanceStrictFieldNotSetBeforeSuperclassInit();
		c.newInstance();
	}

	@Test(expectedExceptions = VerifyError.class, expectedExceptionsMessageRegExp = ".*<init>.*JBinvokespecial.*")
	static public void testInstanceStrictFieldNotSetBeforeSuperclassInitMulti() throws Throwable {
		Class<?> c = StrictFieldGenerator.generateTestInstanceStrictFieldNotSetBeforeSuperclassInitMulti();
		c.newInstance();
	}

	/* A strict final instance field may not be assigned after invoking
	 * another instance initialization method of this.
	 */
	@Test(expectedExceptions = VerifyError.class, expectedExceptionsMessageRegExp = ".*<init>.*JBputfield.*")
	static public void testInstanceStrictFinalFieldSetAfterThisInit() throws Throwable {
		Class<?> c = StrictFieldGenerator.testInstanceStrictFinalFieldSetAfterThisInit();
		c.newInstance();
	}

	/* A non-final strict instance field may be assigned after invoking
	 * another instance initialization method of this.
	 */
	@Test
	static public void testInstanceStrictNonFinalFieldSetAfterThisInit() throws Throwable {
		Class<?> c = StrictFieldGenerator.generateTestInstanceStrictNonFinalFieldSetAfterThisInit();
		c.newInstance();
	}

	/* If an uninitialized 'this' reference is used with IFNONNULL,
	 * verification must fail with a VerifyError.
	 */
	@Test(expectedExceptions = VerifyError.class,
		expectedExceptionsMessageRegExp = ".*<init>.*JBifnonnull.*")
	static public void testIfNonNullUninitializedThis() throws Throwable {
		Class<?> c = StrictFieldGenerator.generateTestIfNonNullUninitializedThis();
		c.newInstance();
	}

	/* If an uninitialized 'this' reference is used with IFNULL,
	 * verification must fail with a VerifyError.
	 */
	@Test(expectedExceptions = VerifyError.class,
		expectedExceptionsMessageRegExp = ".*<init>.*JBifnull.*")
	static public void testIfNullUninitializedThis() throws Throwable {
		Class<?> c = StrictFieldGenerator.generateTestIfNullUninitializedThis();
		c.newInstance();
	}

	/* If a newly created value object of  uninitialized type is used with IF_ACMPEQ
	 * verification must fail with a VerifyError.
	 */
	@Test(expectedExceptions = VerifyError.class,
		expectedExceptionsMessageRegExp = ".*<init>.*JBifacmpeq.*")
	static public void testIfAcmpeqUninitializedValue() throws Throwable {
		Class<?> c = StrictFieldGenerator.generateTestIfAcmpeqUninitializedValue();
		c.newInstance();
	}

	/* If a newly created value object of uninitialized type is used with IF_ACMPNE
	 * verification must fail with a VerifyError.
	 */
	@Test(expectedExceptions = VerifyError.class,
		expectedExceptionsMessageRegExp = ".*<init>.*JBifacmpne.*")
	static void testIfAcmpneUninitializedValue() throws Throwable {
		Class<?> c = StrictFieldGenerator.generateTestIfAcmpneUninitializedValue();
		c.newInstance();
	}
}
