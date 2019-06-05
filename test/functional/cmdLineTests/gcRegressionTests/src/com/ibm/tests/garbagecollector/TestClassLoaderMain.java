/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
package com.ibm.tests.garbagecollector;
import java.io.IOException;
import java.io.InputStream;

/**
 * This test is to exercise the GC's dynamic class unloading capability.  It tries to load a class and then instantiate thousands
 * of copies of it in an array which it continues to over-write with new copies of the class or other objects to force the creation
 * of large amounts of garbage.  Note that this test will give most useful results if run with -Xalwaysclassgc
 *
 * @note This test is currently only version controlled in this JAR since it will eventually live in a larger GC testing project
 * which does not yet exist.
 */
public class TestClassLoaderMain {
	public static final String TEST_CLASS_NAME = "com.ibm.tests.garbagecollector.EmptyTestClass";
	public static final String TEST_CLASS_FILE = "EmptyTestClass.class";
	private static byte[] bytes;

	static {
			bytes = new byte[0];
			InputStream classFileForTesting = TestClassLoaderMain.class.getResourceAsStream(TEST_CLASS_FILE);
			if (null == classFileForTesting)
			{
				System.err.println("Could not find test class file: " + TEST_CLASS_FILE);
			}
			else
			{
				try
				{
					byte[] temp = new byte[4096];
					int read = classFileForTesting.read(temp);
					while (-1 != read) {
						byte[] old = bytes;
						bytes = new byte[read + old.length];
						System.arraycopy(old, 0, bytes, 0, old.length);
						System.arraycopy(temp, 0, bytes, old.length, read);
						read = classFileForTesting.read(temp);
					}
				}
				catch (IOException e)
				{
					System.err.println("IOException while reading test class!");
					e.printStackTrace();
				}
			}
	}

	private static void test(int increment)
	{
		int count = 0;
		for (int i = 0; i < _objects.length; i++) {
			if (count < increment)
			{
				classLoaderAllocate(_objects, i);
			}
			else
			{
				_objects[i] = new Object();
			}
			count = (count +1) % 100;
		}
		for (int i = 0; i < _objects.length; i += increment) {
			_objects[i] = null;
		}
	}

	private static void classLoaderAllocate(Object[] array, int arrayLoc) {
		try {
			array[arrayLoc] = new TestClassLoader(bytes).newInstance();
			if (arrayLoc % 1000 == 0) {
				System.out.println("i="+arrayLoc);
			}
		} catch (Exception e) {
			e.printStackTrace();
		}
	}

	private static Object[] _objects;

	public static void main(String[] args) {
		if (3 == args.length)
		{
			int arraySize = 0;
			if ("-".equals(args[0])) {
				System.out.println("Calibrating live object array size...");
				arraySize = DynamicConfigurationExtractor.getHalfEmptyClassLoaders();
				System.out.println("Set live object array size to " + arraySize);
			} else {
				arraySize = Integer.parseInt(args[0]);
			}

			int percentage = 50;
			if (!"-".equals(args[1])) {
				percentage = Integer.parseInt(args[1]);
				if (percentage > 100) {
					System.err.println("Percentage too high");
					System.exit(-1);
				}
			}

			int iterations = 0;
			if (!"-".equals(args[2])) {
				iterations = Integer.parseInt(args[2]);
			}

			System.out.println("Command line to reproduce this run: com.ibm.tests.garbagecollector.TestClassLoaderMain " + arraySize + " " + percentage + " " + ((iterations == 0) ? "-" : Integer.toString(iterations)));

			_objects = new Object[arraySize];
			long startTime = System.currentTimeMillis();
			boolean timeout = false;
			int iteration = 0;
			for (iteration = 1; (!timeout) && ((iterations == 0) || (iteration <= iterations)); iteration++)
			{
				if (iterations == 0) {
					System.out.println("Allocating array (iteration " + iteration + ")");
				} else {
					System.out.println("Allocating array (iteration " + iteration + " of " + iterations + ")");
				}
				test(percentage);

				/* no iterations specified -- run for one minute instead */
				if ((iterations == 0) && (iteration > DynamicConfigurationExtractor.getMinIterations())) {
					long nowTime = System.currentTimeMillis();
					timeout = (nowTime - startTime) >= DynamicConfigurationExtractor.getTestDurationMillis();
				}
			}
			System.out.println("Successful test run! (" + (iteration - 1) + " iterations)");
		}
		else
		{
			System.err.println("com.ibm.tests.garbagecollector.TestClassLoaderMain:  A test to excercise the GC's dynamic class unloading capability.");
			System.err.println("You must specify the number of objects to keep alive, the percentage of objects which are to be class loaders, and how many times to fill the array of live objects");
			System.err.println("Usage:  \"com.ibm.tests.garbagecollector.TestClassLoaderMain <live object array size> <percentage of live objects which are class loaders> <number of times to fill live object array before declaring success>\"");
			System.err.println("        (any <argument> may be replaced with \"-\" to use the default value)");
		}
	}
}
