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
// Test file: FloatToDoubleNotStrict.java

/* Float to Double

-case1: FP-strict (NOT TESTED IN THIS TEST!  CASE 2 ONLY!)
	-performs widenting primitive conversion.
	-all values of float value set are exactly representable by values of the double value set
	
-case 2: not FP-strict
	-result of conversion may be taken from double extended exponent value set
	-not necessarily rounded to nearest representable value in the double value set
	-if operand value x is taken from the float-extended-exponent value set and target result is constrained to the double value set, rounding of value may be required.
*/

package jit.test.jitt.floats;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity","component.jit" })
public class FloatToDoubleNotStrict extends jit.test.jitt.Test {

   @Test
   public void testFloatToDoubleNotStrict() {
        int result=0;
        result=tstF2D();

        if (result != 1)
                Assert.fail("FloatToDoubleNotStrict: This test did not execute as expected!");

   }


   static int tstF2D() {

	/*without adding strictfp should make this method not FP STRICT.  
	This is Case 2.  */

		//case A- Simple Case.  A double value that guarantees it fits into a float	
                      
            	float proper_value_a;
		double widening_conversion_value_a;
				
		proper_value_a = 8.912F;
		widening_conversion_value_a=proper_value_a;
		
		if (proper_value_a != widening_conversion_value_a)
                        Assert.fail("FloatToDoubleNotStrict: test case A failed!");
			
		/*case B
		*/
		
		float proper_value_b=0.98134F;
		double widening_conversion_value_b;
		
		widening_conversion_value_b=proper_value_b;
		
		
		if (proper_value_b != widening_conversion_value_b)
                        Assert.fail("FloatToDoubleNotStrict: test case B failed!");

		return 1;
	}
}		
