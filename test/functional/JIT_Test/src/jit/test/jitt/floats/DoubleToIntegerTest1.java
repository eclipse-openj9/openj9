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
public class DoubleToIntegerTest1 extends jit.test.jitt.Test 
{
	@Test
	public void testDoubleToIntegerTest1() 
	{
		int value = 0;

		for (int j = 0; j < sJitThreshold; j++)
		{ 
			value = tstD2I();

			if (value != 2147483647)
				Assert.fail("DoubleToIntegerTest1->run(): Bad result for test #1");

			value = tstD2IConst();

			if (value != 0)
				Assert.fail("DoubleToIntegerTest1->run(): Bad result for test #2");

			value = tstD2IReturn();

			if (value != 128)
				Assert.fail("DoubleToIntegerTest1->run(): Bad result for test #3");

			value = tstD2IReturnConst();

			if (value != 128)
				Assert.fail("DoubleToIntegerTest1->run(): Bad result for test #4");

			value = tstD2I();

			if (value != 2147483647)
				Assert.fail("DoubleToIntegerTest1->run(): Bad result for test #5");
	
			value = tstD2INeg();

			if (value != -1)
				Assert.fail("DoubleToIntegerTest1->run(): Bad result for test #6");
				
			value = tstD2INegConst();

			if (value != -1)
				Assert.fail("DoubleToIntegerTest1->run(): Bad result for test #7");

		}

	}


	private int tstD2I() 
	{	
		double a = (double) 1.79769313486231570e+308d; 
		int b = (int) a;
		return b;		
	
	}

	private int tstD2IConst() 
	{	
		int b = (int) 4.94065645841246544e-324d;
		return b;		
	
	}

	private int tstD2IReturn() 
	{	
		double a = 128;
		return (int) a;		
	
	}

	private int tstD2IReturnConst() 
	{	
		return (int) 128.123;		
	
	}

	private int tstD2INeg() 
	{	
		double a = (double) -1.342D; 
		int b = (int) a;
		return b;		
	
	}

	private int tstD2INegConst()
	{
		int b = (int) -1.342D;
		return b;	
	}

}
