/*
 * Copyright IBM Corp. and others 2024
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
package org.openj9.test;

import java.lang.reflect.Method;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;
import java.util.Queue;
import java.util.concurrent.Callable;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.locks.LockSupport;
import org.openj9.test.util.VersionCheck;

public class WorkLoad {

	private static interface GlobalLoack { }

	public static double average;
	public static double stdDev;

	private static final Queue<Class<?>> classes = new ConcurrentLinkedQueue<>();

	private static long globalCounter = 0;

	private static Object globalLock = new GlobalLoack() { };

	private final int numberOfThreads;
	private final int sizeOfNumberList;
	private final int repeats;
	private final boolean vthreads;

	public WorkLoad(int numberOfThreads, int sizeOfNumberList, int repeats, boolean vthreads) {
		this.numberOfThreads = numberOfThreads;
		this.sizeOfNumberList = sizeOfNumberList;
		this.repeats = repeats;
		this.vthreads = vthreads;
	}

	public static void main(String[] args) {
		int numberOfThreads = 100;
		int sizeOfNumberList = 10000;
		int repeats = 50;
		boolean vthreads = Integer.getInteger("java.vm.specification.version") >= 21;

		if (args.length > 0) {
			numberOfThreads = Integer.parseInt(args[0]);
		}

		if (args.length > 1) {
			sizeOfNumberList = Integer.parseInt(args[1]);
		}

		if (args.length > 2) {
			repeats = Integer.parseInt(args[2]);
		}

		if (args.length > 3) {
			vthreads = Boolean.parseBoolean(args[3]);
		}

		WorkLoad workload = new WorkLoad(numberOfThreads, sizeOfNumberList, repeats, vthreads);
		workload.runWork();
		System.gc();
	}

	public void runWork() {
		final AtomicLong completionCount = new AtomicLong(0);
		Thread threads[] = new Thread[numberOfThreads];

		for (int i = 0; i < numberOfThreads; i++) {
			threads[i] = new Thread(() -> {
				workload();
				completionCount.incrementAndGet();
			});
			threads[i].start();
		}

		while (completionCount.get() < numberOfThreads) {
			System.out.println(completionCount.get() + " / " + numberOfThreads + " complete.");
			try {
				Thread.sleep(numberOfThreads * 100);
			} catch (InterruptedException e) {
				e.printStackTrace();
			}
		}

		System.out.println("All runs complete. " + average + " : " + stdDev);
	}

	public void runWorkWithVirtualThreads(int numVirtualThreads) {
		try {
			final AtomicLong completionCount = new AtomicLong(0);
			Thread[] threads = new Thread[numVirtualThreads];

			Class<?> threadClass = Thread.class;
			Method ofVirtualMethod = threadClass.getMethod("ofVirtual");
			Object virtualThreadBuilder = ofVirtualMethod.invoke(null);

			Class<?> builderClass = Class.forName("java.lang.Thread$Builder");
			Method unstartedMethod = builderClass.getMethod("unstarted", Runnable.class);

			Runnable task = () -> {
				burnCPU();
				generateTimedPark();
				burnCPU();
				completionCount.incrementAndGet();
			};

			for (int i = 0; i < numVirtualThreads; i++) {
				Thread vthread = (Thread) unstartedMethod.invoke(virtualThreadBuilder, task);
				threads[i] = vthread;
				vthread.start();
			}
		} catch (Exception e) {
			e.printStackTrace();
		}
	}

	private void workload() {
		for (int i = 0; i < repeats; i++) {
			generateAnonClasses();
			generateTimedPark();
			generateTimedSleep();
			generateTimedWait();
			throwThrowables();
			contendOnLock();
			burnCPU();
			generateClassLoader();
			if (VersionCheck.major() >= 17) {
				emitDataLoss();
			}
			if (vthreads) {
				runWorkWithVirtualThreads(8);
			}
		}
	}

	private void contendOnLock() {
		synchronized (globalLock) {
			globalCounter++;
			try {
				Thread.sleep(1);
			} catch (InterruptedException e) {
				e.printStackTrace();
			}
		}
	}

	private void recursiveFucntion(int depth) {
		if (0 == depth) {
			return;
		}

		recursiveFucntion(depth - 1);
	}

	private void recursiveFucntionWithCallable(int depth, Callable<?> c) {
		if (0 == depth) {
			try {
				c.call();
			} catch (Exception e) {
				// ignore
			}
			return;
		}

		recursiveFucntionWithCallable(depth - 1, c);
	}

	private void generateAnonClasses() {
		Callable<?> c = () -> {
			recursiveFucntion(30);
			return 0;
		};

		try {
			c.call();
		} catch (Exception e) {
			// ignore
		}
	}

	private void generateTimedPark() {
		recursiveFucntionWithCallable(10, () -> {
			LockSupport.parkNanos(100000);
			return 0;
		});
	}

	private void generateTimedSleep() {
		recursiveFucntionWithCallable(10, () -> {
			Thread.sleep(100);
			return 0;
		});
	}

	private void generateTimedWait() {
		recursiveFucntionWithCallable(10, () -> {
			Object o = new Object();

			synchronized (o) {
				o.wait(100);
			}

			return 0;
		});
	}

	private void burnCPU() {
		List<Double> numberList = new ArrayList<>();

		for (int i = 0; i < sizeOfNumberList; i++) {
			numberList.add(Math.random());
		}

		/* Write to public statics to avoid optimizing this code. */
		average += average(numberList);
		stdDev += standardDeviation(numberList);
	}

	static final class ClassLoaderTestClass { }

	private void emitDataLoss() {
		try {
			Class<?> jvmClass = Class.forName("jdk.jfr.internal.JVM");
			jvmClass.getMethod("emitDataLoss", long.class).invoke(null, 1024L);
		} catch (Exception e) {
			throw new RuntimeException(e);
		}
	}

	private void generateClassLoader() {
		try {
			ClassLoader classLoader = new URLClassLoader(
					new URL[] {
						WorkLoad.class.getProtectionDomain().getCodeSource().getLocation()
					},
					new URLClassLoader(new URL[] {}));

			classes.add(
					classLoader.loadClass("org.openj9.test.WorkLoad$ClassLoaderTestClass"));
		} catch (Exception e) {
			throw new RuntimeException(e);
		}
	}

	private double average(List<Double> numbers) {
		double sum = 0.0;

		for (double number : numbers) {
			sum += number;
		}

		return sum / numbers.size();
	}

	private double standardDeviation(List<Double> numbers) {
		double mean = average(numbers);
		double sumOfSquares = 0.0;

		for (double number : numbers) {
			sumOfSquares += Math.pow(number - mean, 2);
		}

		return Math.sqrt(sumOfSquares / (numbers.size() - 1));
	}

	private void throwThrowable(Class<? extends Throwable> throwable) {
		try {
			throw throwable.getDeclaredConstructor().newInstance();
		} catch (Throwable t) {
			// ignore
		}

		try {
			throw throwable.getDeclaredConstructor().newInstance("random string: " + Math.random());
		} catch (Throwable t) {
			// ignore
		}
	}

	private void throwThrowables() {
		throwThrowable(Throwable.class);
		throwThrowable(ClassFormatError.class);
		throwThrowable(StackOverflowError.class);
		throwThrowable(Exception.class);
		throwThrowable(ClassCastException.class);
		throwThrowable(NumberFormatException.class);
	}

}
