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
public class FloatMultiplicationTest1 extends jit.test.jitt.Test 
{
	@Test
	public void testFloatMultiplicationTest1() 
	{
		float value = 0;

		for (int j = 0; j < sJitThreshold; j++)
		{ 
			value = tstFMultiply();      

			       if (value != 12.0f)
				       Assert.fail("FloatMultiplicationTest1->run(): Bad result for test #1");

			value = tstFMultiplyConst();      

			       if (value != (float) 17.6664)
				       Assert.fail("FloatMultiplicationTest1->run(): Bad result for test #2");

			value = tstFMultiplyReturn();      

			       if (value != 11.0987425f)
				       Assert.fail("FloatMultiplicationTest1->run(): Bad result for test #3");

			value = tstFMultiplyReturnConst();      

			       if (value != (float) 11.573604)
				       Assert.fail("FloatMultiplicationTest1->run(): Bad result for test #4");

			value = tstFMultiply();      

			       if (value != 12.0f)
				       Assert.fail("FloatMultiplicationTest1->run(): Bad result for test #5");
	
			value = tstFMultiplyNeg();      
			
			       if (value != (float) -1.4E-45)
				       Assert.fail("FloatMultiplicationTest1->run(): Bad result for test #6");
				
			value = tstFMultiplyNegConst();      
			
			       if (value != (float) 1.342)
				       Assert.fail("FloatMultiplicationTest1->run(): Bad result for test #7");

			value = tstFMultiplyReturnNegConst();      
			
			       if (value != (float) 1.9628011)
				       Assert.fail("FloatMultiplicationTest1->run(): Bad result for test #8");
				
			value = tstFMultiplyPosNegZeroes();      

			       if (value != -0.0f)
				       Assert.fail("FloatMultiplicationTest1->run(): Bad result for test #9");
		
			value = tstFMultiplyZeroAndNonZero();      
			       if (value != (float) 0.0)
				       Assert.fail("FloatMultiplicationTest1->run(): Bad result for test #10");			

			value = tstFMultiplyZeroes();      
			       if (value != 0.0)
				       Assert.fail("FloatMultiplicationTest1->run(): Bad result for test #11");	
				
			value = tstFMultiplyOverFlow();      
			       if (value != Float.NEGATIVE_INFINITY)
				       Assert.fail("FloatMultiplicationTest1->run(): Bad result for test #12");						

			value = tstFMultiplyUnderFlow();      
			       if (value != -0.0)
				       Assert.fail("FloatMultiplicationTest1->run(): Bad result for test #13");
				
			value = tstFMultiplyMax();      
			       if (value != 3.4028235E38f)
				       Assert.fail("FloatMultiplicationTest1->run(): Bad result for test #14");	
				
			value = tstFMultiplyMin();        
			       if (value != 1.4E-45f)
				       Assert.fail("FloatMultiplicationTest1->run(): Bad result for test #15");	
						
			value = tstFMultiplyReturnMax();      
			       if (value != 3.4028235e38f)
				       Assert.fail("FloatMultiplicationTest1->run(): Bad result for test #16");	
				
			value = tstFMultiplyReturnConstMax();      
			       if (value != 3.4028235e38f )
				       Assert.fail("FloatMultiplicationTest1->run(): Bad result for test #17");					
		
			value = tstFMultiplyReturnMin();       
			       if (value != (float) 1.4e-45f)
				       Assert.fail("FloatMultiplicationTest1->run(): Bad result for test #18");			
			
			value = tstFMultiplyReturnConstMin();      
			       if (value != (float) 1.4E-45f)
				       Assert.fail("FloatMultiplicationTest1->run(): Bad result for test #19");				
				   
			value = tstFMultiplyBothNaN();       
			       if (!Float.isNaN(value))   
				       Assert.fail("FloatMultiplicationTest1->run(): Bad result for test #20");				
				   
			value = tstFMultiplyOneNaN();      
			       if (!Float.isNaN(value))
				       Assert.fail("FloatMultiplicationTest1->run(): Bad result for test #21");

			value = tstFMultiplyInfinityByZero();      
			       if (!Float.isNaN(value))
				       Assert.fail("FloatMultiplicationTest1->run(): Bad result for test #22");				
				    
			value = tstFMultiplyInfinityByNonZeroValue();      
			       if (value != Float.NEGATIVE_INFINITY)
				       Assert.fail("FloatMultiplicationTest1->run(): Bad result for test #23");				
				
		}

	}


