package j9vm.test.threads.regression;

/*******************************************************************************
 * Copyright (c) 2008, 2019 IBM Corp. and others
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

import junit.framework.Test;
import junit.framework.TestSuite;
import junit.framework.TestCase;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Constructor;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;

public class ProcessWaitFor extends TestCase {
	
	volatile int count;
	
	class TimerThread extends Thread {
		Runnable run;
		long timeoutMillis;
		
		public TimerThread(Runnable run, long timeoutMillis){
			this.timeoutMillis = timeoutMillis;
			this.run = run;
		}
		
		public void run(){
			try {
				Thread.sleep(timeoutMillis);
			} catch (InterruptedException e){
				fail("Unexpected exception in sleep:" + e);
			}
			run.run();
			try {
				Thread.sleep(timeoutMillis);
			} catch (InterruptedException e){
				fail("Unexpected exception in sleep:" + e);
			}
		}
	}
	
	class Interrupter implements Runnable {
		Thread threadToInterrupt;
		Object synchronizer;
		
		public Interrupter(Thread threadToInterrupt,Object synchronizer){
			this.threadToInterrupt = threadToInterrupt;
			this.synchronizer = synchronizer;
		}


		public void run(){
			if (synchronizer != null){
				synchronized(synchronizer){
					threadToInterrupt.interrupt();
					return;
				}
			}
			threadToInterrupt.interrupt();
		}
	}	
	
	class Stopper implements Runnable {
		Thread threadToStop;
		Object synchronizer;
		
		public Stopper(Thread threadToStop,Object synchronizer){
			this.threadToStop = threadToStop;
			this.synchronizer = synchronizer;
		}


		public void run(){
			if (synchronizer != null){
				synchronized(synchronizer){
					threadToStop.stop();
					return;
				}
			}
			threadToStop.stop();
		}
	}
	
	/**
	 * method that can be used to run the test by itself
	 * 
	 * @param args no valid arguments
	 */
	public static void main (String[] args) {
		junit.textui.TestRunner.run(suite());
	}
	
	public static Test suite(){
		return new TestSuite(ProcessWaitFor.class);
	}
	
	public void testWaitForAfterInterruptInSleep(){
		Thread interruptor = new TimerThread(new Interrupter(Thread.currentThread(),null),1000);
		interruptor.start();
		
		try{
			Thread.sleep(20000);
			/* in this case the test is invalid because of the timing just assume pass */
			System.out.println("WaitForAfterInterruptInSleep - invalid, assuming pass");
			return;
		} catch (InterruptedException e){
			/* this is expected */
		}
		
		try {
			interruptor.join();
		} catch (InterruptedException e){
			fail("Main thread interrupted during join");
		}
		
		doWaitFor();
	}
		
	public void testWaitForAfterInterruptInWaitWithTimeout(){
		
		Thread interruptor = new TimerThread(new Interrupter(Thread.currentThread(),null),1000);
		interruptor.start();
		
		try{
			Object waitObject = new Object();
			synchronized(waitObject){
				waitObject.wait(20000);
			}
			/* in this case the test is invalid because of the timing just assume pass */
			System.out.println("WaitForAfterInterruptInWait - invalid, assuming pass");
			return;
		} catch (InterruptedException e){
			/* this is expected */
		}
		
		try {
			interruptor.join();
		} catch (InterruptedException e){
			fail("Main thread interrupted during join");
		}
		
		doWaitFor();
	}
	
	public void testWaitForAfterInterruptInWait(){
		
		Object waitObject = new Object();
		Thread interruptor = new TimerThread(new Interrupter(Thread.currentThread(),waitObject),1000);
		try{
			synchronized(waitObject){
				interruptor.start();
				waitObject.wait();
			}
			fail("Not interrupted in wait, should not be able to happen");
			return;
		} catch (InterruptedException e){
			/* this is expected */
		}
		
		try {
			interruptor.join();
		} catch (InterruptedException e){
			fail("Main thread interrupted during join");
		}
		
		doWaitFor();
	}
	
	public void testWaitForAfterInterrupted(){

		Thread interruptor = new TimerThread(new Interrupter(Thread.currentThread(),null),1000);
		interruptor.start();
		
		Thread me = Thread.currentThread();
		count = 0;
		while(!me.isInterrupted()){
			count++;
		}
		
		/* at this point we are interrupted so we will clear to make sure that waitFor is not interrupted */
		assertTrue("Main thread not interrupted as expected",Thread.interrupted()== true);
		
		doWaitFor();
		
		try {
			interruptor.join();
		} catch (InterruptedException e){
			fail("Main thread interrupted during join");
		}
	}
	
	public void testWaitForAfterJoin(){
		
		Thread interruptor = new TimerThread(new Interrupter(Thread.currentThread(),null),1000);
		interruptor.start();
		
		Thread me = Thread.currentThread();
		count = 0;
		while(!me.isInterrupted()){
			count++;
		}
		
		/* at this point we are interrupted so make sure that when we do the join we get an 
		 * interrupted exception and the exception is cleared */
		try {
			interruptor.join();
			/* is is possible that the interrupter thread will finish before we do the join in this case 
			 * the test is invalid
			 */
			System.out.println("WaitForAfterJoin - invalid, assuming pass");
			return;
		} catch (InterruptedException e){
			/* this is expected */
		}
		
		doWaitFor();
	}
	
	
	public void testWaitForAfterJoinWithTimeout(){
		
		Thread interruptor = new TimerThread(new Interrupter(Thread.currentThread(),null),1000);
		interruptor.start();
		
		Thread me = Thread.currentThread();
		count = 0;
		while(!me.isInterrupted()){
			count++;
		}
		
		/* at this point we are interrupted so make sure that when we do the join we get an 
		 * interrupted exception and the exception is cleared */
		try {
			interruptor.join(5000);
			/* is is possible that the interrupter thread will finish before we do the join in this case 
			 * the test is invalid
			 */
			System.out.println("WaitForAfterJoinWithTimeout - invalid, assuming pass");
			return;
		} catch (InterruptedException e){
			/* this is expected */
		}
		
		doWaitFor();
	}
	
	public void testWaitForAfterInterruptInSleepByStop(){
		Thread interruptor = new TimerThread(new Stopper(Thread.currentThread(),null),1000);
		interruptor.start();
		
		try{
			Thread.sleep(5000);
			/* in this case the test is invalid because of the timing just assume pass */
			System.out.println("WaitForAfterInterruptInSleep - invalid, assuming pass");
			return;
		} catch (InterruptedException e){
			System.out.println("WaitForAfterInterruptInSleep - invalid received InterruptedException, assuming pass");
		} catch (ThreadDeath e){
			/* this is expected */
		}
		
		try {
			interruptor.join();
		} catch (InterruptedException e){
			fail("Main thread interrupted during join");
		}
		
		doWaitFor();
	}
	
