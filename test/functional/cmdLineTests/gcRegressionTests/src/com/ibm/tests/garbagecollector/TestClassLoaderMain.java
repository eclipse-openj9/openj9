/*
 * Copyright IBM Corp. and others 2001
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package com.ibm.tests.garbagecollector;
import java.io.IOException;
import java.io.InputStream;

import java.lang.management.ManagementFactory;
import java.lang.management.ThreadInfo;
import java.lang.management.LockInfo;
import java.lang.Math;
import com.ibm.lang.management.ExtendedThreadInfo;
import com.ibm.lang.management.ThreadMXBean;
import java.util.concurrent.locks.*;
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

    static ReentrantReadWriteLock _rwl = new ReentrantReadWriteLock();
    static ReentrantReadWriteLock _rwl2 = new ReentrantReadWriteLock();
    static final int LOCK_COUNT = 2;
    static boolean failedTestRetrieveOwnableSynchronizers = false;

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

	public static int testRetrieveOwnableSynchronizers()
	{
		int lockedSynchronizersCount = 0;
		System.out.println("testRetrieveOwnableSynchronizers.");
		/* test if we still can retrieve all ownableSynchronizers via walking whole heap (if the heap is walkable) */
		ThreadMXBean tb = (ThreadMXBean)ManagementFactory.getThreadMXBean();
		try {
			/* dumpAllExtendedThreads() fetches an array of ExtendedThreadInfo objects that we can examine. */
			ExtendedThreadInfo[] tinfo = tb.dumpAllExtendedThreads(true, true);
			/* Loop through the array obtained, examining each thread. */
			for (ExtendedThreadInfo iter : tinfo) {
				java.lang.management.ThreadInfo thr = iter.getThreadInfo();
				LockInfo[] lockedSyncs = thr.getLockedSynchronizers();
				if (null != lockedSyncs) {
					lockedSynchronizersCount += lockedSyncs.length;
				}
			}
			System.out.println("lockedSynchronizersCount =  " + lockedSynchronizersCount);
		} catch (Exception e) {
			e.printStackTrace();
			System.out.println("Unexpected exception occurred while executing dumpAllExtendedThreads().");
		}
		return lockedSynchronizersCount;
	}

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

			ReentrantReadWriteLock.WriteLock writeLock = _rwl.writeLock();
			ReentrantReadWriteLock.ReadLock readLock  = _rwl.readLock();
			ReentrantLock lock = new ReentrantLock();

			int iterations = 0;
			if (!"-".equals(args[2])) {
				iterations = Integer.parseInt(args[2]);
			}

			readLock.lock();
			System.out.println("Command line to reproduce this run: com.ibm.tests.garbagecollector.TestClassLoaderMain " + arraySize + " " + percentage + " " + ((iterations == 0) ? "-" : Integer.toString(iterations)));
			readLock.unlock();
			writeLock.lock();
			System.out.println("created locks for testRetrieveOwnableSynchronizers");
			readLock.lock();
			writeLock.unlock();
			readLock.unlock();

			Thread t = new Thread(new Runnable() {
				public void run() {
					int lockedSynchronizersCount = 0;
					ReentrantReadWriteLock.ReadLock readLock2  = _rwl2.readLock();
					ReentrantReadWriteLock.WriteLock writeLock2 = _rwl2.writeLock();
					ReentrantLock lock2 = new ReentrantLock();

					for (int cnt=0; cnt<100; cnt++) {
						int sleepTime;
						readLock2.lock();
						sleepTime = (int) (Math.random() * 500) + 100;
						readLock2.unlock();
						writeLock2.lock();
						try {
							lockedSynchronizersCount = testRetrieveOwnableSynchronizers();
							readLock2.lock();
						} finally {
							writeLock2.unlock();
						}
						lock2.lock();
						try {
							System.out.println("testRetrieveOwnableSynchronizers done");
						} finally {
							lock2.unlock();
					    }
					    try {
							try {
								Thread.sleep(sleepTime);
							} catch (InterruptedException ie) {
								ie.printStackTrace();
							}
					    } finally {
							readLock2.unlock();
				       }
					   if (LOCK_COUNT != lockedSynchronizersCount) {
						   break;
					   }
					}
					failedTestRetrieveOwnableSynchronizers = (LOCK_COUNT != lockedSynchronizersCount);
				}
			});
			lock.lock();
			t.start();

			_objects = new Object[arraySize];
			long startTime = System.currentTimeMillis();
			boolean timeout = false;
			int iteration = 0;
			try {
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
		    } finally {
				lock.unlock();
		    }

			readLock.lock();
			while (t.isAlive()) {
				try {
					Thread.sleep(500);
				} catch (InterruptedException ie) {
					ie.printStackTrace();
				}
			}
			readLock.unlock();

			writeLock.lock();
			System.out.println("writeLock="+ writeLock.toString() +", readLock=" + readLock.toString() + ", _rwl=" + _rwl.toString() + ", _rwl2=" + _rwl2.toString());
			readLock.lock();
			writeLock.unlock();
			System.out.println("Successful test run! (" + (iteration - 1) + " iterations)");
			readLock.unlock();

			if (failedTestRetrieveOwnableSynchronizers) {
				System.err.println("failed TestRetrieveOwnableSynchronizers!");
				System.exit(-1);
			}
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
