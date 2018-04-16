/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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
package org.openj9.test.java.lang.management.ThreadMXBean;

import java.lang.management.ManagementFactory;
import java.lang.management.ThreadMXBean;

import org.testng.annotations.Test;
import org.testng.asserts.SoftAssert;
import org.testng.log4testng.Logger;

public class APITest2 {
	private SoftAssert softAssert = new SoftAssert();
	private static Logger logger = Logger.getLogger(APITest2.class);
	
	ThreadMXBean mxb = ManagementFactory.getThreadMXBean();
	boolean currentTimeSupported = false;
	boolean anyTimeSupported = false;
	boolean timesEnabled = false;

	private boolean isResultCorrect(String msg, boolean supported,
			boolean enabled, boolean gotException, long cpuTime) {
		boolean ok = true;

		logger.debug("gotException: " + gotException + " cpuTime: " + cpuTime);
		if (supported) {
			if (gotException) {
				ok = false;
				errorPrintln(msg + ": unexpected exception");
			} else {
				if (enabled) {
					if (-1 == cpuTime) {
						ok = false;
						errorPrintln(msg + ": unexpected -1 returned");
					}
				} else {
					if (-1 != cpuTime) {
						errorPrintln(msg + ": expected -1 returned: " + cpuTime);
						ok = false;
					}
				}
			}
		} else {
			if (!gotException) {
				errorPrintln(msg + ": expected exception not thrown");
				ok = false;
			}
		}

		return ok;
	}

	private boolean testCpuTimeEnable() {
		printTestDesc();

		boolean ok = true;
		boolean enableUnsupported = false;

		try {
			mxb.setThreadCpuTimeEnabled(true);
		} catch (UnsupportedOperationException ue) {
			enableUnsupported = true;
		}

		if (currentTimeSupported || anyTimeSupported) {
			if (enableUnsupported) {
				errorPrintln("unexpected exception");
				ok = false;
			} else {
				ok = true;
			}
		} else {
			if (enableUnsupported) {
				ok = true;
			} else {
				errorPrintln("expected exception not thrown");
				ok = false;
			}
		}

		printResult(ok);
		return ok;
	}

	private boolean testCpuTimeDisable() {
		printTestDesc();

		boolean ok = true;
		boolean gotException = false;

		try {
			mxb.setThreadCpuTimeEnabled(false);
		} catch (UnsupportedOperationException ue) {
			gotException = true;
		}

		if (currentTimeSupported || anyTimeSupported) {
			if (gotException) {
				errorPrintln("unexpected exception");
				ok = false;
			} else {
				ok = true;
			}
		} else {
			if (gotException) {
				ok = true;
			} else {
				errorPrintln("expected exception not thrown");
				ok = false;
			}
		}

		printResult(ok);
		return ok;
	}

	private boolean isGetEnabledOk(boolean isEnabled) {
		printTestDesc();
		boolean ok = true;
		boolean gotException = false;
		boolean ret = true;

		try {
			ret = mxb.isThreadCpuTimeEnabled();
		} catch (UnsupportedOperationException ue) {
			gotException = true;
		}

		if (currentTimeSupported || anyTimeSupported) {
			if (gotException) {
				errorPrintln("unexpected exception");
				ok = false;
			} else {
				if (ret != isEnabled) {
					errorPrintln("unexpected return value, expected: " + isEnabled + ", got: " + ret);
				}
			}
		} else {
			if (!gotException) {
				errorPrintln("expected exception not thrown");
				ok = false;
			}
		}

		printResult(ok);
		return ok;
	}

	private boolean isGetCurrentThreadCpuTimeOk(boolean isEnabled) {
		printTestDesc();

		boolean ok = true;
		boolean gotException = false;
		long cpuTime = -1;

		try {
			cpuTime = mxb.getCurrentThreadCpuTime();
		} catch (UnsupportedOperationException ue) {
			gotException = true;
		}
		ok = isResultCorrect("getCurrentThreadCpuTime, enabled(" + isEnabled
				+ ")", currentTimeSupported, isEnabled, gotException, cpuTime);

		printResult(ok);
		return ok;
	}

