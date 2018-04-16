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
public strictfp class DoubleDivisionTest1 extends jit.test.jitt.Test 
{
	@Test
	public void testDoubleDivisionTest1() 
	{
		double value = 0;

		for (int j = 0; j < sJitThreshold; j++)
		{ 
			value = tstDDivide();               
			                 if (value != 0.75d)
				             Assert.fail("DoubleDivisionTest1->run(): Bad result for test #1");
			

			value = tstDDivideConst();           

			                 if (value != 0.5094416519494634d)
				             Assert.fail("DoubleDivisionTest1->run(): Bad result for test #2");

			value = tstDDivideReturn();           

			                 if (value != 1.0689648751481897d)
				             Assert.fail("DoubleDivisionTest1->run(): Bad result for test #3");

			value = tstDDivideReturnConst();           

			              if (value != 1.0d)
				         Assert.fail("DoubleDivisionTest1->run(): Bad result for test #4");

			value = tstDDivide();               

			              if (value != 0.75d)
				         Assert.fail("DoubleDivisionTest1->run(): Bad result for test #5");
	
			value = tstDDivideNeg();           
			
			              if (value != (double) -4.9e-324)
				         Assert.fail("DoubleDivisionTest1->run(): Bad result for test #6");
				
			value = tstDDivideNegConst();           
			
			              if (value != 1.342d)
				         Assert.fail("DoubleDivisionTest1->run(): Bad result for test #7");

			value = tstDDivideReturnNegConst();           
			
			              if (value != 1.0d)
				         Assert.fail("DoubleDivisionTest1->run(): Bad result for test #8");
				
		
			value = tstDDivideZeroAndNonZero();           
			              if (value != 0.0d)
				         Assert.fail("DoubleDivisionTest1->run(): Bad result for test #10");			
				
			value = tstDDivideNegativeIdentity();           
			              if (value != (double) -1.0)
				         Assert.fail("DoubleDivisionTest1->run(): Bad result for test #12");						

			value = tstDDivideNegativeIdentity2();           
			              if (value != (double) -1.0)
				         Assert.fail("DoubleDivisionTest1->run(): Bad result for test #13");
				
			value = tstDDivideMax();           
			              if (value != 1.0)
				         Assert.fail("DoubleDivisionTest1->run(): Bad result for test #14");	
				
			value = tstDDivideMin();           
			              if (value != 1.0)
				         Assert.fail("DoubleDivisionTest1->run(): Bad result for test #15");	
						
			value = tstDDivideReturnMax();           
			              if (value != 1.0)
				         Assert.fail("DoubleDivisionTest1->run(): Bad result for test #16");	
				
			value = tstDDivideReturnConstMax();           
			              if (value != 1.0)
				         Assert.fail("DoubleDivisionTest1->run(): Bad result for test #17");					
		
			value = tstDDivideReturnMin();           
			              if (value != (double) 1.0)
				         Assert.fail("DoubleDivisionTest1->run(): Bad result for test #18");			
			
			value = tstDDivideReturnConstMin();           
			              if (value != (double) 1.0)
				         Assert.fail("DoubleDivisionTest1->run(): Bad result for test #18");
				    
			value = tstDDivideNan();        
			           if (!Double.isNaN(value))
				         Assert.fail("DoubleDivisionTest1->run(): Bad result for test #19");
				    	
			value = tstDDividePositive();        
			              if (value != 3.0)
				         Assert.fail("DoubleDivisionTest1->run(): Bad result for test #20");

			value = tstDDivideYieldNegative();        
			              if (value != -3.0)
				         Assert.fail("DoubleDivisionTest1->run(): Bad result for test #21");				    
				    
			value = tstDDivideYieldPositive2();        
			              if (value != 3.0)
				         Assert.fail("DoubleDivisionTest1->run(): Bad result for test #22");
				    
			value = tstDDivideBoundaryMaxTest();        
			              if (value != (double) 1.7976931348623157e308)
				         Assert.fail("DoubleDivisionTest1->run(): Bad result for test #23");
				    
			value = tstDDivideBoundaryMaxMin1();        
			              if (value != 0.0)
				         Assert.fail("DoubleDivisionTest1->run(): Bad result for test #24");
				    
			value = tstDDivideBoundaryMaxMin2();        
			           if (!Double.isInfinite(value))
				         Assert.fail("DoubleDivisionTest1->run(): Bad result for test #25");
				    
			value = tstDDivideOverBoundary();        
			           if (!Double.isInfinite(value))
				         Assert.fail("DoubleDivisionTest1->run(): Bad result for test #26");
		
			value = tstDDivideInfinityByInfinityTest1();        
			           if (!Double.isNaN(value))
				         Assert.fail("DoubleDivisionTest1->run(): Bad result for test #27");
			
			value = tstDDivideInfinityByInfinityTest2();        
			           if (!Double.isNaN(value))
				         Assert.fail("DoubleDivisionTest1->run(): Bad result for test #28");
			
			value = tstDDivideInfinityByFinite();        
			           if (!Double.isInfinite(value))
				         Assert.fail("DoubleDivisionTest1->run(): Bad result for test #29");

			value = tstDDivideFiniteByInfinite();        
			              if (value != 0.0)
				         Assert.fail("DoubleDivisionTest1->run(): Bad result for test #30");
			
//			value = tstDDivideZeroByZero();        
//			           if (!Double.isInfinite(value))
//				         Assert.fail("DoubleDivisionTest1->run(): Bad result for test #31");

			value = tstDDivideNonZeroByZero();        
			           if (!Double.isInfinite(value))
				         Assert.fail("DoubleDivisionTest1->run(): Bad result for test #32");

			value = tstDDivideOverFlow();        
			           if (!Double.isInfinite(value))
				         Assert.fail("DoubleDivisionTest1->run(): Bad result for test #33");

			value = tstDDivideUnderFlow();        
			              if (value != (double) 1.0e-323)
				         Assert.fail("DoubleDivisionTest1->run(): Bad result for test #34");

			value =	tstDDivideByZeroToYieldInfinity();     	 			    
			           if (!Double.isInfinite(value))
				         Assert.fail("DoubleDivisionTest1->run(): Bad result for test #35");
			
	}

}
	private double tstDDivide() 
	{	
		double a = (double) 3.0; 
		double b = (double) 4.0;
		double c = a / b;
		return c;		
	
	}

	private double tstDDivideByZeroToYieldInfinity() 
	{	
		double a = (double) 3/0; 
		double b = (double) 4.0;
		double c = a / b;
		return c;		
	
	}

	private double tstDDivideConst() 
	{	
		double b = (double) 3.0 / (double) 5.8888;
		return b;		
	
	}				

	private double tstDDivideReturn() 
	{	
		double a = (double) 3.44444; 
		double b = (double) 3.22222;
		return a/b;	
	}

	private double tstDDivideReturnConst() 
	{	
		return (double) 3.402 / (double) 3.402;	
	
	}

	private double tstDDivideNeg() 
	{	
		double a = (double) 4.94065645841246544e-324d; 
		double b = (double) -1;
		double c = a/b;
		return c;		
	
	}

	private double tstDDivideNegConst()
	{
		double b = (double) -1.342 / -1;
		return b;	
	}
	
	private double tstDDivideReturnNegConst()
	{
		return (double) -1.401 / (double) -1.401;	
	}
	
	private double tstDDivideZeroAndNonZero()
	{
		double b = 0 / (double) 1.79769313486231570e+308d;
		return b;	
	}
	
	private double tstDDivideNegativeIdentity()
	{
		double b = (double) 1.79769313486231570e+308d / (double) -1.79769313486231570e+308d;
		return b;	
	}	
	
	private double tstDDivideNegativeIdentity2()
	{
		double b = (double) 4.94065645841246544e-324d / (double) -4.94065645841246544e-324d;
		return b;	
	}
	
	private double tstDDivideMax() 
	{	
		double a = (double) 1.79769313486231570e+308d; 
		double b = (double) 1.79769313486231570e+308d;
		double c = a / b;
		return c;		
	
	}

	private double tstDDivideMin() 
	{	
		double a = (double) 4.94065645841246544e-324d; 
		double b = (double) 4.94065645841246544e-324d;
		double c = a / b;
		return c;		
	
	}

	private double tstDDivideReturnMax() 
	{	
		double a = (double) 1.79769313486231570e+308d; 
		double b = (double) 1.79769313486231570e+308d;
		return a/b;	
	}

	private double tstDDivideReturnConstMax() 
	{	
		return (double) 1.79769313486231570e+308d / (double) 1.79769313486231570e+308d;	
	
	}
	
	private double tstDDivideReturnMin() 
	{	
		double a = (double) 4.94065645841246544e-324d; 
		double b = (double) 4.94065645841246544e-324d;
		return a/b;	
	}
	
	private double tstDDivideReturnConstMin() 
	{	
		return (double) 4.94065645841246544e-324d / (double) 4.94065645841246544e-324d;	
	
	}

	private double tstDDivideNan() 
	{	
		double a;
		double b = (double) 3.4;
		a = Double.NaN;   //   to produce a NaN value
		double c = a/b;
	
		return c;		
	}
	
	private double tstDDividePositive() 
	{	
		double a = (double) 9.0;
		double b = (double) 3.0;
		double c = a/b;
	
		return c;		
	}

	private double tstDDivideYieldNegative() 
	{	
		double a = (double) -9.0;
		double b = (double) 3.0;
		double c = a/b;
	
		return c;		
	}	

	private double tstDDivideYieldPositive2() 
	{	
		double a = (double) -9.0;
		double b = (double) -3.0;
		double c = a/b;
	
		return c;		
	}

	private double tstDDivideBoundaryMaxTest() 
	{	
		double a = (double) 1.79769313486231570e+308d;
		double b = (double) 1;
		double c = a/b;
	
		return c;		
	}

	private double tstDDivideBoundaryMinTest() 
	{	
		double a = (double) 4.94065645841246544e-324d;
		double b = (double) 1;
		double c = a/b;
	
		return c;		
	}
	
	private double tstDDivideBoundaryMaxMin1() 
	{	
		double a = (double) 4.94065645841246544e-324d;
		double b = (double) 1.79769313486231570e+308d;
		double c = a/b;
	
		return c;		
	}
	
	private double tstDDivideBoundaryMaxMin2() 
	{	
		double a = (double) 1.79769313486231570e+308d;
		double b = (double) 4.94065645841246544e-324d;
		double c = a/b;
	
		return c;		
	}

	private double tstDDivideOverBoundary() 
	{	
		double a = (double) 1.79769313486231570e+308d;
		double b = (double) 1.79769313486231570e+308d;
		double c = (a*100)/b;
	
		return c;		
	}	

	private double tstDDivideInfinityByInfinityTest1() 
	{	
		double a;
		double b;
		
		a = Double.POSITIVE_INFINITY;
		b = Double.POSITIVE_INFINITY;
		
		double c = a/b;
	
		return c;		
	}
	
	private double tstDDivideInfinityByInfinityTest2() 
	{	
		double a;
		double b;
		
		a = Double.NEGATIVE_INFINITY;
		b = Double.NEGATIVE_INFINITY;
		
		double c = a/b;
	
		return c;		
	}	

	private double tstDDivideInfinityByFinite()
	{	
		double a;
		double b;
		
		a = Double.POSITIVE_INFINITY;
		b = (double) 100.99;
		
		double c = a/b;
	
		return c;		
	}	


	private double tstDDivideFiniteByInfinite() 
	{	
		double a;
		double b;
		
		a = Double.POSITIVE_INFINITY;
		b = (double) 100.99;
		
		double c = b/a;
	
		return c;		
	}

	private double tstDDivideZeroByZero() 
	{	
		double c = 0/0;
	
		return c;		
	}	

	private double tstDDivideNonZeroByZero() 
	{	
		double a = (double) 9.88888;
		double b = 0;
		double c = a/b;
	
		return c;		
	}	

	private double tstDDivideOverFlow()
	{	
		double a = (double) 1.79769313486231570e+308d;
		double b = (double) 0.5;
		double c = a/b;
	
		return c;		
	}	
	
	private double tstDDivideUnderFlow()
	{	
		double a = (double) 4.94065645841246544e-324d;
		double b = (double) 0.5;
		double c = a/b;
	
		return c;		
	}

	
}
