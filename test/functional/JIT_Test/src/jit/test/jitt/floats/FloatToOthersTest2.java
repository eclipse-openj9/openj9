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
public class FloatToOthersTest2 extends jit.test.jitt.Test 
{
	@Test
	public void testFloatToOthersTest2() 
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

			        if (dvalue != 9.0d)
				         Assert.fail("FloatToOthersTest2->run(): Bad result for test #1");

			ivalue = tstF2I();         
			
			        if (ivalue != -7)
				         Assert.fail("FloatToOthersTest2->run(): Bad result for test #2");

			svalue = tstF2S();         
			
			        if (svalue != 0)
				         Assert.fail("FloatToOthersTest2->run(): Bad result for test #3");

			bvalue = tstF2B();         
			
			        if (bvalue != 3)
				         Assert.fail("FloatToOthersTest2->run(): Bad result for test #4");

			cvalue = tstF2C();         

	
			lvalue = tstF2L();         
			
			        if (lvalue != 3)
				         Assert.fail("FloatToOthersTest2->run(): Bad result for test #6");

		}

	}


	private double tstF2D() 
	{	
		float a1 = 9.0f; 
		double d1 = (double) a1;
				
		return d1;		
	
	}

	private int tstF2I() 
	{	
		
		float a2 = -7.5f;
		int i2 = (int) a2;
		
		return i2;		
	}

	private short tstF2S() 
	{	
		
		float a3 = 0;
		short s3 = (short) a3;
		
		return s3;		
	
	}

	private byte tstF2B() 
	{	
		
		byte b4 = (byte) 3.3f;
				
		return b4;
	
	}

	private char tstF2C()
	{
		float a1 = 0xfe;
		char c1 = (char) a1;

		return c1;
	
	}

	private long tstF2L()
	{		
		long l4 = (long) 3.3f;
		
		return l4;	
	}

}
