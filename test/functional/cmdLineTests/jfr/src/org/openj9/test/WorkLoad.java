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

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.locks.LockSupport;

public class WorkLoad {
	private int numberOfThreads;
	private int sizeOfNumberList;
	private int repeats;

	public static double average;
	public static double stdDev;

	private static long globalCounter = 0;
	static interface GlobalLoack {};
	private static Object globalLock = new GlobalLoack(){};

	public WorkLoad(int numberOfThreads, int sizeOfNumberList, int repeats) {
		this.numberOfThreads = numberOfThreads;
		this.sizeOfNumberList = sizeOfNumberList;
		this.repeats = repeats;
	}

	public static void main(String[] args) {
		int numberOfThreads = 100;
		int sizeOfNumberList = 10000;
		int repeats = 50;

		if (args.length > 0) {
			numberOfThreads = Integer.parseInt(args[0]);
		}

		if (args.length > 1) {
			sizeOfNumberList = Integer.parseInt(args[1]);
		}

		if (args.length > 2) {
			repeats = Integer.parseInt(args[2]);
		}

		WorkLoad workload = new WorkLoad(numberOfThreads, sizeOfNumberList, repeats);
		workload.runWork();
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

	private void workload() {
		for (int i = 0; i < repeats; i++) {
			generateAnonClasses();
			generateTimedPark();
			generateTimedSleep();
			generateTimedWait();
			throwThrowables();
			contendOnLock();
			burnCPU();
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
			} catch (Exception e) {}
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
		} catch (Exception e) {}
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

			synchronized(o) {
				o.wait(100);
			}

			return 0;
		});
	}

	private void burnCPU() {
		ArrayList<Double> numberList = new ArrayList<Double>();

		for (int i = 0; i < sizeOfNumberList; i++) {
			numberList.add(Math.random());
		}

		/* Write to public statics to avoid optimizing this code. */
		average += average(numberList);
		stdDev += standardDeviation(numberList);
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
		} catch (Throwable t) {}

		try {
			throw throwable.getDeclaredConstructor().newInstance("random string: " + Math.random());
		} catch (Throwable t) {}
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
