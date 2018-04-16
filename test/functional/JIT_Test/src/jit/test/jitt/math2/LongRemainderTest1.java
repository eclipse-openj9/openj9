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
public class LongRemainderTest1 extends jit.test.jitt.Test 
{
	@Test
	public void testLongRemainderTest1() 
	{
		long value = 0;

		for (int j = 0; j < sJitThreshold; j++)
		{ 
			value = tstLPositiveRemainder();         
			                       if (value != 1)
				                     Assert.fail("LongRemainderTest1->run(): Bad result for test #1");
			

			value = tstLPositiveRemainderConst();         

			                       if (value != 2)
				                     Assert.fail("LongRemainderTest1->run(): Bad result for test #2");

			value = tstLPositiveRemainderReturn();         

			                       if (value != 2)
				                     Assert.fail("LongRemainderTest1->run(): Bad result for test #3");


			value = tstLPositiveRemainderReturnConst();      
			
			                       if (value != 2)
				                     Assert.fail("LongRemainderTest1->run(): Bad result for test #4");
			

			value = tstLRemainderNeg();         
			
			                    if (value != -2)
				                 Assert.fail("LongRemainderTest1->run(): Bad result for test #5");
				
			value = tstLRemainderNegConst();         
			
			                    if (value != -3)
				                 Assert.fail("LongRemainderTest1->run(): Bad result for test #6");

			value = tstLRemainderReturnNegConst();         
			
			                    if (value != 1)
				                 Assert.fail("LongRemainderTest1->run(): Bad result for test #7");
				
		
			value = tstLRemainderZeroAndNonZero();         
			                    if (value != 0)
				                 Assert.fail("LongRemainderTest1->run(): Bad result for test #8");			
				
			value = tstLRemainderNegativeIdentity();                   
			                    if (value != 0)
				                 Assert.fail("LongRemainderTest1->run(): Bad result for test #9");						

			value = tstLRemainderNegativeIdentity2();                   
			                    if (value != 0)
				                 Assert.fail("LongRemainderTest1->run(): Bad result for test #10");
				
			value = tstLRemainderMax();                   
			                    if (value != 0)
				                 Assert.fail("LongRemainderTest1->run(): Bad result for test #11");	
				
			value = tstLRemainderMin();                   
			                    if (value != 0)
				                 Assert.fail("LongRemainderTest1->run(): Bad result for test #12");	
						
			value = tstLRemainderReturnMax();                   
			                    if (value != 0)
				                 Assert.fail("LongRemainderTest1->run(): Bad result for test #13");	
				
			value = tstLRemainderReturnConstMax();                   
			                    if (value != 0)
				                 Assert.fail("LongRemainderTest1->run(): Bad result for test #14");					
		
			value = tstLRemainderReturnMin();                   
			                    if (value != 0)
				                 Assert.fail("LongRemainderTest1->run(): Bad result for test #15");			
			
			value = tstLRemainderReturnConstMin();                   
			                    if (value != 0)
				                 Assert.fail("LongRemainderTest1->run(): Bad result for test #16");
				    
			value = tstLRemainderMAX_VALUEAndMIN_VALUE();                
			                 if (value != 9223372036854775807L)
				                 Assert.fail("LongRemainderTest1->run(): Bad result for test #17");
				    	
			value = tstLRemainderSpecialCase();               
			                    if (value != 0)
				                 Assert.fail("LongRemainderTest1->run(): Bad result for test #18");

			value = tstLRemainderSpecialCase2();                
			                    if (value != 0)
				                 Assert.fail("LongRemainderTest1->run(): Bad result for test #19");				    
				    
			value = tstLRemainderYieldNegative1();                
			                    if (value != -1)
				                 Assert.fail("LongRemainderTest1->run(): Bad result for test #20");
				
			value = tstLRemainderYieldPositive1();      	
			                    if (value != 1)
				                 Assert.fail("LongRemainderTest1->run(): Bad result for test #21");

				    
			value = tstLRemainderIdentityTest1();                
			                    if (value != 9223372036854775806L)
				                 Assert.fail("LongRemainderTest1->run(): Bad result for test #22");
				    
			value = tstLRemainderIdentityTest2();                
			                    if (value != -9223372036854775807L)
				                 Assert.fail("LongRemainderTest1->run(): Bad result for test #23");
				    
			value = tstLRemainderBoundaryMaxMin2();                
			                 if (value != 9223372036854775807L)
				                 Assert.fail("LongRemainderTest1->run(): Bad result for test #24");
				    
			value = tstLRemainderOverFlowBoundary();                
			                 if (value != -100)
				                 Assert.fail("LongRemainderTest1->run(): Bad result for test #25");
											
			value = tstLRemainderZeroByMaxNeg();                
			                 if (value != 0)
				                 Assert.fail("LongRemainderTest1->run(): Bad result for test #26");


			value =	tstLRemainderMinByMax();             	 			    
			                 if (value != -1)
				                 Assert.fail("LongRemainderTest1->run(): Bad result for test #27");


			value = tstLRemainderBoundaryMaxMin1();      	 
				         if (value != -1)
				                 Assert.fail("LongRemainderTest1->run(): Bad result for test #28");
		
			value = tstLRemainderYieldNegative2();	     
				         if (value != -1)
				                 Assert.fail("LongRemainderTest1->run(): Bad result for test #29");
			
			value = tstLRemainderBoundaryAdjust();
					     if (value != 188232082384791295L)
					             Assert.fail("LongRemainderTest1->run(): Bad result for test #30");
			
	}

}
	private long tstLPositiveRemainder() 
	{	
		long a = 9L;
		long b = 4L;
		long c = remainder (a , b);
		return c;
	
	}

