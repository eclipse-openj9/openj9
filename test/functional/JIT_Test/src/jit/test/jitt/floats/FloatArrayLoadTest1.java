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
public class FloatArrayLoadTest1 extends jit.test.jitt.Test 
{
	@Test
	public void testFloatArrayLoadTest1() 
	{
		float value = 0;

		for (int j = 0; j < sJitThreshold; j++)
		{ 
			value = tstArraySum();      

			 if (value != 499500.0)
				 Assert.fail("FloatArrayLoadTest1->run(): Bad result for test #1");

			value = tstArrayLoadReturn();      
			
			 if (value != 14.5)
				 Assert.fail("FloatArrayLoadTest1->run(): Bad result for test #2");

			value = tst2DArrayLoadSum();      
			 if (value != 190.0)
				 Assert.fail("FloatArrayLoadTest1->run(): Bad result for test #3");
			
			

		}

	}


	private float tstArraySum() 
	{	
		float[] f_array = new float[1000];
		float sum = 0;
		
		// store into array values
		for(int i=0; i < 1000; i++)
		{  f_array[i]=(float) i; }
		
		// sum the values of the array
		for(int i=0; i < 1000; i++)
		{  sum=sum + f_array[i]; }
		
		return sum;		
	
	}

	private float tstArrayLoadReturn() 
	{	
		float[] f_array = {10, 11, 12, 13, 14};
		float a = (float) 0.5;
				
		return f_array[4] + a ;		
	
	}

	private float tst2DArrayLoadSum()
	{
		float twoD[][] = new float[4][5];
		int k=0;
		float sum=0;
		
		// store into 2D array values
		for(int i=0; i<4; i++)
		   for(int j=0;j<5;j++) {
		   	twoD[i][j]=(float) k;
		   	k++;
		   }
		
		// load from the array   
		for(int i=0; i<4; i++)
		   for(int j=0;j<5;j++)
		   	sum = sum + twoD[i][j];
		   	
		return sum;
		
	}
}
