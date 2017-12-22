/*******************************************************************************
 * Copyright (c) 2001, 2012 IBM Corp. and others
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
/*
 * Created on Jun 6, 2006
 *
 * To change the template for this generated file go to
 * Window>Preferences>Java>Code Generation>Code and Comments
 */
package j9vm.test.monitor;

/**
 * 
 * @author PBurka
 *
 * Tests reservation cancellation.
 * 
 * On platforms where lock reservation is not supported this will not
 * really test anything, but should be harmless.
 */
public class CancelDeadThreadTest {

	private static CancelDeadThreadTest objects[][] = new CancelDeadThreadTest[256][1024];
	
	public static int cancelCount = 0;
	public static int normalCount = 0;
	
	public static void main(String[] args) throws InterruptedException {
		Thread threads[] = new Thread[objects.length];
		
		/* 
		 * only start 16 threads at a time to avoid OOM errors
		 * (but do more than one at a time to try to ensure that 
		 * we're not just recycling the same thread over and over
		 */
		for (int i = 0; i < threads.length; i += 16) {
			int end = Math.min(i + 16, threads.length);
			
			/* reserve monitors in many different threads. */
			for (int j = i; j < end; j++) {
				threads[j] = reserveMonitorInNewThread(j);
			}
			
			/* wait for all of the threads to die */
			for (int j = i; j < end; j++) {
				threads[j].join();
			}
		}

		/* now enter each of the monitors */
		long cancelStart = System.currentTimeMillis();
		for (int i = 0; i < objects.length; i++) {
			for (int j = 0; j < objects[i].length; j++) {
				synchronized (objects[i][j]) {
					cancelCount++;
				}
			}
		}
		long cancelEnd = System.currentTimeMillis();

		long normalStart = System.currentTimeMillis();
		/* now enter each of the monitors */ 
		for (int i = 0; i < objects.length; i++) {
			for (int j = 0; j < objects[i].length; j++) {
				synchronized (objects[i][j]) {
					normalCount++;
				}
			}
		}
		long normalEnd = System.currentTimeMillis();
		
		System.out.println("Time to cancel and enter " + cancelCount + " reserved monitors: " + (cancelEnd - cancelStart) + "ms");
		System.out.println("Time to enter " + normalCount + " reserved monitors: " + (normalEnd - normalStart) + "ms");
		
	}
	
	private static Thread reserveMonitorInNewThread(final int index) {
		Thread thr = new Thread () {
			public void run() {
				for (int j = 0; j < objects[index].length; j++) {
					CancelDeadThreadTest obj = new CancelDeadThreadTest(); 
					objects[index][j] = obj;
					Helpers.monitorReserve(obj);
				}
			}
		};
		thr.start();
		return thr;
	}

}
