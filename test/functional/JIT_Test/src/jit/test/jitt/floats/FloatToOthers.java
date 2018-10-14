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
public class FloatToOthers extends jit.test.jitt.Test 
{
	@Test
	public void testFloatToOthers() 
	{
		double dvalue = 0;
		int ivalue = 0;
		short svalue = 0;
		byte bvalue = 0;
		char cvalue = (char) 0;
		long lvalue = 0;

		for (long j = 0; j < sJitThreshold; j++)
		{ 
			dvalue = tstF2D();       

			    if (dvalue != 5.699999570846558d)
				    Assert.fail("FloatToOthers->run(): Bad result for test #1");

			ivalue = tstF2I();    
			
			    if (ivalue != 5)
				    Assert.fail("FloatToOthers->run(): Bad result for test #2");

			svalue = tstF2S();    
			
			    if (svalue != 5)
				    Assert.fail("FloatToOthers->run(): Bad result for test #3");

			bvalue = tstF2B();    
			
			    if (bvalue != 5)
				    Assert.fail("FloatToOthers->run(): Bad result for test #4");

			cvalue = tstF2C();    

			    if (cvalue != (char) 5)
				    Assert.fail("FloatToOthers->run(): Bad result for test #5");
	
			lvalue = tstF2L();    
			
			    if (lvalue != 5)
				    Assert.fail("FloatToOthers->run(): Bad result for test #6");

		}

	}


	private double tstF2D() 
	{	
		float a1 = 9.9f; 
		double d1 = (double) a1;
		
		float a2 = -7.5f;
		double d2 = (double) a2;
		
		float a3 = 0;
		double d3 = (double) a3;
		
		double d4 = (double) 3.3f;
		
		return d1+d2+d3+d4;		
	
	}

	private int tstF2I() 
	{	
		float a1 = 9.9f; 
		int i1 = (int) a1;
		
		float a2 = -7.5f;
		int i2 = (int) a2;
		
		float a3 = 0;
		int i3 = (int) a3;
		
		int i4 = (int) 3.3f;
		
		return i1+i2+i3+i4;		
	}

	private short tstF2S() 
	{	
		float a1 = 9.9f; 
		short s1 = (short) a1;
		
		float a2 = -7.5f;
		short s2 = (short) a2;
		
		float a3 = 0;
		short s3 = (short) a3;
		
		short s4 = (short) 3.3f;
		
		return (short) (s1+s2+s3+s4);		
	
	}

	private byte tstF2B() 
	{	
		float a1 = 9.9f; 
		byte b1 = (byte) a1;
		
		float a2 = -7.5f;
		byte b2 = (byte) a2;
		
		float a3 = 0;
		byte b3 = (byte) a3;
		
		byte b4 = (byte) 3.3f;
		
		return (byte) (b1+b2+b3+b4);
	
	}

	private char tstF2C()
	{
		float a1 = 9.9f;
		char c1 = (char) a1;
		
		float a2 = -7.5f;
		char c2 = (char) a2;
		
		float a3 = 0;
		char c3 = (char) a3;
		
		char c4 = (char) 3.3f;

		return (char) (c1+c2+c3+c4);
	
	}

	private long tstF2L()
	{
		float a1 = 9.9f; 
		long l1 = (long) a1;
		
		float a2 = -7.5f;
		long l2 = (long) a2;
		
		float a3 = 0;
		long l3 = (long) a3;
		
		long l4 = (long) 3.3f;
		
		return l1+l2+l3+l4;	
	}

}
