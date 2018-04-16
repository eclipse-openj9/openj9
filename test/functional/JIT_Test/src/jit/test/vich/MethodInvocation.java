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

public class MethodInvocation implements TestInterface {
	
private static Logger logger = Logger.getLogger(MethodInvocation.class);
Timer timer;
private static final int defaultCount = 1000000;

public MethodInvocation() {
	timer = new Timer ();
}

@Test(groups = { "level.sanity","component.jit" })
public void testMethodInvocation() {
	TestInterface inter = (TestInterface) this;
	int count;
	String countString;
	
	countString = System.getProperty("vich.test.run.count");
	if (countString == null) {
		count = defaultCount;
	} else {
		count = Integer.parseInt(countString);
	}

	/* Resolve the method refs before starting the bench */

	staticCall();
	specialCall();
	virtualCall();
	inter.interfaceCall();

	timer.reset();
	for (int i = 0; i < count; i++) {
		staticCall();
	}
	timer.mark();
	logger.info(count + " Method Calls (Static)    = "+ Long.toString(timer.delta()));
	timer.reset();
	for (int i = 0; i < count; i++) {
		specialCall();
	}
	timer.mark();
	logger.info(count + " Method Calls (Special)   = "+ Long.toString(timer.delta()));
	timer.reset();
	for (int i = 0; i < count; i++) {
		virtualCall();
	}
	timer.mark();
	logger.info(count + " Method Calls (Virtual)   = "+ Long.toString(timer.delta()));
	timer.reset();
	for (int i = 0; i < count; i++) {
		inter.interfaceCall();
	}
	timer.mark();
	logger.info(count + " Method Calls (Interface) = "+ Long.toString(timer.delta()));

synchronized(this) { synchronized(this.getClass()) {

	timer.reset();
	for (int i = 0; i < count; i++) {
		staticCall();
	}
	timer.mark();
	logger.info(count + " Method Calls (Static)    = "+ Long.toString(timer.delta()));
	timer.reset();
	for (int i = 0; i < count; i++) {
		specialCall();
	}
	timer.mark();
	logger.info(count + " Method Calls (Special)   = "+ Long.toString(timer.delta()));
	timer.reset();
	for (int i = 0; i < count; i++) {
		virtualCall();
	}
	timer.mark();
	logger.info(count + " Method Calls (Virtual)   = "+ Long.toString(timer.delta()));
	timer.reset();
	for (int i = 0; i < count; i++) {
		inter.interfaceCall();
	}
	timer.mark();
	logger.info(count + " Method Calls (Interface) = "+ Long.toString(timer.delta()));

}}

	return;
}

public synchronized void interfaceCall() {}
public synchronized static void staticCall () {}
public synchronized void virtualCall () {}
private synchronized void specialCall () {}

}
