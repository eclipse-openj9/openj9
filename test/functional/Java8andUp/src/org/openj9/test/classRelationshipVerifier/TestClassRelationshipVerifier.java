/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

package org.openj9.test.classRelationshipVerifier;

import java.lang.reflect.*;
import org.testng.annotations.Test;
import org.testng.Assert;
import org.objectweb.asm.*;

/**
 * Definitions:
 * 	- Caller: the calling class, represented by A
 * 	- Source: the child class, represented by B
 * 	- Target: the parent class/interface, represented by C
 */

@Test(groups = { "level.sanity" })
public class TestClassRelationshipVerifier {
	static {
		try {
			System.loadLibrary("bcvrelationships");
		} catch (UnsatisfiedLinkError e) {
			Assert.fail(e.getMessage() + "\nlibrary path = " + System.getProperty("java.library.path"));
		}
	}

	native static boolean isRelationshipRecorded(String sourceClassName, String targetClassName, ClassLoader classLoader);

	/**
	 * Target class is not loaded, but source class is loaded.
	 * Check that a relationship is recorded for the source and target classes.
	 *
	 * @throws Throwable
	 */
	@Test
	static public void testOnlySourceLoaded() throws Throwable {
		CustomClassLoader classLoader = new CustomClassLoader();

		byte[] bytesClassA = ClassGenerator.classA_dump();
		byte[] bytesClassB = ClassGenerator.classB_dump();

		Class<?> clazzA = classLoader.getClass("A", bytesClassA);
		Class<?> clazzB = classLoader.getClass("B", bytesClassB);

		try {
			Object instanceB = clazzB.getDeclaredConstructor().newInstance();
			Object instanceA = clazzA.getDeclaredConstructor().newInstance();

			if (!isRelationshipRecorded("B", "C", classLoader)) {
				Assert.fail("relationship was not recorded");
			}
		} catch (InvocationTargetException e) {
			throw e.getCause();
		}
	}

	/**
	 * Source class is not loaded, but target class is loaded.
	 * Check that a relationship is recorded for the source and target classes.
	 *
	 * @throws Throwable
	 */
	@Test
	static public void testOnlyTargetLoaded() throws Throwable {
		CustomClassLoader classLoader = new CustomClassLoader();

		byte[] bytesClassA = ClassGenerator.classA_dump();
		byte[] bytesClassC = ClassGenerator.classC_dump();

		Class<?> clazzA = classLoader.getClass("A", bytesClassA);
		Class<?> clazzC = classLoader.getClass("C", bytesClassC);

		try {
			Object instanceC = clazzC.getDeclaredConstructor().newInstance();
			Object instanceA = clazzA.getDeclaredConstructor().newInstance();

			if (!isRelationshipRecorded("B", "C", classLoader)) {
				Assert.fail("relationship was not recorded");
			}
		} catch (InvocationTargetException e) {
			throw e.getCause();
		}
	}

	/**
	 * Neither source nor target classes are loaded.
	 * Check that a relationship is recorded for the source and target classes.
	 *
	 * @throws Throwable
	 */
	@Test
	static public void testNeitherLoaded() throws Throwable {
		CustomClassLoader classLoader = new CustomClassLoader();

		byte[] bytesClassA = ClassGenerator.classA_dump();

		Class<?> clazzA = classLoader.getClass("A", bytesClassA);

		try {
			Object instanceA = clazzA.getDeclaredConstructor().newInstance();

			if (!isRelationshipRecorded("B", "C", classLoader)) {
				Assert.fail("relationship was not recorded");
			}
		} catch (InvocationTargetException e) {
			throw e.getCause();
		}
	}

	/**
	 * Both source and target classes are loaded, and share a relationship.
	 * Verify that the source class extends the target class.
	 *
	 * @throws Throwable
	 */
	@Test
	static public void testSourceSubclassOfTarget() throws Throwable {
		CustomClassLoader classLoader = new CustomClassLoader();

		byte[] bytesClassA = ClassGenerator.classA_dump();
		byte[] bytesClassB = ClassGenerator.classB_extends_classC_dump();
		byte[] bytesClassC = ClassGenerator.classC_dump();

		Class<?> clazzA = classLoader.getClass("A", bytesClassA);
		Class<?> clazzC = classLoader.getClass("C", bytesClassC);
		Class<?> clazzB = classLoader.getClass("B", bytesClassB);

		try {
			Object instanceA = clazzA.getDeclaredConstructor().newInstance();
			Method passBtoAcceptC = clazzA.getDeclaredMethod("passBtoAcceptC", clazzB);
			Object instanceB = clazzB.getDeclaredConstructor().newInstance();
			passBtoAcceptC.invoke(instanceA, instanceB);
		} catch (InvocationTargetException e) {
			throw e.getCause();
		}
	}

