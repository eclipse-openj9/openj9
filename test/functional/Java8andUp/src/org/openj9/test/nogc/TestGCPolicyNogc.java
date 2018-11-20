/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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
package org.openj9.test.nogc;

import org.testng.Assert;

import org.testng.annotations.Test;
import org.testng.annotations.BeforeTest;
import org.testng.annotations.AfterTest;
import org.testng.log4testng.Logger;

import java.lang.management.GarbageCollectorMXBean;
import java.lang.management.ManagementFactory;
import java.lang.management.MemoryPoolMXBean;
import java.lang.management.MemoryMXBean;
import java.lang.management.MemoryNotificationInfo;
import java.lang.management.MemoryType;
import java.lang.management.MemoryUsage;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Random;

import javax.management.ListenerNotFoundException;
import javax.management.Notification;
import javax.management.NotificationEmitter;
import javax.management.NotificationFilter;
import javax.management.NotificationListener;
import javax.management.openmbean.CompositeData;

import com.sun.management.GarbageCollectionNotificationInfo;
import com.sun.management.GcInfo;

@Test(groups={ "level.extended" })
public class TestGCPolicyNogc {
	static Logger logger = Logger.getLogger(TestGCPolicyNogc.class);
	
	static final class GCState 
	{ 
		final GarbageCollectorMXBean gcBean; 
		final boolean assumeGCIsPartiallyConcurrent; 
		final boolean assumeGCIsOldGen; 
		long  totalCollectionCount = 0;
		long  totalCollectionDuration = 0; 
		long  lastCollectionID = 0;

		GCState(GarbageCollectorMXBean gcBean, boolean assumeGCIsPartiallyConcurrent, boolean assumeGCIsOldGen) 
		{ 
			this.gcBean = gcBean; 
			this.assumeGCIsPartiallyConcurrent = assumeGCIsPartiallyConcurrent; 
			this.assumeGCIsOldGen = assumeGCIsOldGen; 
		} 
	}
	
	final Map<String, GCState> gcStates = new HashMap<>();
	
    private static final long JVM_START_TIME = ManagementFactory.getRuntimeMXBean().getStartTime();

    private static DateFormat dateFormat = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS");

    private static final long ONE_KBYTE = 1024;

	private long totalGcTimeSpend = 0;
	private long used_CollectionThresholdExceeded = 0;
	private long used_ThresholdExceeded = 0;

	private NotificationEmitter gcEmitter;
	private NotificationEmitter  memoryEmitter;
	private NotificationListener gcListener;
	private NotificationListener memoryListener;
	
	@Test (priority=1)
	public final void testNogcModeEnabled() {
		logger.debug("testNogcModeEnabled start");

		List<GarbageCollectorMXBean> gcBeans = ManagementFactory.getGarbageCollectorMXBeans();
		Assert.assertTrue(1 == gcBeans.size());
		GarbageCollectorMXBean gcBean = gcBeans.get(0);
		Assert.assertEquals(gcBean.getName(), "Epsilon");
		for (MemoryPoolMXBean memPoolBean : ManagementFactory.getMemoryPoolMXBeans()) {
			if (MemoryType.HEAP == memPoolBean.getType()) {
				Assert.assertEquals(memPoolBean.getName(), "tenured");
			}
		}
		logger.debug("testNogcModeEnabled end");
	}

