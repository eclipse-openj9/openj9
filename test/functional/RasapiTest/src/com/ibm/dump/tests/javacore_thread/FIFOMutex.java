/*
 * Copyright IBM Corp. and others 2016
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
package com.ibm.dump.tests.javacore_thread;

import java.util.Queue;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.locks.LockSupport;

/**
 * Example FIFOMutex from the JUC docs.
 */
class FIFOMutex {
	   private final AtomicBoolean locked = new AtomicBoolean(false);
	   private final Queue<Thread> waiters
	     = new ConcurrentLinkedQueue<Thread>();
	   

	   public void lock() {
		 long i = 0;
	     boolean wasInterrupted = false;
	     Thread current = Thread.currentThread();
	     waiters.add(current);

	     // Block while not first in queue or cannot acquire lock
	     while (waiters.peek() != current ||
	            !locked.compareAndSet(false, true)) {
	        LockSupport.park(this);
	    	i++;
	        if (Thread.interrupted()) {// ignore interrupts while waiting
	          wasInterrupted = true;
	        }
	     }

	     waiters.remove();
	     if (wasInterrupted) {         // reassert interrupt status on exit
	        current.interrupt();
	     }  
	   }

	   public void unlock() {
	     locked.set(false);
	     LockSupport.unpark(waiters.peek());
	   }
	 }
