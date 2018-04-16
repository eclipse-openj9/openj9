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

import java.lang.management.*;
import java.util.concurrent.locks.*;

import org.openj9.test.fileLock.TestFileLocking;
import org.testng.annotations.Test;
import org.testng.asserts.SoftAssert;
import org.testng.log4testng.Logger;

/*
 * Verify lockedSynchronizers, lockedMonitors usage
 */
public class APITest extends ThreadMXBeanTestCase {
	class LockClass { }
	
	private SoftAssert softAssert = new SoftAssert();
	ThreadMXBean mxb = ManagementFactory.getThreadMXBean();

	long[] tids;

	final ReentrantLock r1 = new ReentrantLock();
	final ReentrantLock r2 = new ReentrantLock();
	
	Thread syncOwner = new Thread("syncOwner") {
		public void run() {
			r2.lock();
			r1.lock();
			r2.unlock();
			r1.unlock();
		}
	};

	Thread monOwner = new Thread("monOwner") {
		public void run() {
			LockClass lock = new LockClass();
			synchronized (lock) {
				try {
					while (true) {
						Thread.sleep(1000);
					}
				} catch (InterruptedException e) {
				}
			}
		}
	};

	Thread ownsNada = new Thread("ownsNada") {
		public void run() {
			try {
				while (true) {
					Thread.sleep(1000);
				}
			} catch (InterruptedException e) {
			}
		}
	};
	
	Thread deadThread = new Thread("dead") {
		public void run() {
			return;
		}
	};

	@Test(groups = { "level.extended" })
	public void testAPITest() {
		LockClass lock = new LockClass();
		synchronized (lock) {
			r1.lock();
			deadThread.start();
			syncOwner.start();
			monOwner.start();
			ownsNada.start();

			try {
				deadThread.join();
				Thread.sleep(1000);
			} catch (InterruptedException e) {
				e.printStackTrace();
			}

			tids = new long[5];
			tids[0] = Thread.currentThread().getId();
			tids[1] = syncOwner.getId();
			tids[2] = monOwner.getId();
			tids[3] = deadThread.getId();
			tids[4] = ownsNada.getId();
			
			
			softAssert.assertTrue(testLockedMonitorsSupported());
			softAssert.assertTrue(testLockedSynchronizersSupported());

			synchronized (lock) {
				softAssert.assertTrue(testDumpAllThreads(tids, true, true));
				softAssert.assertTrue(testDumpAllThreads(tids, false, false));
				softAssert.assertTrue(testDumpAllThreads(tids, true, false));
				softAssert.assertTrue(testDumpAllThreads(tids, false, true));

				softAssert.assertTrue(testGetArrayOfThreads(tids, true, true));
				softAssert.assertTrue(testGetArrayOfThreads(tids, false, false));
				softAssert.assertTrue(testGetArrayOfThreads(tids, true, false));
				softAssert.assertTrue(testGetArrayOfThreads(tids, false, true));
				
				softAssert.assertTrue(testNullArrayOfThreads(true, true));
				softAssert.assertTrue(testNullArrayOfThreads(false, false));
				softAssert.assertTrue(testNullArrayOfThreads(true, false));
				softAssert.assertTrue(testNullArrayOfThreads(false, true));

				softAssert.assertTrue(testEmptyArrayOfThreads(true, true));
				softAssert.assertTrue(testEmptyArrayOfThreads(false, false));
				softAssert.assertTrue(testEmptyArrayOfThreads(true, false));
				softAssert.assertTrue(testEmptyArrayOfThreads(false, true));
			}

			r1.unlock();
			monOwner.interrupt();
			ownsNada.interrupt();
			try {
				syncOwner.join();
				monOwner.join();
				ownsNada.join();
			} catch (InterruptedException e) {
				e.printStackTrace();
			}	
		}
		softAssert.assertAll();
	}

	boolean testLockedMonitorsSupported() {
		printTestDesc("Test that locked monitor usage is supported");

		boolean ok = true;
		ok = (mxb.isObjectMonitorUsageSupported() == true);
		printResult(ok);
		return ok;
	}

	boolean testLockedSynchronizersSupported() {
		printTestDesc("Test that locked synchronizer usage is supported");

		boolean ok = true;
		ok = (mxb.isSynchronizerUsageSupported() == true);
		printResult(ok);
		return ok;
	}

