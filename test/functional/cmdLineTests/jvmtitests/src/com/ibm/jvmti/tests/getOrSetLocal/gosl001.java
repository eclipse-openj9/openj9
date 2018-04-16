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
package com.ibm.jvmti.tests.getOrSetLocal;

import java.util.concurrent.Semaphore;

public class gosl001 
{
	final Semaphore main = new Semaphore(0);
	final Semaphore thrd = new Semaphore(0);
	
	native static boolean getInt(Thread thread, int stackDepth, Class clazz, String methodName, String methodSignature, String varName, int expectedInt);
	
	public boolean testGetSingleIntLocal() 
	{
		IntContainer thread = new IntContainer();
		
		thread.start();

		try {
		
			
			//System.out.println("main: pre acq");
			thrd.acquire();    // wait for the runner to signal that its in the right place
			//System.out.println("main: post acq, calling getInt");			
			//System.out.println("main: back from getInt, release");
			main.release();  // signal IntContainer to exit
			System.out.println("main: MADE SURE method1 is compiled"); 
			
			
			
			//System.out.println("main: pre acq");
			thrd.acquire();    // wait for the runner to signal that its in the right place
			//System.out.println("main: post acq, calling getInt");
			getInt(thread, 1, thread.getClass(), "method1", "()I", "localInt", 1234);
			//System.out.println("main: back from getInt, release");
			main.release();  // signal IntContainer to exit
			//System.out.println("main: join");    
			thread.join();
			//System.out.println("main: joined"); 

		} catch (InterruptedException e) {			
			e.printStackTrace();
		}
	    
	    return true;
	}
	
	public String helpGetSingleIntLocal()
	{
		return "retrieve a single int local from a frame containing just that";		
	}
	
	
	private class IntContainer extends Thread
	{
		public void run() 
		{ 
			try {
				method1();
				method1();
				
			} catch (InterruptedException e) {
				e.printStackTrace();
			}
			
			System.out.println("method1: run done"); 
		}
		
		synchronized private int method1() throws InterruptedException 
		{
			int localInt = 1234;
			int int2 = 0x22222222;
			int int3 = 0x33333333;
			int int4 = 0x44444444;
			int int5 = 0x55555555;
					
					
			//System.out.println("method1: waiting to be released"); 
			thrd.release();  // signal main thread that we are in the right frame
			//System.out.println("method1: acquiring sema");			
			main.acquire();  // wait for the runner to finish performing operations on this frame
			//System.out.println("method1: released, returning");
			
			return localInt;
		}
		
	}

	
	
}
