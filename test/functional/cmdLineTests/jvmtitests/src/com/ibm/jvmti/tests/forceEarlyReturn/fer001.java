/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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

package com.ibm.jvmti.tests.forceEarlyReturn;


/*
 *  Deprecated in favour of fer003
 * 
 */


public class fer001 
{
    TestForceEarlyReturnRunner runner;
    TestForceEarlyReturnTrigger trigger;
    
    Semaphore sem;
	
    public fer001()
    {
    }
    
    public boolean setup(String args)
    {   	    	
    	sem = new Semaphore(0);    	
    	runner = new TestForceEarlyReturnRunner();
    	trigger = new TestForceEarlyReturnTrigger();

    	runner.setName("Thread-Runner");
     	trigger.setName("Thread-Trigger");
    	
     	runner.setSemaphore(sem);
    	trigger.setSemaphore(sem);
    	
    	runner.start();   	
    	trigger.start();
    	
    	// TODO: fix this to wait for the runner thread to start properly
    	try {
			Thread.sleep(100);
		} catch (InterruptedException e1) {		
			e1.printStackTrace();
		}
		
    	/*
    	try {    		
			runner.join();
		} catch (InterruptedException e) {			
			e.printStackTrace();
		}
    	*/  
		return true;
    }
    
    public boolean teardown()
    {
    	runner.sendCommand("exit");
    	trigger.sendCommand("exit");
    	
    	try {    		
			runner.join();
			trigger.join();
		} catch (InterruptedException e) {			
			e.printStackTrace();
		}
    	
    	return true;
    }
	 
    // ****************************************** //    
    public boolean testReturnInt()
    {
    	runner.sendCommand("testReturnInt");
    	trigger.sendCommand("triggerReturnInt");
    	return runner.recvResult();
    }
    public String helpReturnInt()
    {
    	return "Tests ForceEarlyReturnInt() API.";
    }
    
    
    // ****************************************** //   
    public boolean testReturnFloat()
    {
    	runner.sendCommand("testReturnFloat");
    	trigger.sendCommand("triggerReturnFloat");
    	return runner.recvResult();
    }
    public String helpReturnFloat()
    {
    	return "Tests ForceEarlyReturnFloat() API.";
    }
    

    // ****************************************** //   
    public boolean testReturnLong()
    {
    	runner.sendCommand("testReturnLong");
    	trigger.sendCommand("triggerReturnLong");
    	return runner.recvResult();
    }
    public String helpReturnLong()
    {
    	return "Tests ForceEarlyReturnLong() API.";
    }
    
    
    // ****************************************** //   
    public boolean testReturnDouble()
    {
    	runner.sendCommand("testReturnDouble");
    	trigger.sendCommand("triggerReturnDouble");
    	return runner.recvResult();
    }
    public String helpReturnDouble()
    {
    	return "Tests ForceEarlyReturnDouble() API.";
    }
    
    // ****************************************** //
    public boolean testReturnVoid()
    {
    	runner.sendCommand("testReturnVoid");
    	trigger.sendCommand("triggerReturnVoid");
    	return runner.recvResult();
    }
    public String helpReturnVoid()
    {
    	return "Tests ForceEarlyReturnVoid() API.";
    }
    
    // ****************************************** //
    public boolean testReturnObject()
    {
    	runner.sendCommand("testReturnObject");
    	trigger.sendCommand("triggerReturnObject");
    	return runner.recvResult();
    }
    public String helpReturnObject()
    {
    	return "Tests ForceEarlyReturnObject() API.";
    }
          
}

