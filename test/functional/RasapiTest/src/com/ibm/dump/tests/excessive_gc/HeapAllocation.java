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
package com.ibm.dump.tests.excessive_gc;

import sun.misc.*;
import java.util.*;

public class HeapAllocation {

	static Hashtable globalTable = new Hashtable();

	public static void main(String args[]) {
		// Takes 2 parameters - integer numbers of threads and objects
		int nThreads=1,nObjects=100000;

		if (args != null && args.length == 2) {
			try {
				nThreads = Integer.parseInt(args[0]);
				nObjects = Integer.parseInt(args[1]);
			} catch (NumberFormatException nfe){
				System.out.println("Test program HeapAllocation.main() - bad parameter(s)");
				return;
			}
		}

		Object[] objects = new Object[nThreads];
		for (int i=0; i<nThreads; i++) {
			objects[i] = new String("object " + i + " ");
			myThread t = new myThread(i);
			t.init(objects[i], objects[i], nObjects);
			t.start();
		}
		System.out.println("Test program com.ibm.dump.tests.HeapAllocation.main() - " + nThreads + " threads running with " + nObjects + " objects");

		// Wait for 2 seconds - allows JVMRI agent to cut in and trigger dump
		try {
			java.lang.Thread.sleep(2000);
		} catch (InterruptedException e){
			e.printStackTrace();
			throw new RuntimeException("com.ibm.dump.tests.HeapAllocation.main(): thread sleep exception");
		}
	}

	// Thread implementation - inner class
	private static class myThread extends Thread {

		String string1,string2;
		int threadID,nObjects;
		Hashtable table;

		myThread(int i) {
			threadID = i;
			table = new Hashtable();
			globalTable.put(new Integer(i),table);
		}

		public void init(Object p1, Object p2, int p3){
			string1 = (String)p1;
			string2 = (String)p2;
			nObjects = p3;
		}

		public void run(){
			synchronized(string1){
				Object item;
				for (int i=1; i<=nObjects; i++) {
					item = doIt(i);
				}
				yield();
				// Now fill up some heap
				for (int i=1; i<=4 ; i++) {
					string1 = string1 + string2;
					string2 = string2 + string1;
				}
			}
		}

		Object doIt(int n) {
			Integer item = new Integer(n);
			return item;
		}
	} // end class myThread

} // end class myDoWork
