/*******************************************************************************
 * Copyright (c) 2016, 2021 IBM Corp. and others
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
import jdk.internal.misc.Unsafe;

/**
 * This class is used to test -Dreflect.cache options.  
 * 
 * If fields are cached, then their root field should be same. If they are not cached, then root is equal to null.
 */
public class Test_ReflectCache {

	private static Unsafe unsafe;
	static {
		try {
			Field f = Unsafe.class.getDeclaredField("theUnsafe");
			f.setAccessible(true);
			unsafe = (Unsafe) f.get(null);
		} catch (NoSuchFieldException | SecurityException | IllegalArgumentException | IllegalAccessException e) {
			e.printStackTrace();
		}	
	}

	public static void main(String[] args) {
		Class<?> testClass = Test.class;
		try {
			Field testField1 = testClass.getField("testField");
			long fieldRootOffset = unsafe.objectFieldOffset(Field.class, "root");
			Object rootObject1 = unsafe.getObject(testField1, fieldRootOffset);

			Field testField2 = testClass.getField("testField");
			Object rootObject2 = unsafe.getObject(testField2, fieldRootOffset);
			
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
			long methodRootOffset = unsafe.objectFieldOffset(Method.class, "root");
			rootObject1 = unsafe.getObject(testMethod1, methodRootOffset);
			
			Method testMethod2 = testClass.getMethod("testMethod", String.class);
			rootObject2 = unsafe.getObject(testMethod2, methodRootOffset);
			
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
			long constructorRootOffset = unsafe.objectFieldOffset(Constructor.class, "root");
			rootObject1 = unsafe.getObject(testConstructor1, constructorRootOffset);
			
			Constructor<?> testConstructor2 = testClass.getConstructor();
			rootObject2 = unsafe.getObject(testConstructor2, constructorRootOffset);
			
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
			rootObject1 = unsafe.getObject(testConstructorA, constructorRootOffset);
			Constructor<?> testConstructorB = testClass.getConstructor(String.class);
			rootObject2 = unsafe.getObject(testConstructorB, constructorRootOffset);
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
		}
	}
}
