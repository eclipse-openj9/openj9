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
package com.ibm.jvmti.tests.registerNativesWithRetransformation;

import java.lang.reflect.Method;

import com.ibm.jvmti.tests.util.Util;

public class rnwr001 {

	private native boolean registerNative(Class klass);

	private native boolean retransformClass(Class originalClass,
			String className, byte[] transformedClassBytes);

	public boolean setup(String args) {
		return true;
	}

	private static Method methodExists(Class klass, String methodName,
			Class[] parameterTypes) {
		try {
			return klass.getMethod(methodName, parameterTypes);
		} catch (NoSuchMethodException e) {
			return null;
		}
	}

	private static Object invokeMethod(Object obj, Method method,
			Object[] params) {
		try {
			return method.invoke(obj, params);
		} catch (Exception e) {
			e.printStackTrace();
			return null;
		}
	}

	public String helpRegisterNativesWithClassRetransformation() {
		return "Test RegisterNatives when the class containing the native method is retransformed";
	}

	public boolean testRegisterNativesWithClassRetransformation() {
		int value;
		byte[] transformedClassBytes;
		boolean rc = true;
		rnwr001_testClass_V1 obj1 = new rnwr001_testClass_V1();

		try {
			registerNative(obj1.getClass());

			value = obj1.registeredNativeMethod();
			System.out
					.print("Pre retransformation: registered native method returned: "
							+ value);
			if (value != 100) {
				System.out.println("\t FAIL");
				rc = false;
			} else {
				System.out.println("\t PASS");
			}
			value = obj1.getValue();
			System.out.print("Pre retransformation: instance method returned: "
					+ value);
			if (value != 1) {
				System.out.println("\t FAIL");
				rc = false;
			} else {
				System.out.println("\t PASS");
			}

			transformedClassBytes = Util
					.loadRedefinedClassBytesWithOriginalClassName(
							rnwr001_testClass_V1.class,
							rnwr001_testClass_V2.class);
			if (transformedClassBytes == null) {
				System.err
						.println("ERROR: Could not load redefined class bytes");
				return false;
			}
			retransformClass(obj1.getClass(), obj1.getClass()
					.getCanonicalName().replace('.', '/'),
					transformedClassBytes);

			value = obj1.registeredNativeMethod();
			System.out
					.print("Post retransformation: registered native method returned: "
							+ value);
			if (value != 100) {
				System.out.println("\t FAIL");
				rc = false;
			} else {
				System.out.println("\t PASS");
			}

			value = obj1.getValue();
			System.out
					.print("Post retransformation: instance method returned: "
							+ value);
			if (value != 2) {
				System.out.println("\t FAIL");
				rc = false;
			} else {
				System.out.println("\t PASS");
			}
		} catch (UnsatisfiedLinkError e) {
			e.printStackTrace();
			return false;
		}
		return rc;
	}

	public String helpRegisterNativesWithInterfaceRetransformation1() {
		return "Test RegisterNatives when the interface implemented by the class containing the native method is retransformed";
	}

	public boolean testRegisterNativesWithInterfaceRetransformation1() {
		int value;
		Method method;
		boolean rc = true;
		byte[] transformedClassBytes;
		rnwr001_testClassInterfaceImpl_V1 obj1 = new rnwr001_testClassInterfaceImpl_V1();

		try {
			registerNative(obj1.getClass());

			value = obj1.registeredNativeMethod();
			System.out
					.print("Pre retransformation: registered native method returned: "
							+ value);
			if (value != 100) {
				System.out.println("\t FAIL");
				rc = false;
			} else {
				System.out.println("\t PASS");
			}

			value = obj1.getValue();
			System.out.print("Pre retransformation: instance method returned: "
					+ value);
			if (value != 1) {
				System.out.println("\t FAIL");
				rc = false;
			} else {
				System.out.println("\t PASS");
			}

			transformedClassBytes = Util
					.loadRedefinedClassBytesWithOriginalClassName(
							rnwr001_testClassInterface_V1.class,
							rnwr001_testClassInterface_V2.class);
			if (transformedClassBytes == null) {
				System.err
						.println("ERROR: Could not load redefined class bytes");
				return false;
			}
			retransformClass(rnwr001_testClassInterface_V1.class,
					rnwr001_testClassInterface_V1.class.getCanonicalName()
							.replace('.', '/'), transformedClassBytes);

			value = obj1.registeredNativeMethod();
			System.out
					.print("Post retransformation: registered native method returned: "
							+ value);
			if (value != 100) {
				System.out.println("\t FAIL");
				rc = false;
			} else {
				System.out.println("\t PASS");
			}

			value = obj1.getValue();
			System.out
					.print("Post retransformation: instance method returned: "
							+ value);
			if (value != 1) {
				System.out.println("\t FAIL");
				rc = false;
			} else {
				System.out.println("\t PASS");
			}
		} catch (UnsatisfiedLinkError e) {
			e.printStackTrace();
			return false;
		}
		return rc;
	}

