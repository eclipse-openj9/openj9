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
public class DoubleArrayLoadTest2 extends jit.test.jitt.Test 
{
	double[] d_array = {9.0d, 0d, -9.0d};

	@Test
	public void testDoubleArrayLoadTest2() 
	{
		double value = 0;

		for (int j = 0; j < sJitThreshold; j++)
		{ 
			value = tstArrayLoad1();      

			 if (value != 9.0d)
				 Assert.fail("DoubleArrayLoadTest2->run(): Bad result for test #1");

			value = tstArrayLoad2();      
			
			 if (value != 0d)
				 Assert.fail("DoubleArrayLoadTest2->run(): Bad result for test #2");

			value = tstArrayLoad3();      
			 if (value != -9.0d)
				 Assert.fail("DoubleArrayLoadTest2->run(): Bad result for test #3");			

		}

	}


	private double tstArrayLoad1() 
	{	
		double d_value = 100;
		
		// store into array values
		d_value = d_array[0];
				
		return d_value;		
	
	}

	private double tstArrayLoad2() 
	{	
		double d_value = 99;
		int array_index = 1;
		
		d_value = d_array[array_index];
				
		return d_value;		
	
	}

	private double tstArrayLoad3()
	{
		double d_value=99;
		
		d_value = d_array[2];
		
		return d_value;
		
	}
}
