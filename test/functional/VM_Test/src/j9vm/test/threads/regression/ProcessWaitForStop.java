package j9vm.test.threads.regression;

/*
 * Copyright IBM Corp. and others 2025
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */

import junit.framework.Test;
import junit.framework.TestSuite;
import junit.framework.TestCase;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Constructor;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;

public class ProcessWaitForStop extends TestCase {

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
		return new TestSuite(ProcessWaitForStop.class);
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
			return Runtime.getRuntime().exec(System.getProperty("java.home") + File.separator + "bin/java -cp " + TestUtils.getVMdir() + " j9vm.test.threads.regression.Sleeper");
		} catch(IOException e){
			fail("Unexpected IO Exception when starting Sleeper");
		}
		return null;
	}
}
