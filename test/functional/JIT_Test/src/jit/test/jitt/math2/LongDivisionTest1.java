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
package jit.test.jitt.math2;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity","component.jit" })
public class LongDivisionTest1 extends jit.test.jitt.Test 
{

	@Test
	public void testLongDivisionTest1() 
	{
		long value = 0;

		for (int j = 0; j < sJitThreshold; j++)
		{ 
			value = tstLDivide();   
			                   if (value != 0)
				                Assert.fail("LongDivisionTest1->run(): Bad result for test #1");
			

			value = tstLDivideConst();   

			                   if (value != -429496729L)
				                Assert.fail("LongDivisionTest1->run(): Bad result for test #2");

			value = tstLDivideReturn();   

			                   if (value != 1)
				                Assert.fail("LongDivisionTest1->run(): Bad result for test #3");

			value = tstLDivideReturnConst();   

			                if (value != 1)
				            Assert.fail("LongDivisionTest1->run(): Bad result for test #4");

			value = tstLDivideNeg();   
			
			                if (value != 4611686018427387904L)
				            Assert.fail("LongDivisionTest1->run(): Bad result for test #5");
				
			value = tstLDivideNegConst();   
			
			                if (value != -9223372036854775808L)
				            Assert.fail("LongDivisionTest1->run(): Bad result for test #6");

			value = tstLDivideReturnNegConst();   
			
			                if (value != 1)
				            Assert.fail("LongDivisionTest1->run(): Bad result for test #7");
				
		
			value = tstLDivideZeroAndNonZero();   
			                if (value != 0)
				            Assert.fail("LongDivisionTest1->run(): Bad result for test #8");			
				
			value = tstLDivideNegativeIdentity();             
			                if (value != 1)
				            Assert.fail("LongDivisionTest1->run(): Bad result for test #9");						

			value = tstLDivideNegativeIdentity2();             
			                if (value != 1)
				            Assert.fail("LongDivisionTest1->run(): Bad result for test #10");
				
			value = tstLDivideMax();             
			                if (value != 1)
				            Assert.fail("LongDivisionTest1->run(): Bad result for test #11");	
				
			value = tstLDivideMin();             
			                if (value != 1)
				            Assert.fail("LongDivisionTest1->run(): Bad result for test #12");	
						
			value = tstLDivideReturnMax();             
			                if (value != 1)
				            Assert.fail("LongDivisionTest1->run(): Bad result for test #13");	
				
			value = tstLDivideReturnConstMax();             
			                if (value != 1)
				            Assert.fail("LongDivisionTest1->run(): Bad result for test #14");					
		
			value = tstLDivideReturnMin();             
			                if (value != 1)
				            Assert.fail("LongDivisionTest1->run(): Bad result for test #15");			
			
			value = tstLDivideReturnConstMin();             
			                if (value != 1)
				            Assert.fail("LongDivisionTest1->run(): Bad result for test #16");
				    
			value = tstLDivideMAX_VALUEAndMIN_VALUE();          
			             if (value != 0)
				            Assert.fail("LongDivisionTest1->run(): Bad result for test #17");
				    	
			value = tstLDividePositive();          
			                if (value != 3)
				            Assert.fail("LongDivisionTest1->run(): Bad result for test #18");

			value = tstLDivideYieldNegative();          
			                if (value != -3)
				            Assert.fail("LongDivisionTest1->run(): Bad result for test #19");				    
				    
			value = tstLDivideYieldPositive2();          
			                if (value != 3)
				            Assert.fail("LongDivisionTest1->run(): Bad result for test #20");
				    
			value = tstLDivideBoundaryMaxTest();          
			                if (value != 9223372036854775807L)
				            Assert.fail("LongDivisionTest1->run(): Bad result for test #21");
				    
			value = tstLDivideBoundaryMaxMin1();          
			                if (value != -1)
				            Assert.fail("LongDivisionTest1->run(): Bad result for test #22");
				    
			value = tstLDivideBoundaryMaxMin2();          
			             if (value != 0)
				            Assert.fail("LongDivisionTest1->run(): Bad result for test #23");
				    
			value = tstLDivideOverFlowBoundary();          
			             if (value != 0)
				            Assert.fail("LongDivisionTest1->run(): Bad result for test #24");
											
			value = tstLDivideZeroByMaxNeg();          
			             if (value != 0)
				            Assert.fail("LongDivisionTest1->run(): Bad result for test #25");


			value =	tstLDivideMinByMax();       	 			    
			             if (value != -1)
				            Assert.fail("LongDivisionTest1->run(): Bad result for test #26");

			value =	tstLDivideBoundaryOverflow();    
				     if (value != -9223372036854775808L)
				            Assert.fail("LongDivisionTest1->run(): Bad result for test #27");

			value = tstLDivideBoundaryMinTest();	 
				     if (value != -9223372036854775808L)
				            Assert.fail("LongDivisionTest1->run(): Bad result for test #28");
		
			value = tstLDivideShiftUse1();	
				     if (value != 512)
				            Assert.fail("LongDivisionTest1->run(): Bad result for test #29");
			
			value = tstLDivideShiftUse2();	
				     if (value != 2147483648L)
				            Assert.fail("LongDivisionTest1->run(): Bad result for test #30");

			value = tstLDivideShiftUseNegative1();	
				     if (value != -512)
				            Assert.fail("LongDivisionTest1->run(): Bad result for test #31");

			value = tstLDivideShiftUseNegative2();	
				     if (value != -2147483648L)
				            Assert.fail("LongDivisionTest1->run(): Bad result for test #32");

			value = tstLDivideShiftUseNegative3();	
				     if (value != -512)
				            Assert.fail("LongDivisionTest1->run(): Bad result for test #33");

			value = tstLDivideShiftUseNegative4();	
				     if (value != -2147483648L)
				            Assert.fail("LongDivisionTest1->run(): Bad result for test #34");

			value = tstLDivideBoundaryQuotientAdjust();
					 if (value != 48L)
					        Assert.fail("LongDivisionTest1->run(): Bad result for test #35");

		}
	}

