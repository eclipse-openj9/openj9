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
package com.ibm.dump.tests.javacore_lock_context;

import java.util.HashMap;
import java.util.Map;

/**
 * Lock context checks:
 * 
 * Enter a lock. Check we enter expected lock in expected method.
 * 
 * Enter a lock twice. Check lock count incremented twice.
 * 
 * Enter a lock n times going down a stack. Check highest entry count matches
 * entry count in monitor section.
 * 
 * Enter n different locks in 1 frame. Check correct number of entries take
 * place.
 * 
 * Enter 3 different locks, twice, in mixed up order. Check only 1 record per
 * lock. e.g sync(a), sync(b), sync(c), sync(a), sync(b), sync(c)
 * 
 * @author hhellyer
 * 
 */
public class CreateJavaCore {

	private static StackCreator[] stackCreator = { new SimpleEnter(), new EnterTwice(), new EnterInStack(), new EnterInSequence() };
	
	protected static Map<String, StackCreator> threadsToEntries = new HashMap<String, StackCreator>();
	
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
		
		setupThreads();
		
		com.ibm.jvm.Dump.JavaDump();
		if( generateSystemCore ) {
			com.ibm.jvm.Dump.SystemDump();
		}
		// We've deliberately deadlocked a bunch of threads.
		// Don't wait for them before quitting.
		System.exit(0);
	}

	public static void setupThreads() {
		for( StackCreator s: stackCreator ) {
			Thread t = new Thread( s );
			t.start();
			threadsToEntries.put( s.getClass().getSimpleName() + " Thread", s);
		}
		boolean ready = false;
		while( !ready ) {
			ready = true;
			for( StackCreator s: stackCreator ) {
				ready &= s.isReady();
			}
		}
	}

}
