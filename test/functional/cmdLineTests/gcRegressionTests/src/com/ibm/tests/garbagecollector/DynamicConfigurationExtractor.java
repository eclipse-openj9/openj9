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
package com.ibm.tests.garbagecollector;
import java.io.IOException;
import java.io.InputStream;
import java.lang.ref.SoftReference;

/**
 * Used to generate and return variable data to the cmdLineTester script used to run the GCRegressionTests.
 * Takes two arguments which each have a sub-argument:
 * -output key - key will resolve a piece of information to output as text to stdout
 * -return key - key will resolve a number to be returned upon exit (note that an exit value of (1) is considered internal error and (0) is probably to be ignored)
 * Each argument can be specified 0 or 1 times
 */
public class DynamicConfigurationExtractor
{
	private static final String OUPUT_ARG = "-output";
	private static final String RETURN_ARG = "-return";

	private static final String KEY_HALF_EMPTY_CLASS_LOADERS = "halfEmptyClassLoaders";
	private static final String KEY_ITERATIONS_OF_EMPTY_UNLOADS = "iterationsOfEmptyUnloads";
	//maximum number of class loaders we will bother trying to load
	private static final int MAX_LOADERS_ATTEMPTED = 20000;
	private static final int MIN_ITERATIONS = 5;
	private static final int MIN_LOADERS = 1000;
	private static final long TEST_DURATION_MILLIS = 60000; /* one minute */

	private static byte[] bytes;

	private static int _halfEmptyClassLoaders;
	private static long _millisfForHalfEmptyClassLoaders;

	static {
			bytes = new byte[0];
			InputStream classFileForTesting = DynamicConfigurationExtractor.class.getResourceAsStream(TestClassLoaderMain.TEST_CLASS_FILE);
			if (null == classFileForTesting)
			{
				System.err.println("Could not find test class file: " + TestClassLoaderMain.TEST_CLASS_FILE);
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


	public static void main(String[] args)
	{
		String outputKey = null;
		String returnKey = null;

		for (int i = 0; i < (args.length - 1); i++)
		{
			String arg = args[i];

			if (arg.equals(OUPUT_ARG))
			{
				outputKey = args[i+1];
			}
			else if (arg.equals(RETURN_ARG))
			{
				returnKey = args[i+1];
			}
		}
		String outputValue = _stringForKey(outputKey);
		if (null != outputValue)
		{
			System.out.println(outputValue);
		}
		int retCode = 0;
		String returnValue = _stringForKey(returnKey);
		if (null != returnValue)
		{
			retCode = Integer.parseInt(returnValue);
		}
		System.exit(retCode);
	}

	private static String _stringForKey(String key)
	{
		String retCode = null;

		if (null != key)
		{
			if (key.equals(KEY_HALF_EMPTY_CLASS_LOADERS))
			{
				if (0 == _halfEmptyClassLoaders) {
					_gatherUnloadStats();
				}
				retCode = "" + _halfEmptyClassLoaders;
			}
			else if (key.equals(KEY_ITERATIONS_OF_EMPTY_UNLOADS))
			{
				if (0 == _millisfForHalfEmptyClassLoaders) {
					_gatherUnloadStats();
				}
				//estimate how many of these could run in 2 minutes (do at least MIN_ITERATIONS iterations, though)
				retCode = "" + Math.max(MIN_ITERATIONS, (TEST_DURATION_MILLIS / _millisfForHalfEmptyClassLoaders));
			}
		}
		return retCode;
	}

	private static void _gatherUnloadStats()
	{
		long start = System.currentTimeMillis();
		int count = 0;
		SoftReference classes = new SoftReference(new Object[MAX_LOADERS_ATTEMPTED]);

		while (count < MAX_LOADERS_ATTEMPTED)
		{
			try
			{
				Object temp = new TestClassLoader(bytes).newInstance();
				Object[] classesArray = (Object[])classes.get();
				if (classesArray == null) {
					break;
				}
				classesArray[count] = temp;
				classesArray = null;
			}
			catch (ClassNotFoundException e)
			{
				e.printStackTrace();
				System.exit(1);
			}
			catch (OutOfMemoryError e) {
				e.printStackTrace();
				System.exit(2);
			}
			count++;
		}

		long end = System.currentTimeMillis();
		//ensure that we are going to try at least MIN_LOADERS class loaders
		_halfEmptyClassLoaders = Math.max(MIN_LOADERS, count / 2);
		_millisfForHalfEmptyClassLoaders = end - start;
	}

	/**
	 * Determine the number of empty class loaders to allocate in order to use half of available memory.
	 * @return the number of loader (i.e. the desired array size)
	 */
	public static int getHalfEmptyClassLoaders()
	{
		if (0 == _halfEmptyClassLoaders) {
			_gatherUnloadStats();
		}
		return _halfEmptyClassLoaders;
	}

	public static int getMinIterations()
	{
		return MIN_ITERATIONS;
	}

	public static long getTestDurationMillis()
	{
		return TEST_DURATION_MILLIS;
	}
}
