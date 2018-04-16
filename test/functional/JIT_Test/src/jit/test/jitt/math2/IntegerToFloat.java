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
// Test file: IntegerToFloat.java

/* I2F--> Integer to Float
-this conversion performs a widening primitive conversion. 
-may result in a loss of precision because values of type float have only 24 significant bits.
-wont' result in runtime exception
*/

package jit.test.jitt.math2;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity","component.jit" })
public class IntegerToFloat extends jit.test.jitt.Test {

   @Test
   public void testIntegerToFloat() {
        int result=0;
        result=tstI2F();

        if (result != 1)
                Assert.fail("IntegerToFloat->run(): The test did not execute as expected!");

   }

        static int tstI2F() {

		//CASE A:	No lost of precision for integers that are not of 9 digits long
		int proper_value_a;
		float widening_conversion_value_a;
		int no_lost;
		
		proper_value_a = 10;
		
		widening_conversion_value_a = proper_value_a;
		no_lost = proper_value_a - (int)widening_conversion_value_a;
		
		if (no_lost != 0)
                Assert.fail("IntegerToFloat->tstI2F(): test case A failed!");

	
		//CASE B: 	Lost of precision will occur since type float will not be precise to 
		//		nine digits
		int proper_value;
		float widening_conversion_value;
		int lost_precision;
	
		proper_value = 987654321;
		widening_conversion_value = proper_value; //here converting a int to a float
		lost_precision = proper_value - (int)widening_conversion_value;
	
		if ( lost_precision == 0 )
                Assert.fail("IntegerToFloat->tstI2F(): test case B failed!");

			//ie. no lost of precision? But impossible since proper_value was initialized to be a 9 digit number.
	
		
		return 1;
	}
	
	
}