	private long tstLDivide() 
	{	
		long a = 3L; 
		long b = 4L;
		long c = divide(a , b);
		return c;		
	
	}

	private long tstLDivideConst() 
	{	
		long b = 0x80000000 / 5;
		return b;		
	
	}				

	private long tstLDivideReturn() 
	{	
		long a = -1; 
		long b = -1;
		return divide(a , b);
	}

	private long tstLDivideReturnConst() 
	{	
		return (long) 3.402 / (long) 3.402;	
	
	}

	private long tstLDivideNeg() 
	{	
		long a = -9223372036854775808L; 
		long b = -2;
		long c = divide(a , b);
		return c;		
	
	}

	private long tstLDivideNegConst()
	{
		long b =  -9223372036854775808L / 1;
		return b;	
	}
	
	private long tstLDivideReturnNegConst()
	{
		return (long) -4 / (long) -4;
	}
	
	private long tstLDivideZeroAndNonZero()
	{
		long b = 0 / 9223372036854775807L;
		return b;
	}
	
	private long tstLDivideNegativeIdentity()
	{
		long b = -9223372036854775808L / -9223372036854775808L;
		return b;	
	}	
	
	private long tstLDivideNegativeIdentity2()
	{
		long b = -9223372036854775808L / -(-9223372036854775808L);
		return b;	
	}
	
	private long tstLDivideMax() 
	{	
		long a = 9223372036854775807L; 
		long b = 9223372036854775807L;
		long c = divide(a , b);
		return c;		
	
	}

	private long tstLDivideMin() 
	{	
		long a = -9223372036854775808L; 
		long b = -9223372036854775808L;
		long c = divide(a , b);
		return c;		
	
	}

	private long tstLDivideReturnMax() 
	{	
		long a = 9223372036854775807L; 
		long b = 9223372036854775807L;
		return divide(a , b);	
	}

	private long tstLDivideReturnConstMax() 
	{	
		return 9223372036854775807L / 9223372036854775807L;	
	
	}
	
