package org.openj9.test.java.lang.reflect;

import org.testng.annotations.Test;
import org.openj9.test.support.resource.Support_Resources;
import org.testng.Assert;
import java.io.File;
import java.lang.reflect.Constructor;
import java.lang.reflect.Executable;
import java.lang.reflect.Method;
import java.lang.reflect.Parameter;

/*******************************************************************************
 * Copyright (c) 2014, 2019 IBM Corp. and others
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

@Test(groups = { "level.sanity" })
public class Test_Executable {

	/* Method parameter modifiers (aka flags) */
	final static int ACC_DEFAULT = 0x0000;
	final static int ACC_FINAL = 0x0010;
	final static int ACC_SYNTHETIC = 0x1000;
	final static int ACC_MANDATED = 0x8000;

	private static final String[] methodNames = {
			"sampleMethod0",
			"sampleMethod1",
			"sampleMethod2",
			"sampleMethod3",
			"sampleMethod4",
			"sampleMethod5" };

	private static final String[] parameterNames = { "firstParam", "secondFinalParam", "thirdParam", "", "", "" };

	private static final Class[] parameterTypes = { boolean.class, int.class, String.class };

	private static final int[] parameterModifiers = { ACC_DEFAULT, ACC_FINAL, ACC_DEFAULT };

	/**
	 * @tests java.lang.reflect.Executable#getParameters()
	 * @tests java.lang.Class#getDeclaredMethods()
	 * @tests java.lang.Class#getDeclaredConstructors()
	 * @tests java.lang.reflect.Parameter#getName()
	 * @tests java.lang.reflect.Parameter#getType()
	 * @tests java.lang.reflect.Parameter#getModifiers()
	 * @tests java.lang.reflect.Parameter#getDeclaringExecutable()
	 *
	 * @requiredResource org/openj9/resources/openj9tr_general.jar
	 */
	@Test
	public void test_getParameters() {
		File resources = Support_Resources.createTempFolder();
		try {
			Support_Resources.copyFile(resources, null, "openj9tr_general.jar");
			File file = new File(resources.toString() + "/openj9tr_general.jar");
			java.net.URL url = new java.net.URL("file:" + file.getPath());
			ClassLoader loader = new java.net.URLClassLoader(new java.net.URL[] { url }, null);
			Class withParamsClass = null;
			Class withoutParamsClass = null;
			Class withParamsClass_InnerClass = null;
			Class withoutParamsClass_InnerClass = null;
			Class withParamsClass_EnumClass = null;
			Class withoutParamsClass_EnumClass = null;
			Method[] methods = null;
			Constructor[] constructors = null;
			Parameter[] parameters = null;

			/*************** TESTING WITHPARAMS.JAVA ************************************/
			withParamsClass = Class.forName("org.openj9.resources.methodparameters.WithParams", false, loader);
			methods = withParamsClass.getDeclaredMethods();
			constructors = withParamsClass.getDeclaredConstructors();
			nullCheck(methods, "getDeclaredMethods()");
			nullCheck(constructors, "getDeclaredConstructors()");
			checkExecutableCount(methods, 4, true, "org.openj9.resources.methodparameters.WithParams");
			checkExecutableCount(constructors, 4, false, "org.openj9.resources.methodparameters.WithParams");

			for (int j = 0; j < methods.length; j++) {
				checkExecutableName(methods[j], methodNames[j]);
				parameters = methods[j].getParameters();
				nullCheck(parameters, "getParameters()");

				for (int i = 0; i < parameters.length; i++) {
					checkDeclaringExecutable(parameters[i], methods[j]);
					checkParameterName(methods[j], parameters[i], parameterNames[i]);
					checkParameterModifiers(methods[j], parameters[i], parameterModifiers[i]);
					checkParameterType(methods[j], parameters[i], parameterTypes[i]);
				}
			}

			for (int j = 0; j < constructors.length; j++) {
				checkExecutableName(constructors[j], "org.openj9.resources.methodparameters.WithParams");
				parameters = constructors[j].getParameters();
				nullCheck(parameters, "getParameters()");

				for (int i = 0; i < parameters.length; i++) {
					checkDeclaringExecutable(parameters[i], constructors[j]);
					checkParameterName(constructors[j], parameters[i], parameterNames[i]);
					checkParameterModifiers(constructors[j], parameters[i], parameterModifiers[i]);
					checkParameterType(constructors[j], parameters[i], parameterTypes[i]);
				}
			}

			/*************** TESTING WITHPARAMS$INNERCLASS.JAVA ************************************/
			withParamsClass_InnerClass = Class.forName("org.openj9.resources.methodparameters.WithParams$InnerClass", false, loader);
			methods = withParamsClass_InnerClass.getDeclaredMethods();
			constructors = withParamsClass_InnerClass.getDeclaredConstructors();
			nullCheck(methods, "getDeclaredMethods()");
			nullCheck(constructors, "getDeclaredConstructors()");
			/*
			 * InnerClass should not have any method and should have one constructor:
			 * WithParams$InnerClass(withParamsClass.class this$0)
			 * Parameter : this$0 should be final and mandated
			 *
			 */
			checkExecutableCount(methods, 0, true, "InnerClass");
			checkExecutableCount(constructors, 1, false, "InnerClass");

			checkExecutableName(constructors[0], "org.openj9.resources.methodparameters.WithParams$InnerClass");
			parameters = constructors[0].getParameters();
			nullCheck(parameters, "getParameters()");
			if (parameters.length != 1) {
				Assert.fail(
						"Wrong number of parameters for InnerClass constructor. Expected 1. Got: " + parameters.length);
			}

			checkDeclaringExecutable(parameters[0], constructors[0]);
			checkParameterName(constructors[0], parameters[0], "this$0");
			checkParameterModifiers(constructors[0], parameters[0], ACC_FINAL + ACC_MANDATED);
			checkParameterType(constructors[0], parameters[0], withParamsClass);

			/*************** TESTING WITHPARAMS$MOOD.JAVA ************************************/
			withoutParamsClass_EnumClass = Class.forName("org.openj9.resources.methodparameters.WithParams$MOOD", false, loader);
			methods = withoutParamsClass_EnumClass.getDeclaredMethods();
			constructors = withoutParamsClass_EnumClass.getDeclaredConstructors();
			nullCheck(methods, "getDeclaredMethods()");
			nullCheck(constructors, "getDeclaredConstructors()");
			/* MOOD enum should have 2 synthetic method */
			checkExecutableCount(methods, 2, true, "enum class MOOD");
			/* MOOD enum should have 1 synthetic constructor */
			checkExecutableCount(constructors, 1, false, "enum class MOOD");

			checkExecutableName(constructors[0], "org.openj9.resources.methodparameters.WithParams$MOOD");
			parameters = constructors[0].getParameters();
			nullCheck(parameters, "getParameters()");
			/* Constructor should have 2 synthetic parameters :
			 * - String $enum$name
			 * - int $enum$ordinal
			 */
			if (parameters.length != 2) {
				Assert.fail("Wrong number of parameters for enum constructor. Expected 2. Got: " + parameters.length);
			}

			checkDeclaringExecutable(parameters[0], constructors[0]);
			checkParameterName(constructors[0], parameters[0], "$enum$name");
			checkParameterModifiers(constructors[0], parameters[0], ACC_SYNTHETIC);
			checkParameterType(constructors[0], parameters[0], String.class);

			checkDeclaringExecutable(parameters[1], constructors[0]);
			checkParameterName(constructors[0], parameters[1], "$enum$ordinal");
			checkParameterModifiers(constructors[0], parameters[1], ACC_SYNTHETIC);
			checkParameterType(constructors[0], parameters[1], int.class);

			/*
			 * Enum class should have following two synthetic methods
			 *   - values()
			 *   - valueOf(synthetic String name)
			 *
			 */

			/* Test the first synthetic method: values ()*/
			checkExecutableName(methods[0], "values");
			parameters = methods[0].getParameters();
			nullCheck(parameters, "getParameters()");
			if (parameters.length != 0) {
				Assert.fail(
						"Wrong number of parameters for enum method values(). Expected 0. Got: " + parameters.length);
			}

			/* Test the second  synthetic method: valueOf(mandated String name) */
			checkExecutableName(methods[1], "valueOf");
			parameters = methods[1].getParameters();
			nullCheck(parameters, "getParameters()");
			if (parameters.length != 1) {
				Assert.fail(
						"Wrong number of parameters for enum method values(). Expected 1. Got: " + parameters.length);
			}

			checkDeclaringExecutable(parameters[0], methods[1]);
			checkParameterName(methods[1], parameters[0], "name");
			checkParameterModifiers(methods[1], parameters[0], ACC_MANDATED);
			checkParameterType(methods[1], parameters[0], String.class);

			/*************** TESTING WITHOUTPARAMS.JAVA ************************************/
			withoutParamsClass = Class.forName("org.openj9.resources.methodparameters.WithoutParams", false, loader);
			methods = withoutParamsClass.getDeclaredMethods();
			constructors = withoutParamsClass.getDeclaredConstructors();
			nullCheck(methods, "getDeclaredMethods()");
			nullCheck(constructors, "getDeclaredConstructors()");
			checkExecutableCount(methods, 4, true, "org.openj9.resources.methodparameters.WithoutParams");
			checkExecutableCount(constructors, 4, false, "org.openj9.resources.methodparameters.WithoutParams");

			for (int j = 0; j < methods.length; j++) {
				checkExecutableName(methods[j], methodNames[j]);
				parameters = methods[j].getParameters();
				nullCheck(parameters, "getParameters()");

				for (int i = 0; i < parameters.length; i++) {
					checkDeclaringExecutable(parameters[i], methods[j]);
					/*
					 * If there is no parameter attribute, then parameters are named argN where N is the index number of the parameter.
					 */
					checkParameterName(methods[j], parameters[i], "arg" + i);
					/*
					 * If there is no parameter attribute, then getModifiers always returns default.
					 */
					checkParameterModifiers(methods[j], parameters[i], ACC_DEFAULT);
					checkParameterType(methods[j], parameters[i], parameterTypes[i]);
				}
			}

			for (int j = 0; j < constructors.length; j++) {
				checkExecutableName(constructors[j], "org.openj9.resources.methodparameters.WithoutParams");
				parameters = constructors[j].getParameters();
				nullCheck(parameters, "getParameters()");

				for (int i = 0; i < parameters.length; i++) {
					checkDeclaringExecutable(parameters[i], constructors[j]);
					/*
					 * If there is no parameter attribute, then parameters are named argN where N is the index number of the parameter.
					 */
					checkParameterName(constructors[j], parameters[i], "arg" + i);
					/*
					 * If there is no parameter attribute, then getModifiers always returns default.
					 */
					checkParameterModifiers(constructors[j], parameters[i], ACC_DEFAULT);
					checkParameterType(constructors[j], parameters[i], parameterTypes[i]);
				}
			}

			/*************** TESTING WITHOUTPARAMS$INNERCLASS.JAVA ************************************/
			withoutParamsClass_InnerClass = Class.forName("org.openj9.resources.methodparameters.WithoutParams$InnerClass", false,
					loader);
			methods = withoutParamsClass_InnerClass.getDeclaredMethods();
			constructors = withoutParamsClass_InnerClass.getDeclaredConstructors();
			nullCheck(methods, "getDeclaredMethods()");
			nullCheck(constructors, "getDeclaredConstructors()");
			/*
			 * InnerClass should not have any method and should have one constructor:
			 * WithParams$InnerClass(withParamsClass.class this$0)
			 * Parameter : this$0 should be final and mandated
			 *
			 */
			checkExecutableCount(methods, 0, true, "InnerClass");
			checkExecutableCount(constructors, 1, false, "InnerClass");

			checkExecutableName(constructors[0], "org.openj9.resources.methodparameters.WithoutParams$InnerClass");
			parameters = constructors[0].getParameters();
			nullCheck(parameters, "getParameters()");
			if (parameters.length != 1) {
				Assert.fail(
						"Wrong number of parameters for InnerClass constructor. Expected 1. Got: " + parameters.length);
			}

			checkDeclaringExecutable(parameters[0], constructors[0]);
			/*
			 * If there is no parameter attribute, then parameters are named argN where N is the index number of the parameter.
			 */
			checkParameterName(constructors[0], parameters[0], "arg0");
			/*
			 * If there is no parameter attribute, then getModifiers always returns default.
			 */
			checkParameterModifiers(constructors[0], parameters[0], ACC_DEFAULT);
			checkParameterType(constructors[0], parameters[0], withoutParamsClass);

			/*************** TESTING WITHOUTPARAMS$MOOD.JAVA ************************************/
			withoutParamsClass_EnumClass = Class.forName("org.openj9.resources.methodparameters.WithParams$MOOD", false, loader);
			methods = withoutParamsClass_EnumClass.getDeclaredMethods();
			constructors = withoutParamsClass_EnumClass.getDeclaredConstructors();
			nullCheck(methods, "getDeclaredMethods()");
			nullCheck(constructors, "getDeclaredConstructors()");
			/* MOOD enum should have 2 synthetic method */
			checkExecutableCount(methods, 2, true, "InnerClass");
			/* MOOD enum should have 1 synthetic constructor */
			checkExecutableCount(constructors, 1, false, "InnerClass");

			checkExecutableName(constructors[0], "org.openj9.resources.methodparameters.WithParams$MOOD");
			parameters = constructors[0].getParameters();
			nullCheck(parameters, "getParameters()");
			/* Constructor should have 2 synthetic parameters :
			 * - String $enum$name
			 * - int $enum$ordinal
			 *
			 * INTERESTING NOTE: Although it is compiled without -parameters, it seems like it has parameters attribute,
			 * b/c names and modifiers of parameters are reserved.
			 */
			if (parameters.length != 2) {
				Assert.fail("Wrong number of parameters for enum constructor. Expected 1. Got: " + parameters.length);
			}

			checkDeclaringExecutable(parameters[0], constructors[0]);
			checkParameterName(constructors[0], parameters[0], "$enum$name");
			checkParameterModifiers(constructors[0], parameters[0], ACC_SYNTHETIC);
			checkParameterType(constructors[0], parameters[0], String.class);

			checkDeclaringExecutable(parameters[1], constructors[0]);
			checkParameterName(constructors[0], parameters[1], "$enum$ordinal");
			checkParameterModifiers(constructors[0], parameters[1], ACC_SYNTHETIC);
			checkParameterType(constructors[0], parameters[1], int.class);

			/*
			 * Enum class should have following two synthetic methods
			 *   - values()
			 *   - valueOf(synthetic String name)
			 *
			 */

			/* Test the first synthetic method: values ()*/
			checkExecutableName(methods[0], "values");
			parameters = methods[0].getParameters();
			nullCheck(parameters, "getParameters()");
			if (parameters.length != 0) {
				Assert.fail(
						"Wrong number of parameters for enum method values(). Expected 0. Got: " + parameters.length);
			}

			/* Test the second  synthetic method: valueOf(mandated String name) */
			checkExecutableName(methods[1], "valueOf");
			parameters = methods[1].getParameters();
			nullCheck(parameters, "getParameters()");
			if (parameters.length != 1) {
				Assert.fail(
						"Wrong number of parameters for enum method values(). Expected 1. Got: " + parameters.length);
			}

			checkDeclaringExecutable(parameters[0], methods[1]);
			checkParameterName(methods[1], parameters[0], "name");
			checkParameterModifiers(methods[1], parameters[0], ACC_MANDATED);
			checkParameterType(methods[1], parameters[0], String.class);

		} catch (Exception e) {
			if (e instanceof RuntimeException)
				throw (RuntimeException)e;
			Assert.fail("unexpected exception: " + e);
		}
	}

	private void checkExecutableCount(Executable[] executables, int expectedCount, boolean isMethod, String className) {
		if (executables.length != expectedCount) {
			Assert.fail("Number of " + (isMethod ? "methods" : "constructors") + " should be " + expectedCount + " for "
					+ className);
		}
	}

	private void checkExecutableName(Executable executable, String expectedName) {
		if (!executable.getName().equals(expectedName)) {
			Assert.fail(
					"Method/Constructor name is wrong. Expected : " + expectedName + "Got: " + executable.getName());
		}
	}

	private void checkParameterType(Executable executable, Parameter parameter, Class expectedTypeClass) {
		if (parameter.getType() != expectedTypeClass) {
			Assert.fail("Executable (method or constructor) " + executable.getName()
					+ " parameter type is wrong. Expected: " + expectedTypeClass + "Got: " + parameter.getType());
		}
	}

	private void checkParameterModifiers(Executable executable, Parameter parameter, int expectedModifiers) {
		if (parameter.getModifiers() != expectedModifiers) {
			Assert.fail("Executable (method or constructor) " + executable.getName()
					+ " parameter modifier is wrong. Expected: " + expectedModifiers + "Got: "
					+ parameter.getModifiers());
		}
	}

	private void checkParameterName(Executable executable, Parameter parameter, String expectedName) {
		if (!parameter.getName().equals(expectedName)) {
			Assert.fail("Executable (method or constructor) " + executable.getName()
					+ " parameter name is wrong. Expected: " + expectedName + "Got: " + parameter.getName());
		}
	}

	private void checkDeclaringExecutable(Parameter parameter, Executable expectedExecutable) {
		if (parameter.getDeclaringExecutable() != expectedExecutable) {
			Assert.fail("Parameter's declaring executable is wrong. Expected: " + expectedExecutable.getName() + "Got: "
					+ parameter.getDeclaringExecutable().getName());
		}
	}

	private void nullCheck(Object obj, String callingMethod) {
		if (null == obj) {
			Assert.fail(callingMethod + " should never return null");
		}
	}

}