//	
//  Due to timing this test is not reliable to be in the builds but is useful to validate that 	
//	we did not break the ability to break out of waitFor.
//
//	public void testInterruptedInWaitfor(){
//		Thread interruptor = new TimerThread(new Interrupter(Thread.currentThread(),null),1000);
//		interruptor.start();
//		
//		try { 
//			Process newProcess = null;
//			try {     
//				newProcess = startSleeper();
//			} catch (Exception e){
//				fail("Unexpected exception while doing exec:" + e);
//			}  
//			
//			try {     
//				int result = newProcess.waitFor();
//				fail("Did not get interrupted in Waitfor:" + result);
//			} catch (InterruptedException e){
//				/* this is expected */
//			}
//			
//			newProcess.destroy();
//		} catch (Exception e){
//			fail("Unexpected exception:" + e);
//		}
//		
//		try {
//			interruptor.join();
//		} catch (InterruptedException e){
//			fail("Main thread interrupted during join");
//		}
//	}
	
	public void doWaitFor(){
		try { 
			Process newProcess = null;
			try {     
				newProcess = startSleeper();
			} catch (Exception e){
				fail("Unexpected exception while doing exec:" + e);
			}  
			
			try {     
				newProcess.waitFor();
			} catch (InterruptedException e){
				fail("Interrupted exception in waitFor");
			}
			
			newProcess.destroy();
		} catch (Exception e){
			fail("Unexpected exception:" + e);
		}
	}
	
	public Process startSleeper() {
		try {
			return Runtime.getRuntime().exec(System.getProperty("java.home") + File.separator + "bin/java -cp " + RegressionTests.getVMdir() + " j9vm.test.threads.regression.Sleeper");
		} catch(IOException e){
			fail("Unexpected IO Exception when starting Sleeper");
		}
		return null;
	}
}
