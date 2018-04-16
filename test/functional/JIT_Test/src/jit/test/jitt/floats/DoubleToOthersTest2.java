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
package jit.test.jitt.floats;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity","component.jit" })
public class DoubleToOthersTest2 extends jit.test.jitt.Test 
{
	@Test
	public void testDoubleToOthersTest2() 
	{
		float fvalue = 0;
		int ivalue = 0;
		short svalue = 0;
		byte bvalue = 0;
		char cvalue = (char) 0;
		long lvalue = 0;

		for (long j = 0; j < sJitThreshold; j++)
		{ 
			fvalue = tstD2F();            

			        if (fvalue != 9.0f)
				         Assert.fail("DoubleToOthersTest2->run(): Bad result for test #1");

			ivalue = tstD2I();         
			
			        if (ivalue != -7)
				         Assert.fail("DoubleToOthersTest2->run(): Bad result for test #2");

			svalue = tstD2S();         
			
			        if (svalue != 0)
				         Assert.fail("DoubleToOthersTest2->run(): Bad result for test #3");

			bvalue = tstD2B();         
			
			        if (bvalue != 3.0)
				         Assert.fail("DoubleToOthersTest2->run(): Bad result for test #4");

			cvalue = tstD2C();         
			
			        if (cvalue != (char) 0)
				         Assert.fail("DoubleToOthersTest2->run(): Bad result for test #5");
	
			lvalue = tstD2L();         
			
			        if (lvalue != 9.0)
				         Assert.fail("DoubleToOthersTest2->run(): Bad result for test #6");

		}

	}


	private float tstD2F() 
	{	
		double a1 = 9.0d; 
		float f1 = (float) a1;
		
		return f1;		
	}

	private int tstD2I() 
	{			
		double a2 = -7.4f;
		int i2 = (int) a2;
		
		return i2;		
	}

	private short tstD2S() 
	{			
		double a3 = 0;
		short s3 = (short) a3;
		
		return s3;		
	
	}

	private byte tstD2B() 
	{	
		
		byte b4 = (byte) 3.0f;
		
		return b4;	
	
	}

	private char tstD2C()
	{	
		double a3 = 0;
		char c3 = (char) a3;
		
		return c3;		
	
	}

	private long tstD2L()
	{
		double a1 = 9.0f; 
		long l1 = (long) a1;
				
		return l1;	
	}

}