	private long tstLDivideReturnMin() 
	{	
		long a = -9223372036854775808L; 
		long b = -9223372036854775808L;
		return divide(a , b);	
	}
	
	private long tstLDivideReturnConstMin() 
	{	
		return -9223372036854775808L / -9223372036854775808L;	
	
	}

	private long tstLDivideMAX_VALUEAndMIN_VALUE() 
	{	
		long a;
		long b = Long.MIN_VALUE;
		a = Long.MAX_VALUE;
		long c = divide(a , b);
	
		return c;		
	}
	
	private long tstLDividePositive() 
	{	
		long a = 9L;
		long b = 3L;
		long c = divide(a , b);
	
		return c;		
	}

	private long tstLDivideYieldNegative() 
	{	
		long a = -9L;
		long b = 3L;
		long c = divide(a , b);
	
		return c;		
	}	

	private long tstLDivideYieldPositive2() 
	{	
		long a = -9L;
		long b = -3L;
		long c = divide(a , b);
	
		return c;		
	}

	private long tstLDivideBoundaryMaxTest() 
	{	
		long a = 9223372036854775807L;
		long b = 1L;
		long c = divide(a , b);
	
		return c;		
	}

	private long tstLDivideBoundaryMinTest() 
	{	
		long a = -9223372036854775808L;
		long b = 1L;
		long c = divide(a , b);
	
		return c;		
	}
	
	private long tstLDivideBoundaryOverflow() 
	{	
		long a = -9223372036854775808L;
		long b = -1L;
		long c = divide(a , b);
	
		return c;		
	}	
	
	private long tstLDivideBoundaryMaxMin1() 
	{	
		long a = -9223372036854775808L;
		long b = 9223372036854775807L;
		long c = divide(a , b);
	
		return c;		
	}
	
	private long tstLDivideBoundaryMaxMin2() 
	{	
		long a = 9223372036854775807L;
		long b = -9223372036854775808L;
		long c = divide(a , b);
	
		return c;		
	}

	private long tstLDivideOverFlowBoundary() 
	{	
		long a = 9223372036854775807L;
		long b = 9223372036854775807L;
		long c = (a*100)/b;
	
		return c;		
	}	


	private long tstLDivideZeroByMaxNeg() 
	{	
		long c = 0/-9223372036854775808L;
		return c;		
	}	


	private long tstLDivideMinByMax()
	{	
		long a = Long.MIN_VALUE;
		long b = Long.MAX_VALUE;
		long c = divide(a , b);
	
		return c;		
	}

	private long tstLDivideShiftUse1()
	{	
		long a = 32*32;   // 2^10
		long b = 2;
		long c = divide(a , b);
	
		return c;		
	}
	
	
	private long tstLDivideShiftUse2()
	{	
		long a = 65536L*65536L;	  // 2^32
		long b = 2;
		long c = divide(a , b);
	
		return c;		
	}
	
	
	private long tstLDivideShiftUseNegative1()
	{	
		long a = -(32*32);   // -2^10
		long b = 2;
		long c = divide(a , b);
	
		return c;		
	}
	
	
	private long tstLDivideShiftUseNegative2()
	{	
		long a = -(65536L*65536L);    // -2^32		
		long b = 2;
		long c = divide(a , b);
	
		return c;		
	}
	
	
	private long tstLDivideShiftUseNegative3()
	{	
		long a = 32*32;	    // 2^10
		long b = -2;
		long c = divide(a , b);
	
		return c;		
	}
	
	
	private long tstLDivideShiftUseNegative4()
	{	
		long a = 65536L*65536L;	    // 2^32
		long b = -2;
		long c = divide(a , b);
	
		return c;		
	}			

	private long tstLDivideBoundaryQuotientAdjust()
	{
		long a = Long.MAX_VALUE;
		long b = Long.MAX_VALUE/49 + 1;
		long c = divide(a , b);

		return c;
	}

	private long divide(long A, long B)
	{
		long C = A/B;

		return C;
	}
}
