/*******************************************************************************
 * Copyright (c) 2006, 2018 IBM Corp. and others
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

package jit.test.vich;

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import jit.test.vich.utils.Timer;

public class Fibonacci {

private static Logger logger = Logger.getLogger(Fibonacci.class);
Timer timer;

public Fibonacci() {
	timer = new Timer ();
}

int fibonacci(int x) {
	if (x < 3)
		return 1;
	return (fibonacci(x - 1) + fibonacci(x - 2));
}
static int fibonacciStatic(int x) {
	if (x < 3)
		return 1;
	return (fibonacciStatic(x - 1) + fibonacciStatic(x - 2));
}
static synchronized int fibonacciStaticSync(int x) {
	if (x < 3)
		return 1;
	return (fibonacciStaticSync(x - 1) + fibonacciStaticSync(x - 2));
}
synchronized int fibonacciSync(int x) {
	if (x < 3)
		return 1;
	return (fibonacciSync(x - 1) + fibonacciSync(x - 2));
}
@Test(groups = { "level.sanity","component.jit" })
public void testFibonacci() {
	int fib = 12;
	for (int iterations = 10000; iterations <= 30000; iterations += 10000) {
		timer.reset();
		for (int i = 0; i < iterations; i++)
			fibonacci(fib);
		timer.mark();
		logger.info(Integer.toString(iterations) + " x (virtual) fibonacci(" + Integer.toString(fib) + ") = " + Long.toString(timer.delta()));
	}
	for (int iterations = 10000; iterations <= 30000; iterations += 10000) {
		timer.reset();
		for (int i = 0; i < iterations; i++)
			fibonacciSync(fib);
		timer.mark();
		logger.info(Integer.toString(iterations) + " x (virtual, synchronized) fibonacci(" + Integer.toString(fib) + ") = " + Long.toString(timer.delta()));
	}
	for (int iterations = 10000; iterations <= 30000; iterations += 10000) {
		timer.reset();
		for (int i = 0; i < iterations; i++)
			fibonacciStatic(fib);
		timer.mark();
		logger.info(Integer.toString(iterations) + " x (static) fibonacci(" + Integer.toString(fib) + ") = " + Long.toString(timer.delta()));
	}
	for (int iterations = 10000; iterations <= 30000; iterations += 10000) {
		timer.reset();
		for (int i = 0; i < iterations; i++)
			fibonacciStaticSync(fib);
		timer.mark();
		logger.info(Integer.toString(iterations) + " x (static, synchronized) fibonacci(" + Integer.toString(fib) + ") = " + Long.toString(timer.delta()));
	} 
	return;
}
}