	boolean testDumpAllThreads(long[] tid, boolean getLockedMonitors,
			boolean getLockedSynchronizers) {
		printTestDesc("DumpAllThreads: lockedMonitors=" + getLockedMonitors
				+ " lockedSyncs=" + getLockedSynchronizers);

		boolean ok = true;
		ThreadInfo[] ti;

		ti = mxb.dumpAllThreads(getLockedMonitors, getLockedSynchronizers);
		ok = matchUnorderedThreadInfo(tid, ti, getLockedMonitors, getLockedSynchronizers);
		printResult(ok);
		if (!ok) {
			printThreadInfo(ti);
		}
		return ok;
	}

	boolean testGetArrayOfThreads(long[] tid, boolean getLockedMonitors,
			boolean getLockedSynchronizers) {
		printTestDesc("GetArrayOfThreads: tid=" + tid + " lockedMonitors="
				+ getLockedMonitors + " lockedSyncs=" + getLockedSynchronizers);

		boolean ok = true;
		ThreadInfo[] ti;

		ti = mxb.getThreadInfo(tid, getLockedMonitors, getLockedSynchronizers);
		ok = matchOrderedThreadInfo(tid, ti, getLockedMonitors, getLockedSynchronizers);

		printResult(ok);
		if (!ok) {
			printThreadInfo(ti);
		}
		return ok;
	}

	boolean testNullArrayOfThreads(boolean getLockedMonitors, boolean getLockedSynchronizers) {
		printTestDesc("GetArrayOfThreads: tid=null lockedMonitors=" + getLockedMonitors + " lockedSyncs=" + getLockedSynchronizers);
		
		boolean ok = true;
		ThreadInfo[] ti = null;
		
		boolean caughtException = false;
		try {
			ti = mxb.getThreadInfo(null, getLockedMonitors, getLockedSynchronizers);
		} catch (NullPointerException e) {
			caughtException = true;
		}
		ok = caughtException;
		
		printResult(ok);
		if (!ok) {
			printThreadInfo(ti);
		}
		return ok;
	}
	
	/* 
	 * Note: sun jdk throws IllegalArgumentException, but IBM jdk works.
	 * Spec says IllegalArgumentException is thrown if a thread id <=0.
	 */
	boolean testEmptyArrayOfThreads(boolean getLockedMonitors, boolean getLockedSynchronizers) {
		printTestDesc("GetArrayOfThreads: tid=[] lockedMonitors=" + getLockedMonitors + " lockedSyncs=" + getLockedSynchronizers);
		
		boolean ok = true;
		ThreadInfo[] ti = null;

		long[] emptytids = new long[0];
		boolean caughtException = false;
		try {
			ti = mxb.getThreadInfo(emptytids, getLockedMonitors, getLockedSynchronizers);
		} catch (IllegalArgumentException e) {
			caughtException = true;
		}
		if (caughtException) {
			ok = false;
		} else if (ti == null) {
			ok = false;
		} else if (ti.length != 0) {
			ok = false;
		}
		
		printResult(ok);
		if (!ok) {
			printThreadInfo(ti);
		}
		return ok;
	}
	
	boolean testDeadThreads(boolean getLockedMonitors, boolean getLockedSynchronizers) {
		boolean ok = true;
		ThreadInfo[] ti = null;
		
		printResult(ok);
		if (!ok) {
			printThreadInfo(ti);
		}
		return ok;
	}

