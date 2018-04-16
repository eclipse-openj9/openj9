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
public class FloatArrayStoreTest2 extends jit.test.jitt.Test 
{

	float[] f_array = new float[3];

	@Test
	public void testFloatArrayStoreTest2() 
	{
		float value = 0;
	
		for (int j = 0; j < sJitThreshold; j++)
		{ 
			tstArrayStore1(9.0f, 2.0f, -9.0f);          
			if (f_array[0] != 9.0f)
				Assert.fail("FloatArrayStoreTest2->run(): Bad result for test #1");

			if (f_array[1] != 2.0f)
				Assert.fail("FloatArrayStoreTest2->run(): Bad result for test #2");

			if (f_array[2] != -9.0f)
				Assert.fail("FloatArrayStoreTest2->run(): Bad result for test #3");

			tstArrayStore2();          			
			
			if (f_array[0] != 3.40282346638528860e+38f)
				Assert.fail("FloatArrayStoreTest2->run(): Bad result for test #4");

			if (f_array[1] != 1.40129846432481707e-45f)
				Assert.fail("FloatArrayStoreTest2->run(): Bad result for test #5");

			if (f_array[2] != 0f)
				Assert.fail("FloatArrayStoreTest2->run(): Bad result for test #6");

		}

	}


	private void tstArrayStore1(float a1, float a2, float a3) 
	{	
		f_array[0]= a1; 
		f_array[1]= a2;
		f_array[2]= a3;
			
	}

	private void tstArrayStore2() 
	{	
		f_array[0] = (float) 3.40282346638528860e+38;
		f_array[1] = (float) 1.40129846432481707e-45;
		f_array[2] = (float) 0;
					
	}


}
