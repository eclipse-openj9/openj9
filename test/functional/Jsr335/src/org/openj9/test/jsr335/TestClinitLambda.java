/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
package org.openj9.test.jsr335;

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.util.concurrent.Callable;
import java.util.HashMap;
import java.util.Map;
import java.util.function.Function;

public class TestClinitLambda {

	static final String expectedString = "clinit lambda succeeded";
	static final String enumName = "enumName";
	static String clinitedString = null;
	
	static {
		Callable<String> callable = () -> expectedString;
		try {
			clinitedString = callable.call();
		} catch (Exception e) {
			e.printStackTrace();
		}
	}

	/**
	 * The method below returns a string in order to support Handler's (enum)
	 * signature.
	 */
	public String getTestName() {
		return "TestClinitLambda";
	}

	/**
	 * The method below tries to access an enum's values. This method is called
	 * during EnumWrapper's <clinit>. EnumWrapper contains the enum, Handler.
	 * When the JVM materializes a MethodType from a descriptor string, all
	 * classes named in the descriptor must be accessible, and will be loaded.
	 * But, the classes need not be initialized. So, the method below verifies
	 * that no exception should be thrown when the enum's values are
	 * accessed. It returns an instance of Hashmap to support EnumWrapper's
	 * <clinit>.
	 */
	private static Map<String, String> getNameMapping() {
		try {
			EnumWrapper.Handler.values();
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail(e.getMessage());
		}
		return new HashMap<>();
	}

	/**
	 * The class below contains the enum, Handler. It creates a circular
	 * dependency by calling getNameMapping during <clinit>. The call to
	 * getNameMapping tries to access the enum's values, which are not
	 * initialized. This allows us to verify that JVM's materialization of
	 * MethodType from descriptor string only requires the classes to be
	 * accessible and loaded. The classes don't need to be initialized.
	 */
	public static class EnumWrapper {
		private final TestClinitLambda testClass = new TestClinitLambda();
		public static Map<String, String> nameMapping = getNameMapping();

		public enum Handler {
			NAME(TestClinitLambda.enumName, sp -> sp.testClass.getTestName());

			final Function<EnumWrapper, Object> function;
			final String enumName;

			Handler(String enumName, Function<EnumWrapper, Object> function) {
				this.function = function;
				this.enumName = enumName;
			}
		}
	}
	
	/**
	 * Test that a simple lambda behaves as expected in <clinit>.
	 */
	@Test(groups = { "level.sanity" })
	public void testClinitLambda() {
		AssertJUnit.assertTrue("Lambda in <clinit> did not return expected value. "
				+ "\n\tExpected: " + expectedString + "\n\tActual: "
				+ clinitedString, expectedString.equals(clinitedString));
	}
	
	/**
	 * Test that a simple lambda behaves as expected when used in an enum's
	 * signature. Also, test that the JVM materializes MethodType (MT) from a
	 * descriptor string as expected.
	 */
	@Test(groups = { "level.sanity" })
	public void testLambdaInClinitAndMTArgClassInit() {
		final String name = TestClinitLambda.EnumWrapper.Handler.NAME.enumName;
		AssertJUnit.assertTrue("Enum was not initialized properly. " + "\n\tExpected: "
				+ enumName + "\n\tActual: " + name, name.equals(enumName));
	}
		
}
