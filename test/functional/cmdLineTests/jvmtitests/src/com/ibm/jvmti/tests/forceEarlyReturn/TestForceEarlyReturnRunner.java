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
package com.ibm.jvmti.tests.forceEarlyReturn;


public class TestForceEarlyReturnRunner extends Thread
{
	public Semaphore sem;
	private String cmd = new String();
	private boolean result;
	
	String objectReturnTestString = new String("correct");
	
	public TestForceEarlyReturnRunner()
	{
	}
	
	public void setSemaphore(Semaphore sem)
	{
		this.sem = sem;        	
	}

	public void run()
	{
			
		while (true) {
			
			waitForCommand();
			//System.out.println("MESSAGE: " + cmd);
			
			if (cmd.compareTo("exit") == 0) {
				break;
			}
			if (cmd.compareTo("testReturnInt") == 0) {
				returnResult(testReturnInt());
			}
			if (cmd.compareTo("testReturnLong") == 0) {
				returnResult(testReturnLong());				
			}
			if (cmd.compareTo("testReturnFloat") == 0) {
				returnResult(testReturnFloat());				
			}
			if (cmd.compareTo("testReturnDouble") == 0) {
				returnResult(testReturnDouble());				
			}
			if (cmd.compareTo("testReturnVoid") == 0) {
				returnResult(testReturnVoid());				
			}
			if (cmd.compareTo("testReturnObject") == 0) {
				returnResult(testReturnObject());				
			}
		}
		
		// System.out.println("Runner DONE running");
	}
	
	public void waitForCommand()
	{
		//System.out.println("Runner WAITING");
        synchronized (this) {
			try {
				this.wait();
			} catch (InterruptedException e) {
				e.printStackTrace();
			} 
		}
	}

    public void sendCommand(String s) 
    {
    	//System.out.println("Runner msg SENDING");
        synchronized (this) {
			cmd = s;
			this.notifyAll();
		}
        //System.out.println("Runner msg SENT");
	}
	
    public void returnResult(boolean ret)
    {
    	//System.out.println("returnResult SENDING");
    	synchronized (this) {
			result = ret;
			this.notifyAll();
		}
    }
    
    public boolean recvResult()
    {
		//System.out.println("recvResult WAITING");
        synchronized (this) {
			try {
				this.wait();
			} catch (InterruptedException e) {
				e.printStackTrace();
			}
		}
        //System.out.println("recvResult WAITING DONE");
        return result;
    }
    
	/** ******************************************************* */

	public boolean testReturnInt()
	{
		int ret;

		sem.up();

		ret = earlyReturnInt(0xC0DECAFE, 0xDECAFBAD);
		
		/* The agent code will force a return of a a specific int value */
		if (ret != 10002000) {
			System.out.println("    Error [testEarlyReturnInt]: Incorrect return value [" + ret + "]");
			return false;
		}
		
		return true;
	}

	public int earlyReturnInt(int a, int b)
	{
		error();

		return 0xdeadbeef;
	}

	/**********************************************************/
	
	public boolean testReturnFloat()
	{
		float ret;

		sem.up();
		
		ret = earlyReturnFloat(0xC0DECAFE, 0xDECAFBAD);

		/* The agent code will force a return of a a specific int value */
		if (ret != 10002000.0) {
			System.out.println("    Error [testReturnFloat]: Incorrect return value [" + ret + "]");
			return false;
		}

		return true;
	}

	public float earlyReturnFloat(int a, int b)
	{
		error();

		return 0xdeadbeef;
	}

	/**********************************************************/
	
	public boolean testReturnLong()
	{
		long ret;

		sem.up();

		ret = earlyReturnLong(0xC0DECAFE, 0xDECAFBAD);
		
		/* The agent code will force a return of a a specific int value */
		if (ret != 100020003000L) {
			System.out.println("    Error [testReturnLong]: Incorrect return value [" + ret + "]");
			return false;
		}

		return true;
	}

	public long earlyReturnLong(int a, int b)
	{
		error();

		return 0xdeadbeef;
	}

	/**********************************************************/
	
	public boolean testReturnDouble()
	{
		double ret;

		sem.up();

		ret = earlyReturnDouble(0xC0DECAFE, 0xDECAFBAD);
		
		/* The agent code will force a return of a a specific int value */
		if (ret != 100020003000.0) {
			System.out.println("    Error [testReturnDouble]: Incorrect return value [" + ret + "]");
			return false;
		}

		return true;
	}

	public double earlyReturnDouble(int a, int b)
	{
		error();

		return 0.0;
	}

	/**********************************************************/

	public boolean testReturnVoid()
	{
		sem.up();
		
		earlyReturnVoid(0xC0DECAFE, 0xDECAFBAD);

		return true;
	}

	public void earlyReturnVoid(int a, int b)
	{
		error();

		return;
	}

	/**********************************************************/
	
	public boolean testReturnObject()
	{
		sem.up();
		
		TestForceEarlyReturnRunner ret = earlyReturnObject(0xC0DECAFE, 0xDECAFBAD);

		//sem.up();
		
		if (ret.objectReturnTestString.equals("correct") == false)  {
			System.out.println("    Error [testReturnObject]: Incorrect return value [" +
					ret.objectReturnTestString + "]");
			return false;
		}
		return true;
	}

	public TestForceEarlyReturnRunner earlyReturnObject(int a, int b)
	{
		error();

		TestForceEarlyReturnRunner ret = new TestForceEarlyReturnRunner();

		/* Should never execute this code if ForceEarlyReturnObject works correctly */
		ret.objectReturnTestString = new String("bugus");

		return ret;
	}

	/**********************************************************/

	public void error()
	{
		System.out.println("	Error: should not see this message. Indicates that either: ");
		System.out.println("    	- Force early return did not get processed correctly.");
		System.out.println("    	- Method entry hook in the test agent did not get called.");
	}

}