	private float tstFMultiply() 
	{	
		float a = 3.0f; 
		float b = 4.0f;
		float c = a * b;
		return c;
	
	}

	private float tstFMultiplyConst() 
	{	
		float b = 3.0f * 5.8888f;
		return b;		
	
	}

	private float tstFMultiplyReturn() 
	{	
		float a = 3.44444f; 
		float b = 3.22222f;
		return a*b;	
	}

	private float tstFMultiplyReturnConst() 
	{	
		return (float) 3.402 * (float) 3.402;	
	
	}

	private float tstFMultiplyNeg() 
	{	
		float a = (float) 1.40129846432481707e-45; 
		float b = (float) -1;
		float c = a*b;
		return c;		
	
	}

	private float tstFMultiplyNegConst()
	{
		float b = -1.342f * -1f;
		return b;	
	}
	
	private float tstFMultiplyReturnNegConst()
	{
		return -1.401f * -1.401f;	
	}
	
	private float tstFMultiplyPosNegZeroes()
	{
		float b = 0f*(-0f);
		return b;	
	}
	
	private float tstFMultiplyZeroAndNonZero()
	{
		float b = 0f* (float) 3.40282346638528860e+38;
		return b;	
	}

	private float tstFMultiplyZeroes()
	{
		float b = 0*0;
		return b;	
	}	
	
	private float tstFMultiplyOverFlow()
	{
		float b = (float) 3.40282346638528860e+38 * (float) -3.40282346638528860e+38;
		return b;	
	}	
	
	private float tstFMultiplyUnderFlow()
	{
		float b = (float) 1.40129846432481707e-45 * (float) -1.40129846432481707e-45;
		return b;	
	}
	
	private float tstFMultiplyMax() 
	{	
		float a = (float) 3.40282346638528860e+38; 
		float b = (float) 1;
		float c = a * b;
		return c;		
	
	}

	private float tstFMultiplyMin() 
	{	
		float a = (float) 1.40129846432481707e-45; 
		float b = (float) 1;
		float c = a * b;
		return c;		
	
	}

	private float tstFMultiplyReturnMax() 
	{	
		float a = (float) 3.40282346638528860e+38; 
		float b = (float) 1f;
		return a*b;	
	}

	private float tstFMultiplyReturnConstMax() 
	{	
		return (float) 3.40282346638528860e+38 * 1f;	
	
	}
	
	private float tstFMultiplyReturnMin() 
	{	
		float a = (float) 1.40129846432481707e-45; 
		float b = (float) 1f;
		return a*b;	
	}
	
	private float tstFMultiplyReturnConstMin() 
	{	
		return (float) 1.40129846432481707e-45 * 1;	
	
	}

	private float tstFMultiplyBothNaN() 
	{	
		float a = Float.NaN; 
		float b = Float.NaN;
		float c = a*b;
		return c;	
	
	}

	private float tstFMultiplyOneNaN()
	{	
		float a = Float.NaN; 
		float b = 9.0f;
		float c = a*b;
		return c;	
	
	}	

	private float tstFMultiplyInfinityByZero()
	{	
		float a = Float.POSITIVE_INFINITY; 
		float b = 0;
		float c = a*b;
		return c;
		
	}

	private float tstFMultiplyInfinityByNonZeroValue()
	{	
		float a = Float.NEGATIVE_INFINITY; 
		float b = 1f;
		return a*b;
		
		
	}
	
	
	
}
