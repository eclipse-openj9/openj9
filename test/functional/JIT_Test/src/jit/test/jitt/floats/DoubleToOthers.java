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
public class DoubleToOthers extends jit.test.jitt.Test 
{
	@Test
	public void testDoubleToOthers() 
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

			        if (fvalue != 5.7f)
				         Assert.fail("DoubleToOthers->run(): Bad result for test #1");

			ivalue = tstD2I();         
			
			        if (ivalue != 5)
				         Assert.fail("DoubleToOthers->run(): Bad result for test #2");

			svalue = tstD2S();         
			
			        if (svalue != 5)
				         Assert.fail("DoubleToOthers->run(): Bad result for test #3");

			bvalue = tstD2B();         
			
			        if (bvalue != 5)
				         Assert.fail("DoubleToOthers->run(): Bad result for test #4");

			cvalue = tstD2C();         
			
			        if (cvalue != (char) 5)
				         Assert.fail("DoubleToOthers->run(): Bad result for test #5");
	
			lvalue = tstD2L();         
			
			        if (lvalue != 5)
				         Assert.fail("DoubleToOthers->run(): Bad result for test #6");

		}

	}


	private float tstD2F() 
	{	
		double a1 = 9.9d; 
		float f1 = (float) a1;
		
		double a2 = -7.5d;
		float f2 = (float) a2;
		
		double a3 = 0;
		float f3 = (float) a3;
		
		float f4 = (float) 3.3d;
		
		return f1+f2+f3+f4;		
	
	}

	private int tstD2I() 
	{	
		double a1 = 9.9f; 
		int i1 = (int) a1;
		
		double a2 = -7.5f;
		int i2 = (int) a2;
		
		double a3 = 0;
		int i3 = (int) a3;
		
		int i4 = (int) 3.3f;
		
		return i1+i2+i3+i4;		
	}

	private short tstD2S() 
	{	
		double a1 = 9.9f; 
		short s1 = (short) a1;
		
		double a2 = -7.5f;
		short s2 = (short) a2;
		
		double a3 = 0;
		short s3 = (short) a3;
		
		short s4 = (short) 3.3f;
		
		return (short) (s1+s2+s3+s4);		
	
	}

	private byte tstD2B() 
	{	
		double a1 = 9.9f; 
		byte b1 = (byte) a1;
		
		double a2 = -7.5f;
		byte b2 = (byte) a2;
		
		double a3 = 0;
		byte b3 = (byte) a3;
		
		byte b4 = (byte) 3.3f;
		
		return (byte) (b1+b2+b3+b4);	
	
	}

	private char tstD2C()
	{	
		double a1 = 9.9f; 
		char c1 = (char) a1;
		
		double a2 = -7.5f;
		char c2 = (char) a2;
		
		double a3 = 0;
		char c3 = (char) a3;
		
		char c4 = (char) 3.3f;
		
		return (char) (c1+c2+c3+c4);		
	
	}

	private long tstD2L()
	{
		double a1 = 9.9f; 
		long l1 = (long) a1;
		
		double a2 = -7.5f;
		long l2 = (long) a2;
		
		double a3 = 0;
		long l3 = (long) a3;
		
		long l4 = (long) 3.3f;
		
		return l1+l2+l3+l4;	
	}

}
