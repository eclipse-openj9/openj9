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
public class DoubleArrayStoreTest2 extends jit.test.jitt.Test 
{
	double[] d_array = new double[3];

	@Test
	public void testDoubleArrayStoreTest2() 
	{
		double value = 0;
	
		for (int j = 0; j < sJitThreshold; j++)
		{ 
			tstArrayStore1(9.0d, 2.0d, -9.0d);          
			if (d_array[0] != 9.0d)
				Assert.fail("DoubleArrayStoreTest2->run(): Bad result for test #1");

			if (d_array[1] != 2.0d)
				Assert.fail("DoubleArrayStoreTest2->run(): Bad result for test #2");

			if (d_array[2] != -9.0d)
				Assert.fail("DoubleArrayStoreTest2->run(): Bad result for test #3");

			tstArrayStore2();          			
			
			if (d_array[0] != 1.79769313486231570e+308d)
				Assert.fail("DoubleArrayStoreTest2->run(): Bad result for test #4");

			if (d_array[1] != 4.94065645841246544e-324d)
				Assert.fail("DoubleArrayStoreTest2->run(): Bad result for test #5");

			if (d_array[2] != 0d)
				Assert.fail("DoubleArrayStoreTest2->run(): Bad result for test #6");

		}

	}


	private void tstArrayStore1(double a1, double a2, double a3) 
	{	
		d_array[0]= a1; 
		d_array[1]= a2;
		d_array[2]= a3;
			
	}

	private void tstArrayStore2() 
	{	
		d_array[0] = (double) 1.79769313486231570e+308d;
		d_array[1] = (double) 4.94065645841246544e-324d;
		d_array[2] = (double) 0;
					
	}


}
