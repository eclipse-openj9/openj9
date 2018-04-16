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

public class Microbench {
	private static Logger logger = Logger.getLogger(Microbench.class);
	Timer timer;
	static int staticInt = 0;
	static double staticDouble = 0.0;
	static long staticLong = 0L;
	
	int anInt;
	long aLong;
	double aDouble;
/**
 * Microbench constructor comment.
 */
public Microbench() {
	anInt = 1;
	aLong = 1L;
	aDouble = 0.0;
	timer = new Timer ();
}

@Test(groups = { "level.sanity","component.jit" })
public void testMicrobench() {
	int dummy;
	long dummyLong;
	double dummyDouble;

	// ------
	timer.reset();
	for (int i = 0; i < 10000000; i++) {
		dummy = Microbench.staticInt;
	}
	timer.mark();
	logger.info("10000000 Static Int fetch = "+ Long.toString(timer.delta()));
	timer.reset();
	for (int i = 0; i < 10000000; i++) {
		dummyLong = Microbench.staticLong;
	}
	timer.mark();
	logger.info("10000000 Static Long fetch = "+ Long.toString(timer.delta()));
	timer.reset();
	for (int i = 0; i < 10000000; i++) {
		dummyDouble = Microbench.staticDouble;
	}
	timer.mark();
	logger.info("10000000 Static Double fetch = "+ Long.toString(timer.delta()));
	// ------


	timer.reset();
	for (int i = 0; i < 10000000; i++) {
		dummy = anInt;
	}
	timer.mark();
	logger.info("10000000 Int fetch = "+ Long.toString(timer.delta()));
	timer.reset();
	for (int i = 0; i < 10000000; i++) {
		dummyLong = aLong;
	}
	timer.mark();
	logger.info("10000000 Long fetch = "+ Long.toString(timer.delta()));
	timer.reset();
	for (int i = 0; i < 10000000; i++) {
		dummyDouble = aDouble;
	}
	timer.mark();
	logger.info("10000000 Double fetch = "+ Long.toString(timer.delta()));
 ;
	return;
}
}
