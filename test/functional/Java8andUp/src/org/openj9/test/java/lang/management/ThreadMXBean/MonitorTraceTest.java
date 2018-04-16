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
import java.util.Vector;

import org.testng.annotations.Test;
import org.testng.asserts.SoftAssert;
import org.testng.log4testng.Logger;

import java.util.Iterator;

public class MonitorTraceTest extends ThreadMXBeanTestCase {
	private SoftAssert softAssert = new SoftAssert();

	ThreadMXBean mxb = ManagementFactory.getThreadMXBean();

	@Test(groups = { "level.extended" })
	public void testMonitorTraceTest() {
		softAssert.assertTrue(testSameFrame());
		softAssert.assertTrue(testMultiFrame());
		softAssert.assertTrue(testJniUnscoped());
		softAssert.assertAll();
	}

	boolean testSameFrame() {
		boolean ok = true;
		printTestDesc();

		long tid[] = { Thread.currentThread().getId() };
		ThreadInfo[] tinfo;
		int bottomframe = 0;

		tinfo = mxb.getThreadInfo(tid, true, false);
		if (tinfo == null) {
			ok = false;
		} else if (tinfo.length != 1) {
			ok = false;
		} else {
			if (tinfo[0].getLockedMonitors().length != 0) {
				ok = false;
			}
		}
		if (!ok) {
			printThreadInfo(tinfo);
		}
		for (int i = 0; i < tinfo[0].getStackTrace().length; i++) {
			if (tinfo[0].getStackTrace()[i].getMethodName().equals("testSameFrame")) {
				bottomframe = i;
				break;
			}
		}

		LockClass lockA = new LockClass("testSameFrame.lockA");
		LockClass lockB = new LockClass("testSameFrame.lockB");
		
		/* only 1 monitor locked */
		synchronized (lockA) {
			tinfo = mxb.getThreadInfo(tid, true, false);
			if (tinfo[0].getLockedMonitors().length != 1) {
				logger.error("Error: wrong number of lockedMonitors");
				ok = false;
			}
			if (tinfo[0].getLockedMonitors().length > 0) {
				if ((bottomframe != tinfo[0].getLockedMonitors()[0].getLockedStackDepth()) ||
						!tinfo[0].getLockedMonitors()[0].getLockedStackFrame().equals(tinfo[0].getStackTrace()[bottomframe])) {
					logger.error("Error: lockA frame wrong");
					ok = false;
				}
			}
			if (!ok) {
				printThreadInfo(tinfo);
			}
			
			/* 2 monitors locked */
			synchronized (lockB) {
				tinfo = mxb.getThreadInfo(tid, true, false);
				if (tinfo[0].getLockedMonitors().length != 2) {
					logger.error("Error: there should be 2 lockedMonitors");
					ok = false;
				}
				for (int i = 0; i < tinfo[0].getLockedMonitors().length; ++i) {
					if ((bottomframe != tinfo[0].getLockedMonitors()[i].getLockedStackDepth()) ||
							!tinfo[0].getLockedMonitors()[i].getLockedStackFrame().equals(tinfo[0].getStackTrace()[bottomframe])) {
						logger.error("Error: (2) frame wrong " + tinfo[0].getLockedMonitors()[i]);
						ok = false;
					}
				}
				if (!ok) {
					printThreadInfo(tinfo);
				}
				
				/* consecutively recursive lock */
				synchronized (lockB) {
					tinfo = mxb.getThreadInfo(tid, true, false);
					if (tinfo[0].getLockedMonitors().length != 3) {
						logger.error("Error: there should be 3 lockedMonitors");
						ok = false;
					}
					for (int i = 0; i < tinfo[0].getLockedMonitors().length; ++i) {
						if ((bottomframe != tinfo[0].getLockedMonitors()[i].getLockedStackDepth()) ||
								!tinfo[0].getLockedMonitors()[i].getLockedStackFrame().equals(tinfo[0].getStackTrace()[bottomframe])) {
							logger.error("Error: (3) frame wrong " + tinfo[0].getLockedMonitors()[i]);
							ok = false;
						}
					}
					if (!ok) {
						printThreadInfo(tinfo);
					}
					
					/* non-consecutive recursive lock */ 
					synchronized (lockA) {
						tinfo = mxb.getThreadInfo(tid, true, false);
						if (tinfo[0].getLockedMonitors().length != 4) {
							logger.error("Error: there should be 4 lockedMonitors");
							ok = false;
						}
						for (int i = 0; i < tinfo[0].getLockedMonitors().length; ++i) {
							if ((bottomframe != tinfo[0].getLockedMonitors()[i].getLockedStackDepth()) ||
									!tinfo[0].getLockedMonitors()[i].getLockedStackFrame().equals(tinfo[0].getStackTrace()[bottomframe])) {
								logger.error("Error: (4) frame wrong " + tinfo[0].getLockedMonitors()[i]);
								ok = false;
							}
						}
						if (!ok) {
							printThreadInfo(tinfo);
						}
					}
				}
			}
		}

		tinfo = mxb.getThreadInfo(tid, true, false);
		if (tinfo[0].getLockedMonitors().length != 0) {
			logger.error("Error: there should be no locked monitors");
			ok = false;
		}
		if (!ok) {
			printThreadInfo(tinfo);

			logger.error("lockA " + lockA);
			logger.error("lockB " + lockB);
		}

		printResult(ok);
		return ok;
	}

