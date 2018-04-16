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
public class FloatDivisionTest1 extends jit.test.jitt.Test 
{
	@Test
	public void testFloatDivisionTest1() 
	{
		float value = 0;

		for (int j = 0; j < sJitThreshold; j++)
		{ 
			value = tstFDivide();           
			      if (value != 0.75)
				        Assert.fail("FloatDivisionTest1->run(): Bad result for test #1");
			

			value = tstFDivideConst();       

			      if (value != (float) 0.5094416)
				        Assert.fail("FloatDivisionTest1->run(): Bad result for test #2");

			value = tstFDivideReturn();       

			      if (value != (float) 1.0689648)
				        Assert.fail("FloatDivisionTest1->run(): Bad result for test #3");

			value = tstFDivideReturnConst();       

			   if (value != (float) 1.0)
				    Assert.fail("FloatDivisionTest1->run(): Bad result for test #4");

			value = tstFDivide();           

			   if (value != (float) 0.75)
				    Assert.fail("FloatDivisionTest1->run(): Bad result for test #5");
	
			value = tstFDivideNeg();       
			
			   if (value != (float) -1.4E-45)
				    Assert.fail("FloatDivisionTest1->run(): Bad result for test #6");
				
			value = tstFDivideNegConst();       
			
			   if (value != (float) 1.342)
				    Assert.fail("FloatDivisionTest1->run(): Bad result for test #7");

			value = tstFDivideReturnNegConst();       
			
			   if (value != (float) 1.0)
				    Assert.fail("FloatDivisionTest1->run(): Bad result for test #8");
				
		
			value = tstFDivideZeroAndNonZero();       
			   if (value != (float) 0.0)
				    Assert.fail("FloatDivisionTest1->run(): Bad result for test #10");			
				
			value = tstFDivideNegativeIdentity();       
			   if (value != (float) -1.0)
				    Assert.fail("FloatDivisionTest1->run(): Bad result for test #12");						

			value = tstFDivideNegativeIdentity2();       
			   if (value != (float) -1.0)
				    Assert.fail("FloatDivisionTest1->run(): Bad result for test #13");
				
			value = tstFDivideMax();       
			   if (value != 1.0)
				    Assert.fail("FloatDivisionTest1->run(): Bad result for test #14");	
				
			value = tstFDivideMin();       
			   if (value != 1.0)
				    Assert.fail("FloatDivisionTest1->run(): Bad result for test #15");	
						
			value = tstFDivideReturnMax();       
			   if (value != 1.0)
				    Assert.fail("FloatDivisionTest1->run(): Bad result for test #16");	
				
			value = tstFDivideReturnConstMax();       
			   if (value != 1.0)
				    Assert.fail("FloatDivisionTest1->run(): Bad result for test #17");					
		
			value = tstFDivideReturnMin();       
			   if (value != (float) 1.0)
				    Assert.fail("FloatDivisionTest1->run(): Bad result for test #18");			
			
			value = tstFDivideReturnConstMin();       
			   if (value != (float) 1.0)
				    Assert.fail("FloatDivisionTest1->run(): Bad result for test #18");
				    
			value = tstFDivideNan();    
			   if (!Float.isNaN(value))
				    Assert.fail("FloatDivisionTest1->run(): Bad result for test #19");
				    	
			value = tstFDividePositive();    
			   if (value != 3.0)
				    Assert.fail("FloatDivisionTest1->run(): Bad result for test #20");

			value = tstFDivideYieldNegative();    
			   if (value != -3.0)
				    Assert.fail("FloatDivisionTest1->run(): Bad result for test #21");				    
				    
			value = tstFDivideYieldPositive2();    
			   if (value != 3.0)
				    Assert.fail("FloatDivisionTest1->run(): Bad result for test #22");
				    
			value = tstFDivideBoundaryMaxTest();    
			   if (value != (float) 3.4028235e38)
				    Assert.fail("FloatDivisionTest1->run(): Bad result for test #23");
				    
			value = tstFDivideBoundaryMaxMin1();    
			   if (value != 0.0)
				    Assert.fail("FloatDivisionTest1->run(): Bad result for test #24");
				    
			value = tstFDivideBoundaryMaxMin2();    
			   if (!Float.isInfinite(value))
				    Assert.fail("FloatDivisionTest1->run(): Bad result for test #25");
				    
			value = tstFDivideOverBoundary();    
			   if (!Float.isInfinite(value))
				    Assert.fail("FloatDivisionTest1->run(): Bad result for test #26");
		
			value = tstFDivideInfinityByInfinityTest1();    
			   if (!Float.isNaN(value))
				    Assert.fail("FloatDivisionTest1->run(): Bad result for test #27");
			
			value = tstFDivideInfinityByInfinityTest2();    
			   if (!Float.isNaN(value))
				    Assert.fail("FloatDivisionTest1->run(): Bad result for test #28");
			
			value = tstFDivideInfinityByFinite();    
			   if (!Float.isInfinite(value))
				    Assert.fail("FloatDivisionTest1->run(): Bad result for test #29");

			value = tstFDivideFiniteByInfinite();    
			   if (value != 0.0)
				    Assert.fail("FloatDivisionTest1->run(): Bad result for test #30");
			
//			value = tstFDivideZeroByZero();    
//			   if (!Float.isInfinite(value))
//				    Assert.fail("FloatDivisionTest1->run(): Bad result for test #31");

			value = tstFDivideNonZeroByZero();    
			   if (!Float.isInfinite(value))
				    Assert.fail("FloatDivisionTest1->run(): Bad result for test #32");

			value = tstFDivideOverFlow();    
			   if (!Float.isInfinite(value))
				    Assert.fail("FloatDivisionTest1->run(): Bad result for test #33");

			value = tstFDivideUnderFlow();    
			   if (value != (float) 2.8E-45)
				    Assert.fail("FloatDivisionTest1->run(): Bad result for test #34");

			value =	tstFDivideByZeroToYieldInfinity(); 	 			    
			   if (!Float.isInfinite(value))
				    Assert.fail("FloatDivisionTest1->run(): Bad result for test #35");
			
		}

	}