	boolean matchOrderedThreadInfo(long[] tid, ThreadInfo[] ti, boolean getLockedMonitors, boolean getLockedSynchronizers) {
		boolean ok = true;
		
		if (ti == null) {
			ok = false;
		} else if (ti.length != tid.length) {
				ok = false;
		} else {

			for (int i = 0; i < tid.length; ++i) {
				if (tid[i] == deadThread.getId()) {
					if (ti[i] != null) {
						logger.error("non-null ThreadInfo for dead thread");
						ok = false;
					}
				} else {
					long ttid = ti[i].getThreadId();
					
					if (tid[i] != ttid) {
						logger.error("threadinfo out of order");
						ok = false;
						break;
					}
					
					if (ti[i].getLockedMonitors() == null) {
						logger.error("tid " + ttid + " should have non-null locked monitors");
						ok = false;
					} else {
						if (getLockedMonitors == false) {
							if (ti[i].getLockedMonitors().length != 0) {
								logger.error("tid " + ttid + " should have 0 locked monitors");
								ok = false;
							}
						} else {
	
							if ((ttid == ownsNada.getId())
									|| (ttid == syncOwner.getId())) {
								if (ti[i].getLockedMonitors().length != 0) {
									logger.error("tid " + ttid + " should have 0 locked monitors");
									ok = false;
								}
							} else if ((ttid == monOwner.getId())
									|| (ttid == Thread.currentThread().getId())) {
								if (ti[i].getLockedMonitors().length <= 0) {
									logger.error("tid " + ttid + " should have >0 locked monitors");
									ok = false;
								}
							}
						}
					}

					if (ti[i].getLockedSynchronizers() == null) {
						logger.error("tid " + ttid + " should have non-null locked syncs");
						ok = false;
					} else {
						if (getLockedSynchronizers == false) {
							if (ti[i].getLockedSynchronizers().length != 0) {
								logger.error("tid " + ttid + " should have 0 locked syncs");
								ok = false;
							}
						} else {
							if ((ttid == ownsNada.getId())
									| (ttid == monOwner.getId())) {
								if (ti[i].getLockedSynchronizers().length != 0) {
									logger.error("tid " + ttid + " should have 0 locked syncs");
									ok = false;
								}
							} else if ((ttid == syncOwner.getId())
									|| (ttid == Thread.currentThread().getId())) {
								if (ti[i].getLockedSynchronizers().length <= 0) {
									logger.error("tid " + ttid + " should have >0 locked syncs");
									ok = false;
								}
							}
						}
					}
	
					if (ti[i].getStackTrace() == null) {
						logger.error("tid " + ttid + " should have non-null stack trace");
						ok = false;
					}
				}
			}
		}
		
		return ok;
	}
	
	boolean matchUnorderedThreadInfo(long[] tid, ThreadInfo[] ti, boolean getLockedMonitors, boolean getLockedSynchronizers) {
		boolean ok = true;

		if (ti == null) {
            ok = false;
        } else if (ti.length < (tid.length - 1) ) {
                logger.error("ti.length < (tid.length - 1)");
                ok = false;
		} else {

			for (int i = 0; i < ti.length; ++i) {
				long ttid = ti[i].getThreadId();

				if (ti[i].getLockedMonitors() == null) {
					logger.error("tid " + ttid + " should have non-null locked monitors");
					ok = false;
				} else {
					if (getLockedMonitors == false) {
						if (ti[i].getLockedMonitors().length != 0) {
							logger.error("tid " + ttid + " should have 0 locked monitors");
							ok = false;
						}
					} else {

						if ((ttid == ownsNada.getId())
								|| (ttid == syncOwner.getId())) {
							if (ti[i].getLockedMonitors().length != 0) {
								logger.error("tid " + ttid + " should have 0 locked monitors");
								ok = false;
							}
						} else if ((ttid == monOwner.getId())
								|| (ttid == Thread.currentThread().getId())) {
							if (ti[i].getLockedMonitors().length <= 0) {
								logger.error("tid " + ttid + " should have >0 locked monitors");
								ok = false;
							}
						}
					}
				}

				if (ti[i].getLockedSynchronizers() == null) {
					logger.error("tid " + ttid + " should have non-null locked syncs");
					ok = false;
				} else {
					if (getLockedSynchronizers == false) {
						if (ti[i].getLockedSynchronizers().length != 0) {
							logger.error("tid " + ttid + " should have 0 locked syncs");
							ok = false;
						}
					} else {
						if ((ttid == ownsNada.getId())
								| (ttid == monOwner.getId())) {
							if (ti[i].getLockedSynchronizers().length != 0) {
								logger.error("tid " + ttid + " should have 0 locked syncs");
								ok = false;
							}
						} else if ((ttid == syncOwner.getId())
								|| (ttid == Thread.currentThread().getId())) {
							if (ti[i].getLockedSynchronizers().length <= 0) {
								logger.error("tid " + ttid + " should have >0 locked syncs");
								ok = false;
							}
						}
					}
				}

				if (ti[i].getStackTrace() == null) {
					logger.error("tid " + ttid + " should have non-null stack trace");
					ok = false;
				}
			}
		}
		return ok;
	}

	void printTestDesc(String s) {
		logger.debug(s);
	}

	void printResult(boolean ok) {
		if (ok) {
			logger.debug("PASSED");
		} else {
			logger.error("FAILED");
		}
	}
}
