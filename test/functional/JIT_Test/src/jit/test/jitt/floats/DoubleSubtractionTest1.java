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
public class DoubleSubtractionTest1 extends jit.test.jitt.Test 
{
	@Test
	public void testDoubleSubtractionTest1() 
	{
		double value = 0;

		for (double j = 0; j < sJitThreshold; j++)
		{ 
			value = tstDSub();           

			       if (value != 7.0d)
				         Assert.fail("DoubleSubtractionTest1->run(): Bad result for test #1");

			value = tstDSubConst();       

			       if (value != 8.8888d)
				         Assert.fail("DoubleSubtractionTest1->run(): Bad result for test #2");

			value = tstDSubReturn();       

			       if (value != 6.66666d)
				         Assert.fail("DoubleSubtractionTest1->run(): Bad result for test #3");

			value = tstDSubReturnConst();       

			       if (value != 6.804d)
				         Assert.fail("DoubleSubtractionTest1->run(): Bad result for test #4");

			value = tstDSub();           

			       if (value != 7.0d)
				         Assert.fail("DoubleSubtractionTest1->run(): Bad result for test #5");
	
			value = tstDSubNeg();       
			
			       if (value != -1.0d)
				         Assert.fail("DoubleSubtractionTest1->run(): Bad result for test #6");
				
			value = tstDSubNegConst();       
			
			       if (value != -2.342d)
				         Assert.fail("DoubleSubtractionTest1->run(): Bad result for test #7");

			value = tstDSubReturnNegConst();       
			
			       if (value !=  -2.802d)
				         Assert.fail("DoubleSubtractionTest1->run(): Bad result for test #8");
				
			value = tstDSubPosNegZeroes();       

			       if (value != 0.0d)
				         Assert.fail("DoubleSubtractionTest1->run(): Bad result for test #9");
		
			value = tstDSubZeroAndNonZero();       
			       if (value !=  1.7976931348623157e308d)
				         Assert.fail("DoubleSubtractionTest1->run(): Bad result for test #10");			

			value = tstDSubZeroes();       
			       if (value != 0.0d)
				         Assert.fail("DoubleSubtractionTest1->run(): Bad result for test #11");	
				
			value = tstDSubObtainZero();       
			       if (value != 0.0d)
				         Assert.fail("DoubleSubtractionTest1->run(): Bad result for test #12");						

			value = tstDSubObtainZero2();       
			       if (value != 0.0d)
				         Assert.fail("DoubleSubtractionTest1->run(): Bad result for test #13");
				
			value = tstDSubMax();       
//			       if (value != double.POSITIVE_INFINITY)
//				         Assert.fail("DoubleSubtractionTest1->run(): Bad result for test #14");	
				
			value = tstDSubMin();       
			       if (value !=  1.0e-323d)
				         Assert.fail("DoubleSubtractionTest1->run(): Bad result for test #15");	
						
			value = tstDSubReturnMax();       
//			       if (value != double.POSITIVE_INFINITY)
//				         Assert.fail("DoubleSubtractionTest1->run(): Bad result for test #16");	
				
			value = tstDSubReturnConstMax();       
//			       if (value != double.POSITIVE_INFINITY)
//				         Assert.fail("DoubleSubtractionTest1->run(): Bad result for test #17");					
		
			value = tstDSubReturnMin();       
			       if (value !=  1.0e-323d)
				         Assert.fail("DoubleSubtractionTest1->run(): Bad result for test #18");			
			
			value = tstDSubReturnConstMin();       
			       if (value !=  1.0E-323d)
				         Assert.fail("DoubleSubtractionTest1->run(): Bad result for test #18");				
				
		}

	}


	private double tstDSub() 
	{	
		double a =  3.0; 
		double b =  -4.0;
		double c = a - b;
		return c;		
	
	}

	private double tstDSubConst() 
	{	
		double b =  3.0 -  -5.8888;
		return b;		
	
	}

	private double tstDSubReturn() 
	{	
		double a =  3.44444; 
		double b =  -3.22222;
		return a-b;	
	}

	private double tstDSubReturnConst() 
	{	
		return  3.402 -  -3.402;	
	
	}

	private double tstDSubNeg() 
	{	
		double a =  4.94065645841246544e-324d; 
		double b =  1;
		double c = a-b;
		return c;		
	
	}

	private double tstDSubNegConst()
	{
		double b =  -1.342 - 1;
		return b;	
	}
	
	private double tstDSubReturnNegConst()
	{
		return  -1.401 - 1.401;	
	}
	
	private double tstDSubPosNegZeroes()
	{
		double b = 0-0;
		return b;	
	}
	
	private double tstDSubZeroAndNonZero()
	{
		double b = 0 - -1.79769313486231570e+308d;
		return b;	
	}

	private double tstDSubZeroes()
	{
		double b = 0-(-0);
		return b;	
	}	
	
	private double tstDSubObtainZero()
	{
		double b =  1.79769313486231570e+308d -  +1.79769313486231570e+308d;
		return b;	
	}	
	
	private double tstDSubObtainZero2()
	{
		double b =  4.94065645841246544e-324d -  +4.94065645841246544e-324d;
		return b;	
	}
	
	private double tstDSubMax() 
	{	
		double a =  1.79769313486231570e+308d; 
		double b =  -1.79769313486231570e+308d;
		double c = a - b;
		return c;		
	
	}

	private double tstDSubMin() 
	{	
		double a =  4.94065645841246544e-324d; 
		double b =  -4.94065645841246544e-324d;
		double c = a - b;
		return c;		
	
	}

	private double tstDSubReturnMax() 
	{	
		double a =  1.79769313486231570e+308d; 
		double b =  -1.79769313486231570e+308d;
		return a-b;	
	}

	private double tstDSubReturnConstMax() 
	{	
		return  1.79769313486231570e+308d -  -1.79769313486231570e+308d;	
	
	}
	
	private double tstDSubReturnMin() 
	{	
		double a =  4.94065645841246544e-324d; 
		double b =  -4.94065645841246544e-324d;
		return a-b;	
	}
	
	private double tstDSubReturnConstMin() 
	{	
		return  4.94065645841246544e-324d -  -4.94065645841246544e-324d;	
	
	}
	
}
