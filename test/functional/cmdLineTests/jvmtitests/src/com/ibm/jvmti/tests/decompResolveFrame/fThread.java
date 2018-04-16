package com.ibm.jvmti.tests.decompResolveFrame;
/*******************************************************************************
 * Copyright (c) 2006, 2018 IBM Corp. and others
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
/**
 * fThread.java
 * @author Paul Thwaite
 */


public class fThread extends Thread
{
	private String myName;
	private int count = 1;

	public fThread(String theName)
	{
		this.myName = theName;
	}

	public fThread(){

	}

	public void run()
	{
		while(true)
		{
			this.GoGorun();
		}
	}

	public void GoGorun()
	{
		//logMessage("*");
		for (int q=0; q<100; q++)
		{
			method1();
		}
		//logMessage("About to load the medium object!");

	}

	public void method9()
	{
		//logMessage("_");

		// do some work and keep the thread busy
		for (int n=0; n<10; n++)
		{
			String myString[] = new String[20];
			//MediumObject largeObj = new MediumObject();
			//largeObj.letsLoop();
			for (int l=0; l<10; l++)
			{
				SmallObject smallObj1 = new SmallObject();   // <--- (1) add breakpoint here during execution
				String moreString[] = new String[200];
			}
		}

		if(this.getName().startsWith("Ex"))
		{
			int size = 10;
			SmallObject[] smallObjectsArray = new SmallObject[size];
			smallObjectsArray[size+1] = new SmallObject();
		}
	}

	public void method8()
	{
		int size = 10;				// Uncomment from here to ****END for uncaught exception breakpoint
		int wrongSize = 20;

		SmallObject[] smallObjects = new SmallObject[size];

		try
		{
			for(int i = 0; i < wrongSize; i++)
			{
				smallObjects[i] = new SmallObject(); // Uncaught exception will be thrown here
			}  							// ****END
		}
		catch(ArrayIndexOutOfBoundsException ex)
		{
			//logMessage("Caught!");
		}

		method9();
	}

	public void method7()
	{
		method8();
	}

	public void method6()
	{
		method7();
	}


	public void method5()
	{
		method6();
	}

	public void method4()
	{
		method5();
	}

	public void method3()
	{
		method4();
	}

	public void method2()
	{
		method3();
	}

	public void method1()
	{
		method2();
	}

	public void logMessage(String message)
	{
		java.text.NumberFormat formatter = java.text.NumberFormat.getIntegerInstance();
	    formatter.setMinimumIntegerDigits(2);
	    java.util.Calendar c = java.util.Calendar.getInstance();
		System.out.println(formatter.format(c.get(java.util.Calendar.HOUR_OF_DAY)) + ":" + formatter.format(c.get(java.util.Calendar.MINUTE)) + ":" + formatter.format(c.get(java.util.Calendar.SECOND)) + " >> " + myName + " :: " + message);
	}
}


class SmallObject
{
	void foo()
	{
	}

	static void boo()
	{
	}
}
