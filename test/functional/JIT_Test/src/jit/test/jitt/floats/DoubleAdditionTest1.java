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
public class DoubleAdditionTest1 extends jit.test.jitt.Test 
{
	@Test
	public void testDoubleAdditionTest1() 
	{
		double value = 0;

		for (double j = 0; j < sJitThreshold; j++)
		{ 
			value = tstDAdd();           

			       if (value != 7.0d)
				         Assert.fail("DoubleAdditionTest1->run(): Bad result for test #1");

			value = tstDAddConst();       

			       if (value != 8.8888d)
				         Assert.fail("DoubleAdditionTest1->run(): Bad result for test #2");

			value = tstDAddReturn();       

			       if (value != 6.66666d)
				         Assert.fail("DoubleAdditionTest1->run(): Bad result for test #3");

			value = tstDAddReturnConst();       

			       if (value != 6.804d)
				         Assert.fail("DoubleAdditionTest1->run(): Bad result for test #4");

			value = tstDAdd();           

			       if (value != 7.0d)
				         Assert.fail("DoubleAdditionTest1->run(): Bad result for test #5");
	
			value = tstDAddNeg();       
			
			       if (value != -1.0d)
				         Assert.fail("DoubleAdditionTest1->run(): Bad result for test #6");
				
			value = tstDAddNegConst();       
			
			       if (value != -2.342d)
				         Assert.fail("DoubleAdditionTest1->run(): Bad result for test #7");

			value = tstDAddReturnNegConst();       
			
			       if (value !=  -2.802d)
				         Assert.fail("DoubleAdditionTest1->run(): Bad result for test #8");
				
			value = tstDAddPosNegZeroes();       

			       if (value != 0.0d)
				         Assert.fail("DoubleAdditionTest1->run(): Bad result for test #9");
		
			value = tstDAddZeroAndNonZero();       
			       if (value !=  1.7976931348623157e308d)
				         Assert.fail("DoubleAdditionTest1->run(): Bad result for test #10");			

			value = tstDAddZeroes();       
			       if (value != 0.0d)
				         Assert.fail("DoubleAdditionTest1->run(): Bad result for test #11");	
				
			value = tstDAddObtainZero();       
			       if (value != 0.0d)
				         Assert.fail("DoubleAdditionTest1->run(): Bad result for test #12");						

			value = tstDAddObtainZero2();       
			       if (value != 0.0d)
				         Assert.fail("DoubleAdditionTest1->run(): Bad result for test #13");
				
			value = tstDAddMax();       
//			       if (value != double.POSITIVE_INFINITY)
//				         Assert.fail("DoubleAdditionTest1->run(): Bad result for test #14");	
				
			value = tstDAddMin();       
			       if (value !=  1.0e-323d)
				         Assert.fail("DoubleAdditionTest1->run(): Bad result for test #15");	
						
			value = tstDAddReturnMax();       
//			       if (value != double.POSITIVE_INFINITY)
//				         Assert.fail("DoubleAdditionTest1->run(): Bad result for test #16");	
				
			value = tstDAddReturnConstMax();       
//			       if (value != double.POSITIVE_INFINITY)
//				         Assert.fail("DoubleAdditionTest1->run(): Bad result for test #17");					
		
			value = tstDAddReturnMin();       
			       if (value !=  1.0e-323d)
				         Assert.fail("DoubleAdditionTest1->run(): Bad result for test #18");			
			
			value = tstDAddReturnConstMin();       
			       if (value !=  1.0E-323d)
				         Assert.fail("DoubleAdditionTest1->run(): Bad result for test #18");				
				
		}

	}


	private double tstDAdd() 
	{	
		double a =  3.0; 
		double b =  4.0;
		double c = a + b;
		return c;		
	
	}

	private double tstDAddConst() 
	{	
		double b =  3.0 +  5.8888;
		return b;		
	
	}

	private double tstDAddReturn() 
	{	
		double a =  3.44444; 
		double b =  3.22222;
		return a+b;	
	}

	private double tstDAddReturnConst() 
	{	
		return  3.402 +  3.402;	
	
	}

	private double tstDAddNeg() 
	{	
		double a =  4.94065645841246544e-324d; 
		double b =  -1;
		double c = a+b;
		return c;		
	
	}

	private double tstDAddNegConst()
	{
		double b =  -1.342 + -1;
		return b;	
	}
	
	private double tstDAddReturnNegConst()
	{
		return  -1.401 +  -1.401;	
	}
	
	private double tstDAddPosNegZeroes()
	{
		double b = 0+(-0);
		return b;	
	}
	
	private double tstDAddZeroAndNonZero()
	{
		double b = 0 + 1.79769313486231570e+308d;
		return b;	
	}

	private double tstDAddZeroes()
	{
		double b = 0+0;
		return b;	
	}	
	
	private double tstDAddObtainZero()
	{
		double b =  1.79769313486231570e+308d +  -1.79769313486231570e+308d;
		return b;	
	}	
	
	private double tstDAddObtainZero2()
	{
		double b =  4.94065645841246544e-324d +  -4.94065645841246544e-324d;
		return b;	
	}
	
	private double tstDAddMax() 
	{	
		double a =  1.79769313486231570e+308d; 
		double b =  1.79769313486231570e+308d;
		double c = a + b;
		return c;		
	
	}

	private double tstDAddMin() 
	{	
		double a =  4.94065645841246544e-324d; 
		double b =  4.94065645841246544e-324d;
		double c = a + b;
		return c;		
	
	}

	private double tstDAddReturnMax() 
	{	
		double a =  1.79769313486231570e+308d; 
		double b =  1.79769313486231570e+308d;
		return a+b;	
	}

	private double tstDAddReturnConstMax() 
	{	
		return  1.79769313486231570e+308d +  1.79769313486231570e+308d;	
	
	}
	
	private double tstDAddReturnMin() 
	{	
		double a =  4.94065645841246544e-324d; 
		double b =  4.94065645841246544e-324d;
		return a+b;	
	}
	
	private double tstDAddReturnConstMin() 
	{	
		return  4.94065645841246544e-324d +  4.94065645841246544e-324d;	
	
	}
	
}
