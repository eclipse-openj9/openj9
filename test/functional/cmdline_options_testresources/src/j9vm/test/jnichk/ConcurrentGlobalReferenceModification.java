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
package j9vm.test.jnichk;

/**
 * Test exists to check that we aren't concurrently modifying the jniGlobalReferences pool while JNICheck is walking it.
 * This is to ensure that the test for the JNICheck bug found in CMVC 133653 is correct.
 */
public class ConcurrentGlobalReferenceModification extends Test
{
	private volatile static boolean testOK = true;

	/**
	 * @param args (ignored)
	 */
	public static void main(String[] args)
	{
		if (2 == args.length)
		{
			int numberOfThreads = Integer.parseInt(args[0]);
			final int iterationsPerThread = Integer.parseInt(args[1]);
			System.out.println("Send lots of threads through the JNI layer to create and destroy global refs.  This will stress the timing hole on concurrent modification of vm->jniGlobalReferences.  Running " + iterationsPerThread + " test iterations on " + numberOfThreads + " threads.");
			//create the threads
			Thread[] _threads = new Thread[numberOfThreads];
			for (int i = 0; i < numberOfThreads; i++)
			{
				_threads[i] = new Thread(){
					public void run()
					{
						try
						{
							for (int iteration = 0; iteration < iterationsPerThread; iteration++)
							{
								test();
							}
						}
						catch (Throwable t)
						{
							//if something went wrong, fail the test
							testOK = false;
							t.printStackTrace();
						}
					}
				};
			}
			//start concurrent test
			System.out.println("Threads created.  Starting concurrent test...");
			for (int i = 0; i < numberOfThreads; i++)
			{
				_threads[i].start();
			}
			//test running.  Wait for all threads to finish
			System.out.println("Concurrent test running.  Waiting for termination...");
			for (int i = 0; i < numberOfThreads; i++)
			{
				try
				{
					_threads[i].join();
				}
				catch (InterruptedException e)
				{
					//this thread should not be interrupted while joining so print a stack trace if this does happen so we can debug this problem
					e.printStackTrace();
				}
			}
			//test is done.  If we are running this code and none of the threads had any problems, we have passed
			if (testOK)
			{
				System.out.println("Test over.  The fact that the VM got this far without crashing implies TEST PASSED.");
			}
			else
			{
				System.out.println("Test finished but FAILED since there was an error during the run.");
			}
		}
		else
		{
			System.err.println("Usage:  ConcurrentGlobalReferenceModification <number of threads to test> <test iterations per thread>");
			System.exit(-1);
		}
	}

	private static native void test();
}
