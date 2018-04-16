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
package j9vm.test.printstacktest;

public class RunningThreadsStackSizeTest {

	/**
	 * @param args
	 */
	public static void main(String[] args) {
		new RunningThreadsStackSizeTest().test();
	}

	public class RunningThread extends Thread{
		int delay = 0;
		public RunningThread(int d) {
			if(d >= 0)
				delay = d;
		}
		public void run() {
			synchronized (this) {
				this.notify();
				try {
					this.wait(delay);
				} catch (InterruptedException e) {
					return;
				}
			}			
		}
	}
	
	public void test(){
		//let it be alive for 100 seconds. It will be killed by System.exit soon anyway. 
		RunningThread runningThread1 = new RunningThread(100000);
		runningThread1.setName("RunningThread1");
		synchronized (runningThread1) {
			runningThread1.start();
			try {
				runningThread1.wait();
			} catch (InterruptedException e) {
				System.err.println("The thread should not have been interrupted!");
			}
		}

		RunningThread runningThread2 = new RunningThread(100000);
		runningThread2.setName("RunningThread2");
		synchronized (runningThread2) {
			runningThread2.start();
			try {
				runningThread2.wait();
			} catch (InterruptedException e) {
				System.err.println("The thread should not have been interrupted!");
			}
		}
		System.exit(1);
	}
}
