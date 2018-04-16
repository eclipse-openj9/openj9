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
public class FloatAdditionTest1 extends jit.test.jitt.Test 
{
	@Test
	public void testFloatAdditionTest1() 
	{
		float value = 0;

		for (float j = 0; j < sJitThreshold; j++)
		{ 
			value = tstFAdd();       

			   if (value != 7.0)
				    Assert.fail("FloatAdditionTest1->run(): Bad result for test #1");

			value = tstFAddConst();   

			   if (value != (float) 8.8888)
				    Assert.fail("FloatAdditionTest1->run(): Bad result for test #2");

			value = tstFAddReturn();   

			   if (value != (float) 6.66666)
				    Assert.fail("FloatAdditionTest1->run(): Bad result for test #3");

			value = tstFAddReturnConst();   

			   if (value != (float) 6.804)
				    Assert.fail("FloatAdditionTest1->run(): Bad result for test #4");

			value = tstFAdd();       

			   if (value != 7.0)
				    Assert.fail("FloatAdditionTest1->run(): Bad result for test #5");
	
			value = tstFAddNeg();   
			
			   if (value != -1.0)
				    Assert.fail("FloatAdditionTest1->run(): Bad result for test #6");
				
			value = tstFAddNegConst();   
			
			   if (value != (float) -2.342)
				    Assert.fail("FloatAdditionTest1->run(): Bad result for test #7");

			value = tstFAddReturnNegConst();   
			
			   if (value != (float) -2.802)
				    Assert.fail("FloatAdditionTest1->run(): Bad result for test #8");
				
			value = tstFAddPosNegZeroes();   

			   if (value != 0.0)
				    Assert.fail("FloatAdditionTest1->run(): Bad result for test #9");
		
			value = tstFAddZeroAndNonZero();   
			   if (value != (float) 3.4028235e38)
				    Assert.fail("FloatAdditionTest1->run(): Bad result for test #10");			

			value = tstFAddZeroes();   
			   if (value != 0.0)
				    Assert.fail("FloatAdditionTest1->run(): Bad result for test #11");	
				
			value = tstFAddObtainZero();   
			   if (value != 0.0)
				    Assert.fail("FloatAdditionTest1->run(): Bad result for test #12");						

			value = tstFAddObtainZero2();   
			   if (value != 0.0)
				    Assert.fail("FloatAdditionTest1->run(): Bad result for test #13");
				
			value = tstFAddMax();   
//			   if (value != float.POSITIVE_INFINITY)
//				    Assert.fail("FloatAdditionTest1->run(): Bad result for test #14");	
				
			value = tstFAddMin();   
			   if (value != (float) 2.8e-45)
				    Assert.fail("FloatAdditionTest1->run(): Bad result for test #15");	
						
			value = tstFAddReturnMax();   
//			   if (value != float.POSITIVE_INFINITY)
//				    Assert.fail("FloatAdditionTest1->run(): Bad result for test #16");	
				
			value = tstFAddReturnConstMax();   
//			   if (value != float.POSITIVE_INFINITY)
//				    Assert.fail("FloatAdditionTest1->run(): Bad result for test #17");					
		
			value = tstFAddReturnMin();   
			   if (value != (float) 2.8e-45)
				    Assert.fail("FloatAdditionTest1->run(): Bad result for test #18");			
			
			value = tstFAddReturnConstMin();   
			   if (value != (float) 2.8E-45)
				    Assert.fail("FloatAdditionTest1->run(): Bad result for test #18");				
				
		}

	}


	private float tstFAdd() 
	{	
		float a = (float) 3.0; 
		float b = (float) 4.0;
		float c = a + b;
		return c;		
	
	}

	private float tstFAddConst() 
	{	
		float b = (float) 3.0 + (float) 5.8888;
		return b;		
	
	}

	private float tstFAddReturn() 
	{	
		float a = (float) 3.44444; 
		float b = (float) 3.22222;
		return a+b;	
	}

	private float tstFAddReturnConst() 
	{	
		return (float) 3.402 + (float) 3.402;	
	
	}

	private float tstFAddNeg() 
	{	
		float a = (float) 1.40129846432481707e-45; 
		float b = (float) -1;
		float c = a+b;
		return c;		
	
	}

	private float tstFAddNegConst()
	{
		float b = (float) -1.342 + -1;
		return b;	
	}
	
	private float tstFAddReturnNegConst()
	{
		return (float) -1.401 + (float) -1.401;	
	}
	
	private float tstFAddPosNegZeroes()
	{
		float b = 0+(-0);
		return b;	
	}
	
	private float tstFAddZeroAndNonZero()
	{
		float b = 0+ (float) 3.40282346638528860e+38;
		return b;	
	}

	private float tstFAddZeroes()
	{
		float b = 0+0;
		return b;	
	}	
	
	private float tstFAddObtainZero()
	{
		float b = (float) 3.40282346638528860e+38 + (float) -3.40282346638528860e+38;
		return b;	
	}	
	
	private float tstFAddObtainZero2()
	{
		float b = (float) 1.40129846432481707e-45 + (float) -1.40129846432481707e-45;
		return b;	
	}
	
	private float tstFAddMax() 
	{	
		float a = (float) 3.40282346638528860e+38; 
		float b = (float) 3.40282346638528860e+38;
		float c = a + b;
		return c;		
	
	}

	private float tstFAddMin() 
	{	
		float a = (float) 1.40129846432481707e-45; 
		float b = (float) 1.40129846432481707e-45;
		float c = a + b;
		return c;		
	
	}

	private float tstFAddReturnMax() 
	{	
		float a = (float) 3.40282346638528860e+38; 
		float b = (float) 3.40282346638528860e+38;
		return a+b;	
	}

	private float tstFAddReturnConstMax() 
	{	
		return (float) 3.40282346638528860e+38 + (float) 3.40282346638528860e+38;	
	
	}
	
	private float tstFAddReturnMin() 
	{	
		float a = (float) 1.40129846432481707e-45; 
		float b = (float) 1.40129846432481707e-45;
		return a+b;	
	}
	
	private float tstFAddReturnConstMin() 
	{	
		return (float) 1.40129846432481707e-45 + (float) 1.40129846432481707e-45;	
	
	}
	
}
