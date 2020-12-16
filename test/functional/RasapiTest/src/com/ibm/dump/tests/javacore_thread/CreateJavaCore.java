/*******************************************************************************
 * Copyright (c) 2016, 2021 IBM Corp. and others
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
package com.ibm.dump.tests.javacore_thread;

import java.lang.Thread.State;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.locks.LockSupport;
import java.util.concurrent.locks.ReentrantLock;

public class CreateJavaCore {

	/**
	 * @param args
	 */
	public static void main(String[] args) {
		
		boolean generateSystemCore = false;
		if( args.length > 0 && args[0].equals("-core") ) {
			// Only generate system core if that's what we're testing to save space.
			// (We might as well always generate a javacore, they aren't big and would
			// be useful for debugging problems with the core tests.)
			generateSystemCore = true;
		}
		for(CoreThreadCreator job: CoreThreadCreator.setupJobs ) {
			Thread t = new Thread(job);
			t.setName("Setup for" + job.getClass().getName());
			System.out.println("Running: " + t.getName());
			t.start();
		}
		for(CoreThreadCreator job: CoreThreadCreator.setupJobs ) {
			System.out.println("Checking: " + job + " is ready.");
			while( !job.isReady() ) {
				try {
					Thread.sleep(100);
				} catch (InterruptedException e) {
					// Likely to just keep waiting. May have to
					// add a timeout in case something goes wrong.
				}
			}
		}
		com.ibm.jvm.Dump.JavaDump();
		if( generateSystemCore ) {
			com.ibm.jvm.Dump.SystemDump();
		}
		// We've deliberately deadlocked a bunch of threads.
		// Don't wait for them before quitting.
		System.exit(0);
	}

}
