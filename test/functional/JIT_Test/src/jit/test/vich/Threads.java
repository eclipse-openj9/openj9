/*******************************************************************************
 * Copyright (c) 2006, 2019 IBM Corp. and others
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

package jit.test.vich;

import org.testng.Assert;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import jit.test.vich.utils.Timer;

public class Threads {
	private static Logger logger = Logger.getLogger(Threads.class);
	Timer timer;
	int numberOfThreads;
	static final int CALLS = 50000;
	static final int MAXTHREADS = 20;

public Threads() {
	timer = new Timer ();
}
/**
 * 
 */
public synchronized void  attach() {
	// clean sync point to start running threads. 
}
public synchronized void call() {
}
/**
 * 
 */
public synchronized void  detach() {
	numberOfThreads--;
	if (numberOfThreads == 0) 
		this.notifyAll();
}

@Test(groups = { "level.sanity","component.jit" })
public void testThreads() {
	int loop; 

	// helper for threads bench.
	// each create attaches the thread to the shared object.
	// then shared object then waits for all the attached threads to call done()
	class ThreadBenchHelper implements Runnable {
		Threads syncObject;
		Threads sharedObject;
		ThreadBenchHelper(Threads sync, Threads shared) {
			sharedObject = shared;
			syncObject = sync;
		}
		public void run() {
			syncObject.attach();
			for (int i = 0; i < CALLS; i++) {
				sharedObject.call();
			}
			syncObject.detach();
		}
	}
	// test one
	// receiver monitor contended all the time
	// shared object same as receiver
	for (loop = 1; loop < MAXTHREADS; loop++) {
		Thread threads[] = new Thread[loop];
		timer.reset();
		numberOfThreads = loop;
		for (int i = 0; i < threads.length; i++) {
			threads[i] = new Thread(new ThreadBenchHelper(this, this));
		}
		synchronized (this) {
			for (int i = 0; i < threads.length; i++) {
				threads[i].start();
			}
			try {
				this.wait();
			} catch (InterruptedException e) {
				Assert.fail("Wait expired ");
			}
		}
		timer.mark();
		logger.info(loop + " Threads, " + CALLS + " receiver is shared, high contention sync calls = "+ Long.toString(timer.delta()));
	}

	// test two
	// receiver monitor contended all the time
	// shared object different from receiver so sync call not contended

	for (loop = 1; loop < MAXTHREADS; loop++) {
		Thread threads[] = new Thread[loop];
		timer.reset();
		Threads o = new Threads();
		numberOfThreads = loop;
		for (int i = 0; i < threads.length; i++) {
			threads[i] = new Thread(new ThreadBenchHelper(this, o));
		}
		synchronized (this) {
			for (int i = 0; i < threads.length; i++) {
				threads[i].start();
			}
			try {
				this.wait();
			} catch (InterruptedException e) {
				Assert.fail("Wait expired ");
			}
		}
		timer.mark();
		logger.info(loop + " Threads, " + CALLS + " high contention sync calls = "+ Long.toString(timer.delta()));
	}
	// test three
	// receiver monitor contended all the time
	// shared object different per thread so not contention anywhere
	for (loop = 1; loop < MAXTHREADS; loop++) {
		Thread threads[] = new Thread[loop];
		timer.reset();
		numberOfThreads = loop;
		for (int i = 0; i < threads.length; i++) {
			threads[i] = new Thread(new ThreadBenchHelper(this, new Threads()));
		}
		synchronized (this) {
			for (int i = 0; i < threads.length; i++) {
				threads[i].start();
			}
			try {
				this.wait();
			} catch (InterruptedException e) {
				Assert.fail("Wait expired ");
			}
		}
		timer.mark();
		logger.info(loop + " Threads, " + CALLS + " low contention sync calls = "+ Long.toString(timer.delta()));
	}
	return;
}
}