	private float tstFDivide() 
	{	
		float a = (float) 3.0; 
		float b = (float) 4.0;
		float c = a / b;
		return c;		
	
	}

	private float tstFDivideByZeroToYieldInfinity() 
	{	
		float a = (float) 3/0; 
		float b = (float) 4.0;
		float c = a / b;
		return c;		
	
	}

	private float tstFDivideConst() 
	{	
		float b = (float) 3.0 / (float) 5.8888;
		return b;		
	
	}				

	private float tstFDivideReturn() 
	{	
		float a = (float) 3.44444; 
		float b = (float) 3.22222;
		return a/b;	
	}

	private float tstFDivideReturnConst() 
	{	
		return (float) 3.402 / (float) 3.402;	
	
	}

	private float tstFDivideNeg() 
	{	
		float a = (float) 1.40129846432481707e-45; 
		float b = (float) -1;
		float c = a/b;
		return c;		
	
	}

	private float tstFDivideNegConst()
	{
		float b = (float) -1.342 / -1;
		return b;	
	}
	
	private float tstFDivideReturnNegConst()
	{
		return (float) -1.401 / (float) -1.401;	
	}
	
	private float tstFDivideZeroAndNonZero()
	{
		float b = 0 / (float) 3.40282346638528860e+38;
		return b;	
	}
	
	private float tstFDivideNegativeIdentity()
	{
		float b = (float) 3.40282346638528860e+38 / (float) -3.40282346638528860e+38;
		return b;	
	}	
	
	private float tstFDivideNegativeIdentity2()
	{
		float b = (float) 1.40129846432481707e-45 / (float) -1.40129846432481707e-45;
		return b;	
	}
	
	private float tstFDivideMax() 
	{	
		float a = (float) 3.40282346638528860e+38; 
		float b = (float) 3.40282346638528860e+38;
		float c = a / b;
		return c;		
	
	}

	private float tstFDivideMin() 
	{	
		float a = (float) 1.40129846432481707e-45; 
		float b = (float) 1.40129846432481707e-45;
		float c = a / b;
		return c;		
	
	}

