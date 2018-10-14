/*******************************************************************************
 * Copyright (c) 2016, 2018 IBM Corp. and others
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
package org.openj9.test.management;

import org.testng.AssertJUnit;
import org.testng.annotations.Test;
import org.testng.annotations.BeforeTest;
import org.testng.annotations.AfterTest;
import org.testng.log4testng.Logger;

import java.lang.management.GarbageCollectorMXBean;
import java.lang.management.ManagementFactory;
import java.lang.management.MemoryUsage;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import javax.management.Notification;
import javax.management.NotificationEmitter;
import javax.management.NotificationFilter;
import javax.management.NotificationListener;
import javax.management.openmbean.CompositeData;

import com.sun.management.GarbageCollectionNotificationInfo;
import com.sun.management.GcInfo;

@Test(groups={ "level.extended" })
public class GarbageCollectionNotificationTest {
	static Logger logger = Logger.getLogger(GarbageCollectionNotificationTest.class);

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
	
  @Test
  public void testGCNotification() {
  	int count = 0;
  	Thread allocator = new AllocationGenerator();
	allocator.start();
	System.gc();
	
	while (allocator.isAlive()) {
		try {
			count++;
			if (count > 5) {
				AllocationGenerator.bQuit = true;
			}
			Thread.sleep(2*1000);
		} catch (InterruptedException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}
  }
  
  @BeforeTest
  public void beforeTest() {
      //get all GarbageCollectorMXBeans
      List<GarbageCollectorMXBean> gcBeans = ManagementFactory.getGarbageCollectorMXBeans();
      //register every GarbageCollectorMXBean
      for (GarbageCollectorMXBean gcBean : gcBeans) {

    	  gcStates.put(gcBean.getName(), new GCState(gcBean, assumeGCIsPartiallyConcurrent(gcBean), assumeGCIsOldGen(gcBean)));
          NotificationEmitter emitter = (NotificationEmitter) gcBean;
          //new listener
          NotificationListener listener = new NotificationListener() {
              //record total gc time spend
              long totalGcTimeSpend = 0;

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
                  GCState gcState = gcStates.get(gcName);
                  GcInfo gcInfo = info.getGcInfo();
                  long gcId = gcInfo.getId();
                  gcGlance.append(info.getGcAction()).append(" (").append(gcName).append(") : - ").append(gcId);
                  gcGlance.append(" (").append(info.getGcCause()).append(") ");
                  gcGlance.append("start: ")
                          .append(dateFormat.format(new Date(JVM_START_TIME + gcInfo.getStartTime())));
                  gcGlance.append(", end: ")
                          .append(dateFormat.format(new Date(JVM_START_TIME + gcInfo.getEndTime())));

                  logger.debug(gcGlance.toString());
                  
                  AssertJUnit.assertTrue("startTime("+gcInfo.getStartTime()+") should be smaller than endTime("+gcInfo.getEndTime()+").", gcInfo.getStartTime() <= gcInfo.getEndTime());
                  if (0 != gcState.lastCollectionID) {
                	  AssertJUnit.assertTrue("lastCollectionID+1("+(gcState.lastCollectionID+1)+") should match gcId("+gcId+")", gcId == (gcState.lastCollectionID+1));
                  }
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
                              .handUsage(beforeUsage.getCommitted(), afterUsage.getCommitted(),
                                      beforeUsage.getMax()));

                      logger.debug(usage.toString());
                  }
                  totalGcTimeSpend += gcInfo.getDuration();
                  //summary
                  long percent =
                          (gcInfo.getEndTime() - totalGcTimeSpend) * 1000L / gcInfo.getEndTime();
                  StringBuilder summary = new StringBuilder();
                  summary.append("duration:").append(gcInfo.getDuration()).append("ms");
                  summary.append(", throughput:").append((percent / 10)).append(".").append(percent % 10).append('%');  //$NON-NLS-1$//$NON-NLS-2$
                  logger.debug(summary.toString());
                  logger.debug("\n");
              }
          };

          //add the listener
          emitter.addNotificationListener(listener, new NotificationFilter() {
              private static final long serialVersionUID = 3763793138186359389L;

              @Override
              public boolean isNotificationEnabled(Notification notification) {
                  //filter GC notification
                  return notification.getType()
                          .equals(GarbageCollectionNotificationInfo.GARBAGE_COLLECTION_NOTIFICATION);
              }
          }, HandBack.getInstance());
      }
  }

  @AfterTest
  public void afterTest() {
	  int totalCollectionCount = 0;
      for (Map.Entry<String, GCState> entry : gcStates.entrySet()) {
    	  StringBuilder gcGlance = new StringBuilder();
    	  String key = (String)entry.getKey(); 
    	  GCState val = (GCState)entry.getValue();
    	  gcGlance.append("GC Name : ").append(key).append(", assumeGCIsOldGen : ").append(val.assumeGCIsOldGen).append(", assumeGCIsPartiallyConcurrent : ").append(val.assumeGCIsPartiallyConcurrent).append(", TotalGCCount : ").append(val.totalCollectionCount).append(", TotalGCDuration(ms) : ").append(val.totalCollectionDuration);
    	  logger.debug(gcGlance.toString());
    	  totalCollectionCount += val.totalCollectionCount;
      }
	  AssertJUnit.assertTrue("totalCollectionCount should not be 0", 0 != totalCollectionCount);
  }

  private static class HandBack {

      private HandBack() {
      }

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
        	return false;
        default: 
        	//Assume possibly concurrent if unsure 
            return true; 
  	} 
  } 
 

     /* 
      * Assume that a GC type is an old generation collection so TransactionLogs.rescheduleFailedTasks() 
      * should be invoked. 
      * 
      * Defaults to not invoking TransactionLogs.rescheduleFailedTasks() on unrecognized GC names 
      */ 
     private static boolean assumeGCIsOldGen(GarbageCollectorMXBean gc) 
     { 
         switch (gc.getName()) 
         { 
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
            	 return true;
             default: 
                 //Assume not old gen otherwise, don't call 
                 //TransactionLogs.rescheduleFailedTasks() 
                 return false; 
         } 
     } 
}
