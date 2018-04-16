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
// Test file: DoubleToFloat.java

/*
Double to Float 

When an Double to Float conversion is FP-strict, so the result is always rounded to the nearest representable value.

When an D2F conversion is NOT FP-strict, the result of conversion may be taken from the float extended exponent value. 
(note: not always rounded)

Aside: FP-strict = Floating Point Strict.  Determined by the "ACC_STRICT" bit of the access_flags
(NB: methods in classes compiled by predates of Java2, v1.2 are NOT FP-strict

Special Cases:
-A double finite value too small as a float is converted to a 0 of the same sign
-A double finite value too large as a float is converted to an infinity of the same sign
-A double NaN converts to a float NaN

D2F is a narrowing conversion.  :. may lose information about overall magnitude of value and may lose precision.
*/

package jit.test.jitt.floats;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity","component.jit" })
public class DoubleToFloat extends jit.test.jitt.Test {

   @Test
   public void testDoubleToFloat() {
   	int result=0;
	result=tstD2F();
	
        if (result != 1)
                Assert.fail("DoubleToFloat->run(): The test did not execute as expected!");

   } 

	static int tstD2F() {

		/*case A- Simple Case.  A double value that guarantees it fits into a float*/
		
		double proper_value_a;
		float narrowing_conversion_value_a;
		double precision_lost;
		
		proper_value_a = 2.5E22D;  //append 'D' to specify double
		
		narrowing_conversion_value_a = (float)proper_value_a;
		
		precision_lost = (double)narrowing_conversion_value_a - proper_value_a;	
		
		if (precision_lost == 0)
			//There WILL be a loss of precision since the double was changed
			//to a vote using IEEE 754 standards. (round to nearest mode)
		        Assert.fail("DoubleToFloat: test case A failed!");
		
		/*case B- A double value too large to fit into a float should be either neg/pos inf*/
		
		double proper_value_b;
		float narrowing_conversion_value_b;
		
		proper_value_b = -1e200D;  //append 'D' to specify double
		
		narrowing_conversion_value_b = (float)proper_value_b;
		
		if (narrowing_conversion_value_b != Double.NEGATIVE_INFINITY)
                        Assert.fail("DoubleToFloat: test case B failed!");
		
		
		/*case C- A double value too small to fit into a float should be pos or neg zero*/
		
		double proper_value_c;
		float narrowing_conversion_value_c;
		
		proper_value_c = -1e-200D;  //append 'D' to specify double
		
		narrowing_conversion_value_c = (float)proper_value_c;
		
		if (narrowing_conversion_value_c != 0)
                        Assert.fail("DoubleToFloat: test case C failed!");
		
		
		/*case D- A double NaN value is always converted to a float NaN value*/
		
		double proper_value_d;
		float narrowing_conversion_value_d;
		
		proper_value_d = Double.NaN; //to produce a NaN value
		
		narrowing_conversion_value_d = (float)proper_value_d;
		
		if (!Float.isNaN(narrowing_conversion_value_d))
                        Assert.fail("DoubleToFloat: test case D failed!");
		
		
		//If cases A through D passes, then we would get down to this part of the program.
		return 1;
		
		
	}
		
}		
