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
package com.ibm.jvmti.tests.javaLockMonitoring;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class jlm001 
{
	//supported modes, these must match the c definitions
	public static final int COM_IBM_JLM_START                = 0; 
	public static final int COM_IBM_JLM_START_TIME_STAMP     = 1; 
	public static final int COM_IBM_JLM_STOP                 = 2;
	public static final int COM_IBM_JLM_STOP_TIME_STAMP      = 3;
	
	public static final int COM_IBM_JLM_START_INVALID        = 0xffff; 

        // supported formats (ibmjvmti.h)
        public static final int COM_IBM_JLM_DUMP_FORMAT_OBJECT_ID = 0;
        public static final int COM_IBM_JLM_DUMP_FORMAT_TAGS      = 1;
        public static final int COM_IBM_JLM_DUMP_INVALID_FORMAT   = 0xffff; 
         
	// expected error codes from the natives 
	public static final int JVMTI_ERROR_NONE                 = 0;
	public static final int JVMTI_ERROR_INTERNAL             = 113;
	public static final int JVMTI_ERROR_NOT_AVAILABLE        = 98;
	
	// monitor types
	public static final int JVMTI_MONITOR_JAVA               = 0x01;
	public static final int JVMTI_MONITOR_RAW                = 0x02;

        // dump function type
        public static final int JLM001_DUMP                      = 0;
        public static final int JLM001_DUMP_STATS                = 1;

        public static final long JLM001_TAG_VALUE                = 1234;
	
	// natives
	public static native int jvmtiJlmSet(int mode);
	public static native jlmresult001 jvmtiJlmDump(int dumpFunction, int format);
        public static native boolean enableMonitoringEvent();
        public static native boolean disableMonitoringEvent();
	
	// classes used to work with object monitors 
	
	// class used to get info from the JLM dumps
	public class MonitorInfo{
		public boolean valid = false;
		public byte    monitorType;
		public byte    held;
		public int     enterCount;
		public int     slowCount;
		public int     recursiveCount;
		public int     spin2Count;
		public int     yieldCount;
		public long    holdCount;
		public int     empty4;
                public long    empty8;
                public String  is32;


		public MonitorInfo(String monitorName, 	jlmresult001 dumpData){
			ByteBuffer dumpBuffer = ByteBuffer.wrap(dumpData.dumpData);
			dumpBuffer.order(ByteOrder.BIG_ENDIAN);
			valid = false;
			dumpBuffer.rewind();
                        is32 = System.getProperty("com.ibm.vm.bitmode");
                        // System.out.println(is32);

			while (dumpBuffer.hasRemaining()){
				monitorType    = dumpBuffer.get();
				held           = dumpBuffer.get();
				enterCount     = dumpBuffer.getInt();
				slowCount      = dumpBuffer.getInt();
				recursiveCount = dumpBuffer.getInt();
				spin2Count     = dumpBuffer.getInt();
				yieldCount     = dumpBuffer.getInt();
				holdCount      = dumpBuffer.getLong();
                                // The empty objectID field can be 4 or 8 bytes
				if (is32.contentEquals("32")) {
                                    empty4 = dumpBuffer.getInt();
                                } else {
                                    empty8 = dumpBuffer.getLong();
                                }
				
				StringBuffer monitorNameBuffer = new StringBuffer();
				byte nextByte = dumpBuffer.get();
				while (nextByte != 0){
					monitorNameBuffer.append((char) nextByte);
					nextByte = dumpBuffer.get();
				}
				String currentMonitorName = monitorNameBuffer.toString();
				
				if (currentMonitorName.contains(monitorName)){
					valid = true;
					break;
				}
			}
		}
	}
	

        // class used to get info from the JLM dumps, format with tags
        public class MonitorInfoTag{
                public boolean valid = false;
                public byte    monitorType;
                public byte    held;
                public int     enterCount;
                public int     slowCount;
                public int     recursiveCount;
                public int     spin2Count;
                public int     yieldCount;
                public long    holdCount;
                public long    tag;


                public MonitorInfoTag(String monitorName, 	jlmresult001 dumpData){
                        ByteBuffer dumpBuffer = ByteBuffer.wrap(dumpData.dumpData);
                        dumpBuffer.order(ByteOrder.BIG_ENDIAN);
                        valid = false;
                        dumpBuffer.rewind();

                        while (dumpBuffer.hasRemaining()){
                                monitorType    = dumpBuffer.get();
                                held           = dumpBuffer.get();
                                enterCount     = dumpBuffer.getInt();
                                slowCount      = dumpBuffer.getInt();
                                recursiveCount = dumpBuffer.getInt();
                                spin2Count     = dumpBuffer.getInt();
                                yieldCount     = dumpBuffer.getInt();
                                holdCount      = dumpBuffer.getLong();
                                tag            = dumpBuffer.getLong();

                                StringBuffer monitorNameBuffer = new StringBuffer();
                                byte nextByte = dumpBuffer.get();
                                while (nextByte != 0){
                                        monitorNameBuffer.append((char) nextByte);
                                        nextByte = dumpBuffer.get();
                                }
                                String currentMonitorName = monitorNameBuffer.toString();

                                if (currentMonitorName.contains(monitorName)){
                                        valid = true;
                                        break;
                                }
                        }
                }
        }


	// this class is used to start a thread which inflates a monitor for us
	class Inflator extends Thread{
		Object syncObject;
		Inflator(Object syncObject){
			this.syncObject = syncObject;
		};
		
		public void run(){
			try {
				while(true){
					synchronized(syncObject){
						synchronized(this){
							this.notify();
						}
						syncObject.wait();
					}
				}
			} catch(InterruptedException e) {
			}
		}
		
		public void inflate(){
			synchronized(this) {
				start();
				try {
					wait();
				} catch (InterruptedException e){
				}
				
				/* this should act as a barrier */
				synchronized(syncObject){
				}
			}
		}
	}
	
	// this class is used to hold a monitor so other threads have to wait
	class MonitorHolder extends Thread{
		Object syncObject;
		int time;
		boolean started = false;
		boolean done = false;
		MonitorHolder(Object syncObject, int time){
			this.syncObject = syncObject;
			this.time = time;
		};
		
		public void run(){
			try {
				while(true){
					synchronized(syncObject){
						synchronized(this){
							done = false;
							this.notify();
						}
						Thread.sleep(time);
					}
				
					synchronized(this){
						done = true;
						notify();
						wait();
					}
				}
			} catch(InterruptedException e) {
			}
		}
		
		public void startHold(){
			synchronized(this) {
				if (!started){
					start();
					started = true;
				}
				try {
					done = false;
					notify();
					wait();
				} catch (InterruptedException e){
				}
			}
		}
		
		public void doneHold(){
			synchronized(this) {
				if (done != true){
					try{
						wait();
					} catch (InterruptedException e){
					}
				}
			}
		}
	}
	
	// this class is used create threads that will wait for a monitor
	class MonitorWanter extends Thread{
		Object syncObject;
		boolean started = false;
		boolean done = false;
		MonitorWanter(Object syncObject){
			this.syncObject = syncObject;
		};
		
		public void run(){
			try {
				while(true){
					synchronized(this){
						this.notify();
					}
					synchronized(syncObject){
						
					}
		
					synchronized(this){
						done = true;
						notify();
						wait();
					}
				}
			} catch(InterruptedException e) {
			}
		}
		
		public void startWant(){
			synchronized(this) {
				if (!started){
					start();
					started = true;
				}
				try {
					done = false;
					notify();
					wait();
				} catch (InterruptedException e){
				}
			}
		}
		
		public void doneWant(){
			synchronized(this) {
				if (done != true){
					try{
						wait();
					} catch (InterruptedException e){
					}
				}
			}
		}
	}
	
	// this class is used to enter a monitor.  Each time the notifyObject is notified it will
	// enter the syncObject and then notify the notifyObject
	class EnterMonitor extends Thread{
		boolean started = false;
		Object syncObject;
		EnterMonitor(Object syncObject){
			this.syncObject = syncObject;
		};
		
		public void run(){
			try {
				synchronized(this) {
					while(true){
						synchronized(syncObject){
						}
						notify();
						wait();
					}
				}
			} catch(InterruptedException e) {
			}
		}
		
		// this method is called to have the secondary thread enter the syncObject again at which 
		// point it will notify the itself to signal that this has taken place
		public void enterMonitor(){
			synchronized(this){
				try {
					if (!started){
						start();
						started = true;
					}
					notify();
					wait();
				} catch (InterruptedException e){
				}
			}
		}
	}
	
	public boolean setup(String args)
	{
		return true;
	}
	
	public boolean testEnableJLM_START_INVALID() 
	{
		int result = jvmtiJlmSet(COM_IBM_JLM_START_INVALID);
		if (result != JVMTI_ERROR_INTERNAL) {
			/* we should have gotten an error here */
			System.out.println("Failed doing COM_IBM_JLM_START");
			return false;
		}
		return true;
	}
	
	public String helpEnableJLM_START_INVALID()
	{
		return "Tests the Enabling JLM with an invalid option";
	}
		
	public boolean testEnableJLM_START_STOP() 
	{
		int result = jvmtiJlmSet(COM_IBM_JLM_START);
		if (result != JVMTI_ERROR_NONE) {
			/* we failed to load ok so return that the test failed */
			System.out.println("Failed doing COM_IBM_JLM_START");
			return false;
		}
		
		result = jvmtiJlmSet(COM_IBM_JLM_STOP);
		if (result != JVMTI_ERROR_NONE) {
			/* we failed to load ok so return that the test failed */
			System.out.println("Failed doing COM_IBM_JLM_STOP");
			return false;
		}
		
		return true;
	}

	public String helpEnableJLM_START_STOP()
	{
		return "Tests enabling/disabling JLM with calls COM_IBM_JLM_START/STOP ";
	}
	
	
	public boolean testEnableJLM_START_STOP_TIME_STAMP() 
	{
		int result = jvmtiJlmSet(COM_IBM_JLM_START_TIME_STAMP);
		if (result != JVMTI_ERROR_NONE) {
			/* we failed to load ok so return that the test failed */
			System.out.println("Failed doing COM_IBM_JLM_START_TIME_STAMP");
			return false;
		}
		
		result = jvmtiJlmSet(COM_IBM_JLM_STOP_TIME_STAMP);
		if (result != JVMTI_ERROR_NONE) {
			/* we failed to load ok so return that the test failed */
			System.out.println("Failed doing COM_IBM_JLM_STOP_TIME_STAMP");
			return false;
		}		
		
		return true;
	}

	public String helpEnableJLM_START_STOP_TIME_STAMP()
	{
		return "Tests enabling/disabling JLM with call COM_IBM_JLM_STOP/START_TIME_STAMP ";
	}
	
	public boolean testGetJLMDUMP_TIME_STAMP() 
	{
		boolean returnValue = false;
		
		// start JLM
		int result = jvmtiJlmSet(COM_IBM_JLM_START_TIME_STAMP);
		if (result != JVMTI_ERROR_NONE) {
			// we failed to start JLM so fail the test
			System.out.println("Failed doing COM_IBM_JLM_START_TIME_STAMP");
			return false;
		}
		
		// get a dump 
		jlmresult001 dumpData = jvmtiJlmDump(JLM001_DUMP, 0);
		if (dumpData.result != JVMTI_ERROR_NONE){
			System.out.println("Call to jvmtiJlmDump failed");
			return false;
		}
		
		// validate that the dump includes the VM exclusive access monitor
		MonitorInfo monInfo = new MonitorInfo("VM exclusive access", dumpData);
		if(monInfo.valid){
			// validate that we go the right info 
			if (monInfo.monitorType != JVMTI_MONITOR_RAW){
				System.out.println("Wrong monitor type for VM exclusive access monitor");
				return false;
			}
			return true;
		} else {
			System.out.println("Did not find VM exclusive access monitor");
			returnValue = false;
		}
				
		// stop JLM
		result = jvmtiJlmSet(COM_IBM_JLM_STOP_TIME_STAMP);
		if (result != JVMTI_ERROR_NONE) {
			// we failed to load ok so return that the test failed 
			System.out.println("Failed doing COM_IBM_JLM_STOP");
			return false;
		}
		
		return returnValue;
	}

	public String helpGetJLMDUMP_TIME_STAMP()
	{
		return "Tests the getting a JLM dump when timestamps are enabled";
	}
	
	public boolean testCheckEnterCount() 
	{
		class ObjectWithMonitorOfInterestX1 extends Object{
		}

		// start up JLM
		boolean returnValue = false;
		int result = jvmtiJlmSet(COM_IBM_JLM_START_TIME_STAMP);
		if (result != JVMTI_ERROR_NONE) {
			// we failed to start JLM so fail the test
			System.out.println("Failed doing COM_IBM_JLM_START_TIME_STAMP");
			return false;
		}
		
		// create the object monitor that we will use for the test 
		Object syncObject = new ObjectWithMonitorOfInterestX1();
		
		// inflate the monitor so that we get sane numbers.  This causes the get count to be 
		// set to 2
		Inflator inflator = new Inflator(syncObject);
		inflator.inflate();
		
		// create the thread that will enter the monitor and ask it to enter for the
		// first time
		EnterMonitor enter = new EnterMonitor(syncObject);
		enter.enterMonitor();
		
		// now get a dump we expect that the enter count should be 3, 2 for the inflator and
		// one for the EnterMonitor
		jlmresult001 dumpData = jvmtiJlmDump(JLM001_DUMP, 0);
		if (dumpData.result != JVMTI_ERROR_NONE){
			System.out.println("Call to jvmtiJlmDump failed");
			return false;
		}

		MonitorInfo monInfo = new MonitorInfo("ObjectWithMonitorOfInterestX1", dumpData);
		if (monInfo.valid){
			//
			if (monInfo.monitorType != JVMTI_MONITOR_JAVA){
				System.out.println("Wrong monitor type for ObjectWithMonitorOfInterest");
				return false;
			}
			
			// now validate that the enter count is 3 
			if (monInfo.enterCount != 3){
				System.out.println("Enter count not as expected 3, was:" + monInfo.enterCount);
				return false;
			}
			returnValue = true;
		} else {
			System.out.println("Did not find ObjectWithMonitorOfInterest");
			returnValue = false;
		}
		
		// now bump the enter count and get a second dump 
		enter.enterMonitor();
		dumpData = jvmtiJlmDump(JLM001_DUMP, 0);
		if (dumpData.result != JVMTI_ERROR_NONE){
			System.out.println("Call to jvmtiJlmDump failed");
			return false;
		}
		
		// now validate the dump contained the info we expected
		monInfo = new MonitorInfo("ObjectWithMonitorOfInterestX1",dumpData);
		if (monInfo.valid){
			// validate the monitor was of the expected type 
			if (monInfo.monitorType != JVMTI_MONITOR_JAVA){
				System.out.println("Wrong monitor type for ObjectWithMonitorOfInterest");
				return false;
			}

			// now validate that the enter count is 4 
			if ( monInfo.enterCount != 4){
				System.out.println("Enter count not as expected 4, was:" + monInfo.enterCount);
				return false;
			}
		} else {
			System.out.println("Did not find ObjectWithMonitorOfInterest");
			returnValue = false;
		}
		
		// shut down JLM
		result = jvmtiJlmSet(COM_IBM_JLM_STOP_TIME_STAMP);
		if (result != JVMTI_ERROR_NONE) {
			/* we failed to load ok so return that the test failed */
			System.out.println("Failed doing COM_IBM_JLM_STOP");
			return false;
		}
		
		return returnValue;
	}
	
	public String helpCheckEnterCount()
	{
		return "Tests the recusive count for a jlm dump";
	}
	
	public boolean testNonRecursiveCount()  
	{
		class ObjectWithMonitorOfInterestY2 extends Object{
		}

		// start up JLM
		boolean returnValue = false;
		int result = jvmtiJlmSet(COM_IBM_JLM_START_TIME_STAMP);
		if (result != JVMTI_ERROR_NONE) {
			// we failed to start JLM so fail the test
			System.out.println("Failed doing COM_IBM_JLM_START_TIME_STAMP");
			return false;
		}
		
		// create the object monitor that we will use for the test 
		Object syncObject = new ObjectWithMonitorOfInterestY2();
		
		// inflate the monitor so that we get sane numbers.  This causes the get count to be 
		// set to 2
		Inflator inflator = new Inflator(syncObject);
		inflator.inflate();
		
		// create the thread that will enter the monitor and ask it to enter for the
		// first time
		EnterMonitor enter = new EnterMonitor(syncObject);
		enter.enterMonitor();
		
		// now get a dump we expect that the enter count should be 3, 2 for the inflator and
		// one for the EnterMonitor
		jlmresult001 dumpData = jvmtiJlmDump(JLM001_DUMP, 0);
		if (dumpData.result != JVMTI_ERROR_NONE){
			System.out.println("Call to jvmtiJlmDump failed");
			return false;
		}

		MonitorInfo monInfo = new MonitorInfo("ObjectWithMonitorOfInterestY2", dumpData);
		if (monInfo.valid){
			//
			if (monInfo.monitorType != JVMTI_MONITOR_JAVA){
				System.out.println("Wrong monitor type for ObjectWithMonitorOfInterest");
				return false;
			}
			
			// now validate that the enter count is 3 
			if (monInfo.enterCount != 3){
				System.out.println("Enter count not as expected 3, was:" + monInfo.enterCount);
				return false;
			}
			returnValue = true;
		} else {
			System.out.println("Did not find ObjectWithMonitorOfInterest");
			returnValue = false;
		}
		
		// now enter the monitor recursively and ask the Enter thread to bump the enter count
		// we then expect the enter count to go up by 5 but the non recursive count to only go up by 1
		synchronized(syncObject){
			synchronized(syncObject){
				synchronized(syncObject){
					synchronized(syncObject){
						synchronized(syncObject){
						}
					}
				}
			}
		}
		
		dumpData = jvmtiJlmDump(JLM001_DUMP, 0);
		if (dumpData.result != JVMTI_ERROR_NONE){
			System.out.println("Call to jvmtiJlmDump failed");
			return false;
		}
		
		// now validate the dump contained the info we expected
		monInfo = new MonitorInfo("ObjectWithMonitorOfInterestY2",dumpData);
		if (monInfo.valid){
			// validate the monitor was of the expected type 
			if (monInfo.monitorType != JVMTI_MONITOR_JAVA){
				System.out.println("Wrong monitor type for ObjectWithMonitorOfInterestY2");
				return false;
			}

			// now validate that the enter count is 8
			if ( monInfo.enterCount != 8){
				System.out.println("Enter count not as expected 8, was:" + monInfo.enterCount);
				return false;
			}
			
			// now validate that the recursive count 4
			if ( monInfo.recursiveCount != 4){
				System.out.println("Non recursive count not as expected 4, was:" + monInfo.recursiveCount);
				return false;
			}
		} else {
			System.out.println("Did not find ObjectWithMonitorOfInterest");
			returnValue = false;
		}
		
		// shut down JLM
		result = jvmtiJlmSet(COM_IBM_JLM_STOP_TIME_STAMP);
		if (result != JVMTI_ERROR_NONE) {
			// we failed to load ok so return that the test failed 
			System.out.println("Failed doing COM_IBM_JLM_STOP");
			return false;
		}
		
		return returnValue;
	}

	public String helpNonRecursiveCount()
	{
		return "Tests the recusive count for a jlm dump";
	}


	public boolean testSlowCountAndDumpStats() 
	{
		class ObjectWithMonitorOfInterestZ3 extends Object{
		}

		// start up JLM
		boolean returnValue = false;
		int result = jvmtiJlmSet(COM_IBM_JLM_START_TIME_STAMP);
		if (result != JVMTI_ERROR_NONE) {
			// we failed to start JLM so fail the test
			System.out.println("Failed doing COM_IBM_JLM_START_TIME_STAMP");
			return false;
		}

                // Enable Monitor Contended Entered event
                if (enableMonitoringEvent() == false) {
                    System.out.println("Failed enabling Monitor Contended Enter event");
                    return false;
                }
		
		// create the object monitor that we will use for the test 
		Object syncObject = new ObjectWithMonitorOfInterestZ3();
		
		// inflate the monitor so that we get sane numbers
		Inflator inflator = new Inflator(syncObject);
		inflator.inflate();
		
		// create the thread that will hold the monitor and make other threads
		// wait
		MonitorHolder holder = new MonitorHolder(syncObject, 5000);
		
		// create the threads that will wait on the monitor
		MonitorWanter wanter1 = new MonitorWanter(syncObject);
		MonitorWanter wanter2 = new MonitorWanter(syncObject);
		MonitorWanter wanter3 = new MonitorWanter(syncObject);

		
		// start a hold and then start the wanters
		holder.startHold();
		wanter1.startWant();
		wanter2.startWant();
		
		// now wait for it all to be over
		holder.doneHold();
		wanter1.doneWant();
		wanter2.doneWant();

        // Disable Monitor Contended Entered event
        if (disableMonitoringEvent() == false) {
            System.out.println("Failed disabling Monitor Contended Enter event");
            return false;
        }

		jlmresult001 dumpData = jvmtiJlmDump(JLM001_DUMP, 0);
		if (dumpData.result != JVMTI_ERROR_NONE){
			System.out.println("Call to jvmtiJlmDump failed");
			return false;
		}

		MonitorInfo monInfo = new MonitorInfo("ObjectWithMonitorOfInterestZ3",dumpData);
		if (monInfo.valid){
			//
			if (monInfo.monitorType != JVMTI_MONITOR_JAVA){
				System.out.println("Wrong monitor type for ObjectWithMonitorOfInterest");
				return false;
			}
			
			// now validate that the slow count is at least 2
			if (monInfo.slowCount < 2){
				System.out.println("Slow count not as expected at least 2, was:" + monInfo.slowCount);
				return false;
			}
			returnValue = true;
		} else {
			System.out.println("Did not find ObjectWithMonitorOfInterest");
			returnValue = false;
		}

                // now get the dump using the JlmDumpStats API, COM_IBM_JLM_DUMP_FORMAT_OBJECT_ID
                
                dumpData = jvmtiJlmDump(JLM001_DUMP_STATS, COM_IBM_JLM_DUMP_FORMAT_OBJECT_ID);
		if (dumpData.result != JVMTI_ERROR_NONE){
			System.out.println("Call to jvmtiJlmDumpStats failed");
			return false;
		}


		monInfo = new MonitorInfo("ObjectWithMonitorOfInterestZ3",dumpData);
		if (monInfo.valid){
			//
			if (monInfo.monitorType != JVMTI_MONITOR_JAVA){
				System.out.println("JlmDumpStats, FORMAT_OBJECT_ID: Wrong monitor type for ObjectWithMonitorOfInterest");
				return false;
			}
			
			// now validate that the slow count is at least 2 
			if (monInfo.slowCount < 2){
				System.out.println("JlmDumpStats, FORMAT_OBJECT_ID: Slow count not as expected at least 2, was:" + monInfo.slowCount);
				return false;
			}
			returnValue = true;
		} else {
			System.out.println("JlmDumpStats, FORMAT_OBJECT_ID: Did not find ObjectWithMonitorOfInterest");
			returnValue = false;
		}


                // now get the dump using the JlmDumpStats API, COM_IBM_JLM_DUMP_FORMAT_TAGS
                dumpData = jvmtiJlmDump(JLM001_DUMP_STATS, COM_IBM_JLM_DUMP_FORMAT_TAGS);
                if (dumpData.result != JVMTI_ERROR_NONE){
                        System.out.println("Call to jvmtiJlmDumpStats failed");
                        return false;
                }

                MonitorInfoTag monInfoTag = new MonitorInfoTag("ObjectWithMonitorOfInterestZ3", dumpData);
                if (monInfoTag.valid){
                        //
                        if (monInfoTag.monitorType != JVMTI_MONITOR_JAVA){
                                System.out.println("JlmDumpStats, FORMAT_TAGS: Wrong monitor type for ObjectWithMonitorOfInterest");
                                return false;
                        }

                        // now validate that the slow count is at least 2 
                        if (monInfoTag.slowCount < 2){
                                System.out.println("JlmDumpStats, FORMAT_TAGS: Slow count not as expected at least 2, was:" + monInfoTag.slowCount);
                                return false;
                        }

                        if (monInfoTag.tag != JLM001_TAG_VALUE) {
                                System.out.println("JlmDumpStats, FORMAT_TAGS: Tag not as expected, was:" + monInfoTag.tag);
                                return false;
                        }

                        returnValue = true;
                } else {
                        System.out.println("JlmDumpStats, FORMAT_TAGS: Did not find ObjectWithMonitorOfInterest");
                        returnValue = false;
                }


		// now enter the monitor recursively and ask the Enter thread to bump the enter count
		// we then expect the enter count to go up by 6 but the non recursive count to only go up by 1
		
		
		// start a hold and then start the wanters
		holder.startHold();
		wanter1.startWant();
		wanter2.startWant();
		wanter3.startWant();
		
		// now wait for it all to be over
		holder.doneHold();
		wanter1.doneWant();
		wanter2.doneWant();
		wanter3.doneWant();
		

		dumpData = jvmtiJlmDump(JLM001_DUMP, 0);
		if (dumpData.result != JVMTI_ERROR_NONE){
			System.out.println("Call to jvmtiJlmDump failed");
			return false;
		}
		
		// now validate the dump contained the info we expected
		monInfo = new MonitorInfo("ObjectWithMonitorOfInterestZ3", dumpData);
		if (monInfo.valid){
			// validate the monitor was of the expected type 
			if (monInfo.monitorType != JVMTI_MONITOR_JAVA){
				System.out.println("Wrong monitor type for ObjectWithMonitorOfInterestZ3");
				return false;
			}

			// now validate that the slow count is at least 5
			if ( monInfo.slowCount < 5){
				System.out.println("Slow count not as expected at least 5, was:" + monInfo.slowCount);
				return false;
			}
	} else {
			System.out.println("Did not find ObjectWithMonitorOfInterest");
			returnValue = false;
		}
		
		// shut down JLM
		result = jvmtiJlmSet(COM_IBM_JLM_STOP_TIME_STAMP);
		if (result != JVMTI_ERROR_NONE) {
			// we failed to load ok so return that the test failed 
			System.out.println("Failed doing COM_IBM_JLM_STOP");
			return false;
		}
		
		return returnValue;
	}

	public String helpSlowCountAndDumpStats()
	{
		return "Tests the slow count for a jlm dump, jlm dump stats";
	}
	
	public boolean testJLMDumpStats_DUMP_FORMAT_INVALID() 
	{
		boolean returnValue = false;
		
		// start JLM
		int result = jvmtiJlmSet(COM_IBM_JLM_START_TIME_STAMP);
		if (result != JVMTI_ERROR_NONE) {
			// we failed to start JLM so fail the test
			System.out.println("Failed doing COM_IBM_JLM_START_TIME_STAMP");
			return false;
		}
		
		// try to get a dump 
		jlmresult001 dumpData = jvmtiJlmDump(JLM001_DUMP_STATS, COM_IBM_JLM_DUMP_INVALID_FORMAT);
		if (dumpData.result == JVMTI_ERROR_NONE){
			System.out.println("Failed to get a JlmDumpStats error with an invalid format argument.");
			return false;
		}

                // stop JLM
                result = jvmtiJlmSet(COM_IBM_JLM_STOP_TIME_STAMP);
                if (result != JVMTI_ERROR_NONE) {
                        // we failed to stop JLM so fail the test
                        System.out.println("Failed doing COM_IBM_JLM_STOP");
                        return false;
                }

		return true;
	}
	
	public String helpJLMDumpStats_DUMP_FORMAT_INVALID()
	{
		return "Tests getting a JLM dump with JlmDumpStats() with an invalid format option";
	}	
}

