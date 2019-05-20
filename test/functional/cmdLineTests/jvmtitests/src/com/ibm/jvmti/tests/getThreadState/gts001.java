/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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

package com.ibm.jvmti.tests.getThreadState;

import java.util.concurrent.locks.LockSupport;
import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

public class gts001 {
	enum ThreadStatesEnum {
		RUNNING,
		BLOCKED,
		WAITING,
		WAITING_TIMED,
		PARKED,
		PARKED_TIMED,
		SLEEPING,
		SUSPENDED,
		INTERRUPTED_BLOCKED,
		INTERRUPTED_SUSPENDED,
		DEAD,
		NEW
	}
	
	volatile boolean done = false;
	
	native static boolean getThreadStates(int threadStateEnumCount);
	native static boolean getThreadState(Thread thread);
	
	public boolean testGetThreadStates() {
		boolean result;
		Object lock = new Object();
		Object noti = new Object();
		
		/* Initialize Thread States */
		HashMap<ThreadStatesEnum, SupportThread> threadMap = new HashMap<ThreadStatesEnum, SupportThread>();
		threadMap.put(ThreadStatesEnum.RUNNING, new RunningThread(ThreadStatesEnum.RUNNING.toString()));
		threadMap.put(ThreadStatesEnum.BLOCKED, new BlockedThread(ThreadStatesEnum.BLOCKED.toString(), lock));
		threadMap.put(ThreadStatesEnum.WAITING, new WaitingThread(ThreadStatesEnum.WAITING.toString(), noti));
		threadMap.put(ThreadStatesEnum.WAITING_TIMED, new WaitingTimedThread(ThreadStatesEnum.WAITING_TIMED.toString(), noti));
		threadMap.put(ThreadStatesEnum.PARKED, new ParkedThread(ThreadStatesEnum.PARKED.toString()));
		threadMap.put(ThreadStatesEnum.PARKED_TIMED, new ParkedTimedThread(ThreadStatesEnum.PARKED_TIMED.toString()));
		threadMap.put(ThreadStatesEnum.SLEEPING, new SleepingThread(ThreadStatesEnum.SLEEPING.toString()));
		threadMap.put(ThreadStatesEnum.SUSPENDED, new SuspendedThread(ThreadStatesEnum.SUSPENDED.toString()));
		threadMap.put(ThreadStatesEnum.INTERRUPTED_BLOCKED, new BlockedThread(ThreadStatesEnum.INTERRUPTED_BLOCKED.toString(), lock));
		threadMap.put(ThreadStatesEnum.INTERRUPTED_SUSPENDED, new SuspendedThread(ThreadStatesEnum.INTERRUPTED_SUSPENDED.toString()));
		
		synchronized (lock) {
			/* Start Threads */
			for (Iterator iter = threadMap.entrySet().iterator(); iter.hasNext(); ) {
				Map.Entry<ThreadStatesEnum, SupportThread> thread = (Map.Entry<ThreadStatesEnum, SupportThread>)iter.next();
				thread.getValue().start();
			}

			/* Try to ensure all threads started */
			for (Iterator iter = threadMap.entrySet().iterator(); iter.hasNext(); ) {
				Map.Entry<ThreadStatesEnum, SupportThread> thread = (Map.Entry<ThreadStatesEnum, SupportThread>)iter.next();
				while (!thread.getValue().hasStarted());
			}
			
			try {
				Thread.sleep(7 * 1000);
			} catch (InterruptedException e) {
				e.printStackTrace();
			}
			
			/* Interrupt the Threads testing the interrupts */
			threadMap.get(ThreadStatesEnum.INTERRUPTED_BLOCKED).interrupt();
			threadMap.get(ThreadStatesEnum.INTERRUPTED_SUSPENDED).interrupt();
			
			/* Call native function to test if thread states are set properly */
			result = getThreadStates(threadMap.size());
		}

		/* Testing is finished, terminate running threads */
		threadMap.get(ThreadStatesEnum.SLEEPING).interrupt();
		threadMap.get(ThreadStatesEnum.SUSPENDED).resume();
		threadMap.get(ThreadStatesEnum.INTERRUPTED_SUSPENDED).resume();
		synchronized (noti) {
			/* notify waiting threads */
			noti.notifyAll();
		}
		LockSupport.unpark(threadMap.get(ThreadStatesEnum.PARKED));
		LockSupport.unpark(threadMap.get(ThreadStatesEnum.PARKED_TIMED));
		
		try {
			for (Iterator iter = threadMap.entrySet().iterator(); iter.hasNext(); ) {
				Map.Entry<ThreadStatesEnum, SupportThread> thread = (Map.Entry<ThreadStatesEnum, SupportThread>)iter.next();
				thread.getValue().join();
			}
		} catch (InterruptedException e) {
			
		}
		
		return result; 
	}
	
	public boolean testNewState() {
		boolean result;
		
		Thread newThread = new NewThread(ThreadStatesEnum.NEW.toString());
	
		/* Call native function to test if thread states are set properly */
		result = getThreadState(newThread);
		
		return result; 
	}
	
