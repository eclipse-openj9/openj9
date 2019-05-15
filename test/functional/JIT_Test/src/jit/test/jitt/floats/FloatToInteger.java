/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
// Test file: FloatToInteger.java

/**

Test:  F2I  Float to Integer

-Case 1:if pre-converted value is NaN, then the converted value is an int 0
-Case 2:if the pre-converted value is not infinity, the value is rounded using IEEE754 if it can be represented as an int
-Case 3:if pre-converted value does not fit into case1 and case2, then the pre-converted value must be either too small or 
too large.  It will be converted to the smallest or largest representable value of type int.

May lose precision and overall magnitude

*/

package jit.test.jitt.floats;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity","component.jit" })
public class FloatToInteger extends jit.test.jitt.Test {

   @Test
   public void testFloatToInteger() {
        int result=0;
        result=tstF2I();

        if (result != 1)
                Assert.fail("FloatToInteger->run(): The test did not execute as expected!");

   }

        static int tstF2I() {

		//Case A: Float value was NaN.  Should Convert to an int 0
		
		float proper_value_a= Float.NaN;
		int narrowing_conversion_a;
		
		narrowing_conversion_a = (int)proper_value_a;
		
		if (narrowing_conversion_a != 0)
                        Assert.fail("DoubleToFloat: test case A failed!");

		//Case B-1  Float value that is NOT infinity.  Should be rounded to nearest type int value
		
		float proper_value_b_1 = 1.2345F;
		int narrowing_conversion_b_1;
		
		narrowing_conversion_b_1 = (int)proper_value_b_1;
		
		if (narrowing_conversion_b_1 != 1)
                        Assert.fail("DoubleToFloat: test case B1 failed!");

		
		//Case B-2 Float value that is NOT infinity.  Should be rounded to nearest type int value
		
		float proper_value_b_2 = 1.9345F;
		int narrowing_conversion_b_2;
		
		narrowing_conversion_b_2 = (int)proper_value_b_2;
		
		if (narrowing_conversion_b_2 != 1)
                        Assert.fail("DoubleToFloat: test case B2 failed!");

		//Case B-3 Float value that is NOT infinity.  Should be rounded to nearest type int value
		
		float proper_value_b_3 = 2.8345F;
		int narrowing_conversion_b_3;
		
		narrowing_conversion_b_3 = (int)proper_value_b_3;
		
		if (narrowing_conversion_b_3 != 2)
                        Assert.fail("DoubleToFloat: test case B3 failed!");

		//Case C-1: if pre-converted value does not fit into caseA and caseB, then the pre-converted value must be too small  
		//It will be converted to the smallest  representable value of type int.
		
		float proper_value_c_1 = -2.8345E20F;
		int narrowing_conversion_c_1;
		
		narrowing_conversion_c_1 = (int)proper_value_c_1;
		
		if (narrowing_conversion_c_1 != Integer.MIN_VALUE)
                        Assert.fail("DoubleToFloat: test case C1 failed!");
		
		//Case C-2: if pre-converted value does not fit into caseA and caseB, then the pre-converted value must be too small  
		//It will be converted to the largest  representable value of type int.
		
		float proper_value_c_2 = 8.8385E21F;
		int narrowing_conversion_c_2;
		
		narrowing_conversion_c_2 = (int)proper_value_c_2;
		
		if (narrowing_conversion_c_2 != Integer.MAX_VALUE)
                        Assert.fail("DoubleToFloat: test case C2 failed!");
		
		return 1;
	}
}