	private float tstFDivideReturnMax() 
	{	
		float a = (float) 3.40282346638528860e+38; 
		float b = (float) 3.40282346638528860e+38;
		return a/b;	
	}

	private float tstFDivideReturnConstMax() 
	{	
		return (float) 3.40282346638528860e+38 / (float) 3.40282346638528860e+38;	
	
	}
	
	private float tstFDivideReturnMin() 
	{	
		float a = (float) 1.40129846432481707e-45; 
		float b = (float) 1.40129846432481707e-45;
		return a/b;	
	}
	
	private float tstFDivideReturnConstMin() 
	{	
		return (float) 1.40129846432481707e-45 / (float) 1.40129846432481707e-45;	
	
	}

	private float tstFDivideNan() 
	{	
		float a;
		float b = (float) 3.4;
		a = Float.NaN;  // to produce a NaN value
		float c = a/b;
	
		return c;		
	}
	
	private float tstFDividePositive() 
	{	
		float a = (float) 9.0;
		float b = (float) 3.0;
		float c = a/b;
	
		return c;		
	}

	private float tstFDivideYieldNegative() 
	{	
		float a = (float) -9.0;
		float b = (float) 3.0;
		float c = a/b;
	
		return c;		
	}	

	private float tstFDivideYieldPositive2() 
	{	
		float a = (float) -9.0;
		float b = (float) -3.0;
		float c = a/b;
	
		return c;		
	}

	private float tstFDivideBoundaryMaxTest() 
	{	
		float a = (float) 3.40282346638528860e+38;
		float b = (float) 1;
		float c = a/b;
	
		return c;		
	}

	private float tstFDivideBoundaryMinTest() 
	{	
		float a = (float) 1.40129846432481707e-45;
		float b = (float) 1;
		float c = a/b;
	
		return c;		
	}
	
	private float tstFDivideBoundaryMaxMin1() 
	{	
		float a = (float) 1.40129846432481707e-45;
		float b = (float) 3.40282346638528860e+38;
		float c = a/b;
	
		return c;		
	}
	
	private float tstFDivideBoundaryMaxMin2() 
	{	
		float a = (float) 3.40282346638528860e+38;
		float b = (float) 1.40129846432481707e-45;
		float c = a/b;
	
		return c;		
	}

	private float tstFDivideOverBoundary() 
	{	
		float a = (float) 3.40282346638528860e+38;
		float b = (float) 3.40282346638528860e+38;
		float c = (a*100)/b;
	
		return c;		
	}	

	private float tstFDivideInfinityByInfinityTest1() 
	{	
		float a;
		float b;
		
		a = Float.POSITIVE_INFINITY;
		b = Float.POSITIVE_INFINITY;
		
		float c = a/b;
	
		return c;		
	}
	
	private float tstFDivideInfinityByInfinityTest2() 
	{	
		float a;
		float b;
		
		a = Float.NEGATIVE_INFINITY;
		b = Float.NEGATIVE_INFINITY;
		
		float c = a/b;
	
		return c;		
	}	

	private float tstFDivideInfinityByFinite()
	{	
		float a;
		float b;
		
		a = Float.POSITIVE_INFINITY;
		b = (float) 100.99;
		
		float c = a/b;
	
		return c;		
	}	


	private float tstFDivideFiniteByInfinite() 
	{	
		float a;
		float b;
		
		a = Float.POSITIVE_INFINITY;
		b = (float) 100.99;
		
		float c = b/a;
	
		return c;		
	}

	private float tstFDivideZeroByZero() 
	{	
		float c = 0/0;
	
		return c;		
	}	

	private float tstFDivideNonZeroByZero() 
	{	
		float a = (float) 9.88888;
		float b = 0;
		float c = a/b;
	
		return c;		
	}	

	private float tstFDivideOverFlow()
	{	
		float a = (float) 3.40282346638528860e+38;
		float b = (float) 0.5;
		float c = a/b;
	
		return c;		
	}	
	
	private float tstFDivideUnderFlow()
	{	
		float a = (float) 1.40129846432481707e-45;
		float b = (float) 0.5;
		float c = a/b;
	
		return c;		
	}

	
}