	public boolean testDeadState() {
		boolean result;
		
		Thread deadThread = new DeadThread(ThreadStatesEnum.DEAD.toString());
		
		/* start thread and let it terminate */
		deadThread.start();
		while(true){
			try{
				/* wait long enough for it to be marked as started, but 
				 * due to the delay not have a vmThread yet 
				 */
				deadThread.join();
				break;
			} catch(InterruptedException e){
				System.out.println("Dead thread join interrupted");
			}
		}
	
		/* Call native function to test if thread states are set properly */
		result = getThreadState(deadThread);
		
		return result; 
	}
	
	/* this test can only be used when we put a j9thread_sleep(5000) in startJavaThread in
	 * vmThread.c to widen the window between when the thread is marked started
	 * and when the vmThread reference is populated.  Remove the RunManually prefix
	 * to add the test back to the set that you are running
	 */
	public boolean RunManuallytestDistinguishStartingFromTerminated() {
		boolean result;
		
		class ThreadStarter extends Thread {
			
			Thread theThread;
			
			public ThreadStarter(Thread theThread){
				this.theThread = theThread;
			}
			
			public void run(){

				synchronized(theThread){
					theThread.notify();
				}
				theThread.start();
			}
		}
		
		/* since we lock on thread start, when we get the state we wait for the
		 * thread to start and hence will always see it as running
		 */
		Thread runThread = new RunningThread(ThreadStatesEnum.RUNNING.toString());
		
		/* start thread and let it terminate */
		synchronized(runThread){
			ThreadStarter startThread = new ThreadStarter(runThread);
			try {
				startThread.start();
				runThread.wait();
			} catch( InterruptedException e){
				System.out.println("Wait for notification was interrupted");
			}
		}

		/* wait long enough for it to be marked as started, but 
		 * due to the delay not have a vmThread yet 
		 */
		while(true){
			try {
				Thread.sleep(1000);
				break;
			} catch (InterruptedException e) {
				System.out.println("Sleep to wait for start interrupted");
			}
		}
		System.out.println("After sleep will check state of thread now");
		System.out.flush();
	
		/* Call native function to test if thread states are set properly */
		result = getThreadState(runThread);
		
		/* sleep to leave time for printouts to occur in the right order */
		try {
			Thread.sleep(7000);
		} catch (InterruptedException e) {
		}
		
		/* Call native function to test if thread states are set properly */
		boolean result2 = getThreadState(runThread);
		
		return (result && result2); 
	}

	public String helpGetThreadStates() { return "Tests getThreadState() API"; }
	public String helpNewState() { return "Tests getThreadState() API - state for new thread"; }
	public String helpDeadState() { return "Tests getThreadState() API - state for dead thread"; }
	public String helpDistinguishStartingFromTerminated() { return "Tests getThreadState() API - ensure we can tell a thread just starting from one that is terminating, can only be run with manual changes to the vm"; }
	
	class SupportThread extends Thread {
		protected boolean started = false;
		
		SupportThread(String name) {
			super(name);
		}
		
		public boolean hasStarted() {
			return started;
		}
	}
	
	class RunningThread extends SupportThread {		
		RunningThread(String name) {
			super(name);
		}

		public void run() {
			started = true;
			while (!done) {

			}
		}
	}
	class BlockedThread extends SupportThread {
		Object lock;

		BlockedThread(String name, Object lock) {
			super(name);
			this.lock = lock;
		}

		public void run() {
			started = true;
			synchronized (lock) {
				done = true;
			}
		}
	}
	
	class WaitingThread extends SupportThread {
		Object noti;

		WaitingThread(String name, Object noti) {
			super(name);
			this.noti = noti;
		}

		public void run() {
			started = true;
			synchronized (noti) {
				try {
					noti.wait();
				} catch (InterruptedException e) {

				}
			}
		}
	}
	
	class WaitingTimedThread extends SupportThread {
		Object noti;

		WaitingTimedThread(String name, Object noti) {
			super(name);
			this.noti = noti;
		}

		public void run() {
			started = true;
			synchronized (noti) {
				try {
					noti.wait(5 * 60 * 1000);
				} catch (InterruptedException e) {

				}
			}
		}
	}
	
	class ParkedThread extends SupportThread {		
		ParkedThread(String name) {
			super(name);
		}

		public void run() {
			started = true;
			LockSupport.park();
		}
	}

	class ParkedTimedThread extends SupportThread {
		ParkedTimedThread(String name) {
			super(name);
		}

		public void run() {
			started = true;
			LockSupport.parkNanos(300 * 1000000000L);
		}
	}
	
	class SleepingThread extends SupportThread {
		SleepingThread(String name) {
			super(name);
		}

		public void run() {
			started = true;
			try {
				Thread.sleep(5 * 60 * 1000);
			} catch (InterruptedException e) {

			}
		}
	}

	class SuspendedThread extends SupportThread {
		SuspendedThread(String name) {
			super(name);
		}

		public void run() {
			started = true;
			this.suspend();
		}
	}

	class DeadThread extends SupportThread {
		DeadThread(String name) {
			super(name);
		}

		public void run() {
			started = true;
			return;
		}
		
	}
	
	class NewThread extends SupportThread {
		NewThread(String name) {
			super(name);
		}

		public void run() {
		}
		
	}
}
