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
public class FloatToIntegerTest1 extends jit.test.jitt.Test 
{
	@Test
	public void testFloatToIntegerTest1() 
	{
		int value = 0;

		for (int j = 0; j < sJitThreshold; j++)
		{ 
			value = tstF2I();

			if (value != 2147483647)
				Assert.fail("FloatToIntegerTest1->run(): Bad result for test #1");

			value = tstF2IConst();

			if (value != 2147483647)
				Assert.fail("FloatToIntegerTest1->run(): Bad result for test #2");

			value = tstF2IReturn();

			if (value != 128)
				Assert.fail("FloatToIntegerTest1->run(): Bad result for test #3");

			value = tstF2IReturnConst();

			if (value != 128)
				Assert.fail("FloatToIntegerTest1->run(): Bad result for test #4");

			value = tstF2I();

			if (value != 2147483647)
				Assert.fail("FloatToIntegerTest1->run(): Bad result for test #5");
	
			value = tstF2INeg();

			if (value != -1)
				Assert.fail("FloatToIntegerTest1->run(): Bad result for test #6");
				
			value = tstF2INegConst();

			if (value != -1)
				Assert.fail("FloatToIntegerTest1->run(): Bad result for test #7");

		}

	}


	private int tstF2I() 
	{	
		float a = (float) 3.40282346638528860e+38; 
		int b = (int) a;
		return b;		
	
	}

	private int tstF2IConst() 
	{	
		int b = (int) 3.40282346638528860e+38;
		return b;		
	
	}

	private int tstF2IReturn() 
	{	
		float a = 128;
		return (int) a;		
	
	}

	private int tstF2IReturnConst() 
	{	
		return (int) 128.123;		
	
	}

	private int tstF2INeg() 
	{	
		float a = (float) -1.342; 
		int b = (int) a;
		return b;		
	
	}

	private int tstF2INegConst()
	{
		int b = (int) -1.342;
		return b;	
	}

}