	private long tstLPositiveRemainderConst()
	{	
		long b = 17 % 5;
		return b;
	
	}

	private long tstLPositiveRemainderReturn() 
	{	
		long a = 17; 
		long b = 5;
		return remainder (a , b);
	}

	private long tstLPositiveRemainderReturnConst()
	{	
		return (long) 17.4 % (long) 5.3;
	
	}

	private long tstLRemainderNeg() 
	{	
		long a = -9223372036854775808L;
		long b = 3;
		long c = remainder (a , b);
		return c;
	
	}

	private long tstLRemainderNegConst()
	{
		long b =  -9223372036854775808L % 5;
		return b;	
	}
	
	private long tstLRemainderReturnNegConst()
	{
		return (long) 49 % (long) -8 ;
	}
	
	private long tstLRemainderZeroAndNonZero()
	{
		long b = 0 % 9223372036854775807L;
		return b;
	}
	
	private long tstLRemainderNegativeIdentity()
	{
		long b = -9223372036854775808L % -9223372036854775808L;
		return b;	
	}	
	
	private long tstLRemainderNegativeIdentity2()
	{
		long b = -9223372036854775808L % -(-9223372036854775808L);
		return b;	
	}
	
	private long tstLRemainderMax() 
	{	
		long a = 9223372036854775807L; 
		long b = 9223372036854775807L;
		long c = remainder (a , b);
		return c;		
	
	}

	private long tstLRemainderMin() 
	{	
		long a = -9223372036854775808L; 
		long b = -9223372036854775808L;
		long c = remainder (a , b);
		return c;		
	
	}

	private long tstLRemainderReturnMax() 
	{	
		long a = 9223372036854775807L; 
		long b = 9223372036854775807L;
		return remainder (a , b);	
	}

	private long tstLRemainderReturnConstMax() 
	{	
		return 9223372036854775807L % 9223372036854775807L;	
	
	}
	
	private long tstLRemainderReturnMin() 
	{	
		long a = -9223372036854775808L; 
		long b = -9223372036854775808L;
		return remainder (a , b);	
	}
	
	private long tstLRemainderReturnConstMin() 
	{	
		return -9223372036854775808L % -9223372036854775808L;	
	
	}

	private long tstLRemainderMAX_VALUEAndMIN_VALUE() 
	{	
		long a;
		long b = Long.MIN_VALUE;
		a = Long.MAX_VALUE;
		long c = remainder (a , b);
	
		return c;		
	}
	
	private long tstLRemainderSpecialCase() 
	{	
		long a = -9223372036854775808L;
		long b = -1L;
		long c = remainder (a , b);
	
		return c;		
	}

	private long tstLRemainderSpecialCase2() 
	{	
		long a = 9223372036854775807L;
		long b = -1;
		long c = remainder (a , b);
	
		return c;		
	}	

	private long tstLRemainderYieldNegative1() 
	{	
		long a = -9L;
		long b = -4L;
		long c = remainder (a , b);
	
		return c;		
	}

	private long tstLRemainderYieldPositive1() 
	{	
		long a = 9L;
		long b = -4L;
		long c = remainder (a , b);
	
		return c;		
	}
	
	private long tstLRemainderYieldNegative2() 
	{	
		long a = -9L;
		long b = 4L;
		long c = remainder (a , b);
	
		return c;		
	}


	private long tstLRemainderIdentityTest1() 
	{	
		long a = 9223372036854775806L;
		long b = 9223372036854775807L;
		long c = remainder (a , b);
	
		return c;		
	}

	private long tstLRemainderIdentityTest2() 
	{	
		long a = -9223372036854775807L;
		long b = -9223372036854775808L;
		long c = remainder (a , b);
	
		return c;		
	}
		
	private long tstLRemainderBoundaryMaxMin1() 
	{	
		long a = -9223372036854775808L;
		long b = 9223372036854775807L;
		long c = remainder (a , b);
	
		return c;		
	}
	
	private long tstLRemainderBoundaryMaxMin2() 
	{	
		long a = 9223372036854775807L;
		long b = -9223372036854775808L;
		long c = remainder (a , b);
	
		return c;		
	}

	private long tstLRemainderOverFlowBoundary() 
	{	
		long a = 9223372036854775807L;
		long b = 9223372036854775807L;
		long c = (a*100)%b;
	
		return c;		
	}	


	private long tstLRemainderZeroByMaxNeg() 
	{	
		long c = 0%-9223372036854775808L;
		return c;		
	}	


	private long tstLRemainderMinByMax()
	{	
		long a = Long.MIN_VALUE;
		long b = Long.MAX_VALUE;
		long c = remainder (a , b);
	
		return c;		
	}

	private long tstLRemainderBoundaryAdjust()
	{
		long a = Long.MAX_VALUE;
		long b = Long.MAX_VALUE/49 + 1;
		long c = remainder (a , b);

		return c;
	}

	private long remainder(long A, long B)
	{
		long C = A%B;

		return C;
	}
}