	private boolean isGetCurrentThreadUserTimeOk(boolean isEnabled) {
		printTestDesc();

		boolean ok = true;
		boolean gotException = false;
		long cpuTime = -1;

		try {
			cpuTime = mxb.getCurrentThreadUserTime();
		} catch (UnsupportedOperationException ue) {
			gotException = true;
		}
		ok = isResultCorrect("getCurrentThreadUserTime, enabled(" + isEnabled
				+ ")", currentTimeSupported, isEnabled, gotException, cpuTime);

		printResult(ok);
		return ok;
	}

	private boolean isGetThreadCpuTimeOk(Thread t, boolean isEnabled) {
		printTestDesc();

		boolean ok = true;
		boolean gotException = false;
		long cpuTime = -1;

		try {
			cpuTime = mxb.getThreadCpuTime(t.getId());
		} catch (UnsupportedOperationException ue) {
			gotException = true;
		}
		ok = isResultCorrect("getThreadCpuTime, enabled(" + isEnabled + ")",
				anyTimeSupported, isEnabled, gotException, cpuTime);

		printResult(ok);
		return ok;
	}

	private boolean isGetThreadUserTimeOk(Thread t, boolean isEnabled) {
		printTestDesc();

		boolean ok = true;
		boolean gotException = false;
		long cpuTime = -1;

		try {
			cpuTime = mxb.getThreadUserTime(t.getId());
		} catch (UnsupportedOperationException ue) {
			gotException = true;
		}
		ok = isResultCorrect("getThreadUserTime, enabled(" + isEnabled + ")",
				anyTimeSupported, isEnabled, gotException, cpuTime);

		printResult(ok);
		return ok;
	}

	@Test(groups = { "level.extended" })
	public void testAPITest2() {
		Object exitBarrier = new Object();
		Thread self;
		Thread child;

		self = Thread.currentThread();
		currentTimeSupported = mxb.isCurrentThreadCpuTimeSupported();
		anyTimeSupported = mxb.isThreadCpuTimeSupported();

		timesEnabled = false;
		try {
			mxb.setThreadCpuTimeEnabled(false);
		} catch (UnsupportedOperationException ue) {
		}

		synchronized (exitBarrier) {
			child = new ChildThread(exitBarrier);
			child.start();

			softAssert.assertTrue(testCpuTimeEnable());
			softAssert.assertTrue(testCpuTimeDisable());
			runGetTimeTests(false, self, child);
			runGetTimeTests(true, self, child);
		}

		child.interrupt();
		try {
			child.join();
		} catch (InterruptedException e) {
			e.printStackTrace();
		}

		logger.debug("currentThreadSupported: " + currentTimeSupported
				+ " anyThreadSupported: " + anyTimeSupported);
		
		softAssert.assertAll();
	}

	private void runGetTimeTests(boolean isEnabled, Thread self, Thread child) {
		timesEnabled = isEnabled;
		try {
			mxb.setThreadCpuTimeEnabled(isEnabled);
		} catch (UnsupportedOperationException ue) {
			/* do nothing */
		}
		softAssert.assertTrue(isGetEnabledOk(isEnabled));
		softAssert.assertTrue(isGetCurrentThreadCpuTimeOk(isEnabled));
		softAssert.assertTrue(isGetCurrentThreadUserTimeOk(isEnabled));
		softAssert.assertTrue(isGetThreadCpuTimeOk(self, isEnabled));
		softAssert.assertTrue(isGetThreadUserTimeOk(self, isEnabled));
		softAssert.assertTrue(isGetThreadCpuTimeOk(child, isEnabled));
		softAssert.assertTrue(isGetThreadUserTimeOk(child, isEnabled));
		softAssert.assertAll();
	}

	class ChildThread extends Thread {
		Object exitBarrier;

		ChildThread(Object exitBarrier) {
			super("child");
			this.exitBarrier = exitBarrier;
		}

		@Override
		public void run() {
			try {
				while (true) {
					Thread.sleep(500);
				}
			} catch (InterruptedException e) {
			}
			synchronized (exitBarrier) {
				exitBarrier.notify();
			}
		}
	}

	private void printTestDesc() {
		Throwable t = new Throwable();
		t.fillInStackTrace();
		logger.debug("\n");
		logger.debug("Test: " + t.getStackTrace()[1].getMethodName());
		if (timesEnabled) {
			logger.debug(" (times enabled)");
		} else {
			logger.debug(" (times disabled)");
		}
	}

	private void printResult(boolean ok) {
		if (ok) {
			logger.debug("passed");
		} else {
			logger.error("failed");
		}
	}

	private void errorPrintln(String s) {
		logger.error(s);
	}
}
