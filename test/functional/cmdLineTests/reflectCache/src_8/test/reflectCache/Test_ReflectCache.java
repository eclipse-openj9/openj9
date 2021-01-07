/*******************************************************************************
 * Copyright (c) 2016, 2020 IBM Corp. and others
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
package test.reflectCache;

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;

/**
 * This class is used to test -Dreflect.cache options.  
 * 
 * If fields are cached, then their root field should be same. If they are not cached, then root is equal to null.
 */
public class Test_ReflectCache {

	public static void main(String[] args) {
		Class<?> testClass = Test.class;
		try {
			Field testField1 = testClass.getField("testField");
			Field testFieldRoot = Field.class.getDeclaredField("root");
			testFieldRoot.setAccessible(true);
			Object rootObject1 = testFieldRoot.get(testField1);

			Field testField2 = testClass.getField("testField");
			Object rootObject2 = testFieldRoot.get(testField2);
			
			if (rootObject1 != rootObject2) {
				System.out.println("TEST FAILED");
				return;
			} else {
				if (rootObject1 == null) {
					System.out.println("testField is not cached.");
				} else {
					System.out.println("testField is cached.");
				}
			}
	
			Method testMethod1 = testClass.getMethod("testMethod", String.class);
			Field testMethodRoot = Method.class.getDeclaredField("root");
			testMethodRoot.setAccessible(true);
			rootObject1 = testMethodRoot.get(testMethod1);
			
			Method testMethod2 = testClass.getMethod("testMethod", String.class);
			rootObject2 = testMethodRoot.get(testMethod2);
			
			if (rootObject1 != rootObject2) {
				System.out.println("TEST FAILED");
				return;
			} else {
				if (rootObject1 == null) {
					System.out.println("testMethod is not cached.");
				} else {
					System.out.println("testMethod is cached.");
				}
			}
			
			Constructor<?> testConstructor1 = testClass.getConstructor();
			Field testConstructorRoot = Constructor.class.getDeclaredField("root");
			testConstructorRoot.setAccessible(true);
			rootObject1 = testConstructorRoot.get(testConstructor1);
			
			Constructor<?> testConstructor2 = testClass.getConstructor();
			rootObject2 = testConstructorRoot.get(testConstructor2);
			
			if (rootObject1 != rootObject2) {
				System.out.println("TEST FAILED");
				return;
			} else {
				if (rootObject1 == null) {
					System.out.println("testConstructor is not cached.");
				} else {
					System.out.println("testConstructor is cached.");
				}
			}
			
			Constructor<?> testConstructorA = testClass.getConstructor(String.class);
			rootObject1 = testConstructorRoot.get(testConstructorA);
			Constructor<?> testConstructorB = testClass.getConstructor(String.class);
			rootObject2 = testConstructorRoot.get(testConstructorB);
			if (rootObject1 != rootObject2) {
				System.out.println("TEST FAILED");
				return;
			} else {
				if (null == rootObject1) {
					System.out.println("testConstructor is not cached.");
				} else {
					System.out.println("testConstructor is cached.");
				}
			}
		} catch (SecurityException e) {
			e.printStackTrace();
			System.out.println("TEST FAILED");
		} catch (NoSuchFieldException e) {
			e.printStackTrace();
			System.out.println("TEST FAILED");
		} catch (NoSuchMethodException e) {
			e.printStackTrace();
			System.out.println("TEST FAILED");
		}catch (IllegalArgumentException e) {
			e.printStackTrace();
			System.out.println("TEST FAILED");
		} catch (IllegalAccessException e) {
			e.printStackTrace();
			System.out.println("TEST FAILED");
		}
	}
}
