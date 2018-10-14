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

public class TestForceEarlyReturnTrigger extends Thread
{
	public Semaphore sem;
	private String cmd = new String();

	public synchronized void run()
	{
		while (true) {
			
			waitForCommand();
			//System.out.println("MESSAGE: " + cmd);
			if (cmd.compareTo("exit") == 0) {
				break;
			}
			
			if (cmd.compareTo("triggerReturnInt") == 0) {
				sem.down();
				triggerForceEarlyReturnInt();
			}
			if (cmd.compareTo("triggerReturnFloat") == 0) {
				sem.down();
				triggerForceEarlyReturnFloat();
			}
			if (cmd.compareTo("triggerReturnLong") == 0) {
				sem.down();
				triggerForceEarlyReturnLong();
			}
			if (cmd.compareTo("triggerReturnDouble") == 0) {
				sem.down();
				triggerForceEarlyReturnDouble();
			}
			if (cmd.compareTo("triggerReturnVoid") == 0) {
				sem.down();
				triggerForceEarlyReturnVoid();
			}
			if (cmd.compareTo("triggerReturnObject") == 0) {
				sem.down();
				triggerForceEarlyReturnObject();
			}
		}
	}

	public void setSemaphore(Semaphore sem)
	{
		this.sem = sem;        	
	}

	public void waitForCommand()
	{
		//System.out.println("Trigger WAITING");
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
    	//System.out.println("Trigger msg SENDING");
        synchronized (this) {
			cmd = s;
			this.notifyAll();
		}
        //System.out.println("Trigger msg SENT");
	}
	
	public synchronized void triggerForceEarlyReturnInt()
	{
	}

	public void triggerForceEarlyReturnLong()
	{
	}

	public void triggerForceEarlyReturnFloat()
	{
	}

	public void triggerForceEarlyReturnDouble()
	{
	}
	
	public void triggerForceEarlyReturnObject()
	{
	}

	public void triggerForceEarlyReturnVoid()
	{
	}


}