	boolean testMultiFrame() {
		boolean ok = true;
		printTestDesc();
		MonitorRecordList monitors = new MonitorRecordList();
		
		LockClass lockA = new LockClass("lockA");
		LockClass lockB = new LockClass("lockB");
		LockClass lockC = new LockClass("lockC");
		
		class SyncTesterA {
			long[] tid = { Thread.currentThread().getId() };
			ThreadInfo tinfo;

			synchronized void testfunc(LockClass lock) {
				j9vm.test.monitor.Helpers.monitorEnter(lock);
				synchronized (lock) {
					tinfo = getMoninfo()[0];
				}
				j9vm.test.monitor.Helpers.monitorExit(lock);
			}
			
			synchronized ThreadInfo[] getMoninfo() {
				ThreadInfo tinfo[] = mxb.getThreadInfo(tid, true, false);
				return tinfo;
			}
		};
		
		class SyncTesterB {
			synchronized void testfunc2(SyncTesterA syncA, LockClass lock) {
				synchronized (syncA) {
					syncA.testfunc(lock);
				}
			}
			void testfunc(SyncTesterA syncA, LockClass lock) {
				testfunc2(syncA, lock);
			}
		};
		
		SyncTesterA syncA = new SyncTesterA();
		SyncTesterB syncB = new SyncTesterB();

		synchronized (lockA) {
			j9vm.test.monitor.Helpers.monitorEnter(lockB);
			synchronized (lockC) {
				syncB.testfunc(syncA, lockA);
			}
			j9vm.test.monitor.Helpers.monitorExit(lockB);
		}
	
		ThreadInfo tinfo = syncA.tinfo;
		
		/* fill in expected results */
		int bottomframe = 0;
		for (int i = 0; i < tinfo.getStackTrace().length; i++) {
			if (tinfo.getStackTrace()[i].getMethodName().equals("testMultiFrame")) {
				bottomframe = i;
				break;
			}
		}
		
		monitors.add(new MonitorRecord(lockA.toString(), bottomframe - 0, false));
		monitors.add(new MonitorRecord(lockA.toString(), bottomframe - 3, false));
		monitors.add(new MonitorRecord(lockA.toString(), -1, true)); /* RI won't report this one */
		monitors.add(new MonitorRecord(lockB.toString(), -1, false));
		monitors.add(new MonitorRecord(lockC.toString(), bottomframe - 0, false));
		monitors.add(new MonitorRecord(syncB.toString(), bottomframe - 2, false));
		monitors.add(new MonitorRecord(syncA.toString(), bottomframe - 2, false));
		monitors.add(new MonitorRecord(syncA.toString(), bottomframe - 3, false));
		monitors.add(new MonitorRecord(syncA.toString(), bottomframe - 4, false));
		
		/* match expected results */
		ok = matchMonitorRecords(monitors, tinfo.getLockedMonitors());
		
		if (!ok) {
			logger.debug("\n");
			logger.debug("lockA " + lockA);
			logger.debug("lockB " + lockB);
			logger.debug("lockC " + lockC);
			
			logger.debug("\n");
			printThreadInfo(tinfo);
		}
		
		printResult(ok);
		return ok;
	}
		