	@Test (priority=2)
	public void testGCPolicyNogc() {
 	  	int count = 0;
 	  	logger.debug("testGCPolicyNogc start");
 	  	Thread allocator = new Allocator();
		allocator.start();
		System.gc();
		
		while (allocator.isAlive()) {
			try {
				count++;
				if (count > 5) {
					Allocator.bQuit = true;
				}
				Thread.sleep(2*1000);
			} catch (InterruptedException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
		}
		logger.debug("testGCPolicyNogc end");
	}
	
	private byte[] allocateMemory(long bytes) {
		return new byte[(int) bytes];
	}
		
	@BeforeTest (description = "initialize gc and memory notifications")
	public void beforeTest() {
		logger.debug("beforeTest -- initialize gc and memory notifications");
		List<GarbageCollectorMXBean> gcBeans = ManagementFactory.getGarbageCollectorMXBeans();
		//register every GarbageCollectorMXBean
		for (GarbageCollectorMXBean gcBean : gcBeans) {
			gcStates.put(gcBean.getName(), new GCState(gcBean, assumeGCIsPartiallyConcurrent(gcBean), assumeGCIsOldGen(gcBean)));
			NotificationEmitter emitter = (NotificationEmitter) gcBean;
			//new listener
			NotificationListener listener = new NotificationListener() {
				//record total gc time spend

				@Override
				public void handleNotification(Notification notification, Object handback) {
					HandBack handBack = (HandBack) handback;
					
					//get gc info
					GarbageCollectionNotificationInfo info =
							GarbageCollectionNotificationInfo.from((CompositeData) notification.getUserData());
					//output
					//get a glance of gc
					StringBuilder gcGlance = new StringBuilder();
					String gcName = info.getGcName();
					Assert.assertEquals(gcName, "Epsilon");
					Assert.assertEquals(info.getGcAction(), "end of major GC");
					Assert.assertFalse(info.getGcCause().equals("Java code has requested a System.gc()"));
					GCState gcState = gcStates.get(gcName);
					GcInfo gcInfo = info.getGcInfo();
					long gcId = gcInfo.getId();
					gcGlance.append(info.getGcAction()).append(" (").append(gcName).append(") : - ").append(gcId);
					gcGlance.append(" (").append(info.getGcCause()).append(") ");
					gcGlance.append("start: ")
							.append(dateFormat.format(new Date(JVM_START_TIME + gcInfo.getStartTime())));
					gcGlance.append(", end: ")
							.append(dateFormat.format(new Date(JVM_START_TIME + gcInfo.getEndTime())));

					logger.info(gcGlance.toString());

					gcState.lastCollectionID = gcId;
					gcState.totalCollectionCount += 1;
					gcState.totalCollectionDuration += gcInfo.getDuration();

					//memory info
					Map<String, MemoryUsage> beforeUsageMap = gcInfo.getMemoryUsageBeforeGc();
					Map<String, MemoryUsage> afterUsageMap = gcInfo.getMemoryUsageAfterGc();
					for (Map.Entry<String, MemoryUsage> entry : afterUsageMap.entrySet()) {
						String name = entry.getKey();
						MemoryUsage afterUsage = entry.getValue();
						MemoryUsage beforeUsage = beforeUsageMap.get(name);

						StringBuilder usage = new StringBuilder();

						usage.append("\t[").append(String.format("%32s", name)).append("] \t");   //$NON-NLS-1$//$NON-NLS-2$ //$NON-NLS-3$
						usage.append("init:").append(afterUsage.getInit() / ONE_KBYTE).append("K;\t"); //$NON-NLS-1$ //$NON-NLS-2$
						usage.append("used:").append(handBack //$NON-NLS-1$
								.handUsage(beforeUsage.getUsed(), afterUsage.getUsed(), beforeUsage.getMax()))
								.append(";\t"); //$NON-NLS-1$
						usage.append("committed: ").append(handBack //$NON-NLS-1$
								.handUsage(beforeUsage.getCommitted(), afterUsage.getCommitted(), beforeUsage.getMax()));

						logger.info(usage.toString());
					}
					
					totalGcTimeSpend += gcInfo.getDuration();
					//summary
					long percent = (gcInfo.getEndTime() - totalGcTimeSpend) * 1000L / gcInfo.getEndTime();
					StringBuilder summary = new StringBuilder();
					summary.append("duration:").append(gcInfo.getDuration()).append("ms");
					summary.append(", throughput:").append((percent / 10)).append(".").append(percent % 10).append('%');  //$NON-NLS-1$//$NON-NLS-2$
					logger.info(summary.toString());
					logger.info("\n");

				}
			};
			//add the listener
			emitter.addNotificationListener(listener, new NotificationFilter() {
				private static final long serialVersionUID = 3763792138186359389L;

				@Override
				public boolean isNotificationEnabled(Notification notification) {
					//filter GC notification
					return notification.getType().equals(GarbageCollectionNotificationInfo.GARBAGE_COLLECTION_NOTIFICATION);
				}
			}, HandBack.getInstance());

			gcListener = listener;
			gcEmitter = emitter;
		}
		
		for (MemoryPoolMXBean pool : ManagementFactory.getMemoryPoolMXBeans()) {
			if (MemoryType.HEAP == pool.getType()) {
				long usagemax = pool.getUsage().getMax();
				long threshold = (long) (usagemax * 0.3);
				if (pool.isCollectionUsageThresholdSupported()) {
					 pool.setCollectionUsageThreshold(threshold);
				}
				if (pool.isUsageThresholdSupported()) {
					pool.setUsageThreshold(threshold);
				}
			}
		}
		
		MemoryMXBean mbean = ManagementFactory.getMemoryMXBean();
		NotificationEmitter emitter = (NotificationEmitter) mbean;
		memoryEmitter = emitter;
		emitter.addNotificationListener((memoryListener = new NotificationListener() {
			public void handleNotification(Notification n, Object hb) {
				if (n.getType().equals(MemoryNotificationInfo.MEMORY_COLLECTION_THRESHOLD_EXCEEDED)) {
					CompositeData cd = (CompositeData) n.getUserData();
					MemoryNotificationInfo info = MemoryNotificationInfo.from(cd);
					MemoryUsage mu = info.getUsage();
					logger.info("memory collection threshold exceeded !!! : poolName=" + info.getPoolName() +", Used memory="+mu.getUsed());
					Assert.assertTrue(0 == used_CollectionThresholdExceeded);
					used_CollectionThresholdExceeded = mu.getUsed();
	 			} else if (n.getType().equals(MemoryNotificationInfo.MEMORY_THRESHOLD_EXCEEDED)) {
					CompositeData cd = (CompositeData) n.getUserData();
					MemoryNotificationInfo info = MemoryNotificationInfo.from(cd);
					MemoryUsage mu = info.getUsage();
					logger.info("memory threshold exceeded !!! : poolName=" + info.getPoolName() +", Used memory="+mu.getUsed());
	 				Assert.assertTrue(0 == used_ThresholdExceeded);
	 				used_ThresholdExceeded = mu.getUsed();
	 			}
			}
		}), null, null);
	}
	
	@AfterTest (description = "remove gc and memory notifications")
	public void afterTest() {
		logger.debug("afterTest -- remove gc and memory notifications");

		try {
			gcEmitter.removeNotificationListener(gcListener);
		} catch (ListenerNotFoundException ex) {
			ex.printStackTrace();
		}
		try {
			memoryEmitter.removeNotificationListener(memoryListener);
		} catch (ListenerNotFoundException ex) {
			ex.printStackTrace();
		}
		
		Assert.assertTrue(totalGcTimeSpend >= 0);
		Assert.assertTrue(0 != used_CollectionThresholdExceeded);
		Assert.assertTrue(0 != used_ThresholdExceeded);
	}
	
	private static class HandBack {

		private HandBack() {}
		private static HandBack instance = new HandBack();

		public static HandBack getInstance() {
			return instance;
		}
		
		public String handUsage(long before, long after, long max) {
			StringBuilder usage = new StringBuilder();

			if ((max == -1) || (max == 0)) {
				usage.append("").append(before / ONE_KBYTE).append("K -> ").append(after / ONE_KBYTE).append("K)");
				return usage.toString();
			}

			long beforePercent = ((before * 1000L) / max);
			long afterPercent = ((after * 1000L) / max);

			usage.append(beforePercent / 10).append('.').append(beforePercent % 10).append("%(")
				.append(before / ONE_KBYTE).append("K) -> ").append(afterPercent / 10).append('.')
				.append(afterPercent % 10).append("%(").append(after / ONE_KBYTE).append("K)");
			return usage.toString();
		}
		
	}

	private static boolean assumeGCIsPartiallyConcurrent(GarbageCollectorMXBean gc) 
	{ 
		switch (gc.getName()) 
		{ 
		//First two are from the serial collector 
			case "Copy": 
			case "MarkSweepCompact": 
		//Parallel collector 
			case "PS MarkSweep": 
			case "PS Scavenge": 
			case "G1 Young Generation": 
		//CMS young generation collector 
			case "ParNew": 
				return false; 
			case "ConcurrentMarkSweep": 
			case "G1 Old Generation": 
				return true; 
		//IBM J9 garbage collectors
			case "scavenge":
			case "partial gc":
			case "global":
			case "global garbage collect":
			case "Epsilon" :
				return false;
			default: 
		//Assume possibly concurrent if unsure 
				return true; 
		} 
	} 

	private static boolean assumeGCIsOldGen(GarbageCollectorMXBean gc) 
	{ 
		switch (gc.getName()) { 
			case "Copy": 
			case "PS Scavenge": 
			case "G1 Young Generation": 
			case "ParNew": 
				return false; 
			case "MarkSweepCompact": 
			case "PS MarkSweep": 
			case "ConcurrentMarkSweep": 
			case "G1 Old Generation": 
				return true; 
				//IBM J9 garbage collectors
			case "scavenge":
			case "partial gc":
				return false;
			case "global":
			case "global garbage collect":
			case "Epsilon" :
				return true;
			default: 
				//Assume not old gen otherwise, don't call 
				//TransactionLogs.rescheduleFailedTasks() 
				return false; 
		} 
	} 
}