	public String helpRegisterNativesWithInterfaceRetransformation2() {
		return "Test RegisterNatives when the interface implemented by the super class of the class containing the native method is retransformed";
	}

	public boolean testRegisterNativesWithInterfaceRetransformation2() {
		int testMethodsCount = 0;
		boolean rc = true;
		byte[] transformedClassBytes;

		try {
			registerNative(rnwr001_Sub_V1.class);
			rnwr001_Runner_V1 runner = new rnwr001_Runner_V1();
			Method methods[] = runner.getClass().getMethods();

			/* Run all test methods in original runner class */
			for (int i = 0; i < methods.length; i++) {
				if (methods[i].getName().startsWith("runTest")) {
					Boolean ret = (Boolean) invokeMethod(runner, methods[i],
							(Object[]) null);
					System.out.print("Pre retransformation: " + runner.getClass().getName() + "."
								+ methods[i].getName() + " returned: " + ret);
					if (ret == true) {
						System.out.println("\t PASS");
					} else {
						System.out.println("\t FAIL");
						rc = false;
					} 
					testMethodsCount++;
				}
			}
			if (testMethodsCount != 2) {
				System.err
						.println("ERROR: Unexpected number of test methods in class "
								+ runner.getClass().getName()
								+ ". Expected "
								+ 2 + ", found " + testMethodsCount);
				return false;
			}

			/* Retransform interface */
			transformedClassBytes = Util
					.loadRedefinedClassBytesWithOriginalClassName(
							rnwr001_Interface_V1.class,
							rnwr001_Interface_V2.class);
			if (transformedClassBytes == null) {
				System.err
						.println("ERROR: Could not load redefined class bytes for " + rnwr001_Interface_V1.class.getName());
				return false;
			}
			retransformClass(
					rnwr001_Interface_V1.class,
					rnwr001_Interface_V1.class.getCanonicalName().replace('.',
							'/'), transformedClassBytes);

			/* Retransform derived class */
			transformedClassBytes = Util
					.loadRedefinedClassBytesWithOriginalClassName(
							rnwr001_Sub_V1.class, rnwr001_Sub_V2.class);
			if (transformedClassBytes == null) {
				System.err
						.println("ERROR: Could not load redefined class bytes for " + rnwr001_Sub_V1.class.getName());
				return false;
			}
			retransformClass(rnwr001_Sub_V1.class, rnwr001_Sub_V1.class
					.getCanonicalName().replace('.', '/'),
					transformedClassBytes);

			transformedClassBytes = Util.getClassBytes(rnwr001_Runner_V2.class);
			if (transformedClassBytes == null) {
				System.err
						.println("ERROR: Could not get class bytes for " + rnwr001_Runner_V1.class.getName());
				return false;
			}
			if (transformedClassBytes != null) {
				/*
				 * Replace occurrences of rnwr001_Runner_V2 with
				 * rnwr001_Runner_V1
				 */
				transformedClassBytes = Util.replaceClassNames(
						transformedClassBytes, "rnwr001_Runner_V1",
						"rnwr001_Runner_V2");
				
				/*
				 * Replace occurrences of rnwr001_Interface_V2 with
				 * rnwr001_Interface_V1
				 */
				transformedClassBytes = Util.replaceClassNames(
						transformedClassBytes, "rnwr001_Interface_V1",
						"rnwr001_Interface_V2");

				/* Replace occurrences of rnwr001_Sub_V2 with rnwr001_Sub_V1 */
				transformedClassBytes = Util.replaceClassNames(
						transformedClassBytes, "rnwr001_Sub_V1",
						"rnwr001_Sub_V2");

				retransformClass(
						rnwr001_Runner_V1.class,
						rnwr001_Runner_V1.class.getCanonicalName().replace('.',
								'/'), transformedClassBytes);
			}

			/* Run all test methods in re-transformed runner class */
			methods = runner.getClass().getMethods();
			testMethodsCount = 0;
			for (int i = 0; i < methods.length; i++) {
				if (methods[i].getName().startsWith("runTest")) {
					Boolean ret = (Boolean) invokeMethod(runner, methods[i],
							(Object[]) null);
					System.out.print("Post retransformation: " + runner.getClass().getName() + "."
								+ methods[i].getName() + " returned: " + ret);
					if (ret == true) {
						System.out.println("\t PASS");
					} else {
						System.out.println("\t FAIL");
						rc = false;
					}
					testMethodsCount++;
				}
			}
			if (testMethodsCount != 3) {
				System.err
						.println("ERROR: Unexpected number of test methods in class "
								+ runner.getClass().getName()
								+ ". Expected "
								+ 3 + ", found " + testMethodsCount);
				return false;
			}
		} catch (Exception e) {
			e.printStackTrace();
			return false;
		}
		return rc;
	}