	boolean testJniUnscoped() {
		boolean ok = true;
		boolean ret_ok = true;
		printTestDesc();
		
		long[] tid = { Thread.currentThread().getId() };
		ThreadInfo[] tinfo;
		MonitorRecordList monitors = new MonitorRecordList();
		
		LockClass lockA = new LockClass("lockA");
		LockClass lockB = new LockClass("lockB");
		LockClass lockC = new LockClass("lockC");
		LockClass lockD = new LockClass("lockD");
		
		monitors.add(new MonitorRecord(lockA.toString(), -1, false));
		monitors.add(new MonitorRecord(lockB.toString(), -1, false));
		
		j9vm.test.monitor.Helpers.monitorEnter(lockA);
		j9vm.test.monitor.Helpers.monitorEnter(lockB);
		
		tinfo = mxb.getThreadInfo(tid, true, false);
		ok = matchMonitorRecords(monitors, tinfo[0].getLockedMonitors());
		if (!ok) {
			logger.error("Incorrect MonitorInfo after locking A & B");
			printThreadInfo(tinfo);
		}
		ret_ok = ret_ok && ok;
		
		/* unlock first monitor in record list */
		j9vm.test.monitor.Helpers.monitorExit(lockA);
		tinfo = mxb.getThreadInfo(tid, true, false);
		
		monitors.remove(new MonitorRecord(lockA.toString(), -1, false));
		ok = matchMonitorRecords(monitors, tinfo[0].getLockedMonitors());
		if (!ok) {
			logger.error("Incorrect MonitorInfo: locked A,B unlocked A");
			printThreadInfo(tinfo);
		}
		ret_ok = ret_ok && ok;
		
		j9vm.test.monitor.Helpers.monitorEnter(lockA);
		j9vm.test.monitor.Helpers.monitorEnter(lockC);
		j9vm.test.monitor.Helpers.monitorEnter(lockD);
		
		/* unlock not-first, not-last monitor in record list */
		j9vm.test.monitor.Helpers.monitorExit(lockC);
		tinfo = mxb.getThreadInfo(tid, true, false);
		
		monitors.add(new MonitorRecord(lockA.toString(), -1, false));
		monitors.add(new MonitorRecord(lockD.toString(), -1, false));
		ok = matchMonitorRecords(monitors, tinfo[0].getLockedMonitors());
		if (!ok) {
			logger.error("Incorrect MonitorInfo: locked B,A,C,D unlocked C");
		}
		ret_ok = ret_ok && ok;
		
		/* unlock last (but not first) monitor in record list */
		j9vm.test.monitor.Helpers.monitorExit(lockD);
		tinfo = mxb.getThreadInfo(tid, true, false);
		monitors.remove(new MonitorRecord(lockD.toString(), -1, false));
		ok = matchMonitorRecords(monitors, tinfo[0].getLockedMonitors());
		if (!ok) {
			logger.error("IncorrectMonitorInfo: locked B,A,D unlocked D");
		}
		ret_ok = ret_ok && ok;
		
		/* recursively lock */
		j9vm.test.monitor.Helpers.monitorEnter(lockA);
		j9vm.test.monitor.Helpers.monitorEnter(lockA);
		ok = matchMonitorRecords(monitors, tinfo[0].getLockedMonitors());
		if (!ok) {
			logger.error("Incorrect MonitorInfo: locked B,A,A,A");
		}
		
		j9vm.test.monitor.Helpers.monitorExit(lockA);
		j9vm.test.monitor.Helpers.monitorExit(lockA);
		j9vm.test.monitor.Helpers.monitorExit(lockA);
		j9vm.test.monitor.Helpers.monitorExit(lockB);
		tinfo = mxb.getThreadInfo(tid, true, false);
		monitors.clear();
		ok = matchMonitorRecords(monitors, tinfo[0].getLockedMonitors());
		if (!ok) {
			logger.error("There should be no locked monitors");
			printThreadInfo(tinfo);
		}
		ret_ok = ret_ok && ok;
		
		if (!ret_ok) {
			logger.error("\n");
			logger.error("lockA " + lockA);
			logger.error("lockB " + lockB);
			logger.error("lockC " + lockC);
			logger.error("lockD " + lockD);
		}
		
		printResult(ret_ok);
		return ret_ok;
	}
	
