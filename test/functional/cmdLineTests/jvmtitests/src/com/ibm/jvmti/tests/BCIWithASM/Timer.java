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
package com.ibm.jvmti.tests.BCIWithASM;

import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.Set;

public class Timer {
	
	/*Queue of timers*/
	private static ThreadLocal timerQueue_threadLocal = new ThreadLocal();
	
	/*HashMap for statistics */
	private static ThreadLocal profilinigStats_threadLocal = new ThreadLocal();
	
	/**
	 * Initialize the Timer
	 */
	public static synchronized void init() {
		profilinigStats_threadLocal.set( new HashMap<String,Long>() );
		timerQueue_threadLocal.set( new LinkedList<Long>() );
	}
	
	/**
	 * This method is invoked by every profiled method at the beginning of that method's execution.
	 * The method takes the (thread local) current snapshot and puts it in timer queue.
	 */
	public static void start() {
		long currentTimeSnapShot = System.currentTimeMillis();
		LinkedList<Long> timerQueue = (LinkedList<Long>) timerQueue_threadLocal.get();
		timerQueue.push( currentTimeSnapShot );
		timerQueue_threadLocal.set( timerQueue );
	}
	
	/**
	 * This method is invoked by every profiled method on method exit. It pops the thread local snapshot off the 
	 * top of the queue and calculates execution time of the caller profiled method. 
	 * 
	 * @param signature - signature of the caller method. 
	 */
	public static void printExecutionTime( String signature ) {
		LinkedList<Long> timerQueue = (LinkedList<Long>) timerQueue_threadLocal.get();
		long executionTime = System.currentTimeMillis() - timerQueue.pop();
		timerQueue_threadLocal.set( timerQueue );
		
		HashMap<String,Long> profilingStats = (HashMap<String,Long>)profilinigStats_threadLocal.get();
		
		if ( profilingStats.keySet().contains( Thread.currentThread().getName()+signature) == false ) {
			profilingStats.put( Thread.currentThread().getName()+signature, executionTime );
			profilinigStats_threadLocal.set( profilingStats );
		}
	}
	
	/**
	 * Prints the overall statistics - a list of all profiled methods with their execution time.
	 */
	public static void printStats() {
		HashMap<String,Long> profilingStats = (HashMap<String,Long>)profilinigStats_threadLocal.get();
		Iterator<String> keyIterator = profilingStats.keySet().iterator();
		
		System.out.println( Thread.currentThread().getName() + " - Printing method execution statistics:");
		
		while ( keyIterator.hasNext() ) {
			String signature = keyIterator.next();
			Long executionTime = profilingStats.get( signature );
			System.out.println( signature + " took " + executionTime + " to run");
		}
	}
	
	public static boolean isMethodAccountedFor( String signature ) {
		HashMap<String,Long> profilingStats = (HashMap<String,Long>)profilinigStats_threadLocal.get();
		Set<String> keySet = profilingStats.keySet();
		
		Iterator<String> keySetIterator = keySet.iterator();
		
		while ( keySetIterator.hasNext() ) {
			System.out.println(Thread.currentThread().getName() + ": " + keySetIterator.next());
		}
		
		return keySet.contains( Thread.currentThread().getName()+signature );
	}
}