	public String helpRegisterNativesWithSuperClassRetransformation() {
		return "Test RegisterNatives when the super class extended by the class containing the native method is retransformed";
	}

	public boolean testRegisterNativesWithSuperClassRetransformation() {
		int value;
		Method method;
		boolean rc = true;
		byte[] transformedClassBytes;
		rnwr001_testClassSuper_V1 superObj = new rnwr001_testClassSuper_V1();
		rnwr001_testClassSub_V1 obj = new rnwr001_testClassSub_V1();

		try {
			registerNative(obj.getClass());

			value = obj.registeredNativeMethod();
			System.out
					.print("Pre retransformation: registered native method returned: "
							+ value);
			if (value != 100) {
				System.out.println("\t FAIL");
				rc = false;
			} else {
				System.out.println("\t PASS");
			}

			value = superObj.getValue();
			System.out
					.print("Pre retransformation: instance method in super class returned: "
							+ value);
			if (value != 1) {
				System.out.println("\t FAIL");
				rc = false;
			} else {
				System.out.println("\t PASS");
			}

			value = obj.getValue();
			System.out
					.print("Pre retransformation: instance method in sub class returned: "
							+ value);
			if (value != 1) {
				System.out.println("\t FAIL");
				rc = false;
			} else {
				System.out.println("\t PASS");
			}

			if (null != methodExists(superObj.getClass(), "newMethod",
					(Class[]) null)) {
				System.err
						.println("ERROR: Did not expect \"newMethod\" to be present in the super class");
				return false;
			}

			if (null != methodExists(obj.getClass(), "newMethod",
					(Class[]) null)) {
				System.err
						.println("ERROR: Did not expect \"newMethod\" to be present in the sub class");
				return false;
			}

			transformedClassBytes = Util
					.loadRedefinedClassBytesWithOriginalClassName(
							rnwr001_testClassSuper_V1.class,
							rnwr001_testClassSuper_V2.class);
			if (transformedClassBytes == null) {
				System.err
						.println("ERROR: Could not load redefined class bytes");
				return false;
			}
			retransformClass(
					rnwr001_testClassSuper_V1.class,
					rnwr001_testClassSuper_V1.class.getCanonicalName().replace(
							'.', '/'), transformedClassBytes);

			value = obj.registeredNativeMethod();
			System.out
					.print("Post retransformation: registered native method returned: "
							+ value);
			if (value != 100) {
				System.out.println("\t FAIL");
				rc = false;
			} else {
				System.out.println("\t PASS");
			}

			value = superObj.getValue();
			System.out
					.print("Post retransformation: instance method in super class returned: "
							+ value);
			if (value != 2) {
				System.out.println("\t FAIL");
				rc = false;
			} else {
				System.out.println("\t PASS");
			}

			value = obj.getValue();
			System.out
					.print("Post retransformation: instance method in sub class returned: "
							+ value);
			if (value != 2) {
				System.out.println("\t FAIL");
				rc = false;
			} else {
				System.out.println("\t PASS");
			}

			if ((method = methodExists(superObj.getClass(), "newMethod",
					(Class[]) null)) == null) {
				System.err
						.println("ERROR: Expected \"newMethod\" to be present in the super class");
				return false;
			}

			value = (Integer) invokeMethod(superObj, method, (Object[]) null);
			System.out
					.print("Post retransformation: new instance method in super class returned: "
							+ value);
			if (value != 10) {
				System.out.println("\t FAIL");
				rc = false;
			} else {
				System.out.println("\t PASS");
			}

			if ((method = methodExists(obj.getClass(), "newMethod",
					(Class[]) null)) == null) {
				System.err
						.println("ERROR: Expected \"newMethod\" to be present in the sub class");
				return false;
			}

			value = (Integer) invokeMethod(obj, method, (Object[]) null);
			System.out
					.print("Post retransformation: new instance method in sub class returned: "
							+ value);
			if (value != 10) {
				System.out.println("\t FAIL");
				rc = false;
			} else {
				System.out.println("\t PASS");
			}
		} catch (UnsatisfiedLinkError e) {
			e.printStackTrace();
			return false;
		}
		return rc;
	}
}