	boolean matchStackTrace(StackTraceElement actual, StackTraceElement expected) {
		boolean ok = true;

		if (!actual.getMethodName().equals(expected.getMethodName())) {
			ok = false;
		}
		if (!actual.getFileName().equals(expected.getFileName())) {
			ok = false;
		}
		if (actual.getLineNumber() != (expected.getLineNumber() + 1)) {
			ok = false;
		}
		return ok;
	}

	boolean matchMonitorRecords(MonitorRecordList expected, MonitorInfo[] actual) {
		boolean ok = true;
		MonitorRecordList mrl = (MonitorRecordList)expected.clone();
		
		for (int i = 0; i < actual.length; i++) {
			MonitorRecord tmp = new MonitorRecord(actual[i].toString(), actual[i].getLockedStackDepth(), false);
			if (!mrl.remove(tmp)) {
				logger.error("unexpected MonitorInfo");
				logger.error("\t" + actual[i]);
				ok = false;
			}
		}
		if (!mrl.isEmpty()) {
			boolean missing = false;
			Iterator<MonitorRecord> i = mrl.iterator();
			while (i.hasNext()) {
				MonitorRecord mr = i.next();
				if (!mr.optional) {
					missing = true;
				}

			}
			if (missing) {
				ok = false;
				logger.error("missing MonitorInfo");
				i = mrl.iterator();
				while (i.hasNext()) {
					MonitorRecord mr = i.next();
					if (!mr.optional) {
						logger.error("\t" + mr.name + ", " + mr.depth);
						ok = false;
					}
				}	
			}
		}
		return ok;
	}
	
	void printTestDesc() {
		Throwable t = new Throwable();
		t.fillInStackTrace();
		logger.debug("Test: " + t.getStackTrace()[1].getMethodName());
	}

	void printResult(boolean ok) {
		if (ok) {
			logger.debug("PASSED");
		} else {
			logger.error("FAILED");
		}
	}
	
	class LockClass {
		String name;

		LockClass(String name) {
			this.name = name;
		}

		String getLockName() {
			String name = this.getClass().getName() + '@'
					+ Integer.toHexString(System.identityHashCode(this));
			return name;
		}
	}
	
	class MonitorRecord {
		public String name;
		public int depth;
		public boolean optional;
		
		public MonitorRecord(String name, int depth, boolean optional) {
			this.name = name;
			this.depth = depth;
			this.optional = optional;
		}
		
		public boolean equals(Object obj) {
			if (obj instanceof MonitorRecord) {
				MonitorRecord mr = (MonitorRecord)obj;
				return (name.equals(mr.name) && (depth == mr.depth));
			} else {
				return false;
			}
		}
	}
	
	class MonitorRecordList extends Vector<MonitorRecord> {
		/**
		 * 
		 */
		private static final long serialVersionUID = -7435669946190340204L;
	}
}
