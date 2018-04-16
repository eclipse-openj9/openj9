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

/**
 * This test tries to instantiate a large object graph and then drops it and calls System.gc() several times.  A command line option is
 * provided to specify what kind of check to perform.  Here is the list of possible command line values (of which exactly one can be
 * specified) and their meanings:
 * --noContract The totalMemory of the runtime will stay constant through every system.gc() after the allocation phase completes
 * --slowContract The totalMemory of the runtime will reduce across every consecutive system.gc() after the allocation phase completes
 * --fastContract The totalMemory of the runtime will reduce only after the first system.gc() after the allocation phase completes
 * optional command line parameters:
 * --verbose Output additional information to sysout
 */
public class TestContractionMain
{
	public static Object[][] _array;
	public static final int SIZE = 500;

	/**
	 * @param args
	 */
	public static void main(String[] args)
	{
		boolean noContract = _match(args, "--noContract");
		boolean slowContract = _match(args, "--slowContract");
		boolean fastContract = _match(args, "--fastContract");
		boolean verbose = _match(args, "--verbose");

		if (1 == ((noContract ? 1 : 0) + (slowContract ? 1 : 0) + (fastContract ? 1 : 0)))
		{
			if (verbose)
			{
				System.out.println("Starting verbose test run...");
			}
			boolean pass = false;
			Runtime runtime = Runtime.getRuntime();
			long initMem = runtime.totalMemory();
			_array = new Object[SIZE][];
			for (int i = 0; i < _array.length; i++)
			{
				_array[i] = new Object[SIZE];
				for (int j = 0; j < _array[i].length; j++)
				{
					_array[i][j] = new Object();
				}
			}
			long highWaterMem = runtime.totalMemory();
			_array = null;
			System.gc();
			System.gc();
			System.gc();
			long tempMem = runtime.totalMemory();
			System.gc();
			System.gc();
			System.gc();
			long tempMem2 = runtime.totalMemory();

			if (verbose)
			{
				System.out.println("Test complete.  Memory stats:  (initial = " + initMem + ", high water = " + highWaterMem + ", after first GC set = " + tempMem + ", after second GC set = " + tempMem2);
			}

			//check sanity
			if (highWaterMem > initMem)
			{
				if (noContract)
				{
					if (verbose)
					{
						System.out.print("Test noContract (totalMemory should be unchanged across System.GC() calls) completed...");
					}
					pass = (highWaterMem == tempMem) && (highWaterMem == tempMem2);
				}
				else if (slowContract)
				{
					if (verbose)
					{
						System.out.print("Test slowContract (totalMemory should slowly reduce across System.GC() calls) completed...");
					}
					pass = (tempMem2 < tempMem) && (tempMem < highWaterMem);
				}
				else if (fastContract)
				{
					if (verbose)
					{
						System.out.print("Test noContract (fastContract should change only after the first System.GC() call) completed...");
					}
					pass = (tempMem < highWaterMem) && (tempMem == tempMem2);
				}

				if (pass)
				{
					System.out.println("PASS");
					System.exit(0);
				}
				else
				{
					System.out.println("FAIL");
					System.exit(1);
				}
			}
			else
			{
				System.err.println("Test error:  memory usage did not increase over run");
				System.exit(3);
			}
		}
		else
		{
			System.err.println("Specify exactly one of --noContract, --slowContract, or --fastContract");
			System.exit(2);
		}
	}

	private static boolean _match(String[] args, String match)
	{
		boolean toRet = false;

		for (int i = 0; !toRet && (i < args.length); i++)
		{
			toRet = match.equals(args[i]);
		}
		return toRet;
	}
}
