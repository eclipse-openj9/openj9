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

package org.openj9.test.fastresolve;

import org.testng.annotations.Test;
import org.testng.AssertJUnit;
import java.util.concurrent.CyclicBarrier;
import org.testng.log4testng.Logger;

@Test(groups = {"level.sanity"})
public class FastresolveTest {

	private static Logger logger = Logger.getLogger(FastresolveTest.class);
	
	final static String aClassName = "LargeNumFieldsClass";	
	final static int numThread = 10;
	static CyclicBarrier barrier;

	@Test
	public void testFastresolveWithDefaultClassLoader(){
		runFastresolveTest(true);
	}

	@Test
	public void testFastresolveWithCustomClassLoader(){
		runFastresolveTest(false);		
	}

	private void runFastresolveTest(boolean isDefaultLoader){
		
		boolean isPass = true;

		barrier = new CyclicBarrier(numThread);

		Worker[] threads = new Worker[numThread];

		for (int i = 0; i < numThread; i++){
			ClassLoader myLoader;
			
			if(isDefaultLoader){
				myLoader = ClassLoader.getSystemClassLoader();
			}else{
				myLoader = new CustomClassLoader();
			}

			threads[i] = new Worker(i, barrier, myLoader, aClassName);
			threads[i].start();
			logger.debug("Start thread "+i);
		}

		for (int j = 0; j < numThread; j++){
			try {
				threads[j].join();
			} catch (InterruptedException e) {
				e.printStackTrace();
			}
		}

		for (int k=0; k < numThread; k++){
			logger.debug("result "+ k + " is "+ threads[k].getResults());
			if (!threads[k].getResults()){
				isPass = false;
				break;
			}
		}

		AssertJUnit.assertTrue("runFastresolveTest fails, isDefaultLoader is " + isDefaultLoader, isPass);
	}
	
	class Worker extends Thread {

		int myWorkerId;
		CyclicBarrier myBarrier;
		ClassLoader myClassLoader;
		String myClassName;
		boolean myResult;

		Worker(int workId, CyclicBarrier barrier, ClassLoader aClassLoader, String className){
			myWorkerId = workId;
			myBarrier = barrier;
			myClassLoader = aClassLoader;
			myClassName = className;			
		}

		public void run() {		

			logger.debug("Worker id "+ myWorkerId + " (thread id = "+ this.getName() + ") is waiting on barrier.");
			try {
				myBarrier.await();				
				logger.debug("Worker id "+ myWorkerId + " (thread id = "+ this.getName() + ") pass barrier, is loading new class " + myClassName);
				Class aClass = Class.forName("org.openj9.test.fastresolve."+ myClassName, true, myClassLoader);
				Object obj = aClass.newInstance();
				logger.debug("Worker id "+ myWorkerId + " (thread id = "+ this.getName() + ") is retrieving field value ");
				obj.toString();
				myResult = true;	
			} catch (Exception e) {	
				myResult = false;				
				e.printStackTrace();	
				AssertJUnit.fail("Worker id "+ myWorkerId + " (thread id = "+ this.getName() + ") failed");
			}
			logger.debug("Worker id "+ myWorkerId + " (thread id = "+ this.getName() + ") result is "+ myResult);											
			return;						
		}

		public boolean getResults(){
			return myResult;
		}

	}



}




