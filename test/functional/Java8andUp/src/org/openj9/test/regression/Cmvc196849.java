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
package org.openj9.test.regression;

import org.testng.annotations.Test;
import org.testng.AssertJUnit;
import org.testng.log4testng.Logger;

@Test(groups = {"level.extended", "req.cmvc.196849"})
public class Cmvc196849 { 
	
	@Test
	public void testInspect()  {
		try {
			Inspect.inspect();
		} catch (Throwable e) {
			e.printStackTrace();
			AssertJUnit.fail("Inspect failed to run to completion.");
		}				
	}
}

class Inspect implements Runnable { 
	static Thread a;
	static Thread b;
	static Thread c;
	static volatile boolean keepRunning = true;
	static double count = 0;
	static long time = 1000 * 60;
	private static Logger logger = Logger.getLogger(Inspect.class);
	
	public static void inspect() throws Throwable {
		Thread killer = new Thread() {
			public void run() {
				logger.info("Killer running - test will stop after " + time + " ms");
				try {
					Thread.sleep(time);
				} catch(Throwable t) {}
				logger.info("Shutting down.  Test completed successfully.");
				keepRunning = false;
			}
		};
		killer.start();
		a = new Thread(new Inspect(Thread.currentThread()), "A");
		a.start();
		b = new Thread(new Inspect(a), "B");
		b.start();
		c = new Thread(new Inspect(b), "C");
		c.start();
		new Inspect(c).run();
	}

	Thread toInspect;
	double myDouble = 0;

	public Inspect(Thread t) {
		toInspect = t;
	}

	public void run() {
		while (keepRunning) {
			m();
		}
	}

	void m() {
		myDouble += Math.sqrt(Math.random());
		StackTraceElement[] elements = toInspect.getStackTrace();
		myDouble += processStack(elements);
	}

	double processStack(StackTraceElement[] elements) {
		for(StackTraceElement e : elements) {
			count += 1;
			if ((count % 40000) == 0) {
				logger.debug(Thread.currentThread() + " inspecting: " + e);
			}
		}
		return count;
	}
}
