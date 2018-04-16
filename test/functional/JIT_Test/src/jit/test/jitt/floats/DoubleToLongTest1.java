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
public class DoubleToLongTest1 extends jit.test.jitt.Test 
{
	@Test
	public void testDoubleToLongTest1() 
	{
		long value = 0;

		for (long j = 0; j < sJitThreshold; j++)
		{ 
			value = tstD2L();

			if (value != 9223372036854775807L)
				Assert.fail("DoubleToLongTest1->run(): Bad result for test #1");

			value = tstD2LConst();
			
			if (value != 9223372036854775807L)
				Assert.fail("DoubleToLongTest1->run(): Bad result for test #2");

			value = tstD2LReturn();
			
			if (value != 128)
				Assert.fail("DoubleToLongTest1->run(): Bad result for test #3");

			value = tstD2LReturnConst();
			
			if (value != 128)
				Assert.fail("DoubleToLongTest1->run(): Bad result for test #4");

			value = tstD2L();
			
			if (value != 9223372036854775807L)
				Assert.fail("DoubleToLongTest1->run(): Bad result for test #5");
	
			value = tstD2LNeg();
			
			if (value != -1)
				Assert.fail("DoubleToLongTest1->run(): Bad result for test #6");
				
			value = tstD2LNegConst();
			
			if (value != -1)
				Assert.fail("DoubleToLongTest1->run(): Bad result for test #7");

		}

	}


	private long tstD2L() 
	{	
		double a = (double) 3.40282346638528860e+38; 
		long b = (long) a;
		return b;		
	
	}

	private long tstD2LConst() 
	{	
		long b = (long) 3.40282346638528860e+38;
		return b;		
	
	}

	private long tstD2LReturn() 
	{	
		double a = 128;
		return (long) a;		
	
	}

	private long tstD2LReturnConst() 
	{	
		return (long) 128.123;		
	
	}

	private long tstD2LNeg() 
	{	
		double a = (double) -1.342; 
		long b = (long) a;
		return b;		
	
	}

	private long tstD2LNegConst()
	{
		long b = (long) -1.342;
		return b;	
	}

}