	/**
	 * Both the source class and the target interface are loaded, and share a relationship.
	 * Verify that the source class implements the target interface.
	 *
	 * @throws Throwable
	 */
	@Test
	static public void testSourceImplementsTarget() throws Throwable {
		CustomClassLoader classLoader = new CustomClassLoader();

		byte[] bytesClassA = ClassGenerator.classA_dump();
		byte[] bytesClassB = ClassGenerator.classB_implements_interfaceC_dump();
		byte[] bytesInterfaceC = ClassGenerator.interfaceC_dump();

		Class<?> clazzA = classLoader.getClass("A", bytesClassA);
		Class<?> interfaceC = classLoader.getClass("C", bytesInterfaceC);
		Class<?> clazzB = classLoader.getClass("B", bytesClassB);

		try {
			Object instanceA = clazzA.getDeclaredConstructor().newInstance();
			Method passBtoAcceptC = clazzA.getDeclaredMethod("passBtoAcceptC", clazzB);
			Object instanceB = clazzB.getDeclaredConstructor().newInstance();
			passBtoAcceptC.invoke(instanceA, instanceB);
		} catch (InvocationTargetException e) {
			throw e.getCause();
		}
	}

 	/**
	 * Both source and target classes are loaded, but are unrelated.
	 * Verify that the source class is not a subclass of the target class.
	 *
	 * @throws Throwable
	 */
	@Test(expectedExceptions = java.lang.VerifyError.class)
	static public void testSourceNotSubclassOfTarget() throws Throwable {
		CustomClassLoader classLoader = new CustomClassLoader();

		byte[] bytesClassA = ClassGenerator.classA_dump();
		byte[] bytesClassB = ClassGenerator.classB_dump();
		byte[] bytesClassC = ClassGenerator.classC_dump();

		Class<?> clazzA = classLoader.getClass("A", bytesClassA);
		Class<?> clazzB = classLoader.getClass("B", bytesClassB);
		Class<?> clazzC = classLoader.getClass("C", bytesClassC);

		try {
			Object instanceA = clazzA.getDeclaredConstructor().newInstance();
			/**
			 * Verification should fail since B and C do not share a
			 * relationship; VerifyError is expected.
			 */
			Assert.fail("should throw exception on new instance of Class A");
		} catch (InvocationTargetException e) {
			throw e.getCause();
		}
	}

	/**
	 * Both the source class and the target interface are loaded, but are unrelated.
	 * Verify that the source class does not implement the target interface.
	 *
	 * @throws Throwable
	 */
	@Test(expectedExceptions = java.lang.IncompatibleClassChangeError.class)
	static public void testSourceDoesNotImplementTarget() throws Throwable {
		CustomClassLoader classLoader = new CustomClassLoader();

		byte[] bytesClassA = ClassGenerator.classA_dump();
		byte[] bytesClassB = ClassGenerator.classB_dump();
		byte[] bytesInterfaceC = ClassGenerator.interfaceC_dump();

		Class<?> clazzA = classLoader.getClass("A", bytesClassA);
		Class<?> clazzB = classLoader.getClass("B", bytesClassB);
		Class<?> interfaceC = classLoader.getClass("C", bytesInterfaceC);

		try {
			Object instanceA = clazzA.getDeclaredConstructor().newInstance();
			/**
			 * Verification is permissive for interface targets, so there should 
			 * be no VerifyError generated.
			 */
			Method passBtoAcceptInterfaceC = clazzA.getDeclaredMethod("passBtoAcceptInterfaceC", clazzB);
			Object instanceB = clazzB.getDeclaredConstructor().newInstance();
			passBtoAcceptInterfaceC.invoke(instanceA, instanceB);
			/**
			 * Source and target types are incompatible (B does not implement C),
			 * so an IncompatibleClassChangeError is expected to be thrown.
			 */
			Assert.fail("should throw exception on invocation of passBtoAcceptInterfaceC");
		} catch (InvocationTargetException e) {
			throw e.getCause();
		}
	}
}